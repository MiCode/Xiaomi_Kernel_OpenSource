// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/reset.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include "soc_temp.h"

static int thermal_debug_log;
#define thermal_printk(fmt, args...)	\
	do {			\
		if (thermal_debug_log & 0x1) {		\
			pr_err("[Module: Soc_Thermal] " fmt, ##args);	\
		}					\
	} while (0)
#define thermal_dprintk(fmt, args...)	\
	do {			\
		if (thermal_debug_log & 0x2) {		\
			pr_err("[Module: Soc_Thermal] " fmt, ##args);	\
		}					\
	} while (0)

/* MT8173 thermal sensors */
#define MT8173_TS1	0
#define MT8173_TS2	1
#define MT8173_TS3	2
#define MT8173_TS4	3
#define MT8173_TSABB	4

/* AUXADC channel 11 is used for the temperature sensors */
#define MT8173_TEMP_AUXADC_CHANNEL	11

/* The total number of temperature sensors in the MT8173 */
#define MT8173_NUM_SENSORS		5

/* The number of banks in the MT8173 */
#define MT8173_NUM_ZONES		4

/* The number of sensing points per bank */
#define MT8173_NUM_SENSORS_PER_ZONE	4

/*
 * Layout of the fuses providing the calibration data
 * These macros could be used for MT8173
 * MT8173 has 5 sensors and needs 5 VTS calibration data.
 */
#define MT8173_CALIB_BUF0_VALID		BIT(0)
#define MT8173_CALIB_BUF1_ADC_GE(x)	(((x) >> 22) & 0x3ff)
#define MT8173_CALIB_BUF0_VTS_TS1(x)	(((x) >> 17) & 0x1ff)
#define MT8173_CALIB_BUF0_VTS_TS2(x)	(((x) >> 8) & 0x1ff)
#define MT8173_CALIB_BUF1_VTS_TS3(x)	(((x) >> 0) & 0x1ff)
#define MT8173_CALIB_BUF2_VTS_TS4(x)	(((x) >> 23) & 0x1ff)
#define MT8173_CALIB_BUF2_VTS_TSABB(x)	(((x) >> 14) & 0x1ff)
#define MT8173_CALIB_BUF0_DEGC_CALI(x)	(((x) >> 1) & 0x3f)
#define MT8173_CALIB_BUF0_O_SLOPE(x)	(((x) >> 26) & 0x3f)
#define MT8173_CALIB_BUF0_O_SLOPE_SIGN(x)	(((x) >> 7) & 0x1)
#define MT8173_CALIB_BUF1_ID(x)	(((x) >> 9) & 0x1)

/* MT8173 thermal sensor data */
static const int mt8173_bank_data[MT8173_NUM_ZONES][3] = {
	{ MT8173_TS2, MT8173_TS3 },
	{ MT8173_TS2, MT8173_TS4 },
	{ MT8173_TS1, MT8173_TS2, MT8173_TSABB },
	{ MT8173_TS2 },
};

static const int mt8173_msr[MT8173_NUM_SENSORS_PER_ZONE] = {
	TEMP_MSR0, TEMP_MSR1, TEMP_MSR2, TEMP_MSR3
};

static const int mt8173_adcpnp[MT8173_NUM_SENSORS_PER_ZONE] = {
	TEMP_ADCPNP0, TEMP_ADCPNP1, TEMP_ADCPNP2, TEMP_ADCPNP3
};

static const int mt8173_mux_values[MT8173_NUM_SENSORS] = { 0, 1, 2, 3, 16 };


/**
 * The MT6765 thermal controller info.
 */
//=====================MT6765==========================

/* MT6765 thermal sensors */
#define MT6765_TSMCU1		(0)
#define MT6765_TSMCU2		(1)
#define MT6765_TSMCU3		(2)
#define MT6765_TSMCU4		(3)
#define MT6765_TSMCU5		(4)
#define MT6765_TSMCU6		(5)

/* AUXADC channel 11 is used for the temperature sensors */
#define MT6765_TEMP_AUXADC_CHANNEL	11

/* The total number of temperature sensors in the MT6765 */
#define MT6765_NUM_SENSORS		8

/* The number of banks in the MT6765 */
#define MT6765_NUM_ZONES		2

/* The number of sensing points per bank */
#define MT6765_NUM_SENSORS_PER_ZONE	4

#define MT6765_EFUSE_SIZE 4


static const int MT6765_bank_data[MT6765_NUM_ZONES][MT6765_NUM_SENSORS_PER_ZONE] = {
	{ MT6765_TSMCU1, MT6765_TSMCU2, MT6765_TSMCU3 },
	{ MT6765_TSMCU4, MT6765_TSMCU5},
};

static const int MT6765_msr[MT6765_NUM_SENSORS_PER_ZONE] = {
	TEMP_MSR0, TEMP_MSR1, TEMP_MSR2, TEMP_MSR3
};

static const int MT6765_adcpnp[MT6765_NUM_SENSORS_PER_ZONE] = {
	TEMP_ADCPNP0, TEMP_ADCPNP1, TEMP_ADCPNP2, TEMP_ADCPNP3
};

static const int MT6765_mux_values[MT6765_NUM_SENSORS] = { 0, 1, 2, 4, 5, 6, 8, 9 };

static int MT6765_vts_values[MT6765_NUM_SENSORS] = { 260, 260, 260, 260, 260, 260, 260, 260 };

static const unsigned int mt6765_efuse_offset[MT6765_NUM_SENSORS_PER_ZONE] = {
	0x194, 0x190, 0x198, 0x1C8
};


static struct soc_thermal_data mt6765_thermal_data = {
	.auxadc_channel = MT6765_TEMP_AUXADC_CHANNEL,
	.num_banks = MT6765_NUM_ZONES,
	.num_sensors = MT6765_NUM_SENSORS,
	.adc_ge = 512,
	.degc_cali = 40,
	.o_slope = 0,
	.o_const = 1534,
	.o_offset = 3350,
	.vts = MT6765_vts_values,
	.bank_data = {
		{
			.num_sensors = 3,
			.bank_offset = 0x00,
			.protect_high = 105000,
			.protect_middle = -275000,
			.sensors = MT6765_bank_data[0],
		}, {
			.num_sensors = 3,
			.bank_offset = 0x100,
			.protect_high = 105000,
			.protect_middle = -275000,
			.sensors = MT6765_bank_data[1],
		}, {
			.num_sensors = 2,
			.bank_offset = 0x200,
			.protect_high = 105000,
			.protect_middle = -275000,
			.sensors = MT6765_bank_data[2],
		},
	},
	.tc_speed = {
			0x0000000C,
			0x0001003B,
			0x0000030D
	},
	.msr = MT6765_msr,
	.adcpnp = MT6765_adcpnp,
	.sensor_mux_values = MT6765_mux_values,
	.TS2ADCMUX_Addr_offset = APMIXED_SYS_TS_CON0,
	.TS2ADCMUX_BITS = _BITMASK_(29:28),
	.TS2ADCMUX_EnVAL = 0x0 << 28,
	.TSVBE_SEL_Addr_offset = APMIXED_SYS_TS_CON1,

};

//=====================MT6765==========================

/**
 * The MT6768 thermal controller info.
 */
//=====================MT6768==========================

/* MT6768 thermal sensors */
#define MT6768_TSMCU1		(0)
#define MT6768_TSMCU2		(1)
#define MT6768_TSMCU3		(2)
#define MT6768_TSMCU4		(3)
#define MT6768_TSMCU5		(4)
#define MT6768_TSMCU6		(5)
#define MT6768_TSMCU7		(6)
#define MT6768_TSMCU8		(7)

/* AUXADC channel 11 is used for the temperature sensors */
#define MT6768_TEMP_AUXADC_CHANNEL	11

/* The total number of temperature sensors in the MT6768 */
#define MT6768_NUM_SENSORS		8

/* The number of banks in the MT6768 */
#define MT6768_NUM_ZONES		3

/* The number of sensing points per bank */
#define MT6768_NUM_SENSORS_PER_ZONE	4

#define MT6768_EFUSE_SIZE 4


static const int mt6768_bank_data[MT6768_NUM_ZONES][MT6768_NUM_SENSORS_PER_ZONE] = {
	{ MT6768_TSMCU1, MT6768_TSMCU2, MT6768_TSMCU3 },
	{ MT6768_TSMCU4, MT6768_TSMCU5, MT6768_TSMCU6 },
	{ MT6768_TSMCU7, MT6768_TSMCU8 },
};

static const int mt6768_msr[MT6768_NUM_SENSORS_PER_ZONE] = {
	TEMP_MSR0, TEMP_MSR1, TEMP_MSR2, TEMP_MSR3
};

static const int mt6768_adcpnp[MT6768_NUM_SENSORS_PER_ZONE] = {
	TEMP_ADCPNP0, TEMP_ADCPNP1, TEMP_ADCPNP2, TEMP_ADCPNP3
};

static const int mt6768_mux_values[MT6768_NUM_SENSORS] = { 0, 1, 2, 4, 5, 6, 8, 9 };

static int mt6768_vts_values[MT6768_NUM_SENSORS] = { 260, 260, 260, 260, 260, 260, 260, 260 };

static const unsigned int mt6768_efuse_offset[MT6768_NUM_SENSORS_PER_ZONE] = {
	0x194, 0x190, 0x198, 0x1C8
};


static struct soc_thermal_data mt6768_thermal_data = {
	.auxadc_channel = MT6768_TEMP_AUXADC_CHANNEL,
	.num_banks = MT6768_NUM_ZONES,
	.num_sensors = MT6768_NUM_SENSORS,
	.adc_ge = 512,
	.degc_cali = 40,
	.o_slope = 0,
	.o_const = 1534,
	.o_offset = 3350,
	.vts = mt6768_vts_values,
	.bank_data = {
		{
			.num_sensors = 3,
			.bank_offset = 0x00,
			.protect_high = 105000,
			.protect_middle = -275000,
			.sensors = mt6768_bank_data[0],
		}, {
			.num_sensors = 3,
			.bank_offset = 0x100,
			.protect_high = 105000,
			.protect_middle = -275000,
			.sensors = mt6768_bank_data[1],
		}, {
			.num_sensors = 2,
			.bank_offset = 0x200,
			.protect_high = 105000,
			.protect_middle = -275000,
			.sensors = mt6768_bank_data[2],
		},
	},
	.tc_speed = {
			0x0000000C,
			0x0001003B,
			0x0000030D
	},
	.msr = mt6768_msr,
	.adcpnp = mt6768_adcpnp,
	.sensor_mux_values = mt6768_mux_values,
	.TS2ADCMUX_Addr_offset = APMIXED_SYS_TS_CON0,
	.TS2ADCMUX_BITS = _BITMASK_(29:28),
	.TS2ADCMUX_EnVAL = 0x0 << 28,
	.TSVBE_SEL_Addr_offset = APMIXED_SYS_TS_CON1,
};

//=====================MT6768==========================


/**
 * The MT8173 thermal controller has four banks. Each bank can read up to
 * four temperature sensors simultaneously. The MT8173 has a total of 5
 * temperature sensors. We use each bank to measure a certain area of the
 * SoC. Since TS2 is located centrally in the SoC it is influenced by multiple
 * areas, hence is used in different banks.
 *
 * The thermal core only gets the maximum temperature of all banks, so
 * the bank concept wouldn't be necessary here. However, the SVS (Smart
 * Voltage Scaling) unit makes its decisions based on the same bank
 * data, and this indeed needs the temperatures of the individual banks
 * for making better decisions.
 */
static const struct soc_thermal_data mt8173_thermal_data = {
	.auxadc_channel = MT8173_TEMP_AUXADC_CHANNEL,
	.num_banks = MT8173_NUM_ZONES,
	.num_sensors = MT8173_NUM_SENSORS,
	.bank_data = {
		{
			.num_sensors = 2,
			.sensors = mt8173_bank_data[0],
		}, {
			.num_sensors = 2,
			.sensors = mt8173_bank_data[1],
		}, {
			.num_sensors = 3,
			.sensors = mt8173_bank_data[2],
		}, {
			.num_sensors = 1,
			.sensors = mt8173_bank_data[3],
		},
	},
	.msr = mt8173_msr,
	.adcpnp = mt8173_adcpnp,
	.sensor_mux_values = mt8173_mux_values,
};


static DEFINE_SPINLOCK(thermal_spinlock);

static void mt_thermal_lock(unsigned long *x)
{
	spin_lock_irqsave(&thermal_spinlock, *x);
};


static void mt_thermal_unlock(unsigned long *x)
{
	spin_unlock_irqrestore(&thermal_spinlock, *x);
};


static void thermal_buffer_turn_on(struct soc_thermal *mt)
{
	int temp = 0;

	temp = readl(mt->apmixed_base + mt->conf->TS2ADCMUX_Addr_offset);
	pr_info("Before write the register TS_CONx: 0x%x\n",
		temp);
	temp &= ~mt->conf->TS2ADCMUX_BITS;
	temp |= mt->conf->TS2ADCMUX_BITS & mt->conf->TS2ADCMUX_EnVAL;
	/* TS_CON0[29:28]=2'b00,   00: Buffer on, TSMCU to AUXADC */
	writel(temp, mt->apmixed_base + mt->conf->TS2ADCMUX_Addr_offset);
	udelay(200);
	temp = readl(mt->apmixed_base + mt->conf->TS2ADCMUX_Addr_offset);
	pr_info("After write the register TS_CONx: 0x%x\n",
		temp);
}


static void thermal_buffer_turn_off(struct soc_thermal *mt)
{
	int temp;

	temp = readl(mt->apmixed_base + mt->conf->TS2ADCMUX_Addr_offset);
	temp &= ~mt->conf->TS2ADCMUX_BITS;
	temp |= mt->conf->TS2ADCMUX_BITS & (~mt->conf->TS2ADCMUX_EnVAL);
	/* TS_CON0[29:28]=2'b00,   00: Buffer on, TSMCU to AUXADC */
	writel(temp, mt->apmixed_base + mt->conf->TS2ADCMUX_Addr_offset);
	udelay(200);
	pr_info("%s :TS_CONx=0x%x\n", __func__, temp);
}


/**
 * raw_to_mcelsius - convert a raw ADC value to mcelsius
 * @mt:		The thermal controller
 * @raw:	raw ADC value
 *
 * This converts the raw ADC value to mcelsius using the SoC specific
 * calibration constants
 */
static int raw_to_mcelsius(struct soc_thermal *mt, int sensno, s32 raw)
{
	s32 tmp;
	raw &= 0xfff;
	tmp = 100000 * 15/18 * 1000 * 10;
	tmp /= 4096 - 512 + mt->conf->adc_ge;
	tmp /= mt->conf->o_const + mt->conf->o_slope * 10;
	/*(mt->conf->o_slope * 10 = 165* 10 = 1650 = 0.00165 * 100000 * 10)*/
	tmp *= raw - mt->conf->vts[sensno] - mt->conf->o_offset;
	/*pr_info(" raw:0x%x, temp: %d\n", raw, (mt->conf->degc_cali * 500 - tmp));*/
	return (mt->conf->degc_cali * 500 - tmp);
}


/** * mcelsius_to_raw - convert mcelsius to a raw ADC value
 * @mt:		The thermal controller
 * @temp:	mcelsius
 *
 * This converts the raw ADC value to mcelsius using the SoC specific
 * calibration constants
 */
int mcelsius_to_raw(struct soc_thermal *mt, int sensno, s32 tmp)
{
	u32 raw;
	s32 tmp1, tmp2;

	tmp1 = 100000 * 15/18 * 1000 * 10;
	tmp1 /= mt->conf->o_const + mt->conf->o_slope * 10;
	tmp1 /= 4096 - 512 + mt->conf->adc_ge;
	tmp2 = mt->conf->degc_cali * 500 - tmp;
	raw = (tmp2 / tmp1) + mt->conf->vts[sensno] + mt->conf->o_offset;
	raw &= 0xfff;
	return raw;
}


/**
 * soc_thermal_get_bank - get bank
 * @bank:	The bank
 *
 * The bank registers are banked, we have to select a bank in the
 * PTPCORESEL register to access it.
 */
static void soc_thermal_get_bank(struct soc_thermal_bank *bank)
{
	struct soc_thermal *mt = bank->mt;
	mutex_lock(&mt->lock);
/*	mt->thermal_base = bank->bank_base;
 *
 *	val = readl(mt->thermal_base + PTPCORESEL);
 *	val &= ~0xf;
 *	val |= bank->id;
 *	writel(val, mt->thermal_base + PTPCORESEL);
 */
}


/**
 * soc_thermal_put_bank - release bank
 * @bank:	The bank
 *
 * release a bank previously taken with soc_thermal_get_bank,
 */
static void soc_thermal_put_bank(struct soc_thermal_bank *bank)
{
	struct soc_thermal *mt = bank->mt;
	mutex_unlock(&mt->lock);
}


/**
 * soc_thermal_bank_temperature - get the temperature of a bank
 * @bank:	The bank
 *
 * The temperature of a bank is considered the maximum temperature of
 * the sensors associated to the bank.
 */
static int soc_thermal_bank_temperature(struct soc_thermal_bank *bank)
{
	struct soc_thermal *mt = bank->mt;
	const struct soc_thermal_data *conf = mt->conf;
	int i, temp = INT_MIN, max = INT_MIN;
	u32 raw, sensor_id, cnt = 0;
	for (i = 0; i < conf->bank_data[bank->id].num_sensors; i++) {
		sensor_id = conf->bank_data[bank->id].sensors[i];
		raw = readl(mt->banks[bank->id].bank_base + conf->msr[i]);
		while (raw == 0 && cnt < 20) {
			udelay(500);
			raw = readl(mt->banks[bank->id].bank_base + conf->msr[i]);
		}
		if (!raw || !(raw & _BIT_(15))) {
			/* msr not ready or has been updated*/
			temp = conf->sen_data[sensor_id].temp;
			thermal_dprintk("raw = 0x%x not ready or has been updated!\n", raw);
		} else {
			temp = raw_to_mcelsius(mt, sensor_id, raw);
			conf->sen_data[sensor_id].msr_raw = raw;
			conf->sen_data[sensor_id].temp = temp;
		}
		/*
		 * The first read of a sensor often contains very high bogus
		 * temperature value. Filter these out so that the system does
		 * not immediately shut down.
		 */
		if (temp > 200000)
			temp = 0;
		thermal_dprintk("bank_id: %d, sensor_id: %d, temp: %d!\n",
			bank->id, sensor_id, temp);
		if (temp > max)
			max = temp;
	}
	return max;
}


static int soc_thermal_read_temp(void *data, int *temperature)
{
	struct sensor_data *ts_data = (struct sensor_data *)data;
	struct soc_thermal *mt = ts_data->soc_thermal;
	int i;
	static int tempmax = INT_MIN;
	static unsigned long update_flag;

	thermal_dprintk("%s, sensor_id: %d, sensor_flag: %lu, update_flag: %lu,\n ",
		__func__, ts_data->id, ts_data->update_flag,
		update_flag);

	if (update_flag > ts_data->update_flag) {
		ts_data->update_flag = update_flag;
		goto direct_update;
	}

	/*update all thermal sensor */
	tempmax = INT_MIN;
	for (i = 0; i < mt->conf->num_banks; i++) {
		struct soc_thermal_bank *bank = &mt->banks[i];
		/*soc_thermal_get_bank(bank);*/
		tempmax = max(tempmax, soc_thermal_bank_temperature(bank));
		/*soc_thermal_put_bank(bank);*/
	}

	update_flag == ~0U ? update_flag = 0U : ++update_flag;
	ts_data->update_flag = update_flag;

	thermal_printk("max_temp: %d\n", tempmax);

/* other thermal zone has update all sensor data,
 * so can read temp directly
 */
direct_update:
	if (ts_data->id == 0xFF) {
		mt->conf->vir_sen_data.temp = tempmax;
		*temperature = tempmax;
	} else if (ts_data->id  < mt->conf->num_sensors)
		*temperature = ts_data->temp;
	else
		return -EINVAL;
	return 0;
}


static const struct thermal_zone_of_device_ops soc_thermal_ops = {
	.get_temp = soc_thermal_read_temp,
};


static void soc_thermal_init_bank(struct soc_thermal *mt, int num,
				  u32 apmixed_phys_base, u32 auxadc_phys_base)
{
	struct soc_thermal_bank *bank = &mt->banks[num];
	const struct soc_thermal_data *conf = mt->conf;
	int i, temp;
	int sensor_id, protect_high_temp, protect_middle_temp;
	int protect_high_raw = 0xfff, protect_middle_raw = 0xfff;
	soc_thermal_get_bank(bank);
	/* bus clock 66M counting unit is 12 * 15.15ns * 256 = 46.540us */
	writel(mt->conf->tc_speed.tempMonCtl1,
			bank->bank_base + TEMP_MONCTL1);
	/*
	 * filt interval is 1 * 46.540us = 46.54us,
	 * sen interval is x* 46.540us = ms
	 */
	writel(mt->conf->tc_speed.tempMonCtl2,
			bank->bank_base + TEMP_MONCTL2);
	/* poll is set to 10u */
	writel(mt->conf->tc_speed.tempAhbPoll,
	       bank->bank_base + TEMP_AHBPOLL);
	/* temperature sampling control, 1 sample */
	writel(0x924, bank->bank_base + TEMP_MSRCTL0);
	/* exceed this polling time, IRQ would be inserted */
	writel(0xffffffff, bank->bank_base + TEMP_AHBTO);
	/* number of interrupts per event, 1 is enough */
	writel(0x0, bank->bank_base + TEMP_MONIDET0);
	writel(0x0, bank->bank_base + TEMP_MONIDET1);
	/*
	 * thermal controller does not have its own ADC. Instead it
	 * uses AHB bus accesses to control the AUXADC. To do this the thermal
	 * controller has to be programmed with the physical addresses of the
	 * AUXADC registers and with the various bit positions in the AUXADC.
	 * Also the thermal controller controls a mux in the APMIXEDSYS register
	 * space.
	 */
	/*
	 * this value will be stored to TEMP_PNPMUXADDR (TEMP_SPARE0)
	 * automatically by hw
	 */
	writel(BIT(conf->auxadc_channel), bank->bank_base + TEMP_ADCMUX);
	/* AHB address for auxadc mux selection */
	writel(auxadc_phys_base + AUXADC_CON1_CLR_V,
	       bank->bank_base + TEMP_ADCMUXADDR);
	/* AHB address for pnp sensor mux selection */
	writel(mt->apmixed_phys_base + mt->conf->TSVBE_SEL_Addr_offset,
	       bank->bank_base + TEMP_PNPMUXADDR);
	/*writel(apmixed_phys_base + APMIXED_SYS_TS_CON1,
	 *mt->thermal_base + TEMP_PNPMUXADDR);
	 */
	/* AHB value for auxadc enable */
	writel(BIT(conf->auxadc_channel), bank->bank_base + TEMP_ADCEN);
	/* AHB address for auxadc enable (channel 0 immediate mode selected) */
	writel(auxadc_phys_base + AUXADC_CON1_SET_V,
	       bank->bank_base + TEMP_ADCENADDR);
	/* AHB address for auxadc valid bit */
	writel(auxadc_phys_base + AUXADC_DATA(conf->auxadc_channel),
	       bank->bank_base + TEMP_ADCVALIDADDR);
	/* AHB address for auxadc voltage output */
	writel(auxadc_phys_base + AUXADC_DATA(conf->auxadc_channel),
	       bank->bank_base + TEMP_ADCVOLTADDR);
	/* read valid & voltage are at the same register */
	writel(0x0, bank->bank_base + TEMP_RDCTRL);
	/* indicate where the valid bit is */
	writel(TEMP_ADCVALIDMASK_VALID_HIGH | TEMP_ADCVALIDMASK_VALID_POS(12),
	       bank->bank_base + TEMP_ADCVALIDMASK);
	/* no shift */
	writel(0x0, bank->bank_base + TEMP_ADCVOLTAGESHIFT);
	/* enable auxadc mux write transaction */
	writel(TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       bank->bank_base + TEMP_ADCWRITECTRL);
	protect_high_temp = conf->bank_data[num].protect_high;
	protect_middle_temp = conf->bank_data[num].protect_middle;
	for (i = 0; i < conf->bank_data[num].num_sensors; i++) {
		sensor_id = conf->sensor_mux_values[conf->bank_data[num].sensors[i]];
		writel(sensor_id,
		       bank->bank_base + conf->adcpnp[i]);
		protect_high_raw = MIN(protect_high_raw,
			mcelsius_to_raw(mt, sensor_id, protect_high_temp));
		if (protect_middle_temp != -275000)
			protect_middle_raw = MIN(protect_middle_temp,
				mcelsius_to_raw(mt, sensor_id, protect_middle_temp));
	}
	/*disadle protect setting & int setting*/
	temp = readl(bank->bank_base + TEMP_MONINT);
	writel(temp & 0x00000000, bank->bank_base + TEMP_MONINT);
	/*select max temp to protect*/
	writel(0x10000, bank->bank_base + TEMP_PROTCTL);
	/*set protect value*/
	writel(protect_high_raw, bank->bank_base + TEMP_PROTTC);
	if (protect_middle_temp != -275000)
		writel(protect_middle_raw, bank->bank_base + TEMP_PROTTB);
	/*enable protect_c setting & int setting*/
	if (protect_middle_temp != -275000)
		writel(temp | 0xC0000000,
			bank->bank_base + TEMP_MONINT);
	else
		writel(temp | 0x80000000,
			bank->bank_base + TEMP_MONINT);
	writel((1 << conf->bank_data[num].num_sensors) - 1,
	       bank->bank_base + TEMP_MONCTL0);
	writel(TEMP_ADCWRITECTRL_ADC_PNP_WRITE |
	       TEMP_ADCWRITECTRL_ADC_MUX_WRITE,
	       bank->bank_base + TEMP_ADCWRITECTRL);
	soc_thermal_put_bank(bank);
}


static u64 of_get_phys_base(struct device_node *np)
{
	u64 size64;
	const __be32 *regaddr_p;
	regaddr_p = of_get_address(np, 0, &size64, NULL);
	if (!regaddr_p)
		return OF_BAD_ADDR;
	return of_translate_address(np, regaddr_p);
}


static int __attribute__((unused)) thermal_rst_init(struct platform_device *pdev,
						struct soc_thermal *mt)
{
	int num, i;
	struct device_node *np = pdev->dev.of_node;
	struct of_phandle_args args;

	num = of_property_count_strings(np, "therm-rst-name");
	mt->therm_rst_num = num;
	mt->therm_rst_ctrl = devm_kzalloc(&pdev->dev,
						sizeof(*mt->therm_rst_ctrl) * num,
						GFP_KERNEL);
	if (!mt->therm_rst_ctrl)
		return -ENODEV;
	for (i = 0; i < num; i++) {
		if (of_parse_phandle_with_fixed_args(np,
					"mediatek,therm-rst", 6, i, &args))
			return -EINVAL;
		mt->therm_rst_ctrl[i].rst_phys_base = of_get_phys_base(args.np);
		mt->therm_rst_ctrl[i].rst_base = of_iomap(args.np, 0);
		mt->therm_rst_ctrl[i].rst_set_offset = args.args[0];
		mt->therm_rst_ctrl[i].rst_set_bit = args.args[1];
		mt->therm_rst_ctrl[i].rst_clr_offset = args.args[2];
		mt->therm_rst_ctrl[i].rst_clr_bit = args.args[3];
		mt->therm_rst_ctrl[i].rst_sta_offset = args.args[4];
		mt->therm_rst_ctrl[i].rst_sta_bit = args.args[5];
	}
	return 0;
}


static void __attribute__((unused)) thermal_rst_trigger(struct soc_thermal *mt)
{
	int temp, i;

	for (i = 0; i < mt->therm_rst_num; i++) {
		temp = readl(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_set_offset);
		temp |= mt->therm_rst_ctrl[i].rst_set_bit;
		writel(temp, mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_set_offset);
		pr_info("%s: 0x%x, 0x%x\n", __func__,
			(long)(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_set_offset),
			readl(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_set_offset));
		temp = readl(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_clr_offset);
		temp |= mt->therm_rst_ctrl[i].rst_clr_bit;
		writel(temp, mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_clr_offset);
		pr_info("%s: 0x%x, 0x%x\n", __func__,
			(long)(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_clr_offset),
			readl(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_clr_offset));
		pr_info("%s: 0x%x, 0x%x\n", __func__,
			(long)(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_set_offset),
			readl(mt->therm_rst_ctrl[i].rst_base +
			mt->therm_rst_ctrl[i].rst_set_offset));
	}
}


static __attribute__((unused)) int thermal_parse_calibration_table(struct device *dev,
						  struct soc_thermal *mt, u32 *buf)
{
	struct device_node *np = dev->of_node;
	int i, ret, ncell;
	unsigned int *cell;
	unsigned int data;
	unsigned int (*cell_table)[3];
	const char *cali_data_name;
	const __be32 *prop;
	u64 buf_offset, msb, lsb;

	prop = of_get_property(np, "calibration-map", &ncell);
	/*4 bytes on prop in dts*/
	if (ncell / 4 % 3 || ncell == 0) {
		dev_notice(dev, "no lookup table, use default value\n");
		return -EINVAL;
	}

	ncell =  of_property_count_strings(np, "calibration-map-name");

	for (i = 0; i < ncell; i++) {
		of_property_read_string_index(np, "calibration-map-name",
						i, &cali_data_name);
		buf_offset = of_read_number(prop + i * 3, 1);
		msb = of_read_number(prop + i * 3 + 1, 1);
		lsb = of_read_number(prop + i * 3 + 2, 1);
		thermal_dprintk("buf_offset:%llu, msb:%llu, lsb:%llu\n", buf_offset, msb, lsb);
		data = MASK_FILED_VALUE(buf[buf_offset], msb, lsb);
		if (!strcmp(cali_data_name, "cali_en")) {
			mt->conf->o_cali_en = data;
			thermal_dprintk("efuse o_cali_en: %d\n", mt->conf->o_cali_en);
		} else if (!strcmp(cali_data_name, "adc_ge")) {
			mt->conf->adc_ge = data;
			thermal_dprintk("efuse adc_ge: %d\n", mt->conf->adc_ge);
		} else if (!strcmp(cali_data_name, "degc_cali")) {
			mt->conf->degc_cali = data;
			thermal_dprintk("efuse degc_cali: %d\n", mt->conf->degc_cali);
		} else if (!strcmp(cali_data_name, "slope_sign")) {
			mt->conf->o_slope_sign = data;
			thermal_dprintk("efuse o_slope_sign: %d\n", mt->conf->o_slope_sign);
		} else if (!strcmp(cali_data_name, "slope")) {
			mt->conf->o_slope = data;
			thermal_dprintk("efuse o_slope: %d\n", mt->conf->o_slope);
		} else if (!strcmp(cali_data_name, "id")) {
			mt->conf->o_cali_id = data;
			thermal_dprintk("efuse o_cali_id: %d\n", mt->conf->o_cali_id);
		}
	}

	if (!mt->conf->o_cali_en) {
		/* Start with default values */
		mt->conf->adc_ge = 512;
		for (i = 0; i < mt->conf->num_sensors; i++)
			mt->conf->vts[i] = 260;
		mt->conf->degc_cali = 40;
		mt->conf->o_slope = 0;
		dev_info(dev, "Device not calibrated, using default calibration values\n");
		return 0;
	}

	if (mt->conf->o_slope_sign == 1) {
		mt->conf->o_slope = -mt->conf->o_slope;
		thermal_dprintk("efuse o_slope: %d\n", mt->conf->o_slope);
	}

	if (mt->conf->o_cali_id == 0) {
		mt->conf->o_slope = 0;
		thermal_dprintk("efuse o_slope: %d\n", mt->conf->o_slope);
	}

	ncell = of_property_count_elems_of_size(np, "calibration-map-vts",
						 sizeof(u32));

	if (ncell <= 0) {
		dev_notice(dev, "no lookup table, use default value\n");
		return 0;
	}

	if ((ncell % 3) || (ncell / 3 != mt->conf->num_sensors)) {
		dev_err(dev, "calibration value not match!\n");
		return -EINVAL;
	}

	cell = devm_kcalloc(dev, ncell, sizeof(u32), GFP_KERNEL);
	if (!cell)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "calibration-map-vts",
					 cell, ncell);
	if (ret < 0) {
		dev_err(dev, "Failed to read temperature lookup table: %d\n",
			ret);
		return ret;
	}

	cell_table = (void *)cell;
	for (i = 0; i < mt->conf->num_sensors; i++) {
		buf_offset = (*cell_table)[0];
		msb = (*cell_table)[1];
		lsb = (*cell_table)[2];
		thermal_dprintk("buf_offset:%llu, msb:%llu, lsb:%llu\n", buf_offset, msb, lsb);
		data = MASK_FILED_VALUE(buf[buf_offset], msb, lsb);
		mt->conf->vts[i] = data;
		thermal_dprintk("efuse vts[%d]: 0x%0x\n", i, mt->conf->vts[i]);
		cell_table++;
	}

	return 0;
}


static int soc_thermal_get_calibration_data(struct device *dev,
					    struct soc_thermal *mt)
{
	struct nvmem_device *nvmem_dev;
	u32 buf[4];
	int i, ret;

	nvmem_dev = nvmem_device_get(dev, "efuse-data");
	if (IS_ERR(nvmem_dev)) {
		dev_info(dev, "Error: Failed to get nvmem %s from %s\n", "efuse-data", __func__);
		return PTR_ERR(nvmem_dev);
	}

	for (i = 0; i < 4; i++) {
		ret = nvmem_device_read(nvmem_dev, mt6768_efuse_offset[i],
				MT6768_EFUSE_SIZE, &buf[i]);
		if (ret != MT6768_EFUSE_SIZE) {
			thermal_dprintk("Read size: %d\n", ret);
			return -1;
		}
	}
	nvmem_device_put(nvmem_dev);

	thermal_dprintk("efuse: buf[0]: 0x%0x, buf[1]: 0x%0x, buf[2]: 0x%0x, buf[3]: 0x%0x\n",
				buf[0], buf[1], buf[2], buf[3]);
	thermal_parse_calibration_table(dev, mt, buf);

	return 0;
}

static const struct of_device_id soc_thermal_of_match[] = {
	{
		.compatible = "mediatek,mt6765-thermal",
		.data = (void *)&mt6765_thermal_data,
	},
	{
		.compatible = "mediatek,mt6768-thermal",
		.data = (void *)&mt6768_thermal_data,
	},
	{
		.compatible = "mediatek,mt8173-thermal",
		.data = (void *)&mt8173_thermal_data,
	},
	{
	},
};


MODULE_DEVICE_TABLE(of, soc_thermal_of_match);

static void __attribute__((unused)) thermal_interrupt_handler(int tc_id,  void *dev_id)
{
	u32 ret = 0;
	void __iomem *offset;
	unsigned long flags;
	struct soc_thermal *mt = (struct soc_thermal *)dev_id;

	offset = mt->banks[tc_id].bank_base;
	mt_thermal_lock(&flags);
	ret = readl(offset + TEMP_MONINTSTS);
	mt_thermal_unlock(&flags);
	pr_info("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	pr_info("%s,ret=0x%08x\n", __func__, ret);
	pr_info("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
	if (ret & THERMAL_tri_SPM_State0)
		pr_info(
			"thermal_isr: Thermal state0 to trigger SPM state0\n");
	if (ret & THERMAL_tri_SPM_State1)
		pr_info(
			"thermal_isr: Thermal state1 to trigger SPM state1\n");
	if (ret & THERMAL_tri_SPM_State2)
		pr_info(
			"thermal_isr: Thermal state2 to trigger SPM state2\n");
}


static irqreturn_t __attribute__((unused)) irq_handler(int irq, void *dev_id)
{
	struct soc_thermal *mt = (struct soc_thermal *)dev_id;
	unsigned int ret = 0, i  = 0, mask = 1;
	unsigned long flags;

	mt_thermal_lock(&flags);
	ret = readl(mt->thermal_base + THERMINTST);
	ret = ret & 0xF;
	pr_info("thermal_interrupt_handler : THERMINTST = 0x%x\n", ret);
	mt_thermal_unlock(&flags);
	for (i = 0; i < mt->conf->num_banks; i++) {
		mask = 1 << i;
		if ((ret & mask) == 0)
			thermal_interrupt_handler(i, dev_id);
	}
	return IRQ_HANDLED;
}


/**************************
 * Thermal Debug  OPS
 **************************
 */
static ssize_t thermal_debug_proc_write(struct file *file,
				const char *buffer, size_t count, loff_t *data)
{
	int ret, cmd;
	char kbuf[256];
	size_t len = 0;

	len = min(count, (sizeof(kbuf) - 1));
	pr_info("count: %d", count);
	if (count == 0)
		return -1;
	if (count > 255)
		count = 255;
	ret = copy_from_user(kbuf, buffer, count);
	if (ret < 0)
		return -1;
	kbuf[count] = '\0';
	ret = kstrtoint(kbuf, 10, &cmd);
	if (ret)
		return ret;
	thermal_debug_log = cmd;
	return count;
}


static int thermal_debug_proc_read(struct seq_file *m, void *v)
{
	seq_printf(m, "====== thermal_debug_log: %d ======\n",
		thermal_debug_log);
	return 0;
}


static int thermal_debug_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, thermal_debug_proc_read, inode->i_private);
}


static const struct file_operations thermal_debug_fops = {
	.owner = THIS_MODULE,
	.open = thermal_debug_proc_open,
	.read = seq_read,
	.write = thermal_debug_proc_write,
	.release = single_release,
};


static void soc_thermal_debugfs(struct platform_device *pdev)
{
	struct dentry *root;
	struct dentry *thermal_debug;
	struct device *dev = &pdev->dev;

	root = debugfs_create_dir("thermal", NULL);
	if (IS_ERR(root)) {
		dev_info(dev, "create debugfs fail");
		return;
	}
	/* /sys/kernel/debug/thermal/thermal_debug */
	thermal_debug = debugfs_create_file("thermal_debug", 0664,
						root, NULL, &thermal_debug_fops);
	if (IS_ERR(thermal_debug)) {
		dev_info(dev, "failed to create ctrl debugfs");
		return;
	}
	dev_dbg(dev, "Create debugfs success!");
}


static int soc_thermal_probe(struct platform_device *pdev)
{
	int ret, i;
	struct device_node *auxadc, *apmixedsys, *np = pdev->dev.of_node;
	struct soc_thermal *mt;
	struct resource *res;
	struct thermal_zone_device *tzdev;
	struct soc_thermal_data *conf;

	conf = (struct soc_thermal_data *)of_device_get_match_data(&pdev->dev);
	if (!conf)
		return -ENODEV;

	conf->sen_data = devm_kzalloc(&pdev->dev,
						sizeof(*conf->sen_data) * conf->num_sensors,
						GFP_KERNEL);
	if (!conf->sen_data)
		return -ENODEV;

	mt = devm_kzalloc(&pdev->dev,
		sizeof(*mt) + conf->num_banks * sizeof(struct soc_thermal_bank),
		GFP_KERNEL);
	if (!mt)
		return -ENOMEM;
	mt->conf = conf;

	mt->clk_peri_therm = devm_clk_get(&pdev->dev, "therm");
	if (IS_ERR(mt->clk_peri_therm))
		return PTR_ERR(mt->clk_peri_therm);

	mt->clk_auxadc = devm_clk_get(&pdev->dev, "auxadc");
	if (IS_ERR(mt->clk_auxadc))
		return PTR_ERR(mt->clk_auxadc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mt->thermal_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mt->thermal_base))
		return PTR_ERR(mt->thermal_base);

	ret = soc_thermal_get_calibration_data(&pdev->dev, mt);
	if (ret)
		return ret;

	mutex_init(&mt->lock);

	mt->dev = &pdev->dev;

	auxadc = of_parse_phandle(np, "mediatek,auxadc", 0);
	if (!auxadc) {
		dev_err(&pdev->dev, "missing auxadc node\n");
		return -ENODEV;
	}

	mt->auxadc_phys_base = of_get_phys_base(auxadc);
	mt->auxadc_base = of_iomap(auxadc, 0);

	of_node_put(auxadc);

	if (mt->auxadc_phys_base == OF_BAD_ADDR) {
		dev_err(&pdev->dev, "Can't get auxadc phys address\n");
		return -EINVAL;
	}

	apmixedsys = of_parse_phandle(np, "mediatek,apmixedsys", 0);
	if (!apmixedsys) {
		dev_err(&pdev->dev, "missing apmixedsys node\n");
		return -ENODEV;
	}

	mt->apmixed_phys_base = of_get_phys_base(apmixedsys);
	mt->apmixed_base = of_iomap(apmixedsys, 0);

	of_node_put(apmixedsys);

	if (mt->apmixed_phys_base == OF_BAD_ADDR) {
		dev_err(&pdev->dev, "Can't get auxadc phys address\n");
		return -EINVAL;
	}

	thermal_rst_init(pdev, mt);
	thermal_rst_trigger(mt);

	ret = clk_prepare_enable(mt->clk_auxadc);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable auxadc clk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(mt->clk_peri_therm);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable peri clk: %d\n", ret);
		goto err_disable_clk_auxadc;
	}

	thermal_buffer_turn_on(mt);
	udelay(200);
	for (i = 0; i < mt->conf->num_banks; i++) {
		mt->banks[i].id = i;
		mt->banks[i].mt = mt;
		mt->banks[i].bank_base =
			mt->thermal_base + mt->conf->bank_data[i].bank_offset;
	}
	for (i = 0; i < mt->conf->num_banks; i++)
		soc_thermal_init_bank(mt, i, mt->apmixed_phys_base,
				      mt->auxadc_phys_base);
	mdelay(20);

	platform_set_drvdata(pdev, mt);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "No irq resource, index %d\n", 0);
		return -EINVAL;
	}
	ret = devm_request_irq(&pdev->dev, res->start, irq_handler,
				IRQF_TRIGGER_HIGH, "soc_temp", mt);
	if (ret) {
		dev_err(&pdev->dev, "failed to register irq (%d)\n", ret);
		return -EINVAL;
	}

	soc_thermal_debugfs(pdev);

	conf->vir_sen_data.id = 0xFF;
	conf->vir_sen_data.soc_thermal = mt;
	tzdev = devm_thermal_zone_of_sensor_register(&pdev->dev,
			conf->vir_sen_data.id,
			&conf->vir_sen_data,
			&soc_thermal_ops);
	if (IS_ERR(tzdev)) {
		dev_err(&pdev->dev, "Can't register soc_max sensor\n");
		ret = PTR_ERR(tzdev);
		goto err_disable_clk_peri_therm;
	}

	for (i = 0; i < mt->conf->num_sensors ; i++) {
		conf->sen_data[i].id = i;
		conf->sen_data[i].soc_thermal = mt;
		tzdev = devm_thermal_zone_of_sensor_register(&pdev->dev, i, &conf->sen_data[i],
								 &soc_thermal_ops);
		if (IS_ERR(tzdev))
			return 0;
	}

	return 0;
err_disable_clk_peri_therm:
	clk_disable_unprepare(mt->clk_peri_therm);
err_disable_clk_auxadc:
	clk_disable_unprepare(mt->clk_auxadc);

	return ret;
}


static int soc_thermal_remove(struct platform_device *pdev)
{
	struct soc_thermal *mt = platform_get_drvdata(pdev);
	clk_disable_unprepare(mt->clk_peri_therm);
	clk_disable_unprepare(mt->clk_auxadc);
	return 0;
}


static int soc_thermal_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct soc_thermal *mt = platform_get_drvdata(pdev);
	int temp, i, cnt = 0;
	/* disable periodic temp measurement on sensor 0~x */
	for (i = 0; i < mt->conf->num_banks; i++)
		writel(0, mt->banks[i].bank_base + TEMP_MONCTL0);
	while (cnt < 50) {
		temp = (readl(mt->banks[0].bank_base + TEMP_AHBST) >> 16);
		if (temp == 0x0)/* bus no data handle and transfer */
			break;
		udelay(2);
		cnt++;
	}
	clk_disable_unprepare(mt->clk_peri_therm);
	clk_disable_unprepare(mt->clk_auxadc);
	thermal_buffer_turn_off(mt);
	return 0;
}


static int soc_thermal_resume(struct platform_device *pdev)
{
	struct soc_thermal *mt = platform_get_drvdata(pdev);
	int ret, i;

	thermal_rst_trigger(mt);
	ret = clk_prepare_enable(mt->clk_auxadc);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable auxadc clk: %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(mt->clk_peri_therm);
	if (ret) {
		dev_err(&pdev->dev, "Can't enable peri clk: %d\n", ret);
		return ret;
	}
	thermal_buffer_turn_on(mt);
	udelay(200);
	for (i = 0; i < mt->conf->num_banks; i++)
		soc_thermal_init_bank(mt, i, mt->apmixed_phys_base,
				      mt->auxadc_phys_base);
	udelay(500);
	return 0;
}


static struct platform_driver soc_thermal_driver = {
	.probe = soc_thermal_probe,
	.remove = soc_thermal_remove,
	.suspend = soc_thermal_suspend,
	.resume = soc_thermal_resume,
	.driver = {
		.name = "mtk-soc-thermal",
		.of_match_table = soc_thermal_of_match,
	},
};

module_platform_driver(soc_thermal_driver);

MODULE_AUTHOR("Louis Yu <louis.yu@mediatek.com>");
MODULE_AUTHOR("Dawei Chien <dawei.chien@mediatek.com>");
MODULE_AUTHOR("Sascha Hauer <s.hauer@pengutronix.de>");
MODULE_AUTHOR("Hanyi Wu <hanyi.wu@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC thermal driver");
MODULE_LICENSE("GPL v2");
