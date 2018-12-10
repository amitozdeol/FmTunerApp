/*
    On Screen Display cx23415 Framebuffer driver
    
    Copyright (C) 2006  Ian Armstrong <ian@iarmst.demon.co.uk>

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

#define IVTV_OSD_MAX_WIDTH  720
#define IVTV_OSD_MAX_HEIGHT 576

#define IVTV_OSD_BPP_8      0x00
#define IVTV_OSD_BPP_16_444 0x03 
#define IVTV_OSD_BPP_16_555 0x02
#define IVTV_OSD_BPP_16_565 0x01 
#define IVTV_OSD_BPP_32     0x04

struct osd_info {
	// Physical base address
	unsigned long video_pbase;
	// Relative base address (relative to start of decoder memory)
    	u32 video_rbase;
	// Mapped base address
	volatile char __iomem *video_vbase;
	// Buffer size
	u32 video_buffer_size;

#ifdef CONFIG_MTRR
	/* video_base rounded down as required by hardware MTRRs */
	unsigned long fb_start_aligned_physaddr;
	/* video_base rounded up as required by hardware MTRRs */
	unsigned long fb_end_aligned_physaddr;
#endif

	// Alpha settings
	struct ivtvfb_alpha alpha;
	
	// Store the buffer offset
	int set_osd_coords_x;
	int set_osd_coords_y;
	
	// Current dimensions (NOT VISIBLE SIZE!)
	int display_width;
	int display_height;
	int display_byte_stride;
	
	// Current bits per pixel
	int bits_per_pixel;
	int bytes_per_pixel;
	
	// Frame buffer stuff
	struct fb_info ivtvfb_info;
	struct fb_var_screeninfo ivtvfb_defined;
	struct fb_fix_screeninfo ivtvfb_fix;
};
