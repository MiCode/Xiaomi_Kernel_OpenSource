/**
 * Copyright “Copyright (C) 2019 XiaoMi, Inc
 *
 * L.D
**/
#include <linux/module.h>
#include <linux/alarmtimer.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/power_supply.h>
#include <asm/unaligned.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>

#include <linux/power/ln8282.h>

#include "rx1619.h"

#define rx1619_DRIVER_NAME      "rx1619"

#define GOOD                 0
#define STILL                1
#define LIMIT                2
#define EPP_MODE             1
#define BPP_MODE             0
#define MIN_VOUT            4000
#define MAX_VOUT            15000
#define AP_REV_DATA_OK       0xaa
#define AP_SENT_DATA_OK      0x55
#define PRIVATE_VBUCK_SET_CMD 0x80
#define PRIVATE_ID_CMD       0x86
#define PRIVATE_USB_TYPE_CMD 0x87
#define PRIVATE_FAST_CHG_CMD 0x88
#define PRIVATE_PRODUCT_TEST_CMD 0x8D
#define PRIVATE_TX_HW_ID_CMD 0x8b

/*adapter type*/
#define ADAPTER_NONE 0x00
#define ADAPTER_SDP  0x01
#define ADAPTER_CDP  0x02
#define ADAPTER_DCP  0x03
#define ADAPTER_QC2  0x05
#define ADAPTER_QC3  0x06
#define ADAPTER_PD   0x07
#define ADAPTER_AUTH_FAILED   0x08
#define ADAPTER_XIAOMI_QC3    0x09
#define ADAPTER_XIAOMI_PD     0x0a
#define ADAPTER_ZIMI_CAR_POWER    0x0b
#define ADAPTER_XIAOMI_PD_40W     0x0c



#define EPP_MODE_CURRENT 600000
#define DC_OTHER_CURRENT 800000
#define DC_LOW_CURRENT 200000
#define DC_DCP_CURRENT 700000
#define DC_CDP_CURRENT 800000
#define DC_QC2_CURRENT 1000000
#define DC_QC3_CURRENT 1800000
#define DC_QC3_BPP_CURRENT 900000
#define DC_TURBO_CURRENT 1100000
#define DC_QC3_20W_CURRENT 2200000
#define DC_PD_CURRENT      1000000
#define DC_PD_20W_CURRENT  2000000
#define DC_PD_40W_CURRENT  3100000
#define DC_BPP_CURRENT 850000
#define DC_SDP_CURRENT 500000
#define SCREEN_OFF_FUL_CURRENT 250000
#define DC_FULL_CURRENT 360000
#define DC_BPP_AUTH_FAIL_CURRENT 700000

#define ADAPTER_DEFAULT_VOL	6000
#define ADAPTER_BPP_LIMIT_VOL	6500
#define ADAPTER_VOUT_LIMIT_VOL	7000
#define ADAPTER_BPP_PLUS_VOL	9000
#define ADAPTER_EPP_QC3_VOL	11000
#define EPP_VOL_THRESHOLD	12000
#define ADAPTER_EPP_MI_VOL	15000
#define ADAPTER_BPP_QC2_VOL	6500

#define LIMIT_SOC		95
#define FULL_SOC		100
#define LIMIT_EPP_IOUT		500
#define EXCHANGE_9V		0x0
#define EXCHANGE_5V		0x1
#define EXCHANGE_15V		0x0
#define EXCHANGE_10V		0x1
#define ICL_EXCHANGE_COUNT	2 /*5 = 1min*/
#define LIMIT_EPP_IOUT 500
#define LIMIT_BPP_IOUT 500

/* used registers define */
#define REG_RX_SENT_CMD      0x0000
#define REG_RX_SENT_DATA1    0x0001
#define REG_RX_SENT_DATA2    0x0002
#define REG_RX_SENT_DATA3    0x0003
#define REG_RX_SENT_DATA4    0x0004
#define REG_RX_SENT_DATA5    0x0005
#define REG_RX_SENT_DATA6    0x0006
#define REG_RX_REV_CMD       0x0020
#define REG_RX_REV_DATA1     0x0021
#define REG_RX_REV_DATA2     0x0022
#define REG_RX_REV_DATA3     0x0023
#define REG_RX_REV_DATA4     0x0024

#define REG_CEP_VALUE        0x0008
#define REG_RX_VOUT          0x0009
#define REG_RX_VRECT         0x000a
#define REG_RX_IOUT          0x000b

#define REG_AP_RX_COMM       0x000C
#define ADDR_RX              256
#define ADDR_TX              4864

#define ID_CMD               0x3b
#define AUTH_CMD             0x86
#define UUID_CMD             0x90

u8 fod_param[8] = {
0x44, 0x1E,
0x44, 0x37,
0x3A, 0x46,
0x37, 0x5A
};

static struct rx1619_chg *g_chip;
static int g_Delta;
static u8 g_id_done_flag;
static u8 g_fw_rx_id;
static u8 g_fw_tx_id;
static u8 g_hw_id_h,g_hw_id_l;
static u8 g_epp_or_bpp = BPP_MODE;
static u8 g_USB_TYPE = 1;
static int g_current_now = DC_LOW_CURRENT;
static bool g_rx1619_restart_flag;

static int success_count;
static u32 g_fw_data_lenth;
static u8 g_download_area;
static u8 g_update_fw_status = 0x99;
static u8 boot_fw_version;
static u8 rx_fw_version;
static u8 tx_fw_version;

static int rx_set_enable_mode(struct rx1619_chg *chip,
				int enable);
static int rx_get_reverse_chg_mode(struct rx1619_chg *chip);
static int rx_set_reverse_gpio(struct rx1619_chg *chip,
				int enable);
static int rx_set_reverse_chg_mode(struct rx1619_chg *chip,
				int enable);

#define BOOT_AREA   0
#define RX_AREA     1
#define TX_AREA     2

#define NORMAL_MODE 0x1
#define TAPER_MODE  0x2
#define FULL_MODE   0x3
#define RECHG_MODE  0x4


struct rx1619_chg {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap       *regmap;
	unsigned int tx_on_gpio;
	unsigned int irq_gpio;
	unsigned int power_good_gpio;
	unsigned int power_good_irq;
	unsigned int enable_gpio;
	unsigned int chip_enable;
	int online;
	struct pinctrl *rx_pinctrl;
	struct pinctrl_state *rx_gpio_active;
	struct pinctrl_state *rx_gpio_suspend;
	struct delayed_work    wireless_work;
	struct delayed_work    wireless_int_work;
	struct delayed_work    wpc_det_work;
	struct delayed_work    chg_monitor_work;
	struct delayed_work    chg_detect_work;
	struct delayed_work    reverse_sent_state_work;
	struct delayed_work    reverse_chg_state_work;
	struct delayed_work    reverse_dping_state_work;
	struct delayed_work	dc_check_work;
	struct mutex    wireless_chg_lock;
	struct mutex    wireless_chg_int_lock;
	struct mutex	sysfs_op_lock;

	struct power_supply    *wip_psy;
	struct power_supply    *dc_psy;
	struct power_supply_desc    wip_psy_d;
	struct power_supply    *wireless_psy;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *ln_psy;
	struct alarm	reverse_dping_alarm;
	struct alarm	reverse_chg_alarm;
	int epp;
	int auth;
	int op_mode;
	int dcin_present;
	u8 epp_max_power;
	int target_vol;
	int target_curr;
	int is_vin_limit;
	int epp_exchange;
	int count_5v;
	int count_9v;
	int count_10v;
	int count_15v;
	int status;
	int last_vin;
	int last_icl;
	int exchange;
	int last_qc3_icl;
	int is_reverse_chg;
	int power_good_flag;
	u8 fod_mode;
	int ss;
	int is_car_tx;
	int is_compatible_hwid;
	int is_f1_tx;
	u8 epp_tx_id_h;
	u8 epp_tx_id_l;
};


extern char *saved_command_line;
static int get_board_version(void)
{
	char boot[6] = {'\0'};
	int ret = 0;

	char *match = (char *) strnstr(saved_command_line,
			"androidboot.hwversion=",
			strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.hwversion=")),
			sizeof(boot) - 1);
		printk("%s: hwversion is %s\n", __func__, boot);
		if (!strncmp(boot, "7.9.2", strlen("7.9.2")))
			ret = 1;
	}

	return ret;
}

static int rx1619_read(struct rx1619_chg *chip, u8 *val, u16 addr)
{
	unsigned int temp;
	int rc;

	rc = regmap_read(chip->regmap, addr, &temp);
	if (rc >= 0) {
		*val = (u8)temp;

	}

	return rc;
}


static int rx1619_write(struct rx1619_chg *chip, u8 val, u16 addr)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, addr, val);
	if (rc >= 0)
	{

	}

	return rc;
}



void rx1619_set_fod_param(struct rx1619_chg *chip, u8 mode)
{
	rx1619_write(chip, 0x85, REG_RX_SENT_CMD);

	if (mode == 1) {
		rx1619_write(chip, 0x1, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[0], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[1], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x1;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[0], fod_param[1]);
	} else if (mode == 2) {
		rx1619_write(chip, 0x2, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[2], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[3], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x2;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[2], fod_param[3]);
	} else if (mode == 3) {
		rx1619_write(chip, 0x3, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[4], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[5], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x3;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[4], fod_param[5]);
	} else if (mode == 4) {
		rx1619_write(chip, 0x4, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[6], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[7], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x4;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[6], fod_param[7]);
	}

	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
	dev_info(chip->dev, "[%s] mode = 0x%x \n", __func__, mode);
}

void rx1619_set_adap_vol(struct rx1619_chg *chip, u16 mv)
{
	dev_info(chip->dev, "set adapter vol to %d\n", mv);
	rx1619_write(chip, PRIVATE_FAST_CHG_CMD, REG_RX_SENT_CMD);
	rx1619_write(chip, mv & 0xff, REG_RX_SENT_DATA1);
	rx1619_write(chip, (mv >> 8) & 0xff, REG_RX_SENT_DATA2);
	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
}

void rx1619_request_uuid(struct rx1619_chg *chip, int is_epp)
{
	rx1619_write(chip, UUID_CMD, REG_RX_SENT_CMD);

	if (is_epp) {
		rx1619_write(chip, 0x4C, REG_RX_SENT_DATA1);
		chip->is_compatible_hwid = 0;
	} else {
		rx1619_write(chip, 0x3F, REG_RX_SENT_DATA1);
		chip->is_compatible_hwid = 1;
	}

	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
}

static int rx_get_property_names(struct rx1619_chg *chip)
{
	chip->batt_psy = power_supply_get_by_name("battery");
	if (!chip->batt_psy) {
		dev_err(chip->dev, "no batt_psy,return\n");
		return -EINVAL;
	}

	chip->dc_psy = power_supply_get_by_name("dc");
	if (!chip->dc_psy) {
		dev_err(chip->dev, "no dc_psy,return\n");
		return -EINVAL;
	}

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "no wireless_psy,return\n");
		return -EINVAL;
	}
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		dev_err(chip->dev, "[idt] no usb_psy,return\n");
		return -EINVAL;
	}

	return 0;
}
/*
reverse charging function
*/
unsigned int rx1619_start_tx_function(struct rx1619_chg *chip)
{
	u16 ret = 0;

	ret = rx1619_write(chip, 0x20, 0x000d);
	dev_info(chip->dev, "[%s] ret = %d\n", __func__, ret);

	return ret;
}

unsigned int rx1619_is_tx_mode(struct rx1619_chg *chip)
{
	u8 data = 0;

	rx1619_read(chip, &data, 0x000d);
	dev_info(chip->dev, "[%s] data = %d\n", __func__, data);
	if (data == 0x20)
		return 1;
	else
		return 0;
}

/*
0x00 close TX mode
0x11 none otg mode
0x12 otg mode
*/
/*
unsigned char rx1619_set_tx_mode(struct rx1619_chg *chip, u8 mode)
{
	u8 ret = 0;

	ret = rx1619_write(chip, mode, 0x0000);
	dev_info(chip->dev, "[%s] mode = %d\n", __func__, mode);

	return ret;
}
*/

/*
unsigned int rx1619_get_tx_iout(struct rx1619_chg *chip)
{
	u8  data = 0;
	u32 tx_iout = 0;

	//Iout = 2.5(A)*value/256
	rx1619_read(chip, &data, 0x0008);//0x0008
	tx_iout = 2500*data/256;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, tx_iout=%d \n", __func__, data,tx_iout);

	return tx_iout;
}


unsigned int rx1619_get_tx_vout(struct rx1619_chg *chip)
{
	u8  data = 0;
	u32 tx_vout = 0;

	//Vout = 22.5(V)*value/256
	rx1619_read(chip, &data, 0x0009);//0x0009
	tx_vout = 22500*data/256;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, tx_vout=%d \n", __func__, data,tx_vout);

	return tx_vout;
}
*/

#define TX_NORMAL	0x00
#define TX_OVP		0x01
#define TX_OCP		0x02
#define TX_UV		0x03
#define TX_OTP		0x04
#define TX_FOD		0x05
#define TX_LOW		0x06

unsigned char rx1619_get_tx_status(struct rx1619_chg *chip)
{
	u8  data = 0;

	rx1619_read(chip, &data, 0x000a);
	dev_err(chip->dev, "[%s] data=%d\n", __func__, data);

	return data;
}

/*
init 0x00
ping phase 0x1
power transfer 0x02
power limit 0x03
*/
#define PING         0x1
#define TRANSFER     0x2
#define POWER_LIM    0x3

unsigned char rx1619_get_tx_phase(struct rx1619_chg *chip)
{
	u8  data = 0;

	rx1619_read(chip, &data, 0x000b);
	dev_info(chip->dev, "[%s] data=%d\n", __func__, data);

	return data;
}

unsigned int rx1619_get_rx_vrect(struct rx1619_chg *chip)
{
	u16 vrect = 0;
	u8  data = 0;

	rx1619_read(chip, &data, 0x000A);
	vrect = (data*27500) >> 8;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, Vrect=%d mV \n",
				__func__, data, vrect);

	return vrect;
}

unsigned int rx1619_get_rx_vout(struct rx1619_chg *chip)
{
	u16 vout = 0;
	u8  data = 0;

	rx1619_read(chip, &data, 0x0009);
	vout = (int)(data*22500) >> 8;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, Vout=%d mV \n",
				__func__, data, vout);

	return vout;
}

unsigned int rx1619_get_rx_iout(struct rx1619_chg *chip)
{
	u16 iout = 0;
	u8  data = 0;

	rx1619_read(chip, &data, 0x000B);
	iout = (int)(data*2500) >> 8;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, iout=%d mA \n",
				__func__, data, iout);

	return iout;
}


int rx1619_set_vout(struct rx1619_chg *chip, int volt)
{
	u8 value_h,value_l,ret;
	u16 vout_set,vrect_set;

	if ((volt < 4000) && (volt > 21000))
	{
		volt = 6000;
	}

	volt += g_Delta;
	dev_info(chip->dev, "[rx1619] [%s] volt = %d, g_Delta=%d \n",
				__func__, volt, g_Delta);

	vout_set = (u16)((volt*3352) >> 16);
	vrect_set = (u16)(((volt+200)*2438) >> 16);
/*
	dev_info(chip->dev, "[rx1619] [%s] vout_set = 0x%x, vrect_set=0x%x \n",
				__func__, vout_set, vrect_set);
*/

	value_h = (u8)(vout_set >> 8);
	value_l = (u8)(vout_set & 0xFF);
	ret = rx1619_write(chip, value_h, 0x0001);
	ret = rx1619_write(chip, value_l, 0x0002);
/*
	dev_info(chip->dev, "[rx1619] [%s] vout value_h = 0x%x, value_l=0x%x \n",
				__func__, value_h, value_l);
*/

	value_h = (u8)(vrect_set >> 8);
	value_l = (u8)(vrect_set & 0xFF);
	ret = rx1619_write(chip, value_h, 0x0003);
	ret = rx1619_write(chip, value_l, 0x0004);
/*
	dev_info(chip->dev, "[rx1619] [%s] vrect value_h = 0x%x, value_l=0x%x \n",
				__func__, value_h, value_l);
*/

	ret = rx1619_write(chip, 0x80, 0x0000);
	ret = rx1619_write(chip, 0x55, 0x000C);

	return ret;
}


bool rx1619_is_vout_on(struct rx1619_chg *chip)
{
	bool vout_status = false;
	unsigned int  voltage = 0;

	voltage = rx1619_get_rx_vout(chip);

	dev_err(chip->dev, "[rx1619] [%s] Vout = %d \n", __func__, voltage);

	if ((voltage > MIN_VOUT) && (voltage < MAX_VOUT))
	{
		vout_status = true;
	}
	else
	{
		vout_status = false;
	}

	return vout_status;
}


static void determine_initial_status(struct rx1619_chg *chip)
{
	bool vout_on = false;

	vout_on = rx1619_is_vout_on(chip);
	if (vout_on) {
		g_rx1619_restart_flag = true;

	}

	dev_err(chip->dev, "[%s] initial vout_on = %d \n",
				__func__, vout_on);
}

bool rx1619_restore_chip_defaults(struct rx1619_chg *chip)
{
	u8 data[4];
	u8 check_data[4];

	dev_info(chip->dev, "[%s] enter \n", __func__);

/***************read 0x0074~0x0077 addr*************/
	/************prepare_for_mtp_read************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);
	/************prepare_for_mtp_read************/

	usleep_range(2000, 2500);
	rx1619_read(chip, &data[0], 0x0074);
	usleep_range(2000, 2500);
	rx1619_read(chip, &data[1], 0x0075);
	usleep_range(2000, 2500);
	rx1619_read(chip, &data[2], 0x0076);
	usleep_range(2000, 2500);
	rx1619_read(chip, &data[3], 0x0077);
	usleep_range(2000, 2500);

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	dev_info(chip->dev, "[%s]read data: 0x%x, 0x%x, 0x%x, 0x%x\n",
			__func__, data[0], data[1], data[2], data[3]);
/***************read 0x0074~0x0077 addr*************/
	msleep(50);
/***************write 0x0074~0x0077 addr*************/
	/************prepare_for_mtp_write************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);
	/************prepare_for_mtp_write************/
	msleep(10);
	/************enable write************/
	rx1619_write(chip, 0x80, 0x0010);
	rx1619_write(chip, 0x01, 0x0011);
	rx1619_write(chip, 0x01, 0x0017);
	rx1619_write(chip, 0x20, 0x1000);
	rx1619_write(chip, 0x5a, 0x001a);
	/************enable write************/

	/************write data************/
	usleep_range(2000, 2500);
	rx1619_write(chip, data[0], 0x0012);
	usleep_range(2000, 2500);
	rx1619_write(chip, chip_default_data[0], 0x0012);
	usleep_range(2000, 2500);
	rx1619_write(chip, chip_default_data[1], 0x0012);
	usleep_range(2000, 2500);
	rx1619_write(chip, data[3], 0x0012);
	/************write data************/

	/************end************/
	rx1619_write(chip, 0x00, 0x001a);
	rx1619_write(chip, 0x00, 0x0017);
	rx1619_write(chip, 0x00, 0x1000);
	/************end************/

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	dev_info(chip->dev, "[%s] write data: 0x%x, 0x%x, 0x%x, 0x%x \n",
			__func__, data[0], chip_default_data[0], chip_default_data[1], data[3]);
/***************write 0x0074~0x0077 addr*************/
	msleep(50);
/***************read 0x0074~0x0077 addr for check*************/
	/************prepare_for_mtp_read************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);
	/************prepare_for_mtp_read************/

	/************enable read************/
	rx1619_write(chip, 0x80, 0x0010);
	rx1619_write(chip, 0x01, 0x0011);
	rx1619_write(chip, 0x01, 0x0017);
	rx1619_write(chip, 0x02, 0x0018);
	rx1619_write(chip, 0x00, 0x0018);
	/************enable read************/

	usleep_range(2000, 2500);
	rx1619_read(chip, &check_data[0], 0x0013);
	usleep_range(2000, 2500);
	rx1619_read(chip, &check_data[1], 0x0014);
	usleep_range(2000, 2500);
	rx1619_read(chip, &check_data[2], 0x0015);
	usleep_range(2000, 2500);
	rx1619_read(chip, &check_data[3], 0x0016);
	usleep_range(2000, 2500);

	rx1619_write(chip, 0x00, 0x0017);

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	dev_info(g_chip->dev, "[rx1619] [%s] check data: 0x%x, 0x%x, 0x%x, 0x%x \n",
			__func__, check_data[0], check_data[1], check_data[2], check_data[3]);
/***************read 0x0074~0x0077 addr for check*************/

	if ((check_data[0] == data[0]) && (check_data[3] == data[3]) && (check_data[1] == chip_default_data[0])
		&& (check_data[2] == chip_default_data[1])) {
		return true;
	} else {
		return false;
	}
}



static bool rx1619_check_firmware_version(struct rx1619_chg *chip)
{
	static u16 addr;
	u8  addr_h, addr_l;
	int i = 0;
	u8 read_buf[20];
	char *fw_data = NULL;

	dev_err(chip->dev, "[rx1619] [%s] enter \n", __func__);

	if (g_download_area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
		fw_data = fw_data_boot;
	} else if (g_download_area == RX_AREA) {
		addr = ADDR_RX;
		g_fw_data_lenth = sizeof(fw_data_rx);
		fw_data = fw_data_rx;
	} else if (g_download_area == TX_AREA) {

		addr = ADDR_TX;
		g_fw_data_lenth = sizeof(fw_data_tx);
		fw_data = fw_data_tx;
	}

	/************prepare_for_mtp_read************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);

	rx1619_write(chip, 0x01, 0x0017);
	/************prepare_for_mtp_read************/

	msleep(10);

	/*************read 12 byte before fw_data_end addr************/
	addr += (g_fw_data_lenth/4);

	for (i = 0; i < 12; i += 4) {
		/************write_mtp_addr************/
		addr_h = (u8)(addr >> 8);
		addr_l = (u8)(addr & 0xff);
		rx1619_write(chip, addr_h, 0x0010);
		rx1619_write(chip, addr_l, 0x0011);
		/************write_mtp_addr************/

		addr--;

		/************read pause************/
		rx1619_write(chip, 0x02, 0x0018);
		rx1619_write(chip, 0x00, 0x0018);
		/************read pause************/

		/************read data************/
		rx1619_read(chip, &read_buf[i+3], 0x0013);
		rx1619_read(chip, &read_buf[i+2], 0x0014);
		rx1619_read(chip, &read_buf[i+1], 0x0015);
		rx1619_read(chip, &read_buf[i+0], 0x0016);
		/************read data************/
	}
	/*************read 12 byte before fw_data_end addr************/

	/***********end************/
	rx1619_write(chip, 0x00, 0x0017);
	/***********end************/

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	dev_info(chip->dev, "confirm data =0x%x, 0x%x, 0x%x, 0x%x\n",
				read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

	dev_info(chip->dev, "chip_data version=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				read_buf[4], read_buf[5], read_buf[6], read_buf[7],
				read_buf[8], read_buf[9], read_buf[10], read_buf[11]);

	dev_info(chip->dev, "fw_data version=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
				fw_data[g_fw_data_lenth-4], fw_data[g_fw_data_lenth-3],
				fw_data[g_fw_data_lenth-2], fw_data[g_fw_data_lenth-1],
				fw_data[g_fw_data_lenth-8], fw_data[g_fw_data_lenth-7],
				fw_data[g_fw_data_lenth-6], fw_data[g_fw_data_lenth-5]);

	if (g_download_area == RX_AREA) {
		g_fw_rx_id = (~read_buf[7]);
		g_hw_id_h = (~read_buf[10]);
		g_hw_id_l = (~read_buf[11]);
	} else if (g_download_area == TX_AREA) {
		g_fw_tx_id = (~read_buf[7]);
	}

	if ((read_buf[7] == fw_data[g_fw_data_lenth-1]) && (read_buf[0] == 0x66)) {
		dev_info(chip->dev, "g_fw_data_lenth=%d, fw version = 0x%x\n",
					g_fw_data_lenth, (~read_buf[7])&0xff);
		return true;
	}

	return false;
}


static bool rx1619_update_firmware_confirm(struct rx1619_chg *chip)
{
	u16 addr = 0;
	u8  addr_h, addr_l;
	u8 data[4] = {0, 0, 0, 0};

	/************prepare_for_mtp_write************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);

	rx1619_write(chip, 0x01, 0x0017);
	rx1619_write(chip, 0x20, 0x1000);
	/************prepare_for_mtp_write************/

	msleep(20);

	/************write_mtp_addr************/
	if (g_download_area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
	} else if (g_download_area == RX_AREA) {
		addr = ADDR_RX;
		g_fw_data_lenth = sizeof(fw_data_rx);
	} else if (g_download_area == TX_AREA) {

		addr = ADDR_TX;
		g_fw_data_lenth = sizeof(fw_data_tx);
	}

	addr += (g_fw_data_lenth/4);

	addr_h = (u8)(addr >> 8);
	addr_l = (u8)(addr & 0xff);
	rx1619_write(chip, addr_h, 0x0010);
	rx1619_write(chip, addr_l, 0x0011);
	/************write_mtp_addr************/

	/************enable write************/
	rx1619_write(chip, 0x5a, 0x001a);
	/************enable write************/

	/************write data************/
	usleep_range(2000, 2100);
	rx1619_write(chip, 0x00, 0x0012);
	usleep_range(2000, 2100);
	rx1619_write(chip, 0x00, 0x0012);
	usleep_range(2000, 2100);
	rx1619_write(chip, 0x00, 0x0012);
	usleep_range(2000, 2100);
	rx1619_write(chip, g_update_fw_status, 0x0012);
	/************write data************/

	/************end************/
	rx1619_write(chip, 0x00, 0x001a);
	rx1619_write(chip, 0x00, 0x0017);
	/************end************/

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	msleep(100);

	/************prepare_for_mtp_read************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);

	rx1619_write(chip, 0x01, 0x0017);
	/************prepare_for_mtp_read************/

	msleep(10);

	/************write_mtp_addr************/
	addr_h = (u8)(addr >> 8);
	addr_l = (u8)(addr & 0xff);
	rx1619_write(chip, addr_h, 0x0010);
	rx1619_write(chip, addr_l, 0x0011);
	/************write_mtp_addr************/

	/************read pause************/
	rx1619_write(chip, 0x02, 0x0018);
	rx1619_write(chip, 0x00, 0x0018);
	/************read pause************/

	/************read data************/
	rx1619_read(chip, &data[3], 0x0013);
	rx1619_read(chip, &data[2], 0x0014);
	rx1619_read(chip, &data[1], 0x0015);
	rx1619_read(chip, &data[0], 0x0016);
	/************read data************/

	/***********end************/
	rx1619_write(chip, 0x00, 0x0017);
	/***********end************/

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	dev_err(chip->dev, "addr=%d  confirm data = 0x%x, 0x%x, 0x%x, 0x%x\n",
				addr, data[0], data[1], data[2], data[3]);

	if (data[0] == 0x66) {
		dev_err(chip->dev, "0x66 Update fw success!!!\n");
		return true;
	} else if (data[0] == 0x99) {
		dev_err(chip->dev, "0x99 Update fw fail, please tyr again!!! \n");
		return false;
	} else {
		dev_err(chip->dev, "unknow code, Update fw fail, please tyr again!!! \n");
		return false;
	}
}


static bool rx1619_download_firmware(struct rx1619_chg *chip)
{
	u16 addr = 0;
	u8  addr_h, addr_l;
	int i = 0;
	char *fw_data = NULL;

	dev_err(chip->dev, "[rx1619] [%s] enter \n", __func__);

	/************prepare_for_mtp_write************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);

	rx1619_write(chip, 0x01, 0x0017);
	rx1619_write(chip, 0x20, 0x1000);
	/************prepare_for_mtp_write************/

	msleep(20);

	/************write_mtp_addr************/
	if (g_download_area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
		fw_data = fw_data_boot;
	} else if (g_download_area == RX_AREA) {
		addr = ADDR_RX;
		g_fw_data_lenth = sizeof(fw_data_rx);
		fw_data = fw_data_rx;
	} else if (g_download_area == TX_AREA) {

		addr = ADDR_TX;
		g_fw_data_lenth = sizeof(fw_data_tx);
		fw_data = fw_data_tx;
	}
	addr_h = (u8)(addr >> 8);
	addr_l = (u8)(addr & 0xff);
	rx1619_write(chip, addr_h, 0x0010);
	rx1619_write(chip, addr_l, 0x0011);
	/************write_mtp_addr************/

	/************enable write************/
	rx1619_write(chip, 0x5a, 0x001a);
	/************enable write************/

	/************write data************/
	for (i = 0; i < g_fw_data_lenth; i += 4) {
		usleep_range(1000, 1100);
		rx1619_write(chip, fw_data[i+3], 0x0012);
		usleep_range(1000, 1100);
		rx1619_write(chip, fw_data[i+2], 0x0012);
		usleep_range(1000, 1100);
		rx1619_write(chip, fw_data[i+1], 0x0012);
		usleep_range(1000, 1100);
		rx1619_write(chip, fw_data[i+0], 0x0012);
	}
	/************write data************/

	/************end************/
	rx1619_write(chip, 0x00, 0x001a);
	rx1619_write(chip, 0x00, 0x0017);
	/************end************/

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	dev_err(chip->dev, "[rx1619] [%s] exit \n", __func__);

	return 0;
}


static int rx1619_check_firmware(struct rx1619_chg *chip)
{
	u16 addr = 0;
	u8  addr_h, addr_l;
	int i = 0;
	int j = 0;
	u8 read_buf[4] = {0, 0, 0, 0};
	char *fw_data = NULL;

	success_count = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter \n", __func__);

	if (g_download_area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
		fw_data = fw_data_boot;
	} else if (g_download_area == RX_AREA) {
		addr = ADDR_RX;
		g_fw_data_lenth = sizeof(fw_data_rx);
		fw_data = fw_data_rx;
	} else if (g_download_area == TX_AREA) {

		addr = ADDR_TX;
		g_fw_data_lenth = sizeof(fw_data_tx);
		fw_data = fw_data_tx;
	}

	/************prepare_for_mtp_read************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);

	rx1619_write(chip, 0x01, 0x0017);
	/************prepare_for_mtp_read************/

	msleep(10);

	for (i = 0; i < g_fw_data_lenth; i += 4) {
		/************write_mtp_addr************/
		addr_h = (u8)(addr >> 8);
		addr_l = (u8)(addr & 0xff);
		rx1619_write(chip, addr_h, 0x0010);
		rx1619_write(chip, addr_l, 0x0011);
		/************write_mtp_addr************/

		addr++;

		/************read pause************/
		rx1619_write(chip, 0x02, 0x0018);
		rx1619_write(chip, 0x00, 0x0018);
		/************read pause************/

		/************read data************/
		rx1619_read(chip, &read_buf[3], 0x0013);
		rx1619_read(chip, &read_buf[2], 0x0014);
		rx1619_read(chip, &read_buf[1], 0x0015);
		rx1619_read(chip, &read_buf[0], 0x0016);
		/************read data************/

		if ((read_buf[0] == fw_data[i+0]) &&
			(read_buf[1] == fw_data[i+1]) &&
			(read_buf[2] == fw_data[i+2]) &&
			(read_buf[3] == fw_data[i+3])) {
			success_count++;
		} else {
			j++;
			if (j >= 50) {//if error adrr >= 50,new IC
				goto CCC;
			}
		}
	}

CCC:
	/***********end************/
	rx1619_write(chip, 0x00, 0x0017);
	/***********end************/

	/************exit dtm************/
	rx1619_write(chip, 0x00, 0x2018);
	rx1619_write(chip, 0x00, 0x2019);
	rx1619_write(chip, 0x00, 0x0001);
	rx1619_write(chip, 0x00, 0x0003);
	rx1619_write(chip, 0x55, 0x2017);
	/************exit dtm************/

	dev_err(chip->dev, "error_conut= %d, success_count=%d\n", j, success_count);

	dev_info(chip->dev, "(sizeof(fw_data_boot)) = %ld (sizeof(fw_data_rx)) = %ld (sizeof(fw_data_tx)) = %ld \n",
		(sizeof(fw_data_boot)), (sizeof(fw_data_rx)), (sizeof(fw_data_tx)));

	return success_count;
}


bool rx1619_onekey_download_firmware(struct rx1619_chg *chip)
{
	int ret = 0;
	bool update_status = false;

	dev_err(chip->dev, "[rx1619] [%s] enter \n", __func__);

	/************download boot area************/
	g_download_area = BOOT_AREA;

	rx1619_check_firmware_version(chip);
	msleep(20);
	rx1619_download_firmware(chip);
	msleep(20);
	ret = rx1619_check_firmware(chip);
	dev_err(chip->dev, "boot--success_cout=%d, g_fw_data_lenth=%d\n", ret, g_fw_data_lenth);
	if (g_fw_data_lenth == (ret*4)) {
		dev_info(chip->dev, "download boot fw success\n");
		g_update_fw_status = 0x66;
		update_status = rx1619_update_firmware_confirm(chip);
		if (update_status)
			dev_info(chip->dev, "download boot confirm OK! \n");
	} else {
		g_update_fw_status = 0x99;
		rx1619_update_firmware_confirm(chip);
		return false;
	}
	/************download boot area************/

	msleep(50);

	/************download tx area************/
	g_download_area = RX_AREA;

	rx1619_check_firmware_version(chip);
	msleep(20);
	rx1619_download_firmware(chip);
	msleep(20);
	ret = rx1619_check_firmware(chip);
	dev_err(chip->dev, "rx--success_cout=%d, g_fw_data_lenth=%d\n", ret, g_fw_data_lenth);
	if (g_fw_data_lenth == (ret*4)) {
		dev_info(chip->dev, "download rx fw success\n");
		g_update_fw_status = 0x66;
		update_status = rx1619_update_firmware_confirm(chip);
		if (update_status)
			dev_info(chip->dev, "download rx confirm OK! \n");
	} else {
		g_update_fw_status = 0x99;
		rx1619_update_firmware_confirm(chip);
		return false;
	}
	/************download tx area************/

	msleep(50);

	/************download rx area************/
	g_download_area = TX_AREA;

	rx1619_check_firmware_version(chip);
	msleep(20);
	rx1619_download_firmware(chip);
	msleep(20);
	ret = rx1619_check_firmware(chip);
	dev_err(chip->dev, "tx--success_cout=%d, g_fw_data_lenth=%d\n", ret, g_fw_data_lenth);
	if (g_fw_data_lenth == (ret*4)) {
		dev_info(chip->dev, "download tx fw success\n");
		g_update_fw_status = 0x66;
		update_status = rx1619_update_firmware_confirm(chip);
		if (update_status)
			dev_info(chip->dev, "download tx confirm OK! \n");
	} else {
		g_update_fw_status = 0x99;
		rx1619_update_firmware_confirm(chip);
		return false;
	}
	/************download rx area************/
	/************restore chip defaults************/
	update_status = rx1619_restore_chip_defaults(chip);
	if (update_status) {
		dev_info(chip->dev, "restore chip defaults success! \n");
	} else {
		dev_info(chip->dev, "restore chip defaults fail! \n");
		return false;
	}
	/************restore chip defaults************/

	return true;
}

void rx1619_dump_reg(void)
{
	u8 data[32] = {0};

	rx1619_read(g_chip, &data[0], 0x0000);
	rx1619_read(g_chip, &data[1], 0x0001);
	rx1619_read(g_chip, &data[2], 0x0002);
	rx1619_read(g_chip, &data[3], 0x0003);
	rx1619_read(g_chip, &data[4], 0x0004);
	rx1619_read(g_chip, &data[5], 0x0005);
	rx1619_read(g_chip, &data[6], 0x0008);
	rx1619_read(g_chip, &data[7], 0x0009);
	rx1619_read(g_chip, &data[8], 0x000A);
	rx1619_read(g_chip, &data[9], 0x000B);
	rx1619_read(g_chip, &data[10], 0x000C);
	rx1619_read(g_chip, &data[11], 0x0020);
	rx1619_read(g_chip, &data[12], 0x0021);
	rx1619_read(g_chip, &data[13], 0x0022);
	rx1619_read(g_chip, &data[14], 0x0023);
	rx1619_read(g_chip, &data[15], 0x0024);

	dev_info(g_chip->dev, "reave--[rx1619] [%s] REG:0x0000=0x%x\n",
					__func__, data[0]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0001=0x%x\n",
					__func__, data[1]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0002=0x%x\n",
					__func__, data[2]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0003=0x%x\n",
					__func__, data[3]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0004=0x%x\n",
					__func__, data[4]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0005=0x%x\n",
					__func__, data[5]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0008=0x%x\n",
					__func__, data[6]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0009=0x%x\n",
					__func__, data[7]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x000A=0x%x\n",
					__func__, data[8]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x000B=0x%x\n",
					__func__, data[9]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x000C=0x%x\n",
					__func__, data[10]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0020=0x%x\n",
					__func__, data[11]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0021=0x%x\n",
					__func__, data[12]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0022=0x%x\n",
					__func__, data[13]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0023=0x%x\n",
					__func__, data[14]);
	dev_info(g_chip->dev, "[rx1619] [%s] REG:0x0024=0x%x\n",
					__func__, data[15]);
	rx1619_write(g_chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
}

void rx1619_set_pmi_icl(struct rx1619_chg *chip, int mA)
{
	union power_supply_propval val = {0, };

	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			dev_err(chip->dev, "[rx1619] [%s] no dc_psy,return\n",
						 __func__);
			return;
		}
	}
	val.intval = mA;
	power_supply_set_property(chip->dc_psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	dev_info(chip->dev, "[rx1619] [%s] [rx1619] set icl: %d\n",
						 __func__, val.intval);
}

void rx1619_get_pmi_icl(struct rx1619_chg *chip)
{
	union power_supply_propval val = {0, };

	if (!chip->dc_psy) {
		chip->dc_psy = power_supply_get_by_name("dc");
		if (!chip->dc_psy) {
			dev_err(chip->dev, "[rx1619] [%s] no dc_psy,return\n",
						__func__);
			return;
		}
	}
	power_supply_get_property(chip->dc_psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	dev_info(chip->dev, "[rx1619] [%s] [rx1619] get icl: %d\n",
					__func__, val.intval);
	g_current_now = val.intval;
}


void set_usb_type_current(struct rx1619_chg *chip, u8 data)
{
	int i = 0;
	int uA = 0;

	union power_supply_propval val = {0, };

	dev_info(chip->dev, "[%s] data=0x%x \n", __func__, data);

	switch (data) {
	case 0:
		if ((chip->auth == 0) && (chip->epp == 0)) {
			rx1619_set_pmi_icl(chip, DC_OTHER_CURRENT);
			dev_info(chip->dev, "[rx1619] [%s] bpp and no id---800mA \n", __func__);
		} else {
			rx1619_set_pmi_icl(chip, DC_LOW_CURRENT);
			dev_info(chip->dev, "[rx1619] [%s] bpp and id ok---200mA \n", __func__);
		}
		break;

	case 1:
		rx1619_set_pmi_icl(chip, DC_SDP_CURRENT);
		chip->target_vol = ADAPTER_DEFAULT_VOL;
		chip->target_curr = DC_SDP_CURRENT;
		break;

	case 2:
	case 3:
		rx1619_set_pmi_icl(chip, DC_BPP_AUTH_FAIL_CURRENT);
		chip->target_vol = ADAPTER_DEFAULT_VOL;
		chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;
		break;

	case 5:
		for (i = 0; i <= 8; i++) {
			uA = (DC_LOW_CURRENT + 100000*i);
			rx1619_set_pmi_icl(chip, uA);
			msleep(100);
		}
		chip->target_vol = ADAPTER_BPP_QC2_VOL;
		chip->target_curr = uA;
		break;

	case 6:
	case 7:
		if (chip->epp) {
			for (i = 0; i <= 8; i++) {
				uA = (DC_LOW_CURRENT + 200000*i);
				rx1619_set_pmi_icl(chip, uA);
				msleep(100);
			}
			chip->target_vol = ADAPTER_EPP_QC3_VOL;
			chip->target_curr = uA;
		} else {
			for (i = 0; i <= 9; i++) {
				uA = (DC_LOW_CURRENT + 100000*i);
				rx1619_set_pmi_icl(chip, uA);
				msleep(100);
			}
			chip->target_vol = ADAPTER_BPP_LIMIT_VOL;
			chip->target_curr = uA;
		}
		chip->last_qc3_icl = chip->target_curr;
		break;

	case 8:
		break;

	case 9:
	case 10:
	case 11:
		if (chip->op_mode != LN8282_OPMODE_SWITCHING) {
			dev_info(chip->dev, "[20W]not switch mode, don't rise voltage \n");
			break;
		}
/*
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			dev_err(chip->dev, "Battery supply not found\n");
			vol_now = 15000;
		} else {
			power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
			vol_now = val.intval;
			vol_now = (vol_now*4) / 1000 + 200; //delta
		}
*/
		chip->target_vol = ADAPTER_EPP_MI_VOL;
		rx1619_set_pmi_icl(chip, 1000000);
		msleep(100);
		if (chip->epp && chip->auth && chip->wireless_psy) {
			val.intval = 1;
			power_supply_set_property(chip->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			msleep(200);
		}
		for (i = 0; i <= 11; i++) {
			uA = (DC_TURBO_CURRENT + 100000 * i);
			rx1619_set_pmi_icl(chip, uA);
			msleep(100);
		}
		chip->target_curr = uA;
		chip->last_icl = chip->target_curr;
		dev_info(chip->dev, "[%s] 27W adapter \n", __func__);
		break;

	case 12:
		if (chip->op_mode != LN8282_OPMODE_SWITCHING) {
			dev_info(chip->dev, "[30W]not switch mode, don't rise voltage \n");
			break;
		}
/*
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			dev_err(chip->dev, "Battery supply not found\n");
			vol_now = 15000;
		} else {
			power_supply_get_property(chip->batt_psy,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
			vol_now = val.intval;
			vol_now = (vol_now*4) / 1000 + 200; //delta
		}
*/
		chip->target_vol = ADAPTER_EPP_MI_VOL;
		rx1619_set_pmi_icl(chip, 1000000);
		msleep(100);
		if (chip->epp && chip->auth && chip->wireless_psy) {
			val.intval = 1;
			power_supply_set_property(chip->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			msleep(200);
		}
		for (i = 0; i <= 10; i++) {
			uA = (DC_TURBO_CURRENT + 200000 * i);
			rx1619_set_pmi_icl(chip, uA);
			msleep(100);
		}
		chip->target_curr = uA;
		chip->last_icl = chip->target_curr;
		dev_info(chip->dev, "[%s] 40W adapter \n", __func__);
		break;

	default:
		dev_info(chip->dev, "[%s] other Usb_type\n", __func__);
		break;
	}
}

static void rx_charging_info(struct rx1619_chg *chip)
{
	int vout, iout, vrect;
	if (!chip)
		return;

	vout = rx1619_get_rx_vout(chip);
	iout = rx1619_get_rx_iout(chip);
	vrect = rx1619_get_rx_vrect(chip);

	dev_info(chip->dev, "%s:Vout:%dmV, Iout:%dmA, Vrect:%dmV\n",
					 __func__, vout, iout, vrect);
}

void get_usb_type_current(struct rx1619_chg *chip, u8 data)
{
	dev_info(chip->dev, "[%s] data=0x%x \n", __func__, data);

	switch (data) {
	case ADAPTER_NONE:
		if ((chip->auth == 0) && (chip->epp == 0)) {
			chip->target_curr = DC_OTHER_CURRENT;
			dev_info(chip->dev, "[rx1619] [%s] bpp and no id---800mA \n", __func__);
		} else {
			chip->target_curr = DC_LOW_CURRENT;
			dev_info(chip->dev, "[rx1619] [%s] bpp and id ok---200mA \n", __func__);
		}
		break;

	case ADAPTER_SDP:
		chip->target_vol = ADAPTER_DEFAULT_VOL;
		chip->target_curr = DC_SDP_CURRENT;
		break;

	case ADAPTER_CDP:
	case ADAPTER_DCP:
		chip->target_vol = ADAPTER_DEFAULT_VOL;
		chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;
		break;

	case ADAPTER_QC2:
		chip->target_vol = ADAPTER_BPP_QC2_VOL;
		chip->target_curr = DC_QC2_CURRENT;
		break;

	case ADAPTER_QC3:
	case ADAPTER_PD:
		if (chip->epp) {
			chip->target_vol = ADAPTER_EPP_QC3_VOL;
			chip->target_curr = DC_QC3_CURRENT;
		} else {
			chip->target_vol = ADAPTER_DEFAULT_VOL;
			chip->target_curr = DC_QC3_BPP_CURRENT;
		}
		break;

	case ADAPTER_AUTH_FAILED:
		break;

	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
		chip->target_vol = ADAPTER_EPP_MI_VOL;
		chip->target_curr = DC_QC3_20W_CURRENT;
		break;

	case ADAPTER_XIAOMI_PD_40W:
		chip->target_vol = ADAPTER_EPP_MI_VOL;
		chip->target_curr = DC_PD_40W_CURRENT;
		break;

	default:
		break;
	}
}
static void rx_set_charging_param(struct rx1619_chg *chip)
{
	union power_supply_propval val = {0, };
	union power_supply_propval wk_val = {0, };
	int soc = 0, health = 0, batt_sts = 0;
	int vol_now = 0, cur_now = 0, dc_level = 0;
	int iout = 0, ret;
	bool vout_change = false;

	get_usb_type_current(chip, g_USB_TYPE);

	if (chip->batt_psy) {
		power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;

		power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		vol_now = val.intval;

		power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		cur_now = val.intval;

		power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &val);
		health = val.intval;

		power_supply_get_property(chip->batt_psy,
			POWER_SUPPLY_PROP_DC_THERMAL_LEVELS, &val);
		dc_level = val.intval;
	}
	dev_info(chip->dev, "soc:%d,vol_now:%d,cur_now:%d,health:%d, bat_status:%d, dc_level:%d\n",
			soc, vol_now, cur_now, health, batt_sts, dc_level);
	/*epp 10W*/
	if ((g_USB_TYPE >= 6 && g_USB_TYPE <= 7) && (chip->epp)) {
		dev_info(chip->dev, "standard epp logic\n");
		if (soc >= 97)
			chip->target_curr = DC_BPP_CURRENT;
		if (soc == 100)
			chip->target_curr = DC_SDP_CURRENT;
		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;

		if (chip->target_curr != chip->last_qc3_icl) {
			dev_info(chip->dev, "qc3_epp, set new icl: %d, last_icl: %d\n",
					chip->target_curr, chip->last_qc3_icl);
			chip->last_qc3_icl = chip->target_curr;
			rx1619_set_pmi_icl(chip, chip->target_curr);
			msleep(100);
		}
	}
	/*epp plus*/
	if (g_USB_TYPE >= 9) {
		if (chip->op_mode != LN8282_OPMODE_SWITCHING) {
			dev_info(chip->dev, "not switch mode, don't adjust voltage \n");
			goto out;
		}

		if ((dc_level >= 9) || (soc >= LIMIT_SOC)) {
			dev_info(chip->dev, "set vin 12V for dc_level:%d, soc:%d\n", dc_level, soc);

			chip->is_vin_limit = 1;
		} else
			chip->is_vin_limit = 0;

		iout = rx1619_get_rx_iout(chip);
		/* function start
		 * set adapter vol by iout value, threshold is 500mA
		 * lower than threshold will reduce to 12V, more than
		 * threshold will lift to 15V:
		 * 15V-->12V check 3 times
		 * 12V-->15v check 3 times
		 */
		if (iout < LIMIT_EPP_IOUT && chip->epp_exchange == EXCHANGE_15V) {
			chip->count_10v++;
			chip->count_15v = 0;
		} else if (iout > LIMIT_EPP_IOUT && chip->epp_exchange == EXCHANGE_10V) {
			chip->count_15v++;
			chip->count_10v = 0;
		} else {
			chip->count_10v = 0;
			chip->count_15v = 0;
		}
		if (chip->count_10v > ICL_EXCHANGE_COUNT ||
				(chip->epp_exchange == EXCHANGE_10V && chip->count_15v <= ICL_EXCHANGE_COUNT)) {
			dev_info(chip->dev, "iout less than 500mA ,set vin to 12V\n");

			chip->epp_exchange = EXCHANGE_10V;
		} else if (chip->count_15v > (ICL_EXCHANGE_COUNT))
			chip->epp_exchange = EXCHANGE_15V;
		/* function end */
		switch (chip->status) {
		case NORMAL_MODE:
			if (soc >= 97) {

				dev_info(chip->dev, "set curr to %d\n", chip->target_curr);
			}
			if (soc >= FULL_SOC) {
				chip->status = TAPER_MODE;


				dev_info(chip->dev, "ready to taper_mode ,set vin to %d, curr to %d\n",
						chip->target_vol, chip->target_curr);
			}
			break;
		case TAPER_MODE:



			dev_info(chip->dev, "ready to full_mode ,set vin to %d, curr to %d\n",
						chip->target_vol, chip->target_curr);
			if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
				chip->status = FULL_MODE;
			else if (soc < FULL_SOC - 1)
				chip->status = NORMAL_MODE;
			break;
		case FULL_MODE:
			dev_info(chip->dev, "charge full set Vin 10V\n");
			chip->target_vol = EPP_VOL_THRESHOLD;
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;

			if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
				chip->status = RECHG_MODE;
				chip->target_curr = DC_FULL_CURRENT;
			}
			break;
		case RECHG_MODE:
			dev_info(chip->dev, "recharge mode set icl to 360mA\n");
			if (batt_sts == POWER_SUPPLY_STATUS_FULL)
				chip->status = FULL_MODE;

			chip->target_vol = EPP_VOL_THRESHOLD;
			chip->target_curr = DC_FULL_CURRENT;

			if (chip->wireless_psy) {
				wk_val.intval = 1;
				power_supply_set_property(chip->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
			}
			break;
		default:
			break;
		}
		switch (health) {
		case POWER_SUPPLY_HEALTH_GOOD:
			break;
		case POWER_SUPPLY_HEALTH_COOL:
			break;
		case POWER_SUPPLY_HEALTH_WARM:
			chip->target_vol = min(EPP_VOL_THRESHOLD, chip->target_vol);
			chip->target_curr = min(DC_SDP_CURRENT, chip->target_curr);
			break;
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
			chip->target_vol = min(EPP_VOL_THRESHOLD, chip->target_vol);
			chip->target_curr = min(SCREEN_OFF_FUL_CURRENT, chip->target_curr);
			break;
		case POWER_SUPPLY_HEALTH_COLD:
		case POWER_SUPPLY_HEALTH_HOT:
			chip->target_vol = min(EPP_VOL_THRESHOLD, chip->target_vol);
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;
			break;
		default:
			break;
		}
		if (chip->target_vol > 0 && chip->target_vol != chip->last_vin) {
			ret = rx1619_set_vout(chip, chip->target_vol);
			chip->last_vin = chip->target_vol;
			vout_change = true;

			if (chip->target_vol <= EPP_VOL_THRESHOLD) {
				val.intval = 0;
			} else {
				val.intval = 1;
			}
			if (chip->wireless_psy) {
				power_supply_set_property(chip->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			}
		}
		if ((chip->target_curr > 0 && chip->target_curr != chip->last_icl)
			|| vout_change) {
			chip->last_icl = chip->target_curr;
			rx1619_set_pmi_icl(chip, chip->target_curr);
		}
	}
out:
	dev_info(chip->dev, "status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d\n",
			chip->status, chip->target_vol, chip->target_curr, chip->last_vin, chip->last_icl);
}

static void rx1619_wireless_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg, wireless_work.work);

	schedule_delayed_work(&chip->wireless_work, msecs_to_jiffies(200));

	return;
}

static void rx1619_dc_check_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
		 container_of(work, struct rx1619_chg,
					dc_check_work.work);

	dev_info(chip->dev, "[rx1619] dc present: %d\n", chip->dcin_present);
	if (chip->dcin_present) {
		chip->ss = 1;
		dev_info(chip->dev, "dcin present, quit dc check work\n");
		return;
	} else {
		chip->ss = 0;
		dev_info(chip->dev, "dcin no present, continue dc check work\n");
		schedule_delayed_work(&chip->dc_check_work, msecs_to_jiffies(2500));
	}
	power_supply_changed(chip->wireless_psy);
}

#define CHARGING_PERIOD_S	10
static void rx_monitor_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
		 container_of(work, struct rx1619_chg,
					chg_monitor_work.work);
	rx_charging_info(chip);

	rx_set_charging_param(chip);

	schedule_delayed_work(&chip->chg_monitor_work,
			CHARGING_PERIOD_S * HZ);
}

static void rx_chg_detect_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
		 container_of(work, struct rx1619_chg,
					chg_detect_work.work);
	union power_supply_propval val = {0, };
	union power_supply_propval wk_val = {0, };
	int rc;

	dev_info(chip->dev, "[idt] enter %s\n", __func__);

	rc = rx_get_property_names(chip);
	if (rc < 0)
		return;

	g_chip = chip;
	power_supply_get_property(chip->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (val.intval) {
		dev_info(chip->dev, "usb_online:%d set chip disable\n",
							val.intval);
		rx_set_enable_mode(chip, 0);
		return;
	}

	if (chip->dc_psy) {
		power_supply_get_property(chip->dc_psy,
				POWER_SUPPLY_PROP_ONLINE, &val);
		dev_info(chip->dev, "dc_online %d\n", val.intval);
		if (val.intval && chip->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(chip->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
			schedule_delayed_work(&chip->wireless_int_work,
						msecs_to_jiffies(30));
		}
	}
}

static void reverse_chg_sent_state_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
		 container_of(work, struct rx1619_chg,
					reverse_sent_state_work.work);

	union power_supply_propval val = {0, };

	if (chip->wireless_psy) {
		val.intval = chip->is_reverse_chg;
		power_supply_set_property(chip->wireless_psy,
				POWER_SUPPLY_PROP_REVERSE_CHG_STATE, &val);
		dev_info(chip->dev, "sent tx_mode_uevent\n");
		power_supply_changed(chip->wireless_psy);
	} else
		dev_err(chip->dev, "get wls property error\n");
}

static void reverse_chg_state_set_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
		 container_of(work, struct rx1619_chg,
					reverse_chg_state_work.work);
	int ret;

	dev_info(chip->dev, "no rx found and disable reverse charging\n");
	ret = rx_set_reverse_chg_mode(chip, false);
	chip->is_reverse_chg = 1;
	schedule_delayed_work(&chip->reverse_sent_state_work, 0);

	return;
}

static void reverse_dping_state_set_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
		 container_of(work, struct rx1619_chg,
					reverse_dping_state_work.work);
	int ret;

	dev_info(chip->dev, "tx mode fault and disable reverse charging\n");
	ret = rx_set_reverse_chg_mode(chip, false);
	chip->is_reverse_chg = 2;
	schedule_delayed_work(&chip->reverse_sent_state_work, 0);
	return;
}

/* power good work */
static void rx1619_wpc_det_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg, wpc_det_work.work);
	union power_supply_propval val = {0, };
	int ret = 0;

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "[rx1619] no wireless_psy, return\n");
		return;
	}

	if (gpio_is_valid(chip->power_good_gpio)) {
		ret = gpio_get_value(chip->power_good_gpio);
		if (ret) {
			dev_info(chip->dev, "power_good high, wireless attached\n");
			chip->power_good_flag = 1;
			val.intval = 1;
		} else {
			dev_info(chip->dev, "power_good low, wireless detached\n");
			cancel_delayed_work(&chip->dc_check_work);
			chip->power_good_flag = 0;
			chip->ss = 2;
			val.intval = 0;
			chip->ln_psy = power_supply_get_by_name("lionsemi");
			if (chip->ln_psy)
				power_supply_set_property(chip->ln_psy,
					POWER_SUPPLY_PROP_RESET_DIV_2_MODE, &val);
		}
		power_supply_set_property(chip->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN, &val);
	}
}

#define REVERSE_CHG_CHECK_DELAY_MS 80000
#define REVERSE_DPING_CHECK_DELAY_MS 10000
static void rx1619_wireless_int_work(struct work_struct *work)
{
	int i = 0;
	int uA = 0;
	u8 usb_type;
	u8 rx_rev_data[4] = {0, 0, 0, 0};
	u8 rx_req = 0;
	u8 g_tx_id_h = 0;
	u8 g_tx_id_l = 0;
	u8 g_shaone_data_h = 0;
	u8 g_shaone_data_l = 0;
	u8 g_fc_status = 0;
	u8 g_uuid_data[4] = {0};
	int tx_gpio, ret;
	u8 tx_status, tx_phase;
	u8 err_cmd;
	int rc;
	union power_supply_propval cp_val = {0, };
	int fc_flag = 0;

	struct rx1619_chg *chip = container_of(work, struct rx1619_chg, wireless_int_work.work);

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "[rx1619] no wireless_psy, return\n");
	}

	tx_gpio = rx_get_reverse_chg_mode(chip);
	if (tx_gpio) {
		if (rx1619_is_tx_mode(chip)) {
			rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
			tx_status = rx1619_get_tx_status(chip);
			if (tx_status) {
				ret = rx_set_reverse_chg_mode(chip, false);
				chip->is_reverse_chg = 2;
				schedule_delayed_work(&chip->reverse_sent_state_work, 0);
			}
			tx_phase = rx1619_get_tx_phase(chip);
			switch (tx_phase) {
			case PING:

				alarm_start_relative(&chip->reverse_chg_alarm,
					ms_to_ktime(REVERSE_CHG_CHECK_DELAY_MS));

				rc = alarm_cancel(&chip->reverse_dping_alarm);
				if (rc < 0)
					dev_err(chip->dev, "Couldn't cancel reverse_dping_alarm\n");
				pm_relax(chip->dev);
				break;
			case TRANSFER:

				rc = alarm_cancel(&chip->reverse_chg_alarm);
				if (rc < 0)
					dev_err(chip->dev, "Couldn't cancel reverse_dping_alarm\n");
				pm_stay_awake(chip->dev);
				dev_info(chip->dev, "tx mode power transfer\n");
				break;
			case POWER_LIM:
				dev_info(chip->dev, "tx mode power limit\n");
				break;
			default:
				dev_err(chip->dev, "tx phase invalid\n");
				break;
			}
		}
		return;
	}

	rx1619_read(chip, &rx_req, REG_RX_REV_CMD);
	if (rx_req <= 0) {
		dev_info(chip->dev, "rx cmd error:%d\n", rx_req);
		return;
	}

	dev_info(chip->dev, "rx_req = 0x%x\n", rx_req);

	mutex_lock(&chip->wireless_chg_int_lock);



	rx1619_read(chip, &rx_rev_data[0], REG_RX_REV_DATA1);
	rx1619_read(chip, &rx_rev_data[1], REG_RX_REV_DATA2);
	rx1619_read(chip, &rx_rev_data[2], REG_RX_REV_DATA3);
	rx1619_read(chip, &rx_rev_data[3], REG_RX_REV_DATA4);

	dev_err(chip->dev, "[%s] rx_req,rx_rev_data=0x%x,0x%x,0x%x,0x%x,0x%x\n",
				__func__, rx_req, rx_rev_data[0], rx_rev_data[1],
				rx_rev_data[2], rx_rev_data[3]);

	switch (rx_req) {
	case 0x10:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);
		chip->epp_tx_id_h = rx_rev_data[0]; //epp tx id high byte
		chip->epp_tx_id_l = rx_rev_data[1];

		if (chip->epp_tx_id_l == 0x59) {
			dev_info(chip->dev, "mophie tx, start dc check after 8s\n");
			schedule_delayed_work(&chip->dc_check_work, msecs_to_jiffies(8000));
		} else
			schedule_delayed_work(&chip->dc_check_work, msecs_to_jiffies(2500));

		dev_info(chip->dev, "epp_tx_id_h = 0x%x, epp_tx_id_l = 0x%x\n",
					chip->epp_tx_id_h, chip->epp_tx_id_l);
		break;
	case 0x01:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		chip->epp_max_power = 5;
		msleep(10);

		dev_info(chip->dev, "[%s] LDO on Int \n", __func__);

		chip->epp = (rx_rev_data[0] >> 7) & 0xff;
		chip->epp_max_power = (rx_rev_data[0] & 0x7f);
		g_hw_id_h = rx_rev_data[1];
		g_hw_id_l = rx_rev_data[2];
		g_fw_rx_id = rx_rev_data[3];
		g_fw_tx_id = (~(fw_data_tx[sizeof(fw_data_tx)-1]))&0xff;
		if (chip->epp) {
			chip->epp = 1;
			rx1619_set_pmi_icl(chip, 30000);
			dev_info(chip->dev, "[%s] EPP--10mA and epp max power is %d\n",
							__func__, chip->epp_max_power);
			if (chip->wireless_psy) {
				cp_val.intval = 2;
				power_supply_set_property(chip->wireless_psy,
					POWER_SUPPLY_PROP_DIV_2_MODE, &cp_val);
				msleep(10);
				power_supply_get_property(chip->wireless_psy,
					POWER_SUPPLY_PROP_DIV_2_MODE, &cp_val);
				chip->op_mode = cp_val.intval;
				dev_info(chip->dev, "loop ln8282 set switch and get: %d\n",
							chip->op_mode);
			}
			msleep(50);

			if (chip->op_mode == LN8282_OPMODE_SWITCHING) {
					for (i = 0; i <= 1; i++) {
						uA = (600000 + 800000 * i);
						rx1619_set_pmi_icl(chip, uA);
						usleep_range(10000, 11000);
						usleep_range(10000, 11000);
						usleep_range(10000, 11000);
						usleep_range(10000, 11000);
						usleep_range(10000, 11000);
					}
			} else {
				for (i = 0; i <= 1; i++) {
					uA = (300000 + 400000 * i);
					rx1619_set_pmi_icl(chip, uA);
					usleep_range(10000, 11000);
					usleep_range(10000, 11000);
					usleep_range(10000, 11000);
					usleep_range(10000, 11000);
					usleep_range(10000, 11000);
				}
			}
		} else
			rx1619_set_pmi_icl(chip, DC_LOW_CURRENT);

		dev_info(chip->dev, "[%s] hw_id_h=0x%x,hw_id_l=0x%x,fw_rx_id=0x%x,g_fw_tx_id=0x%x,g_epp_or_bpp=0x%x\n",
						__func__, g_hw_id_h, g_hw_id_l,
						g_fw_rx_id, g_fw_tx_id, g_epp_or_bpp);

		rx1619_set_fod_param(chip, 0x1);

		break;
	case 0x02:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		if (chip->fod_mode == 0x1) {
			rx1619_set_fod_param(chip, 0x2);
		} else if (chip->fod_mode == 0x2) {
			rx1619_set_fod_param(chip, 0x3);
		} else if (chip->fod_mode == 0x3) {
			rx1619_set_fod_param(chip, 0x4);
		}
		dev_info(chip->dev, "[rx1619] [%s] fod param setting \n", __func__);
		break;

	case 0x03:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);
		dev_info(chip->dev, "[rx1619] [%s] Calibration OK! \n", __func__);
		if (chip->epp && chip->epp_max_power == 10) {
			if (chip->op_mode == LN8282_OPMODE_SWITCHING)
				rx1619_set_pmi_icl(chip, 1800000);
			else
				rx1619_set_pmi_icl(chip, 900000);
		}
		break;

	case 0x04:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);

		rx1619_read(chip, &g_tx_id_h, REG_RX_REV_DATA1);//0x0021
		rx1619_read(chip, &g_tx_id_l, REG_RX_REV_DATA2);

		rx1619_write(chip, PRIVATE_ID_CMD, REG_RX_SENT_CMD);
		rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
		dev_err(chip->dev, "[rx1619] [%s] ID OK! \n", __func__);
		break;

	case 0x05:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);

		rx1619_read(chip, &g_shaone_data_h, REG_RX_REV_DATA1);//0x0021
		rx1619_read(chip, &g_shaone_data_l, REG_RX_REV_DATA2);

		rx1619_request_uuid(chip, chip->epp);


		dev_info(chip->dev, "[rx1619] [%s] SHA ONE OK! \n", __func__);
		chip->auth = 1;
		break;

	case 0x06:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);

		rx1619_read(chip, &usb_type, REG_RX_REV_DATA1);

/*
		rx1619_write(chip, PRIVATE_FAST_CHG_CMD, REG_RX_SENT_CMD);    //0x88 fast charge req
		rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
*/
		dev_info(chip->dev, "[rx1619] [%s] usb_type=0x%x\n",
							__func__, usb_type);

		if (chip->is_car_tx && (usb_type >= ADAPTER_XIAOMI_QC3))
			usb_type = ADAPTER_ZIMI_CAR_POWER;

		g_USB_TYPE = usb_type;
		switch (g_USB_TYPE) {
		case ADAPTER_QC2:
			if (!chip->epp && !chip->is_f1_tx)
				chip->target_vol = ADAPTER_BPP_PLUS_VOL;
			else
				fc_flag = 1;
			break;
		case ADAPTER_AUTH_FAILED:
		case ADAPTER_DCP:
		case ADAPTER_CDP:
		case ADAPTER_SDP:
			fc_flag = 1;
			break;
		case ADAPTER_QC3:
		case ADAPTER_PD:
			if (!chip->epp)
				chip->target_vol = ADAPTER_BPP_PLUS_VOL;
			else
				fc_flag = 1;
			break;
		case ADAPTER_XIAOMI_QC3:
		case ADAPTER_XIAOMI_PD:
		case ADAPTER_ZIMI_CAR_POWER:
		case ADAPTER_XIAOMI_PD_40W:
			chip->target_vol = ADAPTER_EPP_MI_VOL;
			break;
		default:
			dev_info(chip->dev, "[%s] other Usb_type\n", __func__);
			break;
		}
		if (fc_flag) {
			set_usb_type_current(chip, g_USB_TYPE);
			schedule_delayed_work(&chip->chg_monitor_work,
						msecs_to_jiffies(1000));
		} else {
			if (chip->target_vol > 0) {
				rx1619_set_adap_vol(chip, chip->target_vol);
				chip->last_vin = chip->target_vol;
			}
		}
		if (g_USB_TYPE == ADAPTER_XIAOMI_PD_40W) {
			if (chip->usb_psy)
				power_supply_changed(chip->usb_psy);
		}

		if (chip->wireless_psy)
			power_supply_changed(chip->wireless_psy);
		break;

	case 0x07:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);

		rx1619_read(chip, &g_fc_status, REG_RX_REV_DATA1);
		dev_info(chip->dev, "[%s] FC status = %d\n",
						__func__, g_fc_status);
		if (g_fc_status == 1) {
			set_usb_type_current(chip, g_USB_TYPE);
			dev_info(chip->dev, "[%s] fast charge success!!! \n",
								__func__);
			schedule_delayed_work(&chip->chg_monitor_work,
						msecs_to_jiffies(1000));
		} else {
			dev_info(chip->dev, "[%s] fast charge fail!!! \n",
									__func__);
			if (!chip->epp) {
				rx1619_set_pmi_icl(chip, DC_BPP_AUTH_FAIL_CURRENT);
				chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;
			}
		}
		break;

	case 0x0d:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		if (chip->is_compatible_hwid) {
			for (i = 0; i < 2; i++)
				g_uuid_data[i] = rx_rev_data[i];

			dev_info(chip->dev, "TX hwid: 0x%x 0x%x\n",
						g_uuid_data[0], g_uuid_data[1]);

			if (g_uuid_data[0] == 0x12 && g_uuid_data[1])
				chip->is_f1_tx = 0;

			if (g_uuid_data[0] == 0x16 && g_uuid_data[1] == 0x11)
				chip->is_f1_tx = 1;
		} else {
			for (i = 0; i < 4; i++)
				g_uuid_data[i] = rx_rev_data[i];

			dev_info(chip->dev, "vendor:0x%x, module:0x%x, hw:0x%x and power:0x%x\n",
						g_uuid_data[0], g_uuid_data[1], g_uuid_data[2], g_uuid_data[3]);

			if (g_uuid_data[3] == 0x01 &&
				g_uuid_data[1] == 0x2 &&
				g_uuid_data[2] == 0x8 &&
				g_uuid_data[0] == 0x6) {
				chip->is_car_tx = 1;
			}
		}
		rx1619_write(chip, PRIVATE_USB_TYPE_CMD, REG_RX_SENT_CMD);
		rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);

		break;

	case 0x0f:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		err_cmd = rx_rev_data[0];
		dev_info(chip->dev, "[%s] Receive error cmd %d\n", __func__, err_cmd);
		if (!chip->epp && ((err_cmd == ID_CMD) ||
				(err_cmd == AUTH_CMD) ||
				(err_cmd == UUID_CMD) ||
				(err_cmd == PRIVATE_USB_TYPE_CMD))) {
			rx1619_set_pmi_icl(chip, DC_BPP_AUTH_FAIL_CURRENT);
			dev_info(chip->dev, "[%s] BPP--800mA \n", __func__);
			chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;
		}

		break;

	default:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);
		dev_err(chip->dev, "[rx1619] [%s] other private cmd \n", __func__);
		break;
	}

	mutex_unlock(&chip->wireless_chg_int_lock);

	return;
}


static irqreturn_t rx1619_chg_stat_handler(int irq, void *dev_id)
{
	struct rx1619_chg *chip = dev_id;


	dev_info(chip->dev, "[%s]\n", __func__);

	schedule_delayed_work(&chip->wireless_int_work, 0);

	return IRQ_HANDLED;
}


static irqreturn_t rx1619_power_good_handler(int irq, void *dev_id)
{
	struct rx1619_chg *chip = dev_id;

	schedule_delayed_work(&chip->wpc_det_work, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}


static int rx1619_parse_dt(struct rx1619_chg *chip)
{
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		dev_err(chip->dev, "[rx1619] [%s] No DT data Failing Probe\n", __func__);
		return -EINVAL;
	}

	chip->tx_on_gpio = of_get_named_gpio(node, "rx,tx_on_gpio", 0);
	if (!gpio_is_valid(chip->tx_on_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_tx_on gpio %d\n",
						 __func__, chip->tx_on_gpio);
		return -EINVAL;
	}

	chip->enable_gpio = of_get_named_gpio(node, "rx,enable", 0);
	if ((!gpio_is_valid(chip->enable_gpio)))
		return -EINVAL;

	chip->irq_gpio = of_get_named_gpio(node, "rx,irq_gpio", 0);
	if (!gpio_is_valid(chip->irq_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_irq_gpio %d\n",
						 __func__, chip->irq_gpio);
		return -EINVAL;
	}

	chip->power_good_gpio = of_get_named_gpio(node, "rx,wpc-det", 0);
	if (!gpio_is_valid(chip->power_good_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_power_good_gpio %d\n",
						 __func__, chip->power_good_gpio);
		return -EINVAL;
	}

	return 0;

}

static int rx1619_gpio_init(struct rx1619_chg *chip)
{
	int ret = 0;
	chip->rx_pinctrl = devm_pinctrl_get(chip->dev);
	if (IS_ERR_OR_NULL(chip->rx_pinctrl)) {
		dev_err(chip->dev, "No pinctrl config specified\n");
		ret = PTR_ERR(chip->dev);
		return ret;
	}

	chip->rx_gpio_active =
		pinctrl_lookup_state(chip->rx_pinctrl, "nu_active");
	if (IS_ERR_OR_NULL(chip->rx_gpio_active)) {
		dev_err(chip->dev, "No active config specified\n");
		ret = PTR_ERR(chip->rx_gpio_active);
		return ret;
	}
	chip->rx_gpio_suspend =
		pinctrl_lookup_state(chip->rx_pinctrl, "nu_suspend");
	if (IS_ERR_OR_NULL(chip->rx_gpio_suspend)) {
		dev_err(chip->dev, "No suspend config specified\n");
		ret = PTR_ERR(chip->rx_gpio_suspend);
		return ret;
	}

	ret = pinctrl_select_state(chip->rx_pinctrl,
			chip->rx_gpio_suspend);
	if (ret < 0) {
		dev_err(chip->dev, "fail to select pinctrl active rc=%d\n",
				ret);
		return ret;
	}

	if (gpio_is_valid(chip->irq_gpio)) {
		chip->client->irq=gpio_to_irq(chip->irq_gpio);
		if (chip->client->irq < 0) {
			dev_err(chip->dev, "[rx1619] [%s] gpio_to_irq Fail! \n", __func__);
			goto fail_irq_gpio;
		}
	} else {
		dev_err(chip->dev, "%s: irq gpio not provided\n", __func__);
		goto fail_irq_gpio;
	}

	if (gpio_is_valid(chip->power_good_gpio)) {
		chip->power_good_irq = gpio_to_irq(chip->power_good_gpio);
		if (chip->power_good_irq < 0) {
			dev_err(chip->dev, "[rx1619] [%s] gpio_to_irq Fail! \n", __func__);
			goto fail_power_good_gpio;
		}
	} else {
		dev_err(chip->dev, "%s: power good gpio not provided\n", __func__);
		goto fail_power_good_gpio;
	}

	return ret;


fail_irq_gpio:
	gpio_free(chip->irq_gpio);
fail_power_good_gpio:
	gpio_free(chip->power_good_gpio);

	return ret;
}

static int rx_set_reverse_gpio(struct rx1619_chg *chip, int enable)
{
	int ret;

	if (gpio_is_valid(chip->tx_on_gpio)) {
		ret = gpio_request(chip->tx_on_gpio,
				"tx-on-gpio");
		if (ret) {
			dev_err(chip->dev,
					"%s: unable to request tx_on gpio 130\n",
					__func__);
		}
		ret = gpio_direction_output(chip->tx_on_gpio, enable);
		if (ret) {
			dev_err(chip->dev,
					"%s: cannot set direction for tx_on gpio 130\n",
					__func__);
		}
		gpio_free(chip->tx_on_gpio);
	} else
		dev_err(chip->dev, "%s: unable to set tx_on gpio_130\n");

	return ret;
}

static int rx_get_reverse_chg_mode(struct rx1619_chg *chip)
{
	int ret;

	if (gpio_is_valid(chip->tx_on_gpio))
		ret = gpio_get_value(chip->tx_on_gpio);
	else {
		dev_err(chip->dev, "%s: txon gpio not provided\n", __func__);
		ret = -1;
	}
	dev_info(chip->dev, "txon gpio: %d\n", ret);

	return ret;
}

static int rx_set_reverse_chg_mode(struct rx1619_chg *chip, int enable)
{
	union power_supply_propval cp_val = {0, };
	union power_supply_propval val = {0, };
	int ret, rc;

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "[idt] no wireless_psy,return\n");
		return -EINVAL;
	}


	if (gpio_is_valid(chip->tx_on_gpio)) {
		ret = gpio_request(chip->tx_on_gpio,
				"tx-on-gpio");
		if (ret) {
			dev_err(chip->dev,
					"%s: unable to request tx_on gpio 130\n",
					__func__);
		}
		ret = gpio_direction_output(chip->tx_on_gpio, enable);
		if (ret) {
			dev_err(chip->dev,
					"%s: cannot set direction for tx_on gpio 130\n",
					__func__);
		}
		gpio_free(chip->tx_on_gpio);

		if (enable) {
			val.intval = 1;
			power_supply_set_property(chip->wireless_psy, POWER_SUPPLY_PROP_SW_DISABLE_DC_EN, &val);
		} else
			power_supply_set_property(chip->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN, &val);

		dev_info(chip->dev, "reverse_charge, set ln8282 powerpath and opmode\n");
		msleep(100);
		if (enable) {
			if (chip->wireless_psy) {
				cp_val.intval = 3;
				power_supply_set_property(chip->wireless_psy,
					POWER_SUPPLY_PROP_DIV_2_MODE, &cp_val);
			}
			msleep(100);
			rx1619_start_tx_function(chip);
			alarm_start_relative(&chip->reverse_dping_alarm,
					ms_to_ktime(REVERSE_DPING_CHECK_DELAY_MS));

		} else {
			dev_info(chip->dev, "disable reverse charging for wireless\n");
			cancel_delayed_work(&chip->reverse_chg_state_work);
			cancel_delayed_work(&chip->reverse_dping_state_work);

			rc = alarm_cancel(&chip->reverse_dping_alarm);
			if (rc < 0)
				dev_err(chip->dev, "Couldn't cancel reverse_dping_alarm\n");

			rc = alarm_cancel(&chip->reverse_chg_alarm);
			if (rc < 0)
				dev_err(chip->dev, "Couldn't cancel reverse_chg_alarm\n");
			pm_relax(chip->dev);
		}
	} else
		dev_err(chip->dev, "%s: unable to set tx_on gpio_130\n");

	return ret;
}

static enum alarmtimer_restart reverse_chg_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct rx1619_chg *chip =
		 container_of(alarm, struct rx1619_chg,
					reverse_chg_alarm);

	dev_info(chip->dev, " Reverse Chg Alarm Triggered %lld\n",
			ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chip->dev);
	schedule_delayed_work(&chip->reverse_chg_state_work, 0);

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart reverse_dping_alarm_cb(struct alarm *alarm,
							ktime_t now)
{
	struct rx1619_chg *chip =
		 container_of(alarm, struct rx1619_chg,
					reverse_dping_alarm);

	dev_info(chip->dev, "Reverse Dping Alarm Triggered %lld\n",
			ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chip->dev);
	schedule_delayed_work(&chip->reverse_dping_state_work, 0);

	return ALARMTIMER_NORESTART;
}

static void rx1619_set_present(struct rx1619_chg *chip, int enable)
{
	union power_supply_propval val = {0, };

	dev_info(chip->dev, "dc plug %s\n", enable ? "in" : "out");

	if (enable) {
		chip->dcin_present = 1;
	} else {
		chip->dcin_present = 0;
		g_id_done_flag = 0;
		g_epp_or_bpp = BPP_MODE;
		chip->epp = 0;
		chip->auth = 0;
		chip->count_10v = 0;
		chip->count_15v = 0;
		chip->epp_exchange = 0;
		chip->exchange = 0;
		chip->last_vin = 0;
		chip->last_icl = 0;
		chip->last_qc3_icl = 0;
		chip->op_mode = LN8282_OPMODE_UNKNOWN;
		chip->status = NORMAL_MODE;
		chip->target_vol = 0;
		chip->target_curr = 0;
		chip->is_car_tx = 0;
		g_USB_TYPE = 0;
		cancel_delayed_work_sync(&chip->wireless_int_work);
		cancel_delayed_work(&chip->chg_monitor_work);
		chip->ln_psy = power_supply_get_by_name("lionsemi");
		if (chip->ln_psy)
			power_supply_set_property(chip->ln_psy,
				POWER_SUPPLY_PROP_RESET_DIV_2_MODE, &val);

	}
}

static int rx_set_enable_mode(struct rx1619_chg *chip, int enable)
{
	int ret = 0;

	if (gpio_is_valid(chip->enable_gpio)) {
		ret = gpio_request(chip->enable_gpio,
				"rx-enable-gpio");
		if (ret) {
			dev_err(chip->dev,
					"%s: unable to request enable gpio [%d]\n",
					__func__, chip->enable_gpio);
		}

		ret = gpio_direction_output(chip->enable_gpio, !enable);
		if (ret) {
			dev_err(chip->dev,
					"%s: cannot set direction for idt enable gpio [%d]\n",
					__func__, chip->enable_gpio);
		}
		gpio_free(chip->enable_gpio);
	}

	return ret;
}

static ssize_t chip_vrect_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int vrect = 0;

	vrect = rx1619_get_rx_vrect(g_chip);

	return scnprintf(buf, PAGE_SIZE, "rx1619 Vrect : %d mV\n", vrect);
}


static ssize_t chip_vout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int vout = 0;

	vout = rx1619_get_rx_vout(g_chip);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vout);
}


static ssize_t chip_iout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unsigned int iout = 0;

	iout = rx1619_get_rx_iout(g_chip);

	return scnprintf(buf, PAGE_SIZE, "%d\n", iout);
}


static ssize_t chip_vout_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index;

	index = (int)simple_strtoul(buf, NULL, 10);
	dev_info(g_chip->dev, "[rx1619] [%s] --Store output_voltage = %d\n",
							__func__, index);
	if ((index < 4000) || (index > 21000)) {
		dev_err(g_chip->dev, "[rx1619] [%s] Store Voltage %s is invalid\n",
							__func__, buf);
		rx1619_set_vout(g_chip, 0);
		return count;
	}

	rx1619_set_vout(g_chip, index);

	return count;
}


static ssize_t chip_debug_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	u8 data = 0;

	rx1619_dump_reg();
	msleep(100);
	rx1619_get_rx_vout(g_chip);
	rx1619_get_rx_vrect(g_chip);
	rx1619_get_rx_iout(g_chip);

	return scnprintf(buf, PAGE_SIZE, "AP REQ DATA : 0x%x \n", data);
}


static ssize_t chip_debug_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	return count;
}

static ssize_t chip_fod_parameter_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int i;
	for (i = 0; i < 8; i++) {
		fod_param[i] = (char)simple_strtoul(buf, NULL, 16);
		buf += 3;
	}

	for (i = 0; i < 8; i++)
		dev_info(g_chip->dev, "[%s]: fod_param[%d] = 0x%x\n", __func__, i, fod_param[i]);

	return count;
}

static ssize_t chip_firmware_update_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{

	bool ret = false;

	dev_info(g_chip->dev, "[rx1619] [%s] Firmware Update begin\n",
						__func__);

	rx_set_reverse_gpio(g_chip, true);

	msleep(100);

	ret = rx1619_onekey_download_firmware(g_chip);
	if (!ret) {
		dev_err(g_chip->dev, "[rx1619] [%s] Firmware Update failed! Please try again!\n",
						__func__);
		rx_set_reverse_gpio(g_chip, false);
		return scnprintf(buf, PAGE_SIZE, "Firmware Update failed! Please try again! \n");

	} else {
		boot_fw_version = (~fw_data_boot[sizeof(fw_data_boot)-1])&0xff;
		tx_fw_version = (~fw_data_tx[sizeof(fw_data_tx)-1])&0xff;
		rx_fw_version = (~fw_data_rx[sizeof(fw_data_rx)-1])&0xff;

		dev_info(g_chip->dev, "boot_fw_version=0x%x, tx_fw_version=0x%x, rx_fw_version=0x%x\n",
						boot_fw_version, tx_fw_version, rx_fw_version);

		dev_info(g_chip->dev, "[rx1619] [%s] Firmware Update Success!!! \n", __func__);
		rx_set_reverse_gpio(g_chip, false);
		return scnprintf(buf, PAGE_SIZE, "Success! boot=0x%x, tx=0x%x, rx=0x%x\n",
							boot_fw_version, tx_fw_version, rx_fw_version);
	}
}


static ssize_t chip_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	g_download_area = TX_AREA;
	rx1619_check_firmware_version(g_chip);
	g_download_area = RX_AREA;
	rx1619_check_firmware_version(g_chip);

	return scnprintf(buf, PAGE_SIZE, "RX_HW:Nu%x%x\nTX: 0x%x\napp_ver: 0x%x\n",
					 g_hw_id_h, g_hw_id_l, g_fw_tx_id, g_fw_rx_id);
}


static ssize_t chip_vout_calibration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index = 0;

	index = (int)simple_strtoul(buf, NULL, 10);

	g_Delta = index;
	dev_err(g_chip->dev, "[rx1619] [%s] g_Delta = %d \n", __func__, g_Delta);

	return count;
}

static ssize_t txon_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int mode;

	mode = rx_get_reverse_chg_mode(g_chip);

	return scnprintf(buf, PAGE_SIZE, "reverse chg mode : %d\n", mode);
}

static ssize_t txon_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret, enable;

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;

	rx_set_reverse_gpio(g_chip, enable);

	return count;
}

static DEVICE_ATTR(chip_vrect, S_IRUGO, chip_vrect_show, NULL);
static DEVICE_ATTR(chip_vout_calibration, S_IWUSR, NULL, chip_vout_calibration_store);
static DEVICE_ATTR(chip_firmware_update, S_IWUSR | S_IRUGO, chip_firmware_update_show, NULL);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IRUGO, chip_iout_show, NULL);
static DEVICE_ATTR(chip_debug, S_IWUSR | S_IRUGO, chip_debug_show, chip_debug_store);
static DEVICE_ATTR(txon, S_IWUSR | S_IRUGO, txon_show, txon_store);
static DEVICE_ATTR(chip_fod_parameter, S_IWUSR | S_IRUGO, NULL, chip_fod_parameter_store);

static struct attribute *rx1619_sysfs_attrs[] = {
	&dev_attr_chip_vrect.attr,
	&dev_attr_chip_version.attr,
	&dev_attr_chip_vout.attr,
	&dev_attr_chip_iout.attr,
	&dev_attr_chip_debug.attr,
	&dev_attr_chip_firmware_update.attr,
	&dev_attr_chip_vout_calibration.attr,
	&dev_attr_txon.attr,
	&dev_attr_chip_fod_parameter.attr,
	NULL,
};

static const struct attribute_group rx1619_sysfs_group_attrs = {
	.attrs = rx1619_sysfs_attrs,
};

#if 1
static enum power_supply_property rx1619_wireless_properties[] = {
	/*
	   POWER_SUPPLY_PROP_PRESENT,
	   POWER_SUPPLY_PROP_ONLINE,
	   POWER_SUPPLY_PROP_CHARGING_ENABLED,
	   POWER_SUPPLY_PROP_RX_CHIP_ID, //RX chip id
	   POWER_SUPPLY_PROP_RX_VRECT, //RX vrect
	   POWER_SUPPLY_PROP_RX_IOUT, //RX output current
	   POWER_SUPPLY_PROP_RX_VOUT, //RX output voltage
	   POWER_SUPPLY_PROP_RX_ILIMIT, //RX Main LDO output current limit
	   POWER_SUPPLY_PROP_VOUT_SET, //Vout voltage set
	 */
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_SIGNAL_STRENGTH,
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_REVERSE_CHG_MODE,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_TX_ADAPTER,
};


static int rx1619_wireless_set_property(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	int ret;
	struct rx1619_chg *chip = power_supply_get_drvdata(psy);
	int data;

	switch (prop) {
		/*
		   case POWER_SUPPLY_PROP_PRESENT:
		   break;
		   case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		   rx1619_chip_enable(chip, val->intval);
		   break;
		   case POWER_SUPPLY_PROP_VOUT_SET:
		   ret = rx1619_set_vout(chip, val->intval);
		   if(ret < 0)
		   return ret;
		   break;
		 */
	case POWER_SUPPLY_PROP_PRESENT:
		rx1619_set_present(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		chip->ss = val->intval;
		power_supply_changed(chip->wireless_psy);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		data = val->intval/1000;
		if (data < ADAPTER_VOUT_LIMIT_VOL)
			data = ADAPTER_VOUT_LIMIT_VOL;
		else if (data > ADAPTER_EPP_QC3_VOL) {
			dev_info(chip->dev, "vout: %d > 11V, set 11V\n", data);
			data = ADAPTER_EPP_QC3_VOL;
		}
		if (chip->op_mode == LN8282_OPMODE_SWITCHING)
			data *= 2;
		else if (chip->epp)
			data = EPP_VOL_THRESHOLD;
		ret = rx1619_set_vout(chip, data);
		break;
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		ret = rx_set_enable_mode(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		chip->is_reverse_chg = 0;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
		if (!chip->power_good_flag) {
			ret = rx_set_reverse_chg_mode(chip, val->intval);
		} else {
			chip->is_reverse_chg = 3;
			schedule_delayed_work(&chip->reverse_sent_state_work, 0);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int rx1619_wireless_get_property(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *val)
{
	int tmp;
	struct rx1619_chg *chip = power_supply_get_drvdata(psy);

	switch (prop) {
		/*
		   case POWER_SUPPLY_PROP_ONLINE:
		   val->intval = chip->online;
		   break;
		   case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		   val->intval = chip->chip_enable;
		   break;
		   case POWER_SUPPLY_PROP_RX_CHIP_ID:
		   val->intval = rx1619_get_rx_chip_id(chip);
		   break;
		   case POWER_SUPPLY_PROP_RX_VRECT:
		   val->intval = rx1619_get_rx_vrect(chip);
		   break;
		   case POWER_SUPPLY_PROP_RX_IOUT:
		   val->intval = rx1619_get_rx_iout(chip);
		   break;
		   case POWER_SUPPLY_PROP_RX_VOUT:
		   val->intval = rx1619_get_rx_vout(chip);
		   break;
		   case POWER_SUPPLY_PROP_VOUT_SET:
		   val->intval = 0;
		   break;
		 */
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chip->dcin_present;
		break;
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		val->intval = chip->ss;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		if (!chip->power_good_flag) {
			val->intval = 0;
			break;
		}
		tmp = rx1619_get_rx_vout(chip);
		if (chip->op_mode == LN8282_OPMODE_SWITCHING)
			val->intval = tmp/2;
		else
			val->intval = tmp;
		break;
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		val->intval = !gpio_get_value(chip->enable_gpio);
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		 val->intval = rx_get_reverse_chg_mode(chip);
		break;
	case POWER_SUPPLY_PROP_TX_ADAPTER:
		val->intval = g_USB_TYPE;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
#endif


static const struct regmap_config rx1619_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
};

static int rx1619_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_PIN_ENABLED:
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		return 1;
	default:
		rc = 0;
		break;
	}

	return rc;
}


static int rx1619_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int ret = 0;
	struct rx1619_chg *chip;
	struct kobject *rx1619_kobj;


	struct power_supply_config wip_psy_cfg = {};
/*
	drv_load = get_board_version();
	if (!drv_load)
		return 0;
*/

	/*
	   int hw_id;

	   struct power_supply *batt_psy;

	   batt_psy = power_supply_get_by_name("battery");
	   if (!batt_psy) {
	   dev_err(&client->dev, "Battery supply not found; defer probe\n");
	   return -EPROBE_DEFER;
	   }

	   hw_id = get_hw_country_version();
	   dev_info(&client->dev, "[rx1619] %s: hw_id is %d\n", __func__, hw_id);
	   if (hw_id)  //hw_id=1 is idt
	   return 0;
	 */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	chip->regmap = regmap_init_i2c(client, &rx1619_regmap_config);
	if (!chip->regmap) {
		dev_err(&client->dev, "parent regmap is missing\n");
		return -EINVAL;
	}

	chip->client = client;
	chip->dev = &client->dev;



	chip->chip_enable = false;
	chip->ss = 2;

	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, chip);

	rx1619_parse_dt(chip);
	rx1619_gpio_init(chip);

	mutex_init(&chip->wireless_chg_lock);
	mutex_init(&chip->wireless_chg_int_lock);
	mutex_init(&chip->sysfs_op_lock);
	INIT_DELAYED_WORK(&chip->wireless_work, rx1619_wireless_work);
	INIT_DELAYED_WORK(&chip->wireless_int_work, rx1619_wireless_int_work);
	INIT_DELAYED_WORK(&chip->wpc_det_work, rx1619_wpc_det_work);
	INIT_DELAYED_WORK(&chip->chg_monitor_work, rx_monitor_work);
	INIT_DELAYED_WORK(&chip->chg_detect_work, rx_chg_detect_work);
	INIT_DELAYED_WORK(&chip->reverse_sent_state_work, reverse_chg_sent_state_work);
	INIT_DELAYED_WORK(&chip->reverse_chg_state_work, reverse_chg_state_set_work);
	INIT_DELAYED_WORK(&chip->reverse_dping_state_work, reverse_dping_state_set_work);
	INIT_DELAYED_WORK(&chip->dc_check_work, rx1619_dc_check_work);

	chip->wip_psy_d.name                    = "rx1619";
	chip->wip_psy_d.type                    = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wip_psy_d.get_property            = rx1619_wireless_get_property;
	chip->wip_psy_d.set_property            = rx1619_wireless_set_property;
	chip->wip_psy_d.properties            = rx1619_wireless_properties;
	chip->wip_psy_d.num_properties        = ARRAY_SIZE(rx1619_wireless_properties);
	chip->wip_psy_d.property_is_writeable = rx1619_prop_is_writeable,

	wip_psy_cfg.drv_data = chip;

	chip->wip_psy = devm_power_supply_register(chip->dev, &chip->wip_psy_d, &wip_psy_cfg);
	if (IS_ERR(chip->wip_psy)) {
		dev_err(chip->dev, "Couldn't register wip psy rc=%ld\n", PTR_ERR(chip->wip_psy));
		return ret;
	}

	if (chip->client->irq) {
		ret = devm_request_threaded_irq(&chip->client->dev, chip->client->irq, NULL,
				rx1619_chg_stat_handler,
				(IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
				"rx1619_chg_stat_irq", chip);
		if (ret) {
			dev_err(chip->dev, "Failed irq = %d ret = %d\n", chip->client->irq, ret);
		}
	}
	enable_irq_wake(chip->client->irq);

	if (chip->power_good_irq) {
		ret = devm_request_threaded_irq(&chip->client->dev, chip->power_good_irq, NULL,
				rx1619_power_good_handler,
				(IRQF_TRIGGER_FALLING |  IRQF_TRIGGER_RISING | IRQF_ONESHOT),
				"rx1619_power_good_irq", chip);
		if (ret) {
			dev_err(chip->dev, "Failed irq = %d ret = %d\n", chip->power_good_irq, ret);
		}
	}
	enable_irq_wake(chip->power_good_irq);

	rx1619_kobj = kobject_create_and_add("rx1619", NULL);
	if (!rx1619_kobj) {
		dev_err(chip->dev, "sysfs_create_group fail");
		goto error_sysfs;
	}
	ret = sysfs_create_group(rx1619_kobj,&rx1619_sysfs_group_attrs);
	if (ret < 0)
	{
		dev_err(chip->dev, "sysfs_create_group fail %d\n", ret);
		goto error_sysfs;
	}

	determine_initial_status(chip);


	g_chip = chip;

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->reverse_dping_alarm,
			ALARM_BOOTTIME, reverse_dping_alarm_cb);
	} else {
		dev_err(chip->dev, "Failed to initialize reverse dping alarm\n");
		return -ENODEV;
	}

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->reverse_chg_alarm,
			ALARM_BOOTTIME, reverse_chg_alarm_cb);
	} else {
		dev_err(chip->dev, "Failed to initialize reverse chg alarm\n");
		return -ENODEV;
	}

	dev_err(chip->dev, "[rx1619] [%s] success! \n", __func__);

	schedule_delayed_work(&chip->chg_detect_work, 8 * HZ);
	return 0;


error_sysfs:
	sysfs_remove_group(rx1619_kobj, &rx1619_sysfs_group_attrs);
	dev_err(chip->dev, "[rx1619] [%s] rx1619 probe error_sysfs! \n", __func__);

	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);

	return 0;
}

static int rx1619_remove(struct i2c_client *client)
{
	struct rx1619_chg *chip = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&chip->wireless_work);
	cancel_delayed_work_sync(&chip->wireless_int_work);

	return 0;
}

static const struct i2c_device_id rx1619_id[] = {
	{rx1619_DRIVER_NAME, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rx1619_id);

static struct of_device_id  rx1619_match_table[] = {
	{ .compatible = "nuvolta,wl_charger_rx1619",},
	{}
};

static struct i2c_driver rx1619_driver = {
	.driver = {
		.name = rx1619_DRIVER_NAME,
		.of_match_table = rx1619_match_table,
	},
	.probe = rx1619_probe,
	.remove = rx1619_remove,
	.id_table = rx1619_id,
};

static int __init rx1619_init(void)
{
	int ret;
	int drv_load = 0;

	drv_load = get_board_version();
	if (!drv_load)
		return 0;

	ret = i2c_add_driver(&rx1619_driver);
	if (ret)
		printk(KERN_ERR "rx1619 i2c driver init failed!\n");

	return ret;
}

static void __exit rx1619_exit(void)
{
	i2c_del_driver(&rx1619_driver);
}

module_init(rx1619_init);
module_exit(rx1619_exit);

MODULE_AUTHOR("bsp-charging@xiaomi.com");
MODULE_DESCRIPTION("NUVOLTA Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL/BSD");
