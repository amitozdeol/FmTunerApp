/*
    Frequency change stress test

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
	int fh;
	time_t t;
	double f[2] = { 479.25, 503.25 };

	if (argc > 1) device = argv[1];

	fh = open(device, O_RDONLY);

	if (fh == -1) {
		fprintf(stderr, "cannot open %s\n", device);
		return -1;
	}
	for (;;) {
		struct v4l2_frequency vf;
		int err;

		cnt++;
		vf.tuner = 0;
		vf.type = V4L2_TUNER_ANALOG_TV;
		vf.frequency = f[cnt % 2] * 16;
		t = time(NULL);
		printf("%06u: %.2f %s", cnt, f[cnt % 2], ctime(&t));
		if ((err = ioctl(fh, VIDIOC_S_FREQUENCY, &vf))) {
			perror("could not set frequency");
			return -1;
		}
	}

	return 0;
}
