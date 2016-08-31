/*
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * include/linux/platform_data/tmon_tmp411.h
 *
 */


#ifndef __TMON_TMP411_INCL
#define __TMON_TMP411_INCL

struct tmon_plat_data {
	signed int delta_temp;
	signed int delta_time;
	signed int remote_offset;
	int utmip_temp_bound;
	void (*ltemp_dependent_reg_update)(int ltemp);
	void (*utmip_temp_dep_update)(int rtemp, int utmip_temp_bound);
};


#endif
