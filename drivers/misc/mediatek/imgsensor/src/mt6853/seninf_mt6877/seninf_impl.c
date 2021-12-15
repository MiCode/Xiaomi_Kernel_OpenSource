/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <mt-plat/sync_write.h>

#include "seninf.h"

#define SENINF_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define SENINF_RD32(addr)          ioread32((void *)addr)
#ifdef _CAM_MUX_SWITCH
#define MAX_SWAP_TIMES 3
#define CAM_TG_NUM 3
#define MAX_CAM_CNT CAM_TG_NUM
#define CAM_MUX_NUM 13

struct swap_cammux_info_cache {
	unsigned int swap_cam_mux_src[MAX_SWAP_TIMES];
	unsigned int swap_cam_mux_dest[MAX_SWAP_TIMES];
	unsigned int used;
};

static struct swap_cammux_info_cache swapping_cammux[MAX_CAM_CNT];

void _swap_data_cam_mux(unsigned int cam_tg_offset,
			unsigned int camsv_tg_offset, struct SENINF *pseninf)
{
	int cam_reg_data = 0xFFFFFFFF;
	int sv_reg_data = 0xFFFFFFFF;

	cam_reg_data = SENINF_RD32(pseninf->pseninf_base[0] + cam_tg_offset);
	sv_reg_data = SENINF_RD32(pseninf->pseninf_base[0] + camsv_tg_offset);
	pr_debug("before 0x%x = 0x%x, 0x%x = 0x%x\n",
		cam_tg_offset, SENINF_RD32(pseninf->pseninf_base[0] + cam_tg_offset),
		camsv_tg_offset, SENINF_RD32(pseninf->pseninf_base[0] + camsv_tg_offset));
	SENINF_WR32(pseninf->pseninf_base[0] + cam_tg_offset, sv_reg_data);
	SENINF_WR32(pseninf->pseninf_base[0] + camsv_tg_offset, cam_reg_data);
	pr_debug("after 0x%x = 0x%x, 0x%x = 0x%x\n",
		cam_tg_offset, SENINF_RD32(pseninf->pseninf_base[0] + cam_tg_offset),
		camsv_tg_offset, SENINF_RD32(pseninf->pseninf_base[0] + camsv_tg_offset));
}

static MINT32 __do_swap(unsigned int tg_src, unsigned int tg_dest, struct SENINF *pseninf)
{
	int ret = -1;
	unsigned int cam_src = 0xf;
	unsigned int sv_src = 0xf;
	unsigned int cam_mux_ctrl_addr_offset = 0xFFFFFFFF;
	unsigned int cam_mux_ctrl_digit_offset = 0xFFFFFFFF;
	unsigned int sv_mux_ctrl_addr_offset = 0xFFFFFFFF;
	unsigned int sv_mux_ctrl_digit_offset = 0xFFFFFFFF;
	unsigned int cam_mux_ctrl_value = 0xFFFFFFFF;
	unsigned int sv_mux_ctrl_value = 0xFFFFFFFF;

	pr_debug("%s TG_SRC[%d] to TG_DEST[%d]", __func__, tg_src, tg_dest);

	if (tg_src < CAM_TG_NUM) {
		if (tg_dest > CAM_MUX_NUM || tg_dest == tg_src) {
			pr_debug("%s invalid tg_dest number = %d", __func__, tg_dest);
			return 0;
		}
	/* switch content of cam_mux */

	#define SENINF_CAM_MUX0_OPT 0x0420
	_swap_data_cam_mux(SENINF_CAM_MUX0_OPT + (tg_src * 4),
		SENINF_CAM_MUX0_OPT + (tg_dest * 4), pseninf);
	#define SENINF_CAM_MUX0_CHK_CTL_0 0x0500
	_swap_data_cam_mux(SENINF_CAM_MUX0_CHK_CTL_0 + (tg_src * 0x10),
		SENINF_CAM_MUX0_CHK_CTL_0 + (tg_dest * 0x10), pseninf);

	#define SENINF_CAM_MUX0_CHK_CTL_1 0x0504
	_swap_data_cam_mux(SENINF_CAM_MUX0_CHK_CTL_1 + (tg_src * 0x10),
		SENINF_CAM_MUX0_CHK_CTL_1 + (tg_dest * 0x10), pseninf);

	#define SENINF_CAM_MUX_CTRL_0 0x0400
	#define SENINF_CAM_MUX_CTRL_1 0x0404
	#define SENINF_CAM_MUX_CTRL_2 0x0408
	#define SENINF_CAM_MUX_CTRL_3 0x040c

		cam_mux_ctrl_addr_offset = SENINF_CAM_MUX_CTRL_0 + 4 * (tg_src / 4);
		cam_mux_ctrl_digit_offset = tg_src % 4;
		sv_mux_ctrl_addr_offset = SENINF_CAM_MUX_CTRL_0 + 4 * (tg_dest / 4);
		sv_mux_ctrl_digit_offset = tg_dest % 4;

		pr_debug("before cam 0x%x = 0x%x, sv 0x%x = 0x%x\n",
			cam_mux_ctrl_addr_offset,
			SENINF_RD32(pseninf->pseninf_base[0] + cam_mux_ctrl_addr_offset),
			sv_mux_ctrl_addr_offset,
			SENINF_RD32(pseninf->pseninf_base[0] + sv_mux_ctrl_addr_offset));

		cam_mux_ctrl_value =
			SENINF_RD32(pseninf->pseninf_base[0] + cam_mux_ctrl_addr_offset);
		if (cam_mux_ctrl_addr_offset == sv_mux_ctrl_addr_offset)
			sv_mux_ctrl_value = cam_mux_ctrl_value;
		else {
			sv_mux_ctrl_value =
				SENINF_RD32(pseninf->pseninf_base[0] + sv_mux_ctrl_addr_offset);
		}
		cam_src = (cam_mux_ctrl_value >> (cam_mux_ctrl_digit_offset * 8)) & 0xF;
		sv_src = (sv_mux_ctrl_value >> (sv_mux_ctrl_digit_offset * 8)) & 0xF;
		pr_debug("cam_src  0x%x, sv_src 0x%x\n", cam_src, sv_src);

		cam_mux_ctrl_value &= ~(0xF << (cam_mux_ctrl_digit_offset * 8));
		/* pr_debug("cam_mux_ctrl_value  0x%x\n", cam_mux_ctrl_value); */
		cam_mux_ctrl_value |= (sv_src << (cam_mux_ctrl_digit_offset * 8));
		/* pr_debug("cam_mux_ctrl_value  0x%x\n", cam_mux_ctrl_value); */
		SENINF_WR32(pseninf->pseninf_base[0]
			+ cam_mux_ctrl_addr_offset, cam_mux_ctrl_value);
		if (cam_mux_ctrl_addr_offset == sv_mux_ctrl_addr_offset) {
			sv_mux_ctrl_value = SENINF_RD32(
				pseninf->pseninf_base[0] + sv_mux_ctrl_addr_offset);
		}
		sv_mux_ctrl_value &= ~(0xF << (sv_mux_ctrl_digit_offset * 8));
		/* pr_debug("sv_mux_ctrl_value  0x%x\n", sv_mux_ctrl_value); */
		sv_mux_ctrl_value |= (cam_src << (sv_mux_ctrl_digit_offset * 8));
		/* pr_debug("sv_mux_ctrl_value  0x%x\n", sv_mux_ctrl_value); */

		SENINF_WR32(pseninf->pseninf_base[0] + sv_mux_ctrl_addr_offset, sv_mux_ctrl_value);

		pr_debug("after cam 0x%x = 0x%x, sv 0x%x = 0x%x\n",
			cam_mux_ctrl_addr_offset,
			SENINF_RD32(pseninf->pseninf_base[0] + cam_mux_ctrl_addr_offset),
			sv_mux_ctrl_addr_offset,
			SENINF_RD32(pseninf->pseninf_base[0] + sv_mux_ctrl_addr_offset));
		ret = 0;
	} else
		pr_debug("%s invalid cam_tg number = %d", __func__, tg_src);
	return ret;
}

extern MINT32 _switch_tg_for_stagger(unsigned int cam_tg, struct SENINF *pseninf)
{
	unsigned int i = 0;
	struct swap_cammux_info_cache *p_cache = NULL;

	if (cam_tg >= MAX_CAM_CNT) {
		pr_debug("%s camtg[%d] exceeds cam cnt[%d]", __func__, cam_tg, MAX_CAM_CNT);
		return 0;
	}

	p_cache = &swapping_cammux[cam_tg];

	pr_debug("%s used[%d] swap_cam_mux(src->dest): 0x%x->0x%x; 0x%x->0x%x; 0x%x->0x%x;",
		__func__, p_cache->used,
		p_cache->swap_cam_mux_src[0], p_cache->swap_cam_mux_dest[0],
		p_cache->swap_cam_mux_src[1], p_cache->swap_cam_mux_dest[1],
		p_cache->swap_cam_mux_src[2], p_cache->swap_cam_mux_dest[2]);

	for (; i < p_cache->used; ++i) {
		__do_swap(p_cache->swap_cam_mux_src[i],
				p_cache->swap_cam_mux_dest[i], pseninf);

		p_cache->swap_cam_mux_src[i] = 0;
		p_cache->swap_cam_mux_dest[i] = 0;
	}

	p_cache->used = 0;
	return 0;
}

extern MINT32 _seninf_set_tg_for_switch(unsigned int cam_tg_src, unsigned int cam_tg_dest)
{
	struct swap_cammux_info_cache *p_cache = NULL;

	if (cam_tg_src >= MAX_CAM_CNT) {
		pr_debug("%s camtg[%d] exceeds cam cnt[%d]", __func__, cam_tg_src, MAX_CAM_CNT);
		return 0;
	}

	p_cache = &swapping_cammux[cam_tg_src];

	if (p_cache->used >= MAX_SWAP_TIMES) {
		pr_debug("%d pairs of swap are already set", p_cache->used);
		return 0;
	}

	pr_debug("%s from TG[%d] to TG[%d]", __func__, cam_tg_src, cam_tg_dest);

	p_cache->swap_cam_mux_src[p_cache->used] = cam_tg_src;
	p_cache->swap_cam_mux_dest[p_cache->used] = cam_tg_dest;

	p_cache->used++;

	return 0;
}

extern MINT32 _seninf_irq(MINT32 Irq, void *DeviceId, struct SENINF *pseninf)
{
	pr_debug("seninf crc/ecc erorr irq 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
	SENINF_RD32(pseninf->pseninf_base[0] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[1] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[2] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[3] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[4] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[5] + 0x0Ac8));
	return 0;
}
#endif


