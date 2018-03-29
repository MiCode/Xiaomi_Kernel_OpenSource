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

/*****************************************************************************
 *============================================================================
 *                               HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Log$
 *
 * 11 26 2014 michael.rong
 * [SDIO-GEN3] Pass DVT
 *
 * 10 12 2014 michael.rong
 * .
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/


/*
    Module Name:
    sdio_bus_driver.c

    Abstract:
    Provide SDIO-GEN3  based bus driver routines

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
*/
#include <linux/kernel.h>
#include <asm/io.h>
#include "debug.h"
#include "gl_os.h"
#include "sdio.h"
/* ========================== SDIO Private Routines ============================= */

/**
 *	sdio_f0_readb - read a single byte from SDIO function 0
 *	@func: an SDIO function of the card
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a single byte from the address space of SDIO function 0.
 *	If there is a problem reading the address, 0xff is returned
 *	and @err_ret will contain the error code.
 */

struct sdio_func g_sdio_func;

int sdio_open(void)
{
	struct sdio_func *func = &g_sdio_func;
	INT_32  ret = 0;

	ASSERT(MY_SDIO_BLOCK_SIZE <= 512);

	g_sdio_func.cur_blksize = MY_SDIO_BLOCK_SIZE;
	g_sdio_func.num = SDIO_GEN3_FUNCTION_WIFI;
	g_sdio_func.irq_handler = NULL;
	g_sdio_func.use_dma = 1;/* 1 for DMA mode, 0 for PIO mode */


	/* DBGLOG(INIT, INFO, "g_sdio_func=%p\n", &g_sdio_func); */
	/* function enable */
	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	sdio_release_host(func);
	if (ret)
	{
	   /* DBGLOG(INIT, TRACE, "Enable function failed. Error = %d.\n", ret); */
	   goto err;
	}

	/* set block size */
	sdio_claim_host(func);
	ret = sdio_set_block_size(func, func->cur_blksize);
	sdio_release_host(func);

	if (ret)
	{
		/* DBGLOG(INIT, TRACE, "Set block size failed. Error = %d.\n", ret); */
		goto err;
	}

	/* register sdio irq */
	sdio_claim_host(func);
	ret = sdio_claim_irq(func, NULL); /* Interrupt IRQ handler */
	sdio_release_host(func);
	if (ret)
	{
		/* DBGLOG(INIT, TRACE, "Claim irq failed. Error = %d.\n", ret); */
		goto err;
	}

err:
	return ret;
}

int sdio_cccr_read(UINT_32 addr, UINT_8 *value)
{

	INT_32 ret = 0;
	struct sdio_func *dev_func = &g_sdio_func;

	sdio_claim_host(dev_func);
	*value = sdio_f0_readb(dev_func, addr, &ret);
	sdio_release_host(dev_func);

	if (ret)
	{
		/* DBGLOG(INIT, TRACE, "Read CCCR 0x%02x failed. Error = %d\n", addr, ret); */
	}

	return ret;
}


int sdio_cccr_write(UINT_32 addr, UINT_8 value)
{
	INT_32 ret = 0;
	struct sdio_func *dev_func = &g_sdio_func;

	sdio_claim_host(dev_func);
	sdio_f0_writeb(dev_func, value, addr, &ret);
	sdio_release_host(dev_func);

	if (ret)
	{
		/* DBGLOG(INIT, TRACE, "Write register 0x%02x failed. Error = %d\n", addr, ret); */
	}

	return ret;
}


/**
 *	sdio_readl - read a 32 bit integer from a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a 32 bit integer from the address space of a given SDIO
 *	function. If there is a problem reading the address,
 *	0xffffffff is returned and @err_ret will contain the error
 *	code.
 */
UINT_32 sdio_cr_readl(volatile unsigned int *HifBaseAddr, unsigned int addr)
{
	unsigned int value = -1;
	sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;


	/* CMD53 incremental mode to read 4-byte */
	/* 1. Setup command information */
	info.word = 0;
	info.field.rw_flag = SDIO_GEN3_READ;
	info.field.func_num = func->num;/* SDIO_GEN3_FUNCTION_WIFI */
	info.field.block_mode = SDIO_GEN3_BYTE_MODE;
	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE;
	info.field.addr = addr;
	info.field.count = 4;

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(SDIO_GEN3_CMD_SETUP + (UINT_8 *)HifBaseAddr));
	value = readl((volatile UINT_32 *)(SDIO_GEN3_CMD53_DATA + (UINT_8 *)HifBaseAddr));

	__enable_irq();
	my_sdio_enable(HifLock);

	return value;
}


/**
 *	sdio_writel - write a 32 bit integer to a SDIO function
 *	@func: SDIO function to access
 *	@b: integer to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a 32 bit integer to the address space of a given SDIO
 *	function. @err_ret will contain the status of the actual
 *	transfer.
 */

void sdio_cr_writel(UINT_32 b, volatile unsigned int *HifBaseAddr, unsigned int addr)
{
    sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;


    /* CMD53 incremental mode to read 4-byte */
    /* 1. Setup command information */
	info.word = 0;
	info.field.rw_flag = SDIO_GEN3_WRITE;
	info.field.func_num = func->num; /* SDIO_GEN3_FUNCTION_WIFI */
	info.field.block_mode = SDIO_GEN3_BYTE_MODE;
	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE;
	info.field.addr = addr;
	info.field.count = 4;

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(SDIO_GEN3_CMD_SETUP + (UINT_8 *)HifBaseAddr));
	writel(b, (volatile UINT_32 *)(SDIO_GEN3_CMD53_DATA + (UINT_8 *)HifBaseAddr));

	__enable_irq();
	my_sdio_enable(HifLock);

}


unsigned char ahb_sdio_f0_readb(struct sdio_func *func, unsigned int addr,
	int *err_ret)
{
	unsigned char val;
    sdio_gen3_cmd52_info info;
    info.word = 0;
    /* CMD52 read 1-byte of func0 */

	if (err_ret)
		*err_ret = 0;

    /* 1. Setup command information */
    info.field.rw_flag = SDIO_GEN3_READ;
    info.field.func_num = 0;
    info.field.addr = addr;

	my_sdio_disable(HifLock);
	__disable_irq();
	writel(info.word, (volatile UINT_32 *)(SDIO_GEN3_CMD_SETUP + *g_pHifRegBaseAddr));
	val = readl((volatile UINT_32 *)(SDIO_GEN3_CMD52_DATA + *g_pHifRegBaseAddr));
	__enable_irq();
	my_sdio_enable(HifLock);

	return val;
}

/**
 *	sdio_f0_writeb - write a single byte to SDIO function 0
 *	@func: an SDIO function of the card
 *	@b: byte to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a single byte to the address space of SDIO function 0.
 *	@err_ret will contain the status of the actual transfer.
 *
 *	Only writes to the vendor specific CCCR registers (0xF0 -
 *	0xFF) are permiited; @err_ret will be set to -EINVAL for *
 *	writes outside this range.
 */
void ahb_sdio_f0_writeb(struct sdio_func *func, unsigned char b, unsigned int addr,
	int *err_ret)
{
    sdio_gen3_cmd52_info info;
    info.word = 0;
    /* CMD52 write 1-byte of func0 */

	if (err_ret)
		*err_ret = 0;

    /* 1. Setup command information */
	info.field.rw_flag = SDIO_GEN3_WRITE;
	info.field.func_num = 0;
	info.field.addr = addr;
	info.field.data = b;

	my_sdio_disable(HifLock);
	__disable_irq();
	writel(info.word, (volatile UINT_32 *)(SDIO_GEN3_CMD_SETUP + *g_pHifRegBaseAddr));
	writel(b, (volatile UINT_32 *)(SDIO_GEN3_CMD52_DATA + *g_pHifRegBaseAddr));
	__enable_irq();
	my_sdio_enable(HifLock);

}


/**
 *	sdio_enable_func - enables a SDIO function for usage
 *	@func: SDIO function to enable
 *
 *	Powers up and activates a SDIO function so that register
 *	access is possible.
 */
int ahb_sdio_enable_func(struct sdio_func *func)
{
	int ret;
	unsigned char reg;

	/* DBGLOG(INIT, TRACE, "SDIO: Enabling Function %d...\n", func->num); */

    reg = sdio_f0_readb(func, SDIO_CCCR_IOEx, &ret);
	if (ret)
		goto err;
    /* DBGLOG(INIT, TRACE, "Origin Func enable=0x%x\n", reg); */

	reg |= 1 << func->num;
    sdio_f0_writeb(func, reg, SDIO_CCCR_IOEx, &ret);
	if (ret)
		goto err;

    reg = sdio_f0_readb(func, SDIO_CCCR_IORx, &ret);
	if (ret)
		goto err;
    /* DBGLOG(INIT, TRACE, "Read CCCR_IORx=0x%x\n", reg); */
	if (!(reg & (1 << func->num))) {
		ret = -ETIME;
		goto err;
	}
	/* DBGLOG(INIT, TRACE, "SDIO: Enabled Function %d\n", func->num); */

	return 0;

err:
	/* DBGLOG(INIT, TRACE, "SDIO: Failed to enable Function %d\n", func->num); */
	return ret;
}
/**
 *	sdio_disable_func - disable a SDIO function
 *	@func: SDIO function to disable
 *
 *	Powers down and deactivates a SDIO function. Register access
 *	to this function will fail until the function is reenabled.
 */
int ahb_sdio_disable_func(struct sdio_func *func)
{
    int ret;
    unsigned char reg;

    /* DBGLOG(INIT, TRACE, "SDIO: Disabling Function %d...\n", func->num); */

    reg = sdio_f0_readb(func, SDIO_CCCR_IOEx, &ret);
    if (ret)
        goto err;

	reg &= ~(1 << func->num);


    sdio_f0_writeb(func, reg, SDIO_CCCR_IOEx, &ret);
    if (ret)
        goto err;

    /* DBGLOG(INIT, TRACE, "SDIO: Disabled Function %d\n", func->num); */

    return 0;

err:
    ret = -EIO;
    /* DBGLOG(INIT, TRACE, "SDIO: Failed to Disable Function %d\n", func->num); */
    return ret;

}

/**
 *	sdio_set_block_size - set the block size of an SDIO function
 *	@func: SDIO function to change
 *	@blksz: new block size or 0 to use the default.
 *
 *	The default block size is the largest supported by both the function
 *	and the host, with a maximum of 512 to ensure that arbitrarily sized
 *	data transfer use the optimal (least) number of commands.
 *
 *	A driver may call this to override the default block size set by the
 *	core. This can be used to set a block size greater than the maximum
 *	that reported by the card; it is the driver's responsibility to ensure
 *	it uses a value that the card supports.
 *
 *	Returns 0 on success, -EINVAL if the host does not support the
 *	requested block size, or -EIO (etc.) if one of the resultant FBR block
 *	size register writes failed.
 *
 */
int ahb_sdio_set_block_size(struct sdio_func *func, unsigned blksz)
{
	int ret;

	sdio_f0_writeb(func, (blksz & 0xff),
		SDIO_FBR_BASE(func->num) + SDIO_FBR_BLKSIZE, &ret);

	if (ret)
		return ret;

	sdio_f0_writeb(func, ((blksz >> 8) & 0xff),
		SDIO_FBR_BASE(func->num) + SDIO_FBR_BLKSIZE + 1, &ret);

	if (ret)
		return ret;
	func->cur_blksize = blksz;
	return 0;
}

/**
 *	sdio_claim_irq - claim the IRQ for a SDIO function
 *	@func: SDIO function
 *	@handler: IRQ handler callback
 *
 *	Claim and activate the IRQ for the given SDIO function. The provided
 *	handler will be called when that IRQ is asserted.  The host is always
 *	claimed already when the handler is called so the handler must not
 *	call sdio_claim_host() nor sdio_release_host().
 */
int ahb_sdio_claim_irq(struct sdio_func *func, sdio_irq_handler_t *handler)
{
	int ret;
	unsigned char reg=0;

	/* DBGLOG(INIT, TRACE, "SDIO: Enabling IRQ for func%d...\n", func->num); */

	if (func->irq_handler) {
		/* DBGLOG(INIT, TRACE, "SDIO: IRQ for func%d already in use.\n", func->num); */
		return -2;
	}


    reg = sdio_f0_readb(func,SDIO_CCCR_IENx, &ret);
	if (ret)
		return ret;

	reg |= 1 << func->num;

	reg |= 1; /* Master interrupt enable */
    /* DBGLOG(INIT, TRACE, "Write IENx=0x%x\n", reg); */

    sdio_f0_writeb(func, reg, SDIO_CCCR_IENx, &ret);
	/* set CCCR I/O Enable, will trigger CONNSYS HGFISR bit2 HIF_HGFISR_DRV_SET_WLAN_IOE */
	if (ret)
		return ret;

	func->irq_handler = handler;

    reg = sdio_f0_readb(func,SDIO_CCCR_IENx, &ret);
	if (ret)
		return ret;
    /* DBGLOG(INIT, TRACE, "===> IENx=0x%x\n", reg); */
	return ret;
}

/**
 *	sdio_release_irq - release the IRQ for a SDIO function
 *	@func: SDIO function
 *
 *	Disable and release the IRQ for the given SDIO function.
 */
int ahb_sdio_release_irq(struct sdio_func *func)
{
	int ret;
	unsigned char reg = 0;

	/* DBGLOG(INIT, TRACE, "SDIO: Disabling IRQ for func%d...\n", func->num); */

	if (func->irq_handler) {
		func->irq_handler = NULL;
	}

    sdio_f0_readb(func, SDIO_CCCR_IENx, &ret);
	if (ret)
		return ret;

	reg &= ~(1 << func->num);

	/* Disable master interrupt with the last function interrupt */
	if (!(reg & 0xFE))
		reg = 0;

    sdio_f0_writeb(func, reg, SDIO_CCCR_IENx, &ret);
	if (ret)
		return ret;

	return 0;
}

