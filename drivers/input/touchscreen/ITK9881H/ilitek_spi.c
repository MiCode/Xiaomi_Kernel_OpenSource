/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 */

#include "ilitek.h"

struct touch_bus_info {
	struct spi_driver bus_driver;
	struct ilitek_hwif_info *hwif;
};

struct ilitek_tddi_dev *idev;

#define DMA_TRANSFER_MAX_CHUNK		64   // number of chunks to be transferred.
#define DMA_TRANSFER_MAX_LEN		1024 // length of a chunk.
struct spi_transfer	xfer[DMA_TRANSFER_MAX_CHUNK + 1];

int ilitek_spi_write_then_read_split(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	static DEFINE_MUTEX(lock);

	int status = -1;
	int xfercnt = 0, xferlen = 0, xferloop = 0;
	u8 *dma_txbuf = NULL, *dma_rxbuf = NULL;
	u8 cmd, temp1[1] = {0}, temp2[1] = {0};
	struct spi_message	message;

	dma_txbuf = kzalloc(n_tx, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(dma_txbuf)) {
		ipio_err("Failed to allocate dma_txbuf, %ld\n", PTR_ERR(dma_txbuf));
		goto out;
	}

	dma_rxbuf = kzalloc(n_rx, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(dma_rxbuf)) {
		ipio_err("Failed to allocate dma_rxbuf, %ld\n", PTR_ERR(dma_rxbuf));
		goto out;
	}

	mutex_trylock(&lock);

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	if ((n_tx == 1) && (n_rx == 1))
		cmd = SPI_READ;
	else if ((n_tx > 0) && (n_rx > 0))
		cmd = SPI_READ;
	else
		cmd = *((u8 *)txbuf);

	switch (cmd) {
	case SPI_WRITE:
		if (n_tx % DMA_TRANSFER_MAX_LEN)
			xferloop = (n_tx / DMA_TRANSFER_MAX_LEN) + 1;
		else
			xferloop = n_tx / DMA_TRANSFER_MAX_LEN;

		xferlen = n_tx;
		memcpy(dma_txbuf, (u8 *)txbuf, xferlen);

		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			if (xferlen > DMA_TRANSFER_MAX_LEN)
				xferlen = DMA_TRANSFER_MAX_LEN;

			xfer[xfercnt].len = xferlen;
			xfer[xfercnt].tx_buf = dma_txbuf + xfercnt * DMA_TRANSFER_MAX_LEN;
			spi_message_add_tail(&xfer[xfercnt], &message);
			xferlen = n_tx - (xfercnt+1) * DMA_TRANSFER_MAX_LEN;
		}
		status = spi_sync(spi, &message);
		break;
	case SPI_READ:
		/* for write cmd and head */
		xfer[0].len = n_tx;
		xfer[0].tx_buf = txbuf;
		xfer[0].rx_buf = temp1;
		spi_message_add_tail(&xfer[0], &message);

		/* for read data */
		if (n_rx % DMA_TRANSFER_MAX_LEN)
			xferloop = (n_rx / DMA_TRANSFER_MAX_LEN) + 1;
		else
			xferloop = n_rx / DMA_TRANSFER_MAX_LEN;

		xferlen = n_rx;
		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			if (xferlen > DMA_TRANSFER_MAX_LEN)
				xferlen = DMA_TRANSFER_MAX_LEN;

			xfer[xfercnt+1].len = xferlen;
			xfer[xfercnt+1].tx_buf = temp2;
			xfer[xfercnt+1].rx_buf = dma_rxbuf + xfercnt * DMA_TRANSFER_MAX_LEN;
			spi_message_add_tail(&xfer[xfercnt+1], &message);
			xferlen = n_rx - (xfercnt+1) * DMA_TRANSFER_MAX_LEN;
		}
		status = spi_sync(spi, &message);
		if (status == 0)
			memcpy((u8 *)rxbuf, dma_rxbuf, n_rx);
		break;
	default:
		ipio_info("Unknown command 0x%x\n", cmd);
		break;
	}

	mutex_unlock(&lock);

out:
	ipio_kfree((void **)&dma_txbuf);
	ipio_kfree((void **)&dma_rxbuf);
	return status;
}

int ilitek_spi_write_then_read_direct(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	static DEFINE_MUTEX(lock);

	int status = -1;
	u8 cmd, temp1[1] = {0}, temp2[1] = {0};
	struct spi_message	message;
	struct spi_transfer	xfer[2];

	mutex_trylock(&lock);

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	if ((n_tx == 1) && (n_rx == 1))
		cmd = SPI_READ;
	else
		cmd = *((u8 *)txbuf);

	switch (cmd) {
	case SPI_WRITE:
		xfer[0].len = n_tx;
		xfer[0].tx_buf = txbuf;
		spi_message_add_tail(&xfer[0], &message);
		status = spi_sync(spi, &message);
		break;
	case SPI_READ:
		/* for write cmd and head */
		xfer[0].len = n_tx;
		xfer[0].tx_buf = txbuf;
		xfer[0].rx_buf = temp1;
		spi_message_add_tail(&xfer[0], &message);

		xfer[1].len = n_rx;
		xfer[1].tx_buf = temp2;
		xfer[1].rx_buf = rxbuf;
		spi_message_add_tail(&xfer[1], &message);
		status = spi_sync(spi, &message);
		break;
	default:
		ipio_info("Unknown command 0x%x\n", cmd);
		break;
	}

	mutex_unlock(&lock);
	return status;
}

static int core_rx_lock_check(int *ret_size)
{
	int i, count = 1;
	u8 txbuf[5] = {SPI_WRITE, 0x25, 0x94, 0x0, 0x2};
	u8 rxbuf[4] = {0};
	u16 status = 0, lock = 0x5AA5;

	for (i = 0; i < count; i++) {
		txbuf[0] = SPI_WRITE;
		if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
			ipio_err("spi write (0x25,0x94,0x0,0x2) error\n");
			goto out;
		}

		txbuf[0] = SPI_READ;
		if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, 4) < 0) {
			ipio_err("spi read error\n");
			goto out;
		}

		status = (rxbuf[2] << 8) + rxbuf[3];
		*ret_size = (rxbuf[0] << 8) + rxbuf[1];

		ipio_debug("Rx lock = 0x%x, size = %d\n", status, *ret_size);

		if (status == lock)
			return 0;

		mdelay(1);
	}

out:
	ipio_err("Rx check lock error, lock = 0x%x, size = %d\n", status, *ret_size);
	return -EIO;
}

static int core_tx_unlock_check(void)
{
	int i, count = 100;
	u8 txbuf[5] = {SPI_WRITE, 0x25, 0x0, 0x0, 0x2};
	u8 rxbuf[4] = {0};
	u16 status = 0, unlock = 0x9881;

	for (i = 0; i < count; i++) {
		txbuf[0] = SPI_WRITE;
		if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
			ipio_err("spi write (0x25,0x0,0x0,0x2) error\n");
			goto out;
		}

		txbuf[0] = SPI_READ;
		if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, 4) < 0) {
			ipio_err("spi read error\n");
			goto out;
		}

		status = (rxbuf[2] << 8) + rxbuf[3];

		ipio_debug("Tx unlock = 0x%x\n", status);

		if (status == unlock)
			return 0;

		mdelay(1);
	}

out:
	ipio_err("Tx check unlock error, unlock = 0x%x\n", status);
	return -EIO;
}

static int core_spi_ice_mode_unlock_read(u8 *data, int size)
{
	int ret = 0;
	u8 txbuf[64] = { 0 };

	/* set read address */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x98;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
		ipio_info("spi write (0x25,0x98,0x00,0x2) error\n");
		ret = -EIO;
		return ret;
	}

	/* read data */
	txbuf[0] = SPI_READ;
	if (idev->spi_write_then_read(idev->spi, txbuf, 1, data, size) < 0) {
		ret = -EIO;
		return ret;
	}

	/* write data unlock */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x94;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	txbuf[5] = (size & 0xFF00) >> 8;
	txbuf[6] = size & 0xFF;
	txbuf[7] = (char)0x98;
	txbuf[8] = (char)0x81;
	if (idev->spi_write_then_read(idev->spi, txbuf, 9, txbuf, 0) < 0) {
		ipio_err("spi write unlock (0x9881) error, ret = %d\n", ret);
		ret = -EIO;
	}
	return ret;
}

static int core_spi_ice_mode_lock_write(u8 *data, int size)
{
	int ret = 0;
	int safe_size = size;
	u8 check_sum = 0, wsize = 0;
	u8 *txbuf = NULL;

	txbuf = kcalloc(size + 9, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(txbuf)) {
		ipio_err("Failed to allocate txbuf\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Write data */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x4;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;

	/* Calcuate checsum and fill it in the last byte */
	check_sum = ilitek_calc_packet_checksum(data, size);
	ipio_memcpy(txbuf + 5, data, size, safe_size + 9);
	txbuf[5 + size] = check_sum;
	size++;
	wsize = size;
	if (wsize % 4 != 0)
		wsize += 4 - (wsize % 4);

	if (idev->spi_write_then_read(idev->spi, txbuf, wsize + 5, txbuf, 0) < 0) {
		ipio_info("spi write (0x25,0x4,0x00,0x2) error\n");
		ret = -EIO;
		goto out;
	}

	/* write data lock */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x0;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	txbuf[5] = (size & 0xFF00) >> 8;
	txbuf[6] = size & 0xFF;
	txbuf[7] = (char)0x5A;
	txbuf[8] = (char)0xA5;
	if (idev->spi_write_then_read(idev->spi, txbuf, 9, txbuf, 0) < 0) {
		ipio_err("spi write lock (0x5AA5) error, ret = %d\n", ret);
		ret = -EIO;
	}

out:
	ipio_kfree((void **)&txbuf);
	return ret;
}

static int core_spi_ice_mode_disable(void)
{
	u8 txbuf[5] = {0x82, 0x1B, 0x62, 0x10, 0x18};

	if (idev->spi_write_then_read(idev->spi, txbuf, 5, txbuf, 0) < 0) {
		ipio_err("spi write ice mode disable failed\n");
		return -EIO;
	}
	return 0;
}

static int core_spi_ice_mode_enable(void)
{
	u8 txbuf[5] = {0x82, 0x1F, 0x62, 0x10, 0x18};
	u8 rxbuf[2] = {0};

	if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, 1) < 0) {
		ipio_err("spi write 0x82 error\n");
		return -EIO;
	}

	/* check recover data */
	if (rxbuf[0] != SPI_ACK) {
		ipio_err("Check SPI_ACK failed (0x%x)\n", rxbuf[0]);
		return DO_SPI_RECOVER;
	}

	if (idev->spi_write_then_read(idev->spi, txbuf, 5, rxbuf, 0) < 0) {
		ipio_err("spi write ice mode enable failed\n");
		return -EIO;
	}
	return 0;
}

static int core_spi_ice_mode_write(u8 *data, int len)
{
	int ret = 0;
	ret = core_spi_ice_mode_enable();
	if (ret < 0)
		return ret;

	/* send data and change lock status to 0x5AA5. */
	ret = core_spi_ice_mode_lock_write(data, len);
	if (ret < 0)
		goto out;

	/*
	 * Check FW if they already received the data we sent.
	 * They change lock status from 0x5AA5 to 0x9881 if they did.
	 */
	ret = core_tx_unlock_check();
	if (ret < 0)
		goto out;

out:
	if (core_spi_ice_mode_disable() < 0)
		ret = -EIO;

	return ret;
}

static int core_spi_ice_mode_read(u8 *data, int len)
{
	int size = 0, ret = 0;

	ret = core_spi_ice_mode_enable();
	if (ret < 0)
		return ret;

	/*
	 * Check FW if they already send their data to rxbuf.
	 * They change lock status from 0x9881 to 0x5AA5 if they did.
	 */
	ret = core_rx_lock_check(&size);
	if (ret < 0)
		goto out;

	if (len < size && idev->fw_uart_en == DISABLE) {
		ipio_info("WARRING! size(%d) > len(%d), use len to get data\n", size, len);
		size = len;
	}

	/* receive data from rxbuf and change lock status to 0x9881. */
	ret = core_spi_ice_mode_unlock_read(data, size);
	if (ret < 0)
		goto out;

out:
	if (core_spi_ice_mode_disable() < 0)
		ret = -EIO;

	return (ret >= 0) ? size : ret;
}

static int core_spi_write(u8 *data, int len)
{
	int ret = 0, count = 5;
	u8 *txbuf = NULL;
	int safe_size = len;

	if (atomic_read(&idev->ice_stat) == DISABLE) {
		do {
			ret = core_spi_ice_mode_write(data, len);
			if (ret >= 0)
				break;
		} while (--count > 0);
		goto out;
	}

	txbuf = kcalloc(len + 1, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(txbuf)) {
		ipio_err("Failed to allocate txbuf\n");
		return -ENOMEM;
	}

	txbuf[0] = SPI_WRITE;
	ipio_memcpy(txbuf+1, data, len, safe_size + 1);

	if (idev->spi_write_then_read(idev->spi, txbuf, len+1, txbuf, 0) < 0) {
		ipio_err("spi write data error in ice mode\n");
		ret = -EIO;
		goto out;
	}

out:
	ipio_kfree((void **)&txbuf);
	return ret;
}

static int core_spi_read(u8 *rxbuf, int len)
{
	int ret = 0, count = 5;
	u8 txbuf[1] = {0};

	txbuf[0] = SPI_READ;

	if (atomic_read(&idev->ice_stat) == DISABLE) {
		do {
			ret = core_spi_ice_mode_read(rxbuf, len);
			if (ret >= 0)
				break;
		} while (--count > 0);
		goto out;
	}

	if (idev->spi_write_then_read(idev->spi, txbuf, 1, rxbuf, len) < 0) {
		ipio_err("spi read data error in ice mode\n");
		ret = -EIO;
		goto out;
	}

out:
	return ret;
}

static int ilitek_spi_write(void *buf, int len)
{
	int ret = 0;

	if (!len) {
		ipio_err("spi write len is invaild\n");
		return -EINVAL;
	}

	ret = core_spi_write(buf, len);
	if (ret < 0) {
		if (atomic_read(&idev->tp_reset) == START) {
			ret = 0;
			goto out;
		}
		ipio_err("spi write error, ret = %d\n", ret);
	}

out:
	return ret;
}

/* If ilitek_spi_read success ,this function will return read length */
static int ilitek_spi_read(void *buf, int len)
{
	int ret = 0;

	if (!len) {
		ipio_err("spi read len is invaild\n");
		return -EINVAL;
	}

	ret = core_spi_read(buf, len);
	if (ret < 0) {
		if (atomic_read(&idev->tp_reset) == START) {
			ret = 0;
			goto out;
		}
		ipio_err("spi read error, ret = %d\n", ret);
	}

out:
	return ret;
}

static int core_spi_setup(u32 freq)
{
	ipio_info("spi clock = %d\n", freq);

	idev->spi->mode = SPI_MODE_0;
	idev->spi->bits_per_word = 8;
	idev->spi->max_speed_hz = freq;

	if (spi_setup(idev->spi) < 0) {
		ipio_err("Failed to setup spi device\n");
		return -ENODEV;
	}

	ipio_info("name = %s, bus_num = %d,cs = %d, mode = %d, speed = %d\n",
			idev->spi->modalias,
			idev->spi->master->bus_num,
			idev->spi->chip_select,
			idev->spi->mode,
			idev->spi->max_speed_hz);
	return 0;
}

static int ilitek_spi_probe(struct spi_device *spi)
{
	struct touch_bus_info *info =
	container_of(to_spi_driver(spi->dev.driver),
		struct touch_bus_info, bus_driver);

	ipio_info("ilitek spi probe\n");

	if (!spi) {
		ipio_err("spi device is NULL\n");
		return -ENODEV;
	}

	idev = devm_kzalloc(&spi->dev, sizeof(struct ilitek_tddi_dev), GFP_KERNEL);
	if (ERR_ALLOC_MEM(idev)) {
		ipio_err("Failed to allocate idev memory, %ld\n", PTR_ERR(idev));
		return -ENOMEM;
	}

	idev->fw_dma_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(idev->fw_dma_buf)) {
		ipio_err("fw kzalloc error\n");
		return -ENOMEM;
	}

	idev->i2c = NULL;
	idev->spi = spi;
	idev->dev = &spi->dev;
	idev->hwif = info->hwif;
	idev->phys = "SPI";

	idev->write = ilitek_spi_write;
	idev->read = ilitek_spi_read;
#ifdef SPI_DMA_TRANSFER_SPLIT
	idev->spi_write_then_read = ilitek_spi_write_then_read_split;
#else
	idev->spi_write_then_read = ilitek_spi_write_then_read_direct;
#endif

	idev->spi_speed = ilitek_tddi_ic_spi_speed_ctrl;
	idev->actual_tp_mode = P5_X_FW_DEMO_MODE;

	if (TDDI_RST_BIND)
		idev->reset = TP_IC_WHOLE_RST;
	else
		idev->reset = TP_HW_RST_ONLY;

	idev->rst_edge_delay = 5;
	idev->fw_open = FILP_OPEN;
	idev->fw_upgrade_mode = UPGRADE_IRAM;
	idev->mp_move_code = ilitek_tddi_move_mp_code_iram;
	idev->gesture_move_code = ilitek_tddi_move_gesture_code_iram;
	idev->esd_recover = ilitek_tddi_wq_esd_spi_check;
	idev->ges_recover = ilitek_tddi_touch_esd_gesture_iram;
	idev->gesture_mode = P5_X_FW_GESTURE_INFO_MODE;
	idev->wtd_ctrl = ON;
	idev->report = ENABLE;
	idev->netlink = DISABLE;
	idev->debug_node_open = DISABLE;

	if (ENABLE_GESTURE) {
		idev->gesture = DISABLE;
	}

	idev->gesture_process_ws = wakeup_source_register("gesture_wake_lock");
	if (!idev->gesture_process_ws) {
		ipio_err("gesture_process_ws request failed\n");
	}

	core_spi_setup(SPI_CLK);
	return info->hwif->plat_probe();
}

static int ilitek_spi_remove(struct spi_device *spi)
{
	ipio_info();
	return 0;
}

static struct spi_device_id tp_spi_id[] = {
	{TDDI_DEV_ID, 0},
};

bool ilitek_gesture_flag;

int ilitek_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value == WAKEUP_OFF) {
			idev->gesture = DISABLE;
			ilitek_gesture_flag = false;
			ipio_info("gesture disabled:%d", ilitek_gesture_flag);
		} else if (value == WAKEUP_ON) {
			idev->gesture = ENABLE;
			ilitek_gesture_flag = true;
			ipio_info("gesture enabled:%d", ilitek_gesture_flag);
		}
	}
	return 0;
}

int ilitek_tddi_interface_dev_init(struct ilitek_hwif_info *hwif)
{
	struct touch_bus_info *info;
	ipio_info("ilitek start");

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ipio_err("faied to allocate spi_driver\n");
		return -ENOMEM;
	}

	if (hwif->bus_type != BUS_SPI) {
		ipio_err("Not SPI dev\n");
		return -EINVAL;
	}

	hwif->info = info;

	info->bus_driver.driver.name = hwif->name;
	info->bus_driver.driver.owner = hwif->owner;
	info->bus_driver.driver.of_match_table = hwif->of_match_table;

	info->bus_driver.probe = ilitek_spi_probe;
	info->bus_driver.remove = ilitek_spi_remove;
	info->bus_driver.id_table = tp_spi_id;

	info->hwif = hwif;

	ipio_info("ilitek_tddi_interface_dev_init \n");

	return spi_register_driver(&info->bus_driver);
}

void ilitek_tddi_interface_dev_exit(struct ilitek_hwif_info *hwif)
{
	struct touch_bus_info *info = (struct touch_bus_info *)hwif->info;

	ipio_info("remove spi dev\n");
	spi_unregister_driver(&info->bus_driver);
	ipio_kfree((void **)&info);
}
