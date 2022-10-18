/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_LX7_REG_H_
#define _CAM_LX7_REG_H_

/* ICP_SYS - Protected reg space defined in AC policy */
#define ICP_LX7_SYS_RESET      0x0
#define ICP_LX7_SYS_CONTROL    0x4
#define ICP_LX7_SYS_STATUS     0xC
#define ICP_LX7_SYS_ACCESS     0x10

#define ICP_LX7_STANDBYWFI     (1 << 7)
#define ICP_LX7_EN_CPU         (1 << 9)
#define ICP_LX7_FUNC_RESET     (1 << 4)

#define ICP_LX7_CIRQ_OB_MASK   0x0
#define ICP_LX7_CIRQ_OB_CLEAR  0x4
#define ICP_LX7_CIRQ_OB_STATUS 0xc

/* ICP WD reg space */
#define ICP_LX7_WD_CTRL        0x8
#define ICP_LX7_WD_INTCLR      0xC

/* These bitfields are shared by OB_MASK, OB_CLEAR, OB_STATUS */
#define LX7_WDT_BITE_WS1       (1 << 6)
#define LX7_WDT_BARK_WS1       (1 << 5)
#define LX7_WDT_BITE_WS0       (1 << 4)
#define LX7_WDT_BARK_WS0       (1 << 3)
#define LX7_ICP2HOSTINT        (1 << 2)

#define ICP_LX7_CIRQ_OB_IRQ_CMD 0x10
#define LX7_IRQ_CLEAR_CMD       (1 << 1)

#define ICP_LX7_CIRQ_IB_STATUS0   0x70
#define ICP_LX7_CIRQ_IB_STATUS1   0x74
#define ICP_LX7_CIRQ_HOST2ICPINT  0x124
#define ICP_LX7_CIRQ_PFAULT_INFO  0x128
#define LX7_HOST2ICPINT          (1 << 0)

#endif /* _CAM_LX7_REG_H_ */
