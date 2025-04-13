#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

static int find_card(void);
static drmModeRes *get_resources(int);
static drmModeConnector *get_connector(int, const drmModeRes *);
static drmModeCrtc *get_crtc(int, const drmModeRes *);
static drmModePlane *get_plane(int, const drmModeCrtc *);
static bool is_primary_plane(int, int);

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
	
	drmModeFreeResources(res);
	close(card_fd);
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
