/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VDEC_DRV_BASE_
#define _VDEC_DRV_BASE_

#include "mtk_vcodec_drv.h"

#include "vdec_drv_if.h"

struct vdec_common_if {
	/**
	 * (*init)() - initialize decode driver
	 * @ctx     : [in] mtk v4l2 context
	 * @bs      : [in] input bitstream
	 * @h_vdec  : [out] driver handle
	 * @pic     : [out] width and height of bitstream
	 */
	int (*init)(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
		    unsigned long *h_vdec, struct vdec_pic_info *pic);

	/**
	 * (*decode)() - trigger decode
	 * @h_vdec  : [in] driver handle
	 * @bs      : [in] input bitstream
	 * @fb      : [in] frame buffer to store decoded frame
	 * @res_chg : [out] resolution change happen
	 */
	int (*decode)(unsigned long h_vdec, struct mtk_vcodec_mem *bs,
		      struct vdec_fb *fb, bool *res_chg);

	/**
	 * (*get_param)() - get driver's parameter
	 * @h_vdec : [in] driver handle
	 * @type   : [in] input parameter type
	 * @out    : [out] buffer to store query result
	 */
	int (*get_param)(unsigned long h_vdec, enum vdec_get_param_type type,
			 void *out);

	/**
	 * (*deinit)() - deinitialize driver.
	 * @h_vdec : [in] driver handle to be deinit
	 */
	int (*deinit)(unsigned long h_vdec);
};


#endif
