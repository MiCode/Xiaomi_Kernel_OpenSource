/*
 * bq27z561 fuel gauge driver
 *
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "inc/bq27z561.h"
#include "inc/bq27z561_iio.h"
#include "inc/xm_battery_auth.h"
#include "inc/xm_soc_smooth.h"

static int debug_mask = PR_OEM;
module_param_named(
	debug_mask, debug_mask, int, 0600
);

struct bq_fg_chip *g_bq27z561;

#define bq_dbg(reason, fmt, ...)			\
do {						\
	if (debug_mask & (reason))		\
		pr_info(fmt, ##__VA_ARGS__);	\
	else					\
		pr_debug(fmt, ##__VA_ARGS__);	\
} while (0)

/*
static int __fg_read_byte(struct i2c_client *client, u8 reg, u8 *val)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		bq_dbg(PR_OEM, "i2c read byte fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*val = (u8)ret;

	return 0;
}
*/

static int __fg_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		bq_dbg(PR_REGISTER, "i2c write byte fail: can't write 0x%02X to reg 0x%02X\n",
				val, reg);
		return ret;
	}

	return 0;
}

static int __fg_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		bq_dbg(PR_REGISTER, "i2c read word fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*val = (u16)ret;
	return 0;
}

static int __fg_read_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{

	int ret;
	int i;

	for(i = 0; i < len; i++) {
		ret = i2c_smbus_read_byte_data(client, reg + i);
		if (ret < 0) {
			bq_dbg(PR_REGISTER, "i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
		buf[i] = ret;
	}

	//ret = i2c_smbus_read_i2c_block_data(client, reg, len, buf);

	return ret;
}

static int __fg_write_block(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{
	int ret;
	int i = 0;

	for(i = 0; i < len; i++) {
		ret = i2c_smbus_write_byte_data(client, reg + i, buf[i]);
		if (ret < 0) {
			bq_dbg(PR_REGISTER, "i2c read reg 0x%02X faild\n", reg + i);
			return ret;
		}
	}

	return ret;
}

/*
static int fg_read_byte(struct bq_fg_chip *bq, u8 reg, u8 *val)
{
	int ret;

	if (bq->skip_reads) {
		*val = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}
*/

int fg_write_byte(struct bq_fg_chip *bq, u8 reg, u8 val)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_byte(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

int fg_read_word(struct bq_fg_chip *bq, u8 reg, u16 *val)
{
	int ret;

	if (bq->skip_reads) {
		*val = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_word(bq->client, reg, val);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

int fg_read_block(struct bq_fg_chip *bq, u8 reg, u8 *buf, u8 len)
{
	int ret;

	if (bq->skip_reads)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_read_block(bq->client, reg, buf, len);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;

}

int fg_write_block(struct bq_fg_chip *bq, u8 reg, u8 *data, u8 len)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __fg_write_block(bq->client, reg, data, len);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

u8 checksum(u8 *data, u8 len)
{
	u8 i;
	u16 sum = 0;

	for (i = 0; i < len; i++) {
		sum += data[i];
	}

	sum &= 0xFF;

	return 0xFF - sum;
}

static void fg_print_buf(const char *msg, u8 *buf, u8 len)
{
	int i;
	int idx = 0;
	int num;
	u8 strbuf[128];

	bq_dbg(PR_REGISTER, "%s buf: ", msg);
	for (i = 0; i < len; i++) {
		num = sprintf(&strbuf[idx], "%02X ", buf[i]);
		idx += num;
	}
	bq_dbg(PR_REGISTER, "%s\n", strbuf);
}

#if 0
#define TIMEOUT_INIT_COMPLETED	100
static int fg_check_init_completed(struct bq_fg_chip *bq)
{
	int ret;
	int i = 0;
	u16 status;

	while (i++ < TIMEOUT_INIT_COMPLETED) {
		ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &status);
		if (ret >= 0 && (status & 0x0080))
			return 0;
		msleep(100);
	}
	bq_dbg(PR_OEM, "wait for FG INITCOMP timeout\n");
	return ret;
}
#endif

#if 0
static int fg_get_seal_state(struct bq_fg_chip *bq)
{
	int ret;
	u16 status;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CTRL], &status);
	if (ret < 0) {
		bq_dbg(PR_OEM, "Failed to read control status, ret = %d\n", ret);
		return ret;
	}
	status &= 0x6000;
	status >>= 13;

	if (status == 1)
		bq->seal_state = SEAL_STATE_FA;
	else if (status == 2)
		bq->seal_state = SEAL_STATE_UNSEALED;
	else if (status == 3)
		bq->seal_state = SEAL_STATE_SEALED;

	return 0;
}

static int fg_unseal_send_key(struct bq_fg_chip *bq, int key)
{
	int ret;

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], key & 0xFFFF);

	if (ret < 0) {
		bq_dbg(PR_OEM, "unable to write unseal key step 1, ret = %d\n", ret);
		return ret;
	}

	msleep(5);

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], (key >> 16) & 0xFFFF);
	if (ret < 0) {
		bq_dbg(PR_OEM, "unable to write unseal key step 2, ret = %d\n", ret);
		return ret;
	}

	msleep(100);

	return 0;
}

#define FG_DEFAULT_UNSEAL_KEY	0x80008000
static int fg_unseal(struct bq_fg_chip *bq)
{
	int ret;
	int retry = 0;

	ret = fg_unseal_send_key(bq, FG_DEFAULT_UNSEAL_KEY);
	if (!ret) {
		while (retry++ < 100) {
			ret = fg_get_seal_state(bq);
			if (bq->seal_state == SEAL_STATE_UNSEALED ||
			    bq->seal_state == SEAL_STATE_FA) {
				bq_dbg(PR_OEM, "FG is unsealed");
				return 0;
			}
		}
	}

	return -1;
}

#define FG_DEFAULT_UNSEAL_FA_KEY	0x36724614
static int fg_unseal_full_access(struct bq_fg_chip *bq)
{
	int ret;
	int retry = 0;

	ret = fg_unseal_send_key(bq, FG_DEFAULT_UNSEAL_FA_KEY);
	if (!ret) {
		while (retry++ < 100) {
			fg_get_seal_state(bq);
			if (bq->seal_state == SEAL_STATE_FA) {
				bq_dbg(PR_OEM, "FG is in full access.");
				return 0;
			}
			msleep(200);
		}
	}

	return -1;
}

static int fg_seal(struct bq_fg_chip *bq)
{
	int ret;
	int retry = 0;

	ret = fg_write_word(bq, bq->regs[BQ_FG_REG_ALT_MAC], FG_MAC_CMD_SEAL);

	if (ret < 0) {
		bq_dbg(PR_OEM, "Failed to send seal command\n");
		return ret;
	}

	while (retry++ < 100) {
		fg_get_seal_state(bq);
		if (bq->seal_state == SEAL_STATE_SEALED) {
			bq_dbg(PR_OEM, "FG is sealed successfully");
			return 0;
		}
		msleep(200);
	}

	return -1;
}
#endif

int fg_mac_read_block(struct bq_fg_chip *bq, u16 cmd, u8 *buf, u8 len)
{
	int ret;
	u8 cksum_calc, cksum;
	u8 t_buf[40];
	u8 t_len;
	int i;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 2);
	if (ret < 0)
		return ret;

	msleep(4);

	ret = fg_read_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, 36);
	if (ret < 0)
		return ret;

	fg_print_buf("mac_read_block", t_buf, 36);

	cksum = t_buf[34];
	t_len = t_buf[35];

	cksum_calc = checksum(t_buf, t_len - 2);
	if (cksum_calc != cksum)
		return 1;

	for (i = 0; i < len; i++)
		buf[i] = t_buf[i+2];

	return 0;
}

int fg_mac_write_block(struct bq_fg_chip *bq, u16 cmd, u8 *data, u8 len)
{
	int ret;
	u8 cksum;
	u8 t_buf[40];
	int i;

	if (len > 32)
		return -1;

	t_buf[0] = (u8)cmd;
	t_buf[1] = (u8)(cmd >> 8);
	for (i = 0; i < len; i++)
		t_buf[i+2] = data[i];

	/*write command/addr, data*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_ALT_MAC], t_buf, len + 2);
	if (ret < 0)
		return ret;

	fg_print_buf("mac_write_block", t_buf, len + 2);

	cksum = checksum(t_buf, len + 2);
	t_buf[0] = cksum;
	t_buf[1] = len + 4; /*buf length, cmd, CRC and len byte itself*/
	/*write checksum and length*/
	ret = fg_write_block(bq, bq->regs[BQ_FG_REG_MAC_CHKSUM], t_buf, 2);

	return ret;
}

#if 0
static int fg_get_fastcharge_mode(struct bq_fg_chip *bq)
{
	u8 data[3];
	int ret;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_CHARGING_STATUS, data, 3);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
		return ret;
	}

	return (data[2] & FG_FLAGS_FASTCHAGE) >> 5;
}
#endif

int fg_check_battery_psy(struct bq_fg_chip *bq)
{
	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			bq_dbg(PR_OEM, "%s batt psy not found!\n", __func__);
			return false;
		}
	}

	return true;
}

int fg_get_gague_mode(struct bq_fg_chip *bq)
{
	u8 data[4];
	int ret;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_GAGUE_STATUS, data, 4);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
		return ret;
	}

	bq->batt_fd_1		= !!(data[0] & BIT(0));
	bq->batt_fc_1		= !!(data[0] & BIT(1));
	bq->batt_td_1		= !!(data[0] & BIT(2));
	bq->batt_tc_1		= !!(data[0] & BIT(3));
	bq_dbg(PR_OEM, "gague status FD %d, FC = %d, TD %d TC %d \n",
		bq->batt_fd_1, bq->batt_fc_1, bq->batt_td_1, bq->batt_tc_1);

	return 0;
}

int fg_set_fastcharge_mode(struct bq_fg_chip *bq, bool enable)
{
	u8 data[2];
	int ret;

	data[0] = enable;

	bq_dbg(PR_OEM, "set fastcharge mode: enable: %d\n", enable);
	if (enable) {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_EN, data, 2);
		if (ret < 0) {
			bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
			return ret;
		}
	} else {
		ret = fg_mac_write_block(bq, FG_MAC_CMD_FASTCHARGE_DIS, data, 2);
		if (ret < 0) {
			bq_dbg(PR_OEM, "could not write fastcharge = %d\n", ret);
			return ret;
		}
	}
	bq->fast_mode = enable;

	return ret;
}

int fg_read_status(struct bq_fg_chip *bq)
{
	int ret;
	u16 flags;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_BATT_STATUS], &flags);
	if (ret < 0)
		return ret;

	bq->batt_fc		= !!(flags & FG_FLAGS_FC);
	bq->batt_fd		= !!(flags & FG_FLAGS_FD);
	bq->batt_rca		= !!(flags & FG_FLAGS_RCA);
	bq->batt_dsg		= !!(flags & FG_FLAGS_DSG);

	return 0;
}

static int fg_get_manu_info(unsigned char val, int base, int step)
{
	int index = 0;
	int data = 0;

	bq_dbg(PR_OEM, "val:%d, '0':%d, 'A':%d, 'a':%d\n", val, '0', 'A', 'a');
	if (val > '0' && val < '9')
		index = val - '0';
	if (val > 'A' && val < 'Z')
		index = val - 'A' + 10;
	if (val > 'a' && val < 'z')
		index = val - 'a' + 36;

	bq_dbg(PR_OEM, "base:%d, index:%d, step:%d\n", base, index, step);
	data = base + index * step;

	return data;
}

int fg_get_manufacture_data(struct bq_fg_chip *bq)
{
	u8 t_buf[40];
	int ret;
	int i;
	int byte, base, step;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_MANU_NAME, t_buf, 32);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get MANE NAME\n");
		/* for draco p0 and p0.1 */
		if (bq->ignore_digest_for_debug)
			bq->old_hw = true;
	}

	if (strncmp(t_buf, "MI", 2) != 0) {
		bq_dbg(PR_OEM, "Can not get MI battery data\n");
		manu_info[TERMINATION].data = BQ27Z561_DEFUALT_TERM;
		manu_info[FFC_TERMINATION].data = BQ27Z561_DEFUALT_FFC_TERM;
		manu_info[RECHARGE_VOL].data = BQ27Z561_DEFUALT_RECHARGE_VOL;
		return 0;
	}

	for (i = 0; i < MANU_DATA_LEN; i++) {
		byte = manu_info[i].byte;
		base = manu_info[i].base;
		step = manu_info[i].step;
		manu_info[i].data = fg_get_manu_info(t_buf[byte], base, step);
		bq_dbg(PR_OEM, "manu_info[%d].data %d\n", i, manu_info[i].data);
	}

	return 0;
}

int fg_get_chem_data(struct bq_fg_chip *bq)
{
	u8 t_buf[40];
	int ret;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_CHEM_NAME, t_buf, 4);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get MANE NAME\n");
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "NOT_DEFAULT_BATTERY");
#endif
		bq->batt_id = 0;
		return 0;
	}
	bq_dbg(PR_OEM, "%s\n", t_buf);

	if (strcmp(t_buf, "FLS0") == 0) {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "S88006_SWD_4V45_5000mAh");
#endif
		bq->batt_id = 1;
	} else if (strcmp(t_buf, "FFN0") == 0) {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "S88006_NVT_4V45_5000mAh");
#endif
		bq->batt_id = 2;
	} else if (strcmp(t_buf, "FFN2") == 0) {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "NVT_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "S88006_NVT_4V45_5000mAh");
#endif
		bq->batt_id = 4;
	} else {
#ifdef CONFIG_WT_QGKI
		hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, "BQ27Z561_GAUGE");
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "NOT_DEFAULT_BATTERY");
#endif
		bq->batt_id = 0;
	}

	return 0;
}

int fg_read_rsoc(struct bq_fg_chip *bq)
{
	int soc, ret;

	if (bq->fake_soc > 0)
		return bq->fake_soc;

	if (bq->skip_reads)
		return bq->last_soc;

	ret = regmap_read(bq->regmap, bq->regs[BQ_FG_REG_SOC], &soc);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read RSOC, ret = %d\n", ret);
		if (bq->last_soc >= 0)
			return bq->last_soc;
		else
			soc = BQ_I2C_FAILED_SOC;
	}

	return soc;
}

int fg_read_system_soc(struct bq_fg_chip *bq)
{
	int soc, curr, temp, raw_soc;
	int tmp_soc, tmp_ram_soc;

	soc = fg_read_rsoc(bq);
	bq->batt_rsoc = soc;
	if(soc == BQ_I2C_FAILED_SOC)
		return BQ_I2C_FAILED_SOC;
	raw_soc = fg_get_raw_soc(bq);
	if (bq->charge_full) {
		if (raw_soc > BQ_REPORT_FULL_SOC)
			soc = 100;
		else if (raw_soc > BQ_CHARGE_FULL_SOC)
			soc = 99;
		bq->update_now = true;
	}

	fg_read_current(bq, &curr);
	temp = fg_read_temperature(bq);

	bq_dbg(PR_OEM, "rsoc %d raw_soc:%d,batt_volt:%d,Cur:%d", soc, bq->batt_soc, bq->batt_volt, bq->batt_curr);
	if(!bq->batt_soc_flag) {
		bq->batt_soc_flag = 1;
		if(raw_soc > 3000 && raw_soc < 9700)
			tmp_ram_soc = raw_soc / 100 - 1;
		else
			tmp_ram_soc = raw_soc / 100;
		tmp_soc = soc * 100;
		soc = bq_battery_soc_smooth_tracking(bq, tmp_soc, tmp_ram_soc, temp, curr);
		bq_dbg(PR_OEM, "first rsoc %d raw_soc:%d,batt_volt:%d,Cur:%d", soc, bq->batt_soc, bq->batt_volt, bq->batt_curr);
	} else {
		tmp_ram_soc = raw_soc / 100;
		tmp_soc = soc * 100;
		//soc = bq_battery_soc_smooth_tracking(bq, raw_soc, soc, temp, curr); //old
		soc = bq_battery_soc_smooth_tracking(bq, tmp_soc, tmp_ram_soc, temp, curr);
	}
	bq->last_soc = soc;

	if(soc > 100)
		soc = 100;

	return soc;
}

int fg_read_temperature(struct bq_fg_chip *bq)
{
	int ret;
	u16 temp = 0;
	static int last_temp;

	if (bq->fake_temp > 0)
		return bq->fake_temp;

	if (bq->skip_reads)
		return last_temp;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TEMP], &temp);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read temperature, ret = %d\n", ret);
		return BQ_I2C_FAILED_TEMP;
	}
	last_temp = temp - 2730;

	return temp - 2730;

}

int fg_read_volt(struct bq_fg_chip *bq)
{
	int ret;
	u16 volt = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_VOLT], &volt);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read voltage, ret = %d\n", ret);
		return 3800;
	}

	if(volt < 3450) {
		bq->rm_adjust_max = 0;
		bq->rm_adjust = 0;
	}

	return volt;
}

int fg_read_avg_current(struct bq_fg_chip *bq, int *curr)
{
	int ret;
	s16 avg_curr = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_AI], (u16 *)&avg_curr);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read current, ret = %d\n", ret);
		return ret;
	}
	*curr = -1 * avg_curr;

	return ret;
}

int fg_read_current(struct bq_fg_chip *bq, int *curr)
{
	int ret;
	s16 avg_curr = 0;

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CN], (u16 *)&avg_curr);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read current, ret = %d\n", ret);
		*curr = 0;
		return ret;
	}
	*curr = -1 * avg_curr;

	return ret;
}

int fg_read_fcc(struct bq_fg_chip *bq)
{
	int ret;
	u16 fcc;

	if (bq->regs[BQ_FG_REG_FCC] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "FCC command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_FCC], &fcc);

	if (ret < 0)
		bq_dbg(PR_OEM, "could not read FCC, ret=%d\n", ret);

	return fcc;
}

int fg_read_dc(struct bq_fg_chip *bq)
{

	int ret;
	u16 dc;

	if (bq->regs[BQ_FG_REG_DC] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM,"DesignCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_DC], &dc);

	if (ret < 0) {
		bq_dbg(PR_OEM,"could not read DC, ret=%d\n", ret);
		return ret;
	}

	return dc;
}

int fg_read_rm(struct bq_fg_chip *bq)
{
	int ret;
	u16 rm;
	u8 t_buf[64];
	int tmp = 0;

	memset(t_buf, 0, 64);

	if (bq->regs[BQ_FG_REG_RM] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "RemainingCapacity command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_RM], &rm);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read DC, ret=%d\n", ret);
		return ret;
	}

	if(rm == 0){
		ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
		tmp = (t_buf[1] << 8) | t_buf[0];
		if(tmp > 65000 && !bq->rm_flag) {
			bq->rm_flag = true;
			bq->rm_adjust_max = 65535 - tmp;
			bq_dbg(PR_OEM, "bq->rm_adjust_max =%d\n", bq->rm_adjust_max);
		}
		if(tmp > 65000) {
			bq->rm_adjust = tmp + bq->rm_adjust_max  - 65535;
		}
		bq_dbg(PR_OEM, "bq->rm_adjust =%d\n", bq->rm_adjust);
	}

	return (rm + bq->rm_adjust);
}

int fg_read_soh(struct bq_fg_chip *bq)
{
	int ret;
	u16 soh;

	if (bq->regs[BQ_FG_REG_SOH] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "SOH command not supported!\n");
		return 0;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_SOH], &soh);
	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read DC, ret=%d\n", ret);
		return ret;
	}

	return soh;
}

int fg_read_cyclecount(struct bq_fg_chip *bq)
{
	int ret;
	u16 cc;

	if (bq->regs[BQ_FG_REG_CC] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "Cycle Count not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CC], &cc);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Cycle Count, ret=%d\n", ret);
		return ret;
	}

	return cc;
}

int fg_read_tte(struct bq_fg_chip *bq)
{
	int ret;
	u16 tte;

	if (bq->regs[BQ_FG_REG_TTE] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, "Time To Empty not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TTE], &tte);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return tte;
}

#if 0
static int fg_read_charging_current(struct bq_fg_chip *bq)
{
	int ret;
	u16 cc;

	if (bq->regs[BQ_FG_REG_CHG_CUR] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, " not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_CUR], &cc);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return cc;
}
#endif

int fg_read_charging_voltage(struct bq_fg_chip *bq)
{
	int ret;
	u16 cv;

	if (bq->regs[BQ_FG_REG_CHG_VOL] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM, " not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_CHG_VOL], &cv);

	if (ret < 0) {
		bq_dbg(PR_OEM, "could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return cv;
}

int fg_read_ttf(struct bq_fg_chip *bq)
{
	int ret;
	u16 ttf;

	if (bq->regs[BQ_FG_REG_TTE] == INVALID_REG_ADDR) {
		bq_dbg(PR_OEM,"Time To Empty not supported!\n");
		return -1;
	}

	ret = fg_read_word(bq, bq->regs[BQ_FG_REG_TTF], &ttf);

	if (ret < 0) {
		bq_dbg(PR_OEM,"could not read Time To Empty, ret=%d\n", ret);
		return ret;
	}

	if (ret == 0xFFFF)
		return -ENODATA;

	return ttf;
}

int fg_get_batt_status(struct bq_fg_chip *bq)
{

	fg_read_status(bq);

	if (bq->batt_fc)
		return POWER_SUPPLY_STATUS_FULL;
	else if (bq->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (bq->batt_curr <= 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_DISCHARGING;
}

int fg_get_batt_capacity_level(struct bq_fg_chip *bq)
{

	if (bq->batt_fc)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (bq->batt_rca)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (bq->batt_fd)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
}

int fg_get_soc_decimal_rate(struct bq_fg_chip *bq)
{
	int soc, i;

	if (bq->dec_rate_len <= 0)
		return 0;

	soc = fg_read_rsoc(bq);

	for (i = 0; i < bq->dec_rate_len; i += 2) {
		if (soc < bq->dec_rate_seq[i]) {
			return bq->dec_rate_seq[i - 1];
		}
	}

	return bq->dec_rate_seq[bq->dec_rate_len - 1];
}

int fg_get_soc_decimal(struct bq_fg_chip *bq)
{
	int rsoc, raw_soc;

	if (!bq)
		return 0;

	rsoc = fg_read_rsoc(bq);
	raw_soc = fg_get_raw_soc(bq);

//	if (bq->batt_soc > (raw_soc / 100))
//		return 0;

	return raw_soc % 100;
}

static ssize_t fg_attr_show_Ra_table(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 t_buf[40];
	u8 temp_buf[40];
	int ret;
	int i, idx, len;

	ret = fg_mac_read_block(bq, FG_MAC_CMD_RA_TABLE, t_buf, 32);
	if (ret < 0)
		return 0;

	idx = 0;
	len = sprintf(temp_buf, "Ra Flag:0x%02X\n", t_buf[0] << 8 | t_buf[1]);
	memcpy(&buf[idx], temp_buf, len);
	idx += len;
	len = sprintf(temp_buf, "RaTable:\n");
	memcpy(&buf[idx], temp_buf, len);
	idx += len;
	for (i = 1; i < 16; i++) {
		len = sprintf(temp_buf, "%d ", t_buf[i*2] << 8 | t_buf[i*2 + 1]);
		memcpy(&buf[idx], temp_buf, len);
		idx += len;
	}

	return idx;
}

static ssize_t fg_attr_show_Qmax(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;
	int tmp;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_QMAX, t_buf, 2);
	if (ret < 0) {
		tmp = bq->Qmax_old;
		len = snprintf(buf, sizeof(t_buf), "%d\n", tmp);
		return 0;
	}
	tmp = (t_buf[1] << 8) | t_buf[0];

	if(bq->batt_temp >= 200) {
		if((tmp < 4500) || (tmp > 5500)){
			tmp = bq->Qmax_old;
		}
	}

	len = snprintf(buf, sizeof(t_buf), "%d\n", tmp);
//	pr_err("BQ27z561 Qmax %d\n", tmp);
	return len;
}

static ssize_t fg_attr_show_fcc_soh(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_FCC_SOH, t_buf, 2);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n", (t_buf[1] << 8) | t_buf[0]);

	return len;
}

static ssize_t fg_attr_show_rm(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int rm, len;

	rm = fg_read_rm(bq);
	len = snprintf(buf, 1024, "%d\n", rm);

	return len;
}

static ssize_t fg_attr_show_fcc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int fcc, len;

	fcc = fg_read_fcc(bq);
	len = snprintf(buf, 1024, "%d\n", fcc);

	return len;
}

static ssize_t fg_attr_show_soh(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int soh, len;

	soh = fg_read_soh(bq);
	len = snprintf(buf, 1024, "%d\n", soh);

	return len;
}

static ssize_t verify_digest_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	u8 digest_buf[4];
	int i;
	int len;

	for (i = 0; i < BATTERY_DIGEST_LEN; i++) {
		memset(digest_buf, 0, sizeof(digest_buf));
		snprintf(digest_buf, sizeof(digest_buf) - 1, "%02x", bq->digest[i]);
		strlcat(buf, digest_buf, BATTERY_DIGEST_LEN * 2 + 1);
	}
	len = strlen(buf);
	buf[len] = '\0';

	return strlen(buf) + 1;
}

static ssize_t verify_digest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int i;
	u8 random[32] = {0};
	char kbuf[70] = {0};

	memset(kbuf, 0, sizeof(kbuf));
	strncpy(kbuf, buf, count - 1);

	StringToHex(kbuf, random, &i);
	fg_sha256_auth(bq, random, BATTERY_DIGEST_LEN);

	return count;
}

static ssize_t fg_attr_show_TRemQ(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[1] << 8) | t_buf[0]);

	return len;
}

static ssize_t fg_attr_show_TFullChgQ(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[9] << 8) | t_buf[8]);

	return len;
}

static ssize_t fg_attr_show_TSim(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[13] << 8) | t_buf[12]);

	return len;
}

static ssize_t fg_attr_show_TAmbient(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int ret;
	u8 t_buf[64];
	int len;

	memset(t_buf, 0, 64);

	ret = fg_mac_read_block(bq, FG_MAC_CMD_ITSTATUS1, t_buf, 36);
	if (ret < 0)
		return 0;

	len = snprintf(buf, sizeof(t_buf), "%d\n",
			(t_buf[15] << 8) | t_buf[14]);

	return len;
}

static ssize_t fg_attr_show_cell1_max(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->cell1_max);

	return len;
}

static ssize_t fg_attr_show_max_charge_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->max_charge_current);

	return len;
}

static ssize_t fg_attr_show_max_discharge_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->max_discharge_current);

	return len;
}

static ssize_t fg_attr_show_max_temp_cell(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->max_temp_cell);

	return len;
}

static ssize_t fg_attr_show_min_temp_cell(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->min_temp_cell);

	return len;
}

static ssize_t fg_attr_show_total_fw_runtime(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->total_fw_runtime);

	return len;
}

static ssize_t fg_attr_show_time_spent_in_lt(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->time_spent_in_lt);

	return len;
}

static ssize_t fg_attr_show_time_spent_in_ht(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->time_spent_in_ht);

	return len;
}

static ssize_t fg_attr_show_time_spent_in_ot(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int len;

	len = snprintf(buf, PAGE_SIZE, "%d\n",
			bq->time_spent_in_ot);

	return len;
}

static ssize_t fg_attr_show_avg_current(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int curr, len;

	fg_read_avg_current(bq, &curr);

	len = snprintf(buf, PAGE_SIZE, "%d\n", curr);

	return len;
}

static ssize_t fg_attr_show_rsoc(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int soc, len;

	soc = fg_read_rsoc(bq);

	len = snprintf(buf, PAGE_SIZE, "%d\n", soc);

	return len;
}

static DEVICE_ATTR(verify_digest, 0660, verify_digest_show, verify_digest_store);
static DEVICE_ATTR(RaTable, S_IRUGO, fg_attr_show_Ra_table, NULL);
static DEVICE_ATTR(Qmax, S_IRUGO, fg_attr_show_Qmax, NULL);
static DEVICE_ATTR(fcc_soh, S_IRUGO, fg_attr_show_fcc_soh, NULL);
static DEVICE_ATTR(rm, S_IRUGO, fg_attr_show_rm, NULL);
static DEVICE_ATTR(fcc, S_IRUGO, fg_attr_show_fcc, NULL);
static DEVICE_ATTR(soh, S_IRUGO, fg_attr_show_soh, NULL);
static DEVICE_ATTR(TRemQ, S_IRUGO, fg_attr_show_TRemQ, NULL);
static DEVICE_ATTR(TFullChgQ, S_IRUGO, fg_attr_show_TFullChgQ, NULL);
static DEVICE_ATTR(TSim, S_IRUGO, fg_attr_show_TSim, NULL);
static DEVICE_ATTR(TAmbient, S_IRUGO, fg_attr_show_TAmbient, NULL);
static DEVICE_ATTR(cell1_max, S_IRUGO, fg_attr_show_cell1_max, NULL);
static DEVICE_ATTR(max_charge_current, S_IRUGO, fg_attr_show_max_charge_current, NULL);
static DEVICE_ATTR(max_discharge_current, S_IRUGO, fg_attr_show_max_discharge_current, NULL);
static DEVICE_ATTR(max_temp_cell, S_IRUGO, fg_attr_show_max_temp_cell, NULL);
static DEVICE_ATTR(min_temp_cell, S_IRUGO, fg_attr_show_min_temp_cell, NULL);
static DEVICE_ATTR(total_fw_runtime, S_IRUGO, fg_attr_show_total_fw_runtime, NULL);
static DEVICE_ATTR(time_spent_in_lt, S_IRUGO, fg_attr_show_time_spent_in_lt, NULL);
static DEVICE_ATTR(time_spent_in_ht, S_IRUGO, fg_attr_show_time_spent_in_ht, NULL);
static DEVICE_ATTR(time_spent_in_ot, S_IRUGO, fg_attr_show_time_spent_in_ot, NULL);
static DEVICE_ATTR(avg_current, S_IRUGO, fg_attr_show_avg_current, NULL);
static DEVICE_ATTR(rsoc, S_IRUGO, fg_attr_show_rsoc, NULL);

static struct attribute *fg_attributes[] = {
	&dev_attr_RaTable.attr,
	&dev_attr_Qmax.attr,
	&dev_attr_fcc_soh.attr,
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_soh.attr,
	&dev_attr_verify_digest.attr,
	&dev_attr_TRemQ.attr,
	&dev_attr_TFullChgQ.attr,
	&dev_attr_TSim.attr,
	&dev_attr_TAmbient.attr,
	&dev_attr_cell1_max.attr,
	&dev_attr_max_charge_current.attr,
	&dev_attr_max_discharge_current.attr,
	&dev_attr_max_temp_cell.attr,
	&dev_attr_min_temp_cell.attr,
	&dev_attr_total_fw_runtime.attr,
	&dev_attr_time_spent_in_lt.attr,
	&dev_attr_time_spent_in_ht.attr,
	&dev_attr_time_spent_in_ot.attr,
	&dev_attr_avg_current.attr,
	&dev_attr_rsoc.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};

int fg_dump_registers(struct bq_fg_chip *bq)
{
	int i;
	int ret;
	u16 val;

	for (i = 0; i < ARRAY_SIZE(fg_dump_regs); i++) {
		ret = fg_read_word(bq, fg_dump_regs[i], &val);
		if (!ret)
			bq_dbg(PR_REGISTER, "Reg[%02X] = 0x%04X\n", fg_dump_regs[i], val);
	}

	return ret;
}

int fg_get_lifetime_data(struct bq_fg_chip *bq)
{
	int ret;
	u8 t_buf[40];

	memset(t_buf, 0, sizeof(t_buf));

	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME1, t_buf, 32);
	if (ret < 0)
		return ret;

	bq->cell1_max = (t_buf[1] << 8) | t_buf[0];
	bq->max_charge_current = (t_buf[3] << 8) | t_buf[2];
	bq->max_discharge_current = (signed short int)((t_buf[5] << 8) | t_buf[4]);
	bq->max_temp_cell = t_buf[6];
	bq->min_temp_cell = t_buf[7];

	memset(t_buf, 0, sizeof(t_buf));
	ret = fg_mac_read_block(bq, FG_MAC_CMD_LIFETIME3, t_buf, 32);
	if (ret < 0)
		return ret;

	bq->total_fw_runtime = (t_buf[1] << 8) | t_buf[0];
	bq->time_spent_in_lt = (t_buf[5] << 8) | t_buf[4];
	bq->time_spent_in_ht = (t_buf[13] << 8) | t_buf[12];
	bq->time_spent_in_ot = (t_buf[15] << 8) | t_buf[14];

	return ret;
}

void fg_update_status(struct bq_fg_chip *bq)
{
	static int last_st, last_soc, last_temp;
	mutex_lock(&bq->data_lock);

	bq->batt_soc = fg_read_system_soc(bq);
	bq->batt_volt = fg_read_volt(bq);
	fg_read_current(bq, &bq->batt_curr);
	bq->batt_temp = fg_read_temperature(bq);
	bq->batt_st = fg_get_batt_status(bq);
	bq->batt_rm = fg_read_rm(bq);
	bq->batt_fcc = fg_read_fcc(bq);
	bq->raw_soc = fg_get_raw_soc(bq);
	fg_get_lifetime_data(bq);
	fg_get_gague_mode(bq);

	mutex_unlock(&bq->data_lock);

	if(bq->batt_temp == BQ_I2C_FAILED_TEMP) {
		bq->batt_temp = bq->old_batt_temp;
	} else {
		bq->old_batt_temp = bq->batt_temp;
	}
	bq_dbg(PR_OEM, "SOC:%d,Volt:%d,Cur:%d,Temp:%d,RM:%d,FC:%d,FAST:%d Chg_Status:%d FC%d,FD%d",
			bq->batt_soc, bq->batt_volt, bq->batt_curr,
			bq->batt_temp, bq->batt_rm, bq->batt_fcc, bq->fast_mode, bq->batt_st, bq->batt_fc, bq->batt_fd);

	if ((last_soc != bq->batt_soc) || (last_temp != bq->batt_temp)
			|| (last_st != bq->batt_st)) {
		if (fg_check_battery_psy(bq))
			power_supply_changed(bq->batt_psy);
	}
	if (bq->batt_st == POWER_SUPPLY_STATUS_DISCHARGING)
		bq->cell_ov_check = 0;

	last_soc = bq->batt_soc;
	last_temp = bq->batt_temp;
	last_st = bq->batt_st;
}

int fg_get_raw_soc(struct bq_fg_chip *bq)
{
	int rm, fcc;
	int raw_soc;
	int tmp_soc;

	rm = fg_read_rm(bq);
	fcc = fg_read_fcc(bq);

	if(bq->batt_curr < 0) {
		if(bq->batt_temp < 0){
			tmp_soc = rm * 10000 / fcc;
			if(tmp_soc >= 9400)
				raw_soc = 10000;
			else
				raw_soc = (tmp_soc * 100) / 94;
		} else if(bq->batt_temp >= 0 && bq->batt_temp < 50){
			tmp_soc = rm * 10000 / fcc;
			if(tmp_soc >= 9500)
				raw_soc = 10000;
			else
				raw_soc = (tmp_soc * 100) / 95;
		} else {
			if(bq->fast_mode) {
				tmp_soc = rm * 10000 / fcc;
				if(bq->batt_id == 2 || bq->batt_id == 4) {
					if(tmp_soc >= 9650)
						raw_soc = 10000;
					else
						raw_soc = (tmp_soc * 1000) / 965;
				} else {
					if(tmp_soc >= 9670)
						raw_soc = 10000;
					else
						raw_soc = (tmp_soc * 1000) / 967;
				}
			} else {
				tmp_soc = rm * 10000 / fcc;
				if(tmp_soc >= 9730)
					raw_soc = 10000;
				else
					raw_soc = (tmp_soc * 1000) / 973;
			}
			if(tmp_soc - bq->rm_adjust_max * 10000 / fcc > 9730){
				bq->rm_adjust_max = 0;
				bq->rm_adjust = 0;
			}
		}

	} else {
		tmp_soc = rm * 10000 / fcc;
		if(tmp_soc >= 9730)
			raw_soc = 10000;
		else
			raw_soc = (tmp_soc * 1000) / 973;
	}

	//raw_soc = DIV_ROUND_CLOSEST(rm * 10000 / fcc, 100);

	return raw_soc;
}

int fg_update_charge_full(struct bq_fg_chip *bq)
{
	int rc;
	union power_supply_propval prop = {0, };

	if (!bq->batt_psy) {
		bq->batt_psy = power_supply_get_by_name("battery");
		if (!bq->batt_psy) {
			return 0;
		}
	}

//	rc = power_supply_get_property(bq->batt_psy,
//		POWER_SUPPLY_PROP_CHARGE_DONE, &prop);
	bq->charge_done = prop.intval;

	rc = power_supply_get_property(bq->batt_psy,
		POWER_SUPPLY_PROP_HEALTH, &prop);
	bq->health = prop.intval;

	bq_dbg(PR_OEM, "raw:%d,done:%d,full:%d,health:%d\n",
			bq->raw_soc, bq->charge_done, bq->charge_full, bq->health);

	if (bq->charge_done && !bq->charge_full) {
		if (bq->raw_soc >= BQ_REPORT_FULL_SOC) {
			bq_dbg(PR_OEM, "Setting charge_full to true\n");
			bq->charge_full = true;
			bq->cell_ov_check = 0;
		} else {
			bq_dbg(PR_OEM, "charging is done raw soc:%d\n", bq->raw_soc);
		}
	} else if (bq->raw_soc <= BQ_CHARGE_FULL_SOC && !bq->charge_done && bq->charge_full) {

		if (bq->charge_done)
			goto out;

		bq->charge_full = false;
	}

	if ((bq->raw_soc <= BQ_RECHARGE_SOC) && bq->charge_done && bq->health != POWER_SUPPLY_HEALTH_WARM) {
		prop.intval = true;
//		rc = power_supply_set_property(bq->batt_psy,
	//			POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);
		if (rc < 0) {
			bq_dbg(PR_OEM, "bq could not set force recharging!\n");
			return rc;
		}
	}

out:
	return 0;
}

/*
static int calc_suspend_time(struct timeval *time_start, int *delta_time)
{
	struct timeval *time_now;

	*delta_time = 0;

	//do_gettimeofday(&time_now);
	*delta_time = (time_now->tv_sec - time_start->tv_sec);
	if (*delta_time < 0)
		*delta_time = 0;

	return 0;
}
*/

int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	bq_dbg(PR_DEBUG,  "now:%ld, last:%ld, delta:%d\n", time_now, time_last, *delta_time);

	return 0;
}

static void fg_monitor_workfunc(struct work_struct *work)
{
	struct bq_fg_chip *bq = container_of(work, struct bq_fg_chip, monitor_work.work);
	int rc;

	if (!bq->old_hw) {
		rc = fg_dump_registers(bq);
		/*if (rc < 0)
			return;*/
		fg_update_status(bq);
		fg_update_charge_full(bq);
	}

	if(bq->fast_mode)
		schedule_delayed_work(&bq->monitor_work, 3 * HZ);
	else
		schedule_delayed_work(&bq->monitor_work, 10 * HZ);
}
static int bq_parse_dt(struct bq_fg_chip *bq)
{
	struct device_node *node = bq->dev->of_node;
	int ret, size;

	size = 0;
	of_get_property(node, "bq,soc_decimal_rate", &size);
	if (size) {
		bq->dec_rate_seq = devm_kzalloc(bq->dev,
				size, GFP_KERNEL);
		if (bq->dec_rate_seq) {
			bq->dec_rate_len =
				(size / sizeof(*bq->dec_rate_seq));
			if (bq->dec_rate_len % 2) {
				bq_dbg(PR_OEM, "invalid soc decimal rate seq\n");
				return -EINVAL;
			}
			of_property_read_u32_array(node,
					"bq,soc_decimal_rate",
					bq->dec_rate_seq,
					bq->dec_rate_len);
		} else {
			bq_dbg(PR_OEM, "error allocating memory for dec_rate_seq\n");
		}
	}

	bq->shutdown_delay_enable = of_property_read_bool(node,
						"bq,shutdown-delay-enable");

	ret = of_property_read_u32(node, "bq,charge-full-design",
			&bq->batt_dc);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get bq,charge-full-designe\n");
		bq->batt_dc = DEFUALT_FULL_DESIGN;
		return ret;
	}

	ret = of_property_read_u32(node, "bq,recharge-voltage",
			&bq->batt_recharge_vol);
	if (ret < 0) {
		bq_dbg(PR_OEM, "failed to get bq,recharge-voltage\n");
		bq->batt_recharge_vol = -EINVAL;
		return ret;
	}

	bq->ignore_digest_for_debug = of_property_read_bool(node,
				"bq,ignore-digest-debug");

	size = 0;
	of_get_property(node, "bq,cold_thermal_seq", &size);
	if (size) {
		bq->cold_thermal_seq = devm_kzalloc(bq->dev,
				size, GFP_KERNEL);
		if (bq->cold_thermal_seq) {
			bq->cold_thermal_len =
				(size / sizeof(int));
			if (bq->cold_thermal_len % 4) {
				bq_dbg(PR_OEM, "invalid cold thermal seq\n");
				return -EINVAL;
			}
			of_property_read_u32_array(node,
					"bq,cold_thermal_seq",
					(int *)bq->cold_thermal_seq,
					bq->cold_thermal_len);
			bq->cold_thermal_len = bq->cold_thermal_len / 4;
		} else {
			bq_dbg(PR_OEM, "error allocating memory for cold thermal seq\n");
		}
	}

	return 0;
}

static struct regmap_config i2c_bq27z561_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0xFFFF,
};

static int bq_fg_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret;
	struct bq_fg_chip *bq;
	u8 *regs;
	struct iio_dev *indio_dev;

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*bq));
	bq = iio_priv(indio_dev);
	bq->indio_dev = indio_dev;

	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;
	bq->chip = id->driver_data;
	bq->model_name = (char *)id->name;

	bq->batt_soc	= -ENODATA;
	bq->batt_fcc	= -ENODATA;
	bq->batt_rm	= -ENODATA;
	bq->batt_dc	= -ENODATA;
	bq->batt_volt	= -ENODATA;
	bq->batt_temp	= 250;
	bq->old_batt_temp = 250;
	bq->batt_curr	= -ENODATA;
	bq->batt_cyclecnt = -ENODATA;
	bq->batt_tte = -ENODATA;
	bq->raw_soc = -ENODATA;
	bq->fcc_curr = 12200;
	bq->last_soc = -EINVAL;
	bq->cell_ov_check = 0;
	bq->batt_soc_old = -107;
	bq->Qmax_old = 5000;
	bq->rm_flag = false;
	bq->rm_adjust = 0;
	bq->rm_adjust_max = 0;
	bq->batt_soc_flag = 0;
	bq->old_batt_curr = 0;

	bq->fake_soc	= -EINVAL;
	bq->fake_temp	= -EINVAL;
	bq->fake_volt	= -EINVAL;
	bq->fake_chip_ok = -EINVAL;

	if (bq->chip == BQ27Z561) {
		regs = bq27z561_regs;
	} else {
		bq_dbg(PR_OEM, "unexpected fuel gauge: %d\n", bq->chip);
		regs = bq27z561_regs;
	}
	memcpy(bq->regs, regs, NUM_REGS);
	i2c_set_clientdata(client, bq);

	bq_parse_dt(bq);

	g_bq27z561 = bq;
	bq->regmap = devm_regmap_init_i2c(client, &i2c_bq27z561_regmap_config);
	if (!bq->regmap)
		return -ENODEV;

	ret = bq27z561_init_iio_psy(bq);
	if (ret < 0) {
		pr_err("Failed to initialize FG IIO PSY, rc=%d\n", ret);
		return ret;
	}

	mutex_init(&bq->i2c_rw_lock);
	mutex_init(&bq->data_lock);
	device_init_wakeup(bq->dev, 1);

	fg_get_manufacture_data(bq);
	fg_set_fastcharge_mode(bq, false);
	fg_update_status(bq);

	//bq->fcc_votable = find_votable("FCC");
	ret = sysfs_create_group(&bq->dev->kobj, &fg_attr_group);
	if (ret) {
		bq_dbg(PR_OEM, "Failed to register sysfs, err:%d\n", ret);
		return ret;
	}

	INIT_DELAYED_WORK(&bq->monitor_work, fg_monitor_workfunc);
	schedule_delayed_work(&bq->monitor_work,10 * HZ);

	pr_err("%s bq fuel gauge probe successfully, %s\n",
			__func__, device2str[bq->chip]);
	fg_get_chem_data(bq);

	return 0;
}

static int bq_fg_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&bq->monitor_work);
	bq->skip_reads = true;
	//do_gettimeofday(&bq->suspend_time);
	bq->suspend_time = ktime_get();

	return 0;
}

static int bq_fg_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bq_fg_chip *bq = i2c_get_clientdata(client);
	int delta_time;

	bq->skip_reads = false;
	//calc_suspend_time(bq->suspend_time, &delta_time);
	calc_delta_time(bq->suspend_time, &delta_time);

	if (delta_time > BQ_RESUME_UPDATE_TIME) {
		bq_dbg(PR_OEM, "suspend more than %d, update soc now\n",
				BQ_RESUME_UPDATE_TIME);
		bq->update_now = true;
	}
	bq->resume_update = true;
	schedule_delayed_work(&bq->monitor_work, HZ);

	return 0;
}

static int bq_fg_remove(struct i2c_client *client)
{
	struct bq_fg_chip *bq = i2c_get_clientdata(client);

	mutex_destroy(&bq->data_lock);
	mutex_destroy(&bq->i2c_rw_lock);

	sysfs_remove_group(&bq->dev->kobj, &fg_attr_group);

	return 0;
}

static void bq_fg_shutdown(struct i2c_client *client)
{
	bq_dbg(PR_OEM, "bq fuel gauge driver shutdown!\n");
}

static struct of_device_id bq_fg_match_table[] = {
	{.compatible = "ti,bq27z561",},
	{.compatible = "ti,bq28z610",},
	{},
};
MODULE_DEVICE_TABLE(of, bq_fg_match_table);

static const struct i2c_device_id bq_fg_id[] = {
	{ "bq27z561", BQ27Z561 },
	{ "bq28z610", BQ28Z610 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq_fg_id);

static const struct dev_pm_ops bq_fg_pm_ops = {
	.resume		= bq_fg_resume,
	.suspend	= bq_fg_suspend,
};

static struct i2c_driver bq_fg_driver = {
	.driver	= {
		.name   = "bq27z561",
		.owner  = THIS_MODULE,
		.of_match_table = bq_fg_match_table,
		.pm     = &bq_fg_pm_ops,
	},
	.id_table       = bq_fg_id,

	.probe          = bq_fg_probe,
	.remove		= bq_fg_remove,
	.shutdown	= bq_fg_shutdown,

};

module_i2c_driver(bq_fg_driver);

MODULE_DESCRIPTION("TI BQ27Z561 Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");

