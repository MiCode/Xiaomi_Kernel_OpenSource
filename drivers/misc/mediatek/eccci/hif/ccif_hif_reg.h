/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __CCIF_HIF_PLATFORM_H__
#define __CCIF_HIF_PLATFORM_H__

#include <linux/io.h>

#define ccif_write16(b, a, v) \
do {\
	writew(v, (b) + (a));\
	mb(); /* make sure register access in order */ \
} while (0)

#define ccif_write8(b, a, v) \
do {\
	writeb(v, (b) + (a));\
	mb(); /* make sure register access in order */ \
} while (0)

#define ccif_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccif_read8(b, a)                ioread8((void __iomem *)((b)+(a)))

int ccif_read32(void *b, unsigned long a);
void ccif_write32(void *b, unsigned long a, unsigned int v);

/*CCIF */
#define APCCIF_CON        (0x00)
#define APCCIF_BUSY       (0x04)
#define APCCIF_START      (0x08)
#define APCCIF_TCHNUM     (0x0C)
#define APCCIF_RCHNUM     (0x10)
#define APCCIF_ACK        (0x14)
#define APCCIF_IRQ0_MASK  (0x20)
#define APCCIF_IRQ1_MASK  (0x24)
#define APCCIF_CHDATA     (0x100)

#define RINGQ_BASE (0)
#define RINGQ_SRAM (15)
#define RINGQ_EXP_BASE (15)
#define CCIF_CH_NUM 24
#define AP_MD_CCB_WAKEUP (7)


/*AP to MD*/
#define H2D_EXCEPTION_ACK        (RINGQ_EXP_BASE+1)
#define H2D_EXCEPTION_CLEARQ_ACK (RINGQ_EXP_BASE+2)
#define H2D_FORCE_MD_ASSERT      (RINGQ_EXP_BASE+3)
#define H2D_MPU_FORCE_ASSERT     (RINGQ_EXP_BASE+4)

#define H2D_SRAM    (RINGQ_SRAM)
#define H2D_RINGQ0  (RINGQ_BASE+0)
#define H2D_RINGQ1  (RINGQ_BASE+1)
#define H2D_RINGQ2  (RINGQ_BASE+2)
#define H2D_RINGQ3  (RINGQ_BASE+3)
#define H2D_RINGQ4  (RINGQ_BASE+4)
#define H2D_RINGQ5  (RINGQ_BASE+5)
#define H2D_RINGQ6  (RINGQ_BASE+6)
#define H2D_RINGQ7  (RINGQ_BASE+7)

/*MD to AP*/
#define CCIF_HW_CH_RX_RESERVED \
			((1 << (RINGQ_EXP_BASE+0)) | (1 << (RINGQ_EXP_BASE+5)))
#define D2H_EXCEPTION_INIT        (RINGQ_EXP_BASE+1)
#define D2H_EXCEPTION_INIT_DONE   (RINGQ_EXP_BASE+2)
#define D2H_EXCEPTION_CLEARQ_DONE (RINGQ_EXP_BASE+3)
#define D2H_EXCEPTION_ALLQ_RESET  (RINGQ_EXP_BASE+4)
#define AP_MD_PEER_WAKEUP         (RINGQ_EXP_BASE+5)
#define AP_MD_SEQ_ERROR           (RINGQ_EXP_BASE+6)
#define AP_MD_DATA_NOTIFY         (RINGQ_EXP_BASE+8)
#define D2H_SRAM    (RINGQ_SRAM)
#define D2H_RINGQ0  (RINGQ_BASE+0)
#define D2H_RINGQ1  (RINGQ_BASE+1)
#define D2H_RINGQ2  (RINGQ_BASE+2)
#define D2H_RINGQ3  (RINGQ_BASE+3)
#define D2H_RINGQ4  (RINGQ_BASE+4)
#define D2H_RINGQ5  (RINGQ_BASE+5)
#define D2H_RINGQ6  (RINGQ_BASE+6)
#define D2H_RINGQ7  (RINGQ_BASE+7)


#define CCIF_SRAM_SIZE 512

/* use hw_channel = usr_id + AP_MD_DATA_NOTIFY */
enum ccif_isr_cb_user_id {
	ID_CCIF_USER_DATA = 0,
	ID_CCIF_CB_MAX,
};


#endif /*__CCIF_HIF_PLATFORM_H__*/
