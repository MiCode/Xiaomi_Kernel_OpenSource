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
 *
 */

#ifndef __ASM_ARCH_MSM_SIRC_FSM9XXX_H
#define __ASM_ARCH_MSM_SIRC_FSM9XXX_H

/* Group A */
#define INT_EBI2_WR_ER_DONE           (FIRST_SIRC_IRQ + 0)
#define INT_EBI2_OP_DONE              (FIRST_SIRC_IRQ + 1)
#define INT_SDC1_0                    (FIRST_SIRC_IRQ + 2)
#define INT_SDC1_1                    (FIRST_SIRC_IRQ + 3)
#define INT_UARTDM                    (FIRST_SIRC_IRQ + 4)
#define INT_UART1                     (FIRST_SIRC_IRQ + 5)
/*  RESERVED 6 */
#define INT_CE                        (FIRST_SIRC_IRQ + 7)
#define INT_SYS_ENZO_IEQ              (FIRST_SIRC_IRQ + 8)
#define INT_PERPH_ENZO                (FIRST_SIRC_IRQ + 9)
#define INT_MXBAR_ENZO                (FIRST_SIRC_IRQ + 10)
#define INT_AXIAGG_ENZO               (FIRST_SIRC_IRQ + 11)
#define INT_UART3                     (FIRST_SIRC_IRQ + 12)
#define INT_UART2                     (FIRST_SIRC_IRQ + 13)
#define INT_PORT0_SSBI2               (FIRST_SIRC_IRQ + 14)
#define INT_PORT1_SSBI2               (FIRST_SIRC_IRQ + 15)
#define INT_PORT2_SSBI2               (FIRST_SIRC_IRQ + 16)
#define INT_PORT3_SSBI2               (FIRST_SIRC_IRQ + 17)
#define INT_GSBI_QUP_INBUF            (FIRST_SIRC_IRQ + 18)
#define INT_GSBI_QUP_OUTBUF           (FIRST_SIRC_IRQ + 19)
#define INT_GSBI_QUP_ERROR            (FIRST_SIRC_IRQ + 20)
#define INT_SPB_DECODER               (FIRST_SIRC_IRQ + 21)
#define INT_FPB_DEC                   (FIRST_SIRC_IRQ + 22)
#define INT_BPM_HW                    (FIRST_SIRC_IRQ + 23)
#define INT_GPIO_167                  (FIRST_SIRC_IRQ + 24)

/* Group B */
#define INT_GPIO_166                  (FIRST_SIRC_IRQ + 25)
#define INT_GPIO_165                  (FIRST_SIRC_IRQ + 26)
#define INT_GPIO_164                  (FIRST_SIRC_IRQ + 27)
#define INT_GPIO_163                  (FIRST_SIRC_IRQ + 28)
#define INT_GPIO_162                  (FIRST_SIRC_IRQ + 29)
#define INT_GPIO_161                  (FIRST_SIRC_IRQ + 30)
#define INT_GPIO_160                  (FIRST_SIRC_IRQ + 31)
#define INT_GPIO_159                  (FIRST_SIRC_IRQ + 32)
#define INT_GPIO_158                  (FIRST_SIRC_IRQ + 33)
#define INT_GPIO_157                  (FIRST_SIRC_IRQ + 34)
#define INT_GPIO_156                  (FIRST_SIRC_IRQ + 35)
#define INT_GPIO_155                  (FIRST_SIRC_IRQ + 36)
#define INT_GPIO_154                  (FIRST_SIRC_IRQ + 37)
#define INT_GPIO_153                  (FIRST_SIRC_IRQ + 38)
#define INT_GPIO_152                  (FIRST_SIRC_IRQ + 39)
#define INT_GPIO_151                  (FIRST_SIRC_IRQ + 40)
#define INT_GPIO_150                  (FIRST_SIRC_IRQ + 41)
#define INT_GPIO_149                  (FIRST_SIRC_IRQ + 42)
#define INT_GPIO_148                  (FIRST_SIRC_IRQ + 43)
#define INT_GPIO_147                  (FIRST_SIRC_IRQ + 44)
#define INT_GPIO_146                  (FIRST_SIRC_IRQ + 45)
#define INT_GPIO_145                  (FIRST_SIRC_IRQ + 46)
#define INT_GPIO_144                  (FIRST_SIRC_IRQ + 47)
/* RESERVED 48 */

#define NR_SIRC_IRQS_GROUPA           25
#define NR_SIRC_IRQS_GROUPB           24
#define NR_SIRC_IRQS                  49
#define SIRC_MASK_GROUPA              0x01ffffff
#define SIRC_MASK_GROUPB              0x00ffffff

#define SPSS_SIRC_INT_CLEAR           (MSM_SIRC_BASE + 0x00)
#define SPSS_SIRC_INT_POLARITY        (MSM_SIRC_BASE + 0x5C)
#define SPSS_SIRC_INT_SET             (MSM_SIRC_BASE + 0x18)
#define SPSS_SIRC_INT_ENABLE          (MSM_SIRC_BASE + 0x20)
#define SPSS_SIRC_IRQ_STATUS          (MSM_SIRC_BASE + 0x38)
#define SPSS_SIRC_INT_TYPE            (MSM_SIRC_BASE + 0x30)
#define SPSS_SIRC_VEC_INDEX_RD        (MSM_SIRC_BASE + 0x48)

#endif /* __ASM_ARCH_MSM_SIRC_FSM9XXX_H */
