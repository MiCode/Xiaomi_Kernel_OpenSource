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

/*
 * Module Name:
 * sdio_bus_driver.c
 *
 * Abstract:
 * Provide SDIO-GEN3  based bus driver routines
 *
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include "gl_os.h"
#include "debug.h"
#include "sdio.h"

/* ========================== SDIO Private Routines ============================= */

struct sdio_func g_sdio_func;

int sdio_open(void)
{
	struct sdio_func *func = &g_sdio_func;
	INT_32  ret = 0;

	func->num = SDIO_GEN3_FUNCTION_WIFI;
	func->use_dma = 1;/* 1 for DMA mode, 0 for PIO mode */

	/* function enable */
	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	sdio_release_host(func);
	if (ret) {
		DBGLOG(HAL, ERROR, "Enable function failed, error %d\n", ret);
		goto err;
	}

	/* set block size */
	sdio_claim_host(func);
	ret = sdio_set_block_size(func, MY_SDIO_BLOCK_SIZE);
	sdio_release_host(func);
	if (ret) {
		DBGLOG(HAL, ERROR, "Set block size failed, error %d\n", ret);
		goto err;
	}

	/* register sdio irq */
	sdio_claim_host(func);
	ret = sdio_claim_irq(func, NULL); /* Interrupt IRQ handler */
	sdio_release_host(func);
	if (ret) {
		DBGLOG(HAL, ERROR, "Claim irq failed, error %d\n", ret);
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

	return ret;
}


int sdio_cccr_write(UINT_32 addr, UINT_8 value)
{
	INT_32 ret = 0;
	struct sdio_func *dev_func = &g_sdio_func;

	sdio_claim_host(dev_func);
	sdio_f0_writeb(dev_func, value, addr, &ret);
	sdio_release_host(dev_func);

	return ret;
}


/**
 *	sdio_cr_readl - read a 32 bit integer from HIF controller driver domain register
 *	@prHifBaseAddr: HIF host address virtual base
 *	@addr: HIF controller address to read
 *
 *	Reads a 32 bit integer from the address space of HIF controller
 *	driver domain register in SDIO function via CMD53.
 */
UINT_32 sdio_cr_readl(volatile UINT_8 *prHifBaseAddr, UINT_32 addr)
{
	UINT_32 value;
	sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;

	/* 1. Setup command information */
	info.word = 0;
	info.field.rw_flag = SDIO_GEN3_READ;
	info.field.func_num = func->num;
	info.field.block_mode = SDIO_GEN3_BYTE_MODE;
	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE; /* fix mode to read 4-byte CR */
	info.field.addr = addr;
	info.field.count = 4;

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(prHifBaseAddr + SDIO_GEN3_CMD_SETUP));

	/* 2. Read CMD53 port to retrieve SDIO function CR value */
	value = readl((volatile UINT_32 *)(prHifBaseAddr + SDIO_GEN3_CMD53_DATA));

	__enable_irq();
	my_sdio_enable(HifLock);

	DBGLOG(HAL, TRACE, "readl f%d 0x%08x = 0x%08x\n", func->num, addr, value);
	return value;
}

/**
 *	sdio_cr_writel - write a 32 bit integer to HIF controller driver domain register
 *	@value: integer to write
 *	@prHifBaseAddr: HIF host address virtual base
 *	@addr: HIF controller address to write to
 *
 *	Writes a 32 bit integer to the address space of HIF controller
 *	driver domain register in SDIO function via CMD53.
 */
VOID sdio_cr_writel(UINT_32 value, volatile UINT_8 *prHifBaseAddr, UINT_32 addr)
{
	sdio_gen3_cmd53_info info;
	struct sdio_func *func = &g_sdio_func;

	/* 1. Setup command information */
	info.word = 0;
	info.field.rw_flag = SDIO_GEN3_WRITE;
	info.field.func_num = func->num;
	info.field.block_mode = SDIO_GEN3_BYTE_MODE;
	info.field.op_mode = SDIO_GEN3_FIXED_PORT_MODE; /* fix mode to write 4-byte CR */
	info.field.addr = addr;
	info.field.count = 4;

	my_sdio_disable(HifLock);
	__disable_irq();

	writel(info.word, (volatile UINT_32 *)(prHifBaseAddr + SDIO_GEN3_CMD_SETUP));

	/* 2. Write CMD53 port to set SDIO function CR value */
	writel(value, (volatile UINT_32 *)(prHifBaseAddr + SDIO_GEN3_CMD53_DATA));

	__enable_irq();
	my_sdio_enable(HifLock);

	DBGLOG(HAL, TRACE, "writel f%d 0x%08x = 0x%08x\n", func->num, addr, value);
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

	DBGLOG(HAL, TRACE, "readb f0 0x%08x = 0x%02x\n", addr, val);
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

	DBGLOG(HAL, TRACE, "writeb f0 0x%08x = 0x%02x\n", addr, b);
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

	DBGLOG(HAL, TRACE, "SDIO: Enabling func%d...\n", func->num);

	reg = sdio_f0_readb(func, SDIO_CCCR_IOEx, &ret);
	if (ret)
		goto err;

	reg |= 1 << func->num;

	sdio_f0_writeb(func, reg, SDIO_CCCR_IOEx, &ret);
	if (ret)
		goto err;

	reg = sdio_f0_readb(func, SDIO_CCCR_IORx, &ret);
	if (ret)
		goto err;

	if (!(reg & (1 << func->num))) {
		ret = -ETIME;
		goto err;
	}

	DBGLOG(HAL, TRACE, "SDIO: Enabled func%d\n", func->num);
	return 0;

err:
	DBGLOG(HAL, TRACE, "SDIO: Failed to enable func%d\n", func->num);
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

	DBGLOG(HAL, TRACE, "SDIO: Disabling func%d...\n", func->num);

	reg = sdio_f0_readb(func, SDIO_CCCR_IOEx, &ret);
	if (ret)
		goto err;

	reg &= ~(1 << func->num);

	sdio_f0_writeb(func, reg, SDIO_CCCR_IOEx, &ret);
	if (ret)
		goto err;

	DBGLOG(HAL, TRACE, "SDIO: Disabled func%d\n", func->num);

	return 0;

err:
	DBGLOG(HAL, TRACE, "SDIO: Failed to disable func%d\n", func->num);
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
	unsigned char reg = 0;

	DBGLOG(HAL, TRACE, "SDIO: Enabling IRQ for func%d...\n", func->num);

	if (func->irq_handler) {
		DBGLOG(HAL, TRACE, "SDIO: IRQ for func%d already in use.\n", func->num);
		return -EBUSY;
	}

	reg = sdio_f0_readb(func, SDIO_CCCR_IENx, &ret);
	if (ret)
		return ret;

	reg |= 1 << func->num;

	reg |= 1; /* Master interrupt enable */

	sdio_f0_writeb(func, reg, SDIO_CCCR_IENx, &ret);
	if (ret)
		return ret;

	func->irq_handler = handler;

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

	DBGLOG(HAL, TRACE, "SDIO: Disabling IRQ for func%d...\n", func->num);

	if (func->irq_handler)
		func->irq_handler = NULL;

	reg = sdio_f0_readb(func, SDIO_CCCR_IENx, &ret);
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
