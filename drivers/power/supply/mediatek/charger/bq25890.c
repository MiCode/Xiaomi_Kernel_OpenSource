/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/charger_type.h>
#include "bq25890.h"
#include "mtk_charger_intf.h"


/*BQ25890 REG06 VREG[5:0]*/
const unsigned int VBAT_CV_VTH[] = {
	3840000, 3856000, 3872000, 3888000,
	3904000, 3920000, 3936000, 3952000,
	3968000, 3984000, 4000000, 4016000,
	4032000, 4048000, 4064000, 4080000,
	4096000, 4112000, 4128000, 4144000,
	4160000, 4176000, 4192000, 4208000,
	4224000, 4240000, 4256000, 4272000,
	4288000, 4304000, 4320000, 4336000,
	4352000, 4368000, 4384000, 4400000,
	4416000, 4432000, 4448000, 4464000,
	4480000, 4496000, 4512000, 4528000,
	4544000, 4560000, 4576000, 4592000,
	4608000
};

/*BQ25890 REG04 ICHG[6:0]*/
const unsigned int CS_VTH[] = {
	0, 6400, 12800, 19200,
	25600, 32000, 38400, 44800,
	51200, 57600, 64000, 70400,
	76800, 83200, 89600, 96000,
	102400, 108800, 115200, 121600,
	128000, 134400, 140800, 147200,
	153600, 160000, 166400, 172800,
	179200, 185600, 192000, 198400,
	204800, 211200, 217600, 224000,
	230400, 236800, 243200, 249600,
	256000, 262400, 268800, 275200,
	281600, 288000, 294400, 300800,
	307200, 313600, 320000, 326400,
	332800, 339200, 345600, 352000,
	358400, 364800, 371200, 377600,
	384000, 390400, 396800, 403200,
	409600, 416000, 422400, 428800,
	435200, 441600, 448000, 454400,
	460800, 467200, 473600, 480000,
	486400, 492800, 499200, 505600
};

/*BQ25890 REG00 IINLIM[5:0]*/
const unsigned int INPUT_CS_VTH[] = {
	10000, 15000, 20000, 25000,
	30000, 35000, 40000, 45000,
	50000, 55000, 60000, 65000,
	70000, 75000, 80000, 85000,
	90000, 95000, 100000, 105000,
	110000, 115000, 120000, 125000,
	130000, 135000, 140000, 145000,
	150000, 155000, 160000, 165000,
	170000, 175000, 180000, 185000,
	190000, 195000, 200000, 200500,
	210000, 215000, 220000, 225000,
	230000, 235000, 240000, 245000,
	250000, 255000, 260000, 265000,
	270000, 275000, 280000, 285000,
	290000, 295000, 300000, 305000,
	310000, 315000, 320000, 325000
};

const unsigned int VINDPM_REG[] = {
	2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500,
	3600, 3700, 3800, 3900, 4000, 4100, 4200, 4300, 4400, 4500,
	4600, 4700, 4800, 4900, 5000, 5100, 5200, 5300, 5400, 5500,
	5600, 5700, 5800, 5900, 6000, 6100, 6200, 6300, 6400, 6500,
	6600, 6700, 6800, 6900, 7000, 7100, 7200, 7300, 7400, 7500,
	7600, 7700, 7800, 7900, 8000, 8100, 8200, 8300, 8400, 8500,
	8600, 8700, 8800, 8900, 9000, 9100, 9200, 9300, 9400, 9500,
	9600, 9700, 9800, 9900, 10000, 10100, 10200, 10300, 10400, 10500,
	10600, 10700, 10800, 10900, 11000, 11100, 11200, 11300, 11400, 11500,
	11600, 11700, 11800, 11900, 12000, 12100, 12200, 12300, 12400, 12500,
	12600, 12700, 12800, 12900, 13000, 13100, 13200, 13300, 13400, 13500,
	13600, 13700, 13800, 13900, 14000, 14100, 14200, 14300, 14400, 14500,
	14600, 14700, 14800, 14900, 15000, 15100, 15200, 15300
};

/* BQ25890 REG0A BOOST_LIM[2:0], mA */
const unsigned int BOOST_CURRENT_LIMIT[] = {
	500, 750, 1200, 1400, 1650, 1875, 2150,
};


/* BQ25890 REG08 VCLAMP, mV */
const unsigned int IRCMP_VOLT_CLAMP[] = {
	0, 32, 64, 128, 160, 192, 224,
};

/* BQ25890 REG08 BAT_COMP, mohm */
const unsigned int IRCMP_RESISTOR[] = {
	0, 20, 40, 60, 80, 100, 120, 140,
};


#ifdef CONFIG_OF
#else

#define bq25890_SLAVE_ADDR_WRITE   0xD4
#define bq25890_SLAVE_ADDR_Read    0xD5

#ifdef I2C_SWITHING_CHARGER_CHANNEL
#define bq25890_BUSNUM I2C_SWITHING_CHARGER_CHANNEL
#else
#define bq25890_BUSNUM 0
#endif

#endif

struct bq25890_info {
	struct charger_device *chg_dev;
	struct power_supply *psy;
	struct charger_properties chg_props;
	struct device *dev;
	struct delayed_work chrdet_dwork;
	const char *chg_dev_name;
	const char *eint_name;
	enum charger_type chg_type;
	int irq;
};

static unsigned int g_input_current;
DEFINE_MUTEX(g_input_current_mutex);
static unsigned int charging_error;
static struct i2c_client *new_client;
static const struct i2c_device_id bq25890_i2c_id[] = { {"bq25890", 0}, {} };

unsigned int charging_value_to_parameter(const unsigned int *parameter,
					 const unsigned int array_size,
					 const unsigned int val)
{
	if (val < array_size)
		return parameter[val];

	pr_info("Can't find the parameter\n");
	return parameter[0];
}

unsigned int charging_parameter_to_value(const unsigned int *parameter,
					 const unsigned int array_size,
					 const unsigned int val)
{
	unsigned int i;

	pr_debug_ratelimited("array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("NO register value match\n");
	/* TODO: ASSERT(0);    // not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
					   unsigned int number,
					   unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		/* max value in the last element */
		for (i = (number - 1); i != 0; i--) {
			if (pList[i] <= level) {
				pr_debug_ratelimited("zzf_%d<=%d, i=%d\n",
						     pList[i], level, i);
				return pList[i];
			}
		}

		pr_info("Can't find closest level\n");
		return pList[0];
	}

	/* max value in the first element */
	for (i = 0; i < number; i++) {
		if (pList[i] <= level)
			return pList[i];
	}

	pr_info("Can't find closest level\n");
	return pList[number - 1];
}


/* Global Variable */
unsigned char bq25890_reg[bq25890_REG_NUM] = { 0 };

static DEFINE_MUTEX(bq25890_i2c_access);
static DEFINE_MUTEX(bq25890_access_lock);

int g_bq25890_hw_exist;

/* I2C Function For Read/Write bq25890 */
#ifdef CONFIG_MTK_I2C_EXTENSION
unsigned int bq25890_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&bq25890_i2c_access);

	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG)
				| I2C_WR_FLAG | I2C_DIRECTION_FLAG;

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
		new_client->ext_flag = 0;
		mutex_unlock(&bq25890_i2c_access);

		return 0;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
	new_client->ext_flag = 0;
	mutex_unlock(&bq25890_i2c_access);

	return 1;
}

unsigned int bq25890_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&bq25890_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG)
				| I2C_DIRECTION_FLAG;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		new_client->ext_flag = 0;
		mutex_unlock(&bq25890_i2c_access);
		return 0;
	}

	new_client->ext_flag = 0;
	mutex_unlock(&bq25890_i2c_access);
	return 1;
}
#else
unsigned int bq25890_read_byte(unsigned char cmd, unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;

	mutex_lock(&bq25890_i2c_access);

	do {
		struct i2c_msg msgs[2] = {
			{
				.addr = new_client->addr,
				.flags = 0,
				.len = 1,
				.buf = &cmd,
			},
			{

				.addr = new_client->addr,
				.flags = I2C_M_RD,
				.len = 1,
				.buf = returnData,
			}
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n",
				new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&bq25890_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int bq25890_write_byte(unsigned char cmd, unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];

	mutex_lock(&bq25890_i2c_access);

	buf[0] = cmd;
	memcpy(&buf[1], &writeData, 1);

	do {
		struct i2c_msg msgs[1] = {
			{
				.addr = new_client->addr,
				.flags = 0,
				.len = 1 + 1,
				.buf = buf,
			},
		};

		/*
		 * Avoid sending the segment addr to not upset non-compliant
		 * DDC monitors.
		 */
		ret = i2c_transfer(new_client->adapter, msgs, xfers);

		if (ret == -ENXIO) {
			pr_info("skipping non-existent adapter %s\n",
				new_client->adapter->name);
			break;
		}
	} while (ret != xfers && --retries);

	mutex_unlock(&bq25890_i2c_access);

	return ret == xfers ? 1 : -1;
}
#endif

/* Read / Write Function */
unsigned int bq25890_read_interface(unsigned char RegNum, unsigned char *val,
				    unsigned char MASK, unsigned char SHIFT)
{
	unsigned char bq25890_reg = 0;
	unsigned int ret = 0;

	ret = bq25890_read_byte(RegNum, &bq25890_reg);

	chr_debug("[%s] Reg[%x]=0x%x\n", __func__, RegNum, bq25890_reg);

	bq25890_reg &= (MASK << SHIFT);
	*val = (bq25890_reg >> SHIFT);

	chr_debug("[%s] val=0x%x\n", __func__, *val);

	return ret;
}

unsigned int bq25890_config_interface(unsigned char RegNum, unsigned char val,
				      unsigned char MASK, unsigned char SHIFT)
{
	unsigned char bq25890_reg = 0;
	unsigned char bq25890_reg_ori = 0;
	unsigned int ret = 0;

	mutex_lock(&bq25890_access_lock);
	ret = bq25890_read_byte(RegNum, &bq25890_reg);

	bq25890_reg_ori = bq25890_reg;
	bq25890_reg &= ~(MASK << SHIFT);
	bq25890_reg |= (val << SHIFT);

	ret = bq25890_write_byte(RegNum, bq25890_reg);
	mutex_unlock(&bq25890_access_lock);
	chr_debug("[%s] write Reg[%x]=0x%x from 0x%x\n", __func__,
		  RegNum, bq25890_reg, bq25890_reg_ori);

	/* Check */
	/* bq25890_read_byte(RegNum, &bq25890_reg);
	 * pr_info("[%s] Check Reg[%x]=0x%x\n", __func__, RegNum, bq25890_reg);
	 */

	return ret;
}

/* write one register directly */
unsigned int bq25890_reg_config_interface(unsigned char RegNum,
					  unsigned char val)
{
	unsigned int ret = 0;

	ret = bq25890_write_byte(RegNum, val);

	return ret;
}

/* Internal Function */
/* CON0 */
void bq25890_set_en_hiz(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_HIZ_MASK),
				       (unsigned char) (CON0_EN_HIZ_SHIFT));
}

void bq25890_set_en_ilim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_ILIM_MASK),
				       (unsigned char) (CON0_EN_ILIM_SHIFT));
}

void bq25890_set_iinlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON0),
				       (val),
				       (unsigned char) (CON0_IINLIM_MASK),
				       (unsigned char) (CON0_IINLIM_SHIFT));
}

unsigned int bq25890_get_iinlim(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON0),
				     (&val),
				     (unsigned char) (CON0_IINLIM_MASK),
				     (unsigned char) (CON0_IINLIM_SHIFT));
	return val;
}

/* CON1 */
void bq25890_ADC_start(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_CONV_START_MASK),
				       (unsigned char) (CON2_CONV_START_SHIFT));
}

void bq25890_set_ADC_rate(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_CONV_RATE_MASK),
				       (unsigned char) (CON2_CONV_RATE_SHIFT));
}

void bq25890_set_vindpm_os(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_VINDPM_OS_MASK),
				       (unsigned char) (CON1_VINDPM_OS_SHIFT));
}

/* CON2 */
void bq25890_set_ico_en_start(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_ICO_EN_MASK),
				       (unsigned char) (CON2_ICO_EN_RATE_SHIFT)
					);
}

void bq25890_set_force_dpdm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_FORCE_DPDM_MASK),
				       (unsigned char) (CON2_FORCE_DPDM_SHIFT));
}

static void bq25890_set_auto_dpdm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_AUTO_DPDM_EN_MASK),
				       (unsigned char) (CON2_AUTO_DPDM_EN_SHIFT)
					);
}

/* CON3 */
void bq25890_wd_reset(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON3),
				       (val),
				       (unsigned char) (CON3_WD_MASK),
				       (unsigned char) (CON3_WD_SHIFT));
}

void bq25890_otg_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON3),
				       (val),
				       (unsigned char) (CON3_OTG_CONFIG_MASK),
				       (unsigned char) (CON3_OTG_CONFIG_SHIFT));
}

static int bq25890_is_otg_en(bool *en)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON3),
				     (&val),
				     (unsigned char) (CON3_OTG_CONFIG_MASK),
				     (unsigned char) (CON3_OTG_CONFIG_SHIFT));

	*en = (val == 0 ? false : true);

	return ret;
}

void bq25890_chg_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON3),
				       (val),
				       (unsigned char) (CON3_CHG_CONFIG_MASK),
				       (unsigned char) (CON3_CHG_CONFIG_SHIFT));
}

unsigned int bq25890_get_chg_en(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON3),
				     (&val),
				     (unsigned char) (CON3_CHG_CONFIG_MASK),
				     (unsigned char) (CON3_CHG_CONFIG_SHIFT));
	return val;
}

void bq25890_set_sys_min(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON3),
				       (val),
				       (unsigned char) (CON3_SYS_V_LIMIT_MASK),
				       (unsigned char) (CON3_SYS_V_LIMIT_SHIFT)
					);
}

/* CON4 */
void bq25890_en_pumpx(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_EN_PUMPX_MASK),
				       (unsigned char) (CON4_EN_PUMPX_SHIFT));
}

void bq25890_set_ichg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_ICHG_MASK),
				       (unsigned char) (CON4_ICHG_SHIFT));
}

unsigned int bq25890_get_reg_ichg(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON4),
				     (&val),
				     (unsigned char) (CON4_ICHG_MASK),
				     (unsigned char) (CON4_ICHG_SHIFT));
	return val;
}

/* CON5 */
void bq25890_set_iprechg(unsigned int val)
{
	unsigned int ret = 0;


	ret = bq25890_config_interface((unsigned char) (bq25890_CON5),
				       (val),
				       (unsigned char) (CON5_IPRECHG_MASK),
				       (unsigned char) (CON5_IPRECHG_SHIFT));

}

void bq25890_set_iterml(unsigned int val)
{
	unsigned int ret = 0;


	ret = bq25890_config_interface((unsigned char) (bq25890_CON5),
				       (val),
				       (unsigned char) (CON5_ITERM_MASK),
				       (unsigned char) (CON5_ITERM_SHIFT));

}

/* CON6 */
void bq25890_set_vreg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_VREG_MASK),
				       (unsigned char) (CON6_VREG_SHIFT));
}

unsigned int bq25890_get_vreg(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON6),
				     (&val),
				     (unsigned char) (CON6_VREG_MASK),
				     (unsigned char) (CON6_VREG_SHIFT));
	return val;
}

void bq25890_set_batlowv(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BATLOWV_MASK),
				       (unsigned char) (CON6_BATLOWV_SHIFT));
}

void bq25890_set_vrechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_VRECHG_MASK),
				       (unsigned char) (CON6_VRECHG_SHIFT));
}

/* CON7 */
void bq25890_en_term_chg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_EN_TERM_CHG_MASK),
				       (unsigned char) (CON7_EN_TERM_CHG_SHIFT)
					);
}

void bq25890_en_state_dis(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_STAT_DIS_MASK),
				       (unsigned char) (CON7_STAT_DIS_SHIFT));
}

void bq25890_set_wd_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_WTG_TIM_SET_MASK),
				       (unsigned char) (CON7_WTG_TIM_SET_SHIFT)
					);
}

void bq25890_en_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_EN_TIMER_MASK),
				       (unsigned char) (CON7_EN_TIMER_SHIFT));
}

unsigned int bq25890_get_chg_timer_enable(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON7),
				     &val,
				     (unsigned char) (CON7_EN_TIMER_MASK),
				     (unsigned char) (CON7_EN_TIMER_SHIFT));

	return val;
}

void bq25890_set_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_SET_CHG_TIM_MASK),
				       (unsigned char) (CON7_SET_CHG_TIM_SHIFT)
					);
}

/* CON8 */
void bq25890_set_thermal_regulation(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON8),
				       (unsigned char) (val),
				       (unsigned char) (CON8_TREG_MASK),
				       (unsigned char) (CON8_TREG_SHIFT));
}

void bq25890_set_VBAT_clamp(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON8),
				       (unsigned char) (val),
				       (unsigned char) (CON8_VCLAMP_MASK),
				       (unsigned char) (CON8_VCLAMP_SHIFT));
}

void bq25890_set_VBAT_IR_compensation(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON8),
				       (unsigned char) (val),
				       (unsigned char) (CON8_BAT_COMP_MASK),
				       (unsigned char) (CON8_BAT_COMP_SHIFT));
}

/* CON9 */
void bq25890_pumpx_up(unsigned int val)
{
	unsigned int ret = 0;

	/* Input current limit = 500 mA, changes after PE+ detection */
	bq25890_set_iinlim(0x08);

	/* CC mode current = 2048 mA */
	bq25890_set_ichg(0x20);

	bq25890_chg_en(1);

	bq25890_en_pumpx(1);
	if (val == 1) {
		ret = bq25890_config_interface(bq25890_CON9, 1, CON9_PUMPX_UP,
					       CON9_PUMPX_UP_SHIFT);
	} else {
		ret = bq25890_config_interface(bq25890_CON9, 1, CON9_PUMPX_DN,
					       CON9_PUMPX_DN_SHIFT);
	}

	/* Input current limit = 500 mA, changes after PE+ detection */
	bq25890_set_iinlim(0x08);

	/* CC mode current = 2048 mA */
	bq25890_set_ichg(0x20);

	msleep(3000);
}

void bq25890_set_force_ico(void)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CON9),
				       (unsigned char) (1),
				       (unsigned char) (FORCE_ICO_MASK),
				       (unsigned char) (FORCE_ICO__SHIFT));
}

/* CONA */
void bq25890_set_boost_ilim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CONA),
				       (unsigned char) (val),
				       (unsigned char) (CONA_BOOST_ILIM_MASK),
				       (unsigned char) (CONA_BOOST_ILIM_SHIFT));
}

void bq25890_set_boost_vlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_CONA),
				       (unsigned char) (val),
				       (unsigned char) (CONA_BOOST_VLIM_MASK),
				       (unsigned char) (CONA_BOOST_VLIM_SHIFT));
}

/* CONB */
unsigned int bq25890_get_vbus_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONB),
				     (&val),
				     (unsigned char) (CONB_VBUS_STAT_MASK),
				     (unsigned char) (CONB_VBUS_STAT_SHIFT));
	return val;
}

unsigned int bq25890_get_chrg_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONB),
				     (&val),
				     (unsigned char) (CONB_CHRG_STAT_MASK),
				     (unsigned char) (CONB_CHRG_STAT_SHIFT));
	return val;
}

unsigned int bq25890_get_pg_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONB),
				     (&val),
				     (unsigned char) (CONB_PG_STAT_MASK),
				     (unsigned char) (CONB_PG_STAT_SHIFT));
	return val;
}

unsigned int bq25890_get_sdp_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONB),
				     (&val),
				     (unsigned char) (CONB_SDP_STAT_MASK),
				     (unsigned char) (CONB_SDP_STAT_SHIFT));
	return val;
}

unsigned int bq25890_get_vsys_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONB),
				     (&val),
				     (unsigned char) (CONB_VSYS_STAT_MASK),
				     (unsigned char) (CONB_VSYS_STAT_SHIFT));
	return val;
}

/* CON0C */
unsigned int bq25890_get_wdt_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONC),
				     (&val),
				     (unsigned char) (CONB_WATG_STAT_MASK),
				     (unsigned char) (CONB_WATG_STAT_SHIFT));
	return val;
}

unsigned int bq25890_get_boost_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONC),
				     (&val),
				     (unsigned char) (CONB_BOOST_STAT_MASK),
				     (unsigned char) (CONB_BOOST_STAT_SHIFT));
	return val;
}

unsigned int bq25890_get_chrg_fault_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONC),
				     (&val),
				     (unsigned char) (CONC_CHRG_FAULT_MASK),
				     (unsigned char) (CONC_CHRG_FAULT_SHIFT));
	return val;
}

unsigned int bq25890_get_bat_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONC),
				     (&val),
				     (unsigned char) (CONB_BAT_STAT_MASK),
				     (unsigned char) (CONB_BAT_STAT_SHIFT));
	return val;
}

/* COND */
void bq25890_set_force_vindpm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_COND),
				       (unsigned char) (val),
				       (unsigned char) (COND_FORCE_VINDPM_MASK),
				       (unsigned char) (COND_FORCE_VINDPM_SHIFT)
					);
}

void bq25890_set_vindpm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25890_config_interface((unsigned char) (bq25890_COND),
				       (unsigned char) (val),
				       (unsigned char) (COND_VINDPM_MASK),
				       (unsigned char) (COND_VINDPM_SHIFT));
}

unsigned int bq25890_get_vindpm(void)
{
	int ret = 0;
	unsigned char val = 0;


	ret = bq25890_read_interface((unsigned char) (bq25890_COND),
				     (&val),
				     (unsigned char) (COND_VINDPM_MASK),
				     (unsigned char) (COND_VINDPM_SHIFT));
	return val;
}

/* CONDE */
unsigned int bq25890_get_vbat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CONE),
				     (&val),
				     (unsigned char) (CONE_VBAT_MASK),
				     (unsigned char) (CONE_VBAT_SHIFT));
	return val;
}

/* CON11 */
unsigned int bq25890_get_vbus(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON11),
				     (&val),
				     (unsigned char) (CON11_VBUS_MASK),
				     (unsigned char) (CON11_VBUS_SHIFT));
	return val;
}

/* CON12 */
unsigned int bq25890_get_ichg(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON12),
				     (&val),
				     (unsigned char) (CONB_ICHG_STAT_MASK),
				     (unsigned char) (CONB_ICHG_STAT_SHIFT));
	return val;
}



/* CON13 */
unsigned int bq25890_get_idpm_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON13),
				     (&val),
				     (unsigned char) (CON13_IDPM_STAT_MASK),
				     (unsigned char) (CON13_IDPM_STAT_SHIFT));
	return val;
}

unsigned int bq25890_get_vdpm_state(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface((unsigned char) (bq25890_CON13),
				     (&val),
				     (unsigned char) (CON13_VDPM_STAT_MASK),
				     (unsigned char) (CON13_VDPM_STAT_SHIFT));
	return val;
}

static int bq25890_get_charger_type(struct bq25890_info *info)
{
	unsigned int vbus_stat = 0;
	unsigned int pg_stat = 0;

	enum charger_type CHR_Type_num = CHARGER_UNKNOWN;

	pg_stat = bq25890_get_pg_state();
	vbus_stat = bq25890_get_vbus_state();
	pr_notice("vbus_stat: 0x%x, pg_stat:0x%x\n", vbus_stat, pg_stat);

	switch (vbus_stat) {
	case 0: /* No input */
		CHR_Type_num = CHARGER_UNKNOWN;
		break;
	case 1: /* SDP */
		CHR_Type_num = STANDARD_HOST;
		break;
	case 2: /* CDP */
		CHR_Type_num = CHARGING_HOST;
		break;
	case 3: /* DCP */
		CHR_Type_num = STANDARD_CHARGER;
		break;
	case 5: /* Unknown adapter */
		CHR_Type_num = NONSTANDARD_CHARGER;
		break;
	case 6: /* Non-standard adapter */
		CHR_Type_num = NONSTANDARD_CHARGER;
		break;
	default:
		CHR_Type_num = NONSTANDARD_CHARGER;
		break;
	}

	return CHR_Type_num;
}

static int bq25890_set_charger_type(struct bq25890_info *info)
{
	int ret = 0;
	union power_supply_propval propval;

	info->psy = power_supply_get_by_name("charger");
	if (!info->psy) {
		pr_info("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}

#if defined(CONFIG_PROJECT_PHY) || defined(CONFIG_PHY_MTK_SSUSB)
	if (info->chg_type == STANDARD_HOST ||
	    info->chg_type == CHARGING_HOST)
		Charger_Detect_Release();
#endif

	if (info->chg_type != CHARGER_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;
	ret = power_supply_set_property(info->psy,
		POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0)
		pr_notice("%s: inform power supply online failed, ret = %d\n",
			__func__, ret);

	propval.intval = info->chg_type;
	ret = power_supply_set_property(info->psy,
		POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0)
		pr_notice("%s: inform power supply type failed, ret = %d\n",
			__func__, ret);

	return ret;
}

/* Internal Function */
static void bq25890_hw_component_detect(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25890_read_interface(0x03, &val, 0xFF, 0x0);

	if (val == 0)
		g_bq25890_hw_exist = 0;
	else
		g_bq25890_hw_exist = 1;

	pr_debug("[%s] exist=%d, Reg[0x03]=0x%x\n", __func__,
		 g_bq25890_hw_exist, val);
}

#if 0
static int is_bq25890_exist(void)
{
	pr_debug("[%s] g_bq25890_hw_exist=%d\n", __func__, g_bq25890_hw_exist);

	return g_bq25890_hw_exist;
}
#endif

static unsigned int charging_get_error_state(void)
{
	return charging_error;
}

static int bq25890_enable_charging(struct charger_device *chg_dev, bool en)
{
	int status = 0;

	if (en) {
		bq25890_set_en_hiz(0x0);
		bq25890_chg_en(en);
	} else {
		bq25890_chg_en(en);
		if (charging_get_error_state())
			pr_info("under test mode: disable charging\n");
		/* bq25890_set_en_hiz(0x1); */
	}

	return status;
}

static int bq25890_get_current(struct charger_device *chg_dev, u32 *ichg)
{
	int status = 0;
	unsigned int array_size;
	unsigned int val;

	/* Get current level */
	array_size = ARRAY_SIZE(CS_VTH);
	val = bq25890_get_reg_ichg();
	*ichg = charging_value_to_parameter(CS_VTH, array_size, val) * 10;

	return status;
}

static int bq25890_set_current(struct charger_device *chg_dev,
			       u32 current_value)
{
	int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	current_value /= 10;
	array_size = ARRAY_SIZE(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size,
						 current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size,
						     set_chr_current);
	bq25890_set_ichg(register_value);

	return status;
}

static int bq25890_get_input_current(struct charger_device *chg_dev, u32 *aicr)
{
	int ret = 0;

	*aicr = g_input_current;

	return ret;
}

static int bq25890_set_input_current(struct charger_device *chg_dev,
				     u32 current_value)
{
	int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	mutex_lock(&g_input_current_mutex);
	current_value /= 10;
	array_size = ARRAY_SIZE(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size,
						 current_value);
	g_input_current = set_chr_current;
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size,
						     set_chr_current);
	bq25890_set_iinlim(register_value);
	mutex_unlock(&g_input_current_mutex);

	/*
	 * For USB_IF compliance test only when USB is in suspend(Ibus < 2.5mA)
	 * or unconfigured(Ibus < 70mA) states
	 */
#ifdef CONFIG_USBIF_COMPLIANCE
	if (current_value < CHARGE_CURRENT_100_00_MA)
		register_value = 0x7f;
	else
		register_value = 0x13;

	charging_set_vindpm(&register_value);
#endif

	return status;
}

static int bq25890_set_cv_voltage(struct charger_device *chg_dev, u32 cv)
{
	int status = 0;
	unsigned short int array_size;
	unsigned int set_cv_voltage;
	unsigned short int register_value;
	/*static kal_int16 pre_register_value; */

	array_size = ARRAY_SIZE(VBAT_CV_VTH);
	/*pre_register_value = -1; */
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, cv);
	register_value =
	    charging_parameter_to_value(VBAT_CV_VTH, ARRAY_SIZE(VBAT_CV_VTH),
					set_cv_voltage);
	pr_info("%s: register_value=0x%x %d %d\n", __func__,
		register_value, cv, set_cv_voltage);
	bq25890_set_vreg(register_value);

	return status;
}

static int bq25890_reset_watch_dog_timer(struct charger_device *chg_dev)
{
	int status = 0;

	/* reset watchdog timer */
	bq25890_config_interface(bq25890_CON3, 0x1, 0x1, 6);

	return status;
}

static int charging_set_vindpm(u32 v)
{
	int status = 0;

	bq25890_set_vindpm(v);

	return status;
}

static int bq25890_get_is_power_path_enable(struct charger_device *chg_dev,
					    bool *en)
{
	int ret = 0;
	u32 reg_vindpm = 0;

	reg_vindpm = bq25890_get_vindpm();
	*en = (reg_vindpm == 0x7F) ? false : true;

	return ret;
}

static int bq25890_set_vindpm_voltage(struct charger_device *chg_dev,
				      u32 vindpm)
{
	int status = 0;
	unsigned int array_size;
	bool is_power_path_enable = true;
	int ret = 0;

	vindpm /= 1000;
	array_size = ARRAY_SIZE(VINDPM_REG);
	vindpm = bmt_find_closest_level(VINDPM_REG, array_size, vindpm);
	vindpm = charging_parameter_to_value(VINDPM_REG, array_size, vindpm);

	/*
	 * Since BQ25890 uses vindpm to turn off power path
	 * If power path is disabled, do not adjust mivr
	 */
	ret = bq25890_get_is_power_path_enable(chg_dev, &is_power_path_enable);
	if (ret == 0 && !is_power_path_enable) {
		pr_info("%s: power path is disable, skip setting vindpm = %d\n",
			__func__, vindpm);
		return 0;
	}

	charging_set_vindpm(vindpm);
	/*bq25890_set_en_hiz(en);*/

	return status;
}

static int bq25890_get_charging_status(struct charger_device *chg_dev,
				       bool *is_done)
{
	int status = 0;
	unsigned char reg_value;

	bq25890_read_interface(bq25890_CONB, &reg_value, 0x3, 3);

	/* check if chrg done */
	if (reg_value == 0x3)
		*is_done = true;
	else
		*is_done = false;

	return status;
}

static int bq25890_enable_power_path(struct charger_device *chg_dev, bool en)
{
	int ret = 0;

	bq25890_set_force_vindpm(1);
	if (en)
		bq25890_set_vindpm(0x13);
	else
		bq25890_set_vindpm(0x7F);

	return ret;
}

static int bq25890_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;

	/* If OTG is enabled, BC1.2 should not work */
	bq25890_set_auto_dpdm(!en);
	bq25890_otg_en(en);

	return ret;
}

static int bq25890_set_boost_current_limit(struct charger_device *chg_dev,
					   u32 uA)
{
	int ret = 0;
	u32 array_size = 0;
	u32 boost_ilimit = 0;
	u8 boost_reg = 0;

	uA /= 1000;
	array_size = ARRAY_SIZE(BOOST_CURRENT_LIMIT);
	boost_ilimit = bmt_find_closest_level(BOOST_CURRENT_LIMIT, array_size,
					      uA);
	boost_reg = charging_parameter_to_value(BOOST_CURRENT_LIMIT, array_size,
						boost_ilimit);
	bq25890_set_boost_ilim(boost_reg);

	return ret;
}

static int bq25890_enable_safetytimer(struct charger_device *chg_dev, bool en)
{
	int status = 0;

	bq25890_en_chg_timer(en);

	return status;
}

static int bq25890_get_is_safetytimer_enable(struct charger_device *chg_dev,
					     bool *en)
{
	int ret = 0;
	u32 reg_safetytimer = 0;

	reg_safetytimer = bq25890_get_chg_timer_enable();
	*en = (reg_safetytimer) ? true : false;

	return ret;
}

static int bq25890_set_ta_current_pattern(struct charger_device *chg_dev,
					  bool is_increase)
{
#if 1
	bq25890_pumpx_up(is_increase);
	pr_debug("Pumping up adaptor...");

#else
	if (increase == KAL_TRUE) {
		bq25890_set_ichg(0x0);	/* 64mA */
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_increase() on 1");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_increase() off 1");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_increase() on 2");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_increase() off 2");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_increase() on 3");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_increase() off 3");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_increase() on 4");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_increase() off 4");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_increase() on 5");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_increase() off 5");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_increase() on 6");
		msleep(485);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_increase() off 6");
		msleep(50);

		pr_info("mtk_ta_increase() end\n");

		bq25890_set_ichg(0x8);	/* 512mA */
		msleep(200);
	} else {
		bq25890_set_ichg(0x0);	/* 64mA */
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_decrease() on 1");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_decrease() off 1");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_decrease() on 2");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_decrease() off 2");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_decrease() on 3");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_decrease() off 3");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_decrease() on 4");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_decrease() off 4");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_decrease() on 5");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_decrease() off 5");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		pr_debug("mtk_ta_decrease() on 6");
		msleep(485);

		bq25890_set_ichg(0x0);	/* 64mA */
		pr_debug("mtk_ta_decrease() off 6");
		msleep(50);

		pr_info("mtk_ta_decrease() end\n");

		bq25890_set_ichg(0x8);	/* 512mA */
	}
#endif
	return 0;
}

static int bq25890_set_ta20_reset(struct charger_device *chg_dev)
{
	bq25890_set_vindpm(0x13);
	bq25890_set_ichg(8);

	bq25890_set_ico_en_start(0);
	bq25890_set_iinlim(0x0);
	msleep(250);
	bq25890_set_iinlim(0xc);
	bq25890_set_ico_en_start(1);
	return 0;
}

struct timespec ptime[13];
static int cptime[13][2];

static int dtime(int i)
{
	struct timespec time;

	time = timespec_sub(ptime[i], ptime[i-1]);
	return time.tv_nsec/1000000;
}

#define PEOFFTIME 40
#define PEONTIME 90

static int bq25890_set_ta20_current_pattern(struct charger_device *chg_dev,
					    u32 chr_vol)
{
	int value;
	int i, j = 0;
	int flag;

	bq25890_set_vindpm(0x13);
	bq25890_set_ichg(8);
	bq25890_set_ico_en_start(0);

	usleep_range(1000, 1200);
	value = (chr_vol - 5500000) / 500000;

	bq25890_set_iinlim(0x0);
	msleep(70);

	get_monotonic_boottime(&ptime[j++]);
	for (i = 4; i >= 0; i--) {
		flag = value & (1 << i);

		if (flag == 0) {
			bq25890_set_iinlim(0xc);
			msleep(PEOFFTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				pr_info("%s fail1: idx:%d target:%d actual:%d\n",
					__func__, i, PEOFFTIME, cptime[j][1]);
				return -EIO;
			}
			j++;
			bq25890_set_iinlim(0x0);
			msleep(PEONTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				pr_info("%s fail2: idx:%d target:%d actual:%d\n",
					__func__, i, PEOFFTIME, cptime[j][1]);
				return -EIO;
			}
			j++;

		} else {
			bq25890_set_iinlim(0xc);
			msleep(PEONTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				pr_info("%s fail3: idx:%d target:%d actual:%d\n",
					__func__, i, PEOFFTIME, cptime[j][1]);
				return -EIO;
			}
			j++;
			bq25890_set_iinlim(0x0);
			msleep(PEOFFTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				pr_info("%s fail4: idx:%d target:%d actual:%d\n",
					__func__, i, PEOFFTIME, cptime[j][1]);
				return -EIO;
			}
			j++;
		}
	}

	bq25890_set_iinlim(0xc);
	msleep(160);
	get_monotonic_boottime(&ptime[j]);
	cptime[j][0] = 160;
	cptime[j][1] = dtime(j);
	if (cptime[j][1] < 150 || cptime[j][1] > 240) {
		pr_info("%s fail5: idx:%d target:%d actual:%d\n",
			__func__, i, PEOFFTIME, cptime[j][1]);
		return -EIO;
	}
	j++;

	bq25890_set_iinlim(0x0);
	msleep(30);
	bq25890_set_iinlim(0xc);

	pr_info("[%s]:chr_vol:%d bit:%d time:%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
		__func__, chr_vol, value, cptime[1][0], cptime[2][0],
		cptime[3][0], cptime[4][0], cptime[5][0], cptime[6][0],
		cptime[7][0], cptime[8][0], cptime[9][0], cptime[10][0],
		cptime[11][0]);

	pr_info("[%s]:chr_vol:%d bit:%d time:%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
		__func__, chr_vol, value, cptime[1][1], cptime[2][1],
		cptime[3][1], cptime[4][1], cptime[5][1], cptime[6][1],
		cptime[7][1], cptime[8][1], cptime[9][1], cptime[10][1],
		cptime[11][1]);

	bq25890_set_ico_en_start(1);
	bq25890_set_iinlim(0x3f);

	return 0;
}

static int bq25890_dump_register(struct charger_device *chg_dev)
{
	unsigned char i = 0;
	unsigned char ichg = 0;
	unsigned char ichg_reg = 0;
	unsigned char iinlim = 0;
	unsigned char vbat = 0;
	unsigned char chrg_state = 0;
	unsigned char chr_en = 0;
	unsigned char vbus = 0;
	unsigned char vdpm = 0;
	unsigned char fault = 0;

	bq25890_ADC_start(1);
	for (i = 0; i < bq25890_REG_NUM; i++) {
		bq25890_read_byte(i, &bq25890_reg[i]);
		chr_debug("[bq25890 reg@][0x%x]=0x%x ", i, bq25890_reg[i]);
	}
	bq25890_ADC_start(1);
	iinlim = bq25890_get_iinlim();
	chrg_state = bq25890_get_chrg_state();
	chr_en = bq25890_get_chg_en();
	ichg_reg = bq25890_get_reg_ichg();
	ichg = bq25890_get_ichg();
	vbat = bq25890_get_vbat();
	vbus = bq25890_get_vbus();
	vdpm = bq25890_get_vdpm_state();
	fault = bq25890_get_chrg_fault_state();
	pr_info("[%s]Ibat=%d, Ilim=%d, Vbus=%d, err=%d, Ichg=%d, Vbat=%d, ChrStat=%d, CHGEN=%d, VDPM=%d\n",
		__func__, ichg_reg * 64, iinlim * 50 + 100, vbus * 100 + 2600,
		fault, ichg * 50, vbat * 20 + 2304, chrg_state, chr_en, vdpm);

	return 0;
}

static void bq25890_chrdet_dwork(struct work_struct *work)
{
	unsigned int pg_stat = 0;

	/* Force charger type detection */
#if defined(CONFIG_PROJECT_PHY) || defined(CONFIG_PHY_MTK_SSUSB)
	Charger_Detect_Init();
#endif
	msleep(50);

	pg_stat = bq25890_get_pg_state();
	if (pg_stat) {
		pr_info("%s: force charger type detection\n", __func__);
		/* Force dpdm will become 0 after detecting is finished */
		bq25890_set_force_dpdm(1);
	}
}

static irqreturn_t bq25890_irq_handler(int irq, void *data)
{
	u8 pg_stat = 0;
	enum charger_type org_chg_type;
	bool en = false;
	struct bq25890_info *info = (struct bq25890_info *)data;

	pr_info("%s\n", __func__);

	/* Skip irq if in OTG mode */
	bq25890_is_otg_en(&en);
	if (en)
		return IRQ_HANDLED;

	/* Set vindpm to 4.5V */
	bq25890_set_force_vindpm(1);
	bq25890_set_vindpm(0x13);

	pg_stat = bq25890_get_pg_state();

	org_chg_type = info->chg_type;
	if (pg_stat) {
		info->chg_type = bq25890_get_charger_type(info);
	} else {
#if defined(CONFIG_PROJECT_PHY) || defined(CONFIG_PHY_MTK_SSUSB)
		Charger_Detect_Init();
#endif
		info->chg_type = CHARGER_UNKNOWN;
		pr_info("%s: plugout\n", __func__);
	}
	if (info->chg_type != org_chg_type)
		bq25890_set_charger_type(info);

	return IRQ_HANDLED;
}

static int bq25890_register_irq(struct bq25890_info *info)
{
	int ret = 0;
	struct device_node *np;

	/* Parse irq number from dts */
	np = of_find_node_by_name(NULL, info->eint_name);
	if (np)
		info->irq = irq_of_parse_and_map(np, 0);
	else {
		pr_info("%s: cannot get node\n", __func__);
		ret = -ENODEV;
		goto err_nodev;
	}
	pr_info("%s: irq = %d\n", __func__, info->irq);

	/* Request threaded IRQ */
	ret = devm_request_threaded_irq(info->dev, info->irq, NULL,
		bq25890_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		info->eint_name, info);
	if (ret < 0) {
		pr_info("%s: request thread irq failed\n", __func__);
		goto err_request_irq;
	}

	enable_irq_wake(info->irq);
	return 0;

err_nodev:
err_request_irq:
	return ret;
}


static int bq25890_parse_dt(struct bq25890_info *info, struct device *dev)
{
	struct device_node *np = dev->of_node;

	pr_info("%s\n", __func__);

	if (!np) {
		pr_info("%s: no of node\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name",
				    &info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		pr_info("%s: no charger name\n", __func__);
	}

	if (of_property_read_string(np, "alias_name",
				    &(info->chg_props.alias_name)) < 0) {
		info->chg_props.alias_name = "bq25890";
		pr_info("%s: no alias name\n", __func__);
	}

	if (of_property_read_string(np, "eint_name", &info->eint_name) < 0) {
		info->eint_name = "chr_stat";
		pr_info("%s: no eint name\n", __func__);
	}

	return 0;
}

static int bq25890_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}

static struct charger_ops bq25890_chg_ops = {
#if 0
	.enable_hz = bq25890_enable_hz,
#endif

	/* Normal charging */
	.dump_registers = bq25890_dump_register,
	.enable = bq25890_enable_charging,
	.get_charging_current = bq25890_get_current,
	.set_charging_current = bq25890_set_current,
	.get_input_current = bq25890_get_input_current,
	.set_input_current = bq25890_set_input_current,
	/*.get_constant_voltage = bq25890_get_battery_voreg,*/
	.set_constant_voltage = bq25890_set_cv_voltage,
	.kick_wdt = bq25890_reset_watch_dog_timer,
	.set_mivr = bq25890_set_vindpm_voltage,
	.is_charging_done = bq25890_get_charging_status,

	/* Safety timer */
	.enable_safety_timer = bq25890_enable_safetytimer,
	.is_safety_timer_enabled = bq25890_get_is_safetytimer_enable,

	/* Power path */
	.enable_powerpath = bq25890_enable_power_path,
	.is_powerpath_enabled = bq25890_get_is_power_path_enable,

#if 0
	/* Charger type detection */
	.enable_chg_type_det = bq25890_enable_chg_type_det,
#endif

	/* OTG */
	.enable_otg = bq25890_enable_otg,
	.set_boost_current_limit = bq25890_set_boost_current_limit,

#if 0
	/* IR compensation */
	.set_ircmp_resistor = bq25890_set_ircmp_resistor,
	.set_ircmp_vclamp = bq25890_set_ircmp_vclamp,
#endif

	/* PE+/PE+20 */
	.send_ta_current_pattern = bq25890_set_ta_current_pattern,
#if 0
	.set_pe20_efficiency_table = bq25890_set_pep20_efficiency_table,
#endif
	.send_ta20_current_pattern = bq25890_set_ta20_current_pattern,
	.reset_ta = bq25890_set_ta20_reset,
	.event = bq25890_do_event,
};

static int bq25890_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct bq25890_info *info = NULL;

	pr_info("[%s]\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(struct bq25890_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	new_client = client;
	info->dev = &client->dev;

	ret = bq25890_parse_dt(info, &client->dev);
	if (ret < 0)
		return ret;

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
		&client->dev, info, &bq25890_chg_ops, &info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		pr_info("%s: register charger device failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}

	bq25890_hw_component_detect();
	/* bq25890_hw_init(); //move to charging_hw_xxx.c */

	INIT_DELAYED_WORK(&info->chrdet_dwork, bq25890_chrdet_dwork);
	schedule_delayed_work(&info->chrdet_dwork, msecs_to_jiffies(5000));

	bq25890_set_auto_dpdm(1);
	bq25890_register_irq(info);

	bq25890_dump_register(info->chg_dev);

	return 0;
}

/* platform_driver API */
unsigned char g_reg_value_bq25890;
static ssize_t show_bq25890_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_info("[%s] 0x%x\n", __func__, g_reg_value_bq25890);
	return sprintf(buf, "0x%x\n", g_reg_value_bq25890);
}

static ssize_t store_bq25890_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr = NULL;
	char temp_buf[32];
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	strncpy(temp_buf, buf, sizeof(temp_buf) - 1);
	temp_buf[sizeof(temp_buf) - 1] = '\0';
	pvalue = temp_buf;

	if (size != 0) {
		if (size > 3) {
			addr = strsep(&pvalue, " ");
			if (addr == NULL) {
				pr_info("[%s] format error\n", __func__);
				return -EINVAL;
			}
			ret = kstrtou32(addr, 16, &reg_address);
			if (ret) {
				pr_info("[%s] format error, ret = %d\n",
					__func__, ret);
				return ret;
			}

			if (pvalue == NULL) {
				pr_info("[%s] format error\n", __func__);
				return -EINVAL;
			}
			ret = kstrtou32(pvalue, 16, &reg_value);
			if (ret) {
				pr_info("[%s] format error, ret = %d\n",
					__func__, ret);
				return ret;
			}

			pr_info("[%s] write bq25890 reg 0x%x with value 0x%x\n",
				__func__, reg_address, reg_value);
			ret = bq25890_config_interface(reg_address, reg_value,
						       0xFF, 0x0);
		} else {
			ret = kstrtou32(pvalue, 16, &reg_address);
			if (ret) {
				pr_info("[%s] format error, ret = %d\n",
					__func__, ret);
				return ret;
			}
			ret = bq25890_read_interface(reg_address,
						     &g_reg_value_bq25890,
						     0xFF, 0x0);
			pr_info("[%s] read bq25890 reg 0x%x with value 0x%x\n",
				__func__, reg_address, g_reg_value_bq25890);
			pr_info("[%s] Please use \"cat bq25890_access\" to get value\n",
				__func__);
		}
	}
	return size;
}

static DEVICE_ATTR(bq25890_access, 0664, show_bq25890_access,
		   store_bq25890_access);

static int bq25890_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("%s\n", __func__);

	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_bq25890_access);

	return 0;
}

struct platform_device bq25890_user_space_device = {
	.name = "bq25890-user",
	.id = -1,
};

static struct platform_driver bq25890_user_space_driver = {
	.probe = bq25890_user_space_probe,
	.driver = {
		   .name = "bq25890-user",
		   },
};

#ifdef CONFIG_OF
static const struct of_device_id bq25890_of_match[] = {
	{.compatible = "mediatek,swithing_charger"},
	{},
};
#else
static struct i2c_board_info i2c_bq25890 __initdata = {
	I2C_BOARD_INFO("bq25890", (bq25890_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver bq25890_driver = {
	.driver = {
		   .name = "bq25890",
#ifdef CONFIG_OF
		   .of_match_table = bq25890_of_match,
#endif
		   },
	.probe = bq25890_driver_probe,
	.id_table = bq25890_i2c_id,
};

static int __init bq25890_init(void)
{
	int ret = 0;

	/* i2c registeration using DTS instead of boardinfo*/
#ifdef CONFIG_OF
	pr_info("[%s] init start with i2c DTS", __func__);
#else
	pr_info("[%s] init start. ch=%d\n", __func__, bq25890_BUSNUM);
	i2c_register_board_info(bq25890_BUSNUM, &i2c_bq25890, 1);
#endif
	if (i2c_add_driver(&bq25890_driver) != 0) {
		pr_info("[%s] failed to register bq25890 i2c driver\n",
			__func__);
	} else {
		pr_info("[%s] Success to register bq25890 i2c driver\n",
			__func__);
	}

	/* bq25890 user space access interface */
	ret = platform_device_register(&bq25890_user_space_device);
	if (ret) {
		pr_info("[%s] Unable to device register(%d)\n", __func__, ret);
		return ret;
	}
	ret = platform_driver_register(&bq25890_user_space_driver);
	if (ret) {
		pr_info("[%s] Unable to register driver(%d)\n", __func__, ret);
		return ret;
	}

	return 0;
}

static void __exit bq25890_exit(void)
{
	i2c_del_driver(&bq25890_driver);
}
module_init(bq25890_init);
module_exit(bq25890_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq25890 Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
