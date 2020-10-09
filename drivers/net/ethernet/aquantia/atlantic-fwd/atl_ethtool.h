/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_ETHTOOL_H_
#define _ATL_ETHTOOL_H_

enum atl_rxf_type {
	ATL_RXF_VLAN = 0,
	ATL_RXF_ETYPE,
	ATL_RXF_NTUPLE,
	ATL_RXF_FLEX,
};

s8 atl_reserve_filter(enum atl_rxf_type type);
void atl_release_filter(enum atl_rxf_type type);

#endif
