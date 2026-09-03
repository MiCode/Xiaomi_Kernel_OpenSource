/*
 * RX driver for nu1619 wireless charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power/charger-manager.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/usb/phy.h>
#include "rx1619.h"

/* used registers define */
#define NU1619_RX_REG_00		0x0000
#define NU1619_RX_REG_01		0x0001
#define NU1619_RX_REG_02		0x0002
#define NU1619_RX_REG_03		0x0003
#define NU1619_RX_REG_04		0x0004
#define NU1619_RX_REG_05		0x0005
#define NU1619_RX_REG_06		0x0006
#define NU1619_RX_REG_07		0x0007
#define NU1619_RX_REG_08		0x0008
#define NU1619_RX_REG_09		0x0009
#define NU1619_RX_REG_0A		0x000A
#define NU1619_RX_REG_0B		0x000B
#define NU1619_RX_REG_0C		0x000C
#define NU1619_RX_REG_0D		0x000D
#define NU1619_RX_REG_10		0x0010
#define NU1619_RX_REG_11		0x0011
#define NU1619_RX_REG_12		0x0012
#define NU1619_RX_REG_13		0x0013
#define NU1619_RX_REG_14		0x0014
#define NU1619_RX_REG_15		0x0015
#define NU1619_RX_REG_16		0x0016
#define NU1619_RX_REG_17		0x0017
#define NU1619_RX_REG_18		0x0018
#define NU1619_RX_REG_1A		0x001A
#define NU1619_RX_REG_20		0x0020
#define NU1619_RX_REG_21		0x0021
#define NU1619_RX_REG_22		0x0022
#define NU1619_RX_REG_23		0x0023
#define NU1619_RX_REG_24		0x0024
#define NU1619_RX_REG_1000		0x1000
#define NU1619_RX_REG_1130		0x1130
#define NU1619_RX_REG_2017		0x2017
#define NU1619_RX_REG_2018		0x2018
#define NU1619_RX_REG_2019		0x2019

#define NU1619_RX_CMD_WR_TO_TX		0x01
#define NU1619_RX_CMD_SET_VOLT		0x02
#define NU1619_RX_CMD_CLEAR_INT		0x08
#define NU1619_RX_CMD_SET_VTX		0x09
#define NU1619_RX_CMD_READ_CEP		0x95
#define NU1619_RX_CMD_RPP_TYPE		0x9D
#define NU1619_RX_CMD_READ_VRECT	0xAD
#define NU1619_RX_CMD_READ_VOUT		0xAB
#define NU1619_RX_CMD_READ_IOUT		0xAF

#define NU1619_RX_MODE			0x10
#define NU1619_TX_MODE			0x20

#define NU1619_RX_VTX_MAX		21000
#define NU1619_RX_VTX_MIN		4000
#define NU1619_RX_VTX_DEFAULT		6000
#define NU1619_RX_VOUT_MAX		21000
#define NU1619_RX_VOUT_MIN		4000
#define NU1619_RX_VOUT_DEFAULT		6000
#define NU1619_RX_VDROP_MV		100

#define NU1619_RX_INT_OVER_CURR			BIT(0)
#define NU1619_RX_INT_OVER_VOLT			BIT(1)
#define NU1619_RX_INT_OVER_TEMP			BIT(2)
#define NU1619_RX_INT_REMAIN_VOL		BIT(3)
#define NU1619_RX_INT_TX_DATA_RCVD		BIT(4)
#define NU1619_RX_INT_MODE_CHANGED		BIT(5)
#define NU1619_RX_INT_LDO_ON			BIT(6)
#define NU1619_RX_INT_LDO_OFF			BIT(7)
#define NU1619_RX_INT_TX_EPP_CAP		BIT(8)
#define NU1619_RX_INT_SEND_PKT_TIMEOUT		BIT(10)
#define NU1619_RX_INT_SEND_PKT_SUCCESS		BIT(11)
#define NU1619_RX_INT_SLEEP_MODE		BIT(14)

#define NU1619_RX_WORK_CYCLE_MS			2000
#define NU1619_RPP_TYPE_BPP			(0x04)
#define NU1619_RPP_TYPE_EPP			(0x31)

static u8 fod_param[8] = {
	0x44, 0x1E,
	0x44, 0x37,
	0x3A, 0x46,
	0x37, 0x5A
};

static u8 g_fw_boot_id;
static u8 g_fw_rx_id;
static u8 g_fw_tx_id;
static u8 g_hw_id_h;
static u8 g_hw_id_l;

static u32 g_fw_data_length;

#define BOOT_AREA   0
#define RX_AREA     1
#define TX_AREA     2

struct nu1619_wl_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_nu1619_rx_chip_cep;
	struct device_attribute attr_nu1619_rx_chip_vrect;
	struct device_attribute attr_nu1619_rx_chip_firmware_update;
	struct device_attribute attr_nu1619_rx_chip_version;
	struct device_attribute attr_nu1619_rx_chip_vout;
	struct device_attribute attr_nu1619_rx_chip_vtx;
	struct device_attribute attr_nu1619_rx_chip_iout;
	struct device_attribute attr_nu1619_rx_chip_debug;
	struct device_attribute attr_nu1619_rx_chip_fod_parameter;
	struct attribute *attrs[10];

	struct nu1619_rx *info;
};

struct nu1619_rx {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap       *regmap;
	unsigned int irq_gpio;
	unsigned int switch_chg_en_gpio;
	unsigned int switch_flag_en_gpio;
	unsigned int power_good_gpio;
	int chip_enable;
	int online;
	struct delayed_work    wireless_work;
	struct delayed_work    wireless_int_work;
	struct mutex    wireless_chg_lock;
	struct mutex    wireless_chg_int_lock;
	struct power_supply    *wip_psy;

	bool is_charging;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	u32 limit;

	struct nu1619_wl_charger_sysfs *sysfs;
};

static struct nu1619_rx *g_chip;

static int nu1619_rx_read(struct nu1619_rx *chip, u8 *val, u16 addr)
{
	unsigned int temp = 0;
	int rc;

	rc = regmap_read(chip->regmap, addr, &temp);
	if (rc) {
		dev_err(chip->dev, "Fail to read [rx1619] [0x%04x]\n", addr);
		return rc;
	}

	*val = (u8)temp;

	return rc;
}

static int nu1619_rx_write(struct nu1619_rx *chip, u8 val, u16 addr)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, addr, val);
	if (rc)
		dev_err(chip->dev, "Fail to write [rx1619] [0x%04x] = [0x%x]\n", addr, val);

	return rc;
}

static int nu1619_read_rx_buffer(struct nu1619_rx *chip, u8 *buf, u16 addr, u32 size)
{
    return regmap_bulk_read(chip->regmap, addr, buf, size);
}

static void write_cmd_d(struct nu1619_rx *chip, u8 val)
{
	static u8 cmd_d;

	cmd_d &= 0x20;
	cmd_d |= val;
	cmd_d ^= 0x20;
	nu1619_rx_write(chip, cmd_d, NU1619_RX_REG_0D);
}

/*
 * return value:
 *	BPP 0x04
 *	EPP 0x31
 */
static u8 nu1619_rx_get_rx_rpp_type(struct nu1619_rx *chip)
{
	u8 rpp_type = 0;
	u8 read_buffer[3] = {0};

	nu1619_rx_write(chip, NU1619_RX_CMD_RPP_TYPE, NU1619_RX_REG_0C);
	nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);
	if (read_buffer[0] == (NU1619_RX_CMD_RPP_TYPE ^ 0x80))
		rpp_type = read_buffer[1];

	return rpp_type;
}

static int nu1619_rx_read_C_team_reg(struct nu1619_rx *chip, u8 cmd, u8 *data1, u8 *data2)
{
	u8 read_buffer[3] = {0};
	u8 rty_count = 0;
	int ret;

	*data1 = 0;
	*data2 = 0;

retry:
	ret = nu1619_rx_write(chip, cmd, NU1619_RX_REG_0C);
	if (ret)
		return ret;

	ret = nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);

	if (read_buffer[0] == (cmd ^ 0x80)) {
		*data1 = read_buffer[1];
		*data2 = read_buffer[2];
	} else {
		if (rty_count < 3) {
			dev_info(chip->dev, "rty_count=%d\n", rty_count);
			msleep(20);
			rty_count++;
			goto retry;
		}
	}

	dev_info(chip->dev, "0x000C: cmd=0x%x , 0x0008=0x%x, 0x0009=0x%x, 0x000a=0x%x\n",
		 cmd, read_buffer[0], read_buffer[1], read_buffer[2]);

	return ret;
}

static unsigned int nu1619_rx_get_rx_vout(struct nu1619_rx *chip)
{
	u16 vout = 0;
	u8 data1, data2;
	int ret;

	ret = nu1619_rx_read_C_team_reg(chip, NU1619_RX_CMD_READ_VOUT, &data1, &data2);
	vout = (data2 << 8) + data1;

	dev_info(chip->dev, "--%s----0x000C->0xab----data1=0x%x, data2=0x%x vout=%d\n",
		 __func__, data1, data2, vout);

	return vout;
}

static unsigned int nu1619_rx_get_rx_iout(struct nu1619_rx *chip)
{
	u16 iout = 0;
	u8 data1, data2;
	int ret;

	ret = nu1619_rx_read_C_team_reg(chip, NU1619_RX_CMD_READ_IOUT, &data1, &data2);
	iout = (data2 << 8) + data1;

	dev_info(chip->dev, "--%s----0x000C->0xaf----data1=0x%x, data2=0x%x iout=%d\n",
		 __func__, data1, data2, iout);

	return iout;
}

static unsigned int nu1619_rx_get_rx_vrect(struct nu1619_rx *chip)
{
	u16 vrect = 0;
	u8 data1, data2;
	int ret;

	ret = nu1619_rx_read_C_team_reg(chip, NU1619_RX_CMD_READ_VRECT, &data1, &data2);
	vrect = (data2 << 8) + data1;

	dev_info(chip->dev, "--%s----0x000C->0xad----data1=0x%x, data2=0x%x, vrect=%d\n",
		 __func__, data1, data2, vrect);

	return vrect;
}

static int nu1619_rx_set_vtx(struct nu1619_rx *chip, u32 volt)
{
	u8 value_h, value_l, ret;

	if ((volt < NU1619_RX_VTX_MIN) || (volt > NU1619_RX_VTX_MAX))
		volt = NU1619_RX_VTX_DEFAULT;

	dev_info(chip->dev, "[rx1619] [%s] Vtx = %d\n", __func__, volt);

	value_h = (u8)(volt >> 8);
	value_l = (u8)(volt & 0xFF);
	ret = nu1619_rx_write(chip, value_h, NU1619_RX_REG_01);
	if (ret)
		return ret;

	ret = nu1619_rx_write(chip, value_l, NU1619_RX_REG_00);
	write_cmd_d(chip, NU1619_RX_CMD_SET_VTX);

	dev_info(chip->dev, "[rx1619] [%s] Vtx value_h = 0x%x, value_l=0x%x\n",
		 __func__, value_h, value_l);

	return ret;
}

static int nu1619_rx_set_vout(struct nu1619_rx *chip, u32 volt)
{
	u8 vout_h, vout_l, vrect_h, vrect_l, ret;

	if ((volt < NU1619_RX_VOUT_MIN) || (volt > NU1619_RX_VOUT_MAX))
		volt = NU1619_RX_VOUT_DEFAULT;

	dev_info(chip->dev, "[rx1619] [%s] volt = %d\n", __func__, volt);

	vout_h = (u8)(volt >> 8);
	vout_l = (u8)(volt & 0xFF);
	ret = nu1619_rx_write(chip, vout_h, NU1619_RX_REG_00);
	if (ret)
		return ret;

	ret = nu1619_rx_write(chip, vout_l, NU1619_RX_REG_01);
	if (ret)
		return ret;

	vrect_h = (u8)((volt + NU1619_RX_VDROP_MV) >> 8);
	vrect_l = (u8)((volt + NU1619_RX_VDROP_MV) & 0xFF);
	ret = nu1619_rx_write(chip, vrect_h, NU1619_RX_REG_02);
	if (ret)
		return ret;

	ret = nu1619_rx_write(chip, vrect_l, NU1619_RX_REG_03);

	write_cmd_d(chip, NU1619_RX_CMD_SET_VOLT);

	dev_info(chip->dev, "[rx1619] [%s] vout_h = 0x%x, vout_l=0x%x, vrect_h = 0x%x, vrect_l=0x%x\n",
		 __func__, vout_h, vout_l, vrect_h, vrect_l);

	return ret;
}

static u8 nu1619_rx_get_cep_value(struct nu1619_rx *chip)
{
	int cep = 0;
	u8 read_buffer[3] = {0};

	nu1619_rx_write(chip, NU1619_RX_CMD_READ_CEP, NU1619_RX_REG_0C);
	nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);

	if (read_buffer[0] == (NU1619_RX_CMD_READ_CEP ^ 0x80))
		cep = read_buffer[1];

	dev_info(chip->dev, "--%s----0x000C->0x95------0x0008=0x%x, 0x0009=0x%x, 0x000a=0x%x cep=%d\n",
		__func__, read_buffer[0], read_buffer[1], read_buffer[2], cep);

	return cep;
}

static int nu1619_rx_enter_write_mode(struct nu1619_rx *chip)
{
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_write(chip, 0x01, NU1619_RX_REG_17);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x30, NU1619_RX_REG_1000);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x5a, NU1619_RX_REG_1A);

	return ret;
}

static int nu1619_rx_exit_write_mode(struct nu1619_rx *chip)
{
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_1A);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_17);

	return ret;
}

static int nu1619_rx_enter_read_mode(struct nu1619_rx *chip)
{
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_write(chip, 0x01, NU1619_RX_REG_17);

	return ret;
}

static int nu1619_rx_exit_read_mode(struct nu1619_rx *chip)
{
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_17);

	return ret;
}

static int nu1619_rx_read_pause(struct nu1619_rx *chip)
{
	int ret = 0;

	ret = nu1619_rx_write(chip, 0x02, NU1619_RX_REG_18);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_18);

	return ret;
}

static int nu1619_rx_write_firmware_data(struct nu1619_rx *chip, u16 addr, u16 length)
{
	const char *fw_data = NULL;
	u8  addr_h, addr_l;
	int i = 0;
	u32 count = 0;
	int ret = 0;

	/* write_mtp_addr */
	if (addr == 0)
		fw_data = fw_data_boot;
	if (addr == 256)
		fw_data = fw_data_rx;
	if (addr == 4864)
		fw_data = fw_data_tx;

	if (!fw_data) {
		dev_err(chip->dev, "[rx1619] [%s] fw_data is null\n", __func__);
		return -EINVAL;
	}

	dev_info(chip->dev, "[rx1619] [%s] addr=%d length=%d\n", __func__, addr, length);

	addr_h = (u8)(addr >> 8);
	addr_l = (u8)(addr & 0xff);
	ret = nu1619_rx_write(chip, addr_h, NU1619_RX_REG_10);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, addr_l, NU1619_RX_REG_11);
	if (ret < 0)
		return ret;

	/* write_mtp_addr */
	ret = nu1619_rx_enter_write_mode(chip);
	if (ret < 0)
		return ret;

	/* write data */
	for (i = 0; i < length; i += 4) {
		ret = nu1619_rx_write(chip, fw_data[i+3], NU1619_RX_REG_12);
		if (ret < 0)
			return ret;
		usleep_range(900, 1000);

		ret = nu1619_rx_write(chip, fw_data[i+2], NU1619_RX_REG_12);
		if (ret < 0)
			return ret;
		usleep_range(900, 1000);

		ret = nu1619_rx_write(chip, fw_data[i+1], NU1619_RX_REG_12);
		if (ret < 0)
			return ret;
		usleep_range(900, 1000);

		ret = nu1619_rx_write(chip, fw_data[i+0], NU1619_RX_REG_12);
		if (ret < 0)
			return ret;
		usleep_range(900, 1000);
	}
	/* write data */

	ret = nu1619_rx_exit_write_mode(chip);

	dev_info(chip->dev, "[rx1619] [%s] count=%d\n", __func__, count);

	return ret;
}

static int nu1619_rx_config_gpio_1V8(struct nu1619_rx *chip)
{
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_write(chip, 0x10, NU1619_RX_REG_1000);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x3e, NU1619_RX_REG_1130);

	return ret;
}

static int nu1619_rx_enter_dtm_mode(struct nu1619_rx *chip)
{
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_write(chip, 0x69, NU1619_RX_REG_2017);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x96, NU1619_RX_REG_2017);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x66, NU1619_RX_REG_2017);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x99, NU1619_RX_REG_2017);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0xff, NU1619_RX_REG_2018);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0xff, NU1619_RX_REG_2019);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x5a, NU1619_RX_REG_01);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0xa5, NU1619_RX_REG_03);

	return ret;
}

static int nu1619_rx_exit_dtm_mode(struct nu1619_rx *chip)
{
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_2018);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_2019);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x00, NU1619_RX_REG_03);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_write(chip, 0x55, NU1619_RX_REG_2017);

	return ret;
}

static bool nu1619_req_checksum_and_fw_version(struct nu1619_rx *chip, u8 *boot_check_sum,
					       u8 *rx_check_sum, u8 *tx_check_sum,
					       u8 *boot_ver, u8 *rx_ver, u8 *tx_ver)
{
	u8 read_buffer1[5] = {0};
	u8 read_buffer2[5] = {0};

	nu1619_rx_write(chip, 0x21, NU1619_RX_REG_0D);
	msleep(20);

	nu1619_read_rx_buffer(chip, read_buffer1, NU1619_RX_REG_08, 3);
	dev_info(chip->dev, "check sum value: 0x0008,9,a = 0x%x,0x%x,0x%x\n",
		 read_buffer1[0], read_buffer1[1], read_buffer1[2]);
	nu1619_read_rx_buffer(chip, read_buffer2, NU1619_RX_REG_20, 5);
	dev_info(chip->dev, "0x0020,21,22,23,24 = 0x%x,0x%x,0x%x,0x%x,0x%x\n",
		 read_buffer2[0], read_buffer2[1], read_buffer2[2],
		 read_buffer2[3], read_buffer2[4]);

	*boot_check_sum = read_buffer1[0];
	*rx_check_sum = read_buffer1[1];
	*tx_check_sum = read_buffer1[2];
	*boot_ver = read_buffer2[2];
	*rx_ver = read_buffer2[1];
	*tx_ver = read_buffer2[0];

	g_fw_boot_id = *boot_ver;
	g_fw_rx_id = *rx_ver;
	g_fw_tx_id = *tx_ver;
	g_hw_id_h = read_buffer2[3];
	g_hw_id_l = read_buffer2[4];

	return true;
}

static bool nu1619_rx_check_firmware_version(struct nu1619_rx *chip)
{
	bool g_boot_no_need_update = false;
	bool g_rx_no_need_update = false;
	bool g_tx_no_need_update = false;
	u8 boot_checksum, rx_checksum, tx_checksum, boot_version, rx_version, tx_version;

	nu1619_req_checksum_and_fw_version(chip, &boot_checksum, &rx_checksum, &tx_checksum,
					   &boot_version, &rx_version, &tx_version);
	dev_info(chip->dev, "MTP check_sum: 0x%x,0x%x,0x%x  Version:0x%x,0x%x,0x%x\n",
		 boot_checksum, rx_checksum, tx_checksum, boot_version, rx_version, tx_version);

	dev_info(chip->dev, "boot ver: (0xff&(~fw_data_boot[1020-5]))=0x%x\n",
		 (0xff&(~fw_data_boot[1020-5])));
	dev_info(chip->dev, "rx ver: (0xff&(~fw_data_rx[18428-5]))=0x%x\n",
		 (0xff&(~fw_data_rx[18428-5])));
	dev_info(chip->dev, "tx ver: (0xff&(~fw_data_tx[13308-5]))=0x%x\n",
		 (0xff&(~fw_data_tx[13308-5])));

	if ((boot_version >= (0xff&(~fw_data_boot[1020-5]))) && (boot_checksum == 0x66)) {
		g_boot_no_need_update = true;
		dev_info(chip->dev, "[rx1619] [%s] g_boot_no_need_update = true\n", __func__);
	}

	if ((rx_version >= (0xff&(~fw_data_rx[18428-5]))) && (rx_checksum == 0x66)) {
		g_rx_no_need_update = true;
		dev_info(chip->dev, "[rx1619] [%s] g_rx_no_need_update = true\n", __func__);
	}

	if ((tx_version >= (0xff&(~fw_data_tx[13308-5]))) && (tx_checksum == 0x66)) {
		g_tx_no_need_update = true;
		dev_info(chip->dev, "[rx1619] [%s] g_tx_no_need_update = true\n", __func__);
	}

	dev_info(chip->dev, "g_boot_no_need_update=%d, g_rx_no_need_update=%d, g_tx_no_need_update=%d\n",
		 g_boot_no_need_update, g_rx_no_need_update, g_tx_no_need_update);

	if ((g_boot_no_need_update) && (g_rx_no_need_update) && (g_tx_no_need_update))
		return true;

	return false;
}

static u32 nu1619_rx_check_firmware_area(struct nu1619_rx *chip, u8 area)
{
	u16 addr = 0;
	u8  addr_h, addr_l;
	int i = 0;
	int j = 0;
	u8 read_buf[4] = {0, 0, 0, 0};
	const char *fw_data = NULL;
	u32 success_count = 0;
	int ret = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	if (area == BOOT_AREA) {
		addr = 0;
		g_fw_data_length = sizeof(fw_data_boot);
		fw_data = fw_data_boot;
	} else if (area == RX_AREA) {
		/* 1024...19452 */
		addr = 256;
		g_fw_data_length = sizeof(fw_data_rx);
		fw_data = fw_data_rx;
	} else if (area == TX_AREA) {
		/* 19456 */
		addr = 4864;
		g_fw_data_length = sizeof(fw_data_tx);
		fw_data = fw_data_tx;
	}

	if (!fw_data) {
		dev_err(chip->dev, "[rx1619] [%s] fw_data is null\n", __func__);
		return false;
	}

	/* prepare_for_mtp_read */
	ret = nu1619_rx_enter_dtm_mode(chip);
	if (ret < 0)
		return ret;

	ret = nu1619_rx_enter_read_mode(chip);
	if (ret < 0)
		return ret;
	/* prepare_for_mtp_read */

	msleep(20);

	for (i = 0; i < g_fw_data_length; i += 4) {
		/* write_mtp_addr */
		addr_h = (u8)(addr >> 8);
		addr_l = (u8)(addr & 0xff);
		nu1619_rx_write(chip, addr_h, NU1619_RX_REG_10);
		nu1619_rx_write(chip, addr_l, NU1619_RX_REG_11);
		/* write_mtp_addr */

		addr++;

		/* read pause */
		ret = nu1619_rx_read_pause(chip);
		if (ret < 0)
			return ret;
		/* read pause */

		/* read data */
		nu1619_rx_read(chip, &read_buf[3], NU1619_RX_REG_13);
		nu1619_rx_read(chip, &read_buf[2], NU1619_RX_REG_14);
		nu1619_rx_read(chip, &read_buf[1], NU1619_RX_REG_15);
		nu1619_rx_read(chip, &read_buf[0], NU1619_RX_REG_16);
		/* read data */

		if ((read_buf[0] == fw_data[i+0]) &&
			(read_buf[1] == fw_data[i+1]) &&
			(read_buf[2] == fw_data[i+2]) &&
			(read_buf[3] == fw_data[i+3])) {
			success_count++;
		} else {
			j++;
			/* if error addr >= 50, new IC */
			if (j >= 50)
				goto check_fw_end;
		}
	}

check_fw_end:
	/* end */
	ret = nu1619_rx_exit_read_mode(chip);
	if (ret < 0)
		return ret;

	/* exit dtm */
	ret = nu1619_rx_exit_dtm_mode(chip);
	if (ret < 0)
		return ret;

	/* exit dtm */
	dev_info(chip->dev, "error_conut= %d, success_count=%d\n", j, success_count);

	if (g_fw_data_length == (success_count*4))
		return true;

	return false;
}

static bool nu1619_rx_download_firmware_data(struct nu1619_rx *chip, u8 area)
{
	int ret;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	ret = nu1619_rx_enter_dtm_mode(chip);
	if (ret < 0)
		return false;

	ret = nu1619_rx_config_gpio_1V8(chip);
	if (ret < 0)
		return false;

	msleep(20);

	if (area == BOOT_AREA) {
		ret = nu1619_rx_write_firmware_data(chip, 0, sizeof(fw_data_boot));
		if (ret < 0)
			return false;
	}

	if (area == RX_AREA) {
		ret = nu1619_rx_write_firmware_data(chip, 256, sizeof(fw_data_rx));
		if (ret < 0)
			return false;
	}

	if (area == TX_AREA) {
		ret = nu1619_rx_write_firmware_data(chip, 4864, sizeof(fw_data_tx));
		if (ret < 0)
			return false;
	}

	ret = nu1619_rx_exit_dtm_mode(chip);
	if (ret < 0)
		return false;

	dev_info(chip->dev, "[rx1619] [%s] exit\n", __func__);

	return true;
}

static bool nu1619_rx_check_i2c_is_ok(struct nu1619_rx *chip)
{
	u8 read_data = 0;

	nu1619_rx_write(chip, 0x88, NU1619_RX_REG_00);
	msleep(20);
	nu1619_rx_read(chip, &read_data, NU1619_RX_REG_00);

	if (read_data == 0x88)
		return true;

	return false;
}

#define RETRY_COUNT 2
static bool nu1619_rx_download_firmware_area(struct nu1619_rx *chip, u8 area)
{
	bool status = false;

	if (area == BOOT_AREA)
		g_fw_data_length = sizeof(fw_data_boot);
	else if (area == RX_AREA)
		g_fw_data_length = sizeof(fw_data_rx);
	else if (area == TX_AREA)
		g_fw_data_length = sizeof(fw_data_tx);

	status = nu1619_rx_download_firmware_data(chip, area);
	if (!status)
		return false;

	return true;
}

static bool nu1619_rx_download_firmware(struct nu1619_rx *chip)
{
	bool status = false;

	status = nu1619_rx_download_firmware_area(chip, BOOT_AREA);
	if (!status) {
		dev_err(chip->dev, "BOOT_AREA download fail!!!\n");
		return false;
	}

	msleep(20);

	status = nu1619_rx_download_firmware_area(chip, RX_AREA);
	if (!status) {
		dev_err(chip->dev, "RX_AREA download fail!!!\n");
		return false;
	}

	msleep(20);

	status = nu1619_rx_download_firmware_area(chip, TX_AREA);
	if (!status) {
		dev_err(chip->dev, "TX_AREA download fail!!!\n");
		return false;
	}

	return true;
}

static bool nu1619_rx_check_firmware(struct nu1619_rx *chip)
{
	bool status = false;

	status = nu1619_rx_check_firmware_area(chip, BOOT_AREA);
	if (!status) {
		dev_err(chip->dev, "%s BOOT_AREA fail!!!\n", __func__);
		return false;
	}

	status = nu1619_rx_check_firmware_area(chip, RX_AREA);
	if (!status) {
		dev_err(chip->dev, "%s RX_AREA fail!!!\n", __func__);
		return false;
	}

	status = nu1619_rx_check_firmware_area(chip, TX_AREA);
	if (!status) {
		dev_err(chip->dev, "%s TX_AREA fail!!!\n", __func__);
		return false;
	}

	return true;
}

static bool nu1619_rx_onekey_download_firmware(struct nu1619_rx *chip)
{
	bool status = false;

	dev_info(chip->dev, "[rx1619] [%s] enter\n", __func__);

	if (!nu1619_rx_check_i2c_is_ok(chip)) {
		dev_err(chip->dev, " i2c error!\n");
		return false;
	}

	status = nu1619_rx_check_firmware_version(chip);
	if (status) {
		dev_err(chip->dev, "NO need download fw!!!\n");
		return true;
	}

	status = nu1619_rx_download_firmware(chip);
	if (!status) {
		dev_err(chip->dev, "nu1619_rx_download_firmware fail!!!\n");
		return false;
	}

	dev_err(chip->dev, "***********************reset power*************************\n");

	msleep(100);

	status = nu1619_rx_check_firmware(chip);
	if (!status) {
		dev_err(chip->dev, "nu1619_rx_check_firmware fail!!!\n");
		return false;
	}

	return true;
}

static void nu1619_rx_dump_reg(struct nu1619_rx *chip)
{
	u8 data[32] = {0};

	nu1619_rx_read(chip, &data[0], NU1619_RX_REG_00);
	nu1619_rx_read(chip, &data[1], NU1619_RX_REG_01);
	nu1619_rx_read(chip, &data[2], NU1619_RX_REG_02);
	nu1619_rx_read(chip, &data[3], NU1619_RX_REG_03);
	nu1619_rx_read(chip, &data[4], NU1619_RX_REG_04);
	nu1619_rx_read(chip, &data[5], NU1619_RX_REG_05);
	nu1619_rx_read(chip, &data[6], NU1619_RX_REG_08);
	nu1619_rx_read(chip, &data[7], NU1619_RX_REG_09);
	nu1619_rx_read(chip, &data[8], NU1619_RX_REG_0A);
	nu1619_rx_read(chip, &data[9], NU1619_RX_REG_0B);
	nu1619_rx_read(chip, &data[10], NU1619_RX_REG_0C);
	nu1619_rx_read(chip, &data[11], NU1619_RX_REG_0D);
	nu1619_rx_read(chip, &data[12], NU1619_RX_REG_20);
	nu1619_rx_read(chip, &data[13], NU1619_RX_REG_21);
	nu1619_rx_read(chip, &data[14], NU1619_RX_REG_22);
	nu1619_rx_read(chip, &data[15], NU1619_RX_REG_23);
	nu1619_rx_read(chip, &data[16], NU1619_RX_REG_24);

	dev_info(chip->dev, "reave--[rx1619] [%s] REG:0x0000=0x%x\n",
		 __func__, data[0]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0001=0x%x\n",
		 __func__, data[1]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0002=0x%x\n",
		 __func__, data[2]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0003=0x%x\n",
		 __func__, data[3]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0004=0x%x\n",
		 __func__, data[4]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0005=0x%x\n",
		 __func__, data[5]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0008=0x%x\n",
		 __func__, data[6]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0009=0x%x\n",
		 __func__, data[7]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x000A=0x%x\n",
		 __func__, data[8]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x000B=0x%x\n",
		 __func__, data[9]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x000C=0x%x\n",
		 __func__, data[10]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x000D=0x%x\n",
		 __func__, data[11]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0020=0x%x\n",
		 __func__, data[12]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0021=0x%x\n",
		 __func__, data[13]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0022=0x%x\n",
		 __func__, data[14]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0023=0x%x\n",
		 __func__, data[15]);
	dev_info(chip->dev, "[rx1619] [%s] REG:0x0024=0x%x\n",
		 __func__, data[16]);
}

static void nu1619_rx_transparent_data(struct nu1619_rx *chip, u8 cmd, u8 value0,
				       u8 value1, u8 value2, u8 value3)
{
	u8 cmd_data = 0;
	u8 data[4] = {0, 0, 0, 0};

	cmd_data = cmd;
	data[0] = value0;
	data[1] = value1;
	data[2] = value2;
	data[3] = value3;

	nu1619_rx_write(chip, cmd_data, NU1619_RX_REG_00);
	nu1619_rx_write(chip, data[0], NU1619_RX_REG_01);
	nu1619_rx_write(chip, data[1], NU1619_RX_REG_02);
	nu1619_rx_write(chip, data[2], NU1619_RX_REG_03);
	nu1619_rx_write(chip, data[3], NU1619_RX_REG_04);

	write_cmd_d(chip, NU1619_RX_CMD_WR_TO_TX);
}

static void nu1619_rx_wireless_work(struct work_struct *work)
{
	struct nu1619_rx *chip = container_of(work, struct nu1619_rx, wireless_work.work);
	u8 rpp_type;

	mutex_lock(&chip->wireless_chg_lock);
	rpp_type = nu1619_rx_get_rx_rpp_type(chip);

	if (rpp_type == NU1619_RPP_TYPE_BPP || rpp_type == NU1619_RPP_TYPE_EPP) {
		schedule_delayed_work(&chip->wireless_work,
				      msecs_to_jiffies(NU1619_RX_WORK_CYCLE_MS));
		mutex_unlock(&chip->wireless_chg_lock);
	} else {
		dev_info(chip->dev, "%s, stop wireless charge, rpp_type = 0x%x\n",
			 __func__, rpp_type);
		chip->is_charging = false;
		chip->online = 0;
		gpio_set_value(chip->switch_flag_en_gpio, 0);
		mutex_unlock(&chip->wireless_chg_lock);
		cm_notify_event(chip->wip_psy, CM_EVENT_WL_CHG_START_STOP, NULL);
	}
}

static void nu1619_rx_wireless_int_work(struct work_struct *work)
{
	u8 rx_rev_data[4] = {0, 0, 0, 0};
	u8 read_buffer[4] = {0, 0, 0, 0};
	u16 int_flag = 0;
	u8 rpp_type;

	struct nu1619_rx *chip = container_of(work, struct nu1619_rx, wireless_int_work.work);

	mutex_lock(&chip->wireless_chg_int_lock);

	nu1619_rx_read(chip, &rx_rev_data[0], NU1619_RX_REG_23);
	nu1619_rx_read(chip, &rx_rev_data[1], NU1619_RX_REG_24);
	dev_info(chip->dev, "[rx1619] [%s] 0x0024,0x0023 =0x%x,0x%x\n",
		 __func__, rx_rev_data[1], rx_rev_data[0]);

	int_flag = rx_rev_data[0] + (rx_rev_data[1] << 8);
	dev_info(chip->dev, "[rx1619] [%s] int_flag =0x%x\n", __func__, int_flag);

	if (int_flag & NU1619_RX_INT_OVER_CURR) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x01, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}

	if (int_flag & NU1619_RX_INT_OVER_VOLT) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x02, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}

	if (int_flag & NU1619_RX_INT_OVER_TEMP) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x04, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	if (int_flag & NU1619_RX_INT_TX_EPP_CAP) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x08, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	if (int_flag & NU1619_RX_INT_TX_DATA_RCVD) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x10, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	if (int_flag & NU1619_RX_INT_MODE_CHANGED) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x20, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	if (int_flag & NU1619_RX_INT_LDO_ON) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x40, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
		gpio_set_value(chip->switch_chg_en_gpio, 0);
		gpio_set_value(chip->switch_flag_en_gpio, 1);
		chip->online = 1;
	}
	if (int_flag & NU1619_RX_INT_LDO_OFF) {
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x80, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
		chip->online = 0;
	}
	/* dev auth failed */
	if (int_flag & 0x0100) {
		nu1619_rx_write(chip, 0x01, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	/* dev auth success */
	if (int_flag & 0x0200) {
		nu1619_rx_write(chip, 0x02, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	/* power on */
	if (int_flag & 0x0400) {
		dev_info(chip->dev, "%s power on\n", __func__);
		nu1619_rx_write(chip, 0x04, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);

		msleep(20);

		if (gpio_get_value(chip->irq_gpio) == 0)
			dev_err(chip->dev, "[rx1619] irq_gpio is low level\n");
		else
			dev_err(chip->dev, "[rx1619] irq_gpio is high level\n");
	}
	/* rx ready */
	if (int_flag & 0x0800) {
		dev_info(chip->dev, "[rx1619] [%s] int_flag & 0x0800\n", __func__);

		nu1619_rx_write(chip, 0x08, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	/* send pkt timeout */
	if (int_flag & 0x1000) {
		dev_info(chip->dev, "[rx1619] [%s] int_flag & 0x1000\n", __func__);

		nu1619_rx_write(chip, 0x10, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}
	/* send pkt success */
	if (int_flag & 0x2000) {
		dev_err(chip->dev, "[rx1619] [%s] int_flag & 0x2000\n", __func__);

		nu1619_rx_write(chip, 0x20, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
		/* thansparent */
		nu1619_rx_write(chip, 0x83, NU1619_RX_REG_0C);
		nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);
		if ((0x83 ^ 0x80) == read_buffer[0])
			dev_err(chip->dev, "0x83---0x000C, 0x0008=0x%x, 0x0009=0x%x, 0x000a=0x%x\n",
				read_buffer[0], read_buffer[1], read_buffer[2]);

		nu1619_rx_write(chip, 0x85, NU1619_RX_REG_0C);
		nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);
		if ((0x85 ^ 0x80) == read_buffer[0])
			dev_err(chip->dev, "0x85---0x000C, 0x0008=0x%x, 0x0009=0x%x, 0x000a=0x%x\n",
				read_buffer[0], read_buffer[1], read_buffer[2]);

		nu1619_rx_write(chip, 0x87, NU1619_RX_REG_0C);
		nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);
		if ((0x87 ^ 0x80) == read_buffer[0])
			dev_err(chip->dev, "0x87---0x000C, 0x0008=0x%x, 0x0009=0x%x, 0x000a=0x%x\n",
				read_buffer[0], read_buffer[1], read_buffer[2]);

		nu1619_rx_write(chip, 0x89, NU1619_RX_REG_0C);
		nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);
		if ((0x89 ^ 0x80) == read_buffer[0])
			dev_err(chip->dev, "0x89---0x000C, 0x0008=0x%x, 0x0009=0x%x, 0x000a=0x%x\n",
				read_buffer[0], read_buffer[1], read_buffer[2]);

		nu1619_rx_write(chip, 0x8a, NU1619_RX_REG_0C);
		nu1619_read_rx_buffer(chip, read_buffer, NU1619_RX_REG_08, 3);
		if ((0x8a ^ 0x80) == read_buffer[0])
			dev_err(chip->dev, "0x8a---0x000C, 0x0008=0x%x, 0x0009=0x%x, 0x000a=0x%x\n",
				read_buffer[0], read_buffer[1], read_buffer[2]);
	}
	/* id auth failed */
	if (int_flag & 0x4000) {
		nu1619_rx_write(chip, 0x40, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);

	}
	/* id auth success */
	if (int_flag & 0x8000) {
		nu1619_rx_write(chip, 0x80, NU1619_RX_REG_01);
		nu1619_rx_write(chip, 0x00, NU1619_RX_REG_00);
		write_cmd_d(chip, NU1619_RX_CMD_CLEAR_INT);
	}

	rpp_type = nu1619_rx_get_rx_rpp_type(chip);
	if (rpp_type != NU1619_RPP_TYPE_BPP && rpp_type != NU1619_RPP_TYPE_EPP) {
		dev_info(chip->dev, "%s, stop wl charge, rpp_type = 0x%x\n", __func__, rpp_type);
		cancel_delayed_work_sync(&chip->wireless_work);
		chip->online = 0;
		gpio_set_value(chip->switch_flag_en_gpio, 0);
	}

	mutex_unlock(&chip->wireless_chg_int_lock);

	dev_info(chip->dev, "wireless charger online = %d, is_charging = %d\n",
		 chip->online, chip->is_charging);
	if ((!chip->is_charging && chip->online == 1) ||
	    (chip->is_charging && chip->online == 0)) {
		chip->is_charging = !chip->is_charging;
		cm_notify_event(chip->wip_psy, CM_EVENT_WL_CHG_START_STOP, NULL);
		schedule_delayed_work(&chip->wireless_work,
				      msecs_to_jiffies(NU1619_RX_WORK_CYCLE_MS));
	}
}

static irqreturn_t nu1619_rx_chg_stat_handler(int irq, void *dev_id)
{
	struct nu1619_rx *chip = dev_id;

	schedule_delayed_work(&chip->wireless_int_work, 0);

	return IRQ_HANDLED;
}

static int nu1619_rx_parse_dt(struct nu1619_rx *chip)
{
	struct device_node *node = chip->dev->of_node;
	int ret;

	if (!node) {
		dev_err(chip->dev, "[rx1619] [%s] No DT data Failing Probe\n", __func__);
		return -EINVAL;
	}

	chip->irq_gpio = of_get_named_gpio(node, "rx,irq_gpio", 0);
	if (!gpio_is_valid(chip->irq_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_irq_gpio %d\n",
			__func__, chip->irq_gpio);
		return -EINVAL;
	}

	chip->switch_chg_en_gpio  = of_get_named_gpio(node, "switch_chg_en_gpio", 0);
	if (!gpio_is_valid(chip->switch_chg_en_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_switch_chg_en_gpio %d\n",
			__func__, chip->switch_chg_en_gpio);
		return -EINVAL;
	}
	ret = devm_gpio_request_one(chip->dev,  chip->switch_chg_en_gpio,
				   GPIOF_DIR_OUT | GPIOF_INIT_LOW,
				   "switch_chg_en_gpio");
	if (ret) {
		dev_err(chip->dev, "init switch chg en gpio fail\n");
		return -EINVAL;
	}

	chip->switch_flag_en_gpio  = of_get_named_gpio(node, "switch_flag_en_gpio", 0);
	if (!gpio_is_valid(chip->switch_flag_en_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_switch_flag_en_gpio %d\n",
			__func__, chip->switch_flag_en_gpio);
		return -EINVAL;
	}
	ret = devm_gpio_request_one(chip->dev,  chip->switch_flag_en_gpio,
				   GPIOF_OUT_INIT_HIGH, "switch_flag_en_gpio");
	if (ret) {
		dev_err(chip->dev, "init switch flag en gpio fail\n");
		return -EINVAL;
	}


	gpio_set_value(chip->switch_chg_en_gpio, 0);
	gpio_set_value(chip->switch_flag_en_gpio, 0);

	return ret;
}

static int nu1619_rx_gpio_init(struct nu1619_rx *chip)
{
	int ret = 0;

	if (gpio_is_valid(chip->irq_gpio)) {
		ret = devm_gpio_request_one(chip->dev, chip->irq_gpio,
					    GPIOF_DIR_IN,
					    "nu1619_wireless_charge_int");
		chip->client->irq = gpio_to_irq(chip->irq_gpio);
		if (chip->client->irq < 0) {
			dev_err(chip->dev, "[rx1619] [%s] gpio_to_irq Fail!\n", __func__);
			goto fail_irq_gpio;
		}
	} else {
		dev_err(chip->dev, "%s: irq gpio not provided\n", __func__);
		goto fail_irq_gpio;
	}

	return ret;


fail_irq_gpio:
	gpio_free(chip->irq_gpio);

	return ret;
}

static ssize_t chip_cep_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	u8 cep_value, rpp_type;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return scnprintf(buf, PAGE_SIZE, "chip is null\n");

	cep_value = nu1619_rx_get_cep_value(chip);

	rpp_type = nu1619_rx_get_rx_rpp_type(chip);

	return sprintf(buf, "rx1619 cep : 0x%x , rpp_type = 0x%x\n", cep_value, rpp_type);
}

static ssize_t chip_vrect_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	unsigned int vrect = 0;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return scnprintf(buf, PAGE_SIZE, "chip is null\n");

	vrect = nu1619_rx_get_rx_vrect(chip);

	return sprintf(buf, "rx1619 Vrect : %d mV\n", vrect);
}

static ssize_t chip_vout_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	unsigned int vout = 0;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return scnprintf(buf, PAGE_SIZE, "chip is null\n");

	vout = nu1619_rx_get_rx_vout(chip);

	return sprintf(buf, "rx1619 Vout : %d mV\n", vout);
}

static ssize_t chip_iout_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	unsigned int iout = 0;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return scnprintf(buf, PAGE_SIZE, "chip is null\n");

	iout = nu1619_rx_get_rx_iout(chip);

	return sprintf(buf, "rx1619 Iout: %d mA\n", iout);
}

static ssize_t chip_vtx_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t count)
{
	int ret;
	unsigned long index = 0;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return count;

	ret = kstrtoul(buf, 10, &index);
	if (ret)
		dev_err(chip->dev, "%s failed\n", __func__);

	dev_info(chip->dev, "[rx1619] [%s] --Store output_voltage = %ld\n",
		 __func__, index);
	if ((index < 4000) || (index > 21000)) {
		dev_err(chip->dev, "[rx1619] [%s] Store Voltage %s is invalid\n",
			__func__, buf);
		nu1619_rx_set_vtx(chip, 0);
		return count;
	}

	nu1619_rx_set_vtx(chip, (u32)index);

	return count;
}

static ssize_t chip_vout_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	int ret;
	unsigned long index = 0;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return count;

	ret = kstrtoul(buf, 10, &index);
	if (ret)
		dev_err(chip->dev, "%s failed\n", __func__);

	dev_info(chip->dev, "[rx1619] [%s], Store output_voltage = %ld\n",
		 __func__, index);
	if ((index < 4000) || (index > 21000)) {
		dev_err(chip->dev, "[rx1619] [%s] Store Voltage %s is invalid\n",
			__func__, buf);
		nu1619_rx_set_vout(chip, (u32)index);
		return count;
	}

	return count;
}

static ssize_t chip_debug_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return scnprintf(buf, PAGE_SIZE, "chip is null\n");

	nu1619_rx_dump_reg(chip);
	msleep(100);
	nu1619_rx_get_rx_vout(chip);
	nu1619_rx_get_rx_vrect(chip);
	nu1619_rx_get_rx_iout(chip);

	return sprintf(buf, "chip debug show\n");
}

static ssize_t chip_debug_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t count)
{
	int ret;
	unsigned long index = 0;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return count;

	ret = kstrtoul(buf, 10, &index);
	if (ret)
		dev_err(chip->dev, "%s failed\n", __func__);

	if (index == 0)
		cancel_delayed_work_sync(&chip->wireless_work);
	else if (index == 200)
		schedule_delayed_work(&chip->wireless_work, 0);

	return count;
}

static ssize_t chip_fod_parameter_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	int ret, i;
	unsigned long val;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return count;

	for (i = 0; i < 8; i++) {
		val = 0;
		ret = kstrtoul(&buf[i * 3], 16, &val);
		if (ret) {
			dev_err(chip->dev, "%s store fod_param[%d] failed\n",
				__func__, i);
			return ret;
		}
		fod_param[i] = (u8)val;
	}

	nu1619_rx_transparent_data(chip, fod_param[0], fod_param[1],
				   fod_param[2], fod_param[3], fod_param[4]);

	dev_info(chip->dev, "[rx1619] [%s] exit\n", __func__);

	return count;
}

static ssize_t chip_firmware_update_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	bool status = false;
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return scnprintf(buf, PAGE_SIZE, "chip is null\n");

	dev_info(chip->dev, "[sp6001] [%s] Firmware Update begin\n", __func__);

	status = nu1619_rx_onekey_download_firmware(chip);
	if (!status) {
		dev_err(chip->dev, "[rx1619] [%s] Firmware Update failed! Please try again!\n",
			__func__);
		return scnprintf(buf, PAGE_SIZE, "Firmware Update failed! Please try again!\n");

	} else {
		dev_info(chip->dev, "Firmware Update Success!!!"
			 "boot_fw=0x%02x, rx_fw=0x%02x, tx_fw=0x%02x\n",
			 g_fw_boot_id, g_fw_rx_id, g_fw_tx_id);

		dev_info(chip->dev, "[rx1619] [%s] Firmware Update Success!!!\n", __func__);
		return scnprintf(buf, PAGE_SIZE, "Firmware Update Success!!!"
				 "rx_fw=0x%02x, =0x%02x, tx_fw=0x%02x\n",
				 g_fw_boot_id, g_fw_rx_id, g_fw_tx_id);
	}
}

static ssize_t chip_version_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct nu1619_rx *chip = g_chip;

	if (!chip)
		return scnprintf(buf, PAGE_SIZE, "chip is null\n");

	nu1619_rx_check_firmware_version(chip);

	return scnprintf(buf, PAGE_SIZE, "boot_fw=V%02x, rx_fw=V%02x, tx_fw=V%02x, chip:Nu%x%x\n",
			 g_fw_boot_id, g_fw_rx_id, g_fw_tx_id, g_hw_id_h, g_hw_id_l);
}

static enum power_supply_property nu1619_wireless_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_TYPE,
};

static int nu1619_wireless_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	int ret = 0;
	struct nu1619_rx *chip = power_supply_get_drvdata(psy);

	if (!chip)
		return -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = nu1619_rx_set_vout(chip, (u32)val->intval);
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int nu1619_wireless_get_property(struct power_supply *psy,
					enum power_supply_property prop,
					union power_supply_propval *val)
{
	int ret = 0;
	u8 rpp_type;
	struct nu1619_rx *chip = power_supply_get_drvdata(psy);

	if (!chip)
		return -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = nu1619_rx_get_rx_vrect(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = nu1619_rx_get_rx_iout(chip);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = nu1619_rx_get_rx_vout(chip);
		break;
	case POWER_SUPPLY_PROP_TYPE:
		rpp_type = nu1619_rx_get_rx_rpp_type(chip);
		if (rpp_type == NU1619_RPP_TYPE_BPP)
			val->intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP;
		else if (rpp_type == NU1619_RPP_TYPE_EPP)
			val->intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP;
		else
			val->intval = POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN;
		break;
	default:
		ret =  -EINVAL;
	}

	return ret;
}

static const struct regmap_config nu1619_rx_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static int nu1619_prop_is_writeable(struct power_supply *psy,
				    enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		rc = 1;
		break;
	default:
		rc = 0;
	}

	return rc;
}

static const struct power_supply_desc nu1619_wireless_charger_desc = {
	.name			= "nu1619_wireless_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= nu1619_wireless_properties,
	.num_properties		= ARRAY_SIZE(nu1619_wireless_properties),
	.get_property		= nu1619_wireless_get_property,
	.set_property		= nu1619_wireless_set_property,
	.property_is_writeable	= nu1619_prop_is_writeable,
};

static int nu1619_rx_register_sysfs(struct nu1619_rx *info)
{
	struct nu1619_wl_charger_sysfs *nu1619_sysfs;
	int ret;

	nu1619_sysfs = devm_kzalloc(info->dev, sizeof(*nu1619_sysfs), GFP_KERNEL);
	if (!nu1619_sysfs)
		return -EINVAL;

	info->sysfs = nu1619_sysfs;
	nu1619_sysfs->name = "nu1619_sysfs";
	nu1619_sysfs->info = info;
	nu1619_sysfs->attrs[0] = &nu1619_sysfs->attr_nu1619_rx_chip_cep.attr;
	nu1619_sysfs->attrs[1] = &nu1619_sysfs->attr_nu1619_rx_chip_vrect.attr;
	nu1619_sysfs->attrs[2] = &nu1619_sysfs->attr_nu1619_rx_chip_firmware_update.attr;
	nu1619_sysfs->attrs[3] = &nu1619_sysfs->attr_nu1619_rx_chip_version.attr;
	nu1619_sysfs->attrs[4] = &nu1619_sysfs->attr_nu1619_rx_chip_vout.attr;
	nu1619_sysfs->attrs[5] = &nu1619_sysfs->attr_nu1619_rx_chip_vtx.attr;
	nu1619_sysfs->attrs[6] = &nu1619_sysfs->attr_nu1619_rx_chip_iout.attr;
	nu1619_sysfs->attrs[7] = &nu1619_sysfs->attr_nu1619_rx_chip_debug.attr;
	nu1619_sysfs->attrs[8] = &nu1619_sysfs->attr_nu1619_rx_chip_fod_parameter.attr;
	nu1619_sysfs->attrs[9] = NULL;
	nu1619_sysfs->attr_g.name = "debug";
	nu1619_sysfs->attr_g.attrs = nu1619_sysfs->attrs;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_cep.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_cep.attr.name = "nu1619_rx_chip_cep";
	nu1619_sysfs->attr_nu1619_rx_chip_cep.attr.mode = 0444;
	nu1619_sysfs->attr_nu1619_rx_chip_cep.show = chip_cep_show;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_vrect.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_vrect.attr.name = "nu1619_rx_chip_vrect";
	nu1619_sysfs->attr_nu1619_rx_chip_vrect.attr.mode = 0444;
	nu1619_sysfs->attr_nu1619_rx_chip_vrect.show = chip_vrect_show;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_firmware_update.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_firmware_update.attr.name = "nu1619_rx_chip_firmware_update";
	nu1619_sysfs->attr_nu1619_rx_chip_firmware_update.attr.mode = 0644;
	nu1619_sysfs->attr_nu1619_rx_chip_firmware_update.show = chip_firmware_update_show;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_version.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_version.attr.name = "nu1619_rx_chip_version";
	nu1619_sysfs->attr_nu1619_rx_chip_version.attr.mode = 0644;
	nu1619_sysfs->attr_nu1619_rx_chip_version.show = chip_version_show;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_vout.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_vout.attr.name = "nu1619_rx_chip_vout";
	nu1619_sysfs->attr_nu1619_rx_chip_vout.attr.mode = 0644;
	nu1619_sysfs->attr_nu1619_rx_chip_vout.show = chip_vout_show;
	nu1619_sysfs->attr_nu1619_rx_chip_vout.store = chip_vout_store;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_vtx.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_vtx.attr.name = "nu1619_rx_chip_vtx";
	nu1619_sysfs->attr_nu1619_rx_chip_vtx.attr.mode = 0200;
	nu1619_sysfs->attr_nu1619_rx_chip_vtx.store = chip_vtx_store;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_iout.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_iout.attr.name = "nu1619_rx_chip_iout";
	nu1619_sysfs->attr_nu1619_rx_chip_iout.attr.mode = 0444;
	nu1619_sysfs->attr_nu1619_rx_chip_iout.show = chip_iout_show;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_debug.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_debug.attr.name = "nu1619_rx_chip_debug";
	nu1619_sysfs->attr_nu1619_rx_chip_debug.attr.mode = 0644;
	nu1619_sysfs->attr_nu1619_rx_chip_debug.show = chip_debug_show;
	nu1619_sysfs->attr_nu1619_rx_chip_debug.store = chip_debug_store;

	sysfs_attr_init(&nu1619_sysfs->attr_nu1619_rx_chip_fod_parameter.attr);
	nu1619_sysfs->attr_nu1619_rx_chip_fod_parameter.attr.name = "nu1619_rx_chip_fod_parameter";
	nu1619_sysfs->attr_nu1619_rx_chip_fod_parameter.attr.mode = 0200;
	nu1619_sysfs->attr_nu1619_rx_chip_fod_parameter.store = chip_fod_parameter_store;

	ret = sysfs_create_group(&info->wip_psy->dev.kobj, &nu1619_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static int nu1619_rx_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *data)
{
	struct nu1619_rx *chip = container_of(nb, struct nu1619_rx, usb_notify);

	chip->limit = limit;

	if (chip->limit) {
		gpio_set_value(chip->switch_chg_en_gpio, 0);
		gpio_set_value(chip->switch_flag_en_gpio, 0);
	}

	return NOTIFY_OK;
}

static void nu1619_rx_detect_status(struct nu1619_rx *chip)
{
	u32 min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	if (chip->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(chip->usb_phy, &min, &max);
	chip->limit = min;

	if (chip->limit) {
		gpio_set_value(chip->switch_chg_en_gpio, 0);
		gpio_set_value(chip->switch_flag_en_gpio, 0);
	}
}

static int nu1619_rx_charger_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct power_supply_config wip_psy_cfg = {};
	struct device *dev = &client->dev;
	struct nu1619_rx *chip;
	int ret = 0;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(dev, "i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	chip->regmap = regmap_init_i2c(client, &nu1619_rx_regmap_config);
	if (IS_ERR(chip->regmap)) {
		dev_err(&client->dev, "parent regmap is missing\n");
		return PTR_ERR(chip->regmap);
	}

	chip->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(chip->usb_phy)) {
		dev_err(dev, "failed to find USB phy\n");
		return -EPROBE_DEFER;
	}

	chip->client = client;
	chip->dev = dev;

	chip->chip_enable = false;

	device_init_wakeup(dev, true);
	i2c_set_clientdata(client, chip);

	nu1619_rx_parse_dt(chip);
	nu1619_rx_gpio_init(chip);

	mutex_init(&chip->wireless_chg_lock);
	mutex_init(&chip->wireless_chg_int_lock);
	INIT_DELAYED_WORK(&chip->wireless_work, nu1619_rx_wireless_work);
	INIT_DELAYED_WORK(&chip->wireless_int_work, nu1619_rx_wireless_int_work);

	wip_psy_cfg.drv_data = chip;
	wip_psy_cfg.of_node = dev->of_node;

	chip->wip_psy = devm_power_supply_register(dev,
						   &nu1619_wireless_charger_desc,
						   &wip_psy_cfg);
	if (IS_ERR(chip->wip_psy)) {
		dev_err(dev, "Couldn't register wip psy rc=%ld\n", PTR_ERR(chip->wip_psy));
		ret = PTR_ERR(chip->wip_psy);
		goto error_mutex;
	}

	if (chip->client->irq) {
		ret = devm_request_threaded_irq(&chip->client->dev, chip->client->irq, NULL,
						nu1619_rx_chg_stat_handler, IRQF_TRIGGER_FALLING |
						 IRQF_TRIGGER_RISING | IRQF_ONESHOT,
						"nu1619_rx_chg_stat_irq", chip);
		if (ret) {
			dev_err(chip->dev, "Failed irq = %d ret = %d\n", chip->client->irq, ret);
			goto error_irq;
		}

		enable_irq_wake(chip->client->irq);
	}

	chip->usb_notify.notifier_call = nu1619_rx_usb_change;
	ret = usb_register_notifier(chip->usb_phy, &chip->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		goto error_irq;
	}

	ret = nu1619_rx_register_sysfs(chip);
	if (ret) {
		dev_err(chip->dev, "register sysfs fail, ret = %d\n", ret);
		goto error_sysfs;
	}

	nu1619_rx_detect_status(chip);
	g_chip = chip;

	return 0;

error_sysfs:
	usb_unregister_notifier(chip->usb_phy, &chip->usb_notify);
error_irq:
	power_supply_unregister(chip->wip_psy);
error_mutex:
	mutex_destroy(&chip->wireless_chg_lock);
	mutex_destroy(&chip->wireless_chg_int_lock);
	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);

	return ret;
}

static int nu1619_rx_charger_remove(struct i2c_client *client)
{
	struct nu1619_rx *chip = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&chip->wireless_work);
	cancel_delayed_work_sync(&chip->wireless_int_work);
	regmap_exit(chip->regmap);
	sysfs_remove_group(&chip->wip_psy->dev.kobj, &chip->sysfs->attr_g);

	return 0;
}

static const struct i2c_device_id nu1619_rx_id[] = {
	{"wl_charger_nu1619", 0},
	{},
};

static const struct of_device_id  nu1619_rx_match_table[] = {
	{ .compatible = "nuvolta,wl_charger_nu1619",},
	{}
};

MODULE_DEVICE_TABLE(of, nu1619_rx_match_table);

static struct i2c_driver nu1619_wireless_charger_driver = {
	.driver = {
		.name = "nu1619_rx",
		.of_match_table = nu1619_rx_match_table,
	},
	.probe = nu1619_rx_charger_probe,
	.remove = nu1619_rx_charger_remove,
	.id_table = nu1619_rx_id,
};
module_i2c_driver(nu1619_wireless_charger_driver);
MODULE_DESCRIPTION("NUVOLTA Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
