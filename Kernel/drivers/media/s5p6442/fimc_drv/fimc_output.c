/* linux/drivers/media/video/samsung/fimc_output.c
 *
 * V4L2 Output device support file for Samsung Camera Interface (FIMC) driver
 *
 * Jonghun Han, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>
#include <media/videobuf-core.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <plat/media.h>
#include <plat/power_clk_gating.h>
#include "fimc.h"
#ifdef S5P6442_POWER_GATING_CAM
extern void s5p_fimc_out_domain_set(int dev_id, int flag);
#endif
//#define S5P6442_EVT0
static __u32 fimc_get_pixel_format_type(__u32 pixelformat)
{
	switch(pixelformat)
	{
		case V4L2_PIX_FMT_RGB32:
		case V4L2_PIX_FMT_RGB565:
			return FIMC_RGB;

		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV12T:
		case V4L2_PIX_FMT_NV21:		
		case V4L2_PIX_FMT_YUV420:
			return FIMC_YUV420;

		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YVYU:
		case V4L2_PIX_FMT_VYUY:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
		case V4L2_PIX_FMT_YUV422P:
			return FIMC_YUV422;

		default:
			return FIMC_YUV420;
	}
}
void fimc_outdev_set_src_addr(struct fimc_control *ctrl, dma_addr_t *base)
{
	fimc_hwset_addr_change_disable(ctrl);
	fimc_hwset_input_address(ctrl, base);
	fimc_hwset_addr_change_enable(ctrl);
}

int fimc_outdev_start_camif(void *param)
{
	struct fimc_control *ctrl = (struct fimc_control *)param;

	fimc_hwset_start_scaler(ctrl);
	fimc_hwset_enable_capture(ctrl);
	fimc_hwset_start_input_dma(ctrl);

	return 0;
}

static int fimc_outdev_stop_camif(void *param)
{
	struct fimc_control *ctrl = (struct fimc_control *)param;

	fimc_hwset_stop_input_dma(ctrl);
	fimc_hwset_stop_scaler(ctrl);
	fimc_hwset_disable_capture(ctrl);

	return 0;
}

static int fimc_stop_fifo(struct fimc_control *ctrl, u32 sleep)
{
	int ret = -1;

	//dev_dbg(ctrl->dev, "%s: called\n", __func__);

	ret = ctrl->fb.close_fifo(ctrl->id, fimc_outdev_stop_camif,
			(void *)ctrl, sleep);
	if (ret < 0)
		dev_err(ctrl->dev, "FIMD FIFO close fail\n");

	return 0;
}

int fimc_outdev_stop_streaming(struct fimc_control *ctrl)
{
	int ret = 0;

//	dev_dbg(ctrl->dev, "%s: called\n", __func__);

	if (ctrl->out->fbuf.base) {	/* DMA OUT */
		ret = wait_event_interruptible_timeout(ctrl->wq, \
				(ctrl->status == FIMC_STREAMON_IDLE), \
				FIMC_ONESHOT_TIMEOUT);
		if (ret == 0)
			dev_err(ctrl->dev, "Fail : %s\n", __func__);
		else if (ret == -ERESTARTSYS)
			fimc_print_signal(ctrl);

		fimc_outdev_stop_camif(ctrl);
	} else {				/* FIMD FIFO */
		ctrl->status = FIMC_READY_OFF;
		fimc_stop_fifo(ctrl, FIFO_CLOSE);
	}

	return ret;
}

static 
int fimc_init_out_buf(struct fimc_control *ctrl, enum v4l2_memory mem_type)
{
	int width = ctrl->out->pix.width; 
	int height = ctrl->out->pix.height;
	u32 format = ctrl->out->pix.pixelformat;
	u32 y_size, cbcr_size = 0, rgb_size, total_size = 0, i, offset;
	u32 base = ctrl->mem.base;

	/* VIDIOC_S_FMT should be called before VIDIOC_REQBUFS*/
	/* Y,C components should be located sequentially. */

	/* Validation Check */
	switch (format) {
	case V4L2_PIX_FMT_RGB32:
		rgb_size = PAGE_ALIGN(width * height * 4);
		total_size = rgb_size * FIMC_OUTBUFS;
		break;
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
		rgb_size = PAGE_ALIGN(width * height * 2);
		total_size = rgb_size * FIMC_OUTBUFS;
		break;
	case V4L2_PIX_FMT_NV12:	
	case V4L2_PIX_FMT_NV21:	
		y_size = (width * height);
		cbcr_size = (y_size>>1); /* the size of cbcr plane is half the size of y plane */
		total_size = PAGE_ALIGN(y_size + cbcr_size) * FIMC_OUTBUFS;
		break;
	case V4L2_PIX_FMT_NV12T:
		y_size = ALIGN(ALIGN(width,128) * ALIGN(height, 32), SZ_8K);
		cbcr_size = ALIGN(ALIGN(width,128) * ALIGN(height/2, 32), SZ_8K);
		total_size = PAGE_ALIGN(y_size + cbcr_size) * FIMC_OUTBUFS;
		break;
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_NV61:
		y_size = (width * height);
		cbcr_size = y_size;
		total_size = PAGE_ALIGN(y_size + cbcr_size) * FIMC_OUTBUFS;
		break;
	case V4L2_PIX_FMT_YUV420:
		y_size = (width * height);
		cbcr_size = (y_size>>2);
		total_size = PAGE_ALIGN(y_size + (cbcr_size<<1)) * FIMC_OUTBUFS;
		break;
	default: 
		dev_err(ctrl->dev, "%s: Invalid pixelformt : %d\n", 
				__func__, format);
		return -EINVAL;
	}

	if ((mem_type == V4L2_MEMORY_MMAP) && (total_size > ctrl->mem.size)) {
		dev_err(ctrl->dev, "Reserved memory is not sufficient\n");
		return -EINVAL;
	}

	/* Initialize input buffer addrs. of OUTPUT device */
	switch (format) {
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
	case V4L2_PIX_FMT_RGB32:
		/* 1-plane */
		for (i = 0; i < FIMC_OUTBUFS; i++) {
			offset = (total_size * i);
			ctrl->out->buf[i].base[FIMC_ADDR_Y] = base + offset;
			ctrl->out->buf[i].length[FIMC_ADDR_Y] = total_size;
			ctrl->out->buf[i].base[FIMC_ADDR_CB] = 0;
			ctrl->out->buf[i].length[FIMC_ADDR_CB] = 0;
			ctrl->out->buf[i].base[FIMC_ADDR_CR] = 0;
			ctrl->out->buf[i].length[FIMC_ADDR_CR] = 0;
		}

		break;
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
        case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_NV61:
		/* 2-plane */
		for (i = 0; i < FIMC_OUTBUFS; i++) {
			offset = (total_size * i);
			ctrl->out->buf[i].base[FIMC_ADDR_Y] = base + offset;
			ctrl->out->buf[i].length[FIMC_ADDR_Y] = y_size;
			ctrl->out->buf[i].base[FIMC_ADDR_CB] = base + offset + y_size;
			ctrl->out->buf[i].length[FIMC_ADDR_CB] = cbcr_size;
			ctrl->out->buf[i].base[FIMC_ADDR_CR] = 0;
			ctrl->out->buf[i].length[FIMC_ADDR_CR] = 0;
		}
		break;

	case V4L2_PIX_FMT_YUV420:
		/* 3-plane */
                for (i = 0; i < FIMC_OUTBUFS; i++) {
                        offset = (total_size * i);
                        ctrl->out->buf[i].base[FIMC_ADDR_Y] = base + offset;
                        ctrl->out->buf[i].length[FIMC_ADDR_Y] = y_size;
                        ctrl->out->buf[i].base[FIMC_ADDR_CB] = base + offset + y_size;
                        ctrl->out->buf[i].length[FIMC_ADDR_CB] = cbcr_size;
                        ctrl->out->buf[i].base[FIMC_ADDR_CR] = base + offset + y_size + cbcr_size;
                        ctrl->out->buf[i].length[FIMC_ADDR_CR] = cbcr_size;
                }
                break;

	default: 
		dev_err(ctrl->dev, "%s: Invalid pixelformt : %d\n", 
				__func__, format);
		return -EINVAL;
	}

	for (i = 0; i < FIMC_OUTBUFS; i++) {
		ctrl->out->buf[i].state = VIDEOBUF_IDLE;
		ctrl->out->buf[i].flags = 0x0;

		ctrl->out->in_queue[i] = -1;
		ctrl->out->out_queue[i] = -1;
	}

	ctrl->out->is_requested	= 0;

	return 0;
}

static int fimc_set_rot_degree(struct fimc_control *ctrl, int degree)
{
	switch (degree) {
	case 0:		/* fall through */
	case 90:	/* fall through */
	case 180:	/* fall through */
	case 270:
		ctrl->out->rotate = degree;
		break;

	default:
		dev_err(ctrl->dev, "Invalid rotate value : %d\n", degree);
		return -EINVAL;
	}

	return 0;
}

static int fimc_outdev_check_param(struct fimc_control *ctrl)
{
	struct v4l2_rect dst, bound;
	u32 is_rotate = 0;
	int ret = 0;

	is_rotate = fimc_mapping_rot_flip(ctrl->out->rotate, ctrl->out->flip);
	dst.top = ctrl->out->win.w.top;
	dst.left = ctrl->out->win.w.left;
	dst.width = ctrl->out->win.w.width;
	dst.height = ctrl->out->win.w.height;

	if (ctrl->out->fbuf.base) {	/* Destructive OVERLAY device */
		bound.width = ctrl->out->fbuf.fmt.width;
		bound.height = ctrl->out->fbuf.fmt.height;
	} else {			/* Non-Destructive OVERLAY device */
		if (is_rotate & FIMC_ROT) {	/* Landscape mode */
			bound.width = ctrl->fb.lcd_vres;
			bound.height = ctrl->fb.lcd_hres;
		} else {			/* Portrait mode */
			bound.width = ctrl->fb.lcd_hres;
			bound.height = ctrl->fb.lcd_vres;
		}
	}

	if ((dst.left + dst.width) > bound.width) {
		dev_err(ctrl->dev, "Horizontal position setting is failed\n");
		dev_err(ctrl->dev,
				"\tleft = %d, width = %d, bound width = %d,\n",
				dst.left, dst.width, bound.width);
		ret = -EINVAL;
	} else if ((dst.top + dst.height) > bound.height) {
		dev_err(ctrl->dev, "Vertical position setting is failed\n");
		dev_err(ctrl->dev,
			"\ttop = %d, height = %d, bound height = %d, \n",
			dst.top, dst.height, bound.height);
		ret = -EINVAL;
	}

	return ret;
}

static void fimc_outdev_set_src_format(struct fimc_control *ctrl, u32 pixfmt)
{
	fimc_hwset_input_burst_cnt(ctrl, 4);
	fimc_hwset_input_yuv(ctrl, pixfmt);
	fimc_hwset_input_colorspace(ctrl, pixfmt);
	fimc_hwset_input_rgb(ctrl, pixfmt);
	fimc_hwset_ext_rgb(ctrl, 1);
	fimc_hwset_input_addr_style(ctrl, pixfmt);
}

static void fimc_outdev_set_dst_format(struct fimc_control *ctrl, u32 pixfmt)
{
	fimc_hwset_output_colorspace(ctrl, pixfmt);
	fimc_hwset_output_yuv(ctrl, pixfmt);
	fimc_hwset_output_rgb(ctrl, pixfmt);
	fimc_hwset_output_addr_style(ctrl, pixfmt);
}

static void fimc_outdev_set_format(struct fimc_control *ctrl)
{
	u32 pixfmt = 0;

	pixfmt = ctrl->out->pix.pixelformat;
	fimc_outdev_set_src_format(ctrl, pixfmt);

	if (ctrl->out->fbuf.base)
		pixfmt = ctrl->out->fbuf.fmt.pixelformat;
	else 	/* FIFO mode */
		pixfmt = V4L2_PIX_FMT_RGB32;
	fimc_outdev_set_dst_format(ctrl, pixfmt);

}

static void fimc_outdev_set_path(struct fimc_control *ctrl)
{
	/* source path */
	fimc_hwset_input_source(ctrl, FIMC_SRC_MSDMA);

	/* dst path */
	if (ctrl->out->fbuf.base) {
		fimc_hwset_disable_lcdfifo(ctrl);
		fimc_hwset_disable_autoload(ctrl);
	} else { /* FIFO mode */
		fimc_hwset_enable_lcdfifo(ctrl);
		fimc_hwset_enable_autoload(ctrl);
	}
}

static void fimc_outdev_set_rot(struct fimc_control *ctrl)
{
	u32 rot = ctrl->out->rotate;
	u32 flip = ctrl->out->flip;

	if (ctrl->out->fbuf.base) {
		fimc_hwset_input_rot(ctrl, 0, 0);
		fimc_hwset_input_flip(ctrl, 0, 0);
		fimc_hwset_output_rot_flip(ctrl, rot, flip);
	} else { /* FIFO mode */
		fimc_hwset_input_rot(ctrl, rot, flip);
		fimc_hwset_input_flip(ctrl, rot, flip);
		fimc_hwset_output_rot_flip(ctrl, 0, 0);
	}
}

static void fimc_outdev_set_src_dma_offset(struct fimc_control *ctrl)
{
	struct v4l2_rect bound, crop;
	u32 pixfmt = ctrl->out->pix.pixelformat;

	bound.width = ctrl->out->pix.width;
	bound.height = ctrl->out->pix.height;

	crop.left = ctrl->out->crop.left;
	crop.top = ctrl->out->crop.top;
	crop.width = ctrl->out->crop.width;
	crop.height = ctrl->out->crop.height;

	fimc_hwset_input_offset(ctrl, pixfmt, &bound, &crop);
}

static int fimc_outdev_check_src_size(struct fimc_control *ctrl, \
				struct v4l2_rect *real, struct v4l2_rect *org)
{
	u32 rot = ctrl->out->rotate;

	if ((!ctrl->out->fbuf.base) && ((rot == 90) || (rot == 270))) {
                /* Input Rotator */
		 if (real->height < 16) {
                        dev_err(ctrl->dev, "SRC Real_H: Min 16, received %d\n", real->height);
                        return -EINVAL;
                }
		
		if (real->width < 8) {
                        dev_err(ctrl->dev, "SRC Real_W: Min 8, received %d\n", real->width);
                        return -EINVAL;
                }

		 if (real->height > ctrl->limit->real_h_rot) {
                        dev_err(ctrl->dev, "SRC REAL_H: Real_H(%d) <= %d\n", \
                                                real->height, ctrl->limit->real_h_rot);
                        return -EINVAL;
                }


	} else {
		/* No Input Rotator */
		if (real->height < 8) {
			dev_err(ctrl->dev, "SRC Real_H: Min 8, received %d\n", real->height);
			return -EINVAL;
		}

		if (real->width < 16) {
			dev_err(ctrl->dev, "SRC Real_W: Min 16, received %d\n", real->width);
			return -EINVAL;
		}

		if (real->width > ctrl->limit->real_w_no_rot) {
			dev_err(ctrl->dev, "SRC REAL_W: Real_W(%d) <= %d\n", \
						real->width, ctrl->limit->real_w_no_rot);
			return -EINVAL;
		}
	}

	if (org->height < 8) {
		dev_err(ctrl->dev, "SRC Org_H: Min 8, received %d\n", org->height);
		return -EINVAL;
	}

	if (org->height < real->height) {
		dev_err(ctrl->dev, "SRC Org_H: larger than Real_H, received %d and %d\n", org->height, real->height);
		return -EINVAL;
	}

	if (org->width < real->width) {
		dev_err(ctrl->dev, "SRC Org_W: Org_W >= Real_W, received %d and %d\n", org->width, real->width);
		return -EINVAL;
	}

	return 0;
}

static int fimc_outdev_set_src_dma_size(struct fimc_control *ctrl)
{
	struct v4l2_rect real, org;
	int ret = 0;

	real.width = ctrl->out->crop.width;
	real.height = ctrl->out->crop.height;
	org.width = ctrl->out->pix.width;
	org.height = ctrl->out->pix.height;

	ret = fimc_outdev_check_src_size(ctrl, &real, &org);
	if (ret < 0)
		return ret;

	fimc_hwset_org_input_size(ctrl, org.width, org.height);
	fimc_hwset_real_input_size(ctrl, real.width, real.height);

	return 0;
}

static void fimc_outdev_set_dst_dma_offset(struct fimc_control *ctrl)
{
	struct v4l2_rect bound, win;
	u32 pixfmt = ctrl->out->fbuf.fmt.pixelformat;

	switch (ctrl->out->rotate) {
	case 0:
		bound.width = ctrl->out->fbuf.fmt.width;
		bound.height = ctrl->out->fbuf.fmt.height;

		win.left = ctrl->out->win.w.left;
		win.top = ctrl->out->win.w.top;
		win.width = ctrl->out->win.w.width;
		win.height = ctrl->out->win.w.height;

		break;

	case 90:
		bound.width = ctrl->out->fbuf.fmt.height;
		bound.height = ctrl->out->fbuf.fmt.width;

#if 0
		win.left = ctrl->out->fbuf.fmt.height -
				(ctrl->out->win.w.height +
				 ctrl->out->win.w.top);
#else
		win.left = ctrl->out->win.w.top;
#endif
		win.top = ctrl->out->win.w.left;
		win.width = ctrl->out->win.w.height;
		win.height = ctrl->out->win.w.width;

		break;

	case 180:
		bound.width = ctrl->out->fbuf.fmt.width;
		bound.height = ctrl->out->fbuf.fmt.height;

		win.left = ctrl->out->fbuf.fmt.width -
				(ctrl->out->win.w.left +
				 ctrl->out->win.w.width);
		win.top = ctrl->out->fbuf.fmt.height -
				(ctrl->out->win.w.top +
				 ctrl->out->win.w.height);
		win.width = ctrl->out->win.w.width;
		win.height = ctrl->out->win.w.height;

		break;

	case 270:
		bound.width = ctrl->out->fbuf.fmt.height;
		bound.height = ctrl->out->fbuf.fmt.width;

		win.left = ctrl->out->win.w.top;
		win.top = ctrl->out->fbuf.fmt.width -
				(ctrl->out->win.w.left +
				 ctrl->out->win.w.width);
		win.width = ctrl->out->win.w.height;
		win.height = ctrl->out->win.w.width;

		break;

	default:
		dev_err(ctrl->dev, "Rotation degree is inavlid\n");
		break;
	}


	dev_dbg(ctrl->dev, "bound.width(%d), bound.height(%d)\n",
				bound.width, bound.height);
	dev_dbg(ctrl->dev, "win.width(%d), win.height(%d)\n",
				win.width, win.height);
	dev_dbg(ctrl->dev, "win.top(%d), win.left(%d)\n", win.top, win.left);

	fimc_hwset_output_offset(ctrl, pixfmt, &bound, &win);
}

static int fimc_outdev_check_dst_size(struct fimc_control *ctrl, \
				struct v4l2_rect *real, struct v4l2_rect *org)
{
	u32 rot = ctrl->out->rotate;
    
	__u32 pixel_type;
	
	pixel_type = fimc_get_pixel_format_type(ctrl->out->fbuf.fmt.pixelformat); 

	
	// height constraint is not for RGB565
	if ((pixel_type == FIMC_YUV420) && (real->height & 1)) {
		dev_err(ctrl->dev, "DST Real_H: even number for YUV420 formats\n");
		return -EINVAL;
	}

    if (real->height < 4) {
		dev_err(ctrl->dev, "DST Real_H: Min 4 - received %d\n", real->height);
		return -EINVAL;
	}


	if (ctrl->out->fbuf.base && ((rot == 90) || (rot == 270))) {
		/* Output Rotator */
		if (org->height < real->width) {
			dev_err(ctrl->dev, "DST Org_H: Org_H >= Real_W\n");
			return -EINVAL;
		}

		if (org->width < real->height) {
			dev_err(ctrl->dev, "DST Org_W: Org_W >= Real_H\n");
			return -EINVAL;
		}

		if (real->height > ctrl->limit->trg_h_rot) {
			dev_err(ctrl->dev, "DST REAL_H: Real_H <= %d\n", \
						ctrl->limit->trg_h_rot);
			return -EINVAL;
		}
		
		if (((pixel_type == FIMC_YUV420)|| (pixel_type == FIMC_YUV422)) && (real->height & 1)) {
				dev_err(ctrl->dev, "DST Real_W: even for YUV420/YUV422 formats - received %d\n", real->height);
				return -EINVAL;
		} 

	} else if (ctrl->out->fbuf.base) {
		/* No Output Rotator */
		 if (org->height < real->height) {
                        dev_err(ctrl->dev, "DST Org_H: Org_H >= Real_H\n");
			return -EINVAL;
		}

		if (org->width < real->width) {
			dev_err(ctrl->dev, "DST Org_W: Org_W >= Real_W\n");
			return -EINVAL;
		}

		if (real->height > ctrl->limit->trg_h_no_rot) {
			dev_err(ctrl->dev, "DST REAL_H: Real_H <= %d\n", \
						ctrl->limit->trg_h_no_rot);
			return -EINVAL;
		}
		
		if (((pixel_type == FIMC_YUV420)|| (pixel_type == FIMC_YUV422)) && (real->width & 1)) {
				dev_err(ctrl->dev, "DST Real_W: even for YUV420/YUV422 formats - received %d\n", real->width);
				return -EINVAL;
		} 
	}

	return 0;
}

static int fimc_outdev_set_dst_dma_size(struct fimc_control *ctrl)
{
	struct v4l2_rect org, real;
	int ret = -1;

	memset(&org, 0, sizeof(org));
	memset(&real, 0, sizeof(real));

	if (ctrl->out->fbuf.base) {
		real.width = ctrl->out->win.w.width;
		real.height = ctrl->out->win.w.height;

		switch (ctrl->out->rotate) {
		case 0:
		case 180:
			org.width = ctrl->out->fbuf.fmt.width;
			org.height = ctrl->out->fbuf.fmt.height;

			break;

		case 90:
		case 270:
			org.width = ctrl->out->fbuf.fmt.height;
			org.height = ctrl->out->fbuf.fmt.width;

			break;

		default:
			dev_err(ctrl->dev, "Rotation degree is inavlid\n");
			break;
		}
	} else {
		switch (ctrl->out->rotate) {
		case 0:
		case 180:
			real.width = ctrl->out->win.w.width;
			real.height = ctrl->out->win.w.height;
			org.width = ctrl->fb.lcd_hres;
			org.height = ctrl->fb.lcd_vres;

			break;

		case 90:
		case 270:
			real.width = ctrl->out->win.w.height;
			real.height = ctrl->out->win.w.width;

			org.width = ctrl->fb.lcd_vres;
			org.height = ctrl->fb.lcd_hres;

			break;

		default:
			dev_err(ctrl->dev, "Rotation degree is inavlid\n");
			break;
		}

	}

	dev_dbg(ctrl->dev, "DST : org.width(%d), org.height(%d)\n", \
				org.width, org.height);
	dev_dbg(ctrl->dev, "DST : real.width(%d), real.height(%d)\n", \
				real.width, real.height);

	ret = fimc_outdev_check_dst_size(ctrl, &real, &org);
	if (ret < 0)
		return ret;

	fimc_hwset_output_size(ctrl, real.width, real.height);
	fimc_hwset_output_area(ctrl, real.width, real.height);
	fimc_hwset_org_output_size(ctrl, org.width, org.height);
	fimc_hwset_ext_output_size(ctrl, real.width, real.height);

	return 0;
}

static void fimc_outdev_calibrate_scale_info(struct fimc_control *ctrl, \
				struct v4l2_rect *src, struct v4l2_rect *dst)
{
	if (ctrl->out->fbuf.base) {	/* OUTPUT ROTATOR */
		src->width = ctrl->out->crop.width;
		src->height = ctrl->out->crop.height;
		dst->width = ctrl->out->win.w.width;
		dst->height = ctrl->out->win.w.height;
	} else {			/* INPUT ROTATOR */
		switch (ctrl->out->rotate) {
		case 0:		/* fall through */
		case 180:
			src->width = ctrl->out->crop.width;
			src->height = ctrl->out->crop.height;
			dst->width = ctrl->out->win.w.width;
			dst->height = ctrl->out->win.w.height;

			break;

		case 90:	/* fall through */
		case 270:
			src->width = ctrl->out->crop.height;
			src->height = ctrl->out->crop.width;
			dst->width = ctrl->out->win.w.height;
			dst->height = ctrl->out->win.w.width;

			break;

		default:
			dev_err(ctrl->dev, "Rotation degree is inavlid\n");
			break;
		}
	}

	dev_dbg(ctrl->dev, "src->width(%d), src->height(%d)\n", \
				src->width, src->height);
	dev_dbg(ctrl->dev, "dst->width(%d), dst->height(%d)\n", \
				dst->width, dst->height);
}

static int fimc_outdev_check_scaler(struct fimc_control *ctrl, \
				struct v4l2_rect *src, struct v4l2_rect *dst)
{
	u32 pixels = 0, dstfmt = 0;
	struct s3c_platform_fimc *pdata = to_fimc_plat(ctrl->dev);

	if (pdata->hw_ver == 0x50) return 0;

	/* Check scaler limitation */
	if (ctrl->sc.pre_dst_width > ctrl->limit->pre_dst_w) {
		dev_err(ctrl->dev, "FIMC%d : MAX PreDstWidth is %d\n",
					ctrl->id, ctrl->limit->pre_dst_w);
		return -EDOM;
	}

	/* SRC width double boundary check */
	switch (ctrl->out->pix.pixelformat) {
	case V4L2_PIX_FMT_RGB32:
		pixels = 1;		
		break;
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_NV61:		/* fall through */
		pixels = 2;
		break;		
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12T:
		pixels = 8;		
		break;
	default:
		dev_err(ctrl->dev, "Invalid color format\n");
		return -EINVAL;
	}

	if (src->width % pixels) {
		dev_err(ctrl->dev, "source width multiple of \
						%d pixels\n", pixels);
		return -EDOM;
	}

	/* DST width double boundary check */
	if (ctrl->out->fbuf.base)
		dstfmt = ctrl->out->fbuf.fmt.pixelformat;
	else
		dstfmt = V4L2_PIX_FMT_RGB32;

	switch (dstfmt) {
	case V4L2_PIX_FMT_RGB32:
		pixels = 1;
		break;
	case V4L2_PIX_FMT_RGB565:
		pixels = 2;
		break;
	default:
		dev_err(ctrl->dev, "Invalid color format\n");
		return -EINVAL;
	}

	if (dst->width % pixels) {
		dev_err(ctrl->dev, "source width multiple of \
						%d pixels\n", pixels);
		return -EDOM;
	}

	return 0;
}

static int fimc_outdev_set_scaler(struct fimc_control *ctrl)
{
	struct v4l2_rect src, dst;
	struct s3c_platform_fimc *pdata = to_fimc_plat(ctrl->dev);
	int ret = 0;
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));

	fimc_outdev_calibrate_scale_info(ctrl, &src, &dst);

	ret = fimc_get_scaler_factor(src.width, dst.width, \
			&ctrl->sc.pre_hratio, &ctrl->sc.hfactor);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail : Out of Width scale range\n");
		return ret;
	}

	ret = fimc_get_scaler_factor(src.height, dst.height, \
			&ctrl->sc.pre_vratio, &ctrl->sc.vfactor);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail : Out of Height scale range\n");
		return ret;
	}

	ctrl->sc.pre_dst_width = src.width / ctrl->sc.pre_hratio;
    if (pdata->hw_ver == 0x50)
       ctrl->sc.main_hratio = (src.width << 14) / (dst.width<<ctrl->sc.hfactor);
	else
		ctrl->sc.main_hratio = (src.width << 8) / (dst.width<<ctrl->sc.hfactor);	

	ctrl->sc.pre_dst_height = src.height / ctrl->sc.pre_vratio;
    if (pdata->hw_ver == 0x50)
       ctrl->sc.main_vratio = (src.height << 14) / (dst.height<<ctrl->sc.vfactor);
	else
		ctrl->sc.main_vratio = (src.height << 8) / (dst.height<<ctrl->sc.vfactor);


	dev_dbg(ctrl->dev, "pre_hratio(%d), hfactor(%d), \
			pre_vratio(%d), vfactor(%d)\n", \
			ctrl->sc.pre_hratio, ctrl->sc.hfactor, \
			ctrl->sc.pre_vratio, ctrl->sc.vfactor);


	dev_dbg(ctrl->dev, "pre_dst_width(%d), main_hratio(%d),\
			pre_dst_height(%d), main_vratio(%d)\n", \
			ctrl->sc.pre_dst_width, ctrl->sc.main_hratio, \
			ctrl->sc.pre_dst_height, ctrl->sc.main_vratio);

	/* Input DMA cannot support scaler bypass. */
	ctrl->sc.bypass = 0;

	ctrl->sc.scaleup_h = (dst.width >= src.width) ? 1 : 0;
	ctrl->sc.scaleup_v = (dst.height >= src.height) ? 1 : 0;

	ctrl->sc.shfactor = 10 - (ctrl->sc.hfactor + ctrl->sc.vfactor);

	ret = fimc_outdev_check_scaler(ctrl, &src, &dst);
	if (ret < 0)
		return ret;

	fimc_hwset_prescaler(ctrl);
	fimc_hwset_scaler(ctrl);

	return 0;
}

static int fimc_outdev_set_param(struct fimc_control *ctrl)
{
	int ret = -1;

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	fimc_hwset_enable_irq(ctrl, 0, 1);
	fimc_outdev_set_format(ctrl);
	fimc_outdev_set_path(ctrl);
	fimc_outdev_set_rot(ctrl);

	fimc_outdev_set_src_dma_offset(ctrl);
	ret = fimc_outdev_set_src_dma_size(ctrl);
	if (ret < 0)
		return ret;

	if (ctrl->out->fbuf.base)
		fimc_outdev_set_dst_dma_offset(ctrl);

	ret = fimc_outdev_set_dst_dma_size(ctrl);
	if (ret < 0)
		return ret;

	ret = fimc_outdev_set_scaler(ctrl);
	if (ret < 0)
		return ret;

	return 0;
}

static int fimc_fimd_rect(const struct fimc_control *ctrl,
		struct v4l2_rect *fimd_rect)
{
	switch (ctrl->out->rotate) {
	case 0:
		fimd_rect->left = ctrl->out->win.w.left;
		fimd_rect->top = ctrl->out->win.w.top;
		fimd_rect->width = ctrl->out->win.w.width;
		fimd_rect->height = ctrl->out->win.w.height;

		break;

	case 90:
		fimd_rect->left = ctrl->fb.lcd_hres -
					(ctrl->out->win.w.top \
						+ ctrl->out->win.w.height);
		fimd_rect->top = ctrl->out->win.w.left;
		fimd_rect->width = ctrl->out->win.w.height;
		fimd_rect->height = ctrl->out->win.w.width;

		break;

	case 180:
		fimd_rect->left = ctrl->fb.lcd_hres -
					(ctrl->out->win.w.left \
						+ ctrl->out->win.w.width);
		fimd_rect->top = ctrl->fb.lcd_vres -
					(ctrl->out->win.w.top \
						+ ctrl->out->win.w.height);
		fimd_rect->width = ctrl->out->win.w.width;
		fimd_rect->height = ctrl->out->win.w.height;

		break;

	case 270:
		fimd_rect->left = ctrl->out->win.w.top;
		fimd_rect->top = ctrl->fb.lcd_vres -
					(ctrl->out->win.w.left \
						+ ctrl->out->win.w.width);
		fimd_rect->width = ctrl->out->win.w.height;
		fimd_rect->height = ctrl->out->win.w.width;

		break;

	default:
		dev_err(ctrl->dev, "Rotation degree is inavlid\n");
		return -EINVAL;

		break;
	}

	return 0;
}

static int fimc_start_fifo(struct fimc_control *ctrl)
{
	struct v4l2_rect fimd_rect;
	struct fb_var_screeninfo var;
	struct s3cfb_user_window window;
	int ret = -1;
	u32 id = ctrl->id;

	memset(&fimd_rect, 0, sizeof(struct v4l2_rect));

	ret = fimc_fimd_rect(ctrl, &fimd_rect);
	if (ret < 0) {
		dev_err(ctrl->dev, "fimc_fimd_rect fail\n");
		return -EINVAL;
	}

	/* Get WIN var_screeninfo  */
	ret = s3cfb_direct_ioctl(id, FBIOGET_VSCREENINFO, (unsigned long)&var);
	if (ret < 0) {
		dev_err(ctrl->dev, "direct_ioctl(FBIOGET_VSCREENINFO) fail\n");
		return -EINVAL;
	}

	/* Don't allocate the memory. */
	ret = s3cfb_direct_ioctl(id, FBIO_ALLOC, 0);
	if (ret < 0) {
		dev_err(ctrl->dev, "direct_ioctl(FBIO_ALLOC) fail\n");
		return -EINVAL;
	}

	/* Update WIN size  */
	var.xres = fimd_rect.width;
	var.yres = fimd_rect.height;
	ret = s3cfb_direct_ioctl(id, FBIOPUT_VSCREENINFO, (unsigned long)&var);
	if (ret < 0) {
		dev_err(ctrl->dev, "direct_ioctl(FBIOPUT_VSCREENINFO) fail\n");
		return -EINVAL;
	}

	/* Update WIN position */
	window.x = fimd_rect.left;
	window.y = fimd_rect.top;
	ret = s3cfb_direct_ioctl(id, S3CFB_WIN_POSITION,
			(unsigned long)&window);
	if (ret < 0) {
		dev_err(ctrl->dev, "direct_ioctl(S3CFB_WIN_POSITION) fail\n");
		return -EINVAL;
	}

	/* Open WIN FIFO */
	ret = ctrl->fb.open_fifo(id, 0, fimc_outdev_start_camif, (void *)ctrl);
	if (ret < 0) {
		dev_err(ctrl->dev, "FIMD FIFO close fail\n");
		return -EINVAL;
	}

	return 0;
}

int fimc_reqbufs_output(void *fh, struct v4l2_requestbuffers *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = -1;

//	dev_info(ctrl->dev, "%s: called\n", __func__);

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	if (ctrl->out->is_requested == 1 && b->count != 0) {
		dev_err(ctrl->dev, "Buffers were already requested\n");
		return -EBUSY;
	}

	/* control user input */
	if (b->count > FIMC_OUTBUFS) {
		dev_warn(ctrl->dev, "The buffer count is modified by driver \
				from %d to %d\n", b->count, FIMC_OUTBUFS);
		b->count = FIMC_OUTBUFS;
	}

	/* Validation check & Initialize all buffers */
	ret = fimc_init_out_buf(ctrl, b->memory);
	if (ret)
		return ret;

	if (b->count != 0)	/* allocate buffers */
		ctrl->out->is_requested = 1;

	ctrl->out->buf_num = b->count;

	return 0;
}

int fimc_querybuf_output(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 buf_length = 0;

//	dev_info(ctrl->dev, "%s: called\n", __func__);

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	if (b->index > ctrl->out->buf_num) {
		dev_err(ctrl->dev, "The index is out of bounds. \n \
			You requested %d buffers. \
			But you set the index as %d\n",
			ctrl->out->buf_num, b->index);
		return -EINVAL;
	}

	b->flags = ctrl->out->buf[b->index].flags;
	b->m.offset = b->index * PAGE_SIZE;
	buf_length = ctrl->out->buf[b->index].length[FIMC_ADDR_Y] + \
			ctrl->out->buf[b->index].length[FIMC_ADDR_CB] + \
			ctrl->out->buf[b->index].length[FIMC_ADDR_CR];
 	b->length = buf_length;

	return 0;
}

int fimc_g_ctrl_output(void *fh, struct v4l2_control *c)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct s3c_platform_fimc *pdata = to_fimc_plat(ctrl->dev);

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	switch (c->id) {
	case V4L2_CID_ROTATION:
		c->value = ctrl->out->rotate;
		break;

#if 1
	// added by jamie (2009.08.25)
	case V4L2_CID_RESERVED_MEM_BASE_ADDR:
		c->value = ctrl->mem.base;
		break;
#endif
#if 0
	case V4L2_CID_FIMC_VERSION:	
		c->value = pdata->hw_ver;
		break;
#endif
	default:
		dev_err(ctrl->dev, "Invalid control id: %d\n", c->id);
		return -EINVAL;
	}

	return 0;
}

int fimc_s_ctrl_output(void *fh, struct v4l2_control *c)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = -1;

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	switch (c->id) {
	case V4L2_CID_ROTATION:
		ret = fimc_set_rot_degree(ctrl, c->value);
		break;

	default:
		dev_err(ctrl->dev, "Invalid control id: %d\n", c->id);
		ret = -EINVAL;
	}

	return ret;
}

int fimc_cropcap_output(void *fh, struct v4l2_cropcap *a)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct fimc_outinfo *out = ctrl->out;
	u32 pixelformat = ctrl->out->pix.pixelformat;
	u32 is_rotate = 0;
	u32 max_w = 0, max_h = 0;

//	dev_info(ctrl->dev, "%s: called\n", __func__);

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	is_rotate = fimc_mapping_rot_flip(ctrl->out->rotate, ctrl->out->flip);
	switch (pixelformat) {
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_NV61:		/* fall through */
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:	/* fall through */
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_YUV420:	/* fall through */
	case V4L2_PIX_FMT_YUYV:		/* fall through */
		max_w = FIMC_SRC_MAX_W;
		max_h = FIMC_SRC_MAX_H;		
	case V4L2_PIX_FMT_RGB32:	/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
		if (is_rotate & FIMC_ROT) {		/* Landscape mode */
			max_w = ctrl->fb.lcd_vres;
			max_h = ctrl->fb.lcd_hres;
		} else {				/* Portrait */
			max_w = ctrl->fb.lcd_hres;
			max_h = ctrl->fb.lcd_vres;
		}

		break;
	default: 
		dev_warn(ctrl->dev, "Supported format : V4L2_PIX_FMT_YUYV, \
				V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV12T, \
				V4L2_PIX_FMT_RGB32, V4L2_PIX_FMT_RGB565\n");
		return -EINVAL;
	}

	/* crop bounds */
	out->cropcap.bounds.left = 0;
	out->cropcap.bounds.top = 0;
	out->cropcap.bounds.width = max_w;
	out->cropcap.bounds.height = max_h;

	/* crop default values */
	out->cropcap.defrect.left = 0;
	out->cropcap.defrect.top = 0;
	out->cropcap.defrect.width = max_w;
	out->cropcap.defrect.height = max_h;

	/* crop pixel aspec values */
	/* To Do : Have to modify but I don't know the meaning. */
	out->cropcap.pixelaspect.numerator = 16;
	out->cropcap.pixelaspect.denominator = 9;

	a->bounds = out->cropcap.bounds;
	a->defrect = out->cropcap.defrect;
	a->pixelaspect = out->cropcap.pixelaspect;

	return 0;
}

int fimc_s_crop_output(void *fh, struct v4l2_crop *a)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;

//	dev_info(ctrl->dev, "%s: called\n", __func__);

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	/* Check arguments : widht and height */
	if ((a->c.width < 0) || (a->c.height < 0)) {
		dev_err(ctrl->dev, "The crop rect must be bigger than 0\n");
		return -EINVAL;
	}

	if ((a->c.width > FIMC_SRC_MAX_W) || (a->c.height > FIMC_SRC_MAX_H)) {
		dev_err(ctrl->dev, "The crop width/height must be smaller than \
				%d and %d\n", FIMC_SRC_MAX_W, FIMC_SRC_MAX_H);
		return -EINVAL;
	}

	/* Check arguments : left and top */
	if ((a->c.left < 0) || (a->c.top < 0)) {
		dev_err(ctrl->dev, "The crop rect left and top must be \
				bigger than zero\n");
		return -EINVAL;
	}

	if ((a->c.left > FIMC_SRC_MAX_W) || (a->c.top > FIMC_SRC_MAX_H)) {
		dev_err(ctrl->dev, "The crop left/top must be smaller than \
				%d, %d\n", FIMC_SRC_MAX_W, FIMC_SRC_MAX_H);
		return -EINVAL;
	}

	if ((a->c.left + a->c.width) > FIMC_SRC_MAX_W) {
		dev_err(ctrl->dev, "The crop rect must be in bound rect\n");
		return -EINVAL;
	}

	if ((a->c.top + a->c.height) > FIMC_SRC_MAX_H) {
		dev_err(ctrl->dev, "The crop rect must be in bound rect\n");
		return -EINVAL;
	}

	ctrl->out->crop.left = a->c.left;
	ctrl->out->crop.top = a->c.top;
	ctrl->out->crop.width = a->c.width;
	ctrl->out->crop.height = a->c.height;

	return 0;
}

int fimc_streamon_output(void *fh)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = -1;

//	dev_info(ctrl->dev, "%s: called\n", __func__);
#ifdef S5P6442_POWER_GATING_CAM
	s5p_fimc_out_domain_set(ctrl->id, 1);
#endif
	ret = fimc_outdev_check_param(ctrl);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail: fimc_check_param\n");
		return ret;
	}

	ret = fimc_outdev_set_param(ctrl);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail: fimc_outdev_set_param\n");
		return ret;
	}

	ctrl->status = FIMC_READY_ON;

	return ret;
}

int fimc_streamoff_output(void *fh)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 i = 0;
	int ret = -1;

//	dev_info(ctrl->dev, "%s: called\n", __func__);

	ret = fimc_outdev_stop_streaming(ctrl);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail: fimc_outdev_stop_streaming\n");
		return -EINVAL;
	}

	ret = fimc_init_in_queue(ctrl);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail: fimc_init_in_queue\n");
		return -EINVAL;
	}

	ret = fimc_init_out_queue(ctrl);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail: fimc_init_out_queue\n");
		return -EINVAL;
	}

	/* Make all buffers DQUEUED state. */
	for (i = 0; i < FIMC_OUTBUFS; i++) {
		ctrl->out->buf[i].state	= VIDEOBUF_IDLE;
		ctrl->out->buf[i].flags = V4L2_BUF_FLAG_MAPPED;
	}

	ctrl->out->idx.prev = -1;
	ctrl->out->idx.active = -1;
	ctrl->out->idx.next = -1;

	ctrl->status = FIMC_STREAMOFF;
#ifdef S5P6442_POWER_GATING_CAM
	s5p_fimc_out_domain_set(ctrl->id, 0);
#endif
	return 0;
}

static int fimc_qbuf_output_dma(struct fimc_control *ctrl)
{
	struct fimc_buf_set buf_set;
	u32 index = 0;
	int ret = -1;
	u32 i = 0;

	if ((ctrl->status == FIMC_READY_ON) || \
		(ctrl->status == FIMC_STREAMON_IDLE)) {
		ret =  fimc_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			dev_err(ctrl->dev, "Fail: fimc_detach_in_queue\n");
			return -EINVAL;
		}

		fimc_outdev_set_src_addr(ctrl, ctrl->out->buf[index].base);

		memset(&buf_set, 0x00, sizeof(buf_set));
		buf_set.base[FIMC_ADDR_Y] = (dma_addr_t)ctrl->out->fbuf.base;

		for (i = 0; i < FIMC_PHYBUFS; i++)
			fimc_hwset_output_address(ctrl, &buf_set, i);

		ret = fimc_outdev_start_camif(ctrl);
		if (ret < 0) {
			dev_err(ctrl->dev, "Fail: fimc_start_camif\n");
			return -EINVAL;
		}

		ctrl->out->idx.active = index;
		ctrl->status = FIMC_STREAMON;
	}

	return 0;
}

static int fimc_qbuf_output_fifo(struct fimc_control *ctrl)
{
	u32 index = 0;
	int ret = -1;

	if (ctrl->status == FIMC_READY_ON) {
		ret =  fimc_detach_in_queue(ctrl, &index);
		if (ret < 0) {
			dev_err(ctrl->dev, "Fail: fimc_detach_in_queue\n");
			return -EINVAL;
		}

		fimc_outdev_set_src_addr(ctrl, ctrl->out->buf[index].base);

		ret = fimc_start_fifo(ctrl);
		if (ret < 0) {
			dev_err(ctrl->dev, "Fail: fimc_start_fifo\n");
			return -EINVAL;
		}

		ctrl->out->idx.active = index;
		ctrl->status = FIMC_STREAMON;
	}

	return 0;
}

static int fimc_update_in_queue_addr(struct fimc_control *ctrl, u32 index, dma_addr_t *addr)
{
	if (index >= FIMC_OUTBUFS) {
		dev_err(ctrl->dev, "%s: Failed \n", __func__);
		return -EINVAL;
	}
	
	ctrl->out->buf[index].base[FIMC_ADDR_Y] = addr[FIMC_ADDR_Y];
	ctrl->out->buf[index].base[FIMC_ADDR_CB] = addr[FIMC_ADDR_CB];
	ctrl->out->buf[index].base[FIMC_ADDR_CR] = addr[FIMC_ADDR_CR];	

	return 0;
}

int fimc_qbuf_output(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	dma_addr_t dst_base = 0;
	int ret = -1;
	struct fimc_buf *buf = (struct fimc_buf *)b->m.userptr;

//	dev_info(ctrl->dev, "%s: queued idx = %d\n", __func__, b->index);

	if (b->index > ctrl->out->buf_num) {
		dev_err(ctrl->dev, "The index is out of bounds" 
			"You requested %d buffers. "
			"But you set the index as %d\n",
			ctrl->out->buf_num, b->index);
		return -EINVAL;
	}

	/* Check the buffer state if the state is VIDEOBUF_IDLE. */
	if (ctrl->out->buf[b->index].state != VIDEOBUF_IDLE) {
		dev_err(ctrl->dev, "The index(%d) buffer must be "
				"dequeued state(%d)\n", b->index, \
				ctrl->out->buf[b->index].state);
		return -EINVAL;
	}

	if (b->memory == V4L2_MEMORY_USERPTR) {
		ret = fimc_update_in_queue_addr(ctrl, b->index, buf->base);
		if (ret < 0)
			return ret;
	}

	/* Attach the buffer to the incoming queue. */
	ret =  fimc_attach_in_queue(ctrl, b->index);
	if (ret < 0) {
		dev_err(ctrl->dev, "Fail: fimc_attach_in_queue\n");
		return -EINVAL;
	}

	dst_base = (dma_addr_t)ctrl->out->fbuf.base;
	if (dst_base)
		ret = fimc_qbuf_output_dma(ctrl);
	else
		ret = fimc_qbuf_output_fifo(ctrl);

	return ret;
}

int fimc_dqbuf_output(void *fh, struct v4l2_buffer *b)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int index = -1, ret = -1;

	ret = fimc_detach_out_queue(ctrl, &index);
	if (ret < 0) {
		ret = wait_event_interruptible_timeout(ctrl->wq, \
			(ctrl->out->out_queue[0] != -1), FIMC_DQUEUE_TIMEOUT);
		if (ret == 0) {
			fimc_dump_context(ctrl);
			dev_err(ctrl->dev, "[0] out_queue is empty\n");
			return -EINVAL;
		} else if (ret == -ERESTARTSYS) {
			fimc_print_signal(ctrl);
		} else {
			/* Normal case */
			ret = fimc_detach_out_queue(ctrl, &index);
			if (ret < 0) {
				dev_err(ctrl->dev, "[1] out_queue is empty\n");
				fimc_dump_context(ctrl);
				return -EINVAL;
			}
		}
	}

	b->index = index;

//	dev_info(ctrl->dev, "%s: dqueued idx = %d\n", __func__, b->index);

	return ret;
}

int fimc_g_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	struct fimc_outinfo *out = ctrl->out;

//	dev_info(ctrl->dev, "%s: called\n", __func__);

	if (!out) {
		out = kzalloc(sizeof(*out), GFP_KERNEL);
		if (!out) {
			dev_err(ctrl->dev, "%s: no memory for " \
				"output device info\n", __func__);
			return -ENOMEM;
		}

		ctrl->out = out;

		ctrl->out->is_requested = 0;
		ctrl->out->rotate = 0;
		ctrl->out->flip	= 0;

		ctrl->out->idx.prev = -1;
		ctrl->out->idx.active = -1;
		ctrl->out->idx.next = -1;
	}

	f->fmt.pix = ctrl->out->pix;

	return 0;
}

int fimc_try_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	u32 format = f->fmt.pix.pixelformat;

//	dev_info(ctrl->dev, "%s: called. width(%d), height(%d)\n", \
			__func__, f->fmt.pix.width, f->fmt.pix.height);
	

	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	/* Check pixel format */
	switch (format) {
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_NV61:		/* fall through */
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:	/* fall through */
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB32:	/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
	case V4L2_PIX_FMT_YUV420:	/* fall through */ 
		break;
	default: 
		dev_warn(ctrl->dev, "Supported format : V4L2_PIX_FMT_YUYV, \
				V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV12T, \
				V4L2_PIX_FMT_RGB32, V4L2_PIX_FMT_RGB565\n");
		dev_warn(ctrl->dev, "Changed format : V4L2_PIX_FMT_RGB32\n");
		f->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
		return -EINVAL;
	}

	/* Fill the return value. */
	switch (format) {
	case V4L2_PIX_FMT_RGB32:
		f->fmt.pix.bytesperline	= f->fmt.pix.width<<2;
		break;
	case V4L2_PIX_FMT_NV16:		/* fall through */
	case V4L2_PIX_FMT_YUYV:		/* fall through */
	case V4L2_PIX_FMT_RGB565:	/* fall through */
		f->fmt.pix.bytesperline	= f->fmt.pix.width<<1;		
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_NV12:		/* fall through */
	case V4L2_PIX_FMT_NV12T:
	case V4L2_PIX_FMT_NV21:
		f->fmt.pix.bytesperline	= (f->fmt.pix.width * 3)>>1;
		break;

	default: 
		/* dummy value*/
		f->fmt.pix.bytesperline	= f->fmt.pix.width;
	}
	
	f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * f->fmt.pix.height;

	return 0;
}

int fimc_s_fmt_vid_out(struct file *filp, void *fh, struct v4l2_format *f)
{
	struct fimc_control *ctrl = (struct fimc_control *) fh;
	int ret = -1;

//	dev_info(ctrl->dev, "%s: called\n", __func__);

	/* Check stream status */
	if (ctrl->status != FIMC_STREAMOFF) {
		dev_err(ctrl->dev, "FIMC is running\n");
		return -EBUSY;
	}

	ret = fimc_try_fmt_vid_out(filp, fh, f);
	if (ret < 0)
		return ret;

	ctrl->out->pix = f->fmt.pix;

	return ret;
}

int fimc_init_in_queue(struct fimc_control *ctrl)
{
	unsigned long	spin_flags;
	unsigned int	i;

	spin_lock_irqsave(&ctrl->lock_in, spin_flags);

	/* Init incoming queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		ctrl->out->in_queue[i] = -1;

	spin_unlock_irqrestore(&ctrl->lock_in, spin_flags);

	return 0;
}

int fimc_init_out_queue(struct fimc_control *ctrl)
{
	unsigned long	spin_flags;
	unsigned int	i;

	spin_lock_irqsave(&ctrl->lock_out, spin_flags);

	/* Init incoming queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		ctrl->out->out_queue[i] = -1;

	spin_unlock_irqrestore(&ctrl->lock_out, spin_flags);

	return 0;
}

int fimc_attach_in_queue(struct fimc_control *ctrl, u32 index)
{
	unsigned long		spin_flags;
	int			swap_queue[FIMC_OUTBUFS];
	int			i;

	dev_dbg(ctrl->dev, "%s: index = %d\n", __func__, index);

	spin_lock_irqsave(&ctrl->lock_in, spin_flags);

	/* Backup original queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		swap_queue[i] = ctrl->out->in_queue[i];

	/* Attach new index */
	ctrl->out->in_queue[0] = index;
	ctrl->out->buf[index].state = VIDEOBUF_QUEUED;
	ctrl->out->buf[index].flags = V4L2_BUF_FLAG_MAPPED |
		V4L2_BUF_FLAG_QUEUED;

	/* Shift the origonal queue */
	for (i = 1; i < FIMC_OUTBUFS; i++)
		ctrl->out->in_queue[i] = swap_queue[i-1];

	spin_unlock_irqrestore(&ctrl->lock_in, spin_flags);

	return 0;
}

int fimc_detach_in_queue(struct fimc_control *ctrl, int *index)
{
	unsigned long		spin_flags;
	int			i, ret = 0;

	spin_lock_irqsave(&ctrl->lock_in, spin_flags);

	/* Find last valid index in incoming queue. */
	for (i = (FIMC_OUTBUFS-1); i >= 0; i--) {
		if (ctrl->out->in_queue[i] != -1) {
			*index = ctrl->out->in_queue[i];
			ctrl->out->in_queue[i] = -1;
			ctrl->out->buf[*index].state = VIDEOBUF_ACTIVE;
			ctrl->out->buf[*index].flags = V4L2_BUF_FLAG_MAPPED;
			break;
		}
	}

	/* incoming queue is empty. */
	if (i < 0)
		ret = -EINVAL;
	else
		dev_dbg(ctrl->dev, "%s: index = %d\n", __func__, *index);

	spin_unlock_irqrestore(&ctrl->lock_in, spin_flags);

	return ret;
}

int fimc_attach_out_queue(struct fimc_control *ctrl, u32 index)
{
	unsigned long		spin_flags;
	int			swap_queue[FIMC_OUTBUFS];
	int			i;

	dev_dbg(ctrl->dev, "%s: index = %d\n", __func__, index);

	spin_lock_irqsave(&ctrl->lock_out, spin_flags);

	/* Backup original queue */
	for (i = 0; i < FIMC_OUTBUFS; i++)
		swap_queue[i] = ctrl->out->out_queue[i];

	/* Attach new index */
	ctrl->out->out_queue[0]	= index;
	ctrl->out->buf[index].state = VIDEOBUF_DONE;
	ctrl->out->buf[index].flags = V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_DONE;

	/* Shift the origonal queue */
	for (i = 1; i < FIMC_OUTBUFS; i++)
		ctrl->out->out_queue[i] = swap_queue[i-1];

	spin_unlock_irqrestore(&ctrl->lock_out, spin_flags);

	return 0;
}

int fimc_detach_out_queue(struct fimc_control *ctrl, int *index)
{
	unsigned long		spin_flags;
	int			i, ret = 0;

	spin_lock_irqsave(&ctrl->lock_out, spin_flags);

	/* Find last valid index in outgoing queue. */
	for (i = (FIMC_OUTBUFS-1); i >= 0; i--) {
		if (ctrl->out->out_queue[i] != -1) {
			*index = ctrl->out->out_queue[i];
			ctrl->out->out_queue[i] = -1;
			ctrl->out->buf[*index].state = VIDEOBUF_IDLE;
			ctrl->out->buf[*index].flags = V4L2_BUF_FLAG_MAPPED;
			break;
		}
	}

	/* outgoing queue is empty. */
	if (i < 0) {
		ret = -EINVAL;
		dev_dbg(ctrl->dev, "%s: outgoing queue : %d, %d, %d\n",
			__func__, ctrl->out->out_queue[0],
			ctrl->out->out_queue[1], ctrl->out->out_queue[2]);
	} else
		dev_dbg(ctrl->dev, "%s: index = %d\n", __func__, *index);


	spin_unlock_irqrestore(&ctrl->lock_out, spin_flags);

	return ret;
}

void fimc_dump_context(struct fimc_control *ctrl)
{
	u32 i = 0;

	for (i = 0; i < FIMC_OUTBUFS; i++) {
		dev_err(ctrl->dev, "in_queue[%d] : %d\n", i, \
				ctrl->out->in_queue[i]);
	}

	for (i = 0; i < FIMC_OUTBUFS; i++) {
		dev_err(ctrl->dev, "out_queue[%d] : %d\n", i, \
				ctrl->out->out_queue[i]);
	}

	dev_err(ctrl->dev, "state : prev = %d, active = %d, next = %d\n", \
		ctrl->out->idx.prev, ctrl->out->idx.active,
		ctrl->out->idx.next);
}

void fimc_print_signal(struct fimc_control *ctrl)
{
	if (signal_pending(current)) {
		dev_dbg(ctrl->dev, ".pend=%.8lx shpend=%.8lx\n",
			current->pending.signal.sig[0],
			current->signal->shared_pending.signal.sig[0]);
	} else {
		dev_dbg(ctrl->dev, ":pend=%.8lx shpend=%.8lx\n",
			current->pending.signal.sig[0],
			current->signal->shared_pending.signal.sig[0]);
	}
}
