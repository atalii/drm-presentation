#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared.h"

#include <xf86drmMode.h>

static int find_card(void);
static drmModeRes *get_resources(int);
static drmModeConnector *get_connector(int, const drmModeRes *);

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

	drmModeFreeResources(res);
	close(card_fd);
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
