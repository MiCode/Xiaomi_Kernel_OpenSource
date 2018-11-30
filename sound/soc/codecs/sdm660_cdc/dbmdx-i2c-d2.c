/*
 * DSPG DBMD2 I2C interface driver
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
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/firmware.h>

#include "dbmdx-interface.h"
#include "dbmdx-va-regmap.h"
#include "dbmdx-vqe-regmap.h"
#include "dbmdx-i2c.h"
#include "dbmdx-i2c-sbl-d2.h"


static const u8 clr_crc[] = {0x5A, 0x03, 0x52, 0x0a, 0x00,
			     0x00, 0x00, 0x00, 0x00, 0x00};



static int dbmd2_i2c_boot(const void *fw_data, size_t fw_size,
		struct dbmdx_private *p, const void *checksum,
		size_t chksum_len, int load_fw)
{
	int retry = RETRY_COUNT;
	int ret = 0;
	ssize_t send_bytes;
	struct dbmdx_i2c_private *i2c_p =
				(struct dbmdx_i2c_private *)p->chip->pdata;

	dev_dbg(i2c_p->dev, "%s\n", __func__);

	/* change to boot I2C address */
	i2c_p->client->addr = (unsigned short)(i2c_p->pdata->boot_addr);

	do {

		if (p->active_fw == DBMDX_FW_PRE_BOOT) {

			if (!(p->boot_mode & DBMDX_BOOT_MODE_RESET_DISABLED)) {
				/* reset DBMD2 chip */
				p->reset_sequence(p);
			} else {
				/* If failed and reset is disabled, break */
				if (retry != RETRY_COUNT) {
					retry = -1;
					break;
				}
			}

			/* delay before sending commands */
			if (p->clk_get_rate(p, DBMDX_CLK_MASTER) <= 32768)
				msleep(DBMDX_MSLEEP_I2C_D2_AFTER_RESET_32K);
			else
				usleep_range(DBMDX_USLEEP_I2C_D2_AFTER_RESET,
					DBMDX_USLEEP_I2C_D2_AFTER_RESET + 5000);

			if (!(p->cur_boot_options &
				DBMDX_BOOT_OPT_NO_I2C_FREQ_CALLBACK) &&
				p->set_i2c_freq_callback) {
				dev_dbg(p->dev,
					"%s: setting master bus to slow freq\n",
					__func__);
				p->set_i2c_freq_callback(
					i2c_p->client->adapter, I2C_FREQ_SLOW);
			}
			if (!(p->cur_boot_options &
				DBMDX_BOOT_OPT_DONT_SENT_SBL)) {

				/* send SBL */
				send_bytes = write_i2c_data(p, sbl,
					sizeof(sbl));
				if (send_bytes != sizeof(sbl)) {
					dev_err(p->dev,
						"%s: -------> load SBL error\n",
						__func__);
					continue;
				}

				usleep_range(DBMDX_USLEEP_I2C_D2_AFTER_SBL,
					DBMDX_USLEEP_I2C_D2_AFTER_SBL + 1000);
			}

			if (!(p->cur_boot_options &
				DBMDX_BOOT_OPT_NO_I2C_FREQ_CALLBACK) &&
				p->set_i2c_freq_callback) {

				msleep(DBMDX_MSLEEP_I2C_D2_CALLBACK);
				dev_dbg(p->dev,
					"%s: setting master bus to fast freq\n",
					__func__);
				p->set_i2c_freq_callback(
					i2c_p->client->adapter, I2C_FREQ_FAST);
			}

			/* verify chip id */
			if (p->cur_boot_options &
				DBMDX_BOOT_OPT_VERIFY_CHIP_ID) {
				ret = i2c_verify_chip_id(p);
				if (ret < 0) {
					dev_err(i2c_p->dev,
						"%s: couldn't verify chip id\n",
						__func__);
					continue;
				}
			}

			if (!(p->cur_boot_options &
				DBMDX_BOOT_OPT_DONT_CLR_CRC)) {

				/* send CRC clear command */
				ret = write_i2c_data(p, clr_crc,
					sizeof(clr_crc));
				if (ret != sizeof(clr_crc)) {
					dev_err(p->dev,
						"%s: failed to clear CRC\n",
						__func__);
					continue;
				}
			}
		} else {
			/* delay before sending commands */
			if (p->active_fw == DBMDX_FW_VQE)
				ret = send_i2c_cmd_vqe(p,
					DBMDX_VQE_SET_SWITCH_TO_BOOT_CMD,
					NULL);
			else if (p->active_fw == DBMDX_FW_VA)
				ret = send_i2c_cmd_va(p,
					DBMDX_VA_SWITCH_TO_BOOT,
					NULL);
			if (ret < 0) {
				dev_err(p->dev,
					"%s: failed to send 'Switch to BOOT' cmd\n",
					 __func__);
				continue;
			}
		}

		if (!load_fw)
			break;
		/* Sleep is needed here to ensure that chip is ready */
		msleep(DBMDX_MSLEEP_I2C_D2_AFTER_SBL);

		/* send firmware */
		send_bytes = write_i2c_data(p, fw_data, fw_size - 4);
		if (send_bytes != fw_size - 4) {
			dev_err(p->dev,
				"%s: -----------> load firmware error\n",
				__func__);
			continue;
		}

		msleep(DBMDX_MSLEEP_I2C_D2_BEFORE_FW_CHECKSUM);

		if (checksum && !(p->cur_boot_options &
					DBMDX_BOOT_OPT_DONT_VERIFY_CRC)) {
			ret = i2c_verify_boot_checksum(p, checksum, chksum_len);
			if (ret < 0) {
				dev_err(i2c_p->dev,
					"%s: could not verify checksum\n",
					__func__);
				continue;
			}
		}

		dev_info(p->dev, "%s: ---------> firmware loaded\n",
			__func__);
		break;
	} while (--retry);

	/* no retries left, failed to boot */
	if (retry <= 0) {
		dev_err(p->dev, "%s: failed to load firmware\n", __func__);
		return -EPERM;
	}

	if (!(p->cur_boot_options & DBMDX_BOOT_OPT_DONT_SEND_START_BOOT)) {
		/* send boot command */
		ret = send_i2c_cmd_boot(p, DBMDX_FIRMWARE_BOOT);
		if (ret < 0) {
			dev_err(p->dev,
				"%s: booting the firmware failed\n", __func__);
			return -EPERM;
		}
	}

	/* wait some time */
	usleep_range(DBMDX_USLEEP_I2C_D2_AFTER_BOOT,
		DBMDX_USLEEP_I2C_D2_AFTER_BOOT + 1000);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dbmdx_i2c_suspend(struct device *dev)
{
	struct chip_interface *ci = i2c_get_clientdata(to_i2c_client(dev));
	struct dbmdx_i2c_private *i2c_p =
				(struct dbmdx_i2c_private *)ci->pdata;


	dev_dbg(dev, "%s\n", __func__);

	i2c_interface_suspend(i2c_p);

	return 0;
}

static int dbmdx_i2c_resume(struct device *dev)
{
	struct chip_interface *ci = i2c_get_clientdata(to_i2c_client(dev));
	struct dbmdx_i2c_private *i2c_p =
				(struct dbmdx_i2c_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);
	i2c_interface_resume(i2c_p);

	return 0;
}
#else
#define dbmdx_i2c_suspend NULL
#define dbmdx_i2c_resume NULL
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int dbmdx_i2c_runtime_suspend(struct device *dev)
{
	struct chip_interface *ci = i2c_get_clientdata(to_i2c_client(dev));
	struct dbmdx_i2c_private *i2c_p =
				(struct dbmdx_i2c_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);

	i2c_interface_suspend(i2c_p);

	return 0;
}

static int dbmdx_i2c_runtime_resume(struct device *dev)
{
	struct chip_interface *ci = i2c_get_clientdata(to_i2c_client(dev));
	struct dbmdx_i2c_private *i2c_p =
				(struct dbmdx_i2c_private *)ci->pdata;

	dev_dbg(dev, "%s\n", __func__);
	i2c_interface_resume(i2c_p);

	return 0;
}
#else
#define dbmdx_i2c_runtime_suspend NULL
#define dbmdx_i2c_runtime_resume NULL
#endif /* CONFIG_PM */


static const struct dev_pm_ops dbmdx_i2c_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(dbmdx_i2c_suspend, dbmdx_i2c_resume)
	SET_RUNTIME_PM_OPS(dbmdx_i2c_runtime_suspend,
			   dbmdx_i2c_runtime_resume, NULL)
};


static int dbmd2_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int rc;
	struct dbmdx_i2c_private *p;
	struct chip_interface *ci;

	rc = i2c_common_probe(client, id);

	if (rc < 0)
		return rc;

	ci = i2c_get_clientdata(client);
	p = (struct dbmdx_i2c_private *)ci->pdata;

	/* fill in chip interface functions */
	p->chip.boot = dbmd2_i2c_boot;

	return rc;
}


static const struct of_device_id dbmd2_i2c_of_match[] = {
	{ .compatible = "dspg,dbmd2-i2c", },
	{},
};
#ifdef CONFIG_SND_SOC_DBMDX
MODULE_DEVICE_TABLE(of, dbmd2_i2c_of_match);
#endif

static const struct i2c_device_id dbmd2_i2c_id[] = {
	{ "dbmdx-i2c", 0 },
	{ "dbmd2-i2c", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dbmd2_i2c_id);

static struct i2c_driver dbmd2_i2c_driver = {
	.driver = {
		.name = "dbmd2-i2c",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dbmd2_i2c_of_match,
#endif
		.pm = &dbmdx_i2c_pm,
	},
	.probe =    dbmd2_i2c_probe,
	.remove =   i2c_common_remove,
	.id_table = dbmd2_i2c_id,
};

#ifdef CONFIG_SND_SOC_DBMDX
static int __init dbmd2_modinit(void)
{
	return i2c_add_driver(&dbmd2_i2c_driver);
}
module_init(dbmd2_modinit);

static void __exit dbmd2_exit(void)
{
	i2c_del_driver(&dbmd2_i2c_driver);
}
module_exit(dbmd2_exit);
#else
int dbmd2_i2c_init_interface(void)
{
	i2c_add_driver(&dbmd2_i2c_driver);
	return 0;
}

void  dbmd2_i2c_deinit_interface(void)
{
	i2c_del_driver(&dbmd2_i2c_driver);
}

int (*dbmdx_init_interface)(void) = &dbmd2_i2c_init_interface;
void (*dbmdx_deinit_interface)(void) = &dbmd2_i2c_deinit_interface;
#endif

MODULE_DESCRIPTION("DSPG DBMD2 I2C interface driver");
MODULE_LICENSE("GPL");
