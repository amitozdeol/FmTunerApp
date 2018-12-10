/*
    Public ivtv API header
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo.com>

    VBI portions:
    Copyright (C) 2004  Hans Verkuil <hverkuil@xs4all.nl>

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

#ifndef _LINUX_IVTV_H
#define _LINUX_IVTV_H

#ifndef __KERNEL__
#define __user
#endif

/* NOTE: the ioctls in this file will eventually be replaced by v4l2 API
   ioctls. */

/* device ioctls should use the range 29-199 */
#define IVTV_IOC_START_DECODE      _IOW ('@', 29, struct ivtv_cfg_start_decode)
#define IVTV_IOC_STOP_DECODE       _IOW ('@', 30, struct ivtv_cfg_stop_decode)
#define IVTV_IOC_G_SPEED           _IOR ('@', 31, struct ivtv_speed)
#define IVTV_IOC_S_SPEED           _IOW ('@', 32, struct ivtv_speed)
#define IVTV_IOC_DEC_STEP          _IOW ('@', 33, int)
#define IVTV_IOC_DEC_FLUSH         _IOW ('@', 34, int)
#define IVTV_IOC_PAUSE_BLACK  	   _IO  ('@', 35)
#define IVTV_IOC_STOP     	   _IO  ('@', 36)
#define IVTV_IOC_PLAY     	   _IO  ('@', 37)
#define IVTV_IOC_PAUSE    	   _IO  ('@', 38)
#define IVTV_IOC_FRAMESYNC	   _IOR ('@', 39, struct ivtv_ioctl_framesync)
#define IVTV_IOC_GET_TIMING	   _IOR ('@', 40, struct ivtv_ioctl_framesync)
#define IVTV_IOC_S_SLOW_FAST       _IOW ('@', 41, struct ivtv_slow_fast)
#define IVTV_IOC_GET_FB            _IOR ('@', 44, int)
#define IVTV_IOC_S_GOP_END         _IOWR('@', 50, int)
#define IVTV_IOC_S_VBI_PASSTHROUGH _IOW ('@', 51, int)
#define IVTV_IOC_G_VBI_PASSTHROUGH _IOR ('@', 52, int)
#define IVTV_IOC_PASSTHROUGH       _IOW ('@', 53, int)
#define IVTV_IOC_PAUSE_ENCODE      _IO  ('@', 56)
#define IVTV_IOC_RESUME_ENCODE     _IO  ('@', 57)
#define IVTV_IOC_DEC_FAST_STOP     _IOW ('@', 59, int)	/* Obsolete: use IVTV_IOC_STOP_DECODE */
#define IVTV_IOC_PREP_FRAME_YUV    _IOW ('@', 60, struct ivtvyuv_ioctl_dma_host_to_ivtv_args)
#define IVTV_IOC_G_YUV_INTERLACE   _IOR ('@', 61, struct ivtv_ioctl_yuv_interlace)
#define IVTV_IOC_S_YUV_INTERLACE   _IOW ('@', 62, struct ivtv_ioctl_yuv_interlace)
#define IVTV_IOC_G_PTS             _IOR ('@', 63, u64)

struct ivtv_ioctl_framesync {
	__u32 frame;
	__u64 pts;
	__u64 scr;
};

struct ivtv_speed {
	int scale;		/* 1-?? (50 for now) */
	int smooth;		/* Smooth mode when in slow/fast mode */
	int speed;		/* 0 = slow, 1 = fast */
	int direction;		/* 0 = forward, 1 = reverse (not supportd */
	int fr_mask;		/* 0 = I, 1 = I,P, 2 = I,P,B    2 = default! */
	int b_per_gop;		/* frames per GOP (reverse only) */
	int aud_mute;		/* Mute audio while in slow/fast mode */
	int fr_field;		/* 1 = show every field, 0 = show every frame */
	int mute;		/* # of audio frames to mute on playback resume (unused, does not work) */
};

struct ivtv_slow_fast {
	int speed;		/* 0 = slow, 1 = fast */
	int scale;		/* 1-?? (50 for now) */
};

struct ivtv_cfg_start_decode {
	__u32 gop_offset;	/*Frames in GOP to skip before starting */
	__u32 muted_audio_frames;	/* #of audio frames to mute (unused, does not work) */
};

struct ivtv_cfg_stop_decode {
	int flags;		/* bit 0: 1 = hide last frame (go to black frame) after stop,
				   	  0 = show last frame
				   bit 1: 1 = wait until PTS is reached or
					      until all pending buffers are processed
					  0 = stop immediately
				 */
	__u64 pts;		/* PTS to stop at. Implies IVTV_STOP_FL_WAIT_FOR_END if non-zero. */
};
#define IVTV_STOP_FL_HIDE_FRAME    (1 << 0)
#define IVTV_STOP_FL_WAIT_FOR_END  (1 << 1)

struct ivtv_ioctl_yuv_interlace{
	int interlace_mode; /* Takes one of IVTV_YUV_MODE_xxxxxx values */
	int threshold; /* If mode is auto then if src_height <= this value treat as progressive otherwise treat as interlaced */
};

#define IVTV_YUV_MODE_INTERLACED	0x00
#define IVTV_YUV_MODE_PROGRESSIVE	0x01
#define IVTV_YUV_MODE_AUTO		0x02
#define IVTV_YUV_MODE_MASK		0x03

#define IVTV_YUV_SYNC_EVEN		0x00
#define IVTV_YUV_SYNC_ODD		0x04
#define IVTV_YUV_SYNC_MASK		0x04

/* Framebuffer external API */

struct ivtvfb_ioctl_state_info {
	unsigned long status;
	unsigned long alpha;
};

struct ivtvfb_ioctl_colorkey {
    int state;
    __u32 colorKey;
};

struct ivtvfb_ioctl_dma_host_to_ivtv_args {
	void __user *source;
	unsigned long dest_offset;
	int count;
};

struct ivtvyuv_ioctl_dma_host_to_ivtv_args {
	void *y_source;
	void *uv_source;
	unsigned int yuv_type;
	int src_x;
	int src_y;
	unsigned int src_w;
	unsigned int src_h;
	int dst_x;
	int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;
	int srcBuf_width;
	int srcBuf_height;
};

struct ivtvfb_ioctl_get_frame_buffer {
	void *mem;
	int size;
	int sizex;
	int sizey;
};

struct ivtv_osd_coords {
	unsigned long offset;
	unsigned long max_offset;
	int pixel_stride;
	int lines;
	int x;
	int y;
};

struct ivtvfb_ioctl_set_window {
	int width;
	int height;
	int left;
	int top;
};

struct ivtvfb_alpha {
	int   global_alpha_state; // 0=off : 1=on
	int   local_alpha_state;  // 0=off : 1=on
	int   color_key_state;    // 0=off : 1=on
	__u32 global_alpha;       // Current global alpha
	__u32 color_key;          // Current color key
};

/* Framebuffer ioctls should use the range 1 - 28 */
#define IVTVFB_IOCTL_GET_STATE          _IOR('@', 1, struct ivtvfb_ioctl_state_info)
#define IVTVFB_IOCTL_SET_STATE          _IOW('@', 2, struct ivtvfb_ioctl_state_info)
#define IVTVFB_IOCTL_PREP_FRAME         _IOW('@', 3, struct ivtvfb_ioctl_dma_host_to_ivtv_args)
// obsolete
#define IVTVFB_IOCTL_GET_ACTIVE_BUFFER  _IOR('@', 5, struct ivtv_osd_coords)
// obsolete
#define IVTVFB_IOCTL_SET_ACTIVE_BUFFER  _IOW('@', 6, struct ivtv_osd_coords)
// obsolete
#define IVTVFB_IOCTL_GET_FRAME_BUFFER   _IOR('@', 7, struct ivtvfb_ioctl_get_frame_buffer)
// belongs with ivtv
#define IVTVFB_IOCTL_SET_WINDOW         _IOW('@', 11, struct ivtvfb_ioctl_set_window)
#define IVTVFB_IOCTL_GET_COLORKEY       _IOW('@', 12, struct ivtvfb_ioctl_colorkey)
#define IVTVFB_IOCTL_SET_COLORKEY       _IOW('@', 13, struct ivtvfb_ioctl_colorkey)

#define IVTVFB_STATUS_ENABLED           (1 << 0)
#define IVTVFB_STATUS_GLOBAL_ALPHA      (1 << 1)
#define IVTVFB_STATUS_LOCAL_ALPHA       (1 << 2)
#define IVTVFB_STATUS_FLICKER_REDUCTION (1 << 3)

#ifdef IVTV_INTERNAL
/* Do not use these structures and ioctls in code that you want to release.
   Only to be used for testing and by the utilities ivtvctl, ivtvfbctl and fwapi. */

/* These are the VBI types as they appear in the embedded VBI private packets.
   It is very likely that this will disappear and be replaced by the DVB standard. */
#define IVTV_SLICED_TYPE_TELETEXT_B     (1)
#define IVTV_SLICED_TYPE_CAPTION_525    (4)
#define IVTV_SLICED_TYPE_WSS_625        (5)
#define IVTV_SLICED_TYPE_VPS            (7)

#define IVTVFB_IOCTL_GET_ALPHA  _IOR('@', 14, struct ivtvfb_alpha)
#define IVTVFB_IOCTL_SET_ALPHA  _IOW('@', 15, struct ivtvfb_alpha)
#define IVTV_IOC_G_INDEX        _IOR('@', 64, struct ivtv_index)
#define IVTV_IOC_S_AUDMODE      _IOW('@', 65, struct ivtv_audmode)

struct ivtv_audmode {
	__u32 dual;
	__u32 stereo;
};
#define IVTV_AUDMODE_STEREO 0
#define IVTV_AUDMODE_LEFT   1
#define IVTV_AUDMODE_RIGHT  2
#define IVTV_AUDMODE_MONO   3
#define IVTV_AUDMODE_SWAP   4
#define IVTV_AUDMODE_UNCHANGED -1

#define IVTV_IDX_FL_P 0
#define IVTV_IDX_FL_I 1
#define IVTV_IDX_FL_B 2

struct ivtv_index_entry {
	__u64 offset;
	__u64 pts;
	__u32 length;
	__u32 flags;
};

#define IVTV_INDEX_MAX_ENTRIES (64)
struct ivtv_index {
	__u32 entries;
	struct ivtv_index_entry entry[IVTV_INDEX_MAX_ENTRIES];
};

/* Debug flags */
#define IVTV_DBGFLG_WARN  (1 << 0)
#define IVTV_DBGFLG_INFO  (1 << 1)
#define IVTV_DBGFLG_API   (1 << 2)
#define IVTV_DBGFLG_DMA   (1 << 3)
#define IVTV_DBGFLG_IOCTL (1 << 4)
#define IVTV_DBGFLG_I2C   (1 << 5)
#define IVTV_DBGFLG_IRQ   (1 << 6)
#define IVTV_DBGFLG_DEC   (1 << 7)
#define IVTV_DBGFLG_YUV   (1 << 8)
/* Flag to turn on high volume debugging */
#define IVTV_DBGFLG_HIGHVOL (1 << 9)

#endif /* IVTV_INTERNAL */

#endif /* _LINUX_IVTV_H */
