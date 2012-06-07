/* drivers/i2c/chips/smb329.c
 *
 * SMB329B Switch Charger (SUMMIT Microelectronics)
 *
 * Copyright (C) 2009 HTC Corporation
 * Author: Justin Lin <Justin_Lin@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <asm/atomic.h>

#include "board-mahimahi-smb329.h"

static struct smb329_data {
	struct i2c_client *client;
	uint8_t version;
	struct work_struct work;
	struct mutex state_lock;
	int chg_state;
} smb329;

static int smb329_i2c_write(uint8_t *value, uint8_t reg, uint8_t num_bytes)
{
	int ret;
	struct i2c_msg msg;

	/* write the first byte of buffer as the register address */
	value[0] = reg;
	msg.addr = smb329.client->addr;
	msg.len = num_bytes + 1;
	msg.flags = 0;
	msg.buf = value;

	ret = i2c_transfer(smb329.client->adapter, &msg, 1);

	return (ret >= 0) ? 0 : ret;
}

static int smb329_i2c_read(uint8_t *value, uint8_t reg, uint8_t num_bytes)
{
	int ret;
	struct i2c_msg msg[2];

	/* setup the address to read */
	msg[0].addr = smb329.client->addr;
	msg[0].len = 1;
	msg[0].flags = 0;
	msg[0].buf = &reg;

	/* setup the read buffer */
	msg[1].addr = smb329.client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = num_bytes;
	msg[1].buf = value;

	ret = i2c_transfer(smb329.client->adapter, msg, 2);

	return (ret >= 0) ? 0 : ret;
}

static int smb329_i2c_write_byte(uint8_t value, uint8_t reg)
{
	int ret;
	uint8_t buf[2] = { 0 };

	buf[1] = value;
	ret = smb329_i2c_write(buf, reg, 1);
	if (ret)
		pr_err("smb329: write byte error (%d)\n", ret);

	return ret;
}

static int smb329_i2c_read_byte(uint8_t *value, uint8_t reg)
{
	int ret = smb329_i2c_read(value, reg, 1);
	if (ret)
		pr_err("smb329: read byte error (%d)\n", ret);

	return ret;
}

int smb329_set_charger_ctrl(uint32_t ctl)
{
	mutex_lock(&smb329.state_lock);
	smb329.chg_state = ctl;
	schedule_work(&smb329.work);
	mutex_unlock(&smb329.state_lock);
	return 0;
}

static void smb329_work_func(struct work_struct *work)
{
	mutex_lock(&smb329.state_lock);

	switch (smb329.chg_state) {
	case SMB329_ENABLE_FAST_CHG:
		pr_info("smb329: charger on (fast)\n");
		smb329_i2c_write_byte(0x84, 0x31);
		smb329_i2c_write_byte(0x08, 0x05);
		if ((smb329.version & 0x18) == 0x0)
			smb329_i2c_write_byte(0xA9, 0x00);
		break;

	case SMB329_DISABLE_CHG:
	case SMB329_ENABLE_SLOW_CHG:
		pr_info("smb329: charger off/slow\n");
		smb329_i2c_write_byte(0x88, 0x31);
		smb329_i2c_write_byte(0x08, 0x05);
		break;
	default:
		pr_err("smb329: unknown charger state %d\n",
			smb329.chg_state);
	}

	mutex_unlock(&smb329.state_lock);
}

static int smb329_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C) == 0) {
		dev_dbg(&client->dev, "[SMB329]:I2C fail\n");
		return -EIO;
	}

	smb329.client = client;
	mutex_init(&smb329.state_lock);
	INIT_WORK(&smb329.work, smb329_work_func);

	smb329_i2c_read_byte(&smb329.version, 0x3B);
	pr_info("smb329 version: 0x%02x\n", smb329.version);

	return 0;
}

static const struct i2c_device_id smb329_id[] = {
	{ "smb329", 0 },
	{  },
};

static struct i2c_driver smb329_driver = {
	.driver.name    = "smb329",
	.id_table   = smb329_id,
	.probe      = smb329_probe,
};

static int __init smb329_init(void)
{
	int ret = i2c_add_driver(&smb329_driver);
	if (ret)
		pr_err("smb329_init: failed\n");

	return ret;
}

module_init(smb329_init);

MODULE_AUTHOR("Justin Lin <Justin_Lin@htc.com>");
MODULE_DESCRIPTION("SUMMIT Microelectronics SMB329B switch charger");
MODULE_LICENSE("GPL");
