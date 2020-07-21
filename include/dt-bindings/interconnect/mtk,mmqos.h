/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Ming-Fan Chen <ming-fan.chen@mediatek.com>
 */

#ifndef __DT_BINDINGS_INTERCONNECT_MTK_MMQOS_H
#define __DT_BINDINGS_INTERCONNECT_MTK_MMQOS_H

#define MTK_MMQOS_MAX_BW	(0x10000000)

#define MTK_MMQOS_NODE_COMMON		(0x1)
#define MTK_MMQOS_NODE_COMMON_PORT	(0x2)
#define MTK_MMQOS_NODE_LARB		(0x3)
#define MTK_MMQOS_NODE_LARB_PORT	(0x4)


#define SLAVE_COMMON(common) \
	((MTK_MMQOS_NODE_COMMON << 16) | ((common) & 0xffff))
#define MASTER_COMMON_PORT(common, port) \
	((MTK_MMQOS_NODE_COMMON_PORT << 16) | \
	(((common) & 0xff) << 8) | ((port) & 0xff))

#define SLAVE_LARB(larb) \
	((MTK_MMQOS_NODE_LARB << 16) | ((larb) & 0xffff))
#define MASTER_LARB_PORT(port) \
	((MTK_MMQOS_NODE_LARB_PORT << 16) | ((port) & 0xffff))

#endif
