/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/
#define SPI_SLAVE_DRV_NAME	"spi-slave"
#define pr_fmt(fmt) SPI_SLAVE_DRV_NAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cache.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/pinctrl/consumer.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/platform_data/spi-mt65xx.h>
#include "spi_slave.h"
/*
 * SPI command description.
 */
#define CMD_PWOFF			0x02 /* Power Off */
#define CMD_PWON			0x04 /* Power On */
#define CMD_RS				0x06 /* Read Status */
#define CMD_WS				0x08 /* Write Status */
#define CMD_CR				0x0a /* Config Read */
#define CMD_CW				0x0c /* Config Write */
#define CMD_RD				0x81 /* Read Data */
#define CMD_WD				0x0e /* Write Data */
#define CMD_CT				0x10 /* Config Type */
/*
 * SPI slave status register (to master).
 */
#define SLV_ON				BIT(0)
#define SR_CFG_SUCCESS			BIT(1)
#define SR_TXRX_FIFO_RDY		BIT(2)
#define SR_RD_ERR			BIT(3)
#define SR_WR_ERR			BIT(4)
#define SR_RDWR_FINISH			BIT(5)
#define SR_TIMEOUT_ERR			BIT(6)
#define SR_CMD_ERR			BIT(7)
#define CONFIG_READY	((SR_CFG_SUCCESS | SR_TXRX_FIFO_RDY))
/*
 * hardware limit for once transfter.
 */
#define MAX_SPI_XFER_SIZE_ONCE		(64 * 1024 - 1)
#define MAX_SPI_TRY_CNT			(10)
/*
 * default never pass more than 32 bytes
 */
#define MTK_SPI_BUFSIZ	min(32, SMP_CACHE_BYTES)
#define SPI_READ_STA_ERR_RET	(1)
/*
 * spi slave config
 */
#define IOCFG_BASE_ADDR	0x00005000
#define DRV_CFG0	(IOCFG_BASE_ADDR + 0x0)
#define SPIS_SLVO_MASK	(0x7 << 21)

#define SPISLV_BASE_ADDR	0x00002000
#define SPISLV_CTRL	(SPISLV_BASE_ADDR + 0x0)
#define EARLY_TRANS_MASK	(0x1 << 16)

/* specific SPI data */
struct mtk_spi_slave_data {
	struct spi_device *spi;
	u32 tx_speed_hz;
	u32 rx_speed_hz;
	u8 slave_drive_strength;
	u8 high_speed_tick_delay;
	u8 low_speed_tick_delay;
	u8 high_speed_early_trans;
	u8 low_speed_early_trans;
	/* mutex for SPI Slave IO */
	struct mutex	spislv_mutex;
	u8 tx_nbits:3;
	u8 rx_nbits:3;
};

static struct mtk_spi_slave_data slv_data = {
	.spi = NULL,
	.tx_speed_hz = SPI_TX_LOW_SPEED_HZ,
	.rx_speed_hz = SPI_RX_LOW_SPEED_HZ,
	.slave_drive_strength = 0,
	.high_speed_tick_delay = 0,
	.low_speed_tick_delay = 0,
	.high_speed_early_trans = 0,
	.low_speed_early_trans = 0,
	.tx_nbits = 0,
	.rx_nbits = 0,
};
/*
 * A piece of default chip info unless the platform
 * supplies it.
 */
static struct mtk_chip_config spislv_chip_info = {
	.rx_mlsb = 0,
	.tx_mlsb = 0,
	.sample_sel = 0,
	.cs_setuptime = 0,
	.cs_holdtime = 0,
	.cs_idletime = 0,
	.deassert_mode = false,
	.tick_delay = 0,
};

static u8 cmd_trans_type_4byte_single[2] = {CMD_CT, 0x04};
static u8 tx_cmd_read_sta[2] = {CMD_RS, 0x00};
static u8 rx_cmd_read_sta[2] = {0x00, 0x00};
static struct spi_transfer CT_TRANSFER = {0};
static struct spi_transfer RS_TRANSFER = {0};

static int spislv_sync_sub(u32 addr, void *val, u32 len, bool is_read)
{
	int ret = 0, i = 0;
	struct spi_message msg;
	struct spi_transfer x[3] = {0}; /* CW/CR, WD/RD, WS */
	void *local_buf = NULL;
	u8 mtk_spi_buffer[MTK_SPI_BUFSIZ];
	u8 cmd_write_sta[2] = {CMD_WS, 0xff};
	u8 status = 0;
	u32 retry = 0;
	u8 cmd_config[9] = {0};

	/* CR or CW */
	if (is_read)
		cmd_config[0] = CMD_CR;
	else
		cmd_config[0] = CMD_CW;

	for (i = 0; i < 4; i++) {
		cmd_config[1 + i] = (addr & (0xff << (i * 8))) >> (i * 8);
		cmd_config[5 + i] = ((len - 1) & (0xff << (i * 8))) >> (i * 8);
	}

	x[0].tx_buf	= cmd_config;
	x[0].len	= ARRAY_SIZE(cmd_config);
	x[0].tx_nbits	= slv_data.tx_nbits;
	x[0].rx_nbits	= slv_data.rx_nbits;
	x[0].speed_hz	= slv_data.tx_speed_hz;
	x[0].cs_change = 1;
	spi_message_init(&msg);
	spi_message_add_tail(&x[0], &msg);

	/* RS */
	rx_cmd_read_sta[1] = 0;
	spi_message_add_tail(&RS_TRANSFER, &msg);

	ret = spi_sync(slv_data.spi, &msg);
	if (ret)
		goto tail;

	status = rx_cmd_read_sta[1];
	/* ignore status for set early transfer bit */
	if (addr == SPISLV_CTRL && !is_read)
		status = 0x6;
	if ((status & CONFIG_READY) != CONFIG_READY) {
		pr_notice("SPI config %s but status error: 0x%x, latched by %dHZ, err addr: 0x%x\n",
				is_read ? "read" : "write", status, slv_data.rx_speed_hz, addr);
		ret = SPI_READ_STA_ERR_RET;
		goto tail;
	}

	/* RD or WD */
	if (len > MTK_SPI_BUFSIZ - 1) {
		local_buf = kzalloc(len + 1, GFP_KERNEL);
		if (!local_buf) {
			ret = -ENOMEM;
			goto tail;
		}
	} else {
		local_buf = mtk_spi_buffer;
		memset(local_buf, 0, MTK_SPI_BUFSIZ);
	}

	if (is_read) {
		*((u8 *)local_buf) = CMD_RD;
		x[1].tx_buf = local_buf;
		x[1].rx_buf = local_buf;
		x[1].speed_hz = slv_data.rx_speed_hz;
	} else {
		*((u8 *)local_buf) = CMD_WD;
		memcpy((u8 *)local_buf + 1, val, len);
		x[1].tx_buf = local_buf;
		x[1].speed_hz = slv_data.tx_speed_hz;
	}
	x[1].tx_nbits = slv_data.tx_nbits;
	x[1].rx_nbits = slv_data.rx_nbits;
	x[1].len = len + 1;
	x[1].cs_change = 1;

	spi_message_init(&msg);
	spi_message_add_tail(&x[1], &msg);

	/* RS */
	rx_cmd_read_sta[1] = 0;
	spi_message_add_tail(&RS_TRANSFER, &msg);
	ret = spi_sync(slv_data.spi, &msg);
	if (ret)
		goto tail;

	status = rx_cmd_read_sta[1];
	/* ignore status for set early transfer bit */
	if (addr == SPISLV_CTRL && !is_read)
		status = 0x26;
	if (((status & SR_RD_ERR) == SR_RD_ERR) ||
		((status & SR_WR_ERR) == SR_WR_ERR) ||
		((status & SR_TIMEOUT_ERR) == SR_TIMEOUT_ERR)) {

		pr_notice("SPI %s error, status: 0x%x, latched by %dHZ, err addr: 0x%x\n",
				is_read ? "read" : "write", status, slv_data.rx_speed_hz, addr);

		/* WS */
		x[2].tx_buf	= cmd_write_sta;
		x[2].len	= ARRAY_SIZE(cmd_write_sta);
		x[2].tx_nbits	= slv_data.tx_nbits;
		x[2].rx_nbits	= slv_data.rx_nbits;
		x[2].speed_hz	= slv_data.tx_speed_hz;
		spi_message_init(&msg);
		spi_message_add_tail(&x[2], &msg);
		ret = spi_sync(slv_data.spi, &msg);
		if (ret)
			goto tail;

		ret = SPI_READ_STA_ERR_RET;
	} else {
		while (((status & SR_RDWR_FINISH) != SR_RDWR_FINISH)) {
			pr_notice("SPI %s not finish, status: 0x%x, latched by %dHZ, err addr: 0x%x, polling: %d\n",
				is_read ? "read" : "write",
				status, slv_data.rx_speed_hz, addr, retry);
			if (retry++ >= MAX_SPI_TRY_CNT) {
				ret = SPI_READ_STA_ERR_RET;
				goto tail;
			}
			mdelay(1);

			/* RS */
			rx_cmd_read_sta[1] = 0;
			spi_message_init(&msg);
			spi_message_add_tail(&RS_TRANSFER, &msg);
			ret = spi_sync(slv_data.spi, &msg);
			if (ret)
				goto tail;
			status = rx_cmd_read_sta[1];
		}
	}

tail:
	/* Only for successful read */
	if (is_read && !ret)
		memcpy(val, ((u8 *)x[1].rx_buf + 1), len);

	if (local_buf != mtk_spi_buffer)
		kfree(local_buf);
	return ret;
}

static int spislv_sync(u32 addr, void *val, u32 len, bool is_read)
{
	int ret = 0;
	u32 addr_local = addr;
	void *val_local = val;
	u32 len_local = len;
	u32 try = 0;

	mutex_lock(&slv_data.spislv_mutex);

	if (len_local < MAX_SPI_XFER_SIZE_ONCE)
		goto transfer_drect;

	while (len_local > MAX_SPI_XFER_SIZE_ONCE) {
		ret = spislv_sync_sub(addr_local, val_local, MAX_SPI_XFER_SIZE_ONCE, is_read);
		while (ret) {
			pr_notice("spi slave error, addr: 0x%x, ret(%d), retry: %d\n",
				addr_local, ret, try);
			if (try++ == MAX_SPI_TRY_CNT)
				goto tail;
			ret = spislv_sync_sub(addr_local, val_local,
				MAX_SPI_XFER_SIZE_ONCE, is_read);
		}
		addr_local = addr_local + MAX_SPI_XFER_SIZE_ONCE;
		val_local = (u8 *)val_local + MAX_SPI_XFER_SIZE_ONCE;
		len_local = len_local - MAX_SPI_XFER_SIZE_ONCE;
	}

transfer_drect:
	try = 0;
	ret = spislv_sync_sub(addr_local, val_local, len_local, is_read);
	while (ret) {
		pr_notice("spi slave error, addr: 0x%x, ret(%d), retry: %d\n",
			addr_local, ret, try);
		if (try++ == MAX_SPI_TRY_CNT)
			goto tail;
		ret = spislv_sync_sub(addr_local, val_local, len_local, is_read);
	}

tail:
	mutex_unlock(&slv_data.spislv_mutex);
	return ret;
}

int spislv_init(void)
{
	struct spi_message msg;
	int ret = 0;

	spislv_chip_info.tick_delay = slv_data.low_speed_tick_delay;
	slv_data.tx_speed_hz = SPI_TX_LOW_SPEED_HZ;
	slv_data.rx_speed_hz = SPI_RX_LOW_SPEED_HZ;
	RS_TRANSFER.speed_hz = slv_data.rx_speed_hz;

	spi_message_init(&msg);
	spi_message_add_tail(&CT_TRANSFER, &msg);
	ret = spi_sync(slv_data.spi, &msg);
	if (ret)
		return ret;

	ret = spislv_write_register(SPISLV_CTRL, (0x40 & (~(EARLY_TRANS_MASK)))
		| ((slv_data.low_speed_early_trans << 16) & (EARLY_TRANS_MASK)));
	if (ret)
		return ret;

	ret = spislv_write_register_mask(DRV_CFG0,
		(slv_data.slave_drive_strength << 21), SPIS_SLVO_MASK);
	return ret;
}

int spislv_switch_speed_hz(u32 tx_speed_hz, u32 rx_speed_hz)
{
	int ret = 0;

	if (rx_speed_hz >= SPI_RX_MAX_SPEED_HZ) {
		ret = spislv_write_register(SPISLV_CTRL, (0x40 & (~(EARLY_TRANS_MASK)))
			| ((slv_data.high_speed_early_trans << 16) & (EARLY_TRANS_MASK)));
		spislv_chip_info.tick_delay = slv_data.high_speed_tick_delay;
	} else {
		ret = spislv_write_register(SPISLV_CTRL, (0x40 & (~(EARLY_TRANS_MASK)))
			| ((slv_data.low_speed_early_trans << 16) & (EARLY_TRANS_MASK)));
		spislv_chip_info.tick_delay = slv_data.low_speed_tick_delay;
	}
	slv_data.tx_speed_hz =
		(tx_speed_hz > SPI_TX_MAX_SPEED_HZ ? SPI_TX_MAX_SPEED_HZ : tx_speed_hz);
	slv_data.rx_speed_hz =
		(rx_speed_hz > SPI_RX_MAX_SPEED_HZ ? SPI_RX_MAX_SPEED_HZ : rx_speed_hz);
	RS_TRANSFER.speed_hz = slv_data.rx_speed_hz;

	return ret;
}

int spislv_write(u32 addr, void *val, u32 len)
{
	return spislv_sync(addr, val, len, 0);
}

int spislv_read(u32 addr, void *val, u32 len)
{
	return spislv_sync(addr, val, len, 1);
}

int spislv_read_register(u32 addr, u32 *val)
{
	return spislv_read(addr, (u8 *)val, 4);
}

int spislv_write_register(u32 addr, u32 val)
{
	return spislv_write(addr, (u8 *)&val, 4);
}

int spislv_write_register_mask(u32 addr, u32 val, u32 msk)
{
	u32 ret = 0;
	u32 read_val;

	ret = spislv_read_register(addr, &read_val);
	if (ret)
		return ret;
	ret = spislv_write_register(addr, ((read_val & (~(msk))) | ((val) & (msk))));

	return ret;
}

static int spi_slave_probe(struct spi_device *spi)
{
	int ret = 0;
	struct device_node *nc = spi->dev.of_node;
	struct pinctrl *spislv_pinctrl;
	struct pinctrl_state *pin_spi_mode;

	slv_data.spi = spi;
	ret = of_property_read_u8(nc, "slave-drive-strength", &(slv_data.slave_drive_strength));
	if (ret)
		pr_info("slave-drive-strength isn't setting!\n");
	else
		pr_info("slave-drive-strength = %d\n", slv_data.slave_drive_strength);

	ret = of_property_read_u8(nc, "high-speed-tick-delay", &(slv_data.high_speed_tick_delay));
	if (ret)
		pr_info("high-speed-tick-delay isn't setting!\n");
	else
		pr_info("high-speed-tick-delay = %d\n", slv_data.high_speed_tick_delay);

	ret = of_property_read_u8(nc, "low-speed-tick-delay", &(slv_data.low_speed_tick_delay));
	if (ret)
		pr_info("low-speed-tick-delay isn't setting!\n");
	else
		pr_info("low-speed-tick-delay = %d\n", slv_data.low_speed_tick_delay);

	ret = of_property_read_u8(nc, "high-speed-early-trans", &(slv_data.high_speed_early_trans));
	if (ret)
		pr_info("high-speed-early-trans isn't setting!\n");
	else
		pr_info("high-speed-early-trans = %d\n", slv_data.high_speed_early_trans);

	ret = of_property_read_u8(nc, "low-speed-early-trans", &(slv_data.low_speed_early_trans));
	if (ret)
		pr_info("low-speed-early-trans isn't setting!\n");
	else
		pr_info("low-speed-early-trans = %d\n", slv_data.low_speed_early_trans);

	if (spi->mode & SPI_TX_DUAL)
		slv_data.tx_nbits = SPI_NBITS_DUAL;
	else if (spi->mode & SPI_TX_QUAD)
		slv_data.tx_nbits = SPI_NBITS_QUAD;
	else
		slv_data.tx_nbits = SPI_NBITS_SINGLE;

	if (spi->mode & SPI_RX_DUAL)
		slv_data.rx_nbits = SPI_NBITS_DUAL;
	else if (spi->mode & SPI_RX_QUAD)
		slv_data.rx_nbits = SPI_NBITS_QUAD;
	else
		slv_data.rx_nbits = SPI_NBITS_SINGLE;

	/* set spi master driving */
	spislv_pinctrl = devm_pinctrl_get(slv_data.spi->controller->dev.parent);
	if (IS_ERR_OR_NULL(spislv_pinctrl))
		pr_notice("Failed to get pinctrl handler!\n");
	pin_spi_mode = pinctrl_lookup_state(spislv_pinctrl, "default");
	ret = pinctrl_select_state(spislv_pinctrl, pin_spi_mode);
	if (ret < 0)
		pr_notice("Failed to select pinctrl!\n");

	/* init transfers */
	if (slv_data.tx_nbits == SPI_NBITS_SINGLE) {
		CT_TRANSFER.tx_buf = cmd_trans_type_4byte_single;
		CT_TRANSFER.len = ARRAY_SIZE(cmd_trans_type_4byte_single);
	} else {
		pr_notice("spi slave: don't support other transfer type.\n");
		return -EINVAL;
	}
	CT_TRANSFER.tx_nbits = slv_data.tx_nbits;
	CT_TRANSFER.rx_nbits = slv_data.rx_nbits;
	CT_TRANSFER.speed_hz = slv_data.tx_speed_hz;
	RS_TRANSFER.tx_buf = tx_cmd_read_sta;
	RS_TRANSFER.rx_buf = rx_cmd_read_sta;
	RS_TRANSFER.len = ARRAY_SIZE(tx_cmd_read_sta);
	RS_TRANSFER.tx_nbits = slv_data.tx_nbits;
	RS_TRANSFER.rx_nbits = slv_data.rx_nbits;
	RS_TRANSFER.speed_hz = slv_data.rx_speed_hz;

	spi->bits_per_word = 8;
	spi->controller_data = (void *)&spislv_chip_info;
	spislv_chip_info.tick_delay = slv_data.low_speed_tick_delay;
	mutex_init(&slv_data.spislv_mutex);

	return 0;
}

static int spi_slave_remove(struct spi_device *spi)
{
	if (spi && spi->controller_data)
		kfree(spi->controller_data);
	return 0;
}

static const struct of_device_id spi_slave_of_ids[] = {
	{ .compatible = "mediatek,spi_slave" },
	{}
};
MODULE_DEVICE_TABLE(of, spi_slave_of_ids);

static struct spi_driver spi_slave_drv = {
	.driver = {
		.name	= SPI_SLAVE_DRV_NAME,
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
		.of_match_table = spi_slave_of_ids,
	},
	.probe	= spi_slave_probe,
	.remove	= spi_slave_remove,
};
module_spi_driver(spi_slave_drv);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shi Ma <shi.ma@mediatek.com>");
MODULE_DESCRIPTION("SPI driver for mt6382");

