/*
 * DSPG DBMD2 UART interface driver
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* #define DEBUG */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/tty.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>

#include "dbmdx-interface.h"
#include "dbmdx-va-regmap.h"
#include "dbmdx-uart-sbl-d2.h"
#include "dbmdx-uart.h"

/* baud rate used during fw upload */
#define UART_TTY_MAX_BAUD_RATE			3000000
/* baud rate used during boot-up */
#define UART_TTY_BOOT_BAUD_RATE			115200
/* baud rate used during fast boot-up */
#define UART_TTY_FAST_BOOT_BAUD_RATE		230400
/* baud rate used when in normal command mode */
#define UART_TTY_NORMAL_BAUD_RATE		57600
/* number of stop bits during boot-up */
#define UART_TTY_BOOT_STOP_BITS			2
/* number of stop bits during normal operation */
#define UART_TTY_NORMAL_STOP_BITS		1
/* parity during boot-up */
#define UART_TTY_BOOT_PARITY			1
/* parity during normal operation */
#define UART_TTY_NORMAL_PARITY			0



#define UART_SYNC_LENGTH			300 /* in msec */
#define UART_SYNC_MIN_BUFFER_LEN		128 /* in bytes */

static const u8 clr_crc[] = {0x5A, 0x03, 0x52, 0x0a, 0x00,
			     0x00, 0x00, 0x00, 0x00, 0x00};

static int dbmd2_uart_boot_rate_by_clk(struct dbmdx_private *p,
	enum dbmd2_xtal_id clk_id);


static int dbmd2_uart_prepare_boot(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);
	uart_p->boot_baud_rate =  dbmd2_uart_boot_rate_by_clk(p, p->clk_type);
	uart_p->boot_parity = UART_TTY_BOOT_PARITY;
	uart_p->boot_stop_bits = UART_TTY_BOOT_STOP_BITS;
	uart_p->normal_parity = UART_TTY_NORMAL_PARITY;
	uart_p->normal_stop_bits = UART_TTY_NORMAL_STOP_BITS;
	dev_dbg(uart_p->dev, "%s: lookup TTY band rate  = %d\n",
		__func__, uart_p->boot_baud_rate);

	/* Send init sequence for up to 100ms at 115200baud.
	 * 1 start bit, 8 data bits, 1 parity bit, 2 stop bits = 12 bits
	 * FIXME: make sure it is multiple of 8
	 */
	uart_p->boot_lock_buffer_size = ((uart_p->boot_baud_rate / 12) *
			UART_SYNC_LENGTH) / 1000;

	if (uart_p->boot_lock_buffer_size < UART_SYNC_MIN_BUFFER_LEN)
		uart_p->boot_lock_buffer_size = UART_SYNC_MIN_BUFFER_LEN;

	return 0;
}


static int dbmd2_uart_sync(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
			(struct dbmdx_uart_private *)p->chip->pdata;
	int rc;
	char *buf;
	int i;

	size_t size = uart_p->boot_lock_buffer_size;

	dev_info(p->dev, "%s: start boot sync\n", __func__);

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < size; i += 8) {
		buf[i]   = 0x00;
		buf[i+1] = 0x00;
		buf[i+2] = 0x00;
		buf[i+3] = 0x00;
		buf[i+4] = 0x00;
		buf[i+5] = 0x00;
		buf[i+6] = 0x41;
		buf[i+7] = 0x63;
	}

	if (uart_p->boot_lock_buffer_size == UART_SYNC_MIN_BUFFER_LEN)
		rc = uart_write_data(p, (void *)buf, size);
	else
		rc = uart_write_data_no_sync(p, (void *)buf, size);

	if (rc != size)
		dev_err(uart_p->dev, "%s: sync buffer not sent correctly\n",
			__func__);

	/* release chip from reset */
	if (p->clk_get_rate(p, DBMDX_CLK_MASTER) > 32768)
		p->reset_release(p);

	kfree(buf);
	/* check if synchronization succeeded */
	usleep_range(300, 400);
	rc = uart_wait_for_ok(p);
	if (rc != 0) {
		dev_err(p->dev, "%s: boot fail: no sync found err = %d\n",
				__func__, rc);
		return  -EAGAIN;
	}

	uart_flush_rx_fifo(uart_p);

	dev_dbg(p->dev, "%s: boot sync successfully\n", __func__);

	return rc;
}



static int dbmd2_uart_reset(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);


	/* set baudrate to BOOT baud */
	ret = uart_configure_tty(uart_p,
				 uart_p->boot_baud_rate,
				 uart_p->boot_stop_bits,
				 uart_p->boot_parity, 0);
	if (ret) {
		dev_err(p->dev, "%s: cannot configure tty to: %us%up%uf%u\n",
			__func__,
			uart_p->boot_baud_rate,
			uart_p->boot_stop_bits,
			uart_p->boot_parity,
			0);
		return -EIO;
	}

	uart_flush_rx_fifo(uart_p);

	usleep_range(DBMDX_USLEEP_UART_D2_BEFORE_RESET,
		DBMDX_USLEEP_UART_D2_BEFORE_RESET + 10000);

	dev_dbg(p->dev, "%s: start boot sync\n", __func__);

	/* put chip in reset */
	p->reset_set(p);

	usleep_range(1000, 1100);
	/* delay before sending commands */

	if (p->clk_get_rate(p, DBMDX_CLK_MASTER) <= 32768) {
		p->reset_release(p);
		msleep(DBMDX_MSLEEP_UART_D2_AFTER_RESET_32K);
	} else
		usleep_range(DBMDX_USLEEP_UART_D2_AFTER_RESET,
			DBMDX_USLEEP_UART_D2_AFTER_RESET + 5000);

	/* check if firmware sync is ok */
	ret = dbmd2_uart_sync(p);
	if (ret != 0) {
		dev_err(uart_p->dev, "%s: sync failed, no OK from firmware\n",
			__func__);
		return  -EAGAIN;
	}

	dev_dbg(p->dev, "%s: boot sync successful\n", __func__);

	uart_flush_rx_fifo(uart_p);

	return 0;
}


static int dbmd2_uart_boot_rate_by_clk(struct dbmdx_private *p,
	enum dbmd2_xtal_id clk_id)
{
	int ret = BOOT_TTY_BAUD_115200;
	int j;

	for (j = 0; j < ARRAY_SIZE(sbl_map); j++)	{
		if (sbl_map[j].id == clk_id)
			return sbl_map[j].boot_tty_rate;
	}

	dev_warn(p->dev,
			"%s: can't match rate for clk:%d. falling back to dflt\n",
			__func__, clk_id);

	return ret;
}

static int dbmd2_uart_sbl_search(struct dbmdx_private *p,
	enum dbmd2_xtal_id clk_id)
{
	int ret = -1;
	int j;

	for (j = 0; j < ARRAY_SIZE(sbl_map); j++) {
		if (sbl_map[j].id == clk_id) {
			dev_dbg(p->dev, "%s: found sbl type %d size %d",
				__func__,
				sbl_map[j].id, sbl_map[j].img_len);
			p->sbl_data = sbl_map[j].img_data;
			return  sbl_map[j].img_len;
		}
	}
	return ret;
}

static int dbmd2_uart_load_firmware(const void *fw_data, size_t fw_size,
	struct dbmdx_private *p, const void *checksum,
	size_t chksum_len)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;
	int sbl_len = 0;

	if (!(p->cur_boot_options & DBMDX_BOOT_OPT_DONT_SENT_SBL)) {

		/* search proper sbl image */
		sbl_len = dbmd2_uart_sbl_search(p,  p->clk_type);
		if (sbl_len < 0) {
			dev_err(p->dev,
				"%s: ---------> can not find proper sbl img\n",
				__func__);
			return -EIO;
		}

		/* send SBL */
		ret = uart_write_data(p, (void *)p->sbl_data, sbl_len);
		if (ret != sbl_len) {
			dev_err(p->dev, "%s: ---------> load sbl error\n",
				__func__);
			return -EIO;
		}

		/* check if SBL is ok */
		ret = uart_wait_for_ok(p);
		if (ret != 0) {
			dev_err(p->dev,
				"%s: sbl does not respond with ok\n", __func__);
			return -EIO;
		}
	}

	/* set baudrate to FW upload speed */
	ret = uart_set_speed_host_only(p, DBMDX_VA_SPEED_MAX);

	if (ret) {
		dev_err(p->dev, "%s: failed to send change speed command\n",
			__func__);
		return -EIO;
	}
	/* verify chip id */
	if (p->cur_boot_options & DBMDX_BOOT_OPT_VERIFY_CHIP_ID) {
		ret = uart_verify_chip_id(p);
		if (ret < 0) {
			dev_err(p->dev, "%s: couldn't verify chip id\n",
					__func__);
			return -EIO;
		}
	}

	if (!(p->cur_boot_options & DBMDX_BOOT_OPT_DONT_CLR_CRC)) {
		/* send CRC clear command */
		ret = uart_write_data(p, clr_crc, sizeof(clr_crc));
		if (ret != sizeof(clr_crc)) {
			dev_err(p->dev, "%s: failed to clear CRC\n", __func__);
			return -EIO;
		}
	}

	/* send firmware */
	ret = uart_write_data(p, fw_data, fw_size - 4);
	if (ret != (fw_size - 4)) {
		dev_err(p->dev, "%s: -----------> load firmware error\n",
			__func__);
		return -EIO;
	}
	/* verify checksum */
	if (checksum && !(p->cur_boot_options &
					DBMDX_BOOT_OPT_DONT_VERIFY_CRC)) {
		msleep(DBMDX_MSLEEP_UART_WAIT_FOR_CHECKSUM);
		ret = uart_verify_boot_checksum(p, checksum, chksum_len);
		if (ret < 0) {
			dev_err(uart_p->dev,
				"%s: could not verify checksum\n",
				__func__);
			return -EIO;
		}
	}

	dev_info(p->dev, "%s: ---------> firmware loaded\n", __func__);

	return 0;
}

static int dbmd2_uart_boot(const void *fw_data, size_t fw_size,
		struct dbmdx_private *p, const void *checksum,
		size_t chksum_len, int load_fw)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int reset_retry = RETRY_COUNT;
	int fw_load_retry = RETRY_COUNT;
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	do {
		if (!(p->boot_mode & DBMDX_BOOT_MODE_RESET_DISABLED)) {

			reset_retry = RETRY_COUNT;
			do {
				ret = dbmd2_uart_reset(p);
				if (ret == 0)
					break;
			} while (--reset_retry);

			/* Unable to reset device */
			if (reset_retry <= 0) {
				dev_err(p->dev,
					"%s, reset device err\n", __func__);
				return -ENODEV;
			}
		} else {
			/* If failed and reset is disabled, break */
			if (fw_load_retry != RETRY_COUNT) {
				fw_load_retry = -1;
				break;
			}
			/* set baudrate to BOOT baud */
			ret = uart_configure_tty(uart_p,
						 uart_p->boot_baud_rate,
						 uart_p->boot_stop_bits,
						 uart_p->boot_parity, 0);
			if (ret) {
				dev_err(p->dev,
					"%s: cannot configure tty to: %us%up%uf%u\n",
					__func__,
					uart_p->boot_baud_rate,
					uart_p->boot_stop_bits,
					uart_p->boot_parity,
					0);
				return -ENODEV;
			}

			uart_flush_rx_fifo(uart_p);

			usleep_range(DBMDX_USLEEP_UART_D2_BEFORE_RESET,
				DBMDX_USLEEP_UART_D2_BEFORE_RESET + 10000);
		}

		/* stop here if firmware does not need to be reloaded */
		if (load_fw) {

			ret = dbmd2_uart_load_firmware(fw_data, fw_size, p,
				checksum, chksum_len);

			if (ret != 0) {
				dev_err(p->dev, "%s: failed to load firwmare\n",
					__func__);
				continue;
			}
		}

		if (!(p->cur_boot_options &
			DBMDX_BOOT_OPT_DONT_SEND_START_BOOT)) {
			/* send boot command */
			ret = send_uart_cmd_boot(p, DBMDX_FIRMWARE_BOOT);
			if (ret) {
				dev_err(p->dev,
				"%s: booting the firmware failed\n", __func__);
				continue;
			}
		}

		ret = uart_set_speed_host_only(p, DBMDX_VA_SPEED_BUFFERING);

		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			continue;
		}

		msleep(DBMDX_MSLEEP_UART_D2_AFTER_LOAD_FW);

		/* everything went well */
		break;
	} while (--fw_load_retry);

	if (fw_load_retry <= 0) {
		dev_err(p->dev, "%s: exceeded max attepmts to load fw\n",
				__func__);
		return -EIO;
	}

	return 0;
}

static int dbmd2_uart_finish_boot(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_uart_set_vqe_firmware_ready(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static int dbmd2_uart_prepare_buffering(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = uart_set_speed(p, DBMDX_VA_SPEED_BUFFERING);

		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			goto out;
		}
	}
out:
	return ret;
}

static int dbmd2_uart_finish_buffering(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = uart_set_speed(p, DBMDX_VA_SPEED_NORMAL);
		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			goto out;
		}
	}
out:
	return ret;
}

static int dbmd2_uart_prepare_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = uart_set_speed(p, DBMDX_VA_SPEED_BUFFERING);

		if (ret) {
			dev_err(p->dev,
				"%s: failed to send change speed command\n",
				__func__);
			goto out;
		}
	}

out:
	return ret;
}

static int dbmd2_uart_finish_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	/* do the same as for finishing buffering */
	return dbmd2_uart_finish_buffering(p);
}

#ifdef CONFIG_PM_SLEEP
static int dbmdx_uart_suspend(struct device *dev)
{
	struct chip_interface *ci = dev_get_drvdata(dev);
	struct dbmdx_uart_private *uart_p =
		(struct dbmdx_uart_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);

	uart_interface_suspend(uart_p);

	return 0;
}

static int dbmdx_uart_resume(struct device *dev)
{
	struct chip_interface *ci = dev_get_drvdata(dev);
	struct dbmdx_uart_private *uart_p =
		(struct dbmdx_uart_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);
	uart_interface_resume(uart_p);

	return 0;
}
#else
#define dbmdx_uart_suspend NULL
#define dbmdx_uart_resume NULL
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int dbmdx_uart_runtime_suspend(struct device *dev)
{
	struct chip_interface *ci = dev_get_drvdata(dev);
	struct dbmdx_uart_private *uart_p =
		(struct dbmdx_uart_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);

	uart_interface_suspend(uart_p);

	return 0;
}

static int dbmdx_uart_runtime_resume(struct device *dev)
{
	struct chip_interface *ci = dev_get_drvdata(dev);
	struct dbmdx_uart_private *uart_p =
		(struct dbmdx_uart_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);
	uart_interface_resume(uart_p);

	return 0;
}
#else
#define dbmdx_uart_runtime_suspend NULL
#define dbmdx_uart_runtime_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops dbmdx_uart_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(dbmdx_uart_suspend, dbmdx_uart_resume)
	SET_RUNTIME_PM_OPS(dbmdx_uart_runtime_suspend,
			   dbmdx_uart_runtime_resume, NULL)
};


static int uart_probe(struct platform_device *pdev)
{
	int rc;
	struct dbmdx_uart_private *p;
	struct chip_interface *ci;

	rc = uart_common_probe(pdev, "dbmd2 uart probe thread");

	if (rc < 0)
		return rc;

	ci = dev_get_drvdata(&pdev->dev);
	p = (struct dbmdx_uart_private *)ci->pdata;

	/* fill in chip interface functions */
	p->chip.prepare_boot = dbmd2_uart_prepare_boot;
	p->chip.boot = dbmd2_uart_boot;
	p->chip.finish_boot = dbmd2_uart_finish_boot;
	p->chip.set_vqe_firmware_ready = dbmd2_uart_set_vqe_firmware_ready;
	p->chip.prepare_buffering = dbmd2_uart_prepare_buffering;
	p->chip.finish_buffering = dbmd2_uart_finish_buffering;
	p->chip.prepare_amodel_loading = dbmd2_uart_prepare_amodel_loading;
	p->chip.finish_amodel_loading = dbmd2_uart_finish_amodel_loading;
	return rc;
}


static const struct of_device_id dbmd2_uart_of_match[] = {
	{ .compatible = "dspg,dbmd2-uart", },
	{},
};
#ifdef CONFIG_SND_SOC_DBMDX_M
MODULE_DEVICE_TABLE(of, dbmd2_uart_of_match);
#endif

static struct platform_driver dbmd2_uart_platform_driver = {
	.driver = {
		.name = "dbmd2-uart",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dbmd2_uart_of_match,
#endif
		.pm = &dbmdx_uart_pm,
	},
	.probe =  uart_probe,
	.remove = uart_common_remove,
};

#ifdef CONFIG_SND_SOC_DBMDX_M
static int __init dbmd2_modinit(void)
{
	return platform_driver_register(&dbmd2_uart_platform_driver);
}
module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	platform_driver_unregister(&dbmd2_uart_platform_driver);
}
module_exit(dbmd2_exit);
#else
int dbmd2_uart_init_interface(void)
{
	platform_driver_register(&dbmd2_uart_platform_driver);
	return 0;
}

void  dbmd2_uart_deinit_interface(void)
{
	platform_driver_unregister(&dbmd2_uart_platform_driver);
}

int (*dbmdx_init_interface)(void) = &dbmd2_uart_init_interface;
void (*dbmdx_deinit_interface)(void) = &dbmd2_uart_deinit_interface;
#endif

MODULE_DESCRIPTION("DSPG DBMD2 UART interface driver");
MODULE_LICENSE("GPL");
