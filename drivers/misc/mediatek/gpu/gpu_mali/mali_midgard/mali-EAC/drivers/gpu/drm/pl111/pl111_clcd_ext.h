/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */


/**
 * pl111_clcd_ext.h
 * Extended CLCD register definitions
 */

#ifndef PL111_CLCD_EXT_H_
#define PL111_CLCD_EXT_H_

/* 
 * PL111 cursor register definitions not defined in the kernel's clcd header.
 * 
 * TODO MIDEGL-1718: move to include/linux/amba/clcd.h
 */

#define CLCD_CRSR_IMAGE				0x00000800
#define CLCD_CRSR_IMAGE_MAX_WORDS		256
#define CLCD_CRSR_IMAGE_WORDS_PER_LINE		4
#define CLCD_CRSR_IMAGE_PIXELS_PER_WORD	16

#define CLCD_CRSR_LBBP_COLOR_MASK	0x00000003
#define CLCD_CRSR_LBBP_BACKGROUND	0x0
#define CLCD_CRSR_LBBP_FOREGROUND	0x1
#define CLCD_CRSR_LBBP_TRANSPARENT	0x2
#define CLCD_CRSR_LBBP_INVERSE		0x3


#define CLCD_CRSR_CTRL			0x00000c00
#define CLCD_CRSR_CONFIG		0x00000c04
#define CLCD_CRSR_PALETTE_0		0x00000c08
#define CLCD_CRSR_PALETTE_1		0x00000c0c
#define CLCD_CRSR_XY			0x00000c10
#define CLCD_CRSR_CLIP			0x00000c14
#define CLCD_CRSR_IMSC			0x00000c20
#define CLCD_CRSR_ICR			0x00000c24
#define CLCD_CRSR_RIS			0x00000c28
#define CLCD_MIS				0x00000c2c

#define CRSR_CTRL_CRSR_ON		(1 << 0)
#define CRSR_CTRL_CRSR_MAX		3
#define CRSR_CTRL_CRSR_NUM_SHIFT	4
#define CRSR_CTRL_CRSR_NUM_MASK		\
	(CRSR_CTRL_CRSR_MAX << CRSR_CTRL_CRSR_NUM_SHIFT)
#define CRSR_CTRL_CURSOR_0		0
#define CRSR_CTRL_CURSOR_1		1
#define CRSR_CTRL_CURSOR_2		2
#define CRSR_CTRL_CURSOR_3		3

#define CRSR_CONFIG_CRSR_SIZE		(1 << 0)
#define CRSR_CONFIG_CRSR_FRAME_SYNC	(1 << 1)

#define CRSR_PALETTE_RED_SHIFT		0
#define CRSR_PALETTE_GREEN_SHIFT	8
#define CRSR_PALETTE_BLUE_SHIFT		16

#define CRSR_PALETTE_RED_MASK		0x000000ff
#define CRSR_PALETTE_GREEN_MASK		0x0000ff00
#define CRSR_PALETTE_BLUE_MASK		0x00ff0000
#define CRSR_PALETTE_MASK		(~0xff000000)

#define CRSR_XY_MASK			0x000003ff
#define CRSR_XY_X_SHIFT			0
#define CRSR_XY_Y_SHIFT			16

#define CRSR_XY_X_MASK			CRSR_XY_MASK
#define CRSR_XY_Y_MASK			(CRSR_XY_MASK << CRSR_XY_Y_SHIFT)

#define CRSR_CLIP_MASK			0x3f
#define CRSR_CLIP_X_SHIFT		0
#define CRSR_CLIP_Y_SHIFT		8

#define CRSR_CLIP_X_MASK		CRSR_CLIP_MASK
#define CRSR_CLIP_Y_MASK		(CRSR_CLIP_MASK << CRSR_CLIP_Y_SHIFT)

#define CRSR_IMSC_CRSR_IM		(1<<0)
#define CRSR_ICR_CRSR_IC		(1<<0)
#define CRSR_RIS_CRSR_RIS		(1<<0)
#define CRSR_MIS_CRSR_MIS		(1<<0)

#endif /* PL111_CLCD_EXT_H_ */
