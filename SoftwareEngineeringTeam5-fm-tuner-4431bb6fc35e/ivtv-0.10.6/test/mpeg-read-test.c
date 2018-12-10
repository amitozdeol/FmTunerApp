/*
    open/read/close stress test

    Copyright (C) 2007  Hans Verkuil  <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <time.h>

#include <linux/videodev2.h>

int main(int argc, char **argv)
{
	char *device = "/dev/video0";
	unsigned cnt = 0;
	unsigned bufsz;
	unsigned char buf[2048 * 1024];
	int fh, sz, i;
	time_t t;

	if (argc > 1) device = argv[1];

	srand(time(NULL));
	for (;;) {
		for (bufsz = 4; bufsz <= 2048 * 1024; bufsz *= 2) {
			for (i = 0; i < 8; i++) {
				cnt++;
				fh = open(device, O_RDONLY);

				printf("%06u/%d/%07u: ", cnt, i, bufsz);
				if (fh == -1) {
					fprintf(stderr, "cannot open %s\n", device);
					return -1;
				}
				if ((sz = read(fh, buf, bufsz)) != bufsz) {
					printf("read %d, expected %u\n", sz, bufsz);
					ioctl(fh, VIDIOC_LOG_STATUS, 0);
					return -1;
				}
				if (buf[0] != 0 || buf[1] != 0 || buf[2] != 1) {
					printf("garbage at start of buffer (%02x %02x %02x)\n",
							buf[0], buf[1], buf[2]);
					//ioctl(fh, VIDIOC_LOG_STATUS, 0);
					return -1;
				}
				else {
					t = time(NULL);
					printf("%s", ctime(&t));
				}

				close(fh);
			}
		}
		for (i = 0; i < 80; i++) {
			bufsz = (rand() % sizeof(buf)) + 1;
			if (bufsz < 3) bufsz = 3;
			cnt++;
			fh = open(device, O_RDONLY);

			printf("%06u/%d/%07u: ", cnt, i, bufsz);
			if (fh == -1) {
				fprintf(stderr, "cannot open %s\n", device);
				return -1;
			}
			if ((sz = read(fh, buf, bufsz)) != bufsz) {
				printf("read %d, expected %u\n", sz, bufsz);
				ioctl(fh, VIDIOC_LOG_STATUS, 0);
				return -1;
			}
			if (buf[0] != 0 || buf[1] != 0 || buf[2] != 1) {
				printf("garbage at start of buffer (%02x %02x %02x)\n",
						buf[0], buf[1], buf[2]);
				//ioctl(fh, VIDIOC_LOG_STATUS, 0);
				return -1;
			}
			else {
				t = time(NULL);
				printf("%s", ctime(&t));
			}

			close(fh);
		}
	}
	return 0;
}
