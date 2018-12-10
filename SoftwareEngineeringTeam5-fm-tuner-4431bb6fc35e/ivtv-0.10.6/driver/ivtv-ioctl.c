/*
    ioctl system call
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo.com>

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

#include "ivtv-driver.h"
#include "ivtv-version.h"
#include "ivtv-mailbox.h"
#include "ivtv-i2c.h"
#include "ivtv-queue.h"
#include "ivtv-fileops.h"
#include "ivtv-vbi.h"
#include "ivtv-audio.h"
#include "ivtv-video.h"
#include "ivtv-streams.h"
#include "ivtv-yuv.h"
#include "ivtv-ioctl.h"
#include "ivtv-gpio.h"
#include "ivtv-controls.h"
#include "ivtv-cards.h"
#include <media/saa7127.h>
#include <media/tveeprom.h>
#include <linux/i2c-id.h>

#ifdef VIDIOC_DBG_S_REGISTER
#define VIDIOC_INT_S_REGISTER VIDIOC_DBG_S_REGISTER
#define VIDIOC_INT_G_REGISTER VIDIOC_DBG_G_REGISTER
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
/* Differs from the VIDIOC_INT_S_REGISTER from v4l2-common.h in the
   2.6.18 kernel due to a bug in that header. ivtvctl issues the
   corrected version below, but the VIDIOC_INT_S_REGISTER ioctl is
   what must be passed to the i2c modules */
#define IVTV_INT_S_REGISTER           _IOW ('d', 100, struct v4l2_register)
#endif

u16 service2vbi(int type)
{
        switch (type) {
                case V4L2_SLICED_TELETEXT_B:
                        return IVTV_SLICED_TYPE_TELETEXT_B;
                case V4L2_SLICED_CAPTION_525:
                        return IVTV_SLICED_TYPE_CAPTION_525;
                case V4L2_SLICED_WSS_625:
                        return IVTV_SLICED_TYPE_WSS_625;
                case V4L2_SLICED_VPS:
                        return IVTV_SLICED_TYPE_VPS;
                default:
                        return 0;
        }
}

static int valid_service_line(int field, int line, int is_pal)
{
        return (is_pal && line >= 6 && (line != 23 || field == 0)) ||
               (!is_pal && line >= 10 && line < 22);
}

static u16 select_service_from_set(int field, int line, u16 set, int is_pal)
{
        u16 valid_set = (is_pal ? V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525);
        int i;

        set = set & valid_set;
        if (set == 0 || !valid_service_line(field, line, is_pal)) {
                return 0;
        }
        if (!is_pal) {
                if (line == 21 && (set & V4L2_SLICED_CAPTION_525))
                        return V4L2_SLICED_CAPTION_525;
        }
        else {
                if (line == 16 && field == 0 && (set & V4L2_SLICED_VPS))
                        return V4L2_SLICED_VPS;
                if (line == 23 && field == 0 && (set & V4L2_SLICED_WSS_625))
                        return V4L2_SLICED_WSS_625;
                if (line == 23)
                        return 0;
        }
        for (i = 0; i < 32; i++) {
                if ((1 << i) & set)
                        return 1 << i;
        }
        return 0;
}

void expand_service_set(struct v4l2_sliced_vbi_format *fmt, int is_pal)
{
        u16 set = fmt->service_set;
        int f, l;

        fmt->service_set = 0;
        for (f = 0; f < 2; f++) {
                for (l = 0; l < 24; l++) {
                        fmt->service_lines[f][l] = select_service_from_set(f, l, set, is_pal);
                }
        }
}

static int check_service_set(struct v4l2_sliced_vbi_format *fmt, int is_pal)
{
        int f, l;
        u16 set = 0;

        for (f = 0; f < 2; f++) {
                for (l = 0; l < 24; l++) {
                        fmt->service_lines[f][l] = select_service_from_set(f, l, fmt->service_lines[f][l], is_pal);
                        set |= fmt->service_lines[f][l];
                }
        }
        return set != 0;
}

u16 get_service_set(struct v4l2_sliced_vbi_format *fmt)
{
        int f, l;
        u16 set = 0;

        for (f = 0; f < 2; f++) {
                for (l = 0; l < 24; l++) {
                        set |= fmt->service_lines[f][l];
                }
        }
        return set;
}

static const struct {
	v4l2_std_id  std;
	char        *name;
} enum_stds[] = {
	{ V4L2_STD_PAL_BG | V4L2_STD_PAL_H, "PAL-BGH" },
	{ V4L2_STD_PAL_DK,    "PAL-DK"    },
	{ V4L2_STD_PAL_I,     "PAL-I"     },
	{ V4L2_STD_PAL_M,     "PAL-M"     },
	{ V4L2_STD_PAL_N,     "PAL-N"     },
	{ V4L2_STD_PAL_Nc,    "PAL-Nc"    },
	{ V4L2_STD_SECAM_B | V4L2_STD_SECAM_G | V4L2_STD_SECAM_H, "SECAM-BGH" },
	{ V4L2_STD_SECAM_DK,  "SECAM-DK"  },
	{ V4L2_STD_SECAM_L,   "SECAM-L"   },
	{ V4L2_STD_SECAM_LC,  "SECAM-L'"  },
	{ V4L2_STD_NTSC_M,    "NTSC-M"    },
	{ V4L2_STD_NTSC_M_JP, "NTSC-J"    },
	{ V4L2_STD_NTSC_M_KR, "NTSC-K"    },
};

static const struct v4l2_standard ivtv_std_60hz = 
{
	.frameperiod = {.numerator = 1001, .denominator = 30000},
	.framelines = 525,
};

static const struct v4l2_standard ivtv_std_50hz = 
{
	.frameperiod = {.numerator = 1, .denominator = 25},
	.framelines = 625,
};

static void ivtv_stream_off(struct ivtv *itv, struct ivtv_stream *s)
{
        IVTV_DEBUG_INFO("Entering STREAMOFF\n");

       /* Special case: a running VBI capture for VBI insertion
           in the mpeg stream. Need to stop that too. */
        if (itv->vbi.insert_mpeg) {
                ivtv_stop_capture(&itv->streams[IVTV_ENC_STREAM_TYPE_VBI]);
                ivtv_release_stream(itv->streams + IVTV_ENC_STREAM_TYPE_VBI);
        }

        /* Prevent others from messing around with streams until
           we've finished here. */
        set_bit(IVTV_F_S_STREAMOFF, &s->s_flags);
        ivtv_stop_capture(s);
}

static int ivtv_stream_on(struct ivtv *itv, struct ivtv_stream *stream)
{
       	struct ivtv_stream *vbi_stream;

        IVTV_DEBUG_INFO("Entering STREAMON\n");

	/* If capture is already in progress, then we have to
	   do nothing. */
	if (test_and_set_bit(IVTV_F_S_STREAMING, &stream->s_flags))
		return 0;

	/* Start VBI capture if required */
       	vbi_stream = &itv->streams[IVTV_ENC_STREAM_TYPE_VBI];
        if (stream->type == IVTV_ENC_STREAM_TYPE_MPG &&
            test_bit(IVTV_F_S_INTERNAL_USE, &vbi_stream->s_flags) &&
            !test_and_set_bit(IVTV_F_S_STREAMING, &vbi_stream->s_flags)) {
                /* Note: the IVTV_ENC_STREAM_TYPE_VBI is claimed
                   automatically when the MPG stream is claimed. 
                   We only need to start the VBI capturing. */
        	if (ivtv_start_v4l2_encode_stream(vbi_stream)) {
               		IVTV_DEBUG_WARN("VBI capture start failed\n");

                	/* Failure, clean up and return an error */
        		clear_bit(IVTV_F_S_STREAMING, &vbi_stream->s_flags);
			clear_bit(IVTV_F_S_STREAMING, &stream->s_flags);
                        /* also releases the associated VBI stream */
			ivtv_release_stream(stream);
                	return -EIO;
                }
               	IVTV_DEBUG_INFO("VBI insertion started\n");
	}

	/* Tell the card to start capturing */
	if (!ivtv_start_v4l2_encode_stream(stream)) {
        	/* We're done */
	        return 0;
        }

        /* failure, clean up */
        IVTV_DEBUG_WARN("Failed to start capturing for stream %d\n", stream->type);

        /* Note: the IVTV_ENC_STREAM_TYPE_VBI is released
           automatically when the MPG stream is released. 
           We only need to stop the VBI capturing. */
        if (stream->type == IVTV_ENC_STREAM_TYPE_MPG &&
            test_bit(IVTV_F_S_STREAMING, &vbi_stream->s_flags)) {
                ivtv_stop_capture(vbi_stream);
                clear_bit(IVTV_F_S_STREAMING, &vbi_stream->s_flags);
        }
        clear_bit(IVTV_F_S_STREAMING, &stream->s_flags);
        ivtv_release_stream(stream);
        return -EIO;
}

int ivtv_s_speed(struct ivtv *itv, struct ivtv_speed *speed)
{
	u32 data[CX2341X_MBOX_MAX_DATA];
	int type;
	struct ivtv_stream *st;
	DEFINE_WAIT(wait);

	/* YUV or MPG decoding stream */
	if (test_bit(IVTV_F_I_DEC_YUV, &itv->i_flags))
		type = IVTV_DEC_STREAM_TYPE_YUV;
	else
		type = IVTV_DEC_STREAM_TYPE_MPG;
	st = &itv->streams[type];

	/* Initialize */
	if ((speed->scale < 0) || (speed->scale > 50)) {
		IVTV_DEBUG_WARN("Error with scale 0x%08x\n", speed->scale);
		return -EINVAL;
	}

	if ((speed->speed < 0) || (speed->speed > 1)) {
		IVTV_DEBUG_WARN("Error with speed 0x%08x\n", speed->speed);
		return -EINVAL;
	}

	if ((speed->fr_mask < 0) || (speed->fr_mask > 2)) {
		IVTV_DEBUG_WARN("Error with Frame Mask 0x%08x\n", speed->fr_mask);
		return -EINVAL;
	}

	data[0] = speed->scale;

	if (speed->smooth)	/* smooth ff */
		data[0] |= 0x40000000;

	if (speed->speed)	/* fast forward */
		data[0] |= 0x80000000;

	if (speed->b_per_gop < 0)
		speed->b_per_gop = itv->params.video_b_frames;

	data[1] = speed->direction;	/* Forward. Reverse not supported */

	/* Unfortunately we translate and the original i/bp settings conflict */
	switch (speed->fr_mask) {
	case 0:
		data[2] = 1;	/* I */
		break;
	case 1:
	case 3:
		data[2] = 3;	/* BP */
		break;
	case 2:
	case 7:
	default:
		data[2] = 7;	/* IBP */
		break;
        }
	data[3] = speed->b_per_gop;
	data[4] = speed->aud_mute;	/* mute while fast/slow */
	data[5] = speed->fr_field;	/* frame or field at a time */
	data[6] = 0;	/* # of frames to mute on normal speed resume (unimplemented in the fw) */

        /* If not decoding, just change speed setting */
        if (atomic_read(&itv->decoding) > 0) {
		int got_sig = 0;

                /* Stop all DMA and decoding activity */
                ivtv_vapi(itv, CX2341X_DEC_PAUSE_PLAYBACK, 1, 0);

                /* Wait for any DMA to finish */
		prepare_to_wait(&itv->dma_waitq, &wait, TASK_INTERRUPTIBLE);
		while (itv->i_flags & IVTV_F_I_DMA) {
			got_sig = signal_pending(current);
			if (got_sig)
				break;
			got_sig = 0;
			schedule();
		}
		finish_wait(&itv->dma_waitq, &wait);
		if (got_sig)
			return -EINTR;

		/* Change Speed safely */
		ivtv_api(itv, CX2341X_DEC_SET_PLAYBACK_SPEED, 7, data);
	}
	/* Save speed options if call succeeded */
	memcpy(&itv->dec_options.speed, speed, sizeof(*speed));

	IVTV_DEBUG_INFO("Setting Speed to 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		   data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
	return 0;
}

static int ivtv_itvc(struct ivtv *itv, unsigned int cmd, void *arg)
{
	struct v4l2_register *regs = arg;
        unsigned long flags;
        volatile u8 __iomem *reg_start;

        if (regs->reg >= IVTV_REG_OFFSET && regs->reg < IVTV_REG_OFFSET + IVTV_REG_SIZE)
                reg_start = itv->reg_mem - IVTV_REG_OFFSET;
        else if (itv->has_cx23415 && regs->reg >= IVTV_DECODER_OFFSET &&
			regs->reg < IVTV_DECODER_OFFSET + IVTV_DECODER_SIZE)
                reg_start = itv->dec_mem - IVTV_DECODER_OFFSET;
        else if (regs->reg >= 0 && regs->reg < IVTV_ENCODER_SIZE)
                reg_start = itv->enc_mem;
        else 
                return -EINVAL;

	spin_lock_irqsave(&ivtv_cards_lock, flags);
	if (cmd == VIDIOC_INT_G_REGISTER) {
		regs->val = readl(regs->reg + reg_start);
	} else {
		writel(regs->val, regs->reg + reg_start);
	}
	spin_unlock_irqrestore(&ivtv_cards_lock, flags);
        return 0;
}

static int ivtv_get_fmt(struct ivtv *itv, int streamtype, struct v4l2_format *fmt)
{
        switch (fmt->type) {
        /* case 0:	 Works around a bug in MythTV (already fixed).
           This case should be removed once post 0.1.10
           versions of ivtv have become mainstream. */
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		/* fall through */
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
                fmt->fmt.pix.width = itv->params.width;
                fmt->fmt.pix.height = itv->params.height;
                fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
                fmt->fmt.pix.field = V4L2_FIELD_INTERLACED;
                if (streamtype == IVTV_ENC_STREAM_TYPE_YUV ||
                                streamtype == IVTV_DEC_STREAM_TYPE_YUV) {
                        fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_HM12;
                        /* YUV size is (Y=(h*w) + UV=(h*(w/2))) */
                        fmt->fmt.pix.sizeimage =
                                fmt->fmt.pix.height * fmt->fmt.pix.width +
                                fmt->fmt.pix.height * (fmt->fmt.pix.width / 2);
                } else {
                        fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_MPEG;
                        fmt->fmt.pix.sizeimage = 128 * 1024;
                }
                break;

        case V4L2_BUF_TYPE_VBI_CAPTURE:
                fmt->fmt.vbi.sampling_rate = 27000000;
                fmt->fmt.vbi.offset = 248;
                fmt->fmt.vbi.samples_per_line = itv->vbi.raw_decoder_line_size - 4;
                fmt->fmt.vbi.sample_format = V4L2_PIX_FMT_GREY;
                fmt->fmt.vbi.start[0] = itv->vbi.start[0];
                fmt->fmt.vbi.start[1] = itv->vbi.start[1];
                fmt->fmt.vbi.count[0] = fmt->fmt.vbi.count[1] = itv->vbi.count;
                /* If this ioctl is used, then the caller
                   wants RAW format. Ugly hack until
                   the sliced VBI API is in V4L2. 
                   [ I think this is no longer required ]
                if (atomic_read(&itv->capturing) == 0) {
                        itv->vbi.in.raw = 1;
                }*/
                break;

        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
        {
                struct v4l2_sliced_vbi_format *vbifmt = &fmt->fmt.sliced;

		if (!(itv->v4l2_cap & V4L2_CAP_SLICED_VBI_OUTPUT))
			return -EINVAL;
                vbifmt->io_size = sizeof(struct v4l2_sliced_vbi_data) * 36;
                memset(vbifmt->reserved, 0, sizeof(vbifmt->reserved));
                memset(vbifmt->service_lines, 0, sizeof(vbifmt->service_lines));
                if (itv->is_60hz) {
                        vbifmt->service_lines[0][21] = V4L2_SLICED_CAPTION_525;
                        vbifmt->service_lines[1][21] = V4L2_SLICED_CAPTION_525;
                } else {
                        vbifmt->service_lines[0][23] = V4L2_SLICED_WSS_625;
                        vbifmt->service_lines[0][16] = V4L2_SLICED_VPS;
                }
                vbifmt->service_set = get_service_set(vbifmt);
                break;
        }

        case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE: 
        {
                struct v4l2_sliced_vbi_format *vbifmt = &fmt->fmt.sliced;

                vbifmt->io_size = sizeof(struct v4l2_sliced_vbi_data) * 36;
                memset(vbifmt->reserved, 0, sizeof(vbifmt->reserved));
                memset(vbifmt->service_lines, 0, sizeof(vbifmt->service_lines));

                if (streamtype == IVTV_DEC_STREAM_TYPE_VBI) {
                        vbifmt->service_set = itv->is_50hz ? V4L2_SLICED_VBI_625 :
                                                 V4L2_SLICED_VBI_525;
                        expand_service_set(vbifmt, itv->is_50hz);
                        break;
                }

                itv->video_dec_func(itv, VIDIOC_G_FMT, fmt);
                vbifmt->service_set = get_service_set(vbifmt);
                break;
        }
        case V4L2_BUF_TYPE_VBI_OUTPUT:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        default:
                return -EINVAL;
        }
        return 0;
}

static int ivtv_try_or_set_fmt(struct ivtv *itv, int streamtype,
                struct v4l2_format *fmt, int set_fmt)
{
        struct v4l2_sliced_vbi_format *vbifmt = &fmt->fmt.sliced;
        u16 set;

        if (fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) 
                return ivtv_get_fmt(itv, streamtype, fmt);

        // set window size
        if (fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		struct cx2341x_mpeg_params *p = &itv->params;
		int w = fmt->fmt.pix.width;
		int h = fmt->fmt.pix.height;

		if (w > 720) w = 720;
		else if (w < 1) w = 1;
		if (h > (itv->is_50hz ? 576 : 480)) h = (itv->is_50hz ? 576 : 480);
		else if (h < 2) h = 2;
		ivtv_get_fmt(itv, streamtype, fmt);
		fmt->fmt.pix.width = w;
		fmt->fmt.pix.height = h;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
                if (p->width != 720 || p->height != (itv->is_50hz ? 576 : 480))
                        p->video_temporal_filter = 0;
                else
                        p->video_temporal_filter = 8;
#endif

                if (!set_fmt || (p->width == w && p->height == h))
			return 0;
		if (atomic_read(&itv->capturing) > 0)
                        return -EBUSY;

                p->width = w;
                p->height = h;
		if (w != 720 || h != (itv->is_50hz ? 576 : 480))
			p->video_temporal_filter = 0;
		else
			p->video_temporal_filter = 8;
		if (p->video_encoding == V4L2_MPEG_VIDEO_ENCODING_MPEG_1)
			fmt->fmt.pix.width /= 2;
                itv->video_dec_func(itv, VIDIOC_S_FMT, fmt);
                return ivtv_get_fmt(itv, streamtype, fmt);
        }
       
        // set raw VBI format
        if (fmt->type == V4L2_BUF_TYPE_VBI_CAPTURE) {
                if (set_fmt && streamtype == IVTV_ENC_STREAM_TYPE_VBI &&
                    itv->vbi.sliced_in->service_set &&
                    atomic_read(&itv->capturing) > 0) {
                        return -EBUSY;
                }
		if (set_fmt) {
			itv->vbi.sliced_in->service_set = 0;
			itv->video_dec_func(itv, VIDIOC_S_FMT, &itv->vbi.in);
		}
                return ivtv_get_fmt(itv, streamtype, fmt);
        }
       
        // set sliced VBI output
        // In principle the user could request that only certain
        // VBI types are output and that the others are ignored.
        // I.e., suppress CC in the even fields or only output
        // WSS and no VPS. Currently though there is no choice.
        if (fmt->type == V4L2_BUF_TYPE_SLICED_VBI_OUTPUT) 
                return ivtv_get_fmt(itv, streamtype, fmt);

        // any else but sliced VBI capture is an error
        if (fmt->type != V4L2_BUF_TYPE_SLICED_VBI_CAPTURE) 
                return -EINVAL;

        if (streamtype == IVTV_DEC_STREAM_TYPE_VBI) 
                return ivtv_get_fmt(itv, streamtype, fmt);

        // set sliced VBI capture format
        vbifmt->io_size = sizeof(struct v4l2_sliced_vbi_data) * 36;
        memset(vbifmt->reserved, 0, sizeof(vbifmt->reserved));

        if (vbifmt->service_set)
                expand_service_set(vbifmt, itv->is_50hz);
        set = check_service_set(vbifmt, itv->is_50hz);
        vbifmt->service_set = get_service_set(vbifmt);

        if (!set_fmt)
                return 0;
        if (set == 0)
                return -EINVAL;
        if (atomic_read(&itv->capturing) > 0 && itv->vbi.sliced_in->service_set == 0) {
                return -EBUSY;
        }
        itv->video_dec_func(itv, VIDIOC_S_FMT, fmt);
	memcpy(itv->vbi.sliced_in, vbifmt, sizeof(*itv->vbi.sliced_in));
        return 0;
}


static int ivtv_internal_ioctls(struct ivtv *itv, int streamtype, unsigned int cmd,
			 void *arg)
{
	struct v4l2_register *reg = arg;

	switch (cmd) {
	/* ioctls to allow direct access to the encoder registers for testing */
	case VIDIOC_INT_G_REGISTER:
		IVTV_DEBUG_IOCTL("VIDIOC_INT_G_REGISTER\n");
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 20)
		if (reg->i2c_id)
			return ivtv_i2c_id(itv, reg->i2c_id, cmd, reg);
#else
		if (reg->match_type == V4L2_CHIP_MATCH_I2C_DRIVER)
			return ivtv_i2c_id(itv, reg->match_chip, cmd, reg);
#endif
		return ivtv_itvc(itv, cmd, arg);

	case VIDIOC_INT_S_REGISTER:
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
	case IVTV_INT_S_REGISTER:
		cmd = VIDIOC_INT_S_REGISTER;
#endif
		IVTV_DEBUG_IOCTL("VIDIOC_INT_S_REGISTER\n");
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 20)
		if (reg->i2c_id)
			return ivtv_i2c_id(itv, reg->i2c_id, cmd, reg);
#else
		if (reg->match_type == V4L2_CHIP_MATCH_I2C_DRIVER)
			return ivtv_i2c_id(itv, reg->match_chip, cmd, reg);
#endif
		return ivtv_itvc(itv, cmd, arg);

	case VIDIOC_INT_S_AUDIO_ROUTING: {
		struct v4l2_routing *route = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_INT_S_AUDIO_ROUTING\n");
		ivtv_audio_set_route(itv, route);
		break;
	}

	case VIDIOC_INT_RESET:
		IVTV_DEBUG_IOCTL("VIDIOC_INT_RESET\n");
		ivtv_reset_ir_gpio(itv);
		break;	
        
	default:
		IVTV_DEBUG_IOCTL( "Unknown internal IVTV command %08x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

int ivtv_v4l2_ioctls(struct ivtv *itv, struct ivtv_open_id *id,
		     int streamtype, unsigned int cmd, void *arg)
{
	struct ivtv_stream *stream = &itv->streams[streamtype];

	switch (cmd) {
	case VIDIOC_QUERYCAP:{
		struct v4l2_capability *vcap = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_QUERYCAP\n");

		memset(vcap, 0, sizeof(*vcap));
		strcpy(vcap->driver, IVTV_DRIVER_NAME);     /* driver name */
		strcpy(vcap->card, itv->card_name); 	    /* card type */
                strcpy(vcap->bus_info, pci_name(itv->dev)); /* bus info... */
		vcap->version = IVTV_DRIVER_VERSION; 	    /* version */
		vcap->capabilities = itv->v4l2_cap; 	    /* capabilities */

		/* reserved.. must set to 0! */
		vcap->reserved[0] = vcap->reserved[1] = 
			vcap->reserved[2] = vcap->reserved[3] = 0;
		break;
	}

	case VIDIOC_ENUMAUDIO:{
		struct v4l2_audio *vin = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_ENUMAUDIO\n");

		return ivtv_get_audio_input(itv, vin->index, vin);
	}

	case VIDIOC_G_AUDIO:{
		struct v4l2_audio *vin = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_G_AUDIO\n");
		vin->index = itv->audio_input;
		return ivtv_get_audio_input(itv, vin->index, vin);
	}

	case VIDIOC_S_AUDIO:{
		struct v4l2_audio *vout = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_S_AUDIO\n");

		if (vout->index >= itv->nof_audio_inputs)
			return -EINVAL;
		itv->audio_input = vout->index;
		ivtv_audio_set_io(itv);
		break;
	}

	case VIDIOC_ENUMAUDOUT:{
		struct v4l2_audioout *vin = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_ENUMAUDOUT\n");

		/* set it to defaults from our table */
		return ivtv_get_audio_output(itv, vin->index, vin);
	}

	case VIDIOC_G_AUDOUT:{
		struct v4l2_audioout *vin = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_G_AUDOUT\n");
		vin->index = 0;
		return ivtv_get_audio_output(itv, vin->index, vin);
	}

	case VIDIOC_S_AUDOUT:{
		struct v4l2_audioout *vout = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_S_AUDOUT\n");

		return ivtv_get_audio_output(itv, vout->index, vout);
	}

	case VIDIOC_ENUMINPUT:{
		struct v4l2_input *vin = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_ENUMINPUT\n");

		/* set it to defaults from our table */
		return ivtv_get_input(itv, vin->index, vin);
	}

	case VIDIOC_ENUMOUTPUT:{
		struct v4l2_output *vout = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_ENUMOUTPUT\n");

		return ivtv_get_output(itv, vout->index, vout);
	}

	case VIDIOC_TRY_FMT:
	case VIDIOC_S_FMT: {
		struct v4l2_format *fmt = arg;

                if (cmd == VIDIOC_S_FMT) {
                        IVTV_DEBUG_IOCTL("VIDIOC_S_FMT\n");
                } else {
                        IVTV_DEBUG_IOCTL("VIDIOC_TRY_FMT\n");
                }
                return ivtv_try_or_set_fmt(itv, streamtype, fmt, cmd == VIDIOC_S_FMT);
	}

	case VIDIOC_G_FMT: {
		struct v4l2_format *fmt = arg;
		int type = fmt->type;

		IVTV_DEBUG_IOCTL("VIDIOC_G_FMT\n");
		memset(fmt, 0, sizeof(*fmt));
		fmt->type = type;
                return ivtv_get_fmt(itv, streamtype, fmt);
        }

        case VIDIOC_S_CROP: {
                struct v4l2_crop *crop = arg;

                IVTV_DEBUG_IOCTL("VIDIOC_S_CROP\n");
		if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		return itv->video_dec_func(itv, VIDIOC_S_CROP, arg);
        }

        case VIDIOC_G_CROP: {
                struct v4l2_crop *crop = arg;

                IVTV_DEBUG_IOCTL("VIDIOC_G_CROP\n");
		if (crop->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		return itv->video_dec_func(itv, VIDIOC_G_CROP, arg);
        }

	case VIDIOC_ENUM_FMT: {
		static struct v4l2_fmtdesc formats[] = {
			{ 0, 0, 0,
			  "HM12 (YUV 4:1:1)", V4L2_PIX_FMT_HM12,
			  { 0, 0, 0, 0 }
			},
			{ 1, 0, V4L2_FMT_FLAG_COMPRESSED,
			  "MPEG", V4L2_PIX_FMT_MPEG,
			  { 0, 0, 0, 0 }
			}
		};
		struct v4l2_fmtdesc *fmt = arg;
		enum v4l2_buf_type type = fmt->type;

		switch (type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT:
			if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}
		if (fmt->index > 1)
			return -EINVAL;
		*fmt = formats[fmt->index];
		fmt->type = type;
		return 0;
	}

	case VIDIOC_G_INPUT:{
		IVTV_DEBUG_IOCTL("VIDIOC_G_INPUT\n");

		*(int *)arg = itv->active_input;
		break;
	}

	case VIDIOC_S_INPUT:{
		int inp = *(int *)arg;

		IVTV_DEBUG_IOCTL("VIDIOC_S_INPUT\n");

		if (inp < 0 || inp >= itv->nof_inputs)
			return -EINVAL;

		if (inp == itv->active_input) {
			IVTV_DEBUG_INFO("Input unchanged\n");
			break;
		}
		if (atomic_read(&itv->capturing) > 0) {
			return -EBUSY;
		}
		IVTV_DEBUG_INFO("Changing input from %d to %d\n",
			       	itv->active_input, inp);

		itv->active_input = inp;
		/* Set the audio input to whatever is appropriate for the
		   input type. */
		itv->audio_input = itv->card->video_inputs[inp].audio_index;

		/* prevent others from messing with the streams until
		   we're finished changing inputs. */
		ivtv_mute(itv);
		ivtv_video_set_io(itv);
		ivtv_audio_set_io(itv);
		ivtv_unmute(itv);
		break;
	}

	case VIDIOC_G_OUTPUT:{
		IVTV_DEBUG_IOCTL("VIDIOC_G_OUTPUT\n");

		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		*(int *)arg = itv->active_output;
		break;
	}

	case VIDIOC_S_OUTPUT:{
		int outp = *(int *)arg;
		struct v4l2_routing route;

		IVTV_DEBUG_IOCTL("VIDIOC_S_OUTPUT\n");

		if (outp >= itv->card->nof_outputs)
			return -EINVAL;

		if (outp == itv->active_output) {
			IVTV_DEBUG_INFO("Output unchanged\n");
			break;
		}
		IVTV_DEBUG_INFO("Changing output from %d to %d\n",
			   itv->active_output, outp);

		itv->active_output = outp;
		route.input = SAA7127_INPUT_TYPE_NORMAL;
		route.output = itv->card->video_outputs[outp].video_output;
		ivtv_saa7127(itv, VIDIOC_INT_S_VIDEO_ROUTING, &route);
		break;
	}

	case VIDIOC_G_FREQUENCY:{
		struct v4l2_frequency *vf = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_G_FREQUENCY\n");

		if (vf->tuner != 0)
			return -EINVAL;
		ivtv_call_i2c_clients(itv, cmd, arg);
		break;
	}

	case VIDIOC_S_FREQUENCY:{
		struct v4l2_frequency vf = *(struct v4l2_frequency *)arg;

		IVTV_DEBUG_IOCTL("VIDIOC_S_FREQUENCY\n");

		if (vf.tuner != 0)
			return -EINVAL;

		ivtv_mute(itv);
		IVTV_DEBUG_INFO("v4l2 ioctl: set frequency %d\n", vf.frequency);
		ivtv_call_i2c_clients(itv, cmd, &vf);
		ivtv_unmute(itv);
		break;
	}

	case VIDIOC_ENUMSTD:{
		struct v4l2_standard *vs = arg;
		int idx = vs->index;

		IVTV_DEBUG_IOCTL("VIDIOC_ENUMSTD\n");

		if (idx < 0 || idx >= ARRAY_SIZE(enum_stds))
			return -EINVAL;

		*vs = (enum_stds[idx].std & V4L2_STD_525_60) ? 
				ivtv_std_60hz : ivtv_std_50hz;
		vs->index = idx;
		vs->id = enum_stds[idx].std;
		strcpy(vs->name, enum_stds[idx].name);
		break;
	}

	case VIDIOC_G_STD:{
		IVTV_DEBUG_IOCTL("VIDIOC_G_STD\n");
		*(v4l2_std_id *) arg = itv->std;
		break;
	}

	case VIDIOC_S_STD: {
		v4l2_std_id std = *(v4l2_std_id *) arg;

		IVTV_DEBUG_IOCTL("VIDIOC_S_STD\n");

		if ((std & V4L2_STD_ALL) == 0)
			return -EINVAL;

		if (std == itv->std)
			break;

		if (test_bit(IVTV_F_I_RADIO_USER, &itv->i_flags) ||
		    atomic_read(&itv->capturing) > 0 ||
		    atomic_read(&itv->decoding) > 0) {
			/* Switching standard would turn off the radio or mess
			   with already running streams, prevent that by
			   returning EBUSY. */
			return -EBUSY;
		}

		itv->std = std;
                itv->is_60hz = (std & V4L2_STD_525_60) ? 1 : 0;
                itv->params.is_50hz = itv->is_50hz = !itv->is_60hz;
		itv->params.width = 720;
		itv->params.height = itv->is_50hz ? 576 : 480;
		itv->vbi.count = itv->is_50hz ? 18 : 12;
		itv->vbi.start[0] = itv->is_50hz ? 6 : 10;
		itv->vbi.start[1] = itv->is_50hz ? 318 : 273;
		if (itv->hw_flags & IVTV_HW_CX25840) {
			itv->vbi.sliced_decoder_line_size = itv->is_60hz ? 272 : 284;
		}
		IVTV_DEBUG_INFO("Switching standard to %llx.\n", itv->std);

		/* Tuner */
		ivtv_call_i2c_clients(itv, VIDIOC_S_STD, &itv->std);

		if (itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT) {
			/* set display standard */
			ivtv_vapi(itv, CX2341X_DEC_SET_STANDARD, 1, itv->is_50hz);
		}
		break;
	}

	case VIDIOC_S_TUNER: {	/* Setting tuner can only set audio mode */
		struct v4l2_tuner *vt = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_S_TUNER\n");

		if (vt->index != 0)
			return -EINVAL;

		ivtv_call_i2c_clients(itv, VIDIOC_S_TUNER, vt);
		break;
	}

	case VIDIOC_G_TUNER: {
		struct v4l2_tuner *vt = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_G_TUNER\n");

		if (vt->index != 0)
			return -EINVAL;

		memset(vt, 0, sizeof(*vt));
		ivtv_call_i2c_clients(itv, VIDIOC_G_TUNER, vt);

		if (test_bit(IVTV_F_I_RADIO_USER, &itv->i_flags)) {
			strcpy(vt->name, "ivtv Radio Tuner");
			vt->type = V4L2_TUNER_RADIO;
		} else {
			strcpy(vt->name, "ivtv TV Tuner");
			vt->type = V4L2_TUNER_ANALOG_TV;
		}
		break;
	}

	case VIDIOC_STREAMOFF:{
		IVTV_DEBUG_IOCTL("VIDIOC_STREAMOFF\n");
		if (id->open_id != stream->id)
			return -EBUSY;

		ivtv_stream_off(itv, stream);
		break;
	}

	case VIDIOC_STREAMON:{
		IVTV_DEBUG_IOCTL("VIDIOC_STREAMON\n");
		if (id->open_id != stream->id)
			return -EBUSY;

		return ivtv_stream_on(itv, stream);
	}

        case VIDIOC_G_SLICED_VBI_CAP: {
		struct v4l2_sliced_vbi_cap *cap = arg;
                int set = itv->is_50hz ? V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525;
                int f, l;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
		enum v4l2_buf_type type = cap->type;
#else
		enum v4l2_buf_type type = VIDIOC_G_SLICED_VBI_CAP;
#endif

		IVTV_DEBUG_IOCTL("VIDIOC_G_SLICED_VBI_CAP\n");
                memset(cap, 0, sizeof(*cap));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
		cap->type = type;
#endif
		if (type == V4L2_BUF_TYPE_SLICED_VBI_CAPTURE) {
                        for (f = 0; f < 2; f++) {
                                for (l = 0; l < 24; l++) {
                                        if (valid_service_line(f, l, itv->is_50hz)) {
                                                cap->service_lines[f][l] = set;
                                        }
                                }
                        }
			return 0;
		}
	       	if (type == V4L2_BUF_TYPE_SLICED_VBI_OUTPUT) {
			if (!(itv->v4l2_cap & V4L2_CAP_SLICED_VBI_OUTPUT))
				return -EINVAL;
                        if (itv->is_60hz) {
                                cap->service_lines[0][21] = V4L2_SLICED_CAPTION_525;
                                cap->service_lines[1][21] = V4L2_SLICED_CAPTION_525;
                        } else {
                                cap->service_lines[0][23] = V4L2_SLICED_WSS_625;
                                cap->service_lines[0][16] = V4L2_SLICED_VPS;
                        }
			return 0;
		}
		return -EINVAL;
        }

        case VIDIOC_G_FBUF:
        {
		struct v4l2_framebuffer *fb = arg;

		IVTV_DEBUG_IOCTL("VIDIOC_G_FBUF\n");
		memset(fb, 0, sizeof(*fb));
                break;
        }

        case VIDIOC_LOG_STATUS:
	{
		int has_output = itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT;
		struct v4l2_input vidin;
		struct v4l2_audio audin;
		int i;

                IVTV_INFO("=================  START STATUS CARD #%d  =================\n", itv->num);
                if (itv->hw_flags & IVTV_HW_TVEEPROM) {
			struct tveeprom tv;

			ivtv_read_eeprom(itv, &tv);
                }
		ivtv_call_i2c_clients(itv, VIDIOC_LOG_STATUS, NULL);
		ivtv_get_input(itv, itv->active_input, &vidin);
		ivtv_get_audio_input(itv, itv->audio_input, &audin);
		IVTV_INFO("Video Input: %s\n", vidin.name);
		IVTV_INFO("Audio Input: %s\n", audin.name);
		if (has_output) {
			struct v4l2_output vidout;
			struct v4l2_audioout audout;
                        int mode = itv->output_mode;
                        static const char * const output_modes[] = {
                                "None",
                                "MPEG Streaming",
                                "YUV Streaming",
                                "YUV Frames",
                                "Passthrough",
                        };

			ivtv_get_output(itv, itv->active_output, &vidout);
			ivtv_get_audio_output(itv, 0, &audout);
			IVTV_INFO("Video Output: %s\n", vidout.name);
			IVTV_INFO("Audio Output: %s\n", audout.name);
                        if (mode < 0 || mode > OUT_PASSTHROUGH)
                                mode = OUT_NONE;
			IVTV_INFO("Output Mode: %s\n", output_modes[mode]);
		}
		IVTV_INFO("Tuner: %s\n",
			test_bit(IVTV_F_I_RADIO_USER, &itv->i_flags) ? "Radio" : "TV");
		cx2341x_log_status(&itv->params, itv->name);
		IVTV_INFO("Status flags: 0x%08lx\n", itv->i_flags);
		for (i = 0; i < IVTV_MAX_STREAMS; i++) {
			struct ivtv_stream *s = &itv->streams[i];

			if (s->v4l2dev == NULL || s->buffers == 0)
				continue;
			IVTV_INFO("Stream %s: status 0x%04lx, %d%% of %d KiB (%d buffers) in use\n", s->name, s->s_flags,
					(s->buffers - s->q_free.buffers) * 100 / s->buffers,
					(s->buffers * s->buf_size) / 1024, s->buffers);
		}
		IVTV_INFO("Read MPEG/VBI: %lld/%lld bytes\n", itv->mpg_data_received, itv->vbi_data_inserted);
                IVTV_INFO("==================  END STATUS CARD #%d  ==================\n", itv->num);
                break;
	}

	default:
		IVTV_DEBUG_WARN("unknown VIDIOC command %08x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int ivtv_ivtv_ioctls(struct ivtv *itv, struct ivtv_open_id *id,
		     int streamtype, unsigned int cmd, void *arg)
{
	struct ivtv_stream *stream = &itv->streams[streamtype];

	switch (cmd) {
	case IVTV_IOC_PASSTHROUGH:{
		int enable = *(int *)arg;

		IVTV_DEBUG_IOCTL("IVTV_IOC_PASSTHROUGH\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		return ivtv_passthrough_mode(itv, enable);
	}

	case IVTV_IOC_GET_FB:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_GET_FB\n");
		if (itv->fb_id < 0)
			return -EINVAL;
		*(int *)arg = itv->fb_id;

		break;
	}

	case IVTV_IOC_FRAMESYNC:{
		struct ivtv_ioctl_framesync *fs = arg;
		u32 data[CX2341X_MBOX_MAX_DATA];
		DEFINE_WAIT(wait);

		IVTV_DEBUG_IOCTL("IVTV_IOC_FRAMESYNC\n");

		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		if (ivtv_waitq(&itv->vsync_waitq))
			return -EINTR;

		if (atomic_read(&itv->decoding)) {
			ivtv_api_get_data(&itv->dec_mbox, IVTV_MBOX_FIELD_DISPLAYED, data);
			fs->frame = data[0];
			fs->pts = ((u64)data[2] << 32) | (u64)data[1];
			fs->scr = ((u64)data[4] << 32) | (u64)data[3];
		}
		else {
			/* scr should be valid anytime */
			if (ivtv_api(itv, CX2341X_DEC_GET_TIMING_INFO, 5, data)) {
				IVTV_DEBUG_WARN("FRAMESYNC: couldn't read clock\n");
				return -EIO;
			}
			fs->frame = 0;
			fs->pts = 0;
			fs->scr = (u64)((u64)data[4] << 32) | (u64)data[3];
		}
		break;
	}

	case IVTV_IOC_PLAY:{
		struct ivtv_stream *s = &itv->streams[IVTV_DEC_STREAM_TYPE_MPG];
		IVTV_DEBUG_IOCTL("IVTV_IOC_PLAY (Obsolete: use IVTV_IOC_START_DECODE)\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
                if (ivtv_set_output_mode(itv, OUT_MPG) != OUT_MPG)
                        return -EBUSY;
		return ivtv_start_v4l2_decode_stream(s, 0);
	}

	case IVTV_IOC_STOP:{
		struct ivtv_stream *s = &itv->streams[IVTV_DEC_STREAM_TYPE_MPG];
		IVTV_DEBUG_IOCTL("IVTV_IOC_STOP (Obsolete: use IVTV_IOC_STOP_DECODE\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
                if (itv->output_mode == OUT_MPG)
                    itv->output_mode = OUT_NONE;
		return ivtv_stop_v4l2_decode_stream(s, IVTV_STOP_FL_HIDE_FRAME, 0);
	}

	case IVTV_IOC_START_DECODE: {
		struct ivtv_cfg_start_decode *sd = arg;
		struct ivtv_stream *s = &itv->streams[IVTV_DEC_STREAM_TYPE_MPG];

                IVTV_DEBUG_IOCTL("IVTV_IOC_START_DECODE\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		if ((sd->gop_offset < 0) || (sd->gop_offset > 15))
			return -EINVAL;
		if (sd->muted_audio_frames < 0)
			return -EINVAL;
                if (ivtv_set_output_mode(itv, OUT_MPG) != OUT_MPG)
                        return -EBUSY;

		if (ivtv_claim_stream(id, IVTV_DEC_STREAM_TYPE_MPG)) {
			/* someone else is using this stream already */
			IVTV_DEBUG_WARN("start decode, stream already claimed\n");
                        itv->output_mode = OUT_NONE;
			return -EBUSY;
		}

		/* stop decoding if already happening */
		if (test_bit(IVTV_F_S_STREAMING, &s->s_flags)) {
			IVTV_DEBUG_WARN("start decode, stream already started! Stopping\n");
			ivtv_stop_v4l2_decode_stream(s, 0, 0);
		}
		return ivtv_start_v4l2_decode_stream(s, sd->gop_offset);
	}

	case IVTV_IOC_STOP_DECODE: {
		struct ivtv_cfg_stop_decode *sd = arg;
		struct ivtv_stream *s = &itv->streams[IVTV_DEC_STREAM_TYPE_MPG];

                IVTV_DEBUG_IOCTL("IVTV_IOC_STOP_DECODE\n");
		if (stream->v4l2dev == NULL)
			return -EINVAL;
		if (sd->flags < 0 || sd->flags > 3)
			return -EINVAL;
                if (itv->output_mode == OUT_MPG)
                    itv->output_mode = OUT_NONE;

		return ivtv_stop_v4l2_decode_stream(s, sd->flags, sd->pts);
	}

	case IVTV_IOC_DEC_FLUSH: {
		IVTV_DEBUG_WARN("IVTV_IOC_DEC_FLUSH is obsolete!\n");
		break;
	}

	case IVTV_IOC_S_AUDMODE: {
		struct ivtv_audmode *mode = arg;

		IVTV_DEBUG_IOCTL("IVTV_IOC_S_AUDMODE\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		ivtv_vapi(itv, CX2341X_DEC_SET_AUDIO_MODE, 2, mode->dual, mode->stereo);
		break;
	}

	case IVTV_IOC_DEC_STEP:{
		int howfar = *(int *)arg;

		IVTV_DEBUG_IOCTL("IVTV_IOC_DEC_STEP\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		if (howfar < 0 || howfar > 2)
			return -EINVAL;

		/* howfar: 0 = 1 frame, 1 = top field, 2 = bottom field */
		ivtv_vapi(itv, CX2341X_DEC_STEP_VIDEO, 1, howfar);
		break;
	}

	case IVTV_IOC_DEC_FAST_STOP:{
		int fast_stop = *(int *)arg;
		IVTV_DEBUG_IOCTL("IVTV_IOC_DEC_FAST_STOP (Obsolete!)\n");
		itv->dec_options.fast_stop = fast_stop;
		break;
	}

	case IVTV_IOC_G_SPEED:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_G_SPEED\n");
		*(struct ivtv_speed *)arg = itv->dec_options.speed;
		break;
	}

	case IVTV_IOC_S_SPEED:{
		struct ivtv_speed *speed = arg;

		IVTV_DEBUG_IOCTL("IVTV_IOC_S_SPEED\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		return ivtv_s_speed(itv, speed);
	}

	case IVTV_IOC_S_SLOW_FAST:{
		struct ivtv_slow_fast *sf = arg;
		struct ivtv_speed speed;

		IVTV_DEBUG_IOCTL("IVTV_IOC_S_SLOW_FAST\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		if ((sf->scale < 0) || (sf->scale > 50))
			return -EINVAL;
		if ((sf->speed < 0) || (sf->speed > 1))
			return -EINVAL;

		speed = itv->dec_options.speed;
		speed.scale = sf->scale;
		speed.speed = sf->speed;

		return ivtv_s_speed(itv, &speed);
	}

	case IVTV_IOC_PAUSE:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_PAUSE\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		if (atomic_read(&itv->decoding) > 0) {
			ivtv_vapi(itv, CX2341X_DEC_PAUSE_PLAYBACK, 1, 0);
		}
		break;
	}

	case IVTV_IOC_PAUSE_BLACK:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_PAUSE\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;

		if (atomic_read(&itv->decoding) > 0) { 
                        ivtv_vapi(itv, CX2341X_DEC_PAUSE_PLAYBACK, 1, 1);
                }
		break;
	}

	case IVTV_IOC_GET_TIMING:{
		struct ivtv_ioctl_framesync *timing = arg;
		u32 data[CX2341X_MBOX_MAX_DATA];

		IVTV_DEBUG_IOCTL("IVTV_IOC_GET_TIMING\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
		if (atomic_read(&itv->decoding) == 0) {
			memset(timing, 0, sizeof(timing));
			/* scr should be valid anytime */
			if (ivtv_api(itv, CX2341X_DEC_GET_TIMING_INFO, 5, data)) {
				IVTV_DEBUG_WARN("GET_TIMING: couldn't read clock\n");
				return -EIO;
			}

			timing->frame = 0;
			timing->pts = 0;
			timing->scr =
			    (u64) (((u64) data[4] << 32) | (u64) (data[3]));

		} else {
			if (ivtv_api(itv, CX2341X_DEC_GET_TIMING_INFO, 5, data)) {
				IVTV_DEBUG_WARN("GET_TIMING: couldn't read clock\n");
				return -EIO;
			}
			timing->frame = data[0];
			timing->pts =
			    (u64) ((u64) data[2] << 32) | (u64) data[1];
			timing->scr =
			    (u64) (((u64) data[4] << 32) | (u64) (data[3]));
		}
		break;
	}

	case IVTV_IOC_S_VBI_PASSTHROUGH:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_S_VBI_PASSTHROUGH (Obsolete)\n");
		if (!(itv->v4l2_cap & V4L2_CAP_SLICED_VBI_OUTPUT))
			return -EINVAL;
		break;
	}

	case IVTV_IOC_G_VBI_PASSTHROUGH:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_G_VBI_PASSTHROUGH (Obsolete)\n");
		*(int *)arg = itv->is_50hz ? V4L2_SLICED_WSS_625 | V4L2_SLICED_VPS :
		       	V4L2_SLICED_CAPTION_525;
		break;
	}

	case IVTV_IOC_S_GOP_END:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_S_GOP_END\n");
		itv->end_gop = *(int *)arg;
		*(int *)arg = itv->end_gop;
		break;
	}

	case IVTV_IOC_PAUSE_ENCODE:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_PAUSE_ENCODE\n");
		if (!atomic_read(&itv->capturing))
			return 0;
		ivtv_mute(itv);
		ivtv_vapi(itv, CX2341X_ENC_PAUSE_ENCODER, 1, 0);
		break;
	}

	case IVTV_IOC_RESUME_ENCODE:{
		IVTV_DEBUG_IOCTL("IVTV_IOC_RESUME_ENCODE\n");
		if (!atomic_read(&itv->capturing))
			return 0;
		ivtv_vapi(itv, CX2341X_ENC_PAUSE_ENCODER, 1, 1);
		ivtv_unmute(itv);
		break;
	}

	case IVTV_IOC_PREP_FRAME_YUV: {
                struct ivtvyuv_ioctl_dma_host_to_ivtv_args *args = arg;
                int ret;

                IVTV_DEBUG_IOCTL("IVTV_IOC_PREP_FRAME_YUV\n");
		if (!(itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT))
			return -EINVAL;
                if (ivtv_set_output_mode(itv, OUT_UDMA_YUV) != OUT_UDMA_YUV)
                        return -EBUSY;
		if (ivtv_claim_stream(id, id->type)) {
			itv->output_mode = OUT_NONE;
			return -EBUSY;
		}
		ret = ivtv_yuv_prep_frame(itv, args);
                itv->output_mode = OUT_NONE;
                return ret;
        }

	case IVTV_IOC_G_YUV_INTERLACE:{
		struct ivtv_ioctl_yuv_interlace *yuv_mode = arg;

		IVTV_DEBUG_IOCTL("IVTV_IOC_G_YUV_INTERLACE\n");
                yuv_mode->interlace_mode = itv->yuv_info.lace_mode;
		yuv_mode->interlace_mode |= itv->yuv_info.lace_sync_field ? IVTV_YUV_SYNC_ODD : IVTV_YUV_SYNC_EVEN;
                yuv_mode->threshold = itv->yuv_info.lace_threshold;
		break;
	}

	case IVTV_IOC_S_YUV_INTERLACE:{
		struct ivtv_ioctl_yuv_interlace *yuv_mode = arg;

		IVTV_DEBUG_IOCTL("IVTV_IOC_S_YUV_INTERLACE\n");
		if (yuv_mode->interlace_mode & ~(IVTV_YUV_SYNC_MASK | IVTV_YUV_MODE_MASK))
                {
                    IVTV_DEBUG_WARN("ivtv ioctl set YUV_INTERLACE invalid mode %d\n",yuv_mode->interlace_mode);
                    return -EINVAL;
                }
		itv->yuv_info.lace_mode = yuv_mode->interlace_mode & IVTV_YUV_MODE_MASK;
		itv->yuv_info.lace_sync_field = (yuv_mode->interlace_mode & IVTV_YUV_SYNC_MASK) == IVTV_YUV_SYNC_EVEN ? 0 : 1;
                itv->yuv_info.lace_threshold = yuv_mode->threshold;

		// Force update of yuv registers
		itv->yuv_info.yuv_forced_update = 1;
		break;
	}

	case IVTV_IOC_G_PTS: {
		u64 *pts = arg;

		IVTV_DEBUG_IOCTL("IVTV_IOC_G_PTS\n");
		*pts = stream->dma_pts;
		break;
	}

	case IVTV_IOC_G_INDEX: {
		struct ivtv_index *idx = arg;
		int i;

		IVTV_DEBUG_IOCTL("IVTV_IOC_G_INDEX\n");
		idx->entries = (itv->pgm_info_write_idx + IVTV_MAX_PGM_INDEX - itv->pgm_info_read_idx) %
		       			IVTV_MAX_PGM_INDEX;
		if (idx->entries > IVTV_INDEX_MAX_ENTRIES)
			idx->entries = IVTV_INDEX_MAX_ENTRIES;
		for (i = 0; i < idx->entries; i++) {
			idx->entry[i] = itv->pgm_info[(itv->pgm_info_read_idx + i) % IVTV_MAX_PGM_INDEX];
		}
		itv->pgm_info_read_idx = (itv->pgm_info_read_idx + idx->entries) % IVTV_MAX_PGM_INDEX;
		break;
	}

	default:
		IVTV_DEBUG_WARN("unknown IVTV command %08x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int ivtv_v4l2_do_ioctl(struct inode *inode, struct file *filp,
			      unsigned int cmd, void *arg)
{
	struct ivtv_open_id *id = (struct ivtv_open_id *)filp->private_data;
	struct ivtv *itv = id->itv;
	int streamtype = id->type;

	IVTV_DEBUG_IOCTL("v4l2 ioctl 0x%08x\n", cmd);

	switch (cmd) {
	case VIDIOC_INT_G_REGISTER:
	case VIDIOC_INT_S_REGISTER:
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
	case IVTV_INT_S_REGISTER:
#endif
	case VIDIOC_INT_S_AUDIO_ROUTING:
	case VIDIOC_INT_RESET:
		return ivtv_internal_ioctls(itv, streamtype, cmd, arg);

	case VIDIOC_QUERYCAP:
	case VIDIOC_ENUMINPUT:
	case VIDIOC_G_INPUT:
	case VIDIOC_S_INPUT:
	case VIDIOC_ENUMOUTPUT:
	case VIDIOC_G_OUTPUT:
	case VIDIOC_S_OUTPUT:
	case VIDIOC_G_FMT:
	case VIDIOC_S_FMT:
	case VIDIOC_TRY_FMT:
	case VIDIOC_ENUM_FMT:
        case VIDIOC_G_CROP:
        case VIDIOC_S_CROP:
	case VIDIOC_G_FREQUENCY:
	case VIDIOC_S_FREQUENCY:
	case VIDIOC_ENUMSTD:
	case VIDIOC_G_STD:
	case VIDIOC_S_STD:
	case VIDIOC_S_TUNER:
	case VIDIOC_G_TUNER:
	case VIDIOC_ENUMAUDIO:
	case VIDIOC_S_AUDIO:
	case VIDIOC_G_AUDIO:
	case VIDIOC_ENUMAUDOUT:
	case VIDIOC_S_AUDOUT:
	case VIDIOC_G_AUDOUT:
        case VIDIOC_G_SLICED_VBI_CAP:
        case VIDIOC_G_FBUF:
        case VIDIOC_LOG_STATUS:
        case VIDIOC_STREAMOFF:
        case VIDIOC_STREAMON:
		return ivtv_v4l2_ioctls(itv, id, streamtype, cmd, arg);

	case VIDIOC_QUERYMENU:
	case VIDIOC_QUERYCTRL:
	case VIDIOC_S_CTRL:
	case VIDIOC_G_CTRL:
	case VIDIOC_S_EXT_CTRLS:
	case VIDIOC_G_EXT_CTRLS:
	case VIDIOC_TRY_EXT_CTRLS:
		return ivtv_control_ioctls(itv, cmd, arg);

	case IVTV_IOC_PASSTHROUGH:
	case IVTV_IOC_GET_FB:
	case IVTV_IOC_FRAMESYNC:
	case IVTV_IOC_PLAY:
	case IVTV_IOC_STOP:
	case IVTV_IOC_START_DECODE:
	case IVTV_IOC_STOP_DECODE:
	case IVTV_IOC_DEC_FLUSH:
	case IVTV_IOC_S_AUDMODE:
	case IVTV_IOC_DEC_STEP:
	case IVTV_IOC_DEC_FAST_STOP:
	case IVTV_IOC_G_SPEED:
	case IVTV_IOC_S_SPEED:
	case IVTV_IOC_S_SLOW_FAST:
	case IVTV_IOC_PAUSE:
	case IVTV_IOC_PAUSE_BLACK:
	case IVTV_IOC_GET_TIMING:
	case IVTV_IOC_S_VBI_PASSTHROUGH:
	case IVTV_IOC_G_VBI_PASSTHROUGH:
	case IVTV_IOC_S_GOP_END:
	case IVTV_IOC_PAUSE_ENCODE:
	case IVTV_IOC_RESUME_ENCODE:
        case IVTV_IOC_PREP_FRAME_YUV:
        case IVTV_IOC_G_YUV_INTERLACE:
        case IVTV_IOC_S_YUV_INTERLACE:
        case IVTV_IOC_G_PTS:
	case IVTV_IOC_G_INDEX:
                return ivtv_ivtv_ioctls(itv, id, streamtype, cmd, arg);

	case 0x00005401:	/* Handle isatty() calls */
		return -EINVAL;
	default:
		return v4l_compat_translate_ioctl(inode, filp, cmd, arg,
						   ivtv_v4l2_do_ioctl);
	}
	return 0;
}

int ivtv_v4l2_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
		    unsigned long arg)
{
	switch (cmd) {
	/* Converts old (0.1.9) non-conforming ioctls that were using
	   'just some values I picked for now'. I hoped this would not be
	   necessary, but too many people were already using existing apps
	   (MythTV) written for this version of the driver. */
	case 0xFFEE7781:
		cmd = IVTV_IOC_PLAY;
		break;
	case 0xFFEE7782:
		cmd = IVTV_IOC_PAUSE;
		break;
	case 0xFFEE7783:
		cmd = IVTV_IOC_FRAMESYNC;
		break;
	case 0xFFEE7784:
		cmd = IVTV_IOC_GET_TIMING;
		break;
	case 0xFFEE7785:
		cmd = IVTV_IOC_S_SLOW_FAST;
		break;
	case 0xFFEE7789:
		cmd = IVTV_IOC_GET_FB;
		break;
	case 0xFFEE7701:
	case 0xFFEE7702:
	case 0xFFEE7703:
	case 0xFFEE7704:
	case 0xFFEE7705:
	case 0xFFEE7706:
	case 0xFFEE7786:
	case 0xFFEE7787:
	case 0xFFEE7788:
		return -EINVAL;
	}
	return video_usercopy(inode, filp, cmd, arg, ivtv_v4l2_do_ioctl);
}
