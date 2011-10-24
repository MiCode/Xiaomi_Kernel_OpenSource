/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_DMA_FSM9XXX_H
#define __ASM_ARCH_MSM_DMA_FSM9XXX_H

/* DMA channels allocated to Scorpion */
#define DMOV_GP_CHAN            4
#define DMOV_CE1_IN_CHAN        5
#define DMOV_CE1_OUT_CHAN       6
#define DMOV_NAND_CHAN          7
#define DMOV_SDC1_CHAN          8
#define DMOV_GP2_CHAN           10
#define DMOV_CE2_IN_CHAN        12
#define DMOV_CE2_OUT_CHAN       13
#define DMOV_CE3_IN_CHAN        14
#define DMOV_CE3_OUT_CHAN       15

/* CRCIs */
#define DMOV_CE1_IN_CRCI        1
#define DMOV_CE1_OUT_CRCI       2
#define DMOV_CE1_HASH_CRCI      3

#define DMOV_NAND_CRCI_DATA     4
#define DMOV_NAND_CRCI_CMD      5

#define DMOV_SDC1_CRCI          6

#define DMOV_HSUART_TX_CRCI     7
#define DMOV_HSUART_RX_CRCI     8

#define DMOV_CE2_IN_CRCI        9
#define DMOV_CE2_OUT_CRCI       10
#define DMOV_CE2_HASH_CRCI      11

#define DMOV_CE3_IN_CRCI        12
#define DMOV_CE3_OUT_CRCI       13
#define DMOV_CE3_HASH_DONE_CRCI 14

/* Following CRCIs are not defined in FSM9XXX, but these are added to keep
 * the existing SDCC host controller driver compatible with FSM9XXX.
 */
#define DMOV_SDC2_CRCI         DMOV_SDC1_CRCI
#define DMOV_SDC3_CRCI         DMOV_SDC1_CRCI
#define DMOV_SDC4_CRCI         DMOV_SDC1_CRCI

#endif /* __ASM_ARCH_MSM_DMA_FSM9XXX_H */
