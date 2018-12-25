/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk-hw-compoent.h
 *
 * Project:
 * --------
 *   Audio soc vendor plarform relate
 *
 * Description:
 * ------------
 *   Audio machine driver
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 ******************************************************************************
 */

#ifndef _MTK_HW_COMPONENT_H_
#define _MTK_HW_COMPONENT_H_

#include <linux/debugfs.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>


unsigned int get_hw_version(void);
int get_exthp_dai_codec_name(struct snd_soc_dai_link *mt_soc_exthp_dai);
int get_extspk_dai_codec_name(struct snd_soc_dai_link *mt_soc_extspk_dai);

extern int mt_set_gpio_mode(unsigned long pin, unsigned long mode);
extern int mt_set_gpio_dir(unsigned long pin, unsigned long dir);
extern int mt_set_gpio_pull_enable(unsigned long pin, unsigned long enable);
extern int mt_set_gpio_pull_select(unsigned long pin, unsigned long select);

#endif
