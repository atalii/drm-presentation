#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "shared.h"

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct buffer {
	uint32_t id;
	uint32_t stride;
	uint64_t size;
	uint8_t *data;
};

static int find_card(void);
static drmModeRes *get_resources(int);
static drmModeConnector *get_connector(int, const drmModeRes *);
static drmModeCrtc *get_crtc(int, const drmModeRes *);
static drmModePlane *get_plane(int, const drmModeCrtc *);
static bool is_primary_plane(int, int);
static struct buffer get_dumb_buf(int, const drmModeModeInfo *);

int main(void)
{
	int card_fd = find_card();
	LOG_STAGE("Opened card.");
	fprintf(stderr, "fd:\t%d\n", card_fd);
	
	drmModeRes *res = get_resources(card_fd);
	LOG_STAGE("Got resources.");

	fprintf(stderr, "fbs:\t\t%d\ncrtcs:\t\t%d\nconnectors:\t%d\n", res->count_fbs, res->count_crtcs, res->count_connectors);

	drmModeConnector *conn = get_connector(card_fd, res);
	LOG_STAGE("Got connector.");

	fprintf(stderr, "%dmm x %dmm\n", conn->mmWidth, conn->mmHeight);

	drmModeCrtc *crtc = get_crtc(card_fd, res);
	LOG_STAGE("Got crtc.");
	fprintf(stderr, "%dpx x %dpx\n", crtc->width, crtc->height);

	drmModeModeInfo mode = crtc->mode;
	LOG_STAGE("Got mode.");
	fprintf(stderr, "%dpx x %dpx @ %dHz\n", mode.hdisplay, mode.vdisplay, mode.vrefresh);

	if (drmSetClientCap(card_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0)
		FATAL_ERR("Could not set DRM_CLIENT_CAP_UNIVERSAL_PLANES.");
	LOG_STAGE("Set DRM_CLIENT_CAP_UNIVERSAL_PLANES.");

	drmModePlane *plane = get_plane(card_fd, crtc);
	LOG_STAGE("Got plane.");
	fprintf(stderr, "id:\t%d\n", plane->plane_id);

	uint64_t cap = 0;
	if (drmGetCap(card_fd, DRM_CAP_DUMB_BUFFER, &cap) < 0)
		FATAL_ERR("DRM_IOCTL_GET_CAP failed: %s", strerror(errno));

	if (!cap)
		FATAL_ERR("Device doesn't support dumb buffers.");

	LOG_STAGE("Device supports dumb buffers.");

	struct buffer buf = get_dumb_buf(card_fd, &mode);
	LOG_STAGE("Got dumb buffer.");

	// Draw a blue square, and then sleep for a little while.
	
	// Little endian! BGRA
	uint8_t mapped[4] = { 0xbd, 0x22, 0x3a, 0xFF };

	for (size_t x = 0; x < 400; x++) {
		for (size_t y = 0; y < 400; y++) {
			size_t off = (buf.stride * y) + (x * sizeof(mapped));
	memcpy(&buf.data[off], mapped, sizeof(mapped));
		}
	}

	while (drmModePageFlip(card_fd, crtc->crtc_id, buf.id, 0, NULL) != 0) {
		if (errno != EBUSY)
			FATAL_ERR("drmModePageFlip failed: %s", STR_ERR);
	}

	sleep(5);

	
	drmModeFreeResources(res);
	close(card_fd);
}

static struct buffer get_dumb_buf(int fd, const drmModeModeInfo *mode)
{
	struct buffer target;

	
	uint32_t width = mode->hdisplay;
	uint32_t height = mode->vdisplay;

	struct drm_mode_create_dumb fb_create = {
		.width = width,
		.height = width,
		.bpp = 32,
	};

	if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &fb_create) != 0)
		FATAL_ERR("DRM_IOCTL_MODE_CREATE_DUMB failed: %s", STR_ERR);

	target.stride = fb_create.pitch;
	target.size = fb_create.size;

	uint32_t handles[4] = { fb_create.handle };
	uint32_t strides[4] = { fb_create.pitch };
	uint32_t offsets[4] = { 0 };
	
	if (drmModeAddFB2(fd, width, height, DRM_FORMAT_XRGB8888,
		handles, strides, offsets, &target.id, 0) != 0)
		FATAL_ERR("drmModeAddFB2 failed: %s", STR_ERR);

	uint64_t offset;
	if (drmModeMapDumbBuffer(fd, fb_create.handle, &offset) != 0)
		FATAL_ERR("drmModeMapDumbBuffer failed: %s", STR_ERR);

	target.data = mmap(0, fb_create.size, PROT_READ | PROT_WRITE,
	    MAP_SHARED, fd, offset);

	if (!target.data)
		FATAL_ERR("Couldn't map frame.");

	return target;
}

static drmModePlane *get_plane(int fd, const drmModeCrtc *crtc)
{
	drmModePlane *plane = NULL;
	drmModePlaneRes *planes = drmModeGetPlaneResources(fd);

	for (uint32_t i = 0; i < planes->count_planes; i++) {
		int plane_id = planes->planes[i];
		plane = drmModeGetPlane(fd, plane_id);

		if (plane->crtc_id == crtc->crtc_id && is_primary_plane(fd, plane_id)) {
			break;
		}

		drmModeFreePlane(plane);
		plane = NULL;
	}

	drmModeFreePlaneResources(planes);

	if (!plane)
		FATAL_ERR("No valid plane found.");

	return plane;
}

static bool is_primary_plane(int fd, int plane_id)
{
	drmModeObjectProperties *props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);

	if (!props)
		FATAL_ERR("drmModeObjectGetProperties failed: %s", STR_ERR);

	for (uint32_t i = 0; i < props->count_props; i++) {
		drmModePropertyRes *prop =
		    drmModeGetProperty(fd, props->props[i]);

		if (strcmp(prop->name, "type") != 0) {
			drmModeFreeProperty(prop);
			continue;
		}

		uint64_t val = props->prop_values[i];
		drmModeFreeProperty(prop);
		drmModeFreeObjectProperties(props);
		return val == DRM_PLANE_TYPE_PRIMARY;
	}

	FATAL_ERR("No primary plane could be found.");
}

static drmModeCrtc *get_crtc(int fd, const drmModeRes *res)
{
	drmModeCrtc *crtc = NULL;

	int ncrtcs = res->count_crtcs;

	for (int i = 0; i < ncrtcs; i++) {
		int crtc_id = res->crtcs[i];
		crtc = drmModeGetCrtc(fd, crtc_id);
		if (!crtc)
			FATAL_ERR("drmModeGetCrtc failed: %s", STR_ERR);

		if (crtc->mode_valid)
			break;

		drmModeFreeCrtc(crtc);
		crtc = NULL;
	}

	if (!crtc)
		FATAL_ERR("No appropriate CRTC found.");

	return crtc;
}

static drmModeConnector *get_connector(int fd, const drmModeRes *res)
{
	int num_connectors = res->count_connectors;
	for (int i = 0; i < num_connectors; i++) {
		int conn_id = res->connectors[i];
		drmModeConnector *conn = drmModeGetConnector(fd, conn_id);

		if (!conn)
			FATAL_ERR("drmModeGetConnector failed: %s", STR_ERR);

		if (conn->connection == DRM_MODE_CONNECTED &&
		    conn->count_modes > 0) {
			return conn;
		}

		drmModeFreeConnector(conn);
	}

	FATAL_ERR("No connectors are available.");
}

static drmModeRes *get_resources(int fd)
{
	drmModeRes *res = drmModeGetResources(fd);
	if (!res)
		FATAL_ERR("drmModeGetResources failed: %s", STR_ERR);

	return res;
}

static int find_card(void)
{
	DIR *dris = opendir("/dev/dri");

	if (!dris)
		FATAL_ERR("Failed to open /dev/dri: %s", STR_ERR);

	struct dirent *card_candidate = NULL;

	while ((card_candidate = readdir(dris))) {
		if (strncmp(card_candidate->d_name, "card", 4) == 0)
			break;
	}

	if (!card_candidate) {
		FATAL_ERR("No appropriate card found.");
	} else {
		fprintf(stderr, "found card at: %s\n", card_candidate->d_name);
	}

	int fd = openat(dirfd(dris), card_candidate->d_name, O_RDWR | O_NONBLOCK);

	if (fd < 0)
		FATAL_ERR("Failed to open device: %s: %s",
		    card_candidate->d_name, STR_ERR);

	closedir(dris);
	return fd;
}
