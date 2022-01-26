/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef _SIA81XX_SET_VDD_H
#define _SIA81XX_SET_VDD_H

#include <linux/regmap.h>

#define SIA81XX_AUTO_VDD_EN_SET(en)		((en) & 0x1)
#define SIA81XX_AUTO_VDD_EN_GET(en)		((en) & 0x1)
#define SIA81XX_AUTO_PVDD_EN_SET(en)	(((en) & 0x1) << 1)
#define SIA81XX_AUTO_PVDD_EN_GET(en)	(((en) >> 1) & 0x1)

void sia81xx_set_auto_set_vdd_work_state(
	uint32_t timer_task_hdl, 
	uint32_t channel_num, 
	uint32_t state);

int sia81xx_auto_set_vdd_probe(
	uint32_t timer_task_hdl, 
	uint32_t chip_type,
	uint32_t channel_num,
	struct regmap *regmap,
	uint32_t cal_id, 
	uint32_t state);

int sia81xx_auto_set_vdd_remove(
	uint32_t timer_task_hdl, 
	uint32_t channel_num);

int sia81xx_set_vdd_init(void);
void sia81xx_set_vdd_exit(void);

#endif /* _SIA81XX_SET_VDD_H */

