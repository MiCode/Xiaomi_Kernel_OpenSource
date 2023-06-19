// SPDX-License-Identifier: GPL-2.0
//
// LN8000 switched-cap charger driver
//
// Copyright © 2021 Cirrus Logic Incorporated - https://cirrus.com

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include "ln8000_charger.h"
#include "ln8000_iio.h"
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>

static const char *ln8000_dev_name[] = {
	"ln8000-standalone",
	"ln8000-master",
	"ln8000-slave",
};

//add ipc log start
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
	#if IS_ENABLED(CONFIG_DEBUG_OBJECTS)
		#define IPC_CHARGER_DEBUG_LOG
	#endif
#endif

#ifdef IPC_CHARGER_DEBUG_LOG
extern void *charger_ipc_log_context;

#define ln8000_err(fmt,...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define ln_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "ln8000: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}
#undef pr_err
#define pr_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "ln8000: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}
#undef pr_info
#define pr_info(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_INFO pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "ln8000: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}
#define ln_info(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_INFO pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "ln8000: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#else
#define ln8000_err(fmt,...)
//add ipc log end
#define ln_err(fmt, ...)                       \
do {                                            \
	if (info->dev_role == LN_ROLE_STANDALONE)   \
		printk(KERN_ERR "ln8000-standalone: %s: " fmt, __func__, ##__VA_ARGS__);   \
	else if (info->dev_role == LN_ROLE_MASTER)                              \
		printk(KERN_ERR "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__);   \
	else                                                                    \
		printk(KERN_ERR "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__);   \
} while (0);

#define ln_info(fmt, ...)                       \
do {                                            \
	if (info->dev_role == LN_ROLE_STANDALONE)   \
		printk(KERN_INFO "ln8000-standalone: %s: " fmt, __func__, ##__VA_ARGS__);  \
	else if (info->dev_role == LN_ROLE_MASTER)                              \
		printk(KERN_INFO "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__);  \
	else                                                                    \
		printk(KERN_INFO "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__);  \
} while (0);
#endif
#define ln_dbg(fmt, ...)                        \
do {                                            \
	if (info->dev_role == LN_ROLE_STANDALONE)   \
		printk(KERN_DEBUG "ln8000-standalone: %s: " fmt, __func__, ##__VA_ARGS__); \
	else if (info->dev_role == LN_ROLE_MASTER)                              \
		printk(KERN_DEBUG "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__); \
	else                                                                    \
		printk(KERN_DEBUG "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__); \
} while (0);

#define LN8000_REG_PRINT(reg_addr, val)                         \
do {                                                            \
	ln_info("  --> [%-20s]   0x%02X   :   0x%02X\n",            \
		#reg_addr, LN8000_REG_##reg_addr, (val) & 0xFF);    \
} while (0);

#define LN8000_PARSE_PROP(ret, pdata, field, prop, default_prop)        \
do {                                                                    \
	if (ret) {                                                          \
	    ln_info("%s = %d (set to default)\n", #field, default_prop);    \
	    pdata->field = default_prop;                                    \
	} else {                                                            \
	    ln_info("%s = %d\n", #field, prop);                             \
	    pdata->field = prop;                                            \
	}                                                                   \
} while (0);


#define LN8000_BIT_CHECK(val, idx, desc) if (val & (1<<idx)) ln_info("-> %s\n", desc)
#define LN8000_USE_GPIO(pdata) ((pdata != NULL) && (!IS_ERR_OR_NULL(pdata->irq_gpio)))
#define LN8000_STATUS(val, mask) ((val & mask) ? true : false)
static int hv_charge_enable = 1;
/**
 * I2C control functions : when occurred I2C tranfer fault, we
 * will retry to it. (default count:3)
 */
#define I2C_RETRY_CNT   3
static int ln8000_read_reg(struct ln8000_info *info, u8 addr, u8 *data)
{
	int i, ret = 0;

        for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(info->client, addr);
		if (IS_ERR_VALUE((unsigned long)ret)) {
			ln_info("failed-read, reg(0x%02X), ret(%d)\n", addr, ret);
		} else {
			*data = (u8)ret;
		}
	}
	return ret;
}

static int ln8000_bulk_read_reg(struct ln8000_info *info, u8 addr, void *data, int count)
{
	int i, ret = 0;

        for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_i2c_block_data(info->client, addr, count, data);
		if (IS_ERR_VALUE((unsigned long)ret)) {
			ln_info("failed-bulk-read, reg(0x%02X, %d bytes), ret(%d)\n", addr, count, ret);
		} else {
			break;
		}
	}
	return ret;
}

static int ln8000_write_reg(struct ln8000_info *info, u8 addr, u8 data)
{
	int i, ret = 0;

        for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_write_byte_data(info->client, addr, data);
		if (IS_ERR_VALUE((unsigned long)ret)) {
			ln_info("failed-write, reg(0x%02X), ret(%d)\n", addr, ret);
		} else {
			break;
		}
	}
	return ret;
}

static int ln8000_update_reg(struct ln8000_info *info, u8 addr, u8 mask, u8 data)
{
	int i, ret;
	u8 old_val, new_val;

        for (i = 0; i < I2C_RETRY_CNT; ++i) {
		ret = i2c_smbus_read_byte_data(info->client, addr);
		if (ret < 0) {
			ln_err("failed-update, reg(0x%02X), ret(%d)\n", addr, ret);
		} else {
			old_val = ret & 0xff;
			new_val = (data & mask) | (old_val & ~(mask));
			ret = i2c_smbus_write_byte_data(info->client, addr,
							new_val);
			if (ret < 0) {
				ln_err("failed-update, reg(0x%02X), ret(%d)\n",
				       addr, ret);
			} else {
				break;
			}
		}
	}
	return ret;
}

/**
 * Register control functions
 */
#if 0
static int ln8000_set_sw_freq(struct ln8000_info *info, u8 fsw_cfg)
{
	return ln8000_update_reg(info, LN8000_REG_SYS_CTRL, 0xF << 4, fsw_cfg << 4);
}
#endif

static int ln8000_set_vac_ovp(struct ln8000_info *info, unsigned int ovp_th)
{
	u8 cfg;

	if (ovp_th <= 6500000) {
		cfg = LN8000_VAC_OVP_6P5V;
	} else if (ovp_th <= 11000000) {
		cfg = LN8000_VAC_OVP_11V;
	} else if (ovp_th <= 12000000) {
		cfg = LN8000_VAC_OVP_12V;
	} else {
		cfg = LN8000_VAC_OVP_13V;
	}

	return ln8000_update_reg(info, LN8000_REG_GLITCH_CTRL, 0x3 << 2, cfg << 2);
}

/* battery float voltage */
static int ln8000_set_vbat_float(struct ln8000_info *info, unsigned int cfg)
{
	u8 val;
	unsigned int adj_cfg = cfg - 30000;     /* adjust v_float bg offset (-30mV) */

	ln_info("ori_cfg=%d, adj_cfg=%d\n", cfg, adj_cfg);

	if (adj_cfg < LN8000_VBAT_FLOAT_MIN)
		val = 0x00;
	else if (adj_cfg > LN8000_VBAT_FLOAT_MAX)
		val = 0xFF;
	else
		val = (adj_cfg - LN8000_VBAT_FLOAT_MIN) / LN8000_VBAT_FLOAT_LSB;

	return ln8000_write_reg(info, LN8000_REG_V_FLOAT_CTRL, val);
}

static int ln8000_set_iin_limit(struct ln8000_info *info, unsigned int cfg)
{
	u8 val = cfg / LN8000_IIN_CFG_LSB;

	return ln8000_update_reg(info, LN8000_REG_IIN_CTRL, 0x7F, val);
}

static int ln8000_set_ntc_alarm(struct ln8000_info *info, unsigned int cfg)
{
	int ret;

	/* update lower bits */
	ret = ln8000_write_reg(info, LN8000_REG_NTC_CTRL, (cfg & 0xFF));
	if (ret < 0)
		return ret;

	/* update upper bits */
	ret = ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x3, (cfg >> 8));
	return ret;
}

/* battery voltage OV protection */
static int ln8000_enable_vbat_ovp(struct ln8000_info *info, bool enable)
{
	u8 val;

	val = (enable) ? 0 : 1;//disable
	val <<= LN8000_BIT_DISABLE_VBAT_OV;

	return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL, BIT(LN8000_BIT_DISABLE_VBAT_OV), val);
}

/* input current OC protection */
static int ln8000_enable_iin_ocp(struct ln8000_info *info, bool enable)
{
	return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL,
				0x1 << LN8000_BIT_DISABLE_IIN_OCP,
				!(enable) << LN8000_BIT_DISABLE_IIN_OCP);
}

static int ln8000_enable_ocp(struct ln8000_info *info, bool enable)
{
	int ret = 0;

	if (!info->pdata->iin_ocp_disable) {
		ret = ln8000_enable_iin_ocp(info, enable);
	}

	return ret;
}

static int ln8000_enable_vac_ov(struct ln8000_info *info, bool enable)
{
	return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL,
				0x1 << LN8000_BIT_DISABLE_VAC_OV,
				!(enable) << LN8000_BIT_DISABLE_VAC_OV);
}

static int ln8000_enable_vin_uv_track(struct ln8000_info *info, bool enable)
{
	return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL, 0x1 << 0, !(enable) << 0);
}

static int ln8000_enable_tdie_prot(struct ln8000_info *info, bool enable)
{
	return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
				0x1 << LN8000_BIT_TEMP_MAX_EN, enable << LN8000_BIT_TEMP_MAX_EN);
}

static int ln8000_enable_tdie_regulation(struct ln8000_info *info, bool enable)
{
	return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
				0x1 << LN8000_BIT_TEMP_REG_EN, enable << LN8000_BIT_TEMP_REG_EN);
}

/* ADC channel enable */
static int ln8000_set_adc_ch(struct ln8000_info *info, unsigned int ch, bool enable)
{
	u8 mask;
	u8 val;
	int ret;

	if ((ch > LN8000_ADC_CH_ALL) || (ch < 1))
		return -EINVAL;

	if (ch == LN8000_ADC_CH_ALL) {
		// update all channels
		val  = (enable) ? 0x3E : 0x00;
		ret  = ln8000_write_reg(info, LN8000_REG_ADC_CFG, val);
	} else {
		// update selected channel
		mask = 1<<(ch-1);
		val  = (enable) ? 1 : 0;
		val <<= (ch-1);
		ret  = ln8000_update_reg(info, LN8000_REG_ADC_CFG, mask, val);
	}

	return ret;
}

/* BUS temperature monitoring (protection+alarm) */
static int ln8000_enable_tbus_monitor(struct ln8000_info *info, bool enable)
{
	int ret;

	/* enable BUS monitoring */
	ret = ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 1, enable << 1);
	if (ret < 0)
		return ret;

	/* enable BUS ADC channel */
	if (enable) {
		ret = ln8000_set_adc_ch(info, LN8000_ADC_CH_TSBUS, true);
	}
	return ret;
}

/* BAT temperature monitoring (protection+alarm) */
static int ln8000_enable_tbat_monitor(struct ln8000_info *info, bool enable)
{
	int ret;

	/* enable BAT monitoring */
	ret = ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 0, enable << 0);
	if (ret < 0)
		return ret;

	/* enable BAT ADC channel */
	if (enable) {
		ret = ln8000_set_adc_ch(info, LN8000_ADC_CH_TSBAT, true);
	}
	return ret;
}

/* watchdog timer */
static int ln8000_enable_wdt(struct ln8000_info *info, bool enable)
{
	return ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 7, enable << 7);
}

#if 0
static int ln8000_set_wdt(struct ln8000_info *info, unsigned int cfg)
{
	if (cfg >= LN8000_WATCHDOG_MAX) {
		cfg = LN8000_WATCHDOG_40SEC;
	}

	return ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x3 << 5, cfg << 5);
}
#endif

/* unplug / reverse-current detection */
static int ln8000_enable_rcp(struct ln8000_info *info, bool enable)
{
	info->rcp_en = enable;

	return ln8000_update_reg(info, LN8000_REG_SYS_CTRL,
				BIT(LN8000_BIT_REV_IIN_DET),
				enable << LN8000_BIT_REV_IIN_DET);
}

/* auto-recovery */
static int ln8000_enable_auto_recovery(struct ln8000_info *info, bool enable)
{
	return ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0xF << 4, ((0xF << 4) * enable));
}

static int ln8000_set_adc_mode(struct ln8000_info *info, unsigned int cfg)
{
	return ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x7 << 5, cfg << 5);
}

static int ln8000_set_adc_hib_delay(struct ln8000_info *info, unsigned int cfg)
{
	return ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x3 << 3, cfg << 3);
}

static int ln8000_set_ovpfet_high_imp(struct ln8000_info *info, bool enable)
{
	int ret;

	ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0xAA);
	ret = ln8000_update_reg(info, LN8000_REG_PRODUCT_ID, 0x1 << 5, enable << 5);
	ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0x00);

	return ret;
}

/* grab programmed battery float voltage (uV) */
static int ln8000_get_vbat_float(struct ln8000_info *info)
{
	int ret;
	u8 val;

	ret = ln8000_read_reg(info, LN8000_REG_V_FLOAT_CTRL, &val);
	if (ret < 0)
		return ret;

	return ((val & 0xFF) * LN8000_VBAT_FLOAT_LSB  + LN8000_VBAT_FLOAT_MIN);//uV
}

/* grab programmed input current limit (uA) */
static int ln8000_get_iin_limit(struct ln8000_info *info)
{
	int ret, iin;
	u8 val;

	ret = ln8000_read_reg(info, LN8000_REG_IIN_CTRL, &val);
	if (ret < 0)
		return ret;

	iin = ((val & 0x7F) * LN8000_IIN_CFG_LSB);

	if (iin < LN8000_IIN_CFG_MIN) {
		iin = LN8000_IIN_CFG_MIN;
	}

	return iin;
}

/* enable/disable STANDBY */
static inline void ln8000_sw_standby(struct ln8000_info *info, bool standby)
{
	u8 val = (standby) ? BIT(LN8000_BIT_STANDBY_EN) : 0x00;

	ln8000_update_reg(info, LN8000_REG_SYS_CTRL, BIT(LN8000_BIT_STANDBY_EN), val);
}

/* Convert Raw ADC Code */
static void ln8000_convert_adc_code(struct ln8000_info *info, unsigned int ch, u8 *sts, int *result)
{
	int adc_raw;	// raw ADC value
	int adc_final;	// final (converted) ADC value

	switch (ch) {
	case LN8000_ADC_CH_VOUT:
		adc_raw   = ((sts[1] & 0xFF)<<2) | ((sts[0] & 0xC0)>>6);
		adc_final = adc_raw * LN8000_ADC_VOUT_STEP;//uV
		break;
	case LN8000_ADC_CH_VIN:
		adc_raw   = ((sts[1] & 0x3F)<<4) | ((sts[0] & 0xF0)>>4);
		adc_final = adc_raw * LN8000_ADC_VIN_STEP;//uV
		break;
	case LN8000_ADC_CH_VBAT:
		adc_raw   = ((sts[1] & 0x03)<<8) | (sts[0] & 0xFF);
		adc_final = adc_raw * LN8000_ADC_VBAT_STEP;//uV
		break;
	case LN8000_ADC_CH_VAC:
		adc_raw   = (((sts[1] & 0x0F)<<6) | ((sts[0] & 0xFC)>>2)) + LN8000_ADC_VAC_OS;
		adc_final = adc_raw * LN8000_ADC_VAC_STEP;//uV
		break;
	case LN8000_ADC_CH_IIN:
		adc_raw   = ((sts[1] & 0x03)<<8) | (sts[0] & 0xFF);
		adc_final = adc_raw * LN8000_ADC_IIN_STEP;//uA
		break;
	case LN8000_ADC_CH_DIETEMP:
		adc_raw   = ((sts[1] & 0x0F)<<6) | ((sts[0] & 0xFC)>>2);
		adc_final = (935 - adc_raw) * 1000 / 2300;//dC
		if (adc_final > LN8000_ADC_DIETEMP_MAX)
			adc_final = LN8000_ADC_DIETEMP_MAX;
		else if (adc_final < LN8000_ADC_DIETEMP_MIN)
			adc_final = LN8000_ADC_DIETEMP_MIN;
		break;
	case LN8000_ADC_CH_TSBAT:
		adc_raw   = ((sts[1] & 0x3F)<<4) | ((sts[0] & 0xF0)>>4);
		adc_final = adc_raw * LN8000_ADC_NTCV_STEP;//(NTC) uV
		break;
	case LN8000_ADC_CH_TSBUS:
		adc_raw   = ((sts[1] & 0xFF)<<2) | ((sts[0] & 0xC0)>>6);
		adc_final = adc_raw * LN8000_ADC_NTCV_STEP;//(NTC) uV
		break;
	default:
		adc_raw   = -EINVAL;
		adc_final = -EINVAL;
		break;
	}

	*result = adc_final;
	return;
}

static void ln8000_print_regmap(struct ln8000_info *info)
{
	const u8 print_reg_num = (LN8000_REG_CHARGE_CTRL - LN8000_REG_INT1_MSK) + 1;
	u8 regs[64] = {0x0, };
	char temp_buf[128] = {0,};
	int i, ret;

	for (i = 0; i < print_reg_num; ++i) {
		ret = ln8000_read_reg(info, LN8000_REG_INT1_MSK + i, &regs[i]);
		if (IS_ERR_VALUE((unsigned long)ret)) {
			ln_err("fail to read reg for print_regmap[%d]\n", i);
			regs[i] = 0xFF;
		}
		sprintf(temp_buf + strlen(temp_buf), "0x%02X[0x%02X],", LN8000_REG_INT1_MSK + i, regs[i]);
		if (((i+1) % 10 == 0) || ((i+1) == print_reg_num)) {
			ln_info("%s\n", temp_buf);
			memset(temp_buf, 0x0, sizeof(temp_buf));
		}
	}
	ln8000_read_reg(info, LN8000_REG_BC_OP_1, &regs[0]);
	ln8000_read_reg(info, LN8000_REG_PRODUCT_ID, &regs[1]);
	ln8000_read_reg(info, LN8000_REG_BC_STS_B, &regs[2]);
	ln_info("dual-config: 0x41=[0x%x], 0x31=[0x%x], 0x4A=[0x%x]\n", regs[0], regs[1], regs[2]);
}

/**
 * LN8000 device driver control routines
 */
static int ln8000_check_status(struct ln8000_info *info)
{
	u8 val[4];

	if (ln8000_bulk_read_reg(info, LN8000_REG_SYS_STS, val, 4) < 0) {
		return -EINVAL;
	}

	mutex_lock(&info->data_lock);

	info->vbat_regulated  = LN8000_STATUS(val[0], LN8000_MASK_VFLOAT_LOOP_STS);
	info->iin_regulated   = LN8000_STATUS(val[0], LN8000_MASK_IIN_LOOP_STS);
	info->pwr_status      = val[0] & (LN8000_MASK_BYPASS_ENABLED | LN8000_MASK_SWITCHING_ENABLED | \
				      LN8000_MASK_STANDBY_STS | LN8000_MASK_SHUTDOWN_STS);
	info->tdie_fault      = LN8000_STATUS(val[1], LN8000_MASK_TEMP_MAX_STS);
	info->tdie_alarm      = LN8000_STATUS(val[1], LN8000_MASK_TEMP_REGULATION_STS);
	if (!info->pdata->tbat_mon_disable || !info->pdata->tbus_mon_disable) {
		info->tbus_tbat_fault = LN8000_STATUS(val[1], LN8000_MASK_NTC_SHUTDOWN_STS); //tbus or tbat
		info->tbus_tbat_alarm = LN8000_STATUS(val[1], LN8000_MASK_NTC_ALARM_STS);//tbus or tbat
	}
	info->iin_rc          = LN8000_STATUS(val[1], LN8000_MASK_REV_IIN_STS);

	info->wdt_fault  = LN8000_STATUS(val[2], LN8000_MASK_WATCHDOG_TIMER_STS);
	info->vbat_ov    = LN8000_STATUS(val[2], LN8000_MASK_VBAT_OV_STS);
	info->vac_unplug = LN8000_STATUS(val[2], LN8000_MASK_VAC_UNPLUG_STS);
	info->vac_ov     = LN8000_STATUS(val[2], LN8000_MASK_VAC_OV_STS);
	info->vbus_ov    = LN8000_STATUS(val[2], LN8000_MASK_VIN_OV_STS);
	info->volt_qual  = !(LN8000_STATUS(val[2], 0x7F));
	if (info->volt_qual == 1 && info->chg_en == 1) {
		info->volt_qual = !(LN8000_STATUS(val[3], 1 << 5));
		if (info->volt_qual == 0) {
			ln_info("volt_fault_detected (volt_qual=%d)\n", info->volt_qual);
			/* clear latched status */
			ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 2, 0x1 << 2);
			ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 2, 0x0 << 2);
		}
	}
	info->iin_oc     = LN8000_STATUS(val[3], LN8000_MASK_IIN_OC_DETECTED);

	mutex_unlock(&info->data_lock);

	return 0;
}

static void ln8000_irq_sleep(struct ln8000_info *info, int suspend)
{
	if (info->client->irq <= 0)
		return;

	if (suspend) {
		ln_info("disable/suspend IRQ\n");
		disable_irq(info->client->irq);
	} else {
		ln_info("enable/resume IRQ\n");
		enable_irq(info->client->irq);
	}
}

static void ln8000_soft_reset(struct ln8000_info *info)
{
	ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0xC6);

	ln8000_irq_sleep(info, 1);

	ln_info("Trigger soft-reset\n");
	ln8000_update_reg(info, LN8000_REG_BC_OP_2, 0x1 << 0, 0x1 << 0);
	msleep(5 * 2);  /* ln8000 min wait time 5ms (after POR) */

	ln8000_irq_sleep(info, 0);
}

static void ln8000_update_opmode(struct ln8000_info *info)
{
	unsigned int op_mode;
	u8 val;

	/* chack mode status */
	ln8000_read_reg(info, LN8000_REG_SYS_STS, &val);
	if (val == 0x0) {
		/* wait for translate state. */
		msleep(5);
		ln8000_read_reg(info, LN8000_REG_SYS_STS, &val);
	}

	if (val & LN8000_MASK_SHUTDOWN_STS) {
		op_mode = LN8000_OPMODE_STANDBY;
	} else if (val & LN8000_MASK_STANDBY_STS) {
		op_mode = LN8000_OPMODE_STANDBY;
	} else if (val & LN8000_MASK_SWITCHING_ENABLED) {
		op_mode = LN8000_OPMODE_SWITCHING;
	} else if (val & LN8000_MASK_BYPASS_ENABLED) {
		op_mode = LN8000_OPMODE_BYPASS;
	} else {
		op_mode = LN8000_OPMODE_UNKNOWN;
	}

	if (info->op_mode == LN8000_OPMODE_BYPASS) {
		/* recovery(enable) VIN_UV_TRACK default */
		ln8000_enable_vin_uv_track(info, 1);
	}
	if (op_mode != info->op_mode) {
		/* IC already has been entered standby_mode, need to trigger standbt_en bit */
		if (op_mode == LN8000_OPMODE_STANDBY) {
			ln8000_update_reg(info, LN8000_REG_SYS_CTRL, 1 << LN8000_BIT_STANDBY_EN, 1 << LN8000_BIT_STANDBY_EN);
			ln_info("forced trigger standby_en\n");
			info->chg_en = 0;
		}
		ln_info("op_mode has been changed [%d]->[%d] (sys_st=0x%x)\n", info->op_mode, op_mode, val);
		info->op_mode = op_mode;
		ln8000_print_regmap(info);
	}

	return;
}

static int ln8000_change_opmode(struct ln8000_info *info, unsigned int target_mode)
{
	int ret = 0;
	u8 val, msk = (0x1 << LN8000_BIT_STANDBY_EN | 0x1 << LN8000_BIT_EN_1TO1);

	/* clear latched status */
	ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 2, 0x1 << 2);
	ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 2, 0x0 << 2);

	switch (target_mode) {
	case LN8000_OPMODE_STANDBY:
		val = (1 << LN8000_BIT_STANDBY_EN) | (1 << LN8000_BIT_EN_1TO1);
		ret = ln8000_update_reg(info, LN8000_REG_SYS_CTRL, msk, val);
		break;
	case LN8000_OPMODE_BYPASS:
		/* temp disable VIN_UV_TRACK during on start-up BYPASS mode */
		ln8000_enable_vin_uv_track(info, 0);
		val = (1 << LN8000_BIT_EN_1TO1);
		ret = ln8000_update_reg(info, LN8000_REG_SYS_CTRL, msk, val);
		val = (0 << LN8000_BIT_STANDBY_EN) | (1 << LN8000_BIT_EN_1TO1);
		ret = ln8000_update_reg(info, LN8000_REG_SYS_CTRL, msk, val);
		break;
	case LN8000_OPMODE_SWITCHING:
		val = (0 << LN8000_BIT_STANDBY_EN) | (0 << LN8000_BIT_EN_1TO1);
		ret = ln8000_update_reg(info, LN8000_REG_SYS_CTRL, msk, val);
		break;
	default:
		ln_err("invalid index (target_mode=%d)\n", target_mode);
		return -EINVAL;
	}

	if (IS_ERR_VALUE((unsigned long)ret)) {
		return -EINVAL;
	}
	ln_info("changed opmode [%d] -> [%d]\n", info->op_mode, target_mode);
	info->op_mode = target_mode;

	return 0;
}

static int ln8000_init_device(struct ln8000_info *info)
{
	unsigned int vbat_float;

	/* config default charging paramter by dt */
	vbat_float = info->pdata->bat_ovp_th * 100 / 102;   /* ovp thershold = v_float x 1.02 */
        vbat_float = (vbat_float / 1000) * 1000;
	ln_info("bat_ovp_th=%d, vbat_float=%d\n", info->pdata->bat_ovp_th, vbat_float);
	ln8000_set_vbat_float(info, vbat_float);
	info->vbat_ovp_alarm_th = info->pdata->bat_ovp_alarm_th;
	ln8000_set_vac_ovp(info, info->pdata->bus_ovp_th);
	info->vin_ovp_alarm_th = info->pdata->bus_ovp_alarm_th;
	ln8000_set_iin_limit(info, info->pdata->bus_ocp_th - 700000);
	info->iin_ocp_alarm_th = info->pdata->bus_ocp_alarm_th;
	ln8000_set_ntc_alarm(info, info->pdata->ntc_alarm_cfg);

	/* disable VBAT_REG, IIN_REG. DON'T change this, we can't support regulation mode */
	ln8000_write_reg(info, LN8000_REG_REGULATION_CTRL, 0x30);
	ln8000_enable_tdie_regulation(info, 0);
	ln8000_enable_auto_recovery(info, 0);

	/* config charging protection */
	ln8000_enable_vbat_ovp(info, !info->pdata->vbat_ovp_disable);
	ln8000_enable_iin_ocp(info, !info->pdata->iin_ocp_disable);
	ln8000_enable_tdie_prot(info, !info->pdata->tdie_prot_disable);
	ln8000_enable_rcp(info, 1);
	ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
	if (info->dev_role == LN_ROLE_SLAVE) {
		/* slave device didn't connect to OVPGATE */
		ln8000_enable_vac_ov(info, 0);
	} else {
		ln8000_enable_vac_ov(info, 1);
	}

	/* wdt : disable, adc : shutdown mode */
	ln8000_enable_wdt(info, false);
	ln8000_set_adc_mode(info, ADC_SHUTDOWN_MODE);//disable before updating
	ln8000_set_adc_hib_delay(info, ADC_HIBERNATE_4S);
	ln8000_set_adc_ch(info, LN8000_ADC_CH_ALL, true);
	ln8000_enable_tbus_monitor(info, !info->pdata->tbus_mon_disable);//+enables ADC ch
	ln8000_enable_tbat_monitor(info, !info->pdata->tbat_mon_disable);//+enables ADC ch
	ln8000_set_adc_mode(info, ADC_AUTO_HIB_MODE);
	/* mark sw initialized (used CHARGE_CTRL bit:7) */
	ln8000_update_reg(info, LN8000_REG_CHARGE_CTRL, 0x1 << 7, 0x1 << 7);
	ln8000_write_reg(info, LN8000_REG_THRESHOLD_CTRL, 0x0E);
	/* restore regval for prevent EOS attack */
	ln8000_read_reg(info, LN8000_REG_REGULATION_CTRL, &info->regulation_ctrl);
	ln8000_read_reg(info, LN8000_REG_ADC_CTRL, &info->adc_ctrl);
	ln8000_read_reg(info, LN8000_REG_V_FLOAT_CTRL, &info->v_float_ctrl);
	ln8000_read_reg(info, LN8000_REG_CHARGE_CTRL, &info->charge_ctrl);

	/* if want to use external OVP control IC */
	if (info->pdata->ext_ovp_ctrl) {
		ln8000_set_ovpfet_high_imp(info, 1);
	}

	ln8000_print_regmap(info);

	return 0;
}

/**
 * Support power_supply platform for charger block.
 * propertis are compatible by Xiaomi platform
 */
static int ln8000_get_adc_data(struct ln8000_info *info, unsigned int ch, int *result)
{
	int ret;
	u8 sts[2];

	/* pause adc update */
	ret  = ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 1, 0x1 << 1);
	if (ret < 0) {
		ln_err("fail to update bit PAUSE_ADC_UPDATE:1 (ret=%d)\n", ret);
		return ret;
	}

	switch (ch) {
	case LN8000_ADC_CH_VOUT:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC04_STS, sts, 2);
		break;
	case LN8000_ADC_CH_VIN:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC03_STS, sts, 2);
		break;
	case LN8000_ADC_CH_VBAT:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC06_STS, sts, 2);
		break;
	case LN8000_ADC_CH_VAC:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC02_STS, sts, 2);
		break;
	case LN8000_ADC_CH_IIN:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC01_STS, sts, 2);
		break;
	case LN8000_ADC_CH_DIETEMP:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC07_STS, sts, 2);
		break;
	case LN8000_ADC_CH_TSBAT:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC08_STS, sts, 2);
		break;
	case LN8000_ADC_CH_TSBUS:
		ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC09_STS, sts, 2);
		break;
	default:
		ln_err("invalid ch(%d)\n", ch);
		ret = -EINVAL;
		break;
	}

	/* resume adc update */
	ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 1, 0x0 << 1);

	if (IS_ERR_VALUE((unsigned long)ret) == false) {
		ln8000_convert_adc_code(info, ch, sts, result);
	}

	return ret;
}

static int psy_chg_get_charging_enabled(struct ln8000_info *info)
{
	int enabled = 0;    /* disabled */

	if (info->chg_en) {
		ln8000_update_opmode(info);
		if (info->op_mode >= LN8000_OPMODE_BYPASS) {
			enabled = 1;
		}
	}

	return enabled;
}

static int ln8000_check_regmap_data(struct ln8000_info *info)
{
	u8 regulation_ctrl;
	u8 adc_ctrl;
	u8 v_float_ctrl;
	u8 charge_ctrl;

	ln8000_read_reg(info, LN8000_REG_REGULATION_CTRL, &regulation_ctrl);
	ln8000_read_reg(info, LN8000_REG_ADC_CTRL, &adc_ctrl);
	ln8000_read_reg(info, LN8000_REG_V_FLOAT_CTRL, &v_float_ctrl);
	ln8000_read_reg(info, LN8000_REG_CHARGE_CTRL, &charge_ctrl);

	if ((info->regulation_ctrl != regulation_ctrl) ||
	    (info->adc_ctrl != adc_ctrl) ||
	    (info->charge_ctrl != charge_ctrl) ||
	    (info->v_float_ctrl != v_float_ctrl)) {
		/* Decide register map was reset. caused by EOS */
		ln_err("decided register map RESET, re-initialize device\n");
		ln_err("regulation_ctrl = 0x%x : 0x%x\n", info->regulation_ctrl, regulation_ctrl);
		ln_err("adc_ctrl        = 0x%x : 0x%x\n", info->adc_ctrl, adc_ctrl);
		ln_err("charge_ctrl     = 0x%x : 0x%x\n", info->charge_ctrl, charge_ctrl);
		ln_err("vbat_float      = 0x%x : 0x%x\n", info->v_float_ctrl, v_float_ctrl);
		ln8000_init_device(info);
		msleep(300);
	}

	return 0;
}

static int ln8000_enter_SW(struct ln8000_info *info, int op_mode)
{
	int ret;

	/* valid check the regmap data for recovery EOS attack */
	ln8000_check_regmap_data(info);
#if defined(LN8000_RCP_PATCH)
	/* when the start-up to charging, we need to disabled RCP&OCP */
	/* those are will be enable after stable power line */
	ln8000_enable_rcp(info, 0);
	ln8000_enable_ocp(info, 0);
	schedule_delayed_work(&info->rcp_work, msecs_to_jiffies(1000));
#endif
	ret = ln8000_change_opmode(info, op_mode);
	if (ret < 0) {
		return ret;
	}
	msleep(10);
	ln8000_update_opmode(info);

	ln8000_print_regmap(info);

	return 0;
}

static int ln8000_enter_STANDBY(struct ln8000_info *info)
{
#if defined(LN8000_RCP_PATCH)
	ln8000_enable_rcp(info, 1);
	ln8000_enable_ocp(info, 1);
#endif
	ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
	msleep(10);
	ln8000_print_regmap(info);

	return 0;
}

static int psy_chg_get_ti_alarm_status(struct ln8000_info *info)
{
	int alarm;
	bool bus_ovp, bus_ocp, bat_ovp;
	u8 val[4];

	ln8000_check_status(info);
	ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
	ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
	ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
	bus_ovp = (info->vbus_uV > info->vin_ovp_alarm_th) ? 1 : 0;
	bus_ocp = (info->iin_uA > info->iin_ocp_alarm_th) ? 1 : 0;
	bat_ovp = (info->vbat_uV > info->vbat_ovp_alarm_th) ? 1 : 0;
	/* BAT alarm status not support (ovp/ocp/ucp) */
	alarm = ((bus_ovp << BUS_OVP_ALARM_SHIFT) |
		(bus_ocp << BUS_OCP_ALARM_SHIFT) |
		(bat_ovp << BAT_OVP_ALARM_SHIFT) |
		(info->tbus_tbat_alarm << BAT_THERM_ALARM_SHIFT) |
		(info->tbus_tbat_alarm << BUS_THERM_ALARM_SHIFT) |
		(info->tdie_alarm << DIE_THERM_ALARM_SHIFT));
	ln8000_bulk_read_reg(info, LN8000_REG_SYS_STS, val, 4);

	ln_info("st:0x%x:0x%x:0x%x:0x%x alarm=0x%x, adc_vin=%d, adc_iin=%d, adc_vbat=%d\n",
		val[0], val[1], val[2], val[3], alarm, info->vbus_uV/1000, info->iin_uA/1000,
		info->vbat_uV/1000);

	return alarm;
}

static int psy_chg_get_ti_fault_status(struct ln8000_info *info)
{
	int fault;

	ln8000_check_status(info);

	/* BAT ocp fault status not suppport */
	fault = ((info->vbat_ov << BAT_OVP_FAULT_SHIFT) |
		(info->vbus_ov << BUS_OVP_FAULT_SHIFT) |
		(info->iin_oc << BUS_OCP_FAULT_SHIFT) |
		(info->tbus_tbat_fault << BAT_THERM_FAULT_SHIFT) |
		(info->tbus_tbat_fault << BUS_THERM_FAULT_SHIFT) |
		(info->tdie_fault << DIE_THERM_FAULT_SHIFT));

	if (info->volt_qual == 0) {
		fault  |= ((1 << BAT_OVP_FAULT_SHIFT) |
		(1 << BUS_OVP_FAULT_SHIFT));
	}

	if (fault) {
		ln_info("fault=0x%x\n", fault);
	}

	return fault;
}

static int psy_chg_get_it_bus_error_status(struct ln8000_info *info)
{
        ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
        if (info->vbus_uV < 2040000/*6000000*/) {
                return VBUS_ERROR_LOW;
	    } else if (info->vbus_uV > 2320000 /*13000000*/ ) {
                return VBUS_ERROR_HIGHT;
        }
        return VBUS_ERROR_NONE;
}

static int ln8000_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct ln8000_info *info = iio_priv(indio_dev);
	int ret = 0;
	int chip_id = 0;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_STATUS:
	case PSY_IIO_TI_SET_BUS_PROTECTION_FOR_QC3:
		*val1 = 0;
		break;
	case PSY_IIO_PRESENT:
		*val1 = info->usb_present;
		break;
	case PSY_IIO_CONSTANT_CHARGE_VOLTAGE:
		*val1 = ln8000_get_vbat_float(info);
		break;
	case PSY_IIO_CP_INPUT_CURRENT_LIMIT:
		*val1 = ln8000_get_iin_limit(info);
		break;
	case PSY_IIO_MODEL_NAME:
		//val->strval = ln8000_dev_name[info->dev_role];
		*val1 = info->dev_role;
		break;
	case PSY_IIO_CHARGING_ENABLED:
	case PSY_IIO_TI_BYPASS_MODE_ENABLED:
		*val1 = psy_chg_get_charging_enabled(info);
		break;
	case PSY_IIO_SC_BATTERY_PRESENT:
		ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
		if (info->vbat_uV > LN8000_ADC_VBAT_MIN) {
			*val1 = 1;    /* detected battery */
		} else {
			*val1 = 0;    /* non-detected battery */
		}
		break;
	case PSY_IIO_SC_VBUS_PRESENT:
		ln8000_check_status(info);
		*val1 = !(info->vac_unplug);
		break;
	case PSY_IIO_SC_BATTERY_VOLTAGE:
		ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
		*val1 = info->vbat_uV/1000;
		/* valid check the regmap data for recovery EOS attack */
		ln8000_check_regmap_data(info);
		break;
	case PSY_IIO_SC_BATTERY_CURRENT:  /* ln8000 not support IBAT_ADC */
		ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
		*val1 = (info->iin_uA * 2)/1000;         /* return to IBUS_ADC x 2 */
		break;
	case PSY_IIO_SC_BATTERY_TEMPERATURE:
		if (info->pdata->tbat_mon_disable) {
			*val1 = 0;
		} else {
			ln8000_get_adc_data(info, LN8000_ADC_CH_TSBAT, &info->tbat_uV);
			*val1 = info->tbat_uV;
		}
		break;
	case PSY_IIO_SC_BUS_VOLTAGE:
		ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
		*val1 = info->vbus_uV/1000;
		break;
	case PSY_IIO_SC_BUS_CURRENT:
		ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
		*val1 = info->iin_uA/1000;
		break;
	case PSY_IIO_SC_BUS_TEMPERATURE:
		if (info->pdata->tbus_mon_disable) {
			*val1 = 0;
		} else {
			ln8000_get_adc_data(info, LN8000_ADC_CH_TSBUS, &info->tbus_uV);
			*val1 = info->tbus_uV;
		}
		break;
	case PSY_IIO_SC_DIE_TEMPERATURE:
		ln8000_get_adc_data(info, LN8000_ADC_CH_DIETEMP, &info->tdie_dC);
		*val1 = info->tdie_dC;
		break;
	case PSY_IIO_SC_ALARM_STATUS:
		*val1 = psy_chg_get_ti_alarm_status(info);
		break;
	case PSY_IIO_SC_FAULT_STATUS:
		*val1 = psy_chg_get_ti_fault_status(info);
		break;
	case PSY_IIO_TI_REG_STATUS:
		ln8000_check_status(info);
		*val1 = ((info->vbat_regulated << VBAT_REG_STATUS_SHIFT) |
		/* ln8000 not support ibat_reg, we are can be ibus_reg */
		(info->iin_regulated << IBAT_REG_STATUS_SHIFT));
		if (*val1) {
			ln_info("ti_reg_status: intavl=0x%x\n", *val1);
		}
		break;
	case PSY_IIO_HV_CHARGE_ENABLED:
		*val1 = hv_charge_enable;
		break;
	case PSY_IIO_SC_VBUS_ERROR_STATUS:
		*val1 = psy_chg_get_it_bus_error_status(info);
		break;
	case PSY_IIO_DEV_CHIP_ID:
		chip_id = i2c_smbus_read_byte_data(info->client, LN8000_REG_DEVICE_ID);
		*val1 = chip_id;
		break;

	default:
		pr_debug("Unsupported ln8000 IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_err("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, ret);
		return ret;
	}
	return IIO_VAL_INT;
}
static int psy_chg_set_charging_enable(struct ln8000_info *info, int val)
{
	int ret;

	/* skip duplicate command of charging enable */
	if (val == info->chg_en) {
		return 0;
	}
	info->chg_en = val;

	if (val) {
		ret = ln8000_enter_SW(info, LN8000_OPMODE_SWITCHING);
	} else {
		ret = ln8000_enter_STANDBY(info);
	}

	return ret;
}

static int psy_chg_set_bypass_enable(struct ln8000_info *info, int val)
{
	int ret;

	/* skip duplicate command of charging enable */
	if (val == info->chg_en) {
		return 0;
	}
	info->chg_en = val;

	if (val) {
		ret = ln8000_enter_SW(info, LN8000_OPMODE_BYPASS);
	} else {
		ret = ln8000_enter_STANDBY(info);
	}

	return ret;
}

static int psy_chg_set_present(struct ln8000_info *info, int val)
{
	bool usb_present = (bool)val;

	if (usb_present != info->usb_present) {
		ln_info("changed usb_present [%d] -> [%d]\n", info->usb_present, usb_present);
		if (usb_present) {
			ln8000_init_device(info);
		}
		info->usb_present = usb_present;
	}

	return 0;
}

static int psy_chg_set_bus_protection_for_qc3(struct ln8000_info *info, int hvdcp3_type)
{
	ln_info("hvdcp3_type: %d\n", hvdcp3_type);

	if (hvdcp3_type == HVDCP3_CLASSA_18W) {
		ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC);
		info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC;
		ln8000_set_iin_limit(info, BUS_OCP_FOR_QC_CLASS_A - 700000);
		info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC_CLASS_A;
	} else if (hvdcp3_type == HVDCP3_CLASSB_27W) {
		ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC);
		info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC;
		ln8000_set_iin_limit(info, BUS_OCP_FOR_QC_CLASS_B - 700000);
		info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC_CLASS_B;
		} else if (hvdcp3_type == HVDCP3_P_CLASSA_18W) {
		ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC35);
		info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC35;
		ln8000_set_iin_limit(info, BUS_OCP_FOR_QC35_CLASS_A_P - 700000);
		info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC35_CLASS_A_P;
		} else if (hvdcp3_type == HVDCP3_P_CLASSB_27W) {
		ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC35);
		info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC35;
		ln8000_set_iin_limit(info, BUS_OCP_FOR_QC35_CLASS_A_P - 700000);
		info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC35_CLASS_A_P;
	} else {
		ln8000_set_vac_ovp(info, info->pdata->bus_ovp_th);
		info->vin_ovp_alarm_th = info->pdata->bus_ovp_alarm_th;
		ln8000_set_iin_limit(info, info->pdata->bus_ocp_th - 700000);
		info->iin_ocp_alarm_th = info->pdata->bus_ocp_alarm_th;
	}

	ln8000_print_regmap(info);

	return 0;
}

static int ln8000_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct ln8000_info *info = iio_priv(indio_dev);
	int ret = 0;

	switch (chan->channel) {
	case PSY_IIO_PRESENT:
		ret = psy_chg_set_present(info, val1);
		break;
	case PSY_IIO_CONSTANT_CHARGE_VOLTAGE:
		ret = ln8000_set_vbat_float(info, val1);
		break;
	case PSY_IIO_CP_INPUT_CURRENT_LIMIT:
		ret = ln8000_set_iin_limit(info, val1);
		break;
	case PSY_IIO_CHARGING_ENABLED:
		ret = psy_chg_set_charging_enable(info, val1);
		break;
	case PSY_IIO_TI_BYPASS_MODE_ENABLED:
		ret = psy_chg_set_bypass_enable(info, val1);
		break;
	case PSY_IIO_TI_SET_BUS_PROTECTION_FOR_QC3:
		ret = psy_chg_set_bus_protection_for_qc3(info, val1);
		break;
	case PSY_IIO_HV_CHARGE_ENABLED:
		hv_charge_enable = val1;
		break;
	default:
		pr_debug("Unsupported LN8000 IIO chan %d\n", chan->channel);
		ret = -EINVAL;
	}
	if(ret < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, ret);

	return 0;
}

static int read_reg(void *data, u64 *val)
{
	struct ln8000_info *info = data;
	int ret;
	u8 temp;

	ret = ln8000_read_reg(info, info->debug_address, &temp);
	if (ret) {
		ln_err("Unable to read reg(0x%02X), ret=%d\n", info->debug_address, ret);
		return -EAGAIN;
	}
	*val = (u64)temp;
	return 0;
}

static int write_reg(void *data, u64 val)
{
	struct ln8000_info *info = data;
	int ret;
	u8 temp = (u8) val;

	ret = ln8000_write_reg(info, info->debug_address, temp);
	if (ret) {
		ln_err("Unable to write reg(0x%02X), data(0x%02X), ret=%d\n",
		info->debug_address, temp, ret);
		return -EAGAIN;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, read_reg, write_reg, "0x%02llX\n");

static int ln8000_create_debugfs_entries(struct ln8000_info *info)
{
	struct dentry *ent;

	info->debug_root = debugfs_create_dir(ln8000_dev_name[info->dev_role], NULL);
	if (!info->debug_root) {
		ln_err("unable to create debug dir\n");
		return -ENOENT;
	} else {
#if 0
		ent = debugfs_create_x32("address", S_IFREG | 0644, info->debug_root, &(info->debug_address));
		if (!ent) {
			ln_err("unable to create address debug file\n");
			return -ENOENT;
		}
#endif
		ent = debugfs_create_file("data", S_IFREG | 0644, info->debug_root, info, &register_debug_ops);
		if (!ent) {
			ln_err("unable to create data debug file\n");
			return -ENOENT;
		}
	}

	return 0;
}

/**
 * Support IRQ interface
 */
static int ln8000_read_int_value(struct ln8000_info *info, u8 *reg_val)
{
	int ret;

	/* pause INT updates */
	ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1, 0x1);
	mdelay(1);

	ret = ln8000_read_reg(info, LN8000_REG_INT1, reg_val);

	/* resume INT updates */
	ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1, 0x0);

	return ret;
}

#if defined(LN8000_VAC_OV_PATCH)
static void vac_ov_control_work(struct work_struct *work)
{
	struct ln8000_info *info = container_of(work, struct ln8000_info, vac_ov_work.work);
	int i, cnt, adc_check_cnt, ta_detach_cnt, delay = 50;
	u8 sys_st;
	bool enable_vac_ov = 1;

	adc_check_cnt = 0;
	ta_detach_cnt = 0;
	cnt = 10000 / delay;
	for (i = 0; i < cnt; ++i) {
		ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
		ln8000_read_reg(info, LN8000_REG_SYS_STS, &sys_st);

		if (enable_vac_ov) {
			/* Check ADC_VIN during the 10sec, if vin higher then 10V, disable to vac_ov */
			if (info->vbus_uV > 10000000) {
				adc_check_cnt += 1;
				ln_info("vin=%dmV, adc_check_cnt=%d\n", info->vbus_uV/1000, adc_check_cnt);
				if (adc_check_cnt > 2) {
					enable_vac_ov = 0;
					ln8000_enable_vac_ov(info, enable_vac_ov);
					ln_info("vac_ov=disable, vin=%dmV, i=%d, cnt=%d, delay=%d\n", info->vbus_uV/1000, i, cnt, delay);
					adc_check_cnt = 0;
				}
			}
		} else {
			/* After disabled vac_ov, if ADC_VIN lower then 7V goto the terminate work */
			if (info->vbus_uV < 7000000) {
				adc_check_cnt += 1;
				ln_info("vin=%dmV, adc_check_cnt=%d\n", info->vbus_uV/1000, adc_check_cnt);
				if (adc_check_cnt > 2) {
					enable_vac_ov = 1;
					ln_info("vac_ov=enable, vin=%dmV, i=%d, cnt=%d, delay=%d\n", info->vbus_uV/1000, i, cnt, delay);
					goto teminate_work;
					}

			}
		}
		/* If judged 3 times by TA disconnected, goto the terminate work */
		if (sys_st == 0x1) { /* it's means entered shutdown mode */
			ta_detach_cnt += 1;
			ln_info("sys_st=0x%x, ta_detached=%d\n", sys_st, ta_detach_cnt);
			if (ta_detach_cnt > 2) {
				goto teminate_work;
			}
		}
		msleep(delay);
	}

teminate_work:
	ln_info("terminate_work:enable_vac_ov (i=%d, cnt=%d)\n", i, cnt);
	ln8000_enable_vac_ov(info, 1);
	info->vac_ov_work_on = 0;
}

static void check_vac_ov_work(struct ln8000_info *info)
{
	u8 sys_st, fault1_st;

	ln8000_read_reg(info, LN8000_REG_SYS_STS, &sys_st);
	ln8000_read_reg(info, LN8000_REG_FAULT1_STS, &fault1_st);

	if (sys_st == 0x02 && fault1_st == 0x00) {  /* connected valid VBUS */
		if (info->vac_ov_work_on == 0) {        /* vac_ov_work not worked */
			schedule_delayed_work(&info->vac_ov_work, msecs_to_jiffies(0));
			info->vac_ov_work_on = 1;
			ln_info("schedule_work : vac_ov_work\n");
		}
	}
}
#endif

static irqreturn_t ln8000_interrupt_handler(int irq, void *data)
{
	struct ln8000_info *info = data;
	u8 int_reg, int_msk;
	u8 masked_int;
	int ret;

	ln_err("ln8000_interrupt_handler enter!\n");
	ret = ln8000_read_int_value(info, &int_reg);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		ln_err("fail to read INT reg (ret=%d)\n", ret);
		return IRQ_NONE;
	}
	ln8000_read_reg(info, LN8000_REG_INT1_MSK, &int_msk);
	masked_int = int_reg & ~int_msk;

	ln_info("int_reg=0x%x, int_msk=0x%x, masked_int=0x%x\n", int_reg, int_msk, masked_int);

	ln8000_print_regmap(info);
	LN8000_BIT_CHECK(masked_int, 7, "(INT) FAULT_INT");
	LN8000_BIT_CHECK(masked_int, 6, "(INT) NTC_PROT_INT");
	LN8000_BIT_CHECK(masked_int, 5, "(INT) CHARGE_PHASE_INT");
	LN8000_BIT_CHECK(masked_int, 4, "(INT) MODE_INT");
	LN8000_BIT_CHECK(masked_int, 3, "(INT) REV_CURR_INT");
	LN8000_BIT_CHECK(masked_int, 2, "(INT) TEMP_INT");
	LN8000_BIT_CHECK(masked_int, 1, "(INT) ADC_DONE_INT");
	LN8000_BIT_CHECK(masked_int, 0, "(INT) TIMER_INT");
	ln8000_check_status(info);

	if (masked_int & LN8000_MASK_FAULT_INT) { /* FAULT_INT */
		if (info->volt_qual) {
			ln_info("connected to power_supplier\n");
		} else {
			ln_info("FAULT_INT has occurred\n");
		}
	}
	if (masked_int & LN8000_MASK_NTC_PROT_INT) { /* NTC_PROT_INT */
		ln_info("NTC_PROT_INT has occurred(ntc_fault=%d, ntc_alarm=%d)\n",
		info->tbus_tbat_fault, info->tbus_tbat_alarm);
	}
	if (masked_int & LN8000_MASK_CHARGE_PHASE_INT) { /* CHARGE_PHASE_INT */
		if (info->vbat_regulated) {
			ln_info("CHARGE_PHASE_INT: VFLOAT regulated\n");
		} else if (info->iin_regulated) {
			ln_info("CHARGE_PHASE_INT: IIN regulated\n");
		}
	}
	if (masked_int & LN8000_MASK_MODE_INT) { /* MODE_INT */
		switch (info->pwr_status) {
		case LN8000_MASK_BYPASS_ENABLED:
			ln_info("MODE_INT: device in BYPASS mode\n");
			break;
		case LN8000_MASK_SWITCHING_ENABLED:
			ln_info("MODE_INT: device in SWITCHING mode\n");
			break;
		case LN8000_MASK_STANDBY_STS:
			ln_info("MODE_INT: device in STANDBY mode\n");
			break;
		case LN8000_MASK_SHUTDOWN_STS:
			ln_info("MODE_INT: device in SHUTDOWN mode\n");
			break;
		default:
			ln_info("MODE_INT: device in  unknown mode\n");
			break;
		}
	}
	if (masked_int & LN8000_MASK_TEMP_INT) { /* TEMP_INT */
		ln_info("TEMP_INT has occurred(tdie_fault=%d, tdie_alarm=%d)\n",
		info->tdie_fault, info->tdie_alarm);
	}
		if (masked_int & LN8000_MASK_TIMER_INT) { /* TIMER_INT */
		ln_info("Watchdog timer has expired(wdt_fault=%d)\n", info->wdt_fault);
	}

#if defined(LN8000_VAC_OV_PATCH)
	if (info->dev_role != LN_ROLE_SLAVE) {
		check_vac_ov_work(info);
	}
#endif

	return IRQ_HANDLED;
}
#if 0
static int ln8000_irq_init(struct ln8000_info *info)
{
	const struct ln8000_platform_data *pdata = info->pdata;
	int ret;
	u8 mask, int_reg;

	if (info->pdata->irq_gpio) {
		info->client->irq = gpiod_to_irq(pdata->irq_gpio);
		if (info->client->irq < 0) {
			ln_err("fail to get irq from gpio(irq_gpio=%p)\n", pdata->irq_gpio)
			info->client->irq = 0;
			return -EINVAL;
		}
		ln_info("mapped GPIO to irq (%d)\n", info->client->irq);
	}

	/* interrupt mask setting */
	mask = LN8000_MASK_ADC_DONE_INT | LN8000_MASK_TIMER_INT | LN8000_MASK_FAULT_INT | LN8000_MASK_REV_CURR_INT | LN8000_MASK_CHARGE_PHASE_INT;
	if (info->pdata->tdie_prot_disable)
		mask |= LN8000_MASK_TEMP_INT;
	if (info->pdata->tbat_mon_disable && info->pdata->tbus_mon_disable)
		mask |= LN8000_MASK_NTC_PROT_INT;
	ln8000_write_reg(info, LN8000_REG_INT1_MSK, mask);
	/* read clear int_reg */
	ret = ln8000_read_int_value(info, &int_reg);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		ln_err("fail to read INT reg (ret=%d)\n", ret);
		return IRQ_NONE;
	}
	ln_info("int1_msk=0x%x\n", mask);

	return 0;
}
#endif
#if defined(LN8000_RCP_PATCH)
static void rcp_control_work(struct work_struct *work)
{
	struct ln8000_info *info = container_of(work, struct ln8000_info, rcp_work.work);
	unsigned int v_offset;
	u8 sys_st;

	ln8000_read_reg(info, LN8000_REG_SYS_STS, &sys_st);
	if ((sys_st & 0x3) > 0 || info->rcp_en) {
		/* if already has been cut-off the charge, we don't need this work */
		return;
	}

	ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
	ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
	ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
	if (info->vbus_uV < (info->vbat_uV * 2)) {
		v_offset = 0;
	} else {
		v_offset = info->vbus_uV - (info->vbat_uV * 2);
	}
	ln_info("vbus:%d iin:%d, vbat:%d, v_offset=%d, sys_st=0x%x\n", info->vbus_uV/1000,
		info->iin_uA/1000, info->vbat_uV/1000, v_offset/1000, sys_st);

	/* When the input current rises above 400mA, we can activate RCP. */
	if (info->iin_uA > 400000) {
		ln8000_enable_rcp(info, 1);
		ln8000_enable_ocp(info, 1);
		ln_info("enabled rcp\n");
		return;
	}

	/* If an unplug event occurred, we need to switching to standby mode. */
	/* unplug event cond. : IBUS lower then 70mA and v_offset(100mV) */
	if (info->iin_uA < 70000 && v_offset < 100000) {
		ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
		ln8000_enable_rcp(info, 1);
		ln8000_enable_ocp(info, 1);
		info->chg_en = 0;
		ln_info("forced change standby_mode for prevent reverse current\n");
		return;
	}

	schedule_delayed_work(&info->rcp_work, msecs_to_jiffies(1000));
}
#endif

static void determine_initial_status(struct ln8000_info *info)
{
	if (info->client->irq)
		ln8000_interrupt_handler(info->client->irq, info);
}


static const struct of_device_id ln8000_dt_match[] = {
	{
		.compatible = "lionsemi,ln8000",
		.data = (void *) LN_ROLE_STANDALONE
	},
	{
		.compatible = "lionsemi,ln8000-master",
		.data = (void *) LN_ROLE_MASTER
	},
	{
		.compatible = "lionsemi,ln8000-slave",
		.data = (void *) LN_ROLE_SLAVE
	},
	{ },
};
MODULE_DEVICE_TABLE(of, ln8000_dt_match);

static const struct i2c_device_id ln8000_id[] = {
	{ "ln8000", LN_ROLE_STANDALONE },
	{ "ln8000-master", LN_ROLE_MASTER },
	{ "ln8000-slave", LN_ROLE_SLAVE },
	{ }
};

static int ln8000_get_dev_role(struct i2c_client *client)
{
	const struct of_device_id *of_id;

	of_id = of_match_device(of_match_ptr(ln8000_dt_match), &client->dev);
	if (of_id == NULL) {
                dev_err(&client->dev, "%s: fail to matched of_device_id\n", __func__);
		return -EINVAL;
	}

         dev_info(&client->dev, "%s: matched to %s\n", __func__, of_id->compatible);

	return (long)(of_id->data);
}

static int ln8000_parse_dt(struct ln8000_info *info)
{
	struct device *dev = info->dev;
	struct ln8000_platform_data *pdata = info->pdata;
	struct device_node *np = dev->of_node;
	u32 prop;
	int ret;

	if (np == NULL)
		return -EINVAL;

	/* first we try to used client->irq */
	if (info->client->irq == 0) {
		/* sencond we try to used irq_index info */
		ret = of_property_read_u32(np, "ln8000_charger,irq_index", &prop);
		if (ret < 0) {
			ln_err("can't get irq_index(ret=%d)\n", ret);
			prop = 0;
		}
		info->client->irq = prop;
	}
	ln_info("info->client->irq=%d\n", info->client->irq);

	if (!info->client->irq) {
		pdata->irq_gpio = gpiod_get(dev, "irqb", GPIOD_IN);
		if (LN8000_USE_GPIO(pdata)) {
			ln_info("[gpio] found valid GPIO/IRQ descriptor\n");
		} else {
			ln_info("[gpio] unspecified or invalid GPIO descriptor\n");
			pdata->irq_gpio = 0;
		}
	}

	/* device configuration */
	ret = of_property_read_u32(np, "ln8000_charger,bat-ovp-threshold", &prop);
	LN8000_PARSE_PROP(ret, pdata, bat_ovp_th, (prop*1000/*uV*/), LN8000_BAT_OVP_DEFAULT);
	ret = of_property_read_u32(np, "ln8000_charger,bat-ovp-alarm-threshold", &prop);
	LN8000_PARSE_PROP(ret, pdata, bat_ovp_alarm_th, (prop*1000/*uV*/), 0);
	ret = of_property_read_u32(np, "ln8000_charger,bus-ovp-threshold", &prop);
	LN8000_PARSE_PROP(ret, pdata, bus_ovp_th, (prop*1000/*uV*/), LN8000_BUS_OVP_DEFAULT);
	ret = of_property_read_u32(np, "ln8000_charger,bus-ovp-alarm-threshold", &prop);
	LN8000_PARSE_PROP(ret, pdata, bus_ovp_alarm_th, (prop*1000/*uA*/), 0);
	ret = of_property_read_u32(np, "ln8000_charger,bus-ocp-threshold", &prop);
	LN8000_PARSE_PROP(ret, pdata, bus_ocp_th, (prop*1000/*uA*/), LN8000_BUS_OCP_DEFAULT);
	ret = of_property_read_u32(np, "ln8000_charger,bus-ocp-alarm-threshold", &prop);
	LN8000_PARSE_PROP(ret, pdata, bus_ocp_alarm_th, (prop*1000/*uA*/), 0);
	ret = of_property_read_u32(np, "ln8000_charger,ntc-alarm-cfg", &prop);
	LN8000_PARSE_PROP(ret, pdata, ntc_alarm_cfg, prop, LN8000_NTC_ALARM_CFG_DEFAULT);
	ret = of_property_read_u32(np, "ln8000_charger,ext_ovp_ctrl", &prop);
	LN8000_PARSE_PROP(ret, pdata, ext_ovp_ctrl, prop, 0);

	/* protection/alarm disable (defaults to enable) */
	pdata->vbat_ovp_disable     = of_property_read_bool(np, "ln8000_charger,vbat-ovp-disable");
	pdata->iin_ocp_disable      = of_property_read_bool(np, "ln8000_charger,iin-ocp-disable");
	pdata->tbus_mon_disable     = of_property_read_bool(np, "ln8000_charger,tbus-mon-disable");
	pdata->tbat_mon_disable     = of_property_read_bool(np, "ln8000_charger,tbat-mon-disable");
	pdata->tdie_prot_disable    = of_property_read_bool(np, "ln8000_charger,tdie-prot-disable");

	/* override device tree */
	if (info->dev_role == LN_ROLE_MASTER) {
		ln_info("disable TS_BAT monitor for primary device on dual-mode\n");
		pdata->tbat_mon_disable = true;
	} else if (info->dev_role == LN_ROLE_SLAVE) {
		ln_info("disable VBAT_OVP and TS_BAT for secondary device on dual-mode\n");
		pdata->vbat_ovp_disable   = true;
		pdata->tbat_mon_disable   = true;
	}

	ln_info("vbat_ovp_disable = %d\n", pdata->vbat_ovp_disable);
	ln_info("iin_ocp_disable = %d\n", pdata->iin_ocp_disable);
	ln_info("tbus_mon_disable = %d\n", pdata->tbus_mon_disable);
	ln_info("tbat_mon_disable = %d\n", pdata->tbat_mon_disable);
	ln_info("tdie_prot_disable = %d\n", pdata->tdie_prot_disable);

	return 0;
}

static int ln8000_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct ln8000_info *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;
	for (i = 0; i < ARRAY_SIZE(ln8000_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;
	return -EINVAL;
}

static const struct iio_info ln8000_iio_info = {
	.read_raw	= ln8000_iio_read_raw,
	.write_raw	= ln8000_iio_write_raw,
	.of_xlate	= ln8000_iio_of_xlate,
};

static int ln8000_init_iio_psy(struct ln8000_info *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(ln8000_iio_psy_channels);
	int rc, i;
	pr_err("ln8000_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;
	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;
	indio_dev->info = &ln8000_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	if (chip->dev_role == LN_ROLE_MASTER) {
		indio_dev->name = "ln8000-master";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_iio_psy_channels[i].channel_num;
			chan->type = ln8000_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_iio_psy_channels[i].info_mask;
		}
	} else if (chip->dev_role == LN_ROLE_SLAVE) {
		indio_dev->name = "ln8000-slave";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_iio_psy_channels[i].channel_num;
			chan->type = ln8000_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_iio_psy_channels[i].info_mask;
		}
	}
	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register ln8000 IIO device, rc=%d\n", rc);
	pr_err("ln8000 IIO device, rc=%d\n", rc);
	return rc;
}

#if defined(LN8000_I2C_FIND_PATCH)
static void raw_i2c_sw_reset(struct i2c_client *client)
{
	u8 reg;

	i2c_smbus_write_byte_data(client, LN8000_REG_LION_CTRL, 0xC6);

	/* SOFT_RESET_REQ = 1 */
	reg = i2c_smbus_read_byte_data(client, LN8000_REG_BC_OP_2);
	reg = reg | (0x1 << 0);
	i2c_smbus_write_byte_data(client, LN8000_REG_BC_OP_2, reg);

	msleep(5 * 2);  /* ln8000 min wait time 5ms (after POR) */
}

static int try_to_find_i2c_address(struct i2c_client *client)
{
	u8 addr_set[] = {0x55, 0x5f, 0x51, 0x5b};
	u8 ori_addr = client->addr;
	int i, ret;

        for (i = 0; i < ARRAY_SIZE(addr_set); ++i) {
		client->addr = addr_set[i];
		ret = i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
		if (ret == LN8000_DEVICE_ID) {
			dev_info(&client->dev, "find to can be access address(0x%x)(ori=0x%x)\n",
				 client->addr, ori_addr);
			raw_i2c_sw_reset(client);
			break;
		} else {
			dev_info(&client->dev, "can't access address(0x%x)(ori=0x%x)\n",
				 client->addr, ori_addr);
		}
	}

	client->addr = ori_addr;
        if (ret == LN8000_DEVICE_ID) {
		ret = i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
	}

	return ret;
}
#endif

static int ln8000_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ln8000_info *info = NULL;
	struct iio_dev *indio_dev = NULL;
	struct gpio_desc *irq_gpio;
	int ret = 0;
	pr_err("ln8000 probe start!\n");
	ln8000_err("ln8000 probe start!\n");
	/* detect device on connected i2c bus */
	ret = i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
	if (ret != LN8000_DEVICE_ID) {
#if defined(LN8000_I2C_FIND_PATCH)
		ret = try_to_find_i2c_address(client);
		if (ret != LN8000_DEVICE_ID) {
			dev_err(&client->dev, "fail to detect ln8000 on i2c_bus(addr=0x%x)\n", client->addr);
			return -ENODEV;
		}
#else
		dev_err(&client->dev, "fail to detect ln8000 on i2c_bus(addr=0x%x)\n", client->addr);
		return -ENODEV;
#endif
	}
	 dev_info(&client->dev, "device id=0x%x\n", ret);

	if(client->dev.of_node){
		indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct ln8000_info));
		if (indio_dev == NULL) {
			dev_err(&client->dev, "fail to alloc devm for ln8000_info\n");
			return -ENOMEM;
		}
	}else{
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);
	info->indio_dev = indio_dev;

	info->dev_role = ln8000_get_dev_role(client);
	if (IS_ERR_VALUE((unsigned long)info->dev_role)) {
		kfree(info);
		return -EINVAL;
	}

	info->pdata = devm_kzalloc(&client->dev, sizeof(struct ln8000_platform_data), GFP_KERNEL);
	if (info->pdata == NULL) {
		ln_err("fail to alloc devm for ln8000_platform_data\n");
		ln8000_err("fail to alloc devm for ln8000_platform_data\n");
		kfree(info);
		return -ENOMEM;
	}
	info->dev = &client->dev;
	info->client = client;
	ret = ln8000_parse_dt(info);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		ln_err("fail to parsed dt\n");
		ln8000_err("fail to parsed dt\n");
		goto err_devmem;
	}

	mutex_init(&info->data_lock);
	mutex_init(&info->i2c_lock);
	mutex_init(&info->irq_lock);
	i2c_set_clientdata(client, info);

	ln8000_soft_reset(info);
	ln8000_init_device(info);

	ret = ln8000_init_iio_psy(info);
	if (ret < 0) {
		pr_err("Failed to initialize ln8000 IIO PSY, rc=%d\n", ret);
		ln8000_err("Failed to initialize ln8000 IIO PSY, rc=%d\n", ret);
	}
	/*
	ret = ln8000_irq_init(info);
	if (ret < 0) {
		goto err_psy;
	}
	*/

	irq_gpio = devm_gpiod_get(&client->dev, "irq", GPIOD_IN);
	if (IS_ERR(irq_gpio)) {
		ret = PTR_ERR(irq_gpio);
		dev_err(&client->dev, "Failed to get irq gpio");
		goto err_devmem;
	} else {
		client->irq = gpiod_to_irq(irq_gpio);
	}
	if (client->irq > 0) {
		ret = devm_request_threaded_irq(&client->dev, client->irq,
						NULL, ln8000_interrupt_handler,
						IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
						"ln8000-charger-irq", info);
		if (ret < 0) {
			ln_err("request irq for irq=%d failed, ret =%d\n",
			client->irq, ret);
			ln8000_err("request irq for irq=%d failed, ret =%d\n",
			client->irq, ret);
			goto err_wakeup;
		}
		enable_irq_wake(client->irq);
#if defined(LN8000_VAC_OV_PATCH)
		INIT_DELAYED_WORK(&info->vac_ov_work, vac_ov_control_work);
#endif
	} else {
		ln_info("don't support isr(irq=%d)\n", info->client->irq);
	}
#if defined(LN8000_RCP_PATCH)
	INIT_DELAYED_WORK(&info->rcp_work, rcp_control_work);
#endif

	device_init_wakeup(info->dev, 1);

	ret = ln8000_create_debugfs_entries(info);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		goto err_wakeup;
	}

	determine_initial_status(info);
	ln_err("ln8000 probe end!\n");
	ln8000_err("ln8000 probe end!\n");
	return 0;

err_wakeup:
	if (client->irq) {
		free_irq(client->irq, info);
		if (info->pdata->irq_gpio) {
			gpiod_put(info->pdata->irq_gpio);
		}
	}
//err_psy:

	i2c_set_clientdata(client, NULL);
	mutex_destroy(&info->data_lock);
	mutex_destroy(&info->i2c_lock);
	mutex_destroy(&info->irq_lock);
err_devmem:
	kfree(info->pdata);
	kfree(info);

	return ret;
}

static int ln8000_remove(struct i2c_client *client)
{
	struct ln8000_info *info = i2c_get_clientdata(client);

	ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);

	debugfs_remove_recursive(info->debug_root);

	if (client->irq) {
		free_irq(client->irq, info);
		if (info->pdata->irq_gpio) {
			gpiod_put(info->pdata->irq_gpio);
		}
	}

	i2c_set_clientdata(info->client, NULL);

	mutex_destroy(&info->data_lock);
	mutex_destroy(&info->i2c_lock);
	mutex_destroy(&info->irq_lock);

	kfree(info->pdata);
	kfree(info);

	return 0;
}

static void ln8000_shutdown(struct i2c_client *client)
{
	struct ln8000_info *info = i2c_get_clientdata(client);

	ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
}

#if defined(CONFIG_PM)
static int ln8000_suspend(struct device *dev)
{
	struct ln8000_info *info = dev_get_drvdata(dev);

	if (device_may_wakeup(dev) && info->client->irq)
		enable_irq_wake(info->client->irq);

	ln8000_irq_sleep(info, 1);

	return 0;
}

static int ln8000_resume(struct device *dev)
{
	struct ln8000_info *info = dev_get_drvdata(dev);

	if (device_may_wakeup(dev) && info->client->irq)
		disable_irq_wake(info->client->irq);

	ln8000_irq_sleep(info, 0);

	return 0;
}

static const struct dev_pm_ops ln8000_pm_ops = {
	.suspend = ln8000_suspend,
	.resume	= ln8000_resume,
};
#endif

static struct i2c_driver ln8000_driver = {
	.driver   = {
		.name = "ln8000_charger",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ln8000_dt_match),
#if defined(CONFIG_PM)
		.pm   = &ln8000_pm_ops,
#endif
	},
	.probe    = ln8000_probe,
	.remove   = ln8000_remove,
	.shutdown = ln8000_shutdown,
	.id_table = ln8000_id,
};
module_i2c_driver(ln8000_driver);

MODULE_AUTHOR("sungdae choi<sungdae.choi@cirrus.com>");
MODULE_DESCRIPTION("Cirrus Logic ln8000 switched-cap charger driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0.0");
