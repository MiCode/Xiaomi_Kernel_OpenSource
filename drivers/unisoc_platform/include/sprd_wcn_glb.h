/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SPRD_WCN_GLB_H
#define __SPRD_WCN_GLB_H

enum cp2_chip_type {
	type_unknow,
	m3lite_ums2653,
	m3_sc2355,
	m3e_umw2653,
	l6_ums2631,
	l3_ums2342,
};

enum cp2_chip_type wcn_get_cp2_type(void);

#endif //__SPRD_WCN_GLB_H

