// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <linux/printk.h>
#include <linux/stddef.h>
#include "mtk_drm_rc.h"
#include "nt36672c_fhdp_shenchao_rc.h"

static const struct mtk_lcm_rc_pattern mtk_lcm_rc_pattern_list[] = {
	/* nt36672c_fhdp_sc*/
	{
		.name			= "NT36672C_FHDP_SHENCHAO_RC",
		.left_top		= {864, nt36672c_fhdp_sc_left_top_pattern},
		.left_top_left	= {0, NULL},
		.left_top_right	= {0, NULL},
		.right_top		= {0, NULL},
		.left_bottom	= {0, NULL},
		.right_bottom	= {0, NULL},
	},
};

static void lcm_rc_check_size(unsigned int *src, unsigned int dst)
{
	if (src == NULL || *src == dst)
		return;

	pr_info("%s: warning not match:0x%x, 0x%x\n",
		__func__, *src, dst);

	if (*src > dst)
		*src = dst;
}

void *mtk_lcm_get_rc_addr(const char *name, enum mtk_lcm_rc_locate locate,
		unsigned int *size)
{
	unsigned int i = 0, dst_size = 0;
	void *addr;
	unsigned int count = sizeof(mtk_lcm_rc_pattern_list) /
			sizeof(struct mtk_lcm_rc_pattern);

	if (IS_ERR_OR_NULL(size) || *size == 0 || IS_ERR_OR_NULL(name))
		return NULL;

	for (i = 0; i < count; i++) {
		if (strcmp(mtk_lcm_rc_pattern_list[i].name, name) == 0)
			break;
	}

	if (i == count) {
		pr_info("%s, %d, failed to find pattern\n", __func__, __LINE__);
		return NULL;
	}

	switch (locate) {
	case RC_LEFT_TOP:
		dst_size = mtk_lcm_rc_pattern_list[i].left_top.size;
		addr = mtk_lcm_rc_pattern_list[i].left_top.addr;
		break;
	case RC_LEFT_TOP_LEFT:
		dst_size = mtk_lcm_rc_pattern_list[i].left_top_left.size;
		addr = mtk_lcm_rc_pattern_list[i].left_top_left.addr;
		break;
	case RC_LEFT_TOP_RIGHT:
		dst_size = mtk_lcm_rc_pattern_list[i].left_top_right.size;
		addr = mtk_lcm_rc_pattern_list[i].left_top_right.addr;
		break;
	case RC_RIGHT_TOP:
		dst_size = mtk_lcm_rc_pattern_list[i].right_top.size;
		addr = mtk_lcm_rc_pattern_list[i].right_top.addr;
		break;
	case RC_LEFT_BOTTOM:
		dst_size = mtk_lcm_rc_pattern_list[i].left_bottom.size;
		addr = mtk_lcm_rc_pattern_list[i].left_bottom.addr;
		break;
	case RC_RIGHT_BOTTOM:
		dst_size = mtk_lcm_rc_pattern_list[i].right_bottom.size;
		addr = mtk_lcm_rc_pattern_list[i].right_bottom.addr;
		break;
	default:
		addr = NULL;
		break;
	}

	if (dst_size > 0 && addr != NULL) {
		lcm_rc_check_size(size, dst_size);
		pr_info("%s, %d, pattern name:%s, locate:%d, addr=0x%lx,size=%u\n",
			__func__, __LINE__, name, locate, (unsigned long)addr, *size);
	} else {
		pr_info("%s, %d, pattern name:%s, locate:%d, empty pattern\n",
			__func__, __LINE__, name, locate);
	}

	return addr;
}
