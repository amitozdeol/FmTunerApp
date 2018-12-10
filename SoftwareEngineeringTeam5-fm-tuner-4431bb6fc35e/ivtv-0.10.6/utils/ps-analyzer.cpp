/*
#
# MPEG2Parser class testing tool
#
# Copyright (C) 2001-2003 Kees Cook
# kees@outflux.net, http://outflux.net/
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
# http://www.gnu.org/copyleft/gpl.html
#
*/

/*
 * buffer management inspired by mplayer
 */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <string.h>
#include <errno.h>
#include "mpeg2structs.h"

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

static u32 g_vid_count;
static u32 g_aud_count;
static u32 g_vbi_count;

static u64 g_first_vid_pts;
static u64 g_last_vid_pts;

static int g_verbose;

static void statistics()
{
	printf("\nStatistics:\n\n");
	printf("video frames:    %u\n", g_vid_count);
	printf("audio frames:    %u\n", g_aud_count);
	printf("private packets: %u\n", g_vbi_count);
	printf("total time:      %.2f seconds\n", (g_last_vid_pts - g_first_vid_pts) / 90000.0);
}

static void psread(int fh, void *buf, int cnt, bool valid_eof = false)
{
	int res = read(fh, buf, cnt);

	if (res == cnt)
		return;
	if (res == 0 && valid_eof)
		fprintf(stderr, "end of file\n");
	else
		fprintf(stderr, "broken stream\n");
	statistics();
	exit(1);
}

static unsigned psread_u32(int fh, bool valid_eof = false)
{
    unsigned char buf[4];

    psread(fh, buf, 4, valid_eof);
    return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

static unsigned short psread_u16(int fh, bool valid_eof = false)
{
    unsigned char buf[2];

    psread(fh, buf, 2, valid_eof);
    return (buf[0] << 8) | buf[1];
}

static void vbi_decode(u8 *buf, int length)
{
	if (!memcmp(buf, "ITV0", 4)) {
		printf("36 lines ");
		return;
	}
	if (memcmp(buf, "itv0", 4)) {
		printf("unknown ");
		return;
	}
	printf("<36 lines ");
}

static void pack_header(int fh, u64 pos)
{
	unsigned char hdr[10];
	unsigned marker;
	u64 scr;
	unsigned scr_ext;
	unsigned mux_rate;

	psread(fh, hdr, 10);
	lseek64(fh, hdr[9] & 3, SEEK_CUR);  // Skip stuffing bytes

	scr = (u64)(hdr[0] & 0x38) << 27;
	scr |= (u64)(hdr[0] & 3) << 28;
	scr |= (u64)hdr[1] << 20;
	scr |= (u64)(hdr[2] & 0xf8) << 12;
	scr |= (u64)(hdr[2] & 3) << 13;
	scr |= (u64)hdr[3] << 5;
	scr |= (u64)(hdr[4] & 0xf8) >> 3;
	scr_ext = (hdr[4] & 0x1) << 8;
	scr_ext |= hdr[5];
	mux_rate = (hdr[6] & 0x7f) << 15;
	mux_rate |= hdr[7] << 7;
	mux_rate |= (hdr[8] & 0xfe) >> 1;
	if (g_verbose)
		printf("%lld: pack scr=%lld scr_ext=%u mux_rate=%u\n", pos, scr, scr_ext, mux_rate);

	marker = psread_u32(fh);
	if (marker != 0x000001bb) {
		lseek64(fh, -4, SEEK_CUR);
		return;
	}

	// System header
	unsigned short length = psread_u16(fh);

	if (g_verbose)
		printf("\tSystem header\n");
	lseek64(fh, length, SEEK_CUR);
}

static void pes_packet(int fh, int code, u16 length, u64 pos)
{
	if (code == 0xbe) {
		if (g_verbose)
			printf("\t%lld: Padding length=%d\n", pos, length);
		return;
	}
	if (code == 0xbf) {
		printf("\t%lld: Navigation packet length=%d\n", pos, length);
		return;
	}
	if (code != 0xbd && code != 0xc0 && code != 0xe0) {
		printf("\t%lld: Unknown packet %02x\n", pos, code);
		return;
	}
	
	unsigned char hdr[8];
	u64 pts = 0;
	u64 dts = 0;

	psread(fh, hdr, 8);
	if (hdr[1] & 0x80) {
		pts = (u64)(hdr[3] & 0xe) << 29;
		pts |= (u64)(hdr[4]) << 22;
		pts |= (u64)(hdr[5] & 0xfe) << 14;
		pts |= (u64)(hdr[6]) << 7;
		pts |= (u64)(hdr[7]) >> 1;
	}
	if (hdr[1] & 0x40) {
		dts = (u64)(hdr[8] & 0xe) << 29;
		dts |= (u64)(hdr[9]) << 22;
		dts |= (u64)(hdr[10] & 0xfe) << 14;
		dts |= (u64)(hdr[11]) << 7;
		dts |= (u64)(hdr[12]) >> 1;
	}
	if (pts == 0)
		return;
	if (code == 0xc0) {
		printf("\t%lld: Audio ", pos);
		g_aud_count++;
	}
	else if (code == 0xbd) {
		u8 buf[2048];
		printf("\t%lld: Private Packet ", pos);
		if (length <= sizeof(buf) + 8) {
			psread(fh, buf, length - 8);
			vbi_decode(buf + 2, length - 10);
		}
		g_vbi_count++;
	}
	else {
		printf("\t%lld: Video ", pos);
		if (g_vid_count++ == 0)
			g_first_vid_pts = pts;
		g_last_vid_pts = pts;
	}
	if (dts)
		printf("pts=%llu dts=%llu length=%d\n", pts, dts, length);
	else
		printf("pts=%llu length=%d\n", pts, length);
}

int main(int argc, char *argv[])
{
	int fh;
	int opt;
	u64 pos = 0;
	u64 len;

	for (;;) {
		opt = getopt(argc, argv, "vh");

		if (opt == -1)
			break;
		switch (opt) {
		case 'v':
			g_verbose = 1;
			break;

		case 'h':
			printf("usage: %s [-v] [-h] mpeg-file\n", argv[0]);
			printf("-v: verbose output\n");
			printf("-h: this help message\n");
			exit(0);
		}
	}

	if (optind != argc - 1) {
		printf("usage: %s [-v] [-h] mpeg-file\n", argv[0]);
		printf("-v: verbose output\n");
		printf("-h: this help message\n");
		return -1;
	}

	fh = open(argv[optind], O_RDONLY);
	if (fh < 0) {
		fprintf(stderr, "%s: cannot open mpeg file %s\n", argv[0], argv[optind]);
		return -2;
	}

	len = lseek64(fh, 0, SEEK_END);
	while (1) {
		lseek64(fh, pos, SEEK_SET);

		u32 marker = psread_u32(fh);

		if (marker != 0x000001ba) {
			if (marker == 0x000001b9)	// PS end marker
				break;
			fprintf(stderr, "missing pack marker (got %08x @ %lld)\n", marker, pos);
			break;
		}
		pack_header(fh, pos);
		while (1) {
			pos = lseek64(fh, 0, SEEK_CUR);
			marker = psread_u32(fh, pos == len);
			if (marker == 0x000001ba || marker == 0x000001b9)
				break;
			u16 length = psread_u16(fh);
			pes_packet(fh, marker & 0xff, length, pos);
			pos += length + 6;
			lseek64(fh, pos, SEEK_SET);
		}
	}

	close(fh);
	statistics();
	return 0;
}
