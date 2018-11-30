/*
 * DSPG DBMD4 UART interface driver
 *
 * Copyright (C) 2014 DSP Group
 * Copyright (C) 2018 XiaoMi, Inc.
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
#include "dbmdx-uart.h"

/* baud rate used during fw upload */
#define UART_TTY_MAX_BAUD_RATE			4000000
/* number of stop bits during boot-up */
#define UART_TTY_BOOT_STOP_BITS			1
/* number of stop bits during normal operation */
#define UART_TTY_NORMAL_STOP_BITS		1
/* parity during boot-up */
#define UART_TTY_BOOT_PARITY			0
/* parity during normal operation */
#define UART_TTY_NORMAL_PARITY			0


static const u8 read_divider[6] = {0x5a, 0x07, 0x0c, 0x00, 0x00, 0x00};


static int dbmd4_uart_prepare_boot(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);
	uart_p->boot_baud_rate =  UART_TTY_MAX_BAUD_RATE;
	uart_p->boot_parity = UART_TTY_BOOT_PARITY;
	uart_p->boot_stop_bits = UART_TTY_BOOT_STOP_BITS;
	uart_p->normal_parity = UART_TTY_NORMAL_PARITY;
	uart_p->normal_stop_bits = UART_TTY_NORMAL_STOP_BITS;
	dev_dbg(uart_p->dev, "%s: lookup TTY band rate  = %d\n",
		__func__, uart_p->boot_baud_rate);

	return 0;
}



#define DBMD4_UART_SYNC_LENGTH 20

static int dbmd4_uart_sync(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
			(struct dbmdx_uart_private *)p->chip->pdata;
	int rc;
	u8 sync_buf[DBMD4_UART_SYNC_LENGTH];
	u8 rx_divider[6];
	u32 divider;
	u32 expected_divider;
	u32 clock_rate;

	dev_info(p->dev, "%s: start boot sync\n", __func__);

	memset(sync_buf, 0, sizeof(sync_buf));
	memset(rx_divider, 0, sizeof(rx_divider));

	rc = uart_write_data(p, (void *)sync_buf, DBMD4_UART_SYNC_LENGTH);

	if (rc != DBMD4_UART_SYNC_LENGTH)
		dev_err(uart_p->dev, "%s: sync buffer not sent correctly\n",
			__func__);

	/* check if synchronization succeeded */
	usleep_range(300, 400);
	rc = uart_wait_for_ok(p);
	if (rc != 0) {
		dev_err(p->dev, "%s: boot fail: no sync found err = %d\n",
				__func__, rc);
		return  -EAGAIN;
	}

	uart_flush_rx_fifo(uart_p);

	clock_rate = p->master_pll_rate;

	if (clock_rate == 0) {
		dev_info(uart_p->dev,
			"%s: No master clock defined, cannot verify divider\n",
			__func__);
		return 0;
	}

	/* Read divider register to verify that baud rate was locked
	   successfully */
	rc = uart_write_data(p, read_divider, sizeof(read_divider));
	if (rc != sizeof(read_divider)) {
		dev_err(uart_p->dev, "%s: could not read divider (send)\n",
			__func__);
		return  -EAGAIN;
	}

	rc = uart_read_data(p, rx_divider, 6);
	if (rc < 0) {
		dev_err(uart_p->dev,
			"%s: could not read divider data (recieve)\n",
			__func__);
		return  -EAGAIN;
	}

	if ((rx_divider[0] != read_divider[0]) ||
	    (rx_divider[1] != read_divider[1])) {
		dev_err(uart_p->dev,
			"%s: could not read divider data (mismatch)\n",
			__func__);
		dev_err(uart_p->dev,
			"%s: %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
			__func__, rx_divider[0], rx_divider[1], rx_divider[2],
			rx_divider[3], rx_divider[4], rx_divider[5]);
		return  -EAGAIN;
	}

	divider = (u32)(rx_divider[2]) +
		  (((u32)(rx_divider[3]) << 8) & 0x000000ff) +
		  (((u32)(rx_divider[4]) << 16) & 0x0000ff00) +
		  (((u32)(rx_divider[5]) << 24) & 0x00ff0000);


	expected_divider = ((u32)(clock_rate / uart_p->normal_baud_rate) % 16);

	if (expected_divider != divider) {
		dev_err(uart_p->dev,
			"%s: divider mismatch: expected %u, received %u\n",
			__func__, expected_divider, divider);

	}

	dev_dbg(p->dev, "%s: boot sync successfully\n", __func__);

	return 0;
}



static int dbmd4_uart_reset(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);


	/* set baudrate to FW upload speed */
	ret = uart_set_speed_host_only(p, DBMDX_VA_SPEED_MAX);

	if (ret) {
		dev_err(p->dev, "%s: failed configure uart\n",
			__func__);
		return -EPERM;
	}


	uart_flush_rx_fifo(uart_p);

	dev_dbg(p->dev, "%s: start boot sync\n", __func__);

	if (!(p->boot_mode & DBMDX_BOOT_MODE_RESET_DISABLED))
		/* reset the chip */
		p->reset_sequence(p);

	/* delay before sending commands */
	if (p->clk_get_rate(p, DBMDX_CLK_MASTER) <= 32768)
		msleep(DBMDX_MSLEEP_UART_D4_AFTER_RESET_32K);
	else
		usleep_range(DBMDX_USLEEP_UART_D4_AFTER_RESET,
			DBMDX_USLEEP_UART_D4_AFTER_RESET + 5000);

	if (!(p->cur_boot_options & DBMDX_BOOT_OPT_VA_NO_UART_SYNC)) {

		/* check if firmware sync is ok */
		ret = dbmd4_uart_sync(p);
		if (ret != 0) {
			dev_err(uart_p->dev, "%s: sync failed\n",
				__func__);
			return  -EAGAIN;
		}

		dev_dbg(p->dev, "%s: boot sync successful\n", __func__);
	}

	uart_flush_rx_fifo(uart_p);

	return 0;
}


static int dbmd4_uart_load_firmware(const void *fw_data, size_t fw_size,
	struct dbmdx_private *p, const void *checksum,
	size_t chksum_len)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;

	/* verify chip id */
	if (p->cur_boot_options & DBMDX_BOOT_OPT_VERIFY_CHIP_ID) {
		ret = uart_verify_chip_id(p);
		if (ret < 0) {
			dev_err(p->dev, "%s: couldn't verify chip id\n",
					__func__);
			return -EPERM;
		}
	}

	if (!(p->cur_boot_options & DBMDX_BOOT_OPT_DONT_CLR_CRC)) {
		/* send CRC clear command */
		ret = send_uart_cmd_boot(p, DBMDX_CLEAR_CHECKSUM);
		if (ret) {
			dev_err(p->dev, "%s: failed to clear CRC\n", __func__);
			return -EPERM;
		}
	}

	/* send firmware */
	ret = uart_write_data(p, fw_data, fw_size - 4);
	if (ret != (fw_size - 4)) {
		dev_err(p->dev, "%s: -----------> load firmware error\n",
			__func__);
		return -EPERM;
	}

	/* verify checksum */
	if (checksum && !(p->cur_boot_options &
					DBMDX_BOOT_OPT_DONT_VERIFY_CRC)) {
		ret = uart_verify_boot_checksum(p, checksum, chksum_len);
		if (ret < 0) {
			dev_err(uart_p->dev,
				"%s: could not verify checksum\n",
				__func__);
			return -EPERM;
		}
	}

	dev_info(p->dev, "%s: ---------> firmware loaded\n", __func__);

	return 0;
}

static int dbmd4_uart_boot(const void *fw_data, size_t fw_size,
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
		if ((p->boot_mode & DBMDX_BOOT_MODE_RESET_DISABLED) &&
			(fw_load_retry != RETRY_COUNT)) {
			fw_load_retry = -1;
			break;
		}

		reset_retry = RETRY_COUNT;
		do {
			ret = dbmd4_uart_reset(p);
			if (ret == 0)
				break;
		} while (--reset_retry);

		/* Unable to reset device */
		if (reset_retry <= 0) {
			dev_err(p->dev,	"%s, reset device err\n",
				__func__);
			return -ENODEV;
		}

		/* stop here if firmware does not need to be reloaded */
		if (load_fw) {

			ret = dbmd4_uart_load_firmware(fw_data, fw_size, p,
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

		usleep_range(DBMDX_USLEEP_UART_D4_AFTER_LOAD_FW,
			DBMDX_USLEEP_UART_D4_AFTER_LOAD_FW + 1000);

		/* everything went well */
		break;
	} while (--fw_load_retry);

	if (fw_load_retry <= 0) {
		dev_err(p->dev, "%s: exceeded max attepmts to load fw\n",
				__func__);
		return -EPERM;
	}

	return 0;
}

static int dbmd4_uart_reset_post_pll_divider(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	ret = send_uart_cmd_va(p,
				DBMDX_VA_GENERAL_CONFIGURATION_2,
				&uart_p->post_pll_div);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to get post pll divider\n",
			__func__);
		return ret;
	}


	ret = send_uart_cmd_va(p,
			DBMDX_VA_GENERAL_CONFIGURATION_2 |
			(uart_p->post_pll_div & ~DBMDX_POST_PLL_DIV_MASK),
			NULL);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to get post pll divider\n",
			__func__);
		return ret;
	}

	return 0;
}

static int dbmd4_uart_restore_post_pll_divider(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	ret = send_uart_cmd_va(p,
				DBMDX_VA_GENERAL_CONFIGURATION_2 |
				uart_p->post_pll_div,
				NULL);
	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed to restore post pll divider\n",
			__func__);
		return ret;
	}

	return 0;
}

static int dbmd4_uart_switch_to_buffering_speed(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	ret = dbmd4_uart_reset_post_pll_divider(p);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed, cannot reset post pll divider\n",
			__func__);
		return ret;
	}

	ret = uart_set_speed(p, DBMDX_VA_SPEED_BUFFERING);

	if (ret < 0) {
		dev_err(uart_p->dev, "%s:failed setting speed %x\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int dbmd4_uart_switch_to_normal_speed(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	ret = uart_set_speed(p, DBMDX_VA_SPEED_NORMAL);

	if (ret < 0) {
		dev_err(uart_p->dev, "%s:failed setting speed %x\n",
			__func__, ret);
		return ret;
	}


	ret = dbmd4_uart_restore_post_pll_divider(p);

	if (ret < 0) {
		dev_err(p->dev,
			"%s: failed, cannot restore post pll divider\n",
			__func__);
		return ret;
	}

	return 0;
}

static int dbmd4_uart_prepare_buffering(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = dbmd4_uart_switch_to_buffering_speed(p);

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

static int dbmd4_uart_finish_buffering(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = dbmd4_uart_switch_to_normal_speed(p);

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


static int dbmd4_uart_prepare_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = dbmd4_uart_switch_to_buffering_speed(p);

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

static int dbmd4_uart_finish_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled) {

		ret = dbmd4_uart_switch_to_normal_speed(p);

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

static int dbmd4_uart_probe(struct platform_device *pdev)
{
	int rc;
	struct dbmdx_uart_private *p;
	struct chip_interface *ci;

	rc = uart_common_probe(pdev, "dbmd4 uart probe thread");

	if (rc < 0)
		return rc;

	ci = dev_get_drvdata(&pdev->dev);
	p = (struct dbmdx_uart_private *)ci->pdata;

	/* fill in chip interface functions */
	p->chip.prepare_boot = dbmd4_uart_prepare_boot;
	p->chip.prepare_amodel_loading = dbmd4_uart_prepare_amodel_loading;
	p->chip.finish_amodel_loading = dbmd4_uart_finish_amodel_loading;
	p->chip.prepare_buffering = dbmd4_uart_prepare_buffering;
	p->chip.finish_buffering = dbmd4_uart_finish_buffering;
	p->chip.boot = dbmd4_uart_boot;

	return rc;
}


static const struct of_device_id dbmd_4_6_uart_of_match[] = {
	{ .compatible = "dspg,dbmdx-uart", },
	{ .compatible = "dspg,dbmd4-uart", },
	{ .compatible = "dspg,dbmd6-uart", },
	{},
};
#ifdef CONFIG_SND_SOC_DBMDX
MODULE_DEVICE_TABLE(of, dbmd_4_6_uart_of_match);
#endif

static struct platform_driver dbmd_4_6_uart_platform_driver = {
	.driver = {
		.name = "dbmd_4_6-uart",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dbmd_4_6_uart_of_match,
#endif
		.pm = &dbmdx_uart_pm,
	},
	.probe =  dbmd4_uart_probe,
	.remove = uart_common_remove,
};

#ifdef CONFIG_SND_SOC_DBMDX
static int __init dbmd_4_6_modinit(void)
{
	return platform_driver_register(&dbmd_4_6_uart_platform_driver);
}
module_init(dbmd_4_6_modinit);

static void __exit dbmd_4_6_exit(void)
{
	platform_driver_unregister(&dbmd_4_6_uart_platform_driver);
}
module_exit(dbmd_4_6_exit);
#else
int dbmd4_uart_init_interface(void)
{
	return platform_driver_register(&dbmd_4_6_uart_platform_driver);
}

void  dbmd4_uart_deinit_interface(void)
{
	platform_driver_unregister(&dbmd_4_6_uart_platform_driver);
}

int (*dbmdx_init_interface)(void) = &dbmd4_uart_init_interface;
void (*dbmdx_deinit_interface)(void) = &dbmd4_uart_deinit_interface;
#endif

MODULE_DESCRIPTION("DSPG DBMD4/DBMD6 UART interface driver");
MODULE_LICENSE("GPL");
