/*
 * Driver (skeleton code) for LIONSEMI LN8282 SC Voltage Regulator
 *
 * Copyright (C) 2019 Lion Semiconductor Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Jae Lee <kjaelee@lionsemi.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 */

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
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include <linux/power/ln8282.h>

#define LN8282_DRIVER_NAME	"ln8282"
#define LN8282_HW_REV_Bx	//HW rev.

// DEBUG options (uncomment)
#define LN8282_DEBUG_OTP_CTRL		// **WARNING** this is an experimental feature
					//             NEVER use in production releases


/*********************************************************************************
 * DESIGN GUIDE:
 *********************************************************************************/
// key (top-level) functions
//   --> ln8282_change_opmode()	: set control registers and change operation mode (STANDBY, BYPASS, SWITCHING)
//   --> ln8282_set_powerpath()	: configure power path (forward or reverse) based on external power connections/scenario
//   --> ln8282_set_infet()	: enable/disable INFET
//   --> ln8282_use_ext_5V()	: enable/disable EXT_5V connection
//   --> ln8282_hw_init()	: initialize chip (after POR)

// misc. features (not implemented here)
//   --> IRQ  : enable interrupt. (should connect to LN8282 nINT pin)
//   --> GPIO : may be used for configuring nEN (typically tied low), and/or nINT
//   --> fault handling (esp. if auto-recovery is disabled)


/*********************************************************************************
 * register map
 *********************************************************************************/

// masks
#define LN8282_MASK_SC_OPERATION_MODE	0x03
#define LN8282_MASK_POWERON_IRQ_EN	0x04
#define LN8282_MASK_SWAP_EN		0x80

#ifdef LN8282_HW_REV_Bx
   #define LN8282_MASK_BC_SYS		0x1F
#else
   #define LN8282_MASK_BC_SYS		0x0F
#endif

#define LN8282_INT_BYTES	3	//only use 3 bytes (INT_HV_SC_1 RSVD)

// register addresses
#define LN8282_REG_DEVICE_ID		0x00
#define LN8282_REG_INT_DEVICE_0		0x01
#define LN8282_REG_INT_DEVICE_1		0x02
#define LN8282_REG_INT_HV_SC_0		0x03
#define LN8282_REG_INT_HV_SC_1		0x04
#define LN8282_REG_INT_DEVICE_0_MSK	0x05
#define LN8282_REG_INT_DEVICE_1_MSK	0x06
#define LN8282_REG_INT_HV_SC_0_MSK	0x07
#define LN8282_REG_INT_HV_SC_1_MSK	0x08
#define LN8282_REG_INT_DEVICE_0_STS	0x09
#define LN8282_REG_INT_DEVICE_1_STS	0x0A
#define LN8282_REG_INT_HV_SC_0_STS	0x0B
#define LN8282_REG_INT_HV_SC_1_STS	0x0C
#define LN8282_REG_DEVICE_CTRL_0	0x0D
#define LN8282_REG_DEVICE_CTRL_1	0x0E
#define LN8282_REG_HV_SC_CTRL_0		0x0F
#define LN8282_REG_HV_SC_CTRL_1		0x10
#define LN8282_REG_HV_SC_CTRL_2		0x11
#define LN8282_REG_SC_DITHER_CTRL	0x12
#define LN8282_REG_GLITCH_CTRL		0x13
#define LN8282_REG_FAULT_CTRL		0x14
#define LN8282_REG_TRACK_CTRL		0x15
#define LN8282_REG_LION_CTRL		0x20

#define LN8282_REG_TRIM_4		0x25
#define LN8282_REG_TRIM_7		0x28


#ifdef LN8282_HW_REV_Bx
   #define LN8282_REG_BC_OP_SUPPORT_CTRL 0x2E
   #define LN8282_REG_NVM_DIN		0x38
   #define LN8282_REG_NVM_CTRL		0x39
   #define LN8282_REG_NVM_DOUT		0x3A
   #define LN8282_REG_STS_F		0x42
   #define LN8282_REG_STS_G		0x43
   #define LN8282_REG_INTERNAL_RSVD	0x4D
   #define LN8282_MAX_REGISTER		LN8282_REG_INTERNAL_RSVD
#else
   #define LN8282_REG_BC_OP_SUPPORT_CTRL 0x2D
   #define LN8282_REG_NVM_DIN		0x32
   #define LN8282_REG_NVM_CTRL		0x33
   #define LN8282_REG_NVM_DOUT		0x34
   #define LN8282_REG_STS_D		0x3A
   #define LN8282_REG_DEVICE_MARKER	0x46
   #define LN8282_MAX_REGISTER		LN8282_REG_DEVICE_MARKER
#endif


// Macros
#define LN8282_REG_PRINT(regmap, reg_addr, val)    \
	do {                                        \
		regmap_read(regmap, reg_addr, &val); \
		pr_info("ln8282_reg:  --> [%-30s]   0x%02X   :   0x%02X\n", \
			#reg_addr, reg_addr, val & 0xFF); \
	} while (0)

/*********************************************************************************
 * data structures / platform data
 *********************************************************************************/


// chip (internal) system state
enum {
    LN8282_STATE_UNKNOWN = -1,
    LN8282_STATE_IDLE = 2,
    LN8282_STATE_SW_ACTIVE  = 7,
    LN8282_STATE_BYPASS_ACTIVE  = 12,
};

// timer values
#define LN8282_TIMER_INIT      3	// min. wait time (after POWERON_IRQ)

/**
 * struct ln8282_info - ln8282 regulator instance
 * @monitor_wake_lock: lock to enter the suspend mode
 * @lock: protects concurrent access to online variables
 * @client: pointer to client
 * @regmap: pointer to driver regmap
 * @op_mode : chip operation mode (STANDBY, BYPASS, SWITCHING)
 * @reverse_power : enable reverse power path
 * @pdata: pointer to platform data
 */

struct ln8282_dt_props {
	unsigned int enable_gpio;
};

struct ln8282_info {
	//struct wake_lock	monitor_wake_lock;
	char                *name;
	struct device       *dev;
	struct i2c_client       *client;
	struct mutex		lock;
	struct regmap		*regmap;
	//struct power_supply	*mains;
	//struct delayed_work work;

	int			op_mode;
	bool			reverse_power;
	bool			auto_recovery;

	struct ln8282_dt_props dt_props;
	struct pinctrl *ln_pinctrl;
	struct pinctrl_state *ln_gpio_active;
	struct pinctrl_state *ln_gpio_suspend;
	//struct ln8282_platform_data *pdata;
	struct power_supply	*ln_psy;
};

static struct ln8282_info *g_info;

static const struct regmap_config ln8282_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= LN8282_MAX_REGISTER,
};


/*********************************************************************************
 * supported CORE functionality
 *********************************************************************************/

void ln8282_use_ext_5V(struct ln8282_info *info, unsigned int enable)
{
	regmap_update_bits(info->regmap, LN8282_REG_DEVICE_CTRL_1,
				0x01,
				(enable & 0x01));
}
EXPORT_SYMBOL(ln8282_use_ext_5V);


void ln8282_set_infet(struct ln8282_info *info, unsigned int enable)
{
	regmap_update_bits(info->regmap, LN8282_REG_DEVICE_CTRL_0,
				0x01, (enable & 0x01));
}
EXPORT_SYMBOL(ln8282_set_infet);


void ln8282_set_powerpath(struct ln8282_info *info, bool forward_path)
{
	info->reverse_power = (!forward_path);
}
EXPORT_SYMBOL(ln8282_set_powerpath);

/* add for other module run */
/*
void ln8282_set_powerpath_ext(bool forward_path)
{
	g_info->reverse_power = (!forward_path);
	printk("%s:reverse_power:%d\n", __func__, g_info->reverse_power);
}
*/

static inline int ln8282_get_opmode(struct ln8282_info *info)
{
	int val;
	if (regmap_read(info->regmap, LN8282_REG_HV_SC_CTRL_0, &val) < 0)
		val = LN8282_OPMODE_UNKNOWN;
	return (val & LN8282_MASK_SC_OPERATION_MODE);
}

/* add for wireless driver get opmode */
/*
unsigned int ln8282_get_opmode_ext(void)
{
	int val;
	if (regmap_read(g_info->regmap, LN8282_REG_HV_SC_CTRL_0, &val) < 0)
	   val = LN8282_OPMODE_UNKNOWN;
	return (val & LN8282_MASK_SC_OPERATION_MODE);
}
*/
static inline void ln8282_auto_recovery(struct ln8282_info *info, unsigned int enable)
{
	//EXPERIMENTAL:
	//-- auto-recovery should be programmed via OTP instead
	//-- enable option to control via SW for testing
	regmap_update_bits(info->regmap, LN8282_REG_DEVICE_CTRL_1,
				0x40,
				(enable<<6));
}

/* configure minimum set of control registers */
static inline void ln8282_set_base_opt_Bx(struct ln8282_info *info,
			unsigned int sc_out_precharge_cfg,
			unsigned int vbus_in_max_ov_cfg,
			unsigned int track_cfg)
{
	regmap_update_bits(info->regmap, LN8282_REG_HV_SC_CTRL_1, 0x60,
				(vbus_in_max_ov_cfg<<6) |
				(sc_out_precharge_cfg<<5));
	regmap_update_bits(info->regmap, LN8282_REG_TRACK_CTRL,
				0x10,
				(track_cfg<<4));

	ln8282_auto_recovery(info, 1);
}

static inline void ln8282_set_vbus_uv_track(struct ln8282_info *info,
			unsigned int disable)
{
	regmap_update_bits(info->regmap, LN8282_REG_TRACK_CTRL,
				0x04,
				(disable<<2));
}

static inline void ln8282_set_switch_seq(struct ln8282_info *info,
			unsigned int alt_en)
{
	regmap_write(info->regmap, LN8282_REG_LION_CTRL, 0x5B);
	regmap_update_bits(info->regmap, LN8282_REG_TRIM_4,
				0x04,
				(alt_en<<2));
	regmap_write(info->regmap, LN8282_REG_LION_CTRL, 0x00);
}

static void ln8282_enter_standby(struct ln8282_info *info)
{
	//update opmode
	regmap_update_bits(info->regmap, LN8282_REG_HV_SC_CTRL_0,
				LN8282_MASK_SC_OPERATION_MODE,
				LN8282_OPMODE_STANDBY);
	ln8282_set_base_opt_Bx(info, 1, 0, 1);
	regmap_write(info->regmap, LN8282_REG_FAULT_CTRL, 0x04);
}


static void ln8282_enter_bypass(struct ln8282_info *info)
{
	ln8282_set_base_opt_Bx(info, 1, 0, 1);

	if (info->reverse_power)
	   regmap_write(info->regmap, LN8282_REG_FAULT_CTRL, 0x0B);
	else
	   regmap_write(info->regmap, LN8282_REG_FAULT_CTRL, 0x0C);
	//update opmode
	regmap_update_bits(info->regmap, LN8282_REG_HV_SC_CTRL_0,
			LN8282_MASK_SC_OPERATION_MODE, LN8282_OPMODE_BYPASS);
}

static bool ln8282_vin_switch_ok(struct ln8282_info *info)
{
	int val;
	if (regmap_read(info->regmap, LN8282_REG_INT_DEVICE_0_STS, &val) < 0) {
	   pr_warn("%s: Unable to read INT_DEVICE_0_STS\n", __func__);
	   return false;
	}

	if ((val & 0x08) <= 0) {
	   pr_warn("%s: INVALID VIN range for switching operation.\n", __func__);
	   return false;
	}

	return true;
}

static void ln8282_enter_switching(struct ln8282_info *info)
{
	if (info->reverse_power) {
       pr_info("%s:set switch in reverse mode\n");
	   ln8282_set_switch_seq(info, 0);
	   ln8282_set_base_opt_Bx(info, 0, 1, 0);
	   ln8282_set_vbus_uv_track(info, 1);//disable during startup
	   regmap_write(info->regmap, LN8282_REG_FAULT_CTRL, 0x09);
	} else {
	   //check if VIN is at valid range for SWITCHING operation
	   if (!ln8282_vin_switch_ok(info)) {
	      pr_warn("%s: Failed to transition to forward SWITCHING mode.\n", __func__);
	      return;
	   }
	   pr_info("%s:set switch in forward mode\n");
	   ln8282_set_switch_seq(info, 1);
	   ln8282_set_base_opt_Bx(info, 0, 1, 0);
	   regmap_write(info->regmap, LN8282_REG_FAULT_CTRL, 0x04);
	}
	//update opmode
	regmap_update_bits(info->regmap, LN8282_REG_HV_SC_CTRL_0,
			LN8282_MASK_SC_OPERATION_MODE, LN8282_OPMODE_SWITCHING);

	if (info->reverse_power) {
	   msleep(50);
	   ln8282_set_vbus_uv_track(info, 0);//enable
	} else {
	   //EXPERIMENTAL
	   //-- disable VIN_SWITCH_OK checks to retain SWITCHING operation
	   //   when VIN droops
	   msleep(100);
	   regmap_write(info->regmap, LN8282_REG_FAULT_CTRL, 0x0C);//+DISABLE_VIN_SWITCH_OK=1
	}
}
/* main function for setting/changing operation mode */
bool ln8282_change_opmode(struct ln8282_info *info, unsigned int target_mode)
{
	bool ret;

	info->op_mode = ln8282_get_opmode(info);
	if (target_mode < 0 || target_mode > LN8282_OPMODE_SWITCHING_ALT) {
	   pr_err("%s: target operation mode (0x%02X) is invalid\n",
		  __func__, target_mode);
	   return false;
	}
	/* NOTE:
	 *      CUSTOMER should know/indicate if power path is forward/reverse mode
	 *      based on power connections before attempting to change operation mode
	*/
	//info->reverse_power = false;
	dev_info(info->dev, "opmode from %d change to %d\n",
					info->op_mode, target_mode);
	ret = true;
	switch (target_mode) {
	case LN8282_OPMODE_STANDBY:
		ln8282_enter_standby(info);
		break;
	case LN8282_OPMODE_BYPASS:
		ln8282_enter_bypass(info);
		break;
	case LN8282_OPMODE_SWITCHING:
	case LN8282_OPMODE_SWITCHING_ALT:
		ln8282_enter_switching(info);
		break;
	default:
		ret = false;
	}
	return ret;
}
EXPORT_SYMBOL(ln8282_change_opmode);

/*
bool ln8282_change_opmode_ext(unsigned int target_mode)
{
	g_info->op_mode = ln8282_get_opmode(g_info);

	if (target_mode < 0 || target_mode > LN8282_OPMODE_SWITCHING_ALT) {
	pr_err("%s: target operation mode (0x%02X) is invalid\n",
		  __func__, target_mode);
	   return false;
	}
	printk("opmode from %d change to %d\n", g_info->op_mode, target_mode);
	switch (target_mode) {
	case LN8282_OPMODE_STANDBY:
		ln8282_enter_standby(g_info);
		break;
	case LN8282_OPMODE_BYPASS:
		ln8282_enter_bypass(g_info);
		break;
	case LN8282_OPMODE_SWITCHING:
	case LN8282_OPMODE_SWITCHING_ALT:
		ln8282_enter_switching(g_info);
		break;
	default:
		return false;
	}
	return true;
}
*/

int ln8282_hw_init(struct ln8282_info *info)
{
	/* NOTES:
	 *   When power source is removed, LN8282 is powered off and all register settings are lost
	 *   So, LN8282 should be re-initialized every time it is powered on
	 *   This routine should be called when the RX IC or PMIC detects a new power source
	 */

	/* CUSTOMER should add basic initialization tasks
	 * -- unmasking interrupts
	 * -- overriding OTP settings, etc
	 */
	pr_info("%s: HW initialization\n", __func__);

	// TEMPORARY FIX TO ENABLE 12V BYPASS
#ifdef LN8282_HW_REV_A1
	regmap_update_bits(info->regmap, LN8282_REG_HV_SC_CTRL_1, 0x10, (1<<4)); //SC_OUT_MAX_OV_CFG=1 (20V threshold)
	regmap_update_bits(info->regmap, LN8282_REG_TRACK_CTRL, 0x04, (1<<2));   //DISABLE_VIN_UV_TRACK=1 (side-effect of raising 11V->20V)
#endif
	// TEMPORARY FIX TO PREVENT OV FROM RX IC (due to ASK modulation)
	//regmap_update_bits(info->regmap, LN8282_REG_FAULT_CTRL, 0x30, (0x3<<4)); //DISABLE_SC_OUT_MAX_OV=1, DISABLE_VBUS_IN_MAX_OV=1

	// TEMPORARY FIX (should be addressed later in OTP)
#ifdef LN8282_HW_REV_Bx
	regmap_write(info->regmap, LN8282_REG_LION_CTRL, 0x5B);
	//disable (BYPASS) instruction swap
	regmap_update_bits(info->regmap, LN8282_REG_TRIM_7, LN8282_MASK_SWAP_EN, 0/*<<7*/);
	regmap_write(info->regmap, LN8282_REG_LION_CTRL, 0x00);
#endif


	// Unmask generic interrupts
	// -- unmask here (instead of in ln8282_irq_init()) since chip may not be
	//    powered up when ln8282_irq_init() runs
	//if (info->pdata && (info->pdata->irq_gpio >= 0)) {
	//   regmap_write(info->regmap, LN8282_REG_INT_DEVICE_0_MSK, 0x00);
	//   regmap_write(info->regmap, LN8282_REG_INT_DEVICE_1_MSK, 0x00);
	//   regmap_write(info->regmap, LN8282_REG_INT_HV_SC_0_MSK,  0x00);
	//   //regmap_write(info->regmap, LN8282_REG_INT_HV_SC_1_MSK,  0x00);//RSVD
	//}

	info->reverse_power = false;//LION-DEBUG: set as default

	return 1;
}
EXPORT_SYMBOL(ln8282_hw_init);


/*********************************************************************************
 * (experimental) OTP support	: NEVER use this in production releases
 *********************************************************************************/

#ifdef LN8282_DEBUG_OTP_CTRL
#define LN8282_OTP_MASK_WRITE_EN  0x10
#define LN8282_OTP_MASK_CS        0x80
#define LN8282_OTP_MASK_ADDR      0x0F
#define LN8282_OTP_MASK_PROG      0x20
static void ln8282_otp_power(struct ln8282_info *info, const bool enable)
{
	if (enable) {
	   //power up (raise VPP)
	   msleep(2);//vpps + vpph
	   regmap_update_bits(info->regmap, LN8282_REG_BC_OP_SUPPORT_CTRL,
				0x02, (1<<1));//VPP_to_HV=1
	   msleep(1);//vpph
	} else {
	   //power down (lower VPP)
	   msleep(2);//vpps + vpph
	   regmap_update_bits(info->regmap, LN8282_REG_BC_OP_SUPPORT_CTRL,
				0x02, (0<<1));//VPP_to_HV=0
	   msleep(1);//vppr
	}
}

static void ln8282_otp_write(struct ln8282_info *info,
			unsigned int otp_addr,
			unsigned int otp_data)
{
	unsigned int nvm_ctrl;//NVM_CTRL value

	//-----------------------------------------
	//Pre-program phase
	regmap_write(info->regmap, LN8282_REG_LION_CTRL, 0x5B);
	ln8282_otp_power(info, true/*power-up*/);

	// reset DIN, ADR
	regmap_write(info->regmap, LN8282_REG_NVM_DIN, 0xFF);
	nvm_ctrl = 0xFF & LN8282_OTP_MASK_WRITE_EN;
	regmap_write(info->regmap, LN8282_REG_NVM_CTRL, nvm_ctrl);

	//-----------------------------------------
	//Setup OTP ADDR/DIN
	msleep(1);//vpph

	nvm_ctrl |= (0xFF & LN8282_OTP_MASK_CS);//CS = 1
	regmap_write(info->regmap, LN8282_REG_NVM_CTRL, nvm_ctrl);

	regmap_write(info->regmap, LN8282_REG_NVM_DIN, otp_data);//set DATA

	nvm_ctrl |= (LN8282_OTP_MASK_ADDR & otp_addr);//set ADDR
	regmap_write(info->regmap, LN8282_REG_NVM_CTRL, nvm_ctrl);
	udelay(6);//css

	//-----------------------------------------
	//OTP Program
	nvm_ctrl |= (0xFF & LN8282_OTP_MASK_PROG);//PROG = 1
	regmap_write(info->regmap, LN8282_REG_NVM_CTRL, nvm_ctrl);

	udelay(220);//pgm (200 ~ 400us)

	nvm_ctrl &= (0xFF & ~LN8282_OTP_MASK_PROG);//PROG = 0
	regmap_write(info->regmap, LN8282_REG_NVM_CTRL, nvm_ctrl);

	udelay(6*2);//csh*2

	//-----------------------------------------
	//Post-program phase
	regmap_write(info->regmap, LN8282_REG_NVM_DIN, 0xFF);

	nvm_ctrl &= (0xFF & ~LN8282_OTP_MASK_CS);//CS = 0
	regmap_write(info->regmap, LN8282_REG_NVM_CTRL, nvm_ctrl);

	udelay(6);//csh

	nvm_ctrl &= (0xFF & ~LN8282_OTP_MASK_WRITE_EN);//WRITE_EN = 0
	regmap_write(info->regmap, LN8282_REG_NVM_CTRL, nvm_ctrl);


	//-----------------------------------------
	//Cleanup phase
	ln8282_otp_power(info, false/*power-down*/);
	regmap_write(info->regmap, LN8282_REG_LION_CTRL, 0x00);
}

#endif


/*********************************************************************************
 * LION-DEBUG: user-space interaction through sysfs (for testing only)
 *********************************************************************************/

static unsigned int ln8282_regcmd_addr;
static unsigned int ln8282_regcmd_data;
static bool         ln8282_regcmd_valid;

static ssize_t ln8282_sysfs_show_regcmd(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	//struct ln8282_info *info = dev_get_drvdata(dev);

	if (ln8282_regcmd_valid) {
	   pr_info("%s: /sysfs/ return stored data value (0x%02X)\n",
		__func__, ln8282_regcmd_data & 0xFF);
	   return scnprintf(buf, PAGE_SIZE, "0x%02x\n", ln8282_regcmd_data & 0xFF);
	} else {
	   pr_warn("%s: /sysfs/ regcmd is invalid or was not issued\n", __func__);
	   return scnprintf(buf, PAGE_SIZE, "NA\n");
	}
}

static ssize_t ln8282_sysfs_store_regcmd(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln8282_info *info = dev_get_drvdata(dev);
	bool write_cmd;
	int ret;
	/* command format:
	 * 	>>read,0x01		: read from 0x01
	 * 	>>write,0x01,0xAA	: write to  0x01
	 */
	//parse command (read | write)
	ln8282_regcmd_valid = false;
	if (strncmp("read,",  buf, 5) == 0)
		write_cmd = false;
	else if (strncmp("write,", buf, 6) == 0)
		write_cmd = true;
	else {
		pr_warn("%s: /sysfs/ invalid command: %s\n", __func__, buf);
		goto parse_error;
	}

	if (write_cmd) {
		if (sscanf(buf, "write,%x,%x", &ln8282_regcmd_addr, &ln8282_regcmd_data) != 2)
			goto parse_error;
		if (ln8282_regcmd_addr > LN8282_MAX_REGISTER || ln8282_regcmd_data > 0xFF) {
			pr_warn("%s: /sysfs/ write command addr/data out of bounds\n", __func__);
			goto parse_error;
		}

		pr_info("%s: /sysfs/ writing to addr 0x%02X (data=0x%02X)\n",
			__func__, ln8282_regcmd_addr, ln8282_regcmd_data);
		ret = regmap_write(info->regmap, ln8282_regcmd_addr, ln8282_regcmd_data);
	} else {
		if (sscanf(buf, "read,%x", &ln8282_regcmd_addr) != 1)
			goto parse_error;
		if (ln8282_regcmd_addr > LN8282_MAX_REGISTER) {
			pr_warn("%s: /sysfs/ read command addr out of bounds\n", __func__);
			goto parse_error;
		}

		pr_info("%s: /sysfs/ reading from addr 0x%02X\n",
			__func__, ln8282_regcmd_addr);
		ret = regmap_read(info->regmap, ln8282_regcmd_addr, &ln8282_regcmd_data);
	}
	if (ret < 0)
		return ret;

	ln8282_regcmd_valid = true;
	return count;

parse_error:
	pr_warn("%s: /sysfs/ unable to parse command (%s)\n", __func__, buf);
	return count;
}


static ssize_t ln8282_sysfs_show_opmode(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ln8282_info *info = dev_get_drvdata(dev);
	int ret;

	pr_info("%s: /sysfs/ retrieve op_mode\n", __func__);

	info->op_mode = ln8282_get_opmode(info);
	switch (info->op_mode) {
	case LN8282_OPMODE_STANDBY:
		ret = scnprintf(buf, PAGE_SIZE, "standby\n");
		break;
	case LN8282_OPMODE_BYPASS:
		ret = scnprintf(buf, PAGE_SIZE, "bypass\n");
		break;
	case LN8282_OPMODE_SWITCHING:
	case LN8282_OPMODE_SWITCHING_ALT:
		ret = scnprintf(buf, PAGE_SIZE, "switching\n");
		break;
	case LN8282_OPMODE_UNKNOWN:
	default:
		ret = scnprintf(buf, PAGE_SIZE, "unknown\n");
	}
	return ret;
}

static ssize_t ln8282_sysfs_show_dump(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ln8282_info *info = dev_get_drvdata(dev);
	int val;

	pr_info("----------------------/dump/---------------------\n");
	pr_info("ln8282_reg:      [           %-19s]  /addr/  :  /value/\n", "Register Name");
	LN8282_REG_PRINT(info->regmap, LN8282_REG_DEVICE_CTRL_0, val);
	LN8282_REG_PRINT(info->regmap, LN8282_REG_HV_SC_CTRL_0, val);
	LN8282_REG_PRINT(info->regmap, LN8282_REG_HV_SC_CTRL_1, val);
	LN8282_REG_PRINT(info->regmap, LN8282_REG_FAULT_CTRL, val);
	LN8282_REG_PRINT(info->regmap, LN8282_REG_TRACK_CTRL, val);

#ifdef LN8282_HW_REV_Bx
	LN8282_REG_PRINT(info->regmap, LN8282_REG_STS_F, val);
	LN8282_REG_PRINT(info->regmap, LN8282_REG_STS_G, val);
#else
	LN8282_REG_PRINT(info->regmap, LN8282_REG_STS_D, val);
#endif

	pr_info("----------------------/dump/---------------------\n");
	return scnprintf(buf, PAGE_SIZE, "done\n");
}

static ssize_t ln8282_sysfs_store_opmode(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln8282_info *info = dev_get_drvdata(dev);
	int opmode;

	if (strncmp("standby", buf, 7) == 0)
		opmode = LN8282_OPMODE_STANDBY;
	else if (strncmp("bypass", buf, 6) == 0)
		opmode = LN8282_OPMODE_BYPASS;
	else if (strncmp("switching", buf, 9) == 0)
		opmode = LN8282_OPMODE_SWITCHING;
	else
		opmode = LN8282_OPMODE_UNKNOWN;
	pr_info("%s: /sysfs/ set op_mode to 0x%02X==%s", __func__, opmode, buf);

	if (!ln8282_change_opmode(info, opmode))
		pr_warn("%s: /sysfs/ unable to enter %s\n", __func__, buf);
	return count;
}

static ssize_t ln8282_sysfs_store_infet(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln8282_info *info = dev_get_drvdata(dev);
	unsigned int enable;
	if (strncmp("1", buf, 1) == 0)
		enable = 1;
	else
		enable = 0;
	ln8282_set_infet(info, enable);
	return count;
}

static ssize_t ln8282_sysfs_store_ext_5v(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln8282_info *info = dev_get_drvdata(dev);
	unsigned int enable;
	if (strncmp("1", buf, 1) == 0)
		enable = 1;
	else
		enable = 0;
	ln8282_use_ext_5V(info, enable);
	return count;
}

static ssize_t ln8282_sysfs_store_powerpath(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count)
{
	struct ln8282_info *info = dev_get_drvdata(dev);
	bool forward;
	if (strncmp("forward", buf, 1) == 0)
		forward = true;
	else
		forward = false;
	ln8282_set_powerpath(info, forward);
	return count;
}

static ssize_t ln8282_sysfs_hw_init(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ln8282_info *info = dev_get_drvdata(dev);

	ln8282_hw_init(info);
	return scnprintf(buf, PAGE_SIZE, "done\n");
}


#ifdef LN8282_DEBUG_OTP_CTRL
static ssize_t ln8282_sysfs_otp_overwrite(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct ln8282_info *info = dev_get_drvdata(dev);

	// Experimental Feature:
	// -- this should NEVER be called in production releases
	// -- overwrite OTP cell 0xF:
	//    -- is 0xF0, now overwrite to 0xF8 (DISABLE_VIN_OV_TRACK=1)
#ifdef LN8282_HW_REV_Bx
	pr_info("%s: ********************** WARNING **********************\n", __func__);
	pr_info("%s: --> this is an experimental feature (temporary fix)\n", __func__);
	pr_info("%s:     NEVER use this in production release\n", __func__);
	pr_info("%s: --> begin to overwrite OTP cell [0xF] to 0xF8\n", __func__);

	ln8282_otp_write(info, 0x0F/*OTP cell addr*/, 0xF8/*new OTP value*/);

	pr_info("%s: --> finished overwriting OTP cell\n", __func__);
	pr_info("%s: --> now, unplug/re-plugin VIN to power-up chip\n", __func__);
	pr_info("%s: *****************************************************\n", __func__);
#endif

	return scnprintf(buf, PAGE_SIZE, "unplug VIN to power-up LN8282 again\n");
}
static DEVICE_ATTR(otp_overwrite, 0444, ln8282_sysfs_otp_overwrite, NULL);
#endif


static DEVICE_ATTR(dump, 0444, ln8282_sysfs_show_dump, NULL);
static DEVICE_ATTR(hw_init, 0444, ln8282_sysfs_hw_init, NULL);
static DEVICE_ATTR(infet, 0200, NULL, ln8282_sysfs_store_infet);
static DEVICE_ATTR(ext_5v, 0200, NULL, ln8282_sysfs_store_ext_5v);
static DEVICE_ATTR(powerpath, 0200, NULL, ln8282_sysfs_store_powerpath);
static DEVICE_ATTR(opmode, 0644, ln8282_sysfs_show_opmode, ln8282_sysfs_store_opmode);
static DEVICE_ATTR(regcmd, 0644, ln8282_sysfs_show_regcmd, ln8282_sysfs_store_regcmd);
static struct attribute *ln8282_info_attr[] = {
	&dev_attr_dump.attr,
	&dev_attr_hw_init.attr,
#ifdef LN8282_DEBUG_OTP_CTRL
	&dev_attr_otp_overwrite.attr,
#endif
	&dev_attr_regcmd.attr,
	&dev_attr_opmode.attr,
	&dev_attr_infet.attr,
	&dev_attr_ext_5v.attr,
	&dev_attr_powerpath.attr,
	NULL,
};

static const struct attribute_group ln8282_attr_group = {
	.attrs = ln8282_info_attr,
};


/*********************************************************************************
 * device layer
 *********************************************************************************/

static int ln8282_parse_dt(struct ln8282_info *info)
{
	struct device_node *node = info->dev->of_node;
	if (!node) {
		dev_err(info->dev, "device tree node missing\n");
		return -EINVAL;
	}

	info->dt_props.enable_gpio = of_get_named_gpio(node, "ln,enable", 0);
	if ((!gpio_is_valid(info->dt_props.enable_gpio)))
		return -EINVAL;

	return 0;
}

static int ln8282_gpio_init(struct ln8282_info *info)
{
	int ret = 0;

	info->ln_pinctrl = devm_pinctrl_get(info->dev);
	if (IS_ERR_OR_NULL(info->ln_pinctrl)) {
		dev_err(info->dev, "No pinctrl config specified\n");
		ret = PTR_ERR(info->dev);
		return ret;
	}
	info->ln_gpio_active =
		pinctrl_lookup_state(info->ln_pinctrl, "ln8282_active");
	if (IS_ERR_OR_NULL(info->ln_gpio_active)) {
		dev_err(info->dev, "No active config specified\n");
		ret = PTR_ERR(info->ln_gpio_active);
		return ret;
	}
	info->ln_gpio_suspend =
		pinctrl_lookup_state(info->ln_pinctrl, "ln8282_suspend");
	if (IS_ERR_OR_NULL(info->ln_gpio_suspend)) {
		dev_err(info->dev, "No suspend config specified\n");
		ret = PTR_ERR(info->ln_gpio_suspend);
		return ret;
	}

	ret = pinctrl_select_state(info->ln_pinctrl,
			info->ln_gpio_active);
	if (ret < 0) {
		dev_err(info->dev, "fail to select pinctrl active rc=%d\n",
			ret);
		return ret;
	}

	return ret;
}

#if 0
extern char *saved_command_line;

static int get_board_version(void)
{
	char boot[5] = {'\0'};
	char *match = (char *) strnstr(saved_command_line,
				"androidboot.hwlevel=",
				strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.hwlevel=")),
			sizeof(boot) - 1);
		printk("%s: hwlevel is %s\n", __func__, boot);
		if (!strncmp(boot, "P1.3", strlen("P1.3")))
			return 0;
	}
	return 1;
}
#endif

#define FORWARD_BYPASS		1
#define FORWARD_SWITCH		2
#define REVERSE_SWITCH		3
#define REVERSE_BYPASS		4
static int ln8282_set_mode(struct ln8282_info *info, int mode)
{
	int ret = 0;
	switch (mode) {
	case FORWARD_BYPASS:
		ln8282_set_powerpath(info, true);
		ln8282_change_opmode(info, LN8282_OPMODE_BYPASS);
		break;
	case FORWARD_SWITCH:
		ln8282_set_powerpath(info, true);
		ln8282_change_opmode(info, LN8282_OPMODE_SWITCHING);
		break;
	case REVERSE_SWITCH:
		ln8282_set_powerpath(info, false);
		ln8282_change_opmode(info, LN8282_OPMODE_SWITCHING);
		break;
	case REVERSE_BYPASS:
		ln8282_set_powerpath(info, false);
		ln8282_change_opmode(info, LN8282_OPMODE_BYPASS);
	default:
		dev_err(info->dev, "%s: invalid settings\n",
				 __func__);
	}

	info->op_mode = ln8282_get_opmode(info);

	return ret;
}

static enum power_supply_property ln8282_props[] = {
	POWER_SUPPLY_PROP_DIV_2_MODE,
	POWER_SUPPLY_PROP_RESET_DIV_2_MODE,
};

static int ln8282_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct ln8282_info *info = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_DIV_2_MODE:
		val->intval = info->op_mode;
		break;
	default:
			return -EINVAL;
	}
	return 0;
}

static int ln8282_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct ln8282_info *info = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_DIV_2_MODE:
		rc = ln8282_set_mode(info, val->intval);
		break;
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
		info->op_mode = 0;
		break;
	default:
			return -EINVAL;
	}

	return rc;
}

static int ln8282_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_DIV_2_MODE:
	case POWER_SUPPLY_PROP_RESET_DIV_2_MODE:
		return 1;
	default:
		rc = 0;
		break;
	}

	return rc;
}


static const struct power_supply_desc ln_psy_desc = {
	.name = "lionsemi",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = ln8282_props,
	.num_properties = ARRAY_SIZE(ln8282_props),
	.get_property = ln8282_get_prop,
	.set_property = ln8282_set_prop,
	.property_is_writeable = ln8282_prop_is_writeable,
};

static int ln8282_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ln8282_info *info;
	struct power_supply_config ln_cfg = {};
	int ret = 0;
	pr_info("%s: =/START-PROBE/=\n", __func__);

	/* allocate memory for our device state and initialize it */
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL)
	   return -ENOMEM;

	mutex_init(&info->lock);
	info->name = LN8282_DRIVER_NAME;
	info->client  = client;
	info->dev = &client->dev;

	i2c_set_clientdata(client, info);

	info->regmap = devm_regmap_init_i2c(client, &ln8282_regmap_config);
	if (IS_ERR(info->regmap)) {
	   pr_err("%s: failed to initialize regmap\n", __func__);
	   return PTR_ERR(info->regmap);
	}

	ret = ln8282_parse_dt(info);
	if (ret < 0) {
		dev_err(info->dev, "%s: parse dt error [%d]\n",
				 __func__, ret);
		goto cleanup;
	}

	ret = ln8282_gpio_init(info);
	if (ret < 0) {
		dev_err(info->dev, "%s: gpio init error [%d]\n",
				__func__, ret);
		goto cleanup;
	}

	// NOTES: should only run if HW is powered up
	// 	  --> trigger ln8282_hw_init() from another module
	//if (ln8282_hw_init(info) < 0) {
	//   pr_err("%s: hardware initialization error\n", __func__);
	//   return -EINVAL;
	//}

	/* LION-DEBUG: test user-app through sysfs */
	if (sysfs_create_group(&info->client->dev.kobj, &ln8282_attr_group) < 0) {
	   pr_err("%s: unable to create sysfs entries\n", __func__);
	   return -EINVAL;
	}

	g_info = info;

	ln_cfg.drv_data = info;
	info->ln_psy = power_supply_register(info->dev,
			&ln_psy_desc,
			&ln_cfg);

	pr_info("[ln8282]%s: success probe!\n", __func__);
	return 0;

cleanup:
	i2c_set_clientdata(client, NULL);
	return 0;
}

static int ln8282_remove(struct i2c_client *client)
{
	struct ln8282_info *info = i2c_get_clientdata(client);

	dev_err(info->dev, "%s: driver remove\n", __func__);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ln8282_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ln8282_info *info = i2c_get_clientdata(client);

	/* don't set bypass when suspend for reverse charge
	ln8282_change_opmode(info, LN8282_OPMODE_BYPASS);
	*/
	dev_info(info->dev, "%s: suspend\n", __func__);

	return 0;
}

static int ln8282_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ln8282_info *info = i2c_get_clientdata(client);

	pr_info("%s: update/resume\n", __func__);
	dev_err(info->dev, "%s: resume\n", __func__);
	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(ln8282_pm_ops, ln8282_suspend, ln8282_resume);

#ifdef CONFIG_OF
static const struct of_device_id ln8282_dt_match[] = {
	{ .compatible = "lionsemi,ln8282" },
	{ },
};
MODULE_DEVICE_TABLE(of, ln8282_dt_match);
#endif

static const struct i2c_device_id ln8282_id[] = {
	{ "ln8282", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ln8282_id);

static struct i2c_driver ln8282_driver = {
	.driver   = {
		.name = LN8282_DRIVER_NAME,
		.of_match_table = of_match_ptr(ln8282_dt_match),
		.pm   = &ln8282_pm_ops,
	},
	.probe    = ln8282_probe,
	.remove   = ln8282_remove,
	.id_table = ln8282_id,
};

static int __init ln8282_init(void)
{
	int ret;
#if 0
	int drv_load = 0;

	drv_load = get_board_version();
	if (!drv_load)
		return 0;
#endif
	ret = i2c_add_driver(&ln8282_driver);
	if (ret)
		printk(KERN_ERR "ln8282 i2c driver init failed!\n");

	return ret;
}

static void __exit ln8282_exit(void)
{
	i2c_del_driver(&ln8282_driver);
}

module_init(ln8282_init);
module_exit(ln8282_exit);

MODULE_AUTHOR("Jae Lee");
MODULE_DESCRIPTION("LN8282 driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.3.0");
