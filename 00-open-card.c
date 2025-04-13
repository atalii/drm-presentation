#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "shared.h"

static int find_card(void);

int main(void)
{
	int card_fd = find_card();
	LOG_STAGE("Opened card.");
	fprintf(stderr, "fd:\t%d\n", card_fd);

	close(card_fd);
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

