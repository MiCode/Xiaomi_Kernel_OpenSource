/*
 * max77729.c - mfd core driver for the Maxim 77729
 *
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77729.h>
#include <linux/mfd/max77729-private.h>
#include <linux/regulator/machine.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include <linux/mfd/max77729_pass1.h>

#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_PMIC	(0xCC >> 1)	/* Top sys, Haptic */
#define I2C_ADDR_MUIC	(0x4A >> 1)
#define I2C_ADDR_CHG    (0xD2 >> 1)
#define I2C_ADDR_FG     (0x6C >> 1)
#define I2C_ADDR_DEBUG  (0xC4 >> 1)

#define I2C_RETRY_CNT	3

/*
 * pmic revision information
 */

static struct mfd_cell max77729_devs[] = {
	{ .name = "max77729-usbc", },
	{ .name = "max77729-fuelgauge", },
	{ .name = "max77729-charger", },
};

extern char nopmi_set_charger_ic_type(NOPMI_CHARGER_IC_TYPE nopmi_type);

int max77729_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&max77729->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		/* pr_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
			/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
	}
	mutex_unlock(&max77729->i2c_lock);
	if (ret < 0) {
		/* pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret); */
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77729_read_reg);

int max77729_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&max77729->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
		if (ret >= 0)
			break;
		/* pr_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
			/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
	}
	mutex_unlock(&max77729->i2c_lock);
	if (ret < 0) {
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(max77729_bulk_read);

int max77729_read_word(struct i2c_client *i2c, u8 reg)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&max77729->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_word_data(i2c, reg);
		if (ret >= 0)
			break;
		/* pr_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
			/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
	}
	mutex_unlock(&max77729->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77729_read_word);

int max77729_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int ret = -EIO, i;
	int timeout = 2000; /* 2sec */
	int interval = 100;

	while (ret == -EIO) {
		mutex_lock(&max77729->i2c_lock);
		for (i = 0; i < I2C_RETRY_CNT; ++i) {
			ret = i2c_smbus_write_byte_data(i2c, reg, value);
			if ((ret >= 0) || (ret == -EIO))
				break;
			/* pr_info("%s:%s reg(0x%02x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
				/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
		}
		mutex_unlock(&max77729->i2c_lock);

		if (ret < 0) {
			pr_info("%s:%s reg(0x%x), ret(%d), timeout %d\n",
					MFD_DEV_NAME, __func__, reg, ret, timeout);

			if (timeout < 0)
				break;

			msleep(interval);
			timeout -= interval;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(max77729_write_reg);

int max77729_write_reg_nolock(struct i2c_client *i2c, u8 reg, u8 value)
{
	int ret = -EIO;
	int timeout = 2000; /* 2sec */
	int interval = 100;

	while (ret == -EIO) {
		ret = i2c_smbus_write_byte_data(i2c, reg, value);

		if (ret < 0) {
			pr_info("%s:%s reg(0x%x), ret(%d), timeout %d\n",
					MFD_DEV_NAME, __func__, reg, ret, timeout);

			if (timeout < 0)
				break;

			msleep(interval);
			timeout -= interval;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(max77729_write_reg_nolock);

int max77729_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int ret = -EIO, i;
	int timeout = 2000; /* 2sec */
	int interval = 100;

	while (ret == -EIO) {
		mutex_lock(&max77729->i2c_lock);
		for (i = 0; i < I2C_RETRY_CNT; ++i) {
			ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
			if ((ret >= 0) || (ret == -EIO))
				break;
			/* pr_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
				/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
		}
		mutex_unlock(&max77729->i2c_lock);

		if (ret < 0) {
			pr_info("%s:%s reg(0x%x), ret(%d), timeout %d\n",
					MFD_DEV_NAME, __func__, reg, ret, timeout);

			if (timeout < 0)
				break;

			msleep(interval);
			timeout -= interval;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(max77729_bulk_write);

int max77729_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int ret, i;

	mutex_lock(&max77729->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_word_data(i2c, reg, value);
		if (ret >= 0)
			break;
		/* pr_info("%s:%s reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
			/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
	}
	mutex_unlock(&max77729->i2c_lock);
	if (ret < 0) {
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(max77729_write_word);

int max77729_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int ret, i;
	u8 old_val, new_val;

	mutex_lock(&max77729->i2c_lock);
	for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret >= 0)
			break;
		/* pr_info("%s:%s read reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
			/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
	}
	if (ret < 0) {
		goto err;
	}
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		for (i = 0; i < I2C_RETRY_CNT; ++i) {
			ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
			if (ret >= 0)
				break;
			/* pr_info("%s:%s write reg(0x%x), ret(%d), i2c_retry_cnt(%d/%d)\n", */
				/* MFD_DEV_NAME, __func__, reg, ret, i + 1, I2C_RETRY_CNT); */
		}
		if (ret < 0) {
			goto err;
		}
	}
err:
	mutex_unlock(&max77729->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77729_update_reg);

#if defined(CONFIG_OF)
static int of_max77729_dt(struct device *dev, struct max77729_platform_data *pdata)
{
	struct device_node *np_max77729 = dev->of_node;
	struct device_node *np_battery;
	int ret, val;

	if (!np_max77729)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_max77729, "max77729,irq-gpio", 0);
	/* pr_info("%s: irq-gpio: %u\n", __func__, pdata->irq_gpio); */

	pdata->wakeup = of_property_read_bool(np_max77729, "max77729,wakeup");

	if (of_property_read_u32(np_max77729, "max77729,fw_product_id", &pdata->fw_product_id))
		pdata->fw_product_id = 0;

#if defined(CONFIG_SEC_FACTORY)
	pdata->blocking_waterevent = 0;
#else
	pdata->blocking_waterevent = of_property_read_bool(np_max77729, "max77729,blocking_waterevent");
#endif
	ret = of_property_read_u32(np_max77729, "max77729,extra_fw_enable", &val);
	if (ret) {
		/* pr_info("%s: extra_fw_enable value not specified\n", __func__); */
		pdata->extra_fw_enable = 0;
	} else {
		/* pr_info("%s: extra_fw_enable: %d\n", __func__, val); */
		pdata->extra_fw_enable = val;
	}

	np_battery = of_find_node_by_name(NULL, "mfc-charger");
	if (!np_battery) {
		pr_info("%s: np_battery NULL\n", __func__);
	} else {
		pdata->wpc_en = of_get_named_gpio(np_battery, "battery,wpc_en", 0);
		if (pdata->wpc_en < 0) {
			pr_info("%s: can't get wpc_en (%d)\n", __func__, pdata->wpc_en);
			pdata->wpc_en = 0;
		}

		ret = of_property_read_string(np_battery,
				"battery,wireless_charger_name", (char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Wireless charger name is Empty\n", __func__);
	}
	pdata->support_audio = of_property_read_bool(np_max77729, "max77729,support-audio");
	/* pr_info("%s: support_audio %d\n", __func__, pdata->support_audio); */

	return 0;
}
#endif /* CONFIG_OF */
static void max77729_reset_ic(struct max77729_dev *max77729)
{
	pr_info("Reset!!");
	max77729_write_reg(max77729->muic, 0x80, 0x0F);
	msleep(100);
}

static void max77729_usbc_wait_response_q(struct work_struct *work)
{
	struct max77729_dev *max77729;
	u8 read_value = 0x00;
	u8 dummy[2] = { 0, };

	max77729 = container_of(work, struct max77729_dev, fw_work);

	while (max77729->fw_update_state == FW_UPDATE_WAIT_RESP_START) {
			max77729_bulk_read(max77729->muic, REG_UIC_INT, 1, dummy);
			read_value = dummy[0];
		if ((read_value & BIT_APCmdResI) == BIT_APCmdResI)
			break;
	}

	complete_all(&max77729->fw_completion);
}

static int max77729_usbc_wait_response(struct max77729_dev *max77729)
{
	unsigned long time_remaining = 0;

	max77729->fw_update_state = FW_UPDATE_WAIT_RESP_START;

	init_completion(&max77729->fw_completion);
	queue_work(max77729->fw_workqueue, &max77729->fw_work);

	time_remaining = wait_for_completion_timeout(
			&max77729->fw_completion,
			msecs_to_jiffies(FW_WAIT_TIMEOUT));

	max77729->fw_update_state = FW_UPDATE_WAIT_RESP_STOP;

	if (!time_remaining) {
		pr_info("Failed to update due to timeout");
		cancel_work_sync(&max77729->fw_work);
		return FW_UPDATE_TIMEOUT_FAIL;
	}

	return 0;
}

static int __max77729_usbc_fw_update(
		struct max77729_dev *max77729, const u8 *fw_bin)
{
	u8 fw_cmd = FW_CMD_END;
	u8 fw_len = 0;
	u8 fw_opcode = 0;
	u8 fw_data_len = 0;
	u8 fw_data[FW_CMD_WRITE_SIZE] = { 0, };
	u8 verify_data[FW_VERIFY_DATA_SIZE] = { 0, };
	int ret = -FW_UPDATE_CMD_FAIL;

	/*
	 * fw_bin[0] = Write Command (0x01)
	 * or
	 * fw_bin[0] = Read Command (0x03)
	 * or
	 * fw_bin[0] = End Command (0x00)
	 */
	fw_cmd = fw_bin[0];

	/*
	 * Check FW Command
	 */
	if (fw_cmd == FW_CMD_END) {
		max77729_reset_ic(max77729);
		max77729->fw_update_state = FW_UPDATE_END;
		return FW_UPDATE_END;
	}

	/*
	 * fw_bin[1] = Length ( OPCode + Data )
	 */
	fw_len = fw_bin[1];

	/*
	 * Check fw data length
	 * We support 0x22 or 0x04 only
	 */
	if (fw_len != 0x22 && fw_len != 0x04)
		return FW_UPDATE_MAX_LENGTH_FAIL;

	/*
	 * fw_bin[2] = OPCode
	 */
	fw_opcode = fw_bin[2];

	/*
	 * In case write command,
	 * fw_bin[35:3] = Data
	 *
	 * In case read command,
	 * fw_bin[5:3]  = Data
	 */
	fw_data_len = fw_len - 1; /* exclude opcode */
	memcpy(fw_data, &fw_bin[3], fw_data_len);

	switch (fw_cmd) {
	case FW_CMD_WRITE:
		if (fw_data_len > I2C_SMBUS_BLOCK_MAX) {
			/* write the half data */
			max77729_bulk_write(max77729->muic,
					fw_opcode,
					I2C_SMBUS_BLOCK_HALF,
					fw_data);
			max77729_bulk_write(max77729->muic,
					fw_opcode + I2C_SMBUS_BLOCK_HALF,
					fw_data_len - I2C_SMBUS_BLOCK_HALF,
					&fw_data[I2C_SMBUS_BLOCK_HALF]);
		} else
			max77729_bulk_write(max77729->muic,
					fw_opcode,
					fw_data_len,
					fw_data);

		ret = max77729_usbc_wait_response(max77729);
		if (ret)
			return ret;

		/*
		 * Why do we need 1ms sleep in case MQ81?
		 */
		/* msleep(1); */

		return FW_CMD_WRITE_SIZE;

	case FW_CMD_READ:
		max77729_bulk_read(max77729->muic,
				fw_opcode,
				fw_data_len,
				verify_data);
		/*
		 * Check fw data sequence number
		 * It should be increased from 1 step by step.
		 */
		if (memcmp(verify_data, &fw_data[1], 2)) {
			pr_info("[0x%02x 0x%02x], [0x%02x, 0x%02x], [0x%02x, 0x%02x]",
					verify_data[0], fw_data[0],
					verify_data[1], fw_data[1],
					verify_data[2], fw_data[2]);
			return FW_UPDATE_VERIFY_FAIL;
		}

		return FW_CMD_READ_SIZE;
	}

	pr_info("Command error");

	return ret;
}


static int max77729_fuelgauge_read_vcell(struct max77729_dev *max77729)
{
	u8 data[2];
	u32 vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (max77729_bulk_read(max77729->fuelgauge, VCELL_REG, 2, data) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1] << 8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vcell += (temp2 << 4);

	return vcell;
}

int max77729_usbc_fw_update(struct max77729_dev *max77729,
		const u8 *fw_bin, int fw_bin_len, int enforce_do)
{
	max77729_fw_header *fw_header;
	int offset = 0;
	unsigned long duration = 0;
	int size = 0;
	int try_count = 0;
	int ret = 0;
	u8 try_command = 0;
	u8 chg_cnfg_00 = 0;
	bool chg_mode_changed = 0;
	int vcell = 0;
	u8 chgin_dtls = 0;
	u8 wcin_dtls = 0;
	int error = 0;

	max77729->fw_size = fw_bin_len;
	fw_header = (max77729_fw_header *)fw_bin;
	pr_info("FW: magic/%x/ major/%x/ minor/%x/ product_id/%x/ rev/%x/ id/%x/",
		  fw_header->magic, fw_header->major, fw_header->minor, fw_header->product_id, fw_header->rev, fw_header->id);
	/* if(max77729->device_product_id != fw_header->product_id) { */
		/* pr_info("product indicator mismatch"); */
		/* return 0; */
	/* } */
	if(fw_header->magic == MAX77729_SIGN)
		pr_info("FW: matched");

	max77729_read_reg(max77729->charger, MAX77729_CHG_REG_CNFG_00, &chg_cnfg_00);
retry:
	disable_irq(max77729->irq);
	max77729_write_reg(max77729->muic, REG_PD_INT_M, 0xFF);
	max77729_write_reg(max77729->muic, REG_CC_INT_M, 0xFF);
	max77729_write_reg(max77729->muic, REG_UIC_INT_M, 0xFF);
	max77729_write_reg(max77729->muic, REG_VDM_INT_M, 0xFF);

	offset = 0;
	duration = 0;
	size = 0;
	ret = 0;

	/* to do (unmask interrupt) */
	ret = max77729_read_reg(max77729->muic, REG_UIC_FW_REV, &max77729->FW_Revision);
	ret = max77729_read_reg(max77729->muic, REG_UIC_FW_MINOR, &max77729->FW_Minor_Revision);
	if (ret < 0 && (try_count == 0 && try_command == 0)) {
		pr_info("Failed to read FW_REV");
		error = -EIO;
		goto out;
	}

	duration = jiffies;

	max77729->FW_Product_ID = max77729->FW_Minor_Revision;
	max77729->FW_Minor_Revision &= MINOR_VERSION_MASK;
	pr_info("chip : %02X.%02X(PID 0x%x), FW : %02X.%02X(PID 0x%x)",
			max77729->FW_Revision, max77729->FW_Minor_Revision, max77729->FW_Product_ID,
			fw_header->major, fw_header->minor, fw_header->product_id);

	if ((max77729->FW_Revision != fw_header->major) || (max77729->FW_Minor_Revision != fw_header->minor)) {
		if (try_count == 0 && try_command == 0) {
			/* change chg_mode during FW update */
			vcell = max77729_fuelgauge_read_vcell(max77729);

			if (vcell < 3600) {
				pr_info("%s: keep chg_mode(0x%x), vcell(%dmv)\n",
					__func__, chg_cnfg_00 & 0x0F, vcell);
				error = -EAGAIN;
				goto out;
			}
		}

		max77729_read_reg(max77729->charger, MAX77729_CHG_REG_DETAILS_00, &wcin_dtls);
		wcin_dtls = (wcin_dtls & 0x18) >> 3;


		max77729_read_reg(max77729->charger, MAX77729_CHG_REG_DETAILS_00, &chgin_dtls);

		chgin_dtls = ((chgin_dtls & 0x60) >> 5);

		/* pr_info("%s: chgin_dtls:0x%x, wcin_dtls:0x%x\n", */
			/* __func__, chgin_dtls, wcin_dtls); */

		if ((chgin_dtls != 0x3) && (wcin_dtls != 0x3)) {
			chg_mode_changed = true;
					/* Switching Frequency : 3MHz */
			max77729_update_reg(max77729->charger,
						MAX77729_CHG_REG_CNFG_08, 0x00,	0x3);
			/* pr_info("%s: +set Switching Frequency 3Mhz\n", __func__); */

					/* Disable skip mode */
			max77729_update_reg(max77729->charger,
						MAX77729_CHG_REG_CNFG_12, 0x1, 0x1);
			/* pr_info("%s: +set Disable skip mode\n", __func__); */

			max77729_update_reg(max77729->charger,
						MAX77729_CHG_REG_CNFG_00, 0x09, 0x0F);
			/* pr_info("%s: +change chg_mode(0x9), vcell(%dmv)\n", */
						/* __func__, vcell); */
		} else {
			if (chg_mode_changed) {
				chg_mode_changed = false;
				/* Auto skip mode */
				max77729_update_reg(max77729->charger,
					MAX77729_CHG_REG_CNFG_12, 0x0, 0x1);
				/* pr_info("%s: -set Auto skip mode\n", __func__); */

				max77729_update_reg(max77729->charger,
					MAX77729_CHG_REG_CNFG_12, 0x0, 0x20);
				/* pr_info("%s: -disable CHGINSEL\n", __func__); */

				max77729_update_reg(max77729->charger,
					MAX77729_CHG_REG_CNFG_00, 0x4, 0x0F);
				/* pr_info("%s: -set chg_mode(0x4)\n", __func__); */

				max77729_update_reg(max77729->charger,
					MAX77729_CHG_REG_CNFG_12, 0x20, 0x20);
				/* pr_info("%s: -enable CHGINSEL\n", __func__); */

				max77729_update_reg(max77729->charger,
					MAX77729_CHG_REG_CNFG_00, chg_cnfg_00, 0x0F);
				/* pr_info("%s: -recover chg_mode(0x%x), vcell(%dmv)\n", */
					/* __func__, chg_cnfg_00 & 0x0F, vcell); */

				/* Switching Frequency : 1.5MHz */
				max77729_update_reg(max77729->charger,
					MAX77729_CHG_REG_CNFG_08, 0x02, 0x3);

				/* pr_info("%s: -set Switching Frequency 1.5MHz\n", __func__); */
			}
		}
		msleep(500);

		max77729_write_reg(max77729->muic, 0x21, 0xD0);
		max77729_write_reg(max77729->muic, 0x41, 0x00);
		msleep(500);

		max77729_read_reg(max77729->muic, REG_UIC_FW_REV, &max77729->FW_Revision);
		max77729_read_reg(max77729->muic, REG_UIC_FW_MINOR, &max77729->FW_Minor_Revision);
		max77729->FW_Minor_Revision &= MINOR_VERSION_MASK;
		pr_info("Start FW updating (%02X.%02X)", max77729->FW_Revision, max77729->FW_Minor_Revision);

		if (max77729->FW_Revision != 0xFF) {
			if (++try_command < FW_SECURE_MODE_TRY_COUNT) {
				pr_info("the Fail to enter secure mode %d",
						try_command);
				max77729_reset_ic(max77729);
				msleep(1000);
				goto retry;
			} else {
				pr_info("the Secure Update Fail!!");
				error = -EIO;
				goto out;
			}
		}

		try_command = 0;

		for (offset = FW_HEADER_SIZE;
				offset < fw_bin_len && size != FW_UPDATE_END;) {

			size = __max77729_usbc_fw_update(max77729, &fw_bin[offset]);

			switch (size) {
			case FW_UPDATE_VERIFY_FAIL:
				offset -= FW_CMD_WRITE_SIZE;
				/* FALLTHROUGH */
			case FW_UPDATE_TIMEOUT_FAIL:
				/*
				 * Retry FW updating
				 */
				if (++try_count < FW_VERIFY_TRY_COUNT) {
					pr_info("Retry fw write. ret %d, count %d, offset %d",
							size, try_count, offset);
					max77729_reset_ic(max77729);
					msleep(1000);
					goto retry;
				} else {
					pr_info("Failed to update FW. ret %d, offset %d",
							size, (offset + size));
					error = -EIO;
					goto out;
				}
				break;
			case FW_UPDATE_CMD_FAIL:
			case FW_UPDATE_MAX_LENGTH_FAIL:
				pr_info("Failed to update FW. ret %d, offset %d",
						size, (offset + size));
				error = -EIO;
				goto out;
			case FW_UPDATE_END: /* 0x00 */
				/* JIG PIN for setting HIGH. */
				max77729_read_reg(max77729->muic,
						REG_UIC_FW_REV, &max77729->FW_Revision);
				max77729_read_reg(max77729->muic,
						REG_UIC_FW_MINOR, &max77729->FW_Minor_Revision);
				max77729->FW_Minor_Revision &= MINOR_VERSION_MASK;
				pr_info("chip : %02X.%02X, FW : %02X.%02X",
						max77729->FW_Revision, max77729->FW_Minor_Revision,
						fw_header->major, fw_header->minor);
				pr_info("Completed");
				break;
			default:
				offset += size;
				break;
			}
			if (offset == fw_bin_len) {
				max77729_reset_ic(max77729);
				max77729_read_reg(max77729->muic,
						REG_UIC_FW_REV, &max77729->FW_Revision);
				max77729_read_reg(max77729->muic,
						REG_UIC_FW_MINOR, &max77729->FW_Minor_Revision);
				max77729->FW_Minor_Revision &= MINOR_VERSION_MASK;
				pr_info("chip : %02X.%02X, FW : %02X.%02X",
						max77729->FW_Revision, max77729->FW_Minor_Revision,
						fw_header->major, fw_header->minor);
				pr_info("Completed via SYS path");
			}
		}
	} else {
		pr_info("Don't need to update!");
		goto out;
	}

	duration = jiffies - duration;
	/* pr_info("Duration : %dms", jiffies_to_msecs(duration)); */
out:
	if (chg_mode_changed) {
		vcell = max77729_fuelgauge_read_vcell(max77729);
		/* Auto skip mode */
		max77729_update_reg(max77729->charger,
			MAX77729_CHG_REG_CNFG_12, 0x0, 0x1);
		pr_info("%s: -set Auto skip mode\n", __func__);

		max77729_update_reg(max77729->charger,
			MAX77729_CHG_REG_CNFG_12, 0x0, 0x20);
		pr_info("%s: -disable CHGINSEL\n", __func__);

		max77729_update_reg(max77729->charger,
			MAX77729_CHG_REG_CNFG_00, 0x4, 0x0F);
		pr_info("%s: -set chg_mode(0x4)\n", __func__);

		max77729_update_reg(max77729->charger,
			MAX77729_CHG_REG_CNFG_12, 0x20, 0x20);
		pr_info("%s: -enable CHGINSEL\n", __func__);

		max77729_update_reg(max77729->charger,
			MAX77729_CHG_REG_CNFG_00, chg_cnfg_00, 0x0F);
		pr_info("%s: -recover chg_mode(0x%x), vcell(%dmv)\n",
			__func__, chg_cnfg_00 & 0x0F, vcell);

		/* Switching Frequency : 1.5MHz */
		max77729_update_reg(max77729->charger,
			MAX77729_CHG_REG_CNFG_08, 0x02, 0x3);
		pr_info("%s: -set Switching Frequency 1.5MHz\n", __func__);
	}

	enable_irq(max77729->irq);
	return error;
}
EXPORT_SYMBOL_GPL(max77729_usbc_fw_update);

void max77729_usbc_fw_setting(struct max77729_dev *max77729, int enforce_do)
{
    max77729_usbc_fw_update(max77729, BOOT_FLASH_FW_PASS2,  ARRAY_SIZE(BOOT_FLASH_FW_PASS2), enforce_do);
}
EXPORT_SYMBOL_GPL(max77729_usbc_fw_setting);


static int max77729_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct max77729_dev *max77729;
	struct max77729_platform_data *pdata = i2c->dev.platform_data;
	int ret = 0;
	u8 pmic_id, pmic_rev = 0;

	/* pr_info("%s:%s\n", MFD_DEV_NAME, __func__); */

	max77729 = kzalloc(sizeof(struct max77729_dev), GFP_KERNEL);
	if (!max77729)
		return -ENOMEM;

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct max77729_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err;
		}

		ret = of_max77729_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	max77729->dev = &i2c->dev;
	max77729->i2c = i2c;
	max77729->irq = i2c->irq;
	if (pdata) {
		max77729->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, MAX77729_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
					MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			max77729->irq_base = pdata->irq_base;

		max77729->irq_gpio = pdata->irq_gpio;
		max77729->wakeup = pdata->wakeup;
		max77729->blocking_waterevent = pdata->blocking_waterevent;
		max77729->device_product_id = pdata->fw_product_id;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&max77729->i2c_lock);

	max77729->suspended = false;
	init_waitqueue_head(&max77729->suspend_wait);

	i2c_set_clientdata(i2c, max77729);

	if (max77729_read_reg(i2c, MAX77729_PMIC_REG_PMICID1, &pmic_id) < 0) {
		dev_err(max77729->dev, "device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	}
	if (max77729_read_reg(i2c, MAX77729_PMIC_REG_PMICREV, &pmic_rev) < 0) {
		dev_err(max77729->dev, "device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	}

	pr_info("%s:%s pmic_id:%x, pmic_rev:%x\n",
		MFD_DEV_NAME, __func__, pmic_id, pmic_rev);

	max77729->pmic_rev = pmic_rev;
	if (max77729->pmic_rev == 0) {
		dev_err(max77729->dev, "Can not find matched revision\n");
		ret = -ENODEV;
		goto err_w_lock;
	}
	max77729->pmic_ver = ((pmic_rev & 0xF8) >> 0x3);

	nopmi_set_charger_ic_type(NOPMI_CHARGER_IC_MAXIM);

	/* print rev */
	pr_info("%s:%s device found: rev:%x ver:%x\n",
		MFD_DEV_NAME, __func__, max77729->pmic_rev, max77729->pmic_ver);

	init_completion(&max77729->fw_completion);
	max77729->fw_workqueue = create_singlethread_workqueue("fw_update");
	if (max77729->fw_workqueue == NULL)
		return -ENOMEM;
	INIT_WORK(&max77729->fw_work, max77729_usbc_wait_response_q);
	init_waitqueue_head(&max77729->queue_empty_wait_q);
    //Brandon it should be modifed based on Xiaomi platform
	/* max77729->muic = i2c_new_dummy(i2c->adapter, I2C_ADDR_MUIC); */

	max77729->muic = i2c_new_dummy(i2c->adapter, I2C_ADDR_MUIC);
	i2c_set_clientdata(max77729->muic, max77729);

	max77729->charger = i2c_new_dummy(i2c->adapter, I2C_ADDR_CHG);
	i2c_set_clientdata(max77729->charger, max77729);

	max77729->fuelgauge = i2c_new_dummy(i2c->adapter, I2C_ADDR_FG);
	i2c_set_clientdata(max77729->fuelgauge, max77729);

	max77729_usbc_fw_setting(max77729, 0);

	max77729->debug = i2c_new_dummy(i2c->adapter, I2C_ADDR_DEBUG);
	i2c_set_clientdata(max77729->debug, max77729);
	{
 	struct pinctrl *max_pinctrl = NULL;
	struct pinctrl_state *max_gpio_default= NULL;

 	max_pinctrl = devm_pinctrl_get(max77729->dev);
	if (IS_ERR_OR_NULL(max_pinctrl)) {
		dev_err(max77729->dev, "No pinctrl config specified\n");
	   ret = PTR_ERR(max77729->dev);
		/* return rc; */
	}
	max_gpio_default =
		pinctrl_lookup_state(max_pinctrl, "maxim_int_default");
	if (IS_ERR_OR_NULL(max_gpio_default)) {
		dev_err(max77729->dev, "No active config specified\n");
		ret = PTR_ERR(max_gpio_default);
		/* return rc; */
	}

 	ret = pinctrl_select_state(max_pinctrl,
			max_gpio_default);
	if (ret < 0) {
		dev_err(max77729->dev, "fail to select pinctrl active rc=%d\n",
				ret);
		/* return ret; */
	}
	}
	disable_irq(max77729->irq);
	ret = max77729_irq_init(max77729);

	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(max77729->dev, -1, max77729_devs,
			ARRAY_SIZE(max77729_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(max77729->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(max77729->dev);
err_irq_init:
	i2c_unregister_device(max77729->muic);
	i2c_unregister_device(max77729->charger);
	i2c_unregister_device(max77729->fuelgauge);
	i2c_unregister_device(max77729->debug);
err_w_lock:
	mutex_destroy(&max77729->i2c_lock);
err:
	kfree(max77729);
	return ret;
}

static int max77729_i2c_remove(struct i2c_client *i2c)
{
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);

	device_init_wakeup(max77729->dev, 0);
	max77729_irq_exit(max77729);
	mfd_remove_devices(max77729->dev);
	i2c_unregister_device(max77729->muic);
	i2c_unregister_device(max77729->charger);
	i2c_unregister_device(max77729->fuelgauge);
	i2c_unregister_device(max77729->debug);
	kfree(max77729);

	return 0;
}

static const struct i2c_device_id max77729_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_MAX77729 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77729_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id max77729_i2c_dt_ids[] = {
	{ .compatible = "maxim,max77729" },
	{ },
};
MODULE_DEVICE_TABLE(of, max77729_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int max77729_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);

	/* pr_info("%s:%s\n", MFD_DEV_NAME, __func__); */

	if (device_may_wakeup(dev))
		enable_irq_wake(max77729->irq);

	max77729->suspended =  true;

	wait_event_interruptible_timeout(max77729->queue_empty_wait_q,
					(!max77729->doing_irq) && (!max77729->is_usbc_queue), 1*HZ);
	return 0;
}

static int max77729_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	max77729->suspended =  false;
	wake_up(&max77729->suspend_wait);


	/* pr_info("%s:%s\n", MFD_DEV_NAME, __func__); */

	if (device_may_wakeup(dev))
		disable_irq_wake(max77729->irq);
	return 0;
}
#else
#define max77729_suspend	NULL
#define max77729_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION


static int max77729_freeze(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int i;

	disable_irq(max77729->irq);

	return 0;
}

static int max77729_restore(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77729_dev *max77729 = i2c_get_clientdata(i2c);
	int i;

	enable_irq(max77729->irq);

	return 0;
}
#endif

const struct dev_pm_ops max77729_pm = {
	.suspend = max77729_suspend,
	.resume = max77729_resume,
#ifdef CONFIG_HIBERNATION
	.freeze =  max77729_freeze,
	.thaw = max77729_restore,
	.restore = max77729_restore,
#endif
};

static struct i2c_driver max77729_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &max77729_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= max77729_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= max77729_i2c_probe,
	.remove		= max77729_i2c_remove,
	.id_table	= max77729_i2c_id,
};

static int __init max77729_i2c_init(void)
{
	/* pr_info("%s:%s\n", MFD_DEV_NAME, __func__); */
	return i2c_add_driver(&max77729_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(max77729_i2c_init);

static void __exit max77729_i2c_exit(void)
{
	i2c_del_driver(&max77729_i2c_driver);
}
module_exit(max77729_i2c_exit);

MODULE_DESCRIPTION("Max77705 MFD driver");
MODULE_LICENSE("GPL");
