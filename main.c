#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#define USERSPACE 1
#include "peach.h"

static int peach_fd;

int main(int argc, char **argv)
{
	int ret;

	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	if (-1 == sched_setaffinity(0, sizeof mask, &mask)) {
		printf("failed to set affinity\n");

		goto err0;
	}

	if ((peach_fd = open("/dev/peach", O_RDWR)) < 0) {
		printf("failed to open Peach device\n");

		goto err0;
	}

	if ((ret = ioctl(peach_fd, PEACH_PROBE)) < 0) {
		printf("failed to exec ioctl PEACH_PROBE\n");

		goto err1;
	}

	if ((ret = ioctl(peach_fd, PEACH_RUN)) < 0) {
		printf("failed to exec ioctl PEACH_RUN\n");

		goto err1;
	}

	printf("guest exits\n");

err1:
	close(peach_fd);

err0:

	return 0;
}
