// SPDX-License-Identifier: GPL-2.0
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
#include <linux/string.h>

#include "seninf.h"
#include "platform_common.h"

#define SENINF_WR32(addr, data) \
do {	\
	writel((data), (void __force __iomem *)((addr))); \
	mb(); /* memory barrier */ \
} while (0)

#define SENINF_RD32(addr) ioread32((void *)addr)
#ifdef _CAM_MUX_SWITCH
#define CAM_TG_NUM 3
#define CAM_MUX_NUM 13
static unsigned int _tg_map_cam_2_sv[CAM_TG_NUM] = {0xff, 0xff, 0xff};
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
MINT32 _switch_tg_for_stagger(unsigned int cam_tg, struct SENINF *pseninf)
{
	int ret = -1;
	unsigned int camsv_tg = 0xff;
	unsigned int cam_src = 0xf;
	unsigned int sv_src = 0xf;
	unsigned int cam_mux_ctrl_addr_offset = 0xFFFFFFFF;
	unsigned int cam_mux_ctrl_digit_offset = 0xFFFFFFFF;
	unsigned int sv_mux_ctrl_addr_offset = 0xFFFFFFFF;
	unsigned int sv_mux_ctrl_digit_offset = 0xFFFFFFFF;
	unsigned int cam_mux_ctrl_value = 0xFFFFFFFF;
	unsigned int sv_mux_ctrl_value = 0xFFFFFFFF;

	pr_debug("%s input cam_tg number = %d _tg_map_cam_2_sv 0x%x 0x%x 0x%x",
		__func__, cam_tg, _tg_map_cam_2_sv[0], _tg_map_cam_2_sv[1], _tg_map_cam_2_sv[2]);

	if (cam_tg < CAM_TG_NUM) {
		camsv_tg = _tg_map_cam_2_sv[cam_tg];
		if (camsv_tg > CAM_MUX_NUM || camsv_tg == cam_tg) {
			pr_debug("%s invalid camsv_tg number = %d", __func__, camsv_tg);
			return 0;
		}
	/* switch content of cam_mux */

	#define SENINF_CAM_MUX0_OPT 0x0420
	_swap_data_cam_mux(SENINF_CAM_MUX0_OPT + (cam_tg * 4),
		SENINF_CAM_MUX0_OPT + (camsv_tg * 4), pseninf);
	#define SENINF_CAM_MUX0_CHK_CTL_0 0x0500
	_swap_data_cam_mux(SENINF_CAM_MUX0_CHK_CTL_0 + (cam_tg * 0x10),
		SENINF_CAM_MUX0_CHK_CTL_0 + (camsv_tg * 0x10), pseninf);

	#define SENINF_CAM_MUX0_CHK_CTL_1 0x0504
	_swap_data_cam_mux(SENINF_CAM_MUX0_CHK_CTL_1 + (cam_tg * 0x10),
		SENINF_CAM_MUX0_CHK_CTL_1 + (camsv_tg * 0x10), pseninf);

	#define SENINF_CAM_MUX_CTRL_0 0x0400
	#define SENINF_CAM_MUX_CTRL_1 0x0404
	#define SENINF_CAM_MUX_CTRL_2 0x0408
	#define SENINF_CAM_MUX_CTRL_3 0x040c

		cam_mux_ctrl_addr_offset = SENINF_CAM_MUX_CTRL_0 + 4 * (cam_tg / 4);
		cam_mux_ctrl_digit_offset = cam_tg % 4;
		sv_mux_ctrl_addr_offset = SENINF_CAM_MUX_CTRL_0 + 4 * (camsv_tg / 4);
		sv_mux_ctrl_digit_offset = camsv_tg % 4;

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
		_tg_map_cam_2_sv[cam_tg] = 0xff;
		ret = 0;
	} else
		pr_debug("%s invalid cam_tg number = %d", __func__, cam_tg);
	return ret;
}
MINT32 _seninf_set_tg_for_switch(unsigned int cam_tg, unsigned int camsv_tg)
{
	pr_debug("%s cam_tg number = %d camsv_tg = %d",
			__func__, cam_tg, camsv_tg);
	if (cam_tg < CAM_TG_NUM)
		_tg_map_cam_2_sv[cam_tg] = camsv_tg;
	else
		pr_debug("%s invalid cam_tg number = %d camsv_tg = %d",
			__func__, cam_tg, camsv_tg);
	return 0;
}

MINT32 _seninf_irq(MINT32 Irq, void *DeviceId, struct SENINF *pseninf)
{
	unsigned int seninf_max_num = 0;
	unsigned int i = 0;
	int ret = 0;
	char str_prop_name[LENGTH_FOR_SNPRINTF];
	seninf_max_num = pseninf->g_seninf_max_num_id;

	memset(str_prop_name, 0, sizeof(str_prop_name));
	ret = snprintf(str_prop_name, sizeof(str_prop_name),
			"seninf crc/ecc erorr irq ");

	if (ret < 0)
		pr_info("ERROR: LOG encoding error, ret:%d\n", ret);


	for (i = 0; i < seninf_max_num; i++) {
		ret = snprintf(str_prop_name + strlen(str_prop_name),
			LENGTH_FOR_SNPRINTF - strlen(str_prop_name), 
			"0x%08x ",
			SENINF_RD32(pseninf->pseninf_base[i] + 0x0Ac8));

		if (ret < 0)
			pr_info("ERROR: LOG encoding error, ret:%d\n", ret);
	}


	pr_debug("%s\n", str_prop_name);

	return 0;
}

#endif


