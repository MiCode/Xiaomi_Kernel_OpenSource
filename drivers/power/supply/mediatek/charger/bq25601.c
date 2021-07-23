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
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/charger_type.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "bq25601.h"
#include "mtk_charger_intf.h"
#include <linux/power_supply.h>

/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/

#define GETARRAYNUM(array) (ARRAY_SIZE(array))

/*bq25601 REG06 VREG[5:0]*/
const unsigned int VBAT_CV_VTH[] = {
	3856000, 3888000, 3920000, 3952000,
	3984000, 4016000, 4048000, 4080000,
	4112000, 4144000, 4176000, 4208000,
	4240000, 4272000, 4304000, 4336000,
	4368000, 4400000, 4432000, 4464000,
	4496000, 4528000, 4560000, 4592000,
	4624000

};

/*BQ25601 REG04 ICHG[6:0]*/
const unsigned int CS_VTH[] = {
	0, 6000, 12000, 18000, 24000,
	30000, 36000, 42000, 48000, 54000,
	60000, 66000, 72000, 78000, 84000,
	90000, 96000, 102000, 108000, 114000,
	120000, 126000, 132000, 138000, 144000,
	150000, 156000, 162000, 168000, 174000,
	180000, 186000, 192000, 198000, 204000,
	210000, 216000, 222000, 228000
};

/*BQ25601 REG00 IINLIM[5:0]*/
const unsigned int INPUT_CS_VTH[] = {
	10000, 20000, 30000, 40000,
	50000, 60000, 70000, 80000,
	90000, 100000, 110000, 120000,
	130000, 140000, 150000, 160000,
	170000, 180000, 190000, 200000,
	210000, 220000, 230000, 250000,
	260000, 270000, 280000, 290000,
	300000, 310000, 320000
};


const unsigned int VCDT_HV_VTH[] = {
	4200000, 4250000, 4300000, 4350000,
	4400000, 4450000, 4500000, 4550000,
	4600000, 6000000, 6500000, 7000000,
	7500000, 8500000, 9500000, 10500000

};


const unsigned int VINDPM_REG[] = {
	3900, 4000, 4100, 4200, 4300, 4400,
	4500, 4600, 4700, 4800, 4900, 5000,
	5100, 5200, 5300, 5400, 5500, 5600,
	5700, 5800, 5900, 6000, 6100, 6200,
	6300, 6400
};

/* BQ25601 REG0A BOOST_LIM[2:0], mA */
const unsigned int BOOST_CURRENT_LIMIT[] = {
	500, 1200
};

struct bq25601_info {
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	const char *chg_dev_name;
	const char *eint_name;
	enum charger_type chg_type;
	int irq;
};

DEFINE_MUTEX(g_input_current_mutex);
static struct i2c_client *new_client;
static const struct i2c_device_id bq25601_i2c_id[] = { {"bq25601", 0}, {} };

static int bq25601_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id);

unsigned int charging_value_to_parameter(const unsigned int
		*parameter, const unsigned int array_size,
		const unsigned int val)
{
	if (val < array_size)
		return parameter[val];

	pr_info("Can't find the parameter\n");
	return parameter[0];

}

unsigned int charging_parameter_to_value(const unsigned int
		*parameter, const unsigned int array_size,
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
		for (i = (number - 1); i != 0;
		     i--) {	/* max value in the last element */
			if (pList[i] <= level) {
				pr_debug_ratelimited("zzf_%d<=%d, i=%d\n",
					pList[i], level, i);
				return pList[i];
			}
		}

		pr_info("Can't find closest level\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}

		pr_info("Can't find closest level\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}


/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
unsigned char bq25601_reg[bq25601_REG_NUM] = { 0 };

static DEFINE_MUTEX(bq25601_i2c_access);
static DEFINE_MUTEX(bq25601_access_lock);

int g_bq25601_hw_exist;

/**********************************************************
 *
 *   [I2C Function For Read/Write bq25601]
 *
 *********************************************************/
#ifdef CONFIG_MTK_I2C_EXTENSION
unsigned int bq25601_read_byte(unsigned char cmd,
			       unsigned char *returnData)
{
	char cmd_buf[1] = { 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&bq25601_i2c_access);

	/* new_client->addr = ((new_client->addr) & I2C_MASK_FLAG) |
	 * I2C_WR_FLAG;
	 */
	new_client->ext_flag =
		((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG |
		I2C_DIRECTION_FLAG;

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], (1 << 8 | 1));
	if (ret < 0) {
		/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
		new_client->ext_flag = 0;
		mutex_unlock(&bq25601_i2c_access);

		return 0;
	}

	readData = cmd_buf[0];
	*returnData = readData;

	/* new_client->addr = new_client->addr & I2C_MASK_FLAG; */
	new_client->ext_flag = 0;
	mutex_unlock(&bq25601_i2c_access);

	return 1;
}

unsigned int bq25601_write_byte(unsigned char cmd,
				unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	mutex_lock(&bq25601_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	new_client->ext_flag = ((new_client->ext_flag) & I2C_MASK_FLAG) |
			       I2C_DIRECTION_FLAG;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		new_client->ext_flag = 0;
		mutex_unlock(&bq25601_i2c_access);
		return 0;
	}

	new_client->ext_flag = 0;
	mutex_unlock(&bq25601_i2c_access);
	return 1;
}
#else
unsigned int bq25601_read_byte(unsigned char cmd,
			       unsigned char *returnData)
{
	unsigned char xfers = 2;
	int ret, retries = 1;

	mutex_lock(&bq25601_i2c_access);

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

	mutex_unlock(&bq25601_i2c_access);

	return ret == xfers ? 1 : -1;
}

unsigned int bq25601_write_byte(unsigned char cmd,
				unsigned char writeData)
{
	unsigned char xfers = 1;
	int ret, retries = 1;
	unsigned char buf[8];

	mutex_lock(&bq25601_i2c_access);

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

	mutex_unlock(&bq25601_i2c_access);

	return ret == xfers ? 1 : -1;
}
#endif
/**********************************************************
 *
 *   [Read / Write Function]
 *
 *********************************************************/
unsigned int bq25601_read_interface(unsigned char RegNum,
				    unsigned char *val, unsigned char MASK,
				    unsigned char SHIFT)
{
	unsigned char bq25601_reg = 0;
	unsigned int ret = 0;

	ret = bq25601_read_byte(RegNum, &bq25601_reg);

	pr_debug_ratelimited("[%s] Reg[%x]=0x%x\n", __func__,
			     RegNum, bq25601_reg);

	bq25601_reg &= (MASK << SHIFT);
	*val = (bq25601_reg >> SHIFT);

	pr_debug_ratelimited("[%s] val=0x%x\n", __func__, *val);

	return ret;
}

unsigned int bq25601_config_interface(unsigned char RegNum,
				      unsigned char val, unsigned char MASK,
				      unsigned char SHIFT)
{
	unsigned char bq25601_reg = 0;
	unsigned char bq25601_reg_ori = 0;
	unsigned int ret = 0;

	mutex_lock(&bq25601_access_lock);
	ret = bq25601_read_byte(RegNum, &bq25601_reg);

	bq25601_reg_ori = bq25601_reg;
	bq25601_reg &= ~(MASK << SHIFT);
	bq25601_reg |= (val << SHIFT);

	ret = bq25601_write_byte(RegNum, bq25601_reg);
	mutex_unlock(&bq25601_access_lock);
	pr_debug_ratelimited("[%s] write Reg[%x]=0x%x from 0x%x\n", __func__,
			     RegNum,
			     bq25601_reg, bq25601_reg_ori);

	/* Check */
	/* bq25601_read_byte(RegNum, &bq25601_reg); */
	/* pr_info("[%s] Check Reg[%x]=0x%x\n", __func__,*/
	/* RegNum, bq25601_reg); */

	return ret;
}

/* write one register directly */
unsigned int bq25601_reg_config_interface(unsigned char RegNum,
		unsigned char val)
{
	unsigned int ret = 0;

	ret = bq25601_write_byte(RegNum, val);

	return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */
void bq25601_set_en_hiz(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_EN_HIZ_MASK),
				       (unsigned char) (CON0_EN_HIZ_SHIFT)
				      );
}

void bq25601_set_iinlim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON0),
				       (unsigned char) (val),
				       (unsigned char) (CON0_IINLIM_MASK),
				       (unsigned char) (CON0_IINLIM_SHIFT)
				      );
}

void bq25601_set_stat_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON0),
				   (unsigned char) (val),
				   (unsigned char) (CON0_STAT_IMON_CTRL_MASK),
				   (unsigned char) (CON0_STAT_IMON_CTRL_SHIFT)
				   );
}

/* CON1---------------------------------------------------- */

void bq25601_set_reg_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON11),
				       (unsigned char) (val),
				       (unsigned char) (CON11_REG_RST_MASK),
				       (unsigned char) (CON11_REG_RST_SHIFT)
				      );
}

void bq25601_set_pfm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_PFM_MASK),
				       (unsigned char) (CON1_PFM_SHIFT)
				      );
}

void bq25601_set_wdt_rst(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_WDT_RST_MASK),
				       (unsigned char) (CON1_WDT_RST_SHIFT)
				      );
}

void bq25601_set_otg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_OTG_CONFIG_MASK),
				       (unsigned char) (CON1_OTG_CONFIG_SHIFT)
				      );
}


void bq25601_set_chg_config(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_CHG_CONFIG_MASK),
				       (unsigned char) (CON1_CHG_CONFIG_SHIFT)
				      );
}


void bq25601_set_sys_min(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_SYS_MIN_MASK),
				       (unsigned char) (CON1_SYS_MIN_SHIFT)
				      );
}

void bq25601_set_batlowv(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON1),
				       (unsigned char) (val),
				       (unsigned char) (CON1_MIN_VBAT_SEL_MASK),
				       (unsigned char) (CON1_MIN_VBAT_SEL_SHIFT)
				      );
}



/* CON2---------------------------------------------------- */
void bq25601_set_rdson(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_Q1_FULLON_MASK),
				       (unsigned char) (CON2_Q1_FULLON_SHIFT)
				      );
}

void bq25601_set_boost_lim(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_BOOST_LIM_MASK),
				       (unsigned char) (CON2_BOOST_LIM_SHIFT)
				      );
}

void bq25601_set_ichg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_ICHG_MASK),
				       (unsigned char) (CON2_ICHG_SHIFT)
				      );
}

#if 0 //this function does not exist on bq25601
void bq25601_set_force_20pct(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON2),
				       (unsigned char) (val),
				       (unsigned char) (CON2_FORCE_20PCT_MASK),
				       (unsigned char) (CON2_FORCE_20PCT_SHIFT)
				      );
}
#endif
/* CON3---------------------------------------------------- */

void bq25601_set_iprechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_IPRECHG_MASK),
				       (unsigned char) (CON3_IPRECHG_SHIFT)
				      );
}

void bq25601_set_iterm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON3),
				       (unsigned char) (val),
				       (unsigned char) (CON3_ITERM_MASK),
				       (unsigned char) (CON3_ITERM_SHIFT)
				      );
}

/* CON4---------------------------------------------------- */

void bq25601_set_vreg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VREG_MASK),
				       (unsigned char) (CON4_VREG_SHIFT)
				      );
}

void bq25601_set_topoff_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_TOPOFF_TIMER_MASK),
				       (unsigned char) (CON4_TOPOFF_TIMER_SHIFT)
				      );

}


void bq25601_set_vrechg(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON4),
				       (unsigned char) (val),
				       (unsigned char) (CON4_VRECHG_MASK),
				       (unsigned char) (CON4_VRECHG_SHIFT)
				      );
}

/* CON5---------------------------------------------------- */

void bq25601_set_en_term(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TERM_MASK),
				       (unsigned char) (CON5_EN_TERM_SHIFT)
				      );
}



void bq25601_set_watchdog(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_WATCHDOG_MASK),
				       (unsigned char) (CON5_WATCHDOG_SHIFT)
				      );
}

void bq25601_set_en_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_EN_TIMER_MASK),
				       (unsigned char) (CON5_EN_TIMER_SHIFT)
				      );
}

void bq25601_set_chg_timer(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON5),
				       (unsigned char) (val),
				       (unsigned char) (CON5_CHG_TIMER_MASK),
				       (unsigned char) (CON5_CHG_TIMER_SHIFT)
				      );
}

/* CON6---------------------------------------------------- */

void bq25601_set_treg(unsigned int val)
{
#if 0
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BOOSTV_MASK),
				       (unsigned char) (CON6_BOOSTV_SHIFT)
				      );
#endif
}

void bq25601_set_vindpm(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_VINDPM_MASK),
				       (unsigned char) (CON6_VINDPM_SHIFT)
				      );
}


void bq25601_set_ovp(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_OVP_MASK),
				       (unsigned char) (CON6_OVP_SHIFT)
				      );

}

void bq25601_set_boostv(unsigned int val)
{

	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON6),
				       (unsigned char) (val),
				       (unsigned char) (CON6_BOOSTV_MASK),
				       (unsigned char) (CON6_BOOSTV_SHIFT)
				      );
}



/* CON7---------------------------------------------------- */

void bq25601_set_tmr2x_en(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_TMR2X_EN_MASK),
				       (unsigned char) (CON7_TMR2X_EN_SHIFT)
				      );
}

void bq25601_set_batfet_disable(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON7),
				(unsigned char) (val),
				(unsigned char) (CON7_BATFET_Disable_MASK),
				(unsigned char) (CON7_BATFET_Disable_SHIFT)
				);
}


void bq25601_set_batfet_delay(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON7),
				       (unsigned char) (val),
				       (unsigned char) (CON7_BATFET_DLY_MASK),
				       (unsigned char) (CON7_BATFET_DLY_SHIFT)
				      );
}

void bq25601_set_batfet_reset_enable(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON7),
				(unsigned char) (val),
				(unsigned char) (CON7_BATFET_RST_EN_MASK),
				(unsigned char) (CON7_BATFET_RST_EN_SHIFT)
				);
}


/* CON8---------------------------------------------------- */

unsigned int bq25601_get_system_status(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25601_read_interface((unsigned char) (bq25601_CON8),
				     (&val), (unsigned char) (0xFF),
				     (unsigned char) (0x0)
				    );
	return val;
}

unsigned int bq25601_get_vbus_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25601_read_interface((unsigned char) (bq25601_CON8),
				     (&val),
				     (unsigned char) (CON8_VBUS_STAT_MASK),
				     (unsigned char) (CON8_VBUS_STAT_SHIFT)
				    );
	return val;
}

unsigned int bq25601_get_chrg_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25601_read_interface((unsigned char) (bq25601_CON8),
				     (&val),
				     (unsigned char) (CON8_CHRG_STAT_MASK),
				     (unsigned char) (CON8_CHRG_STAT_SHIFT)
				    );
	return val;
}

unsigned int bq25601_get_vsys_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25601_read_interface((unsigned char) (bq25601_CON8),
				     (&val),
				     (unsigned char) (CON8_VSYS_STAT_MASK),
				     (unsigned char) (CON8_VSYS_STAT_SHIFT)
				    );
	return val;
}

unsigned int bq25601_get_pg_stat(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25601_read_interface((unsigned char) (bq25601_CON8),
				     (&val),
				     (unsigned char) (CON8_PG_STAT_MASK),
				     (unsigned char) (CON8_PG_STAT_SHIFT)
				    );
	return val;
}


/*CON10----------------------------------------------------------*/

void bq25601_set_int_mask(unsigned int val)
{
	unsigned int ret = 0;

	ret = bq25601_config_interface((unsigned char) (bq25601_CON10),
				       (unsigned char) (val),
				       (unsigned char) (CON10_INT_MASK_MASK),
				       (unsigned char) (CON10_INT_MASK_SHIFT)
				      );
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static int bq25601_dump_register(struct charger_device *chg_dev)
{

	unsigned char i = 0;
	unsigned int ret = 0;

	pr_info("[bq25601] ");
	for (i = 0; i < bq25601_REG_NUM; i++) {
		ret = bq25601_read_byte(i, &bq25601_reg[i]);
		if (ret == 0) {
			pr_info("[bq25601] i2c transfor error\n");
			return 1;
		}
		pr_info("[0x%x]=0x%x ", i, bq25601_reg[i]);
	}
	pr_debug("\n");
	return 0;
}


/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static void bq25601_hw_component_detect(void)
{
	unsigned int ret = 0;
	unsigned char val = 0;

	ret = bq25601_read_interface(0x0B, &val, 0xFF, 0x0);

	if (val == 0)
		g_bq25601_hw_exist = 0;
	else
		g_bq25601_hw_exist = 1;

	pr_info("[%s] exist=%d, Reg[0x0B]=0x%x\n", __func__,
		g_bq25601_hw_exist, val);
}


static int bq25601_enable_charging(struct charger_device *chg_dev,
				   bool en)
{
	int status = 0;

	pr_info("enable state : %d\n", en);
	if (en) {
		/* bq25601_config_interface(bq25601_CON3, 0x1, 0x1, 4); */
		/* enable charging */
		bq25601_set_en_hiz(0x0);
		bq25601_set_chg_config(en);
	} else {
		/* bq25601_config_interface(bq25601_CON3, 0x0, 0x1, 4); */
		/* enable charging */
		bq25601_set_chg_config(en);
		pr_info("[charging_enable] under test mode: disable charging\n");

		/*bq25601_set_en_hiz(0x1);*/
	}

	return status;
}

static int bq25601_get_current(struct charger_device *chg_dev,
			       u32 *ichg)
{
	unsigned int ret_val = 0;
#if 0 //todo
	unsigned char ret_force_20pct = 0;

	/* Get current level */
	bq25601_read_interface(bq25601_CON2, &ret_val, CON2_ICHG_MASK,
			       CON2_ICHG_SHIFT);

	/* Get Force 20% option */
	bq25601_read_interface(bq25601_CON2, &ret_force_20pct,
			       CON2_FORCE_20PCT_MASK,
			       CON2_FORCE_20PCT_SHIFT);

	/* Parsing */
	ret_val = (ret_val * 64) + 512;

#endif
	return ret_val;
}

static int bq25601_set_current(struct charger_device *chg_dev,
			       u32 current_value)
{
	unsigned int status = true;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	pr_info("&&&& charge_current_value = %d\n", current_value);
	current_value /= 10;
	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size,
			  current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size,
			 set_chr_current);
	//pr_info("&&&& charge_register_value = %d\n",register_value);
	pr_info("&&&& %s register_value = %d\n", __func__,
		register_value);
	bq25601_set_ichg(register_value);

	return status;
}

static int bq25601_get_input_current(struct charger_device *chg_dev,
				     u32 *aicr)
{
	int ret = 0;
#if 0
	unsigned char val = 0;

	bq25601_read_interface(bq25601_CON0, &val, CON0_IINLIM_MASK,
			       CON0_IINLIM_SHIFT);
	ret = (int)val;
	*aicr = INPUT_CS_VTH[val];
#endif
	return ret;
}


static int bq25601_set_input_current(struct charger_device *chg_dev,
				     u32 current_value)
{
	unsigned int status = true;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	current_value /= 10;
	pr_info("&&&& current_value = %d\n", current_value);
	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size,
			  current_value);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size,
			 set_chr_current);
	pr_info("&&&& %s register_value = %d\n", __func__,
		register_value);
	bq25601_set_iinlim(register_value);

	return status;
}

static int bq25601_set_cv_voltage(struct charger_device *chg_dev,
				  u32 cv)
{
	unsigned int status = true;
	unsigned int array_size;
	unsigned int set_cv_voltage;
	unsigned short register_value;

	array_size = GETARRAYNUM(VBAT_CV_VTH);
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, cv);
	register_value = charging_parameter_to_value(VBAT_CV_VTH, array_size,
			 set_cv_voltage);
	bq25601_set_vreg(register_value);
	pr_info("&&&& cv reg value = %d\n", register_value);

	return status;
}

static int bq25601_reset_watch_dog_timer(struct charger_device
		*chg_dev)
{
	unsigned int status = true;

	pr_info("charging_reset_watch_dog_timer\n");

	bq25601_set_wdt_rst(0x1);	/* Kick watchdog */
	bq25601_set_watchdog(0x3);	/* WDT 160s */

	return status;
}


static int bq25601_set_vindpm_voltage(struct charger_device *chg_dev,
				      u32 vindpm)
{
	int status = 0;
	unsigned int array_size;

	vindpm /= 1000;
	array_size = ARRAY_SIZE(VINDPM_REG);
	vindpm = bmt_find_closest_level(VINDPM_REG, array_size, vindpm);
	vindpm = charging_parameter_to_value(VINDPM_REG, array_size, vindpm);

	pr_info("%s vindpm =%d\r\n", __func__, vindpm);

	//	charging_set_vindpm(vindpm);
	/*bq25601_set_en_hiz(en);*/

	return status;
}

static int bq25601_get_charging_status(struct charger_device *chg_dev,
				       bool *is_done)
{
	unsigned int status = true;
	unsigned int ret_val;

	ret_val = bq25601_get_chrg_stat();

	if (ret_val == 0x3)
		*is_done = true;
	else
		*is_done = false;

	return status;
}

static int bq25601_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;

	pr_info("%s en = %d\n", __func__, en);
	if (en) {
		bq25601_set_chg_config(0);
		bq25601_set_otg_config(1);
		bq25601_set_watchdog(0x3);	/* WDT 160s */
	} else {
		bq25601_set_otg_config(0);
		bq25601_set_chg_config(1);
	}
	return ret;
}

static int bq25601_set_boost_current_limit(struct charger_device
		*chg_dev, u32 uA)
{
	int ret = 0;
	u32 array_size = 0;
	u32 boost_ilimit = 0;
	u8 boost_reg = 0;

	uA /= 1000;
	array_size = ARRAY_SIZE(BOOST_CURRENT_LIMIT);
	boost_ilimit = bmt_find_closest_level(BOOST_CURRENT_LIMIT, array_size,
					      uA);
	boost_reg = charging_parameter_to_value(BOOST_CURRENT_LIMIT,
						array_size, boost_ilimit);
	bq25601_set_boost_lim(boost_reg);

	return ret;
}

static int bq25601_enable_safetytimer(struct charger_device *chg_dev,
				      bool en)
{
	int status = 0;

	if (en)
		bq25601_set_en_timer(0x1);
	else
		bq25601_set_en_timer(0x0);
	return status;
}

static int bq25601_get_is_safetytimer_enable(struct charger_device
		*chg_dev, bool *en)
{
	unsigned char val = 0;

	bq25601_read_interface(bq25601_CON5, &val, CON5_EN_TIMER_MASK,
			       CON5_EN_TIMER_SHIFT);
	*en = (bool)val;
	return val;
}


static unsigned int charging_hw_init(void)
{
	unsigned int status = 0;

	bq25601_set_en_hiz(0x0);
	bq25601_set_vindpm(0x6);	/* VIN DPM check 4.6V */
	bq25601_set_wdt_rst(0x1);	/* Kick watchdog */
	bq25601_set_sys_min(0x5);	/* Minimum system voltage 3.5V */
	bq25601_set_iprechg(0x8);	/* Precharge current 540mA */
	bq25601_set_iterm(0x2);	/* Termination current 180mA */
	bq25601_set_vreg(0x11);	/* VREG 4.4V */
	bq25601_set_pfm(0x1);//disable pfm
	bq25601_set_rdson(0x0);     /*close rdson*/
	bq25601_set_batlowv(0x1);	/* BATLOWV 3.0V */
	bq25601_set_vrechg(0x0);	/* VRECHG 0.1V (4.108V) */
	bq25601_set_en_term(0x1);	/* Enable termination */
	bq25601_set_watchdog(0x3);	/* WDT 160s */
	bq25601_set_en_timer(0x0);	/* Enable charge timer */
	bq25601_set_int_mask(0x0);	/* Disable fault interrupt */
	pr_info("%s: hw_init down!\n", __func__);
	return status;
}

static int bq25601_parse_dt(struct bq25601_info *info,
			    struct device *dev)
{
	struct device_node *np = dev->of_node;
	//int bq25601_en_pin = 0;

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
		info->chg_props.alias_name = "bq25601";
		pr_info("%s: no alias name\n", __func__);
	}
	/*
	 * bq25601_en_pin = of_get_named_gpio(np,"gpio_bq25601_en",0);
	 * if(bq25601_en_pin < 0){
	 * pr_info("%s: no bq25601_en_pin\n", __func__);
	 * return -ENODATA;
	 * }
	 * gpio_request(bq25601_en_pin,"bq25601_en_pin");
	 * gpio_direction_output(bq25601_en_pin,0);
	 * gpio_set_value(bq25601_en_pin,0);
	 */
	/*
	 * if (of_property_read_string(np, "eint_name", &info->eint_name) < 0) {
	 * info->eint_name = "chr_stat";
	 * pr_debug("%s: no eint name\n", __func__);
	 * }
	 */
	return 0;
}

static int bq25601_do_event(struct charger_device *chg_dev, u32 event,
			    u32 args)
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

static struct charger_ops bq25601_chg_ops = {
#if 0
	.enable_hz = bq25601_enable_hz,
#endif

	/* Normal charging */
	.dump_registers = bq25601_dump_register,
	.enable = bq25601_enable_charging,
	.get_charging_current = bq25601_get_current,
	.set_charging_current = bq25601_set_current,
	.get_input_current = bq25601_get_input_current,
	.set_input_current = bq25601_set_input_current,
	/*.get_constant_voltage = bq25601_get_battery_voreg,*/
	.set_constant_voltage = bq25601_set_cv_voltage,
	.kick_wdt = bq25601_reset_watch_dog_timer,
	.set_mivr = bq25601_set_vindpm_voltage,
	.is_charging_done = bq25601_get_charging_status,

	/* Safety timer */
	.enable_safety_timer = bq25601_enable_safetytimer,
	.is_safety_timer_enabled = bq25601_get_is_safetytimer_enable,


	/* Power path */
	/*.enable_powerpath = bq25601_enable_power_path, */
	/*.is_powerpath_enabled = bq25601_get_is_power_path_enable, */


	/* OTG */
	.enable_otg = bq25601_enable_otg,
	.set_boost_current_limit = bq25601_set_boost_current_limit,
	.event = bq25601_do_event,
};


static int bq25601_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct bq25601_info *info = NULL;

	pr_info("[%s]\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(struct bq25601_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	new_client = client;
	info->dev = &client->dev;

	ret = bq25601_parse_dt(info, &client->dev);
	if (ret < 0)
		return ret;

	bq25601_hw_component_detect();
	charging_hw_init();

	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
						&client->dev, info,
						&bq25601_chg_ops,
						&info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		pr_info("%s: register charger device  failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}

	bq25601_dump_register(info->chg_dev);

	return 0;
}

/**********************************************************
 *
 *   [platform_driver API]
 *
 *********************************************************/
unsigned char g_reg_value_bq25601;
static ssize_t show_bq25601_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_info("[%s] 0x%x\n", __func__, g_reg_value_bq25601);
	return sprintf(buf, "%u\n", g_reg_value_bq25601);
}

static ssize_t store_bq25601_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		pr_info("[%s] buf is %s and size is %zu\n", __func__, buf,
			size);

		pvalue = (char *)buf;
		if (size > 3) {
			addr = strsep(&pvalue, " ");
			ret = kstrtou32(addr, 16,
				(unsigned int *)&reg_address);
		} else
			ret = kstrtou32(pvalue, 16,
				(unsigned int *)&reg_address);

		if (size > 3) {
			val = strsep(&pvalue, " ");
			ret = kstrtou32(val, 16, (unsigned int *)&reg_value);
			pr_info(
			"[%s] write bq25601 reg 0x%x with value 0x%x !\n",
			__func__,
			(unsigned int) reg_address, reg_value);
			ret = bq25601_config_interface(reg_address,
				reg_value, 0xFF, 0x0);
		} else {
			ret = bq25601_read_interface(reg_address,
					     &g_reg_value_bq25601, 0xFF, 0x0);
			pr_info(
			"[%s] read bq25601 reg 0x%x with value 0x%x !\n",
			__func__,
			(unsigned int) reg_address, g_reg_value_bq25601);
			pr_info(
			"[%s] use \"cat bq25601_access\" to get value\n",
			__func__);
		}
	}
	return size;
}

static DEVICE_ATTR(bq25601_access, 0664, show_bq25601_access,
		   store_bq25601_access);	/* 664 */

static int bq25601_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("******** %s!! ********\n", __func__);

	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_bq25601_access);

	return 0;
}

struct platform_device bq25601_user_space_device = {
	.name = "bq25601-user",
	.id = -1,
};

static struct platform_driver bq25601_user_space_driver = {
	.probe = bq25601_user_space_probe,
	.driver = {
		.name = "bq25601-user",
	},
};

#ifdef CONFIG_OF
static const struct of_device_id bq25601_of_match[] = {
	{.compatible = "mediatek,bq25601"},
	{},
};
#else
static struct i2c_board_info i2c_bq25601 __initdata = {
	I2C_BOARD_INFO("bq25601", (bq25601_SLAVE_ADDR_WRITE >> 1))
};
#endif

static struct i2c_driver bq25601_driver = {
	.driver = {
		.name = "bq25601",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = bq25601_of_match,
#endif
	},
	.probe = bq25601_driver_probe,
	.id_table = bq25601_i2c_id,
};

static int __init bq25601_init(void)
{
	int ret = 0;

	/* i2c registeration using DTS instead of boardinfo*/
#ifdef CONFIG_OF
	pr_info("[%s] init start with i2c DTS", __func__);
#else
	pr_info("[%s] init start. ch=%d\n", __func__, bq25601_BUSNUM);
	i2c_register_board_info(bq25601_BUSNUM, &i2c_bq25601, 1);
#endif
	if (i2c_add_driver(&bq25601_driver) != 0) {
		pr_info(
			"[%s] failed to register bq25601 i2c driver.\n",
			__func__);
	} else {
		pr_info(
			"[%s] Success to register bq25601 i2c driver.\n",
			__func__);
	}

	/* bq25601 user space access interface */
	ret = platform_device_register(&bq25601_user_space_device);
	if (ret) {
		pr_info("****[%s] Unable to device register(%d)\n", __func__,
			ret);
		return ret;
	}
	ret = platform_driver_register(&bq25601_user_space_driver);
	if (ret) {
		pr_info("****[%s] Unable to register driver (%d)\n", __func__,
			ret);
		return ret;
	}

	return 0;
}

static void __exit bq25601_exit(void)
{
	i2c_del_driver(&bq25601_driver);
}
module_init(bq25601_init);
module_exit(bq25601_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq25601 Driver");
MODULE_AUTHOR("will cai <will.cai@mediatek.com>");
