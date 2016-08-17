/*
 * arch/arm/mach-tegra/include/mach/dtv.h
 *
 * Header file for describing resources of DTV module
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Authors:
 *     Adam Jiang <chaoj@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifndef __DTV_H__
#define __DTV_H__

/* offsets from TEGRA_DTV_BASE */
#define DTV_SPI_CONTROL         0x40
#define DTV_MODE                0x44
#define DTV_CTRL                0x48
#define DTV_PACKET_COUNT        0x4c
#define DTV_ERROR_COUNT         0x50
#define DTV_INTERRUPT_STATUS    0x54
#define DTV_STATUS              0x58
#define DTV_RX_FIFO             0x5c

/* DTV_SPI_CONTROL */
#define DTV_SPI_CONTROL_ENABLE_DTV  1

/* DTV_MODE_0 */
#define DTV_MODE_BYTE_SWIZZLE_SHIFT 6
#define DTV_MODE_BYTE_SWIZZLE       (1 << DTV_MODE_BYTE_SWIZZLE_SHIFT)
#define DTV_MODE_BYTE_SWIZZLE_MASK  1
#define DTV_MODE_BIT_SWIZZLE_SHIFT  5
#define DTV_MODE_BIT_SWIZZLE        (1 << DTV_MODE_BIT_SWIZZLE_SHIFT)
#define DTV_MODE_BIT_SWIZZLE_MASK   1
#define DTV_MODE_CLK_EDGE_SHIFT     4
#define DTV_MODE_CLK_EDGE_MASK      1
#define DTV_MODE_CLK_EDGE_NEG       (1 << DTV_MODE_CLK_EDGE_SHIFT)
#define DTV_MODE_PRTL_SEL_SHIFT     2
#define DTV_MODE_PRTL_SEL_MASK      (0x3 << DTV_MODE_PRTL_SEL_SHIFT)
#define DTV_MODE_CLK_MODE_SHIFT     1
#define DTV_MODE_CLK_MODE_MASK      (0x1 << DTV_MODE_CLK_MODE_SHIFT)
#define DTV_MODE_PRTL_ENABLE        1

/* DTV_CONTROL_0 */
#define DTV_CTRL_FEC_SIZE_SHIFT         24
#define DTV_CTRL_FEC_SIZE_MASK          (0x7F << DTV_CTRL_FEC_SIZE_SHIFT)
#define DTV_CTRL_BODY_SIZE_SHIFT        16
#define DTV_CTRL_BODY_SIZE_MASK         (0xFF << DTV_CTRL_BODY_SIZE_SHIFT)
#define DTV_CTRL_FIFO_ATTN_LEVEL_SHIFT  8
#define DTV_CTRL_FIFO_ATTN_LEVEL_MASK   (0x1F << DTV_CTRL_FIFO_ATTN_LEVEL_SHIFT)
#define DTV_CTRL_FIFO_ATTN_ONE_WORD     (0 << DTV_CTRL_FIFO_ATTN_LEVEL_SHIFT)
#define DTV_CTRL_FIFO_ATTN_TWO_WORD     (1 << DTV_CTRL_FIFO_ATTN_LEVEL_SHIFT)
#define DTV_CTRL_FIFO_ATTN_THREE_WORD   (2 << DTV_CTRL_FIFO_ATTN_LEVEL_SHIFT)
#define DTV_CTRL_FIFO_ATTN_FOUR_WORD    (3 << DTV_CTRL_FIFO_ATTN_LEVEL_SHIFT)
#define DTV_CTRL_BODY_VALID_SEL_SHIFT   6
#define DTV_CTRL_BODY_VALID_SEL_MASK    (1 << DTV_CTRL_BODY_VALID_SEL_SHIFT)
#define DTV_CTRL_START_SEL_SHIFT        4
#define DTV_CTRL_START_SEL_MASK         (1 << DTV_CTRL_START_SEL_SHIFT)
#define DTV_CTRL_ERROR_POLARITY_SHIFT   2
#define DTV_CTRL_ERROR_POLARITY_MASK    (1 << DTV_CTRL_ERROR_POLARITY_SHIFT)
#define DTV_CTRL_PSYNC_POLARITY_SHIFT   1
#define DTV_CTRL_PSYNC_POLARITY_MASK    (1 << DTV_CTRL_PSYNC_POLARITY_SHIFT)
#define DTV_CTRL_VALID_POLARITY_SHIFT   0
#define DTV_CTRL_VALID_POLARITY_MASK    (1 << DTV_CTRL_VALID_POLARITY_SHIFT)

/* DTV_INTERRUPT_STATUS_0 */
#define DTV_INTERRUPT_PACKET_UNDERRUN_ERR 8
#define DTV_INTERRUPT_BODY_OVERRUN_ERR    4
#define DTV_INTERRUPT_BODY_UNDERRUN_ERR   2
#define DTV_INTERRUPT_UPSTREAM_ERR        1

/* DTV_STATUS_0 */
#define DTV_STATUS_RXF_UNDERRUN 4
#define DTV_STATUS_RXF_EMPTY    2
#define DTV_STATUS_RXF_FULL     1

#endif /* __DTV_H__ */
