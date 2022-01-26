/*
 * Copyright (C) 2017 MediaTek Inc.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RS_PFM_H__
#define __RS_PFM_H__

/* VPU */
int rs_get_vpu_core_num(void);
int rs_get_vpu_opp_max(int core);
int rs_vpu_support_idletime(void);
int rs_get_vpu_curr_opp(int core);
int rs_get_vpu_ceiling_opp(int core);
int rs_vpu_opp_to_freq(int core, int step);

/* MDLA */
int rs_get_mdla_core_num(void);
int rs_get_mdla_opp_max(int core);
int rs_mdla_support_idletime(void);
int rs_get_mdla_curr_opp(int core);
int rs_get_mdla_ceiling_opp(int core);
int rs_mdla_opp_to_freq(int core, int step);

#endif
