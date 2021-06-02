/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __L3C_PART_H__
#define __L3C_PART_H__

enum {
	MTK_L3C_PART_MCU,
	MTK_L3C_PART_ACP
};

int mtk_l3c_set_mcu_part(unsigned int ratio);
int mtk_l3c_set_acp_part(unsigned int ratio);
int mtk_l3c_get_part(unsigned int id);

#endif /* __L3C_PART_H__ */

