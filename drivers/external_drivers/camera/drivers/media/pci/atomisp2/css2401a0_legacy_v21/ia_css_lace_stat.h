/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __IA_CSS_LACE_STAT_H
#define __IA_CSS_LACE_STAT_H

/** @file
 * This file contains types used for LACE statistics
 */

#if defined(IS_ISP_2500_SYSTEM)
#include <components/acc_cluster/acc_lace_stat/lace_stat_public.h>
#endif


struct ia_css_isp_lace_statistics;

#if defined(IS_ISP_2500_SYSTEM)
/** @brief Copy LACE statistics from an ACC buffer to a host
 *         buffer.
 * @param[in]	host_stats Host buffer.
 * @param[in]	isp_stats ISP buffer.
 * @return		None
 */
void ia_css_get_lace_statistics(struct ia_css_lace_statistics *host_stats,
			const struct ia_css_isp_lace_statistics *isp_stats);
#endif

/** @brief Allocate mem for the LACE statistics on the ISP
 * @return	Pointer to the allocated LACE statistics
 *         buffer on the ISP
*/
struct ia_css_isp_lace_statistics *ia_css_lace_statistics_allocate(void);

/** @brief Free the ACC LACE statistics memory on the isp
 * @param[in]	me Pointer to the LACE statistics buffer on the
 *       ISP.
 * @return		None
*/
void ia_css_lace_statistics_free(struct ia_css_isp_lace_statistics *me);

#endif /*  __IA_CSS_LACE_STAT_H */
