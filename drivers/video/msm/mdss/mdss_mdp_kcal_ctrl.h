
/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2013, LGE Inc. All rights reserved
 * Copyright (c) 2014, savoca <adeddo27@gmail.com>
 * Copyright (c) 2014, Paul Reioux <reioux@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MDSS_MDP_KCAL_CTRL_H
#define __MDSS_MDP_KCAL_CTRL_H

#define NUM_QLUT 0x100
#define DEF_PA 0xff
#define PCC_ADJ 0x80

struct kcal_lut_data {
	int red;
	int green;
	int blue;
	int minimum;
	int enable;
	int invert;
	int sat;
	int hue;
	int val;
	int cont;
};

void mdss_mdp_pp_kcal_update(struct kcal_lut_data *lut_data);
void mdss_mdp_pp_kcal_pa(struct kcal_lut_data *lut_data);
void mdss_mdp_pp_kcal_invert(struct kcal_lut_data *lut_data);
#endif
