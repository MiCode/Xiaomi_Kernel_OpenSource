/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MMC_SDIO_H
#define MMC_SDIO_H

/*  =======================  PART 1: ahb_sdioLike operation ======================== */

#include "gl_typedef.h"
#include "mt6630_reg.h"

#define SDIO_GEN3_BASE          (0x0)
#define SDIO_GEN3_CMD_SETUP     (SDIO_GEN3_BASE+0x0)
#define SDIO_GEN3_CMD3_DATA     (SDIO_GEN3_BASE+0x8)
#define SDIO_GEN3_CMD5_DATA     (SDIO_GEN3_BASE+0xC)
#define SDIO_GEN3_CMD7_DATA     (SDIO_GEN3_BASE+0x10)
#define SDIO_GEN3_CMD11_DATA    (SDIO_GEN3_BASE+0x14)
#define SDIO_GEN3_CMD52_DATA    (SDIO_GEN3_BASE+0x4)
#define SDIO_GEN3_CMD53_DATA    (SDIO_GEN3_BASE+0x1000)

enum SDIO_GEN3_RW_TYPE {
	SDIO_GEN3_READ,
	SDIO_GEN3_WRITE
};
enum SDIO_GEN3_TRANS_MODE {
	SDIO_GEN3_BYTE_MODE,
	SDIO_GEN3_BLOCK_MODE
};
enum SDIO_GEN3_OP_MODE {
	SDIO_GEN3_FIXED_PORT_MODE,
	SDIO_GEN3_INCREMENT_MODE
};

enum SDIO_GEN3_FUNCTION {
	SDIO_GEN3_FUNCTION_0,
	SDIO_GEN3_FUNCTION_WIFI,
	SDIO_GEN3_FUNCTION_BT,
};

typedef struct _sdio_irq_info_tag {
	BOOLEAN   irq_assert;
	ENHANCE_MODE_DATA_STRUCT_T  irq_data;
} sdio_irq_info_t;

typedef struct _sdio_intr_enhance_arg_tag {
	unsigned char rxNum;
	unsigned char totalBytes;
} sdio_intr_enhance_arg_t;

typedef union _sdio_gen3_cmd52_info {
	struct{
		UINT32 data : 8;             /* data for write, dummy for read */
		UINT32 reserved_8:1;         /* stuff */
		UINT32 addr : 17;            /* register address */
		UINT32 reserved_26_27 : 2;    /* raw flag / stuff */
		UINT32 func_num : 3;          /* function number */
		UINT32 rw_flag : 1;           /* read / write flag */
	} field;
	UINT32 word;
} sdio_gen3_cmd52_info;

typedef union _sdio_gen3_cmd53_info {
	struct {
		UINT32 count : 9;            /* block count for block mode, byte count for byte mode  */
		UINT32 addr : 17;            /* register address */
		UINT32 op_mode :1;            /* 1 for increment mode, 0 for port mode */
		UINT32 block_mode : 1;        /* 1 for block mode, 0 for byte mode */
		UINT32 func_num : 3;          /* function number */
		UINT32 rw_flag : 1;           /* read / write flag */
	} field;
	UINT32 word;
} sdio_gen3_cmd53_info;

struct sdio_func;
extern struct sdio_func g_sdio_func;
extern spinlock_t HifLock;
extern spinlock_t HifSdioLock;
extern int sdioDisableRefCnt;
typedef void (sdio_irq_handler_t)(struct sdio_func *);

struct sdio_func {
	sdio_irq_handler_t	*irq_handler;	/* IRQ callback */
	sdio_irq_info_t     irq_info;
	unsigned int		num;		/* function number */
	unsigned		cur_blksize;	/* current block size */
	unsigned        use_dma;
};

#define LEN_SDIO_TX_TERMINATOR  4 /*HW design spec*/
#define LEN_SDIO_RX_TERMINATOR  4

#define SDIO_HOST_REGISTER_VALUE_MAX    0x014C

#define LEN_SDIO_TX_AGG_WRAPPER(len)    ALIGN_4BYTE((len) + LEN_SDIO_TX_TERMINATOR)


/*===================  Function Declaration =====================*/

int sdio_cccr_read(UINT32 addr, UINT_8 *value);
int sdio_cccr_write(UINT32 addr, UINT_8 value);
int sdio_cr_read(UINT32 addr, UINT32 *value);
int sdio_cr_write(UINT32 addr, UINT32 value);

UINT_32 sdio_cr_readl(volatile UINT_8 *prHifBaseAddr, UINT_32 addr);
VOID sdio_cr_writel(UINT_32 value, volatile UINT_8 *prHifBaseAddr, UINT_32 addr);

int sdio_open(void);
int sdio_close(void);


unsigned char ahb_sdio_f0_readb(struct sdio_func *func, unsigned int addr, int *err_ret);
void ahb_sdio_f0_writeb(struct sdio_func *func, unsigned char b, unsigned int addr, int *err_ret);
int ahb_sdio_enable_func(struct sdio_func *func);
int ahb_sdio_disable_func(struct sdio_func *func);
int ahb_sdio_set_block_size(struct sdio_func *func, unsigned blksz);
int ahb_sdio_claim_irq(struct sdio_func *func, sdio_irq_handler_t *handler);

#define sdio_f0_readb(func, addr, err_ret) ahb_sdio_f0_readb((func), (addr), (err_ret))
#define sdio_f0_writeb(func, b, addr, err_ret) ahb_sdio_f0_writeb((func), (b), (addr), (err_ret))
#define sdio_enable_func(func) ahb_sdio_enable_func((func))
#define sdio_disable_func(func) ahb_sdio_disable_func((func))
#define sdio_set_block_size(func, blksz) ahb_sdio_set_block_size((func), (blksz))
#define sdio_claim_irq(func, handler) ahb_sdio_claim_irq((func), (handler))

#define sdio_claim_host(__func)
#define sdio_release_host(__func)

#define MY_SDIO_BLOCK_SIZE  512 /* it must be less than or eaqual to 512 */

extern UINT_8 **g_pHifRegBaseAddr;

#define __disable_irq()						\
{										\
	unsigned long ulFlags;\
\
	spin_lock_irqsave(&HifSdioLock, ulFlags); \
	if (!sdioDisableRefCnt) \
		writel(0x01, (UINT_32 *)(*g_pHifRegBaseAddr + 0x200));\
	sdioDisableRefCnt++; \
	spin_unlock_irqrestore(&HifSdioLock, ulFlags); \
}
#define __enable_irq()						\
{											\
	unsigned long ulFlags;\
\
	spin_lock_irqsave(&HifSdioLock, ulFlags); \
	if (sdioDisableRefCnt == 1) { \
		sdioDisableRefCnt = 0; \
		writel(0, (UINT_32 *)(*g_pHifRegBaseAddr + 0x200));\
	} else if (sdioDisableRefCnt > 0) \
		sdioDisableRefCnt--; \
	spin_unlock_irqrestore(&HifSdioLock, ulFlags); \
}

/*  ===========================  PART 2: mmc/sdio.h ============================ */
/* Following are from include/linux/mmc/sdio.h */


/* SDIO commands                         type  argument     response */
#define SD_IO_SEND_OP_COND          5 /* bcr  [23:0] OCR         R4  */
#define SD_IO_RW_DIRECT            52 /* ac   [31:0] See below   R5  */
#define SD_IO_RW_EXTENDED          53 /* adtc [31:0] See below   R5  */

/*
 * SD_IO_RW_DIRECT argument format:
 *
 *      [31] R/W flag
 *      [30:28] Function number
 *      [27] RAW flag
 *      [25:9] Register address
 *      [7:0] Data
 */

/*
 * SD_IO_RW_EXTENDED argument format:
 *
 *      [31] R/W flag
 *      [30:28] Function number
 *      [27] Block mode
 *      [26] Increment address
 *      [25:9] Register address
 *      [8:0] Byte/block count
 */

#define R4_MEMORY_PRESENT (1 << 27)

/*
 * SDIO status in R5
 * Type
 *	e : error bit
 *	s : status bit
 *	r : detected and set for the actual command response
 *	x : detected and set during command execution. the host must poll
 *		the card by sending status command in order to read these bits.
 * Clear condition
 *	a : according to the card state
 *	b : always related to the previous command. Reception of
 *		a valid command will clear it (with a delay of one command)
 *	c : clear by read
 */

#define R5_COM_CRC_ERROR	(1 << 15)	/* er, b */
#define R5_ILLEGAL_COMMAND	(1 << 14)	/* er, b */
#define R5_ERROR		(1 << 11)	/* erx, c */
#define R5_FUNCTION_NUMBER	(1 << 9)	/* er, c */
#define R5_OUT_OF_RANGE		(1 << 8)	/* er, c */
#define R5_STATUS(x)		(x & 0xCB00)
#define R5_IO_CURRENT_STATE(x)	((x & 0x3000) >> 12) /* s, b */

/*
 * Card Common Control Registers (CCCR)
 */

#define SDIO_CCCR_CCCR		0x00

#define  SDIO_CCCR_REV_1_00	0	/* CCCR/FBR Version 1.00 */
#define  SDIO_CCCR_REV_1_10	1	/* CCCR/FBR Version 1.10 */
#define  SDIO_CCCR_REV_1_20	2	/* CCCR/FBR Version 1.20 */

#define  SDIO_SDIO_REV_1_00	0	/* SDIO Spec Version 1.00 */
#define  SDIO_SDIO_REV_1_10	1	/* SDIO Spec Version 1.10 */
#define  SDIO_SDIO_REV_1_20	2	/* SDIO Spec Version 1.20 */
#define  SDIO_SDIO_REV_2_00	3	/* SDIO Spec Version 2.00 */

#define SDIO_CCCR_SD		0x01

#define  SDIO_SD_REV_1_01	0	/* SD Physical Spec Version 1.01 */
#define  SDIO_SD_REV_1_10	1	/* SD Physical Spec Version 1.10 */
#define  SDIO_SD_REV_2_00	2	/* SD Physical Spec Version 2.00 */

#define SDIO_CCCR_IOEx		0x02
#define SDIO_CCCR_IORx		0x03

#define SDIO_CCCR_IENx		0x04	/* Function/Master Interrupt Enable */
#define SDIO_CCCR_INTx		0x05	/* Function Interrupt Pending */

#define SDIO_CCCR_ABORT		0x06	/* function abort/card reset */

#define SDIO_CCCR_IF		0x07	/* bus interface controls */

#define  SDIO_BUS_WIDTH_1BIT	0x00
#define  SDIO_BUS_WIDTH_4BIT	0x02
#define  SDIO_BUS_ECSI		0x20	/* Enable continuous SPI interrupt */
#define  SDIO_BUS_SCSI		0x40	/* Support continuous SPI interrupt */

#define  SDIO_BUS_ASYNC_INT	0x20

#define  SDIO_BUS_CD_DISABLE     0x80	/* disable pull-up on DAT3 (pin 1) */

#define SDIO_CCCR_CAPS		0x08

#define  SDIO_CCCR_CAP_SDC	0x01	/* can do CMD52 while data transfer */
#define  SDIO_CCCR_CAP_SMB	0x02	/* can do multi-block xfers (CMD53) */
#define  SDIO_CCCR_CAP_SRW	0x04	/* supports read-wait protocol */
#define  SDIO_CCCR_CAP_SBS	0x08	/* supports suspend/resume */
#define  SDIO_CCCR_CAP_S4MI	0x10	/* interrupt during 4-bit CMD53 */
#define  SDIO_CCCR_CAP_E4MI	0x20	/* enable ints during 4-bit CMD53 */
#define  SDIO_CCCR_CAP_LSC	0x40	/* low speed card */
#define  SDIO_CCCR_CAP_4BLS	0x80	/* 4 bit low speed card */

#define SDIO_CCCR_CIS		0x09	/* common CIS pointer (3 bytes) */

/* Following 4 regs are valid only if SBS is set */
#define SDIO_CCCR_SUSPEND	0x0c
#define SDIO_CCCR_SELx		0x0d
#define SDIO_CCCR_EXECx		0x0e
#define SDIO_CCCR_READYx	0x0f

#define SDIO_CCCR_BLKSIZE	0x10

#define SDIO_CCCR_POWER		0x12

#define  SDIO_POWER_SMPC	0x01	/* Supports Master Power Control */
#define  SDIO_POWER_EMPC	0x02	/* Enable Master Power Control */

#define SDIO_CCCR_SPEED		0x13

#define  SDIO_SPEED_SHS		0x01	/* Supports High-Speed mode */
#define  SDIO_SPEED_EHS		0x02	/* Enable High-Speed mode */

/*
 * Function Basic Registers (FBR)
 */

#define SDIO_FBR_BASE(f)	((f) * 0x100) /* base of function f's FBRs */

#define SDIO_FBR_STD_IF		0x00

#define  SDIO_FBR_SUPPORTS_CSA	0x40	/* supports Code Storage Area */
#define  SDIO_FBR_ENABLE_CSA	0x80	/* enable Code Storage Area */

#define SDIO_FBR_STD_IF_EXT	0x01

#define SDIO_FBR_POWER		0x02

#define  SDIO_FBR_POWER_SPS	0x01	/* Supports Power Selection */
#define  SDIO_FBR_POWER_EPS	0x02	/* Enable (low) Power Selection */

#define SDIO_FBR_CIS		0x09	/* CIS pointer (3 bytes) */


#define SDIO_FBR_CSA		0x0C	/* CSA pointer (3 bytes) */

#define SDIO_FBR_CSA_DATA	0x0F

#define SDIO_FBR_BLKSIZE	0x10	/* block size (2 bytes) */

#endif
