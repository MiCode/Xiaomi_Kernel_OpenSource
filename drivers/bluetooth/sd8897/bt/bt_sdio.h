/** @file bt_sdio.h
 *  @brief This file contains SDIO (interface) module
 *  related macros, enum, and structure.
 *
 *  Copyright (C) 2007-2012, Marvell International Ltd.
 *
 *  This software file (the "File") is distributed by Marvell International
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991
 *  (the "License").  You may use, redistribute and/or modify this File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available along with the File in the gpl.txt file or by writing to
 *  the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
 *
 */

#ifndef _BT_SDIO_H_
#define _BT_SDIO_H_

#include <linux/irqreturn.h>

/** IRQ return type */
typedef irqreturn_t IRQ_RET_TYPE;
/** IRQ return */
#define IRQ_RET		return IRQ_HANDLED
/** ISR notifier function */
typedef IRQ_RET_TYPE(*isr_notifier_fn_t) (s32 irq, void *dev_id,
					  struct pt_regs * reg);

/** SDIO header length */
#define SDIO_HEADER_LEN			4

/* SD block size can not bigger than 64 due to buf size limit in firmware */
/** define SD block size for data Tx/Rx */
#define SD_BLOCK_SIZE			256
/** define SD block size for firmware download */
#define SD_BLOCK_SIZE_FW_DL		256

/** Number of blocks for firmware transfer */
#define FIRMWARE_TRANSFER_NBLOCK	2

/** Firmware ready */
#define FIRMWARE_READY			0xfedc

/* Bus Interface Control Reg 0x07 */
/** SD BUS width 1 */
#define SD_BUS_WIDTH_1			0x00
/** SD BUS width 4 */
#define SD_BUS_WIDTH_4			0x02
/** SD BUS width mask */
#define SD_BUS_WIDTH_MASK		0x03
/** Asynchronous interrupt mode */
#define ASYNC_INT_MODE			0x20
/* Host Control Registers */
/** Host Control Registers : Configuration */
#define CONFIGURATION_REG		0x00
/** Host Control Registers : Host without Command 53 finish host*/
#define HOST_TO_CARD_EVENT       (0x1U << 3)
/** Host Control Registers : Host without Command 53 finish host */
#define HOST_WO_CMD53_FINISH_HOST	(0x1U << 2)
/** Host Control Registers : Host power up */
#define HOST_POWER_UP			(0x1U << 1)
/** Host Control Registers : Host power down */
#define HOST_POWER_DOWN			(0x1U << 0)

/** Host Control Registers : Host interrupt RSR */
#define HOST_INT_RSR_REG		0x01
/** Host Control Registers : Upload host interrupt RSR */
#define UP_LD_HOST_INT_RSR		(0x1U)

/** Host Control Registers : Host interrupt mask */
#define HOST_INT_MASK_REG		0x02
/** Host Control Registers : Upload host interrupt mask */
#define UP_LD_HOST_INT_MASK		(0x1U)
/** Host Control Registers : Download host interrupt mask */
#define DN_LD_HOST_INT_MASK		(0x2U)
/** Enable Host interrupt mask */
#define HIM_ENABLE			(UP_LD_HOST_INT_MASK | DN_LD_HOST_INT_MASK)
/** Disable Host interrupt mask */
#define	HIM_DISABLE			0xff

/** Host Control Registers : Host interrupt status */
#define HOST_INTSTATUS_REG		0x03
/** Host Control Registers : Upload host interrupt status */
#define UP_LD_HOST_INT_STATUS		(0x1U)
/** Host Control Registers : Download host interrupt status */
#define DN_LD_HOST_INT_STATUS		(0x2U)

/** Host Control Registers : Host Transfer status */
#define HOST_INT_STATUS_REG		0x4C
/** Host Control Registers : Upload CRC error */
#define UP_LD_CRC_ERR			(0x1U << 2)
/** Host Control Registers : Upload restart */
#define UP_LD_RESTART			(0x1U << 1)
/** Host Control Registers : Download restart */
#define DN_LD_RESTART			(0x1U << 0)

/** Card Control Registers : Card to Host Event register */
#define CARD_STATUS_REG			0x50
/** Card Control Registers : Card I/O ready */
#define CARD_IO_READY			(0x1U << 3)
/** Card Control Registers : CIS card ready */
#define CIS_CARD_RDY			(0x1U << 2)
/** Card Control Registers : Upload card ready */
#define UP_LD_CARD_RDY			(0x1U << 1)
/** Card Control Registers : Download card ready */
#define DN_LD_CARD_RDY			(0x1U << 0)

/** Card Control Registers : Host interrupt mask register */
#define HOST_INTERRUPT_MASK_REG 	0x54
/** Card Control Registers : Host power interrupt mask */
#define HOST_POWER_INT_MASK		(0x1U << 3)
/** Card Control Registers : Abort card interrupt mask */
#define ABORT_CARD_INT_MASK		(0x1U << 2)
/** Card Control Registers : Upload card interrupt mask */
#define UP_LD_CARD_INT_MASK          	(0x1U << 1)
/** Card Control Registers : Download card interrupt mask */
#define DN_LD_CARD_INT_MASK          	(0x1U << 0)

/** Card Control Registers : Card interrupt status register */
#define CARD_INTERRUPT_STATUS_REG    	0x58
/** Card Control Registers : Power up interrupt */
#define POWER_UP_INT			(0x1U << 4)
/** Card Control Registers : Power down interrupt */
#define POWER_DOWN_INT               	(0x1U << 3)

/** Card Control Registers : Card interrupt RSR register */
#define CARD_INTERRUPT_RSR_REG       	0x5c
/** Card Control Registers : Power up RSR */
#define POWER_UP_RSR                 	(0x1U << 4)
/** Card Control Registers : Power down RSR */
#define POWER_DOWN_RSR               	(0x1U << 3)

/* Card Control Registers */
/** Card Control Registers : Read SQ base address A0 register */
#define SQ_READ_BASE_ADDRESS_A0_REG  	0x60
/** Card Control Registers : Read SQ base address A1 register */
#define SQ_READ_BASE_ADDRESS_A1_REG  	0x61
/** Card Control Registers : Read SQ base address A2 register */
#define SQ_READ_BASE_ADDRESS_A2_REG  	0x62
/** Card Control Registers : Read SQ base address A3 register */
#define SQ_READ_BASE_ADDRESS_A3_REG  	0x63
/** Card Control Registers : Write SQ base address A0 register */
#define SQ_WRITE_BASE_ADDRESS_A0_REG  	0x64
/** Card Control Registers : Write SQ base address A1 register */
#define SQ_WRITE_BASE_ADDRESS_A1_REG  	0x65
/** Card Control Registers : Write SQ base address A2 register */
#define SQ_WRITE_BASE_ADDRESS_A2_REG  	0x66
/** Card Control Registers : Write SQ base address A3 register */
#define SQ_WRITE_BASE_ADDRESS_A3_REG  	0x67

/** Card Control Registers : Card revision register */
#define CARD_REVISION_REG            	0xBC

/** Firmware status 0 register (SCRATCH0_0) */
#define CARD_FW_STATUS0_REG		0xC0
/** Firmware status 1 register (SCRATCH0_1) */
#define CARD_FW_STATUS1_REG		0xC1
/** Rx length register (SCRATCH0_2) */
#define CARD_RX_LEN_REG			0xC2
/** Rx unit register (SCRATCH0_3) */
#define CARD_RX_UNIT_REG		0xC3

/** Card Control Registers : Card OCR 0 register */
#define CARD_OCR_0_REG			0xC8
/** Card Control Registers : Card OCR 1 register */
#define CARD_OCR_1_REG			0xC9
/** Card Control Registers : Card OCR 3 register */
#define CARD_OCR_3_REG			0xCA
/** Card Control Registers : Card config register */
#define CARD_CONFIG_REG			0xCB
/** Card Control Registers : Miscellaneous Configuration Register */
#define CARD_MISC_CFG_REG		0xCC
/** Misc. Config Register : Auto Re-enable interrupts */
#define AUTO_RE_ENABLE_INT		(0x1U << 4)

/** Card Control Registers : Debug 0 register */
#define DEBUG_0_REG			0xD0
/** Card Control Registers : SD test BUS 0 */
#define SD_TESTBUS0			(0x1U)
/** Card Control Registers : Debug 1 register */
#define DEBUG_1_REG			0xD1
/** Card Control Registers : SD test BUS 1 */
#define SD_TESTBUS1			(0x1U)
/** Card Control Registers : Debug 2 register */
#define DEBUG_2_REG			0xD2
/** Card Control Registers : SD test BUS 2 */
#define SD_TESTBUS2			(0x1U)
/** Card Control Registers : Debug 3 register */
#define DEBUG_3_REG			0xD3
/** Card Control Registers : SD test BUS 3 */
#define SD_TESTBUS3			(0x1U)

/** Host Control Registers : I/O port 0 */
#define IO_PORT_0_REG			0xD8
/** Host Control Registers : I/O port 1 */
#define IO_PORT_1_REG			0xD9
/** Host Control Registers : I/O port 2 */
#define IO_PORT_2_REG			0xDA

struct sdio_mmc_card
{
	/** sdio_func structure pointer */
	struct sdio_func *func;
	/** bt_private structure pointer */
	bt_private *priv;
};
/** DMA alignment value */
#define DMA_ALIGNMENT	64
/** Macros for Data Alignment : size */
#define ALIGN_SZ(p, a)	\
	(((p) + ((a) - 1)) & ~((a) - 1))

/** Macros for Data Alignment : address */
#define ALIGN_ADDR(p, a)	\
	((((t_ptr)(p)) + (((t_ptr)(a)) - 1)) & ~(((t_ptr)(a)) - 1))

/** This function reads the Cmd52 value in dev structure */
int sd_read_cmd52_val(bt_private * priv);
/** This function updates card reg based on the Cmd52 value in dev structure */
int sd_write_cmd52_val(bt_private * priv, int func, int reg, int val);

#ifdef SDIO_SUSPEND_RESUME
#ifdef MMC_PM_KEEP_POWER
#ifdef MMC_PM_FUNC_SUSPENDED
/** This function tells lower driver that BT is suspended */
void bt_is_suspended(bt_private * priv);
#endif
#endif
#endif
#endif /* _BT_SDIO_H_ */
