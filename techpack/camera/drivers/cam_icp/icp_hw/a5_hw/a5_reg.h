/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_A5_REG_H_
#define _CAM_A5_REG_H_

#define ICP_SIERRA_A5_CSR_NSEC_RESET    0x4
#define A5_CSR_FUNC_RESET               (1 << 4)
#define A5_CSR_DBG_RESET                (1 << 3)
#define A5_CSR_CPU_RESET                (1 << 2)

#define ICP_SIERRA_A5_CSR_A5_CONTROL    0x8
#define A5_CSR_DBGSWENABLE              (1 << 22)
#define A5_CSR_EDBGRQ                   (1 << 14)
#define A5_CSR_EN_CLKGATE_WFI           (1 << 12)
#define A5_CSR_A5_CPU_EN                (1 << 9)
#define A5_CSR_WAKE_UP_EN               (1 << 4)

#define A5_CSR_FULL_DBG_EN      (A5_CSR_DBGSWENABLE | A5_CSR_EDBGRQ)
#define A5_CSR_FULL_CPU_EN      (A5_CSR_A5_CPU_EN | \
				A5_CSR_WAKE_UP_EN | \
				A5_CSR_EN_CLKGATE_WFI)

#define ICP_SIERRA_A5_CSR_A2HOSTINTEN   0x10
#define A5_WDT_WS1EN                    (1 << 2)
#define A5_WDT_WS0EN                    (1 << 1)
#define A5_A2HOSTINTEN                  (1 << 0)

#define ICP_SIERRA_A5_CSR_HOST2ICPINT   0x30
#define A5_HOSTINT                      (1 << 0)

#define ICP_SIERRA_A5_CSR_A5_STATUS     0x200
#define A5_CSR_A5_STANDBYWFI            (1 << 7)

#endif /* _CAM_A5_REG_H_ */
