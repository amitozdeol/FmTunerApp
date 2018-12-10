/*
    Copyright (C) 2003-2004  <author>
	
    Rewrite with getopt and proper usage
    Copyright (C) 2005 Adam Forsyth <agforsyth@gmail.com>

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
#include <features.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#define __user
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <ctype.h>
#include <sys/time.h>
#include <math.h>

#include "ivtv.h"

#if defined(__i386__) || defined(__x86_64__)
typedef unsigned long long W64;
static inline W64 rdtsc()
{
	W64 t;
	asm volatile ("rdtsc":"=A" (t));
	return t;
}
#endif

inline int bit(unsigned int v, unsigned int i)
{
	return (v >> i) & 1;
}

/* Options */
enum Option {
	OptSetOSD = 'a',
	OptFbDev = 'd',
	OptSetGlobalAlpha = 'g',
	OptHelp = 'h',
	OptSetLocalAlpha = 'l',
	OptNoSleep = 'n',
	OptPrepDma = 'p',
	OptSetAlpha = 'v',
	OptSetWindow = 'w',
	OptLast = 128
};

static char options[OptLast];

static struct option long_options[] = {
	/* Please keep in alphabetical order of the short option.
	   That makes it easier to see which options are still free. */
	{"device", required_argument, 0, OptFbDev},
	{"globalalpha", required_argument, 0, OptSetGlobalAlpha},
	{"help", no_argument, 0, OptHelp},
	{"localalpha", required_argument, 0, OptSetLocalAlpha},
	{"nosleep", no_argument, 0, OptNoSleep},
	{"prepdma", no_argument, 0, OptPrepDma},
	{"alpha", required_argument, 0, OptSetAlpha},
	{"window", required_argument, 0, OptSetWindow},
	{0, 0, 0, 0}
};

void usage(void)
{
	printf("Usage:\n");
	printf("  -d, --device=/dev/fbX\n");
	printf("                        set the framebuffer device used by your tuner card\n");
	printf("                        (default: /dev/fb0)\n");
	printf("  -g, --globalalpha=<on>|<off>\n");
	printf("                        toggle global alpha layer\n");
	printf("  -h, --help            display this help\n");
	printf("  -l, --localalpha=<on>|<off>\n");
	printf("                        toggle local alpha layer\n");
	printf("  -v, --alpha=<val>     sets the OSD alpha level\n");
	printf("  -w, --window=left=<val>,top=<val>,width=<val>,height=<val>\n");
	printf("                        sets the screen area to use for video\n");
	printf("Expert Options:\n");
	printf("  -n, --nosleep         don't sleep while doing prepdma\n");
	printf("  -p, --prepdma         prepare DMA\n");

	exit(0);
}

int get_state(int fd, struct ivtvfb_ioctl_state_info *state,
	      struct ivtv_osd_coords *osd)
{
	if (ioctl(fd, IVTVFB_IOCTL_GET_STATE, state) < 0 && strerror(errno) != "Success") {
		fprintf(stderr, "IVTVFB_IOCTL_GET_STATE failed (error: %s)\n",
			strerror(errno));
		return -2;
	}

	printf("Framebuffer state:\n");
	printf("  Global alpha:            %s\n",
	       (state->
		status & IVTVFB_STATUS_GLOBAL_ALPHA) ? "ENABLED" : "disabled");
	printf("  Local alpha:             %s\n",
	       (state->
		status & IVTVFB_STATUS_LOCAL_ALPHA) ? "ENABLED" : "disabled");
	printf("  Global alpha value:      %d\n", (int)state->alpha);

	printf("\n");

	return 0;
}

//++MTY TODO This is specific to the CPU being tested! Get from /proc/cpuinfo, etc. instead...
#define CPU_HZ (2.2*1000*1000*1000)

#if defined(__i386__) || defined(__x86_64__)
int ivtv_frame_loop(int fd, int do_sleep)
{
	struct ivtvfb_ioctl_dma_host_to_ivtv_args args;
	int size = (720 * 480 * 4) /*/2 */ ;
	char *buffer = (char *)mmap(NULL, size,
				    PROT_READ | PROT_WRITE,
				    MAP_PRIVATE | MAP_ANON, 0, 0);
	int rc, i;
	int val = 0;
	int iters = 1024;
	W64 tprev, tnow, tdiff;
	float fps;
	int ret = 0;

	fprintf(stderr, "frameloop: userspace buffer 0x%08lx of %d bytes:\n",
		(unsigned long)buffer, size);
	fflush(stdout);

	fprintf(stderr,
		"Warning, you must change CPU_HZ to be accurate, currently %2.0f:\n",
		CPU_HZ);
	rc = mlock(buffer, size);
	printf("mlock rc = %d\n", rc);

	for (i = 0; i < iters; i++) {
		memset(buffer, val & 0xff, size);
		val++;
		args.source = buffer;
		args.dest_offset = 0;
		args.count = size;
		tprev = rdtsc();
		if ((ret = (ioctl(fd, IVTVFB_IOCTL_PREP_FRAME, &args) < 0  && strerror(errno) != "Success"))) {
			fprintf(stderr,
				"IVTVFB_IOCTL_PREP_DMA failed (error: %s)\n",
				strerror(errno));

			/* If an EAGAIN error, things should be fine still */
			if (ret != -EAGAIN)
				return ret;
		}
		tnow = rdtsc();
		tdiff = (tnow - tprev);
		fps = (float)CPU_HZ / (float)tdiff;
		printf("iter %d: time = %f fps\n", i, fps);
		if (do_sleep)
			usleep(40000);
	}
	return 0;
}
#endif

int main(int argc, char **argv)
{
	char *filename = strdup("/dev/fb0");
	int fd, rc, i;
	int do_prep_dma = 0;
	int do_sleep = 1;
	int do_state_change = 0;
	int do_window = 0;
	int globalon=0, globaloff=0, localon=0, localoff=0, alpha=0, alphaval=0;

	char *subopts[] = {
#define SUB_LEFT		0
		"left",
#define SUB_TOP			1
		"top",
#define SUB_WIDTH		2
		"width",
#define SUB_HEIGHT		3
		"height",
#define SUB_RIGHT		4
		"right",
#define SUB_BOTTOM		5
		"bottom",
		NULL
	};

	char *value, *subs;
	struct ivtvfb_ioctl_state_info state;
	struct ivtv_osd_coords osd;
	struct ivtvfb_ioctl_set_window window = { 720, 480, 0, 0 };
	
	
	memset(&osd, 0, sizeof(struct ivtv_osd_coords));
	memset(&state, 0, sizeof(struct ivtvfb_ioctl_state_info));

	char short_options[26 * 2 + 1];
	int ch;

	if (argc < 2) {
		usage();
		return -1;
	}

	while (1) {
		int option_index = 0;
		int idx = 0;

		for (i = 0; long_options[i].name; i++) {
			if (!isalpha(long_options[i].val))
				continue;
			short_options[idx++] = long_options[i].val;
			if (long_options[i].has_arg == required_argument)
				short_options[idx++] = ':';
		}
		short_options[idx] = 0;
		ch = getopt_long(argc, argv, short_options,
				 long_options, &option_index);
		if (ch == -1)
			break;

		options[(int)ch] = 1;
		switch (ch) {
		case OptHelp:
			usage();
			return 0;
		case OptFbDev:{
			filename = strdup(optarg);
		break;
		}
		case OptNoSleep:{
			do_sleep = 0;
		break;
		}
		case OptPrepDma:{
			do_prep_dma = 1;
		break;
		}
		case OptSetGlobalAlpha:{
			if(!strcmp(optarg, "on")) {
				globalon=1;
				do_state_change = 1;		
			} else if (!strcmp(optarg, "off")) {
				globaloff=1;
				do_state_change = 1;		
			} else {
				fprintf(stderr,
					"Option --globalalpha must be either on or off\n");
				return 1;
			}
		break;
		}
		case OptSetLocalAlpha:{
			if(!strcmp(optarg, "on")) {
				localon=1;
				do_state_change = 1;		
			} else if (!strcmp(optarg, "off")) {
				localoff=1;
				do_state_change = 1;		
			} else {
				fprintf(stderr,
					"Option --localalpha must be either on or off\n");
				return 1;
			}
		break;
		}
		case OptSetWindow:
			subs = optarg;
			while (*subs != '\0') {
				switch (getsubopt(&subs, subopts, &value)) {
				case SUB_TOP:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <top>\n");
						usage();
						return 1;
					}
					window.top = strtol(value, 0L, 0);
					do_window = 1;
					break;
				case SUB_LEFT:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <left>\n");
						usage();
						return 1;
					}
					window.left = strtol(value, 0L, 0);
					do_window = 1;
					break;
				case SUB_WIDTH:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <width>\n");
						usage();
						return 1;
					}
					window.width = strtol(value, 0L, 0);
					do_window = 1;
					break;
				case SUB_HEIGHT:
					if (value == NULL) {
						fprintf(stderr,
							"No value given to suboption <height>\n");
						usage();
						return 1;
					}
					window.height = strtol(value, 0L, 0);
					do_window = 1;
					break;
				}
			}
		break;
		case OptSetAlpha:{
			alpha=1;
			alphaval = strtol(optarg, 0L, 0);
			do_state_change = 1;
			break;
		}
			
		}
	}
	if ((fd = open(filename, O_RDWR)) < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", filename,
			strerror(errno));
		return -2;
	}

	rc = get_state(fd, &state, &osd);
	if (rc)
		return rc;
	
	if (globalon) {
		state.status |= IVTVFB_STATUS_GLOBAL_ALPHA;
	} else if (globaloff) {
		state.status &= ~IVTVFB_STATUS_GLOBAL_ALPHA;
	}
	if (localon) {
		state.status |= IVTVFB_STATUS_LOCAL_ALPHA;
	} else if (localoff) {
		state.status &= ~IVTVFB_STATUS_LOCAL_ALPHA;
	} 
	if (alpha) {
		state.alpha = alphaval;
	}

	if (do_prep_dma) {
#if defined(__i386__) || defined(__x86_64__)
		ivtv_frame_loop(fd, do_sleep);
#else
		printf("ivtvfbctl: DMA prepare is currently only available on i386 and x86/64.\n");
#endif
	}
	if (do_window) {
		printf("ivtvfbctl: Setting window (left: %d top: %d width: %d height: %d)\n",
			window.left, window.top, window.width, window.height);
		fflush(stdout);
		if (ioctl(fd, IVTVFB_IOCTL_SET_WINDOW, &window) < 0 && errno != 0) {
			fprintf(stderr, "left: %d top: %d width: %d height: %d\n", window.left, window.top, window.width, window.height);
			fprintf(stderr,
				"%s: IVTVFB_IOCTL_SET_WINDOW failed (error: %s, %d)\n",
				filename, strerror(errno), errno);
			return -3;
		}
	}
	if (do_state_change) {
		if (ioctl(fd, IVTVFB_IOCTL_SET_STATE, &state) < 0 && errno != 0) {
			fprintf(stderr,
				"%s: IVTVFB_IOCTL_SET_STATE failed (error: %s)\n",
				filename, strerror(errno));
			return -3;
		}
	}

	rc = get_state(fd, &state, &osd);
	if (rc)
		return rc;

	close(fd);

	return 0;
}
