
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
#include <linux/pmic-voter.h>
//#include <soc/qcom/socinfo.h>
#include <linux/power/ln8282.h>
/* add for get hw country */
#include <soc/qcom/socinfo.h>
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
#define FW_ERROR_CODE 0xFE
#define FW_EMPTY_CODE 0xFF
#define TAPER_SOC 95

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
#define ADAPTER_VOICE_BOX     0x0d
#define ADAPTER_XIAOMI_PD_45W 0x0e
#define ADAPTER_XIAOMI_PD_60W 0x0f

//0x000b[0:3]  0000:no charger, 0001:SDP, 0010:CDP, 0011:DCP, 0101:QC2-other,
//0110:QC3-other, 0111:PD, 1000:fail charger, 1001:QC3-27W, 1010:PD-27W
#define EPP_MODE_CURRENT 600000
#define DC_OTHER_CURRENT 750000
#define DC_LOW_CURRENT 200000	//200mA
#define DC_DCP_CURRENT 700000
#define DC_CDP_CURRENT 800000
#define DC_QC2_CURRENT 1000000
#define DC_QC3_CURRENT 1800000
#define DC_QC3_BPP_CURRENT 900000
#define DC_TURBO_CURRENT 1100000
#define DC_QC3_20W_CURRENT 2100000	//2.1A
#define DC_PD_CURRENT      1000000
#define DC_PD_20W_CURRENT  2000000	//2A
#define DC_PD_40W_CURRENT  3100000	//3.1A
#define DC_BPP_CURRENT 850000
#define DC_SDP_CURRENT 500000
#define SCREEN_OFF_FUL_CURRENT 250000
#define DC_FULL_CURRENT 350000
#define DC_BPP_AUTH_FAIL_CURRENT 750000
#define USB_20W_PLUS_BASE_CURRENT_UA 1000000	//1.0A
#define USB_20W_PLUS_CURRENT_UA 1800000	//1.8A

#define ADAPTER_DEFAULT_VOL	6000
#define ADAPTER_BPP_LIMIT_VOL	6500
#define ADAPTER_VOUT_LIMIT_VOL	6000
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
#define ICL_EXCHANGE_COUNT	2	/*5 = 1min */
#define LIMIT_EPP_IOUT 500
#define LIMIT_BPP_IOUT 500
#define HIGH_THERMAL_LEVEL_THR		12

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
	0x44, 0x64,
	0x44, 0x78,
	0x3A, 0x67,
	0x37, 0x67
};

static struct rx1619_chg *g_chip;
static int g_Delta;
static u8 g_project_id_h;
static u8 g_project_id_l;
static u8 g_fw_boot_id;
static u8 g_id_done_flag;
static u8 g_fw_rx_id;
static u8 g_fw_tx_id;
static u8 g_hw_id_h, g_hw_id_l;
static u8 g_epp_or_bpp = BPP_MODE;
static u8 g_USB_TYPE = 0;
static bool g_rx1619_restart_flag;
static bool g_rx1619_first_flag;
static u8 rx_bin_project_id_h;
static u8 rx_bin_project_id_l;
static bool is_nvt_rx;
static bool need_unconfig_pg;

static u32 g_fw_data_lenth;
static u8 boot_fw_version;
static u8 rx_fw_version;
static u8 tx_fw_version;

static int rx_set_enable_mode(struct rx1619_chg *chip, int enable);
static int rx_get_reverse_chg_mode(struct rx1619_chg *chip);
static int rx_set_reverse_gpio(struct rx1619_chg *chip, int enable);
static int rx_set_reverse_chg_mode(struct rx1619_chg *chip, int enable);

#define BOOT_AREA   0
#define RX_AREA     1
#define TX_AREA     2

#define NORMAL_MODE 0x1
#define TAPER_MODE  0x2
#define FULL_MODE   0x3
#define RECHG_MODE  0x4

//fw version to update
#define FW_VERSION  0x15

struct rx1619_chg {
	char *name;
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	unsigned int tx_on_gpio;
	unsigned int irq_gpio;
	unsigned int power_good_gpio;
	unsigned int power_good_irq;
	unsigned int enable_gpio;
	unsigned int chip_enable;
	unsigned int reverse_boost_enable_gpio;
	int online;
	struct pinctrl *rx_pinctrl;
	struct pinctrl_state *rx_gpio_active;
	struct pinctrl_state *rx_gpio_suspend;
	struct delayed_work wireless_int_work;
	struct delayed_work wpc_det_work;
	struct delayed_work chg_monitor_work;
	struct delayed_work chg_detect_work;
	struct delayed_work reverse_sent_state_work;
	struct delayed_work reverse_chg_state_work;
	struct delayed_work reverse_dping_state_work;
	struct delayed_work oob_set_cep_work;
	struct delayed_work oob_set_ept_work;
	struct delayed_work dc_check_work;
	struct delayed_work cmd_timeout_work;
	struct delayed_work fw_download_work;
	struct delayed_work pan_tx_work;
	struct delayed_work     voice_tx_work;
	struct delayed_work     rx_first_boot;
	struct mutex wireless_chg_lock;
	struct mutex wireless_chg_int_lock;
	struct mutex sysfs_op_lock;

	struct power_supply *wip_psy;
	struct power_supply *dc_psy;
	struct power_supply_desc wip_psy_d;
	struct power_supply *wireless_psy;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *pc_port_psy;
	struct power_supply *ln_psy;
	struct power_supply *bbc_psy;
	struct alarm reverse_dping_alarm;
	struct alarm reverse_chg_alarm;
	struct alarm cmd_timeout_alarm;
	struct alarm reverse_test_ready_alarm;
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
	int power_off_mode;

	/* product related */
	bool wireless_by_usbin;
	int power_good_flag;
	int ss;
	int is_reverse_chg;
	int is_reverse_gpio;
	u8 mac_addr[6];
	u8 rpp_val[2];
	u8 cep_val;
	u8 fod_mode;
	int is_oob_ok;
	int is_car_tx;
	int is_ble_tx;
	int is_voice_box_tx;
	int is_compatible_hwid;
	int is_f1_tx;
	u8 epp_tx_id_h;
	u8 epp_tx_id_l;
	u8 *rx_fw_bin;
	int rx_fw_len;
	bool enabled_aicl;
	bool fw_update;
	int fw_version;
	int chip_ok;
	int hw_country;
	int reverse_gpio_state;
	int is_otg_insert;
	int is_pan_tx;
	bool disable_bq;
	bool is_urd_device;
	bool wait_for_reverse_test;
	struct votable          *fcc_votable;
};

/*extern char *saved_command_line;
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
		if (!strncmp(boot, "P1.2", strlen("P1.2")))
			return 1;
		else if (!strncmp(boot, "P1.6", strlen("P1.6")))
			return 1;
	}
	return 0;
}*/

static int rx1619_read(struct rx1619_chg *chip, u8 *val, u16 addr)
{
	unsigned int temp;
	int rc;

	rc = regmap_read(chip->regmap, addr, &temp);
	if (rc >= 0) {
		*val = (u8) temp;
		//dev_err(chip->dev, "[rx1619] [%s] [0x%04x] = [0x%x] \n", __func__, addr, *val);
	}

	return rc;
}

static int rx1619_write(struct rx1619_chg *chip, u8 val, u16 addr)
{
	int rc = 0;

	rc = regmap_write(chip->regmap, addr, val);
	if (rc >= 0) {
		//dev_err(chip->dev, "[rx1619] [%s] [0x%04x] = [0x%x] \n", __func__, addr, *val);
	}

	return rc;
}

//mode1: gain_code = gain/1000*65535/100; offset_code = offset/100
//mode2,3,4: gain_code = gain/1000*65535; offset_code = offset/10
void rx1619_set_fod_param(struct rx1619_chg *chip, u8 mode)
{
	rx1619_write(chip, 0x85, REG_RX_SENT_CMD);	//send fod parameters

	if (mode == 1) {
		rx1619_write(chip, 0x1, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[0], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[1], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x1;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[0],
			 fod_param[1]);
	} else if (mode == 2) {
		rx1619_write(chip, 0x2, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[2], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[3], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x2;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[2],
			 fod_param[3]);
	} else if (mode == 3) {
		rx1619_write(chip, 0x3, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[4], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[5], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x3;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[4],
			 fod_param[5]);
	} else if (mode == 4) {
		rx1619_write(chip, 0x4, REG_RX_SENT_DATA1);
		rx1619_write(chip, fod_param[6], REG_RX_SENT_DATA2);
		rx1619_write(chip, fod_param[7], REG_RX_SENT_DATA3);
		chip->fod_mode = 0x4;
		dev_info(chip->dev, "[%s] 0x%x,0x%x \n", __func__, fod_param[6],
			 fod_param[7]);
	}

	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
	dev_info(chip->dev, "[%s] mode = 0x%x \n", __func__, mode);
}

void rx1619_set_adap_vol(struct rx1619_chg *chip, u16 mv)
{
	dev_info(chip->dev, "set adapter vol to %d\n", mv);
	rx1619_write(chip, PRIVATE_FAST_CHG_CMD, REG_RX_SENT_CMD);	//0x87 usb type req
	rx1619_write(chip, mv & 0xff, REG_RX_SENT_DATA1);	//LSB
	rx1619_write(chip, (mv >> 8) & 0xff, REG_RX_SENT_DATA2);	//MSB
	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
}

/*OOB function*/

static u8 oob_check_sum(u8 *buf, u32 size)
{
	u8 chksum = 0;
	while (size--) {
		chksum ^= *buf++;
	}

	return chksum;
}

static int rx1619_set_ept(struct rx1619_chg *chip)
{
	union power_supply_propval val = { 0, };
	int ept = 0;
	u8 ept_raw = 0x0b;
	u8 header[2] = { 0 };
	u8 chksum[4] = { 0 };
	int rc;

	header[1] = 0x02;	//add the ept header

	chksum[0] = header[1];
	chksum[1] = ept_raw;
	header[0] = oob_check_sum(chksum, 2);
	ept = header[0] | ept_raw << 8 | (header[1] << 16);
	val.int64val = ept;

	if (chip->wireless_psy) {
		mutex_lock(&chip->sysfs_op_lock);
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_RX_CEP, &val);
		mutex_unlock(&chip->sysfs_op_lock);
	} else {
		dev_err(chip->dev, "BLE EPT set error:\n");
		rc = -EINVAL;
	}

	dev_info(chip->dev, "Header: 0x%x, ept_raw: 0x%x, checksum: 0x%x\n",
		 header[1], ept_raw, header[0]);

	return rc;
}

static int rx1619_set_rpp(struct rx1619_chg *chip)
{
	int rpp = 0;
	int64_t rpp_ul = 0;
	u8 header[2] = { 0 };
	u8 chksum[4] = { 0 };
	union power_supply_propval val = { 0, };
	int rc;

	header[1] = 0x31;	//add the rpp header

	chksum[0] = header[1];
	chksum[1] = chip->rpp_val[0];
	chksum[2] = chip->rpp_val[1];
	header[0] = oob_check_sum(chksum, 3);

	dev_info(chip->dev,
		 "header: 0x%x, RPP_raw:  0x%x, 0x%x, checksum: 0x%x\n",
		 header[1], chip->rpp_val[1], chip->rpp_val[0], header[0]);
	rpp = header[0] | (chip->rpp_val[0] << 16) | (chip->rpp_val[1] << 8);
	rpp_ul = header[1];
	rpp_ul <<= 32;
	rpp_ul |= rpp;
	val.int64val = rpp_ul;

	if (chip->wireless_psy) {
		mutex_lock(&chip->sysfs_op_lock);
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_RX_CR, &val);
		mutex_unlock(&chip->sysfs_op_lock);
	} else {
		dev_err(chip->dev, "BLE RPP set error:\n");
		rc = -EINVAL;
	}
	return rc;
}

static int rx1619_set_cep(struct rx1619_chg *chip)
{
	int cep = 0;
	u8 header[2] = { 0 };
	u8 chksum[4] = { 0 };
	union power_supply_propval val = { 0, };
	int rc;

	header[1] = 0x03;	//add the rpp header

	chksum[0] = header[1];
	chksum[1] = chip->cep_val;
	header[0] = oob_check_sum(chksum, 2);

	dev_info(chip->dev, "Header: 0x%x, CEP_raw: 0x%x, checksum: 0x%x\n",
		 header[1], chip->cep_val, header[0]);
	cep = header[0] | chip->cep_val << 8 | (header[1] << 16);
	val.int64val = cep;
	if (chip->wireless_psy) {
		mutex_lock(&chip->sysfs_op_lock);
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_RX_CEP, &val);
		mutex_unlock(&chip->sysfs_op_lock);
	} else {
		dev_err(chip->dev, "BLE CEP set error:\n");
		rc = -EINVAL;
	}
	return rc;
}

static void rx1619_oob_set_ept_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       oob_set_ept_work.work);

	rx1619_set_ept(chip);

	return;
}

unsigned int rx1619_get_cep_value(struct rx1619_chg *chip)
{
	u8 data = 0;

	rx1619_read(chip, &data, REG_CEP_VALUE);	//cep-0x0008
	dev_info(chip->dev, "[%s] cep value=0x%x \n", __func__, data);

	return data;
}

void rx1619_set_ble_status(struct rx1619_chg *chip, u8 data)
{
	rx1619_write(chip, 0x8b, REG_RX_SENT_CMD);	//0x8b sent BLEOK
	rx1619_write(chip, data, REG_RX_SENT_DATA1);
	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
	dev_info(chip->dev, "[%s] AP sent ble_OK status = 0x%x\n", __func__,
		 data);
}

void rx1619_request_low_addr(struct rx1619_chg *chip)
{
	rx1619_write(chip, 0x8e, REG_RX_SENT_CMD);	//0x8e request low addr
	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);	//0x8b sent BLEOK
}

void rx1619_request_high_addr(struct rx1619_chg *chip)
{
	rx1619_write(chip, 0x8f, REG_RX_SENT_CMD);	//0x8f request high addr
	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);	//0x8b sent BLEOK
}

void rx1619_request_uuid(struct rx1619_chg *chip, int is_epp)
{
	rx1619_write(chip, UUID_CMD, REG_RX_SENT_CMD);	//0x8f request high addr

	if (is_epp) {
		rx1619_write(chip, 0x4C, REG_RX_SENT_DATA1);
		chip->is_compatible_hwid = 0;
	} else {
		rx1619_write(chip, 0x3F, REG_RX_SENT_DATA1);
		chip->is_compatible_hwid = 1;
	}

	rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);	//0x8b sent BLEOK
}

void rx1619_sent_tx_mac(struct rx1619_chg *chip)
{
	int64_t ble_mac = 0;
	union power_supply_propval val = { 0, };
	uint32_t lens = 6;
	memcpy(&ble_mac, chip->mac_addr, lens);
	val.int64val = ble_mac;
	if (chip->wireless_psy)
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_TX_MAC, &val);
	else
		dev_err(chip->dev, "BLE mac addr get error:\n");
}

void rx1619_oob_set_cep_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       oob_set_cep_work.work);

	if (chip->is_oob_ok) {
		mutex_lock(&chip->wireless_chg_lock);
		chip->cep_val = rx1619_get_cep_value(chip);
		rx1619_set_cep(chip);
		dev_info(chip->dev, "[%s] cep_value = 0x%x\n", __func__,
			 chip->cep_val);
		schedule_delayed_work(&chip->oob_set_cep_work,
				      msecs_to_jiffies(1000));
		mutex_unlock(&chip->wireless_chg_lock);
	} else
		dev_info(chip->dev, "[%s] oob disabled = %d\n", __func__,
			 chip->is_oob_ok);
	return;
}

int rx_op_ble_flag(int en)
{
	int rc;
	if (!g_chip)
		return -EINVAL;

	dev_info(g_chip->dev, "set ble flag: %d\n", en);

	if (en)
		rx1619_set_ble_status(g_chip, 1);
	else {
		rx1619_set_ble_status(g_chip, 0);
		cancel_delayed_work_sync(&g_chip->oob_set_cep_work);
	}

	return rc;
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

	chip->pc_port_psy = power_supply_get_by_name("pc_port");
	if (!chip->pc_port_psy) {
		dev_err(chip->dev, "[idt] no pc_port_psy,return\n");
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

	ret = rx1619_write(chip, 0x20, 0x000d);	//0x10 rx, 0x20 tx
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
	u8 data = 0;

	rx1619_read(chip, &data, 0x000a);	//0x000a
	dev_err(chip->dev, "[%s] data=%d\n", __func__, data);

	return data;
}

/*
init 0x00
ping phase 0x1
power transfer 0x02
power limit 0x03
*/
#define PING            0x1
#define TRANSFER        0x2
#define POWER_LIM       0x3
#define CONFIGURE_CNF   0x4
#define CONFIGURE_ERR   0x5
#define REVERSE_TEST_DONE 0x06
#define REVERSE_TEST_READY   0x7

unsigned char rx1619_get_tx_phase(struct rx1619_chg *chip)
{
	u8 data = 0;

	rx1619_read(chip, &data, 0x000b);
	dev_info(chip->dev, "[%s] data=%d\n", __func__, data);

	return data;
}

unsigned int rx1619_get_rx_vrect(struct rx1619_chg *chip)
{
	u16 vrect = 0;
	u8 data = 0;

	rx1619_read(chip, &data, 0x000A);	//vrect-0x000A
	vrect = (data * 27500) >> 8;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, Vrect=%d mV \n",
		__func__, data, vrect);

	return vrect;
}

unsigned int rx1619_get_rx_vout(struct rx1619_chg *chip)
{
	u16 vout = 0;
	u8 data = 0;

	rx1619_read(chip, &data, 0x0009);	//vout-0x0009
	vout = (int)(data * 22500) >> 8;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, Vout=%d mV \n",
		__func__, data, vout);

	return vout;
}

unsigned int rx1619_get_rx_iout(struct rx1619_chg *chip)
{
	u16 iout = 0;
	u8 data = 0;

	rx1619_read(chip, &data, 0x000B);	//Iout-0x000B
	iout = (int)(data * 2500) >> 8;
	dev_err(chip->dev, "[rx1619] [%s] data=%d, iout=%d mA \n",
		__func__, data, iout);

	return iout;
}


int rx1619_set_vout(struct rx1619_chg *chip, int volt)
{
	u8 value_h, value_l, ret;
	u16 vout_set, vrect_set;

	if ((volt < 4000) && (volt > 21000)) {
		volt = 6000;	//6V
	}

	volt += g_Delta;
	dev_info(chip->dev, "[rx1619] [%s] volt = %d, g_Delta=%d \n",
		 __func__, volt, g_Delta);

	vout_set = (u16) ((volt * 3352) >> 16);	//(u16)(volt*1023/20000)
	vrect_set = (u16) (((volt + 200) * 2438) >> 16);	//((volt+200)*1023/27500)
/*
	dev_info(chip->dev, "[rx1619] [%s] vout_set = 0x%x, vrect_set=0x%x \n",
				__func__, vout_set, vrect_set);
*/

	value_h = (u8) (vout_set >> 8);
	value_l = (u8) (vout_set & 0xFF);
	ret = rx1619_write(chip, value_h, 0x0001);
	ret = rx1619_write(chip, value_l, 0x0002);
/*
	dev_info(chip->dev, "[rx1619] [%s] vout value_h = 0x%x, value_l=0x%x \n",
				__func__, value_h, value_l);
*/

	value_h = (u8) (vrect_set >> 8);
	value_l = (u8) (vrect_set & 0xFF);
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
	unsigned int voltage = 0;

	voltage = rx1619_get_rx_vout(chip);

	dev_err(chip->dev, "[rx1619] [%s] Vout = %d \n", __func__, voltage);

	if ((voltage > MIN_VOUT) && (voltage < MAX_VOUT)) {
		vout_status = true;
	} else {
		vout_status = false;
	}

	return vout_status;
}

#define REVERSE_PARAM_CMD               0x13
#define REVERSE_PARAM_CMD_ADDR          0x0000
#define REVERSE_PARAM_MODE_ADDE         0x0001
#define REVERSE_PARAM_IIN_ADDR          0x0002


#define REVERSE_FOD_COMPENSATION_ADDR       0x0003
#define REVERSE_FOD_THRESHOLD_ADDR          0x0004

static void rx1619_set_reverse_fod(struct rx1619_chg *chip, u8 compensation,
				   u8 threshold)
{
	dev_info(chip->dev, "compensation: %d, threshold:%d\n", compensation,
		 threshold);
	rx1619_write(chip, compensation, REVERSE_FOD_COMPENSATION_ADDR);
	rx1619_write(chip, threshold, REVERSE_FOD_THRESHOLD_ADDR);
	return;
}

static void determine_initial_status(struct rx1619_chg *chip)
{
	bool vout_on = false;

	vout_on = rx1619_is_vout_on(chip);
	if (vout_on) {
		g_rx1619_restart_flag = true;
	}

	dev_err(chip->dev, "[%s] initial vout_on = %d \n", __func__, vout_on);
}

static bool rx1619_check_firmware_version(struct rx1619_chg *chip, u8 area)
{
	static u16 addr;
	u8 addr_h, addr_l;
	int i = 0;
	u8 read_buf[20];
	char *fw_data = NULL;

	dev_err(chip->dev, "[rx1619] [%s] enter \n", __func__);

	if (area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
		fw_data = fw_data_boot;
	} else if (area == RX_AREA) {
		addr = 256;
		g_fw_data_lenth = sizeof(fw_data_rx);
		fw_data = fw_data_rx;
	} else if (area == TX_AREA) {
		addr = 4864;
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
	addr += (g_fw_data_lenth / 4);

	for (i = 0; i < 12; i += 4) {
		/************write_mtp_addr************/
		addr_h = (u8) (addr >> 8);
		addr_l = (u8) (addr & 0xff);
		rx1619_write(chip, addr_h, 0x0010);
		rx1619_write(chip, addr_l, 0x0011);
		/************write_mtp_addr************/

		addr--;

		/************read pause************/
		rx1619_write(chip, 0x02, 0x0018);
		rx1619_write(chip, 0x00, 0x0018);
		/************read pause************/

		/************read data************/
		rx1619_read(chip, &read_buf[i + 3], 0x0013);
		rx1619_read(chip, &read_buf[i + 2], 0x0014);
		rx1619_read(chip, &read_buf[i + 1], 0x0015);
		rx1619_read(chip, &read_buf[i + 0], 0x0016);
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

	dev_info(chip->dev,
		 "chip_data version=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		 read_buf[4], read_buf[5], read_buf[6], read_buf[7],
		 read_buf[8], read_buf[9], read_buf[10], read_buf[11]);

	dev_info(chip->dev,
		 "fw_data version=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
		 fw_data[g_fw_data_lenth - 4], fw_data[g_fw_data_lenth - 3],
		 fw_data[g_fw_data_lenth - 2], fw_data[g_fw_data_lenth - 1],
		 fw_data[g_fw_data_lenth - 8], fw_data[g_fw_data_lenth - 7],
		 fw_data[g_fw_data_lenth - 6], fw_data[g_fw_data_lenth - 5]);

	if (area == RX_AREA) {
		if (read_buf[0] == 0x66) {
			g_project_id_h = (~read_buf[5]);
			g_project_id_l = (~read_buf[6]);
			g_fw_rx_id = (~read_buf[7]);
			g_hw_id_h = (~read_buf[10]);
			g_hw_id_l = (~read_buf[11]);
		} else {
			g_project_id_h = 0x00;
			g_project_id_l = 0x00;
			g_fw_rx_id = FW_ERROR_CODE;
			g_hw_id_h = 0x00;
			g_hw_id_l = 0x00;
		}
	} else if (area == TX_AREA) {
		if (read_buf[0] == 0x66) {
			g_project_id_h = (~read_buf[5]);
			g_project_id_l = (~read_buf[6]);
			g_fw_tx_id = (~read_buf[7]);
		} else {
			g_project_id_h = 0x00;
			g_project_id_l = 0x00;
			g_fw_tx_id = FW_ERROR_CODE;
		}
	} else if (area == BOOT_AREA) {
		if (read_buf[0] == 0x66) {
			g_project_id_h = (~read_buf[5]);
			g_project_id_l = (~read_buf[6]);
			g_fw_boot_id = (~read_buf[7]);
		} else {
			g_project_id_h = 0x00;
			g_project_id_l = 0x00;
			g_fw_boot_id = FW_ERROR_CODE;
		}
	}

	if ((read_buf[7] == fw_data[g_fw_data_lenth - 1]) && (read_buf[0] == 0x66)) {	//read_buf[7] is vesion, read_buf[0] is confirm value
		dev_info(chip->dev, "g_fw_data_lenth=%d, fw version = 0x%x\n",
			 g_fw_data_lenth, (~read_buf[7]) & 0xff);
		return true;
	}

	return false;
}

static bool rx1619_update_fw_confirm_data(struct rx1619_chg *chip, u8 area,
					  u8 confirm_data)
{
	u16 addr = 0;
	u8 addr_h, addr_l;
	u8 data[4] = { 0, 0, 0, 0 };

	/************prepare_for_mtp_write************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);

//      rx1619_write(chip, 0x19, 0x0018);
	rx1619_write(chip, 0x10, 0x1000);
	rx1619_write(chip, 0x3e, 0x1130);
	/************prepare_for_mtp_write************/

	msleep(20);

	/************write_mtp_addr************/
	if (area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
	} else if (area == RX_AREA) {
		addr = 256;
		g_fw_data_lenth = sizeof(fw_data_rx);
	} else if (area == TX_AREA) {
		addr = 4864;
		g_fw_data_lenth = sizeof(fw_data_tx);
	}

	addr += (g_fw_data_lenth / 4);

	addr_h = (u8) (addr >> 8);
	addr_l = (u8) (addr & 0xff);
	rx1619_write(chip, addr_h, 0x0010);
	rx1619_write(chip, addr_l, 0x0011);
	/************write_mtp_addr************/

	/************enable write************/
	rx1619_write(chip, 0x01, 0x0017);
	rx1619_write(chip, 0x30, 0x1000);
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
	rx1619_write(chip, confirm_data, 0x0012);
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

	msleep(100);		//

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
	addr_h = (u8) (addr >> 8);
	addr_l = (u8) (addr & 0xff);
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

	if (data[0] == confirm_data) {
		dev_err(chip->dev, "Update Confirm data Success!!!\n");
		return true;
	} else {
		dev_err(chip->dev, "Update Confirm data Failed!!! \n");
		return false;
	}
}

static bool rx1619_download_firmware(struct rx1619_chg *chip, u8 area)
{
	u16 addr = 0;
	u8 addr_h, addr_l;
	int i = 0;
	u8 j = 0;
	char *fw_data = NULL;
	u32 count = 0;

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

//      rx1619_write(chip, 0x19, 0x0018);
	rx1619_write(chip, 0x10, 0x1000);
	rx1619_write(chip, 0x3e, 0x1130);
	/************prepare_for_mtp_write************/

	msleep(20);

	/************write_mtp_addr************/
	if (area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
		fw_data = fw_data_boot;
	} else if (area == RX_AREA) {
		addr = 256;
		g_fw_data_lenth = sizeof(fw_data_rx);
		fw_data = fw_data_rx;
	} else if (area == TX_AREA) {
		addr = 4864;
		g_fw_data_lenth = sizeof(fw_data_tx);
		fw_data = fw_data_tx;
	}
	addr_h = (u8) (addr >> 8);
	addr_l = (u8) (addr & 0xff);
	rx1619_write(chip, addr_h, 0x0010);
	rx1619_write(chip, addr_l, 0x0011);
	/************write_mtp_addr************/

	/************enable write************/
	rx1619_write(chip, 0x01, 0x0017);
	rx1619_write(chip, 0x30, 0x1000);
	rx1619_write(chip, 0x5a, 0x001a);
	/************enable write************/

	/************write data************/
	for (i = 0; i < g_fw_data_lenth; i += 4) {
		rx1619_write(chip, fw_data[i + 3], 0x0012);
		for (j = 0; j < 4; j++) {
			if (gpio_get_value(chip->power_good_gpio) == 1) {
				usleep_range(300, 350);
				if (gpio_get_value(chip->power_good_gpio) == 0) {
					break;
				}
			} else if (gpio_get_value(chip->power_good_gpio) == 0) {
				usleep_range(900, 950);
				count++;
				break;
			}

		}

		rx1619_write(chip, fw_data[i + 2], 0x0012);
		for (j = 0; j < 4; j++) {
			if (gpio_get_value(chip->power_good_gpio) == 1) {
				usleep_range(300, 350);
				if (gpio_get_value(chip->power_good_gpio) == 0) {
					break;
				}
			} else if (gpio_get_value(chip->power_good_gpio) == 0) {
				usleep_range(900, 950);
				count++;
				break;
			}

		}

		rx1619_write(chip, fw_data[i + 1], 0x0012);
		for (j = 0; j < 4; j++) {
			if (gpio_get_value(chip->power_good_gpio) == 1) {
				usleep_range(300, 350);
				if (gpio_get_value(chip->power_good_gpio) == 0) {
					break;
				}
			} else if (gpio_get_value(chip->power_good_gpio) == 0) {
				usleep_range(900, 950);
				count++;
				break;
			}

		}

		rx1619_write(chip, fw_data[i + 0], 0x0012);
		for (j = 0; j < 4; j++) {
			if (gpio_get_value(chip->power_good_gpio) == 1) {
				usleep_range(300, 350);
				if (gpio_get_value(chip->power_good_gpio) == 0) {
					break;
				}
			} else if (gpio_get_value(chip->power_good_gpio) == 0) {
				usleep_range(900, 950);
				count++;
				break;
			}

		}
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

	dev_err(chip->dev,
		"[rx1619] [%s] area=%d,addr=%d, g_fw_data_lenth=%d, count=%d\n",
		__func__, area, addr, g_fw_data_lenth, count);
	dev_err(chip->dev, "[rx1619] [%s] exit \n", __func__);

	return 0;
}

static int rx1619_check_firmware(struct rx1619_chg *chip, u8 area)
{
	u32 success_count = 0;
	u16 addr = 0;
	u8 addr_h, addr_l;
	int i = 0;
	int j = 0;
	u8 read_buf[4] = { 0, 0, 0, 0 };
	char *fw_data = NULL;

	success_count = 0;

	dev_info(chip->dev, "[rx1619] [%s] enter \n", __func__);

	if (area == BOOT_AREA) {
		addr = 0;
		g_fw_data_lenth = sizeof(fw_data_boot);
		fw_data = fw_data_boot;
	} else if (area == RX_AREA) {
		addr = 256;
		g_fw_data_lenth = sizeof(fw_data_rx);
		fw_data = fw_data_rx;
	} else if (area == TX_AREA) {
		addr = 4864;
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
		addr_h = (u8) (addr >> 8);
		addr_l = (u8) (addr & 0xff);
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

		if ((read_buf[0] == fw_data[i + 0]) &&
		    (read_buf[1] == fw_data[i + 1]) &&
		    (read_buf[2] == fw_data[i + 2]) &&
		    (read_buf[3] == fw_data[i + 3])) {
			success_count++;
		} else {
			j++;
			if (j >= 50) {	//if error adrr >= 50,new IC
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

	dev_err(chip->dev, "error_conut= %d, success_count=%d\n", j,
		success_count);

	dev_info(chip->dev,
		 "(sizeof(fw_data_boot)) = %ld (sizeof(fw_data_rx)) = %ld (sizeof(fw_data_tx)) = %ld \n",
		 (sizeof(fw_data_boot)), (sizeof(fw_data_rx)),
		 (sizeof(fw_data_tx)));

	return success_count;
}

static bool rx1619_push_rx_firmware(struct rx1619_chg *chip)
{
	u16 addr = 256;
	u8 addr_h, addr_l;
	int i = 0;
	int j = 0;
	u8 read_buf[4] = { 0, 0, 0, 0 };
	char *fw_data = NULL;
	int pass_count = 0;

	if (chip->rx_fw_bin == NULL) {
		dev_err(chip->dev, "[rx1619] g_chip->rx_fw_bin == NULL \n");
		return false;
	}

	dev_err(chip->dev, "[rx1619] g_chip->rx_fw_bin != NULL \n");
	dev_err(chip->dev, "[rx1619] [%s] download rx enter \n", __func__);

	addr = 256;
	g_fw_data_lenth = chip->rx_fw_len;
	fw_data = chip->rx_fw_bin;

	/************prepare_for_mtp_write************/
	rx1619_write(chip, 0x69, 0x2017);
	rx1619_write(chip, 0x96, 0x2017);
	rx1619_write(chip, 0x66, 0x2017);
	rx1619_write(chip, 0x99, 0x2017);
	rx1619_write(chip, 0xff, 0x2018);
	rx1619_write(chip, 0xff, 0x2019);
	rx1619_write(chip, 0x5a, 0x0001);
	rx1619_write(chip, 0xa5, 0x0003);

	//rx1619_write(chip, 0x19, 0x0018);
	rx1619_write(chip, 0x10, 0x1000);
	rx1619_write(chip, 0x3e, 0x1130);
	/************prepare_for_mtp_write************/

	msleep(20);

	/************write_mtp_addr************/
	addr_h = (u8) (addr >> 8);
	addr_l = (u8) (addr & 0xff);
	rx1619_write(chip, addr_h, 0x0010);
	rx1619_write(chip, addr_l, 0x0011);
	/************write_mtp_addr************/

	/************enable write************/
	rx1619_write(chip, 0x01, 0x0017);
	rx1619_write(chip, 0x30, 0x1000);
	rx1619_write(chip, 0x5a, 0x001a);
	/************enable write************/

	/************write data************/
	for (i = 0; i < g_fw_data_lenth; i += 4) {
		rx1619_write(chip, 0xff & (~fw_data[i + 3]), 0x0012);
		usleep_range(1000, 1100);
		rx1619_write(chip, 0xff & (~fw_data[i + 2]), 0x0012);
		usleep_range(1000, 1100);
		rx1619_write(chip, 0xff & (~fw_data[i + 1]), 0x0012);
		usleep_range(1000, 1100);
		rx1619_write(chip, 0xff & (~fw_data[i + 0]), 0x0012);
		usleep_range(1000, 1100);
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

	dev_err(chip->dev, "[rx1619] [%s] download rx exit \n", __func__);

	msleep(500);

	dev_info(chip->dev, "[rx1619] [%s] check rx enter \n", __func__);
	pass_count = 0;

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
		addr_h = (u8) (addr >> 8);
		addr_l = (u8) (addr & 0xff);
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

		if ((read_buf[0] == (0xff & (~fw_data[i + 0]))) &&
		    (read_buf[1] == (0xff & (~fw_data[i + 1]))) &&
		    (read_buf[2] == (0xff & (~fw_data[i + 2]))) &&
		    (read_buf[3] == (0xff & (~fw_data[i + 3])))) {
			pass_count++;
		} else {
			j++;
			if (j >= 50) {
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

	dev_err(chip->dev,
		"error_conut= %d, pass_count=%d, (sizeof(fw_data_rx)) = %ld \n",
		j, pass_count, (sizeof(fw_data_rx)));

	dev_info(chip->dev, "fw_data version=0x%x, 0x%x, 0x%x, 0x%x \n",
		 0xff & (~fw_data[g_fw_data_lenth - 4]),
		 0xff & (~fw_data[g_fw_data_lenth - 3]),
		 0xff & (~fw_data[g_fw_data_lenth - 2]),
		 0xff & (~fw_data[g_fw_data_lenth - 1]));

	rx_fw_version = fw_data[g_fw_data_lenth - 1];
	rx_bin_project_id_h = fw_data[g_fw_data_lenth - 3];
	rx_bin_project_id_l = fw_data[g_fw_data_lenth - 2];

	dev_info(chip->dev, "[rx1619] [%s] check rx exit \n", __func__);

	if (g_fw_data_lenth == (pass_count * 4)) {
		dev_info(chip->dev, "[rx1619] [%s] Push rx success! \n",
			 __func__);
		return true;
	} else {
		dev_info(chip->dev, "[rx1619] [%s] Push rx fail! \n", __func__);
		return false;
	}
}

bool rx1619_check_i2c_is_ok(struct rx1619_chg *chip)
{
	u8 read_data = 0;

	rx1619_write(chip, 0x88, 0x0000);
	msleep(10);
	rx1619_read(chip, &read_data, 0x0000);

	if (read_data == 0x88) {
		return true;
	} else {
		return false;
	}
}

#define RETRY_COUNT 2
bool rx1619_download_area_firmware(struct rx1619_chg *chip, u8 area)
{
	bool status = false;
	u8 count = 0;
	u32 ret = 0;

	dev_err(chip->dev, "[rx1619] [%s] enter , area=%d \n", __func__, area);

	if (rx1619_check_firmware_version(chip, area) == true) {
		dev_err(chip->dev,
			"[rx1619] [%s] Same fw Version, no upgrade required! \n",
			__func__);
		return true;
	}
	msleep(20);

      RETRY:
	count++;
	dev_err(chip->dev, "count=%d \n", count);
	status = rx1619_update_fw_confirm_data(chip, area, 0x00);	//clear 0
	if (!status) {
		return false;
	}
	rx1619_download_firmware(chip, area);
	msleep(20);
	ret = rx1619_check_firmware(chip, area);
	dev_err(chip->dev,
		"download_area:%d  success_cout=%d, g_fw_data_lenth=%d\n", area,
		ret, g_fw_data_lenth);
	if (g_fw_data_lenth == (ret * 4)) {
		status = rx1619_update_fw_confirm_data(chip, area, 0x66);	//0x66(check success)
		if (!status) {
			dev_err(chip->dev,
				"area:%d update firmware confirm data fail! \n",
				area);
			return false;
		} else {
			dev_info(chip->dev, "download area:%d fw success! \n",
				 area);
		}
	} else {
		if (count > RETRY_COUNT) {
			dev_info(chip->dev, "download area:%d fw failed! \n",
				 area);
			return false;
		} else {
			goto RETRY;
		}
	}

	return true;
}

bool rx1619_onekey_download_firmware(struct rx1619_chg *chip)
{
	bool ret = false;

	dev_err(chip->dev, "[rx1619] [%s] enter \n", __func__);

	if (!rx1619_check_i2c_is_ok(chip)) {
		dev_err(chip->dev, " i2c error! \n");
		return false;
	}

	ret = rx1619_download_area_firmware(chip, BOOT_AREA);
	if (!ret) {
		dev_err(chip->dev, "BOOT_AREA download fail!!! \n");
		return false;
	}

	msleep(20);

	ret = rx1619_download_area_firmware(chip, RX_AREA);
	if (!ret) {
		dev_err(chip->dev, "RX_AREA download fail!!! \n");
		return false;
	}

	msleep(20);

	ret = rx1619_download_area_firmware(chip, TX_AREA);
	if (!ret) {
		dev_err(chip->dev, "TX_AREA download fail!!! \n");
		return false;
	}

	msleep(20);

	return true;
}

void rx1619_dump_reg(void)
{
	u8 data[32] = { 0 };

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
	union power_supply_propval val = { 0, };

	val.intval = mA;

	if (chip->wireless_by_usbin) {
		if (!chip->usb_psy)
			chip->usb_psy = power_supply_get_by_name("usb");

		if (!chip->usb_psy) {
			dev_err(chip->dev, "no usb_psy,return\n");
			return;
		}
		power_supply_set_property(chip->usb_psy,
						POWER_SUPPLY_PROP_CURRENT_MAX,
					&val);
	} else {
		/* set bbc icl only */
		if (!chip->dc_psy)
			chip->dc_psy = power_supply_get_by_name("dc");

		if (!chip->dc_psy) {
			dev_err(chip->dev, "no dc_psy,return\n");
			return;
		}

		power_supply_set_property(chip->dc_psy,
						POWER_SUPPLY_PROP_CURRENT_MAX,
						 &val);
	}

	dev_info(chip->dev, "[rx1619] [%s] [rx1619] set icl: %d\n",
		 __func__, val.intval);
}

void set_usb_type_current(struct rx1619_chg *chip, u8 data)
{
	int i = 0;
	int uA = 0;
	//int vol_now = 0;
	union power_supply_propval val = { 0, };

	dev_info(chip->dev, "[%s] data=0x%x \n", __func__, data);

	switch (data) {
	case ADAPTER_NONE:		//other charger
		if ((chip->auth == 0) && (chip->epp == 0)) {	//bpp and auth fail
			rx1619_set_pmi_icl(chip, DC_OTHER_CURRENT);
			dev_info(chip->dev,
				 "[rx1619] [%s] bpp and no id---800mA \n",
				 __func__);
		} else {	//auth ok
			rx1619_set_pmi_icl(chip, DC_LOW_CURRENT);
			dev_info(chip->dev,
				 "[rx1619] [%s] bpp and id ok---200mA \n",
				 __func__);
		}
		break;

	case ADAPTER_SDP:
		rx1619_set_pmi_icl(chip, DC_SDP_CURRENT);
		chip->target_vol = ADAPTER_DEFAULT_VOL;
		chip->target_curr = DC_SDP_CURRENT;
		break;

	case ADAPTER_CDP:
	case ADAPTER_DCP:
		rx1619_set_pmi_icl(chip, DC_BPP_AUTH_FAIL_CURRENT);
		chip->target_vol = ADAPTER_DEFAULT_VOL;
		chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;
		break;

	case ADAPTER_QC2:		//QC2-other -- 6.5W
		for (i = 0; i <= 8; i++) {
			uA = (DC_LOW_CURRENT + 100000 * i);
			rx1619_set_pmi_icl(chip, uA);
			msleep(100);
		}
		chip->target_vol = ADAPTER_BPP_QC2_VOL;
		chip->target_curr = uA;
		break;

	case ADAPTER_QC3:		//QC3-other -- 10W
	case ADAPTER_PD:		//PD-other -- 10W
		if (chip->epp) {
			for (i = 0; i <= 8; i++) {
				uA = (DC_LOW_CURRENT + 200000 * i);
				rx1619_set_pmi_icl(chip, uA);
				msleep(100);
			}
			chip->target_vol = ADAPTER_EPP_QC3_VOL;
			chip->target_curr = uA;
		} else {
			for (i = 0; i <= 9; i++) {
				uA = (DC_LOW_CURRENT + 100000 * i);
				rx1619_set_pmi_icl(chip, uA);
				msleep(100);
			}
			chip->target_vol = ADAPTER_BPP_LIMIT_VOL;
			chip->target_curr = uA;
		}
		chip->last_qc3_icl = chip->target_curr;
		break;

	case ADAPTER_AUTH_FAILED:		//fail charger
		break;

	case ADAPTER_XIAOMI_QC3:		//QC3-27W(20W)
	case ADAPTER_XIAOMI_PD:		//PD-27W(20W)
	case ADAPTER_ZIMI_CAR_POWER:		//PD-27W(20W)
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
						  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
						  &val);
			msleep(200);
		}
		/* for usb-in design, set max usb icl to 1.8A */
		for (i = 0; i <= 10; i++) {
			uA = (USB_20W_PLUS_BASE_CURRENT_UA + 100000 * i);
			rx1619_set_pmi_icl(chip, uA);
			msleep(100);
		}
		chip->target_curr = uA;
		chip->last_icl = chip->target_curr;
		dev_info(chip->dev, "[%s] 27W adapter \n", __func__);
		break;

	case ADAPTER_XIAOMI_PD_40W:		//40w
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_45W:
	case ADAPTER_XIAOMI_PD_60W:
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
		dev_info(chip->dev, "[30W]ready to enable cp\n");
		if (chip->is_urd_device) {
			for (i = 0; i <= 5; i++) {
				uA = (USB_20W_PLUS_BASE_CURRENT_UA +
				      100000 * i);
				rx1619_set_pmi_icl(chip, uA);
				msleep(100);
			}
		} else {
			/* for usb-in design, set max usb icl to 1.8A */
			for (i = 0; i <= 4; i++) {
				uA = (USB_20W_PLUS_BASE_CURRENT_UA +
				      200000 * i);
				rx1619_set_pmi_icl(chip, uA);
				msleep(100);
			}
		}
		if (chip->epp && chip->auth && chip->wireless_psy) {
			val.intval = 1;
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
						  &val);
			msleep(200);
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
	case ADAPTER_NONE:	//other charger
		if ((chip->auth == 0) && (chip->epp == 0)) {	//bpp and auth fail
			chip->target_curr = DC_OTHER_CURRENT;
			dev_info(chip->dev,
				 "[rx1619] [%s] bpp and no id---800mA \n",
				 __func__);
		} else {	//auth ok
			chip->target_curr = DC_LOW_CURRENT;
			dev_info(chip->dev,
				 "[rx1619] [%s] bpp and id ok---200mA \n",
				 __func__);
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

	case ADAPTER_QC2:	//QC2-other -- 6.5W
		chip->target_vol = ADAPTER_BPP_QC2_VOL;
		chip->target_curr = DC_QC2_CURRENT;	//1A
		break;

	case ADAPTER_QC3:	//QC3-other -- 10W
	case ADAPTER_PD:	//PD-other -- 10W
		if (chip->epp) {
			chip->target_vol = ADAPTER_EPP_QC3_VOL;
			chip->target_curr = DC_QC3_CURRENT;	//1.8A
		} else {
			chip->target_vol = ADAPTER_DEFAULT_VOL;
			chip->target_curr = DC_QC3_BPP_CURRENT;	//0.9A
		}
		break;

	case ADAPTER_AUTH_FAILED:	//fail charger
		break;

	case ADAPTER_XIAOMI_QC3:	//QC3-27W(20W)
	case ADAPTER_XIAOMI_PD:	//PD-27W(20W)
	case ADAPTER_ZIMI_CAR_POWER:	//PD-27W(20W)
		chip->target_vol = ADAPTER_EPP_MI_VOL;
		/* for usb-in design, set max usb icl to 1.8A */
		chip->target_curr = USB_20W_PLUS_CURRENT_UA;	//1.8A
		break;

	case ADAPTER_XIAOMI_PD_40W:	//40w
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_45W:
	case ADAPTER_XIAOMI_PD_60W:
		chip->target_vol = ADAPTER_EPP_MI_VOL;
		/* for usb-in design, set max usb icl to 1.8A */
		chip->target_curr = 1500000;	//1.8A
		break;

	default:
		break;
	}
}

static void rx_set_charging_param(struct rx1619_chg *chip)
{
	union power_supply_propval val = { 0, };
	union power_supply_propval wk_val = { 0, };
	int soc = 0, health = 0, batt_sts = 0;
	int vol_now = 0, cur_now = 0, dc_level = 0;
	int iout = 0, ret = 0;
	bool vout_change = false;
	int last_icl = 0;
	int last_vin = 0;

	get_usb_type_current(chip, g_USB_TYPE);

	if (!chip->batt_psy) {
		ret = rx_get_property_names(chip);
		if (ret < 0)
			return;
	}

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

	dev_info(chip->dev,
		 "soc:%d,vol_now:%d,cur_now:%d,health:%d, bat_status:%d, dc_level:%d\n",
		 soc, vol_now, cur_now, health, batt_sts, dc_level);
	/*epp 10W */
	if ((g_USB_TYPE >= 6 && g_USB_TYPE <= 7) && (chip->epp)) {
		dev_info(chip->dev, "standard epp logic\n");
		if (soc >= 97)
			chip->target_curr = DC_BPP_CURRENT;
		if (soc == 100)
			chip->target_curr = DC_SDP_CURRENT;
		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;

		if (chip->target_curr != chip->last_qc3_icl) {
			dev_info(chip->dev,
				 "qc3_epp, set new icl: %d, last_icl: %d\n",
				 chip->target_curr, chip->last_qc3_icl);
			chip->last_qc3_icl = chip->target_curr;
			rx1619_set_pmi_icl(chip, chip->target_curr);
			msleep(100);
		}
	}
	/*epp plus */
	if (g_USB_TYPE >= 9) {
		if (chip->is_pan_tx) {
			schedule_delayed_work(&chip->pan_tx_work,
					      msecs_to_jiffies(0));
			goto out;
		}

		if (chip->is_voice_box_tx) {
			dev_info(chip->dev, "enter voice work\n");
			schedule_delayed_work(&chip->voice_tx_work, msecs_to_jiffies(0));
			goto out;
		}
		if ((dc_level >= HIGH_THERMAL_LEVEL_THR) || (soc >= LIMIT_SOC)) {
			dev_info(chip->dev,
				 "set vin 12V for dc_level:%d, soc:%d\n",
				 dc_level, soc);
			//chip->target_vol = EPP_VOL_THRESHOLD;
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
		} else if (iout > LIMIT_EPP_IOUT
			   && chip->epp_exchange == EXCHANGE_10V) {
			chip->count_15v++;
			chip->count_10v = 0;
		} else {
			chip->count_10v = 0;
			chip->count_15v = 0;
		}
		if (chip->count_10v > ICL_EXCHANGE_COUNT ||
		    (chip->epp_exchange == EXCHANGE_10V
		     && chip->count_15v <= ICL_EXCHANGE_COUNT)) {
			dev_info(chip->dev,
				 "iout less than 500mA ,set vin to 12V\n");
			//chip->target_vol = EPP_VOL_THRESHOLD;
			chip->epp_exchange = EXCHANGE_10V;
		} else if (chip->count_15v > (ICL_EXCHANGE_COUNT))
			chip->epp_exchange = EXCHANGE_15V;
		/* function end */
		switch (chip->status) {
		case NORMAL_MODE:
			if (soc >= TAPER_SOC) {
				//chip->target_curr = min(DC_BPP_CURRENT, chip->target_curr);
				chip->target_vol = EPP_VOL_THRESHOLD;
				dev_info(chip->dev, "set curr to %d\n",
					 chip->target_curr);
			}
			if (soc >= FULL_SOC) {
				chip->status = TAPER_MODE;
				chip->target_vol = EPP_VOL_THRESHOLD;
				//chip->target_curr = min(DC_SDP_CURRENT, chip->target_curr);
				dev_info(chip->dev,
					 "ready to taper_mode ,set vin to %d, curr to %d\n",
					 chip->target_vol, chip->target_curr);
			}
			break;
		case TAPER_MODE:
			chip->target_vol = EPP_VOL_THRESHOLD;
			//chip->target_curr = min(DC_SDP_CURRENT, chip->target_curr);

			dev_info(chip->dev,
				 "ready to full_mode ,set vin to %d, curr to %d\n",
				 chip->target_vol, chip->target_curr);
			if (soc == FULL_SOC
			    && batt_sts == POWER_SUPPLY_STATUS_FULL)
				chip->status = FULL_MODE;
			else if (soc < FULL_SOC - 1)
				chip->status = NORMAL_MODE;
			break;
		case FULL_MODE:
			dev_info(chip->dev, "charge full set Vin 11V\n");
			chip->target_vol = ADAPTER_EPP_QC3_VOL;
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;

			if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
				dev_info(chip->dev,
					 "full mode -> recharge mode\n");
				chip->status = RECHG_MODE;
				chip->target_curr = DC_FULL_CURRENT;
			}
			break;
		case RECHG_MODE:
			if (batt_sts == POWER_SUPPLY_STATUS_FULL) {
				dev_info(chip->dev,
					 "recharge mode -> full mode\n");
				chip->status = FULL_MODE;
				chip->target_curr = SCREEN_OFF_FUL_CURRENT;
				chip->target_vol = ADAPTER_EPP_QC3_VOL;
				if (chip->wireless_psy) {
					wk_val.intval = 0;
					power_supply_set_property(chip->
								  wireless_psy,
								  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
								  &wk_val);
				}
				break;
			}
			dev_info(chip->dev, "recharge mode set icl to 350mA\n");
			chip->target_vol = ADAPTER_EPP_QC3_VOL;
			chip->target_curr = DC_FULL_CURRENT;

			if (chip->wireless_psy) {
				wk_val.intval = 1;
				power_supply_set_property(chip->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							  &wk_val);
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
			//chip->target_vol = min(EPP_VOL_THRESHOLD, chip->target_vol);
			chip->target_curr =
			    min(DC_SDP_CURRENT, chip->target_curr);
			break;
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
			//chip->target_vol = min(EPP_VOL_THRESHOLD, chip->target_vol);
			chip->target_curr =
			    min(SCREEN_OFF_FUL_CURRENT, chip->target_curr);
			break;
		case POWER_SUPPLY_HEALTH_COLD:
		case POWER_SUPPLY_HEALTH_HOT:
			//chip->target_vol = min(EPP_VOL_THRESHOLD, chip->target_vol);
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;
			break;
		default:
			break;
		}
		last_vin = chip->last_vin;
		last_icl = chip->last_icl;
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
							  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
							  &val);
			}
		}
		if ((chip->target_curr > 0
		     && chip->target_curr != chip->last_icl)
		    || vout_change) {
			chip->last_icl = chip->target_curr;
			rx1619_set_pmi_icl(chip, chip->target_curr);
		}
	}
      out:
	dev_info(chip->dev,
		 "status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d\n",
		 chip->status, chip->target_vol, chip->target_curr, last_vin,
		 last_icl);
}

static void rx1619_dc_check_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       dc_check_work.work);

	dev_info(chip->dev, "[rx1619] dc present: %d\n", chip->dcin_present);
	if (chip->dcin_present) {
		chip->ss = 1;
		dev_info(chip->dev, "dcin present, quit dc check work\n");
		return;
	} else {
		chip->ss = 0;
		dev_info(chip->dev,
			 "dcin no present, continue dc check work\n");
		schedule_delayed_work(&chip->dc_check_work,
				      msecs_to_jiffies(2500));
	}
	if (chip->wireless_psy)
		power_supply_changed(chip->wireless_psy);
}

static void rx1619_cmd_timeout_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       cmd_timeout_work.work);

	dev_info(chip->dev, "[%s] Cmd timeout set 700mA \n", __func__);
	rx1619_set_pmi_icl(chip, DC_BPP_AUTH_FAIL_CURRENT);
	chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;

}

static void rx1619_rx_first_boot(struct work_struct *work)
{
	struct rx1619_chg *chip =
		container_of(work, struct rx1619_chg,
					rx_first_boot.work);
	g_rx1619_first_flag = true;
	if (chip->wip_psy)
		power_supply_changed(chip->wip_psy);
}

#define CHARGING_PERIOD_S	10
static void rx_monitor_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       chg_monitor_work.work);
	rx_charging_info(chip);

	rx_set_charging_param(chip);

	schedule_delayed_work(&chip->chg_monitor_work, CHARGING_PERIOD_S * HZ);
}

static void rx1619_fw_download_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       fw_download_work.work);

	union power_supply_propval val = { 0, };
	bool is_valid_fw = true;

	if (chip->fw_update) {
		dev_info(chip->dev, "[rx1619] [%s] FW Update is on going!\n",
			 __func__);
		return;
	}

	power_supply_get_property(chip->dc_psy, POWER_SUPPLY_PROP_ONLINE, &val);
	if (!val.intval) {
		pm_stay_awake(chip->dev);
		chip->fw_update = true;
		rx_set_reverse_gpio(chip, true);
		msleep(100);
		rx1619_check_firmware_version(chip, BOOT_AREA);
		rx1619_check_firmware_version(chip, RX_AREA);
		rx1619_check_firmware_version(chip, TX_AREA);
		if (g_fw_rx_id == FW_EMPTY_CODE
		    || g_fw_rx_id == FW_ERROR_CODE
		    || g_fw_tx_id == FW_ERROR_CODE
		    || g_fw_boot_id == FW_ERROR_CODE)
			is_valid_fw = false;
		chip->chip_ok = rx1619_check_i2c_is_ok(chip);
		chip->fw_version = g_fw_rx_id;
		dev_info(chip->dev, "[rx1619] %s: FW Version is 0x%x chip_ok:%d\n",
			 __func__, g_fw_rx_id, chip->chip_ok);

		if (is_valid_fw && (g_fw_rx_id >= FW_VERSION)) {
			dev_info(chip->dev,
				 "[rx1619] %s: FW Version correct so skip upgrade\n",
				 __func__);
		} else {
#ifndef CONFIG_FACTORY_BUILD
			dev_info(chip->dev, "[rx1619] %s: FW download start\n",
				 __func__);
			if (!rx1619_onekey_download_firmware(chip))
				dev_err(chip->dev,
					"[rx1619] [%s] program fw failed!\n",
					__func__);
			else {
				dev_info(chip->dev,
					 "[rx1619] [%s] FW Update Success!\n",
					 __func__);
				boot_fw_version =
				    (~fw_data_boot[sizeof(fw_data_boot) - 1]) &
				    0xff;
				tx_fw_version =
				    (~fw_data_tx[sizeof(fw_data_tx) - 1]) &
				    0xff;
				rx_fw_version =
				    (~fw_data_rx[sizeof(fw_data_rx) - 1]) &
				    0xff;
				dev_info(chip->dev,
					 "boot_fw_version=0x%x, tx_fw_version=0x%x, rx_fw_version=0x%x\n",
					 boot_fw_version, tx_fw_version,
					 rx_fw_version);
				chip->fw_version = g_fw_rx_id;
			}
#else
			dev_info(chip->dev,
				 "[rx1619] %s: factory build, don't update\n",
				 __func__);
#endif
		}
		rx_set_reverse_gpio(chip, false);
		chip->fw_update = false;
		pm_relax(chip->dev);
	} else
		dev_err(chip->dev,
			"%s: Skip FW download due to wireless charging\n",
			__func__);
}

static void rx1619_pan_tx_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       pan_tx_work.work);

	int soc = 0, batt_sts = 0, dc_level = 0;
	int last_icl = 0;
	int last_vin = 0;
	int ret = 0;
	unsigned int vout = 0;
	bool vout_change = false;
	union power_supply_propval val = { 0, };
	union power_supply_propval wk_val = { 0, };

	chip->target_vol = ADAPTER_EPP_MI_VOL;
	chip->target_curr = 2000000;

	if (chip->batt_psy) {
		power_supply_get_property(chip->batt_psy,
					  POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(chip->batt_psy,
					  POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;

		power_supply_get_property(chip->batt_psy,
					  POWER_SUPPLY_PROP_DC_THERMAL_LEVELS,
					  &val);
		dc_level = val.intval;
	}

	dev_info(chip->dev, "soc:%d, dc_level:%d, bat_status:%d\n",
		 soc, dc_level, batt_sts);

	if (dc_level) {
		dev_info(chip->dev, "Disable bq, dc_level:%d\n", dc_level);
		chip->target_vol = ADAPTER_EPP_QC3_VOL;
	}

	switch (chip->status) {
	case NORMAL_MODE:
		if (soc >= FULL_SOC)
			chip->status = TAPER_MODE;
		break;
	case TAPER_MODE:
		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			chip->status = FULL_MODE;
		else if (soc < FULL_SOC - 1)
			chip->status = NORMAL_MODE;
		break;
	case FULL_MODE:
		dev_info(chip->dev, "[pan]charge full set Vin 11V\n");
		chip->target_vol = ADAPTER_EPP_QC3_VOL;
		chip->target_curr = SCREEN_OFF_FUL_CURRENT;

		if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
			dev_info(chip->dev,
				 "[pan]full mode -> recharge mode\n");
			chip->status = RECHG_MODE;
			chip->target_curr = DC_LOW_CURRENT;
		}
		break;
	case RECHG_MODE:
		if (batt_sts == POWER_SUPPLY_STATUS_FULL) {
			dev_info(chip->dev,
				 "[pan]recharge mode -> full mode\n");
			chip->status = FULL_MODE;
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;
			if (chip->wireless_psy) {
				wk_val.intval = 0;
				power_supply_set_property(chip->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							  &wk_val);
			}
			break;
		}

		dev_info(chip->dev, "[pan]recharge mode set icl to 350mA\n");
		chip->target_vol = ADAPTER_EPP_QC3_VOL;
		chip->target_curr = DC_LOW_CURRENT;

		if (chip->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
						  &wk_val);
		}
		break;
	default:
		break;
	}

	last_vin = chip->last_vin;
	last_icl = chip->last_icl;

	if (chip->target_vol > 0 && chip->target_vol != chip->last_vin) {
		if (chip->target_vol == ADAPTER_EPP_MI_VOL) {
			chip->disable_bq = false;
			ret = rx1619_set_vout(chip, chip->target_vol);
			if (chip->wireless_psy) {
				val.intval = 1;
				power_supply_set_property(chip->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
							  &val);
			}
		} else if (chip->target_vol == ADAPTER_EPP_QC3_VOL) {
			chip->disable_bq = true;
			/* enable 8150b charge */
			if (chip->batt_psy) {
				val.intval = 1;
				power_supply_set_property(chip->batt_psy,
							  POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED,
							  &val);
			}
			if (chip->wireless_psy) {
				val.intval = 0;
				power_supply_set_property(chip->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
							  &val);
			}

			vout = rx1619_get_rx_vout(chip);
			while (vout > ADAPTER_EPP_QC3_VOL) {
				vout = vout - 1000;
				ret = rx1619_set_vout(chip, vout);
				msleep(200);
			}
			ret = rx1619_set_vout(chip, ADAPTER_EPP_QC3_VOL);
		}
		chip->last_vin = chip->target_vol;
		vout_change = true;
	}

	if ((chip->target_curr > 0 && chip->target_curr != chip->last_icl)
	    || vout_change) {
		chip->last_icl = chip->target_curr;
		rx1619_set_pmi_icl(chip, chip->target_curr);
	}

	dev_info(chip->dev,
		 "chip->status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d, bq_dis:%d\n",
		 chip->status, chip->target_vol, chip->target_curr,
		 chip->last_vin, chip->last_icl, chip->disable_bq);
}
static int rx1619_get_effective_fcc(struct rx1619_chg *chip)
{
	int effective_fcc_val = 0;

	if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("FCC");

	if (!chip->fcc_votable)
		return -EINVAL;

	effective_fcc_val = get_effective_result(chip->fcc_votable);
	effective_fcc_val = effective_fcc_val / 1000;
	dev_info(chip->dev, "effective_fcc: %d\n", effective_fcc_val);
	return effective_fcc_val;
}

#define VOICE_LIMIT_FCC_VOTER "VOICE_LIMIT_FCC_VOTER"
#define VOICE_LIMIT_FCC_1A_VOTER "VOICE_LIMIT_FCC_1A_VOTER"
#define VOICE_CURRENT_LIMIT_HOT_UPPER_TEMP 402
#define VOICE_CURRENT_LIMIT_HOT_LOWER_TEMP 400
#define VOICE_CURRENT_LIMIT_WARM_UPPER_TEMP 375
#define VOICE_CURRENT_LIMIT_WARM_LOWER_TEMP 367
#define VOICE_FCC_HOT_LIMIT_MA 1000
#define VOICE_FCC_WARM_LIMIT_MA 2600
#define VOICE_MAX_ICL_UA 2000000

static void rx1619_voice_tx_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
		 container_of(work, struct rx1619_chg,
					voice_tx_work.work);

	int soc = 0, batt_sts = 0, dc_level = 0, batt_temp = 0;
	int last_icl = 0;
	int last_vin = 0;
	int ret = 0;
	int effective_fcc = 0;
	unsigned int  vout = 0;
	bool vout_change = false;
	union power_supply_propval val = {0, };
	union power_supply_propval wk_val = {0, };

	chip->target_vol = ADAPTER_EPP_MI_VOL;
	chip->target_curr = VOICE_MAX_ICL_UA;

	if (chip->batt_psy) {
		power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;

		power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_DC_THERMAL_LEVELS, &val);
		dc_level = val.intval;

		power_supply_get_property(chip->batt_psy,
				POWER_SUPPLY_PROP_TEMP, &val);
		batt_temp = val.intval;
	}

	dev_info(chip->dev, "soc:%d, dc_level:%d, bat_status:%d, batt_temp:%d\n",
			soc, dc_level, batt_sts, batt_temp);

	if (batt_temp >= VOICE_CURRENT_LIMIT_HOT_UPPER_TEMP) {
		dev_info(chip->dev, "[voice]Tbat limit fcc 1A\n");
		effective_fcc = rx1619_get_effective_fcc(chip);
		if (chip->fcc_votable) {
			effective_fcc = VOICE_FCC_HOT_LIMIT_MA;
			vote(chip->fcc_votable, VOICE_LIMIT_FCC_1A_VOTER,
				true, effective_fcc * 1000);
		}
	} else if (batt_temp < VOICE_CURRENT_LIMIT_HOT_LOWER_TEMP) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, VOICE_LIMIT_FCC_1A_VOTER,
				false, 0);
	}

	if (batt_temp >= VOICE_CURRENT_LIMIT_WARM_UPPER_TEMP) {
		dev_info(chip->dev, "[voice]Tbat limit fcc 3.2A\n");
		effective_fcc = rx1619_get_effective_fcc(chip);
		if (chip->fcc_votable) {
			effective_fcc = VOICE_FCC_WARM_LIMIT_MA;
			vote(chip->fcc_votable, VOICE_LIMIT_FCC_VOTER,
				true, effective_fcc * 1000);
		}
	} else if (batt_temp < VOICE_CURRENT_LIMIT_WARM_LOWER_TEMP) {
		if (chip->fcc_votable)
			vote(chip->fcc_votable, VOICE_LIMIT_FCC_VOTER,
				false, 0);
	}

	if (dc_level > 2) {
		dev_info(chip->dev, "[voice]Disable bq, dc_level:%d\n", dc_level);
		chip->target_vol = ADAPTER_EPP_QC3_VOL;
	}

	switch (chip->status) {
	case NORMAL_MODE:
		if (soc >= TAPER_SOC)
			chip->status = TAPER_MODE;
		break;
	case TAPER_MODE:
		dev_info (chip->dev, "[voice]taper mode set Vin 11V\n");
		chip->target_vol = ADAPTER_EPP_QC3_VOL;
		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			chip->status = FULL_MODE;
		else if (soc < 95)
			chip->status = NORMAL_MODE;
		break;
	case FULL_MODE:
		dev_info (chip->dev, "[voice]charge full set Vin 11V\n");
		chip->target_vol = ADAPTER_EPP_QC3_VOL;
		chip->target_curr = SCREEN_OFF_FUL_CURRENT;

		if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
			dev_info (chip->dev, "[voice]full mode -> recharge mode\n");
			chip->status = RECHG_MODE;
			chip->target_curr = DC_LOW_CURRENT;
		}
		break;
	case RECHG_MODE:
		if (batt_sts == POWER_SUPPLY_STATUS_FULL) {
			dev_info (chip->dev, "[voice]recharge mode -> full mode\n");
			chip->status = FULL_MODE;
			chip->target_curr = SCREEN_OFF_FUL_CURRENT;
			if (chip->wireless_psy) {
				wk_val.intval = 0;
				power_supply_set_property(chip->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
			}
			break;
		}

		dev_info (chip->dev, "[voice]recharge mode set icl to 350mA\n");
		chip->target_vol = ADAPTER_EPP_QC3_VOL;
		chip->target_curr = DC_LOW_CURRENT;

		if (chip->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(chip->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
		}
		break;
	default:
		break;
	}

	last_vin = chip->last_vin;
	last_icl = chip->last_icl;

	if (chip->target_vol > 0 && chip->target_vol != chip->last_vin) {
		if (chip->target_vol == ADAPTER_EPP_MI_VOL) {
			chip->disable_bq = false;
			ret = rx1619_set_vout(chip, chip->target_vol);
			if (chip->wireless_psy) {
				val.intval = 1;
				power_supply_set_property(chip->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			}
		} else if (chip->target_vol == ADAPTER_EPP_QC3_VOL) {
			chip->disable_bq = true;
			/* enable 8150b charge */
			if (chip->batt_psy) {
				val.intval = 1;
				power_supply_set_property(chip->batt_psy,
					POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLED, &val);
			}
			if (chip->wireless_psy) {
				val.intval = 0;
				power_supply_set_property(chip->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			}

			vout = rx1619_get_rx_vout(chip);
			while (vout > ADAPTER_EPP_QC3_VOL) {
				vout = vout - 1000;
				ret = rx1619_set_vout(chip, vout);
				msleep(200);
			}
			ret = rx1619_set_vout(chip, ADAPTER_EPP_QC3_VOL);
		}
		chip->last_vin = chip->target_vol;
		vout_change = true;
	}

	if ((chip->target_curr > 0 && chip->target_curr != chip->last_icl)
		|| vout_change) {
		chip->last_icl = chip->target_curr;
		rx1619_set_pmi_icl(chip, chip->target_curr);
	}

	dev_info(chip->dev, "di->status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d, bq_dis:%d\n",
			chip->status, chip->target_vol, chip->target_curr, chip->last_vin, chip->last_icl, chip->disable_bq);
}


static void rx_chg_detect_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       chg_detect_work.work);
	union power_supply_propval val = { 0, };
	union power_supply_propval pc_val = { 0, };
	union power_supply_propval wk_val = { 0, };
	int rc;

	dev_info(chip->dev, "[idt] enter %s\n", __func__);

	rc = rx_get_property_names(chip);
	if (rc < 0)
		return;

	g_chip = chip;
	power_supply_get_property(chip->usb_psy,
				  POWER_SUPPLY_PROP_ONLINE, &val);
	power_supply_get_property(chip->pc_port_psy,
				  POWER_SUPPLY_PROP_ONLINE, &pc_val);
	if (val.intval || pc_val.intval) {
		dev_info(chip->dev,
			 "usb_online:%d, pc online:%d set chip disable\n",
			 val.intval, pc_val.intval);
		rx_set_enable_mode(chip, 0);
		schedule_delayed_work(&chip->fw_download_work, 1 * HZ);
		return;
	}

	if (chip->dc_psy) {
		power_supply_get_property(chip->dc_psy,
					  POWER_SUPPLY_PROP_ONLINE, &val);
		dev_info(chip->dev, "dc_online %d\n", val.intval);
		if (val.intval && chip->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
						  &wk_val);

			if (!chip->power_off_mode) {
				rx_set_enable_mode(chip, false);
				usleep_range(20000, 25000);
				rx_set_enable_mode(chip, true);
			}

			schedule_delayed_work(&chip->wireless_int_work,
					      msecs_to_jiffies(30));
		}
	}
}

static void reverse_chg_sent_state_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       reverse_sent_state_work.work);

	union power_supply_propval val = { 0, };

	if (chip->wireless_psy) {
		val.intval = chip->is_reverse_chg;
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_REVERSE_CHG_STATE,
					  &val);
		dev_info(chip->dev, "sent tx_mode_uevent\n");
		power_supply_changed(chip->wireless_psy);
	} else
		dev_err(chip->dev, "get wls property error\n");
}

static void reverse_chg_state_set_work(struct work_struct *work)
{
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
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
	struct rx1619_chg *chip = container_of(work, struct rx1619_chg,
					       reverse_dping_state_work.work);
	int ret;

	dev_info(chip->dev, "tx mode fault and disable reverse charging\n");
	ret = rx_set_reverse_chg_mode(chip, false);
	chip->is_reverse_chg = 2;
	schedule_delayed_work(&chip->reverse_sent_state_work, 0);
	return;
}

/* power good work */
#define REVERSE_TEST_READY_CHECK_DELAY_MS 8000
static void rx1619_wpc_det_work(struct work_struct *work)
{
	struct rx1619_chg *chip =
	    container_of(work, struct rx1619_chg, wpc_det_work.work);
	union power_supply_propval val = { 0, };
	int ret = 0;

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "[rx1619] no wireless_psy, return\n");
		return;
	}

	if (gpio_is_valid(chip->power_good_gpio)) {
		ret = gpio_get_value(chip->power_good_gpio);
		if (ret) {
			dev_info(chip->dev,
				 "power_good high, wireless attached\n");
			chip->power_good_flag = 1;
			val.intval = 1;
			schedule_delayed_work(&chip->dc_check_work,
					      msecs_to_jiffies(2500));
		} else {
			dev_info(chip->dev,
				 "power_good low, wireless detached\n");
			cancel_delayed_work(&chip->dc_check_work);
			chip->power_good_flag = 0;
			chip->ss = 2;
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
						  &val);
		}
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN,
					  &val);

		if (chip->wait_for_reverse_test && !chip->power_good_flag) {
			msleep(2000);
			rx_set_reverse_chg_mode(chip, true);
			dev_err(chip->dev, "[ factory reverse test ] wait for factory reverse charging test, power good low\n");
			alarm_start_relative(&chip->reverse_test_ready_alarm,
					     ms_to_ktime
					     (REVERSE_TEST_READY_CHECK_DELAY_MS));
		}

	}
}

static void rx1619_enable_aicl(struct rx1619_chg *chip, bool enable)
{
	union power_supply_propval val = { 0, };
	if (chip->wireless_psy) {
		if (chip->enabled_aicl != enable) {
			val.intval = enable ? 1 : 0;
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_AICL_ENABLE,
						  &val);
			chip->enabled_aicl = enable;
			return;
		}
	}
	return;
}

/************AP->RX************/
//AP set Vout = 0x80
//AP set Iout = 0x81
//charge done = 0x82
//recharge = 0x83
//sent EPT = 0x84
//fod factor = 0x85
//authen req = 0x86
//usb type req = 0x87
//fast charge req = 0x88
//rx fw version = 0x89
//ap private data pkt = 0x8D
/************AP->RX************/

/************RX->AP************/
//RX NONE = 0x00
//RX light screen  = 0x01
//RX cali start  = 0x02
//RX cali ok = 0x03
//RX private id  = 0x04
//RX private authen res  = 0x05
//RX private usb type = 0x06
//RX private fc status  = 0x07
//RX private data error  = 0x0F
/************RX->AP************/
#define REVERSE_CHG_CHECK_DELAY_MS 100000
#define REVERSE_DPING_CHECK_DELAY_MS 10000
#define CMD_TIMEOUT_DELAY_MS 5000
#define REVERSE_CHG_TX_SWITCH_DONE 8
static void rx1619_wireless_int_work(struct work_struct *work)
{
	int i = 0;
	int uA = 0;
	u16 iout = 0;
	u16 vout = 0;
	u8 usb_type;
	u8 rx_rev_data[4] = { 0, 0, 0, 0 };
	u8 data_h, data_l;
	u8 tx_req = 0;
	u8 rx_req = 0;
	u8 g_tx_id_h = 0;
	u8 g_tx_id_l = 0;
	u8 g_shaone_data_h = 0;
	u8 g_shaone_data_l = 0;
	u8 g_fc_status = 0;
	u8 g_uuid_data[4] = { 0 };
	int tx_gpio, ret;
	u8 tx_status, tx_phase;
	u8 ble_flag;
	u8 err_cmd;
	int rc;
	int cnt;
	int fc_flag = 0;
	int vol = 0;

	struct rx1619_chg *chip =
	    container_of(work, struct rx1619_chg, wireless_int_work.work);

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "[rx1619] no wireless_psy, return\n");
		return;
	}

	tx_gpio = rx_get_reverse_chg_mode(chip);
	if (tx_gpio) {
		if (rx1619_is_tx_mode(chip)) {
			rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
			tx_status = rx1619_get_tx_status(chip);
			if (tx_status
			    && tx_status != REVERSE_CHG_TX_SWITCH_DONE) {
				ret = rx_set_reverse_chg_mode(chip, false);
				chip->is_reverse_chg = 2;
				schedule_delayed_work(&chip->
						      reverse_sent_state_work,
						      0);
			} else if (tx_status == REVERSE_CHG_TX_SWITCH_DONE) {
				dev_err(chip->dev,
					"[rx1619] restart because of OTG \n");
			}
			tx_phase = rx1619_get_tx_phase(chip);
			switch (tx_phase) {
			case PING:
				//schedule_delayed_work(&chip->reverse_chg_state_work, 80 * HZ);
				alarm_start_relative(&chip->reverse_chg_alarm,
						     ms_to_ktime
						     (REVERSE_CHG_CHECK_DELAY_MS));
				//cancel_delayed_work(&chip->reverse_dping_state_work);
				rc = alarm_cancel(&chip->reverse_dping_alarm);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't cancel reverse_dping_alarm\n");
				pm_relax(chip->dev);
				if (tx_status == REVERSE_CHG_TX_SWITCH_DONE)
					dev_err(chip->dev, "reverse restart because of otg insertion\n");

				break;
			case TRANSFER:
				//cancel_delayed_work(&chip->reverse_chg_state_work);
				rc = alarm_cancel(&chip->reverse_chg_alarm);
				if (rc < 0)
					dev_err(chip->dev,
						"Couldn't cancel reverse_dping_alarm\n");
				pm_stay_awake(chip->dev);
				/* set reverse charging state to started */
				chip->is_reverse_chg = 4;
				schedule_delayed_work(&chip->
						      reverse_sent_state_work,
						      0);
				dev_info(chip->dev, "tx mode power transfer\n");
				break;
			case POWER_LIM:
				dev_info(chip->dev, "tx mode power limit\n");
				break;
			case CONFIGURE_CNF:
				dev_info(chip->dev, "tx mode CONFIGURE_CNF\n");
				break;

			case CONFIGURE_ERR:
				dev_err(chip->dev, "tx mode CONFIGURE_ERR\n");
				break;
			case REVERSE_TEST_DONE:
				rx_set_reverse_chg_mode(chip, false);
				chip->wait_for_reverse_test = false;
				dev_err(chip->dev, "[ factory reverse test ] receiver reverse test done\n");
				break;
			case REVERSE_TEST_READY:
				dev_err(chip->dev, "[ factory reverse test ] receiver reverse test ready, cancel timer\n");
				alarm_cancel(&chip->reverse_test_ready_alarm);
				break;
			default:
				dev_err(chip->dev, "tx phase invalid\n");
				break;
			}
		}
		return;
	}

	rx1619_read(chip, &rx_req, REG_RX_REV_CMD);	//0x0020
	if (rx_req <= 0) {
		dev_info(chip->dev, "rx cmd error:%d\n", rx_req);
		return;
	}

	dev_info(chip->dev, "rx_req = 0x%x\n", rx_req);

	mutex_lock(&chip->wireless_chg_int_lock);

	//read data
	rx1619_read(chip, &rx_rev_data[0], REG_RX_REV_DATA1);	//0x0021
	rx1619_read(chip, &rx_rev_data[1], REG_RX_REV_DATA2);	//0x0022
	rx1619_read(chip, &rx_rev_data[2], REG_RX_REV_DATA3);	//0x0023
	rx1619_read(chip, &rx_rev_data[3], REG_RX_REV_DATA4);	//0x0024

	dev_err(chip->dev, "[%s] rx_req,rx_rev_data=0x%x,0x%x,0x%x,0x%x,0x%x\n",
		__func__, rx_req, rx_rev_data[0], rx_rev_data[1],
		rx_rev_data[2], rx_rev_data[3]);

	switch (rx_req) {
	case 0x10:		//get epp tx id
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);
		chip->epp_tx_id_h = rx_rev_data[0];	//epp tx id high byte
		chip->epp_tx_id_l = rx_rev_data[1];	//epp tx id low byte

		if (chip->epp_tx_id_l == 0x59) {
			dev_info(chip->dev,
				 "mophie tx, start dc check after 8s\n");
			schedule_delayed_work(&chip->dc_check_work,
					      msecs_to_jiffies(8000));
		} else
			schedule_delayed_work(&chip->dc_check_work,
					      msecs_to_jiffies(2500));

		dev_info(chip->dev, "epp_tx_id_h = 0x%x, epp_tx_id_l = 0x%x\n",
			 chip->epp_tx_id_h, chip->epp_tx_id_l);
		break;

	case 0x01:		//LDO on Int
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		chip->epp_max_power = 5;
		msleep(10);

		dev_info(chip->dev, "[%s] LDO on Int \n", __func__);

		chip->epp = (rx_rev_data[0] >> 7) & 0xff;
		chip->epp_max_power = (rx_rev_data[0] & 0x7f);
		g_hw_id_h = rx_rev_data[1];	//0x16
		g_hw_id_l = rx_rev_data[2];	//0x19
		g_fw_rx_id = rx_rev_data[3];
		g_fw_tx_id = (~(fw_data_tx[sizeof(fw_data_tx) - 1])) & 0xff;
		if (chip->epp) {	//EPP
			chip->epp = 1;
			rx1619_set_pmi_icl(chip, 30000);
			dev_info(chip->dev,
				 "[%s] EPP--10mA and epp max power is %d\n",
				 __func__, chip->epp_max_power);
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

		dev_info(chip->dev,
			 "[%s] hw_id_h=0x%x,hw_id_l=0x%x,fw_rx_id=0x%x,g_fw_tx_id=0x%x,g_epp_or_bpp=0x%x\n",
			 __func__, g_hw_id_h, g_hw_id_l, g_fw_rx_id, g_fw_tx_id,
			 g_epp_or_bpp);
		rx1619_set_fod_param(chip, 0x1);
		if (!chip->is_urd_device)
			rx1619_enable_aicl(chip, false);
		break;

	case 0x02:		//null
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		if (chip->fod_mode == 0x1) {
			rx1619_set_fod_param(chip, 0x2);
		} else if (chip->fod_mode == 0x2) {
			rx1619_set_fod_param(chip, 0x3);
		} else if (chip->fod_mode == 0x3) {
			rx1619_set_fod_param(chip, 0x4);
		}
		dev_info(chip->dev, "[rx1619] [%s] fod param setting \n",
			 __func__);
		break;

	case 0x03:		//cali ok
		rx1619_enable_aicl(chip, true);

		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);
		dev_info(chip->dev, "[rx1619] [%s] Calibration OK! \n",
			 __func__);
		if (chip->epp && chip->epp_max_power == 10) {	//EPP
			if (chip->op_mode == LN8282_OPMODE_SWITCHING)
				rx1619_set_pmi_icl(chip, 1800000);	//10W
			else
				rx1619_set_pmi_icl(chip, 900000);
		}
		break;

	case 0x04:		//id ok
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);

		rx1619_read(chip, &g_tx_id_h, REG_RX_REV_DATA1);	//0x0021
		rx1619_read(chip, &g_tx_id_l, REG_RX_REV_DATA2);	//0x0022

		rx1619_write(chip, PRIVATE_ID_CMD, REG_RX_SENT_CMD);	//0x86 authen req
		rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
		dev_err(chip->dev, "[rx1619] [%s] ID OK! \n", __func__);
		break;

	case 0x05:		//sha one ok
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);

		rx1619_read(chip, &g_shaone_data_h, REG_RX_REV_DATA1);	//0x0021
		rx1619_read(chip, &g_shaone_data_l, REG_RX_REV_DATA2);	//0x0022

		rx1619_request_uuid(chip, chip->epp);
		//rx1619_write(chip, PRIVATE_USB_TYPE_CMD, REG_RX_SENT_CMD);//0x87 usb type req
		//rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
		dev_info(chip->dev, "[rx1619] [%s] SHA ONE OK! \n", __func__);
		chip->auth = 1;
		if (!chip->epp)
			alarm_start_relative(&chip->cmd_timeout_alarm,
					     ms_to_ktime(CMD_TIMEOUT_DELAY_MS));
		break;

	case 0x06:		//USB type 0x6,0x14,0x0,0x19,0x5
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
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

		if (chip->is_voice_box_tx)
			usb_type = ADAPTER_VOICE_BOX;

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
		case ADAPTER_VOICE_BOX:
		case ADAPTER_XIAOMI_PD_45W:
		case ADAPTER_XIAOMI_PD_60W:
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

		if (chip->wireless_psy)
			power_supply_changed(chip->wireless_psy);
		break;

	case 0x07:		//FC status 0x7,0x1,0x0,0x19,0x5
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);

		rx1619_read(chip, &g_fc_status, REG_RX_REV_DATA1);
		dev_info(chip->dev, "[%s] FC status = %d\n",
			 __func__, g_fc_status);
		if (chip->is_ble_tx)
			rx1619_request_low_addr(chip);
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
				rx1619_set_pmi_icl(chip,
						   DC_BPP_AUTH_FAIL_CURRENT);
				chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;
			}
		}
		break;
	case 0x09:		//get mac low addr
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);

		for (i = 0; i < 3; i++)
			chip->mac_addr[i] = rx_rev_data[i];

		dev_info(chip->dev, "get low_addr: 0x%x, 0x%x, 0x%x\n",
			 chip->mac_addr[0], chip->mac_addr[1],
			 chip->mac_addr[2]);
		rx1619_request_high_addr(chip);

		break;
	case 0x0a:		//get mac high addr
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);

		for (i = 0; i < 3; i++)
			chip->mac_addr[i + 3] = rx_rev_data[i];

		dev_info(chip->dev, "get high_addr: %x, %x, %x\n",
			 chip->mac_addr[3], chip->mac_addr[4],
			 chip->mac_addr[5]);

		rx1619_sent_tx_mac(chip);
		break;
	case 0x0b:		//BLE flag -- bit0:BLEOK bit1:CEPOK bit2: OOBOK
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);

		ble_flag = rx_rev_data[0];
		dev_info(chip->dev, "[%s] ble_flag = 0x%x \n",
			 __func__, ble_flag);
		if (ble_flag & BIT(2)) {
			if (!chip->is_oob_ok) {	//OOB OK
				schedule_delayed_work(&chip->oob_set_cep_work,
						      0);
				chip->is_oob_ok = 1;
			}
		} else {
			if (chip->is_oob_ok) {
				cancel_delayed_work(&chip->oob_set_cep_work);
				chip->is_oob_ok = 0;
			}
		}
		break;
	case 0x0c:		//RPP
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);

		chip->rpp_val[0] = rx_rev_data[0];
		chip->rpp_val[1] = rx_rev_data[1];

		dev_info(chip->dev, "[%s] rp_h = 0x%x, rp_l = 0x%x\n",
			 __func__, chip->rpp_val[0], chip->rpp_val[1]);

		rx1619_set_rpp(chip);

		break;
	case 0x0d:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		if (!chip->epp)
			alarm_cancel(&chip->cmd_timeout_alarm);
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

			dev_info(chip->dev,
				 "vendor:0x%x, module:0x%x, hw:0x%x and power:0x%x\n",
				 g_uuid_data[0], g_uuid_data[1], g_uuid_data[2],
				 g_uuid_data[3]);

			if (g_uuid_data[3] == 0x07 &&
			    g_uuid_data[1] == 0x1 &&
			    g_uuid_data[2] == 0x4 &&
			    ((g_uuid_data[0] == 0x9)
			     || (g_uuid_data[0] == 0x1))) {
				chip->is_ble_tx = 1;
			} else if (g_uuid_data[3] == 0x01 &&
				   g_uuid_data[1] == 0x2 &&
				   g_uuid_data[2] == 0x8 &&
				   g_uuid_data[0] == 0x6) {
				chip->is_car_tx = 1;
			} else if (g_uuid_data[3] == 0x07 &&
				   g_uuid_data[1] == 0x8 &&
				   g_uuid_data[2] == 0x6 &&
				   g_uuid_data[0] == 0x9) {
				chip->is_voice_box_tx = 1;
				chip->is_ble_tx = 1;
			} else if (g_uuid_data[3] == 0x06 &&
				   g_uuid_data[1] == 0x1 &&
				   g_uuid_data[2] == 0x5 &&
				   g_uuid_data[0] == 0x9) {
				chip->is_pan_tx = 1;
			}
		}
		rx1619_write(chip, PRIVATE_USB_TYPE_CMD, REG_RX_SENT_CMD);	//0x87 usb type req
		rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);

		break;
	case 0x0f:		//error cmd
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		err_cmd = rx_rev_data[0];
		cnt = 0;
		dev_info(chip->dev, "[%s] Receive error cmd %d\n", __func__,
			 err_cmd);
		if (!chip->epp
		    && ((err_cmd == ID_CMD) || (err_cmd == AUTH_CMD)
			|| (err_cmd == UUID_CMD)
			|| (err_cmd == PRIVATE_USB_TYPE_CMD))) {
			alarm_cancel(&chip->cmd_timeout_alarm);
			for (i = 0; i < 7; i++) {
				uA = (100000 + 100000 * i);
				rx1619_set_pmi_icl(chip, uA);
				msleep(400);
				vol = rx1619_get_rx_vout(chip);
				if (vol < 5200) {
					msleep(400);
					vol = rx1619_get_rx_vout(chip);
					if (vol < 5200 && uA > 200000) {
						uA = (uA - 100000);
						rx1619_set_pmi_icl(chip, uA);
						rx1619_set_vout(chip, 6000);
						break;
					}
				}
			}

			if (i >= 7) {
				/*do{
				   msleep(400);
				   vol = rx1619_get_rx_vout(chip);
				   if (vol < 6300){
				   uA = 500000;
				   rx1619_set_pmi_icl(chip, uA);
				   break;
				   }
				   }while(cnt++<3);
				   if (cnt == 3)
				   rx1619_set_pmi_icl(chip, 750000);
				 */
				rx1619_set_pmi_icl(chip, 750000);
			}
			dev_info(chip->dev, "[%s] BPP--750mA \n", __func__);
			chip->target_curr = DC_BPP_AUTH_FAIL_CURRENT;
		}
		break;

	case 0x1f:		//for product test
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);
		msleep(10);

		rx1619_read(chip, &tx_req, REG_RX_REV_DATA1);	//0x0021

		dev_info(chip->dev, "[rx1619] [%s] tx_req=0x%x \n",
			 __func__, tx_req);

		if (tx_req == 0x12) {	//iout
			rx1619_write(chip, 0x38, REG_RX_SENT_DATA1);	//sent header
			rx1619_write(chip, 0x12, REG_RX_SENT_DATA2);	//sent cmd

			iout = rx1619_get_rx_iout(chip);
			data_h = (iout & 0x00ff);
			data_l = (iout & 0xff00) >> 8;
			rx1619_write(chip, data_h, 0x0003);
			rx1619_write(chip, data_l, 0x0004);
			dev_info(chip->dev,
				 "[rx1619] [%s] product test--0x12--0x%x,0x%x iout=%d \n",
				 __func__, data_h, data_l, iout);
		} else if (tx_req == 0x13) {	//vout
			rx1619_write(chip, 0x38, REG_RX_SENT_DATA1);	//sent header
			rx1619_write(chip, 0x13, REG_RX_SENT_DATA2);	//sent cmd

			vout = rx1619_get_rx_vout(chip);
			data_h = (vout & 0x00ff);
			data_l = (vout & 0xff00) >> 8;
			rx1619_write(chip, data_h, 0x0003);
			rx1619_write(chip, data_l, 0x0004);
			dev_info(chip->dev,
				 "[rx1619] [%s] product test--0x13--0x%x,0x%x vout=%d \n",
				 __func__, data_h, data_l, vout);
		} else if (tx_req == 0x24) {	//firmware Version
			rx1619_write(chip, 0x58, REG_RX_SENT_DATA1);	//sent header
			rx1619_write(chip, 0x24, REG_RX_SENT_DATA2);	//sent cmd
			rx1619_write(chip, 0x00, REG_RX_SENT_DATA3);
			rx1619_write(chip, 0x00, REG_RX_SENT_DATA4);
			rx1619_write(chip, g_fw_rx_id, REG_RX_SENT_DATA5);
			rx1619_write(chip, g_fw_tx_id, REG_RX_SENT_DATA6);
			dev_info(chip->dev,
				 "[rx1619] [%s] product test--0x24--rxtx_fw_version=0x%x,0x%x\n",
				 __func__, g_fw_rx_id, g_fw_tx_id);
		} else if (tx_req == 0x23) {	//RX chip ID
			rx1619_write(chip, 0x38, REG_RX_SENT_DATA1);	//sent header
			rx1619_write(chip, 0x23, REG_RX_SENT_DATA2);	//sent cmd
			rx1619_write(chip, g_hw_id_h, REG_RX_SENT_DATA3);
			rx1619_write(chip, g_hw_id_l, REG_RX_SENT_DATA4);
			dev_info(chip->dev,
				 "[rx1619] [%s] product test--0x23--rx_hw_id=0x%x,0x%x\n",
				 __func__, g_hw_id_h, g_hw_id_l);
		} else if (tx_req == 0x0b) {	//USB type
			rx1619_write(chip, 0x28, REG_RX_SENT_DATA1);	//sent header
			rx1619_write(chip, 0x0b, REG_RX_SENT_DATA2);	//sent cmd
			rx1619_write(chip, g_USB_TYPE, REG_RX_SENT_DATA3);
			dev_info(chip->dev,
				 "[rx1619] [%s] product test--0x25--g_USB_TYPE=0x%x \n",
				 __func__, g_USB_TYPE);
		} else if (tx_req == 0x30) {
			//respond
			rx1619_write(chip, 0x8D, REG_RX_SENT_CMD);
			rx1619_write(chip, 0x18, REG_RX_SENT_DATA1);
			rx1619_write(chip, 0x30, REG_RX_SENT_DATA2);
			dev_info(chip->dev, "[ factory reverse test ] reverse charging test : receive request\n");

			//start to check power good
			chip->wait_for_reverse_test = true;
		} else
			dev_err(chip->dev,
				"[rx1619] [%s] product test--other cmd \n",
				__func__);

		rx1619_write(chip, PRIVATE_PRODUCT_TEST_CMD, REG_RX_SENT_CMD);	//sent test cmd
		rx1619_write(chip, AP_SENT_DATA_OK, REG_AP_RX_COMM);
		break;

	default:
		rx1619_write(chip, AP_REV_DATA_OK, REG_AP_RX_COMM);	//receive ok
		msleep(10);
		dev_err(chip->dev, "[rx1619] [%s] other private cmd \n",
			__func__);
		break;
	}

	mutex_unlock(&chip->wireless_chg_int_lock);

	return;
}

static irqreturn_t rx1619_chg_stat_handler(int irq, void *dev_id)
{
	struct rx1619_chg *chip = dev_id;
	//u8 rx_req = 0;

	dev_info(chip->dev, "[%s]\n", __func__);

	schedule_delayed_work(&chip->wireless_int_work, 0);

	return IRQ_HANDLED;
}

static irqreturn_t rx1619_power_good_handler(int irq, void *dev_id)
{
	struct rx1619_chg *chip = dev_id;

	if (chip->fw_update)
		return IRQ_HANDLED;
	schedule_delayed_work(&chip->wpc_det_work, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}
#define POWER_GOOD_GPIO 1158
static int rx1619_parse_dt(struct rx1619_chg *chip)
{
	struct device_node *node = chip->dev->of_node;

	if (!node) {
		dev_err(chip->dev, "[rx1619] [%s] No DT data Failing Probe\n",
			__func__);
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
	dev_info(chip->dev, "[%s]: rx1619_irq_gpio is %d\n", __func__,
		 chip->irq_gpio);

	if (!gpio_is_valid(chip->irq_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_irq_gpio %d\n",
			__func__, chip->irq_gpio);
		return -EINVAL;
	}

	chip->power_good_gpio = of_get_named_gpio(node, "rx,power_good", 0);
	if (!gpio_is_valid(chip->power_good_gpio)) {
		dev_err(chip->dev, "[rx1619] [%s] fail_power_good_gpio %d\n",
			__func__, chip->power_good_gpio);
		return -EINVAL;
	}
	if (need_unconfig_pg)
		chip->power_good_gpio = POWER_GOOD_GPIO;

	chip->reverse_boost_enable_gpio = of_get_named_gpio(node, "rx,reverse-booset-enable", 0);
	if ((!gpio_is_valid(chip->reverse_boost_enable_gpio))) {
		dev_err(chip->dev, "get reverse_boost_enable_gpio fault\n");
		return -EINVAL;
	}

	chip->wireless_by_usbin = of_property_read_bool(node,
							"mi,wireless-by-usbin");

	chip->is_urd_device = of_property_read_bool(node, "mi,urd-device");

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

	ret = pinctrl_select_state(chip->rx_pinctrl, chip->rx_gpio_active);
	if (ret < 0) {
		dev_err(chip->dev, "fail to select pinctrl active rc=%d\n",
			ret);
		return ret;
	}

	if (gpio_is_valid(chip->power_good_gpio)) {
		chip->power_good_irq = gpio_to_irq(chip->power_good_gpio);
		if (chip->power_good_irq < 0) {
			dev_err(chip->dev,
				"[rx1619] [%s] power good irq Fail! %d\n",
				__func__, chip->power_good_irq);
			goto fail_power_good_gpio;
		}
	} else {
		dev_err(chip->dev, "%s: power good gpio not provided\n",
			__func__);
		goto fail_power_good_gpio;
	}

	if (gpio_is_valid(chip->irq_gpio)) {
		chip->client->irq = gpio_to_irq(chip->irq_gpio);
		if (chip->client->irq < 0) {
			dev_err(chip->dev,
				"[rx1619] [%s] gpio_to_irq Fail! %d \n",
				__func__, chip->irq_gpio);
			goto fail_irq_gpio;
		}
	} else {
		dev_err(chip->dev, "%s: irq gpio not provided\n", __func__);
		goto fail_irq_gpio;
	}

	return ret;

      fail_irq_gpio:
	gpio_free(chip->irq_gpio);
      fail_power_good_gpio:
	gpio_free(chip->power_good_gpio);

	return ret;
}

/*******************************************************
 * SET GPIO STATE TO SMB FOR IGNORE DC_POWER_ON IRQ*
 *******************************************************/
 /* define for reverse state of wireless charging */
#define REVERSE_GPIO_STATE_UNSET 0
#define REVERSE_GPIO_STATE_START 1
#define REVERSE_GPIO_STATE_END     2
static int rx1619_set_reverse_gpio_state(struct rx1619_chg *chip, int enable)
{
	union power_supply_propval reverse_val = { 0, };
	chip->reverse_gpio_state = !!enable;
	if (!chip->wireless_psy)
		chip->wireless_psy = power_supply_get_by_name("wireless");

	if (chip->wireless_psy) {
		dev_dbg(chip->dev, "set_reverse_gpio_state\n",
			reverse_val.intval);
		if (enable) {
			reverse_val.intval = REVERSE_GPIO_STATE_START;
		} else {
			reverse_val.intval = REVERSE_GPIO_STATE_END;
		}
		power_supply_set_property(chip->wireless_psy,
					  POWER_SUPPLY_PROP_REVERSE_GPIO_STATE,
					  &reverse_val);
	} else {
		dev_err(chip->dev, "no wireless_psy,return\n");
		return -EINVAL;
	}
	return 0;
}

static int rx_set_reverse_boost_enable_gpio(struct rx1619_chg *chip, int enable)
{
   int ret;
   if (gpio_is_valid(chip->reverse_boost_enable_gpio)) {
	   ret = gpio_request(chip->reverse_boost_enable_gpio,
				  "reverse-boost-enable-gpio");
	   if (ret) {
		   dev_err(chip->dev,
			   "%s: unable to reverse_boost_enable_gpio [%d]\n",
			   __func__, chip->reverse_boost_enable_gpio);
	   }

	   ret = gpio_direction_output(chip->reverse_boost_enable_gpio, !!enable);
	   if (ret) {
		   dev_err(chip->dev,
			   "%s: cannot set direction for reverse_boost_enable_gpio  gpio [%d]\n",
			   __func__, chip->reverse_boost_enable_gpio);
	   }
	   gpio_free(chip->reverse_boost_enable_gpio);
   } else
	   dev_err(chip->dev, "%s: unable to set reverse_boost_enable_gpio\n");

	return ret;
}


static int rx_set_reverse_gpio(struct rx1619_chg *chip, int enable)
{
	int ret;
	union power_supply_propval val = { 0, };

	if (!chip->wireless_psy)
		chip->wireless_psy = power_supply_get_by_name("wireless");

	if (!chip->wireless_psy) {
		dev_err(chip->dev, "no wireless_psy,return\n");
		return -EINVAL;
	}

	val.intval = !!enable;
	if (gpio_is_valid(chip->tx_on_gpio)) {
		if (enable) {
			rx1619_set_reverse_gpio_state(chip, enable);
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_SW_DISABLE_DC_EN,
						  &val);
			rx_set_reverse_boost_enable_gpio(chip, enable);
			msleep(100);
		}
		ret = gpio_request(chip->tx_on_gpio, "tx-on-gpio");
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

		ret = gpio_get_value(chip->tx_on_gpio);
		dev_info(chip->dev, "txon gpio: %d\n", ret);
		gpio_free(chip->tx_on_gpio);
		if (!enable) {
			msleep(100);
			rx_set_reverse_boost_enable_gpio(chip, enable);
			rx1619_set_reverse_gpio_state(chip, enable);
			power_supply_set_property(chip->wireless_psy,
						  POWER_SUPPLY_PROP_SW_DISABLE_DC_EN,
						  &val);
		}

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

#define REVERSE_COUNTRY_DOMESTIC 0
#define REVERSE_COUNTRY_INTERNATIONAL 1

#define REVERSE_CURRENT_1000MA       0x0A
#define REVERSE_CURRENT_500MA        0x05

static int rx_set_reverse_chg_mode(struct rx1619_chg *chip, int enable)
{
	union power_supply_propval wk_val = { 0, };
	int ret, rc;

	chip->wireless_psy = power_supply_get_by_name("wireless");
	if (!chip->wireless_psy) {
		dev_err(chip->dev, "no wireless_psy,return\n");
		return -EINVAL;
	}

	if (gpio_is_valid(chip->tx_on_gpio)) {
		rx_set_reverse_gpio(chip, enable);
		power_supply_changed(chip->wireless_psy);
		msleep(100);
		if (enable) {
			if (chip->wireless_psy) {
				wk_val.intval = 1;
				power_supply_set_property(chip->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							  &wk_val);
			}
			msleep(100);
			/* set reverse fod */
			rx1619_set_reverse_fod(chip, 0, 0);
			rx1619_start_tx_function(chip);
			alarm_start_relative(&chip->reverse_dping_alarm,
					     ms_to_ktime
					     (REVERSE_DPING_CHECK_DELAY_MS));
		} else {
			dev_info(chip->dev,
				 "disable reverse charging for wireless\n");
			if (chip->wireless_psy) {
				wk_val.intval = 0;
				power_supply_set_property(chip->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							  &wk_val);
			}
			cancel_delayed_work(&chip->reverse_chg_state_work);
			cancel_delayed_work(&chip->reverse_dping_state_work);

			rc = alarm_cancel(&chip->reverse_dping_alarm);
			if (rc < 0)
				dev_err(chip->dev,
					"Couldn't cancel reverse_dping_alarm\n");

			rc = alarm_cancel(&chip->reverse_chg_alarm);
			if (rc < 0)
				dev_err(chip->dev,
					"Couldn't cancel reverse_chg_alarm\n");
			pm_relax(chip->dev);
		}
	} else
		dev_err(chip->dev, "%s: unable to set tx_on gpio_130\n");

	return ret;
}

static enum alarmtimer_restart reverse_chg_alarm_cb(struct alarm *alarm,
						    ktime_t now)
{
	struct rx1619_chg *chip = container_of(alarm, struct rx1619_chg,
					       reverse_chg_alarm);

	dev_info(chip->dev, " Reverse Chg Alarm Triggered %lld\n",
		 ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(chip->dev);
	schedule_delayed_work(&chip->reverse_chg_state_work, 0);

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart cmd_timeout_alarm_cb(struct alarm *alarm,
						    ktime_t now)
{
	struct rx1619_chg *chip = container_of(alarm, struct rx1619_chg,
					       cmd_timeout_alarm);

	dev_info(chip->dev, " CMD_timeout_alarm Triggered %lld\n",
		 ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	schedule_delayed_work(&chip->cmd_timeout_work, 0);

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart reverse_test_ready_alarm_cb(struct alarm *alarm,
							   ktime_t now)
{
	struct rx1619_chg *chip = container_of(alarm, struct rx1619_chg,
					       reverse_test_ready_alarm);

	dev_info(chip->dev, "[ factory reverse test ] reverse_rest_ready_alarm Triggered\n");

	/* Atomic context, cannot use voter */
	pm_stay_awake(chip->dev);
	schedule_delayed_work(&chip->reverse_dping_state_work, 0);
	chip->wait_for_reverse_test = false;

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart reverse_dping_alarm_cb(struct alarm *alarm,
						      ktime_t now)
{
	struct rx1619_chg *chip = container_of(alarm, struct rx1619_chg,
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
	dev_info(chip->dev, "dc plug %s\n", enable ? "in" : "out");

	if (enable) {
		chip->dcin_present = 1;
	} else {
		schedule_delayed_work(&chip->oob_set_ept_work,
				      msecs_to_jiffies(10));
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
		chip->is_oob_ok = 0;
		chip->last_icl = 0;
		chip->last_qc3_icl = 0;
		chip->op_mode = LN8282_OPMODE_UNKNOWN;
		chip->status = NORMAL_MODE;
		chip->target_vol = 0;
		chip->target_curr = 0;
		chip->is_car_tx = 0;
		chip->is_voice_box_tx = 0;
		chip->is_ble_tx = 0;
		g_USB_TYPE = 0;
		chip->is_pan_tx = 0;
		chip->disable_bq = false;
		cancel_delayed_work_sync(&chip->wireless_int_work);
		cancel_delayed_work(&chip->chg_monitor_work);
		/* clear TX address */
		memset(chip->mac_addr, 0x0, sizeof(chip->mac_addr));
		rx1619_sent_tx_mac(chip);

		/* enable aicl if disabled by wireless earlier */
		rx1619_enable_aicl(chip, true);

	}
}

/* set otg state while reverse status */
#define OTG_REG_ADDR            0x00
#define OTG_PLUGIN_CMD          0x12
#define OTG_PLUGOUT_CMD         0x11
#define OTG_STATUS_CMD          0x0000
#define OTG_TRIGGER_ADDR        0x000C
#define OTG_TRIGGER_CMD         0x55

static int rx_set_otg_state(struct rx1619_chg *chip, int plugin)
{
	int otg_status_cmd;

	chip->is_otg_insert = !!plugin;

	if (plugin)
		otg_status_cmd = OTG_PLUGIN_CMD;
	else
		otg_status_cmd = OTG_PLUGOUT_CMD;

	dev_info(g_chip->dev, "set otg state: %d\n", plugin);

	if (chip->reverse_gpio_state) {
		rx1619_write(chip, otg_status_cmd, OTG_REG_ADDR);
		rx1619_write(chip, OTG_TRIGGER_CMD, OTG_TRIGGER_ADDR);
	}

	return 0;
}

static int rx_set_enable_mode(struct rx1619_chg *chip, int enable)
{
	int ret = 0;
	int gpio_enable_val = 0;

	if (gpio_is_valid(chip->enable_gpio)) {
		ret = gpio_request(chip->enable_gpio, "rx-enable-gpio");
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
		gpio_enable_val = gpio_get_value(chip->enable_gpio);
		pr_info("rx1619 enable gpio val is :%d\n", gpio_enable_val);
		gpio_free(chip->enable_gpio);
	}

	return ret;
}

static ssize_t chip_vrect_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	unsigned int vrect = 0;

	vrect = rx1619_get_rx_vrect(g_chip);

	return scnprintf(buf, PAGE_SIZE, "rx1619 Vrect : %d mV\n", vrect);
}

static ssize_t chip_vout_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	unsigned int vout = 0;

	vout = rx1619_get_rx_vout(g_chip);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vout);
}

static ssize_t chip_iout_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	unsigned int iout = 0;

	iout = rx1619_get_rx_iout(g_chip);

	return scnprintf(buf, PAGE_SIZE, "%d\n", iout);
}

static ssize_t chip_vout_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int index;

	index = (int)simple_strtoul(buf, NULL, 10);
	dev_info(g_chip->dev, "[rx1619] [%s] --Store output_voltage = %d\n",
		 __func__, index);
	if ((index < 4000) || (index > 21000)) {
		dev_err(g_chip->dev,
			"[rx1619] [%s] Store Voltage %s is invalid\n", __func__,
			buf);
		rx1619_set_vout(g_chip, 0);
		return count;
	}

	rx1619_set_vout(g_chip, index);

	return count;
}

static ssize_t chip_debug_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
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
				const char *buf, size_t count)
{
	return count;
}

static ssize_t chip_fod_parameter_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int i;
	for (i = 0; i < 8; i++) {
		fod_param[i] = (char)simple_strtoul(buf, NULL, 16);	//fod_param[0]
		buf += 3;
	}

	for (i = 0; i < 8; i++)
		dev_info(g_chip->dev, "[%s]: fod_param[%d] = 0x%x\n", __func__,
			 i, fod_param[i]);

	return count;
}

static ssize_t chip_firmware_update_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{

	bool ret = false;
	u8 boot_project_id_h = 0;
	u8 boot_project_id_l = 0;
	u8 tx_project_id_h = 0;
	u8 tx_project_id_l = 0;
	u8 rx_project_id_h = 0;
	u8 rx_project_id_l = 0;

	dev_info(g_chip->dev, "[rx1619] [%s] Firmware Update begin\n",
		 __func__);

	if (g_chip->fw_update) {
		dev_err(g_chip->dev,
			"[rx1619] [%s] Firmware Update is on going!\n",
			__func__);
		return scnprintf(buf, PAGE_SIZE, "Firmware Update:Failed\n");
	}

	g_chip->fw_update = true;
	rx_set_reverse_gpio(g_chip, true);
	msleep(100);
	ret = rx1619_onekey_download_firmware(g_chip);
	if (!ret) {
		dev_err(g_chip->dev,
			"[rx1619] [%s] Firmware Update failed! Please try again!\n",
			__func__);
		rx_set_reverse_gpio(g_chip, false);
		g_chip->fw_update = false;
		return scnprintf(buf, PAGE_SIZE, "Firmware Update:Failed\n");

	} else {
		boot_fw_version =
		    (~fw_data_boot[sizeof(fw_data_boot) - 1]) & 0xff;
		tx_fw_version = (~fw_data_tx[sizeof(fw_data_tx) - 1]) & 0xff;
		rx_fw_version = (~fw_data_rx[sizeof(fw_data_rx) - 1]) & 0xff;
		boot_project_id_h =
		    (~fw_data_boot[sizeof(fw_data_boot) - 3]) & 0xff;
		boot_project_id_l =
		    (~fw_data_boot[sizeof(fw_data_boot) - 2]) & 0xff;
		tx_project_id_h = (~fw_data_tx[sizeof(fw_data_tx) - 3]) & 0xff;
		tx_project_id_l = (~fw_data_tx[sizeof(fw_data_tx) - 2]) & 0xff;
		rx_project_id_h = (~fw_data_rx[sizeof(fw_data_rx) - 3]) & 0xff;
		rx_project_id_l = (~fw_data_rx[sizeof(fw_data_rx) - 2]) & 0xff;

		dev_info(g_chip->dev,
			 "boot_fw_version=%c%c_V%02x, tx_fw_version=%c%c_V%02x, rx_fw_version=%c%c_V%02x \n",
			 boot_project_id_h, boot_project_id_l, boot_fw_version,
			 tx_project_id_h, tx_project_id_l, tx_fw_version,
			 rx_project_id_h, rx_project_id_l, rx_fw_version);

		dev_info(g_chip->dev,
			 "[rx1619] [%s] Firmware Update Success!!! \n",
			 __func__);
		rx_set_reverse_gpio(g_chip, false);
		g_chip->fw_update = false;
		g_chip->fw_version = g_fw_rx_id;
		return scnprintf(buf, PAGE_SIZE, "Firmware Update:Success\n");
	}
}

static ssize_t chip_version_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	if (!g_chip->fw_update) {
		g_chip->fw_update = true;
		rx_set_reverse_gpio(g_chip, true);
		msleep(100);
		rx1619_check_firmware_version(g_chip, BOOT_AREA);
		rx1619_check_firmware_version(g_chip, TX_AREA);
		rx1619_check_firmware_version(g_chip, RX_AREA);
		rx_set_reverse_gpio(g_chip, false);
		g_chip->fw_update = false;
		return scnprintf(buf, PAGE_SIZE,
				 "app_ver:%02x.%02x.%02x.%x%x\n", g_fw_boot_id,
				 g_fw_tx_id, g_fw_rx_id, g_hw_id_h, g_hw_id_l);
	} else {
		return scnprintf(buf, PAGE_SIZE,
				 "fw update on going, cannot show version\n");
	}
}

static ssize_t chip_vout_calibration_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	int index = 0;

	index = (int)simple_strtoul(buf, NULL, 10);

	g_Delta = index;
	dev_err(g_chip->dev, "[rx1619] [%s] g_Delta = %d \n", __func__,
		g_Delta);

	return count;
}

static ssize_t txon_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	int mode;

	mode = rx_get_reverse_chg_mode(g_chip);

	return scnprintf(buf, PAGE_SIZE, "reverse chg mode : %d\n", mode);
}

static ssize_t txon_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int ret, enable;

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;

	rx_set_reverse_gpio(g_chip, enable);

	return count;
}

static ssize_t attr_firmware_bin_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	bool ret = false;
	ret = rx1619_push_rx_firmware(g_chip);
	if (!ret) {
		dev_err(g_chip->dev,
			"[rx1619] [%s] RX Firmware Push failed! Please try again!\n",
			__func__);
		return scnprintf(buf, PAGE_SIZE,
				 "RX Firmware Push failed! Please try again! \n");

	} else {
		dev_info(g_chip->dev, "rx_fw_version=%c%c_V%02x \n",
			 rx_bin_project_id_h, rx_bin_project_id_l,
			 rx_fw_version);
		dev_info(g_chip->dev,
			 "[rx1619] [%s] RX Firmware Push Success!!! \n",
			 __func__);
		return scnprintf(buf, PAGE_SIZE,
				 "RX Firmware Push Success!!! rx_fw_ver=%c%c_V%02x\n",
				 rx_bin_project_id_h, rx_bin_project_id_l,
				 rx_fw_version);
	}
}

static ssize_t attr_firmware_bin_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	u8 *rx_fw_bin = NULL;
	if (g_chip->rx_fw_len + count >= 3 * 1024 * 1024) {
		dev_err(g_chip->dev, "The firmware is too large.\n");
		goto out;
	}

	rx_fw_bin = kmalloc(count + g_chip->rx_fw_len, GFP_KERNEL);
	if (rx_fw_bin == NULL) {
		dev_err(g_chip->dev, "write %d bytes failed.\n", (int)count);
		goto out;
	}

	memcpy(rx_fw_bin, g_chip->rx_fw_bin, g_chip->rx_fw_len);
	memcpy(rx_fw_bin + g_chip->rx_fw_len, buf, count);
	if (g_chip->rx_fw_bin != NULL) {
		kfree(g_chip->rx_fw_bin);
	}
	g_chip->rx_fw_bin = rx_fw_bin;
	g_chip->rx_fw_len += count;

      out:
	return count;
}

static DEVICE_ATTR(chip_vrect, S_IRUGO, chip_vrect_show, NULL);
static DEVICE_ATTR(chip_vout_calibration, S_IWUSR, NULL,
		   chip_vout_calibration_store);
static DEVICE_ATTR(chip_firmware_update, S_IWUSR | S_IRUGO,
		   chip_firmware_update_show, NULL);
static DEVICE_ATTR(chip_fw_bin, S_IWUSR | S_IRUGO, attr_firmware_bin_show,
		   attr_firmware_bin_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show,
		   chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IRUGO, chip_iout_show, NULL);
static DEVICE_ATTR(chip_debug, S_IWUSR | S_IRUGO, chip_debug_show,
		   chip_debug_store);
static DEVICE_ATTR(txon, S_IWUSR | S_IRUGO, txon_show, txon_store);
static DEVICE_ATTR(chip_fod_parameter, S_IWUSR | S_IRUGO, NULL,
		   chip_fod_parameter_store);

static struct attribute *rx1619_sysfs_attrs[] = {
	&dev_attr_chip_vrect.attr,
	&dev_attr_chip_version.attr,
	&dev_attr_chip_vout.attr,
	&dev_attr_chip_iout.attr,
	&dev_attr_chip_debug.attr,
	&dev_attr_chip_fw_bin.attr,
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
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_VRECT,
	POWER_SUPPLY_PROP_RX_IOUT,
	POWER_SUPPLY_PROP_TX_ADAPTER,
	POWER_SUPPLY_PROP_WIRELESS_VERSION,
	POWER_SUPPLY_PROP_WIRELESS_FW_VERSION,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_OTG_STATE,
};

static int rx1619_wireless_set_property(struct power_supply *psy,
					enum power_supply_property prop,
					const union power_supply_propval *val)
{
	int ret;
	struct rx1619_chg *chip = power_supply_get_drvdata(psy);
	int data;

	switch (prop) {
	case POWER_SUPPLY_PROP_PRESENT:
		rx1619_set_present(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		chip->ss = val->intval;
		if (chip->wireless_psy)
			power_supply_changed(chip->wireless_psy);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		data = val->intval / 1000;

		if (chip->is_urd_device) {
			if (data > 20000)
				data = 20000;
			ret = rx1619_set_vout(chip, data);
			break;
		}

		if (data < ADAPTER_VOUT_LIMIT_VOL)
			data = ADAPTER_VOUT_LIMIT_VOL;
		else if (data > 10000)
			data = 10000;
		if (chip->op_mode == LN8282_OPMODE_SWITCHING)
			data *= 2;
		else if (chip->epp)
			data = EPP_VOL_THRESHOLD;
		if (!chip->disable_bq)
			ret = rx1619_set_vout(chip, data);
		break;
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		ret = rx_set_enable_mode(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		if (chip->fw_update) {
			dev_info(chip->dev, "fw update going, break\n");
			break;
		}
		chip->is_reverse_chg = 0;
		schedule_delayed_work(&chip->reverse_sent_state_work, 0);
		if (!chip->power_good_flag) {
			ret = rx_set_reverse_chg_mode(chip, val->intval);
		} else {
			chip->is_reverse_chg = 3;
			schedule_delayed_work(&chip->reverse_sent_state_work,
					      0);
		}
		break;
	case POWER_SUPPLY_PROP_OTG_STATE:
		rx_set_otg_state(chip, val->intval);
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
	case POWER_SUPPLY_PROP_WIRELESS_VERSION:
		val->intval = chip->epp;
		break;
	case POWER_SUPPLY_PROP_WIRELESS_FW_VERSION:
		val->intval = chip->fw_version;
		break;
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

		if (chip->is_urd_device) {
			val->intval = tmp;
			break;
		}
		if (chip->op_mode == LN8282_OPMODE_SWITCHING)
			val->intval = tmp / 2;
		else
			val->intval = tmp;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_VRECT:
		val->intval = rx1619_get_rx_vrect(chip);
		break;
	case POWER_SUPPLY_PROP_RX_IOUT:
		val->intval = rx1619_get_rx_iout(chip);
		break;
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		if (chip->enable_gpio)
			val->intval = !gpio_get_value(chip->enable_gpio);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		val->intval = rx_get_reverse_chg_mode(chip);
		break;
	case POWER_SUPPLY_PROP_TX_ADAPTER:
		val->intval = g_USB_TYPE;
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = chip->chip_ok;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
#endif

// first step: define regmap_config
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

extern char *saved_command_line;

static int get_cmdline(struct rx1619_chg *chip)
{
	if (strnstr(saved_command_line, "androidboot.mode=",
		    strlen(saved_command_line))) {

		chip->power_off_mode = 1;
		dev_info(chip->dev,
			 "[idtp9220]: enter power off charging app\n");
	} else {
		chip->power_off_mode = 0;
		dev_info(chip->dev, "[idtp9220]: enter normal boot mode\n");
	}
	return 1;
}

static int rx1619_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct rx1619_chg *chip;
	struct kobject *rx1619_kobj;
	//int drv_load = 0;

	struct power_supply_config wip_psy_cfg = { };
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev,
			"i2c allocated device info data failed!\n");
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
	chip->fw_update = false;
	chip->fw_version = 0;

	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, chip);

	rx1619_parse_dt(chip);
	rx1619_gpio_init(chip);

	mutex_init(&chip->wireless_chg_lock);
	mutex_init(&chip->wireless_chg_int_lock);
	mutex_init(&chip->sysfs_op_lock);
	INIT_DELAYED_WORK(&chip->wireless_int_work, rx1619_wireless_int_work);
	INIT_DELAYED_WORK(&chip->wpc_det_work, rx1619_wpc_det_work);
	INIT_DELAYED_WORK(&chip->chg_monitor_work, rx_monitor_work);
	INIT_DELAYED_WORK(&chip->chg_detect_work, rx_chg_detect_work);
	INIT_DELAYED_WORK(&chip->reverse_sent_state_work,
			  reverse_chg_sent_state_work);
	INIT_DELAYED_WORK(&chip->reverse_chg_state_work,
			  reverse_chg_state_set_work);
	INIT_DELAYED_WORK(&chip->reverse_dping_state_work,
			  reverse_dping_state_set_work);
	INIT_DELAYED_WORK(&chip->oob_set_cep_work, rx1619_oob_set_cep_work);
	INIT_DELAYED_WORK(&chip->oob_set_ept_work, rx1619_oob_set_ept_work);
	INIT_DELAYED_WORK(&chip->dc_check_work, rx1619_dc_check_work);
	INIT_DELAYED_WORK(&chip->cmd_timeout_work, rx1619_cmd_timeout_work);
	INIT_DELAYED_WORK(&chip->fw_download_work, rx1619_fw_download_work);
	INIT_DELAYED_WORK(&chip->rx_first_boot, rx1619_rx_first_boot);
	INIT_DELAYED_WORK(&chip->pan_tx_work, rx1619_pan_tx_work);
	INIT_DELAYED_WORK(&chip->voice_tx_work, rx1619_voice_tx_work);

	chip->wip_psy_d.name = "rx1619";
	chip->wip_psy_d.type = POWER_SUPPLY_TYPE_WIRELESS;
	chip->wip_psy_d.get_property = rx1619_wireless_get_property;
	chip->wip_psy_d.set_property = rx1619_wireless_set_property;
	chip->wip_psy_d.properties = rx1619_wireless_properties;
	chip->wip_psy_d.num_properties = ARRAY_SIZE(rx1619_wireless_properties);
	chip->wip_psy_d.property_is_writeable = rx1619_prop_is_writeable,
	    wip_psy_cfg.drv_data = chip;

	chip->wip_psy =
	    devm_power_supply_register(chip->dev, &chip->wip_psy_d,
				       &wip_psy_cfg);
	if (IS_ERR(chip->wip_psy)) {
		dev_err(chip->dev, "Couldn't register wip psy rc=%ld\n",
			PTR_ERR(chip->wip_psy));
		return ret;
	}

	if (chip->client->irq) {
		ret =
		    devm_request_threaded_irq(&chip->client->dev,
					      chip->client->irq, NULL,
					      rx1619_chg_stat_handler,
					      (IRQF_TRIGGER_FALLING |
					       IRQF_TRIGGER_RISING |
					       IRQF_ONESHOT),
					      "rx1619_chg_stat_irq", chip);
		if (ret) {
			dev_err(chip->dev, "Failed irq = %d ret = %d\n",
				chip->client->irq, ret);
		}
	}
	enable_irq_wake(chip->client->irq);

	if (chip->power_good_irq) {
		ret =
		    devm_request_threaded_irq(&chip->client->dev,
					      chip->power_good_irq, NULL,
					      rx1619_power_good_handler,
					      (IRQF_TRIGGER_FALLING |
					       IRQF_TRIGGER_RISING |
					       IRQF_ONESHOT),
					      "rx1619_power_good_irq", chip);
		if (ret) {
			dev_err(chip->dev, "Failed irq = %d ret = %d\n",
				chip->power_good_irq, ret);
		}
	}
	enable_irq_wake(chip->power_good_irq);

	rx1619_kobj = kobject_create_and_add("rx1619", NULL);
	if (!rx1619_kobj) {
		dev_err(chip->dev, "sysfs_create_group fail");
		goto error_sysfs;
	}
	ret = sysfs_create_group(rx1619_kobj, &rx1619_sysfs_group_attrs);
	if (ret < 0) {
		dev_err(chip->dev, "sysfs_create_group fail %d\n", ret);
		goto error_sysfs;
	}

	determine_initial_status(chip);

	//rx1619_dump_reg();
	g_chip = chip;
	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->reverse_test_ready_alarm,
			   ALARM_BOOTTIME, reverse_test_ready_alarm_cb);
	} else {
		dev_err(chip->dev,
			"Failed to initialize reverse_test_ready_alarm alarm\n");
		return -ENODEV;
	}

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->reverse_dping_alarm,
			   ALARM_BOOTTIME, reverse_dping_alarm_cb);
	} else {
		dev_err(chip->dev,
			"Failed to initialize reverse dping alarm\n");
		return -ENODEV;
	}

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->reverse_chg_alarm,
			   ALARM_BOOTTIME, reverse_chg_alarm_cb);
	} else {
		dev_err(chip->dev, "Failed to initialize reverse chg alarm\n");
		return -ENODEV;
	}

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&chip->cmd_timeout_alarm,
			   ALARM_BOOTTIME, cmd_timeout_alarm_cb);
	} else {
		dev_err(chip->dev, "Failed to initialize cmd_timeout_alarm\n");
		return -ENODEV;
	}
	chip->hw_country = get_hw_country_version();
	dev_info(&client->dev, "hw_country: %d\n", chip->hw_country);
	dev_err(chip->dev, "[rx1619] [%s] success! \n", __func__);
	get_cmdline(chip);

	if (chip->power_off_mode) {
		rx_set_enable_mode(chip, false);
		usleep_range(20000, 25000);
		rx_set_enable_mode(chip, true);
	} else {
		schedule_delayed_work(&chip->chg_detect_work, 3 * HZ);
	}

	if (!g_rx1619_first_flag)
		schedule_delayed_work(&chip->rx_first_boot, msecs_to_jiffies(30000));

	return 0;
      error_sysfs:
	sysfs_remove_group(rx1619_kobj, &rx1619_sysfs_group_attrs);
	dev_err(chip->dev, "[rx1619] [%s] rx1619 probe error_sysfs! \n",
		__func__);

	if (chip->irq_gpio > 0)
		gpio_free(chip->irq_gpio);

	return 0;
}

static void rx1619_shutdown(struct i2c_client *client)
{
	struct rx1619_chg *chip = i2c_get_clientdata(client);

	if (chip->power_good_flag) {
		rx_set_enable_mode(chip, false);
		usleep_range(20000, 25000);
		rx_set_enable_mode(chip, true);
	}
}

static int rx1619_remove(struct i2c_client *client)
{
	struct rx1619_chg *chip = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&chip->wireless_int_work);

	return 0;
}

static const struct i2c_device_id rx1619_id[] = {
	{rx1619_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, rx1619_id);

static struct of_device_id rx1619_match_table[] = {
	{.compatible = "nuvolta,wl_charger_rx1619",},
	{}
};

static struct i2c_driver rx1619_driver = {
	.driver = {
		   .name = rx1619_DRIVER_NAME,
		   .of_match_table = rx1619_match_table,
		   },
	.probe = rx1619_probe,
	.remove = rx1619_remove,
	.shutdown = rx1619_shutdown,
	.id_table = rx1619_id,
};

static int __init rx1619_init(void)
{
	int ret;

	printk("is_nvt_rx flag is:%d\n", is_nvt_rx);
	if (!is_nvt_rx)
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
#define BOARD_P01 "P0.1"
#define BOARD_P1 "P1"
#define BOARD_P11 "P1.1"
static int __init early_parse_hw_level(char *p)
{
	/* just p01 is nuvlota */
	if (p) {
		if (!strcmp(p, "P0.1") && strlen(p) == strlen("P0.1"))
			is_nvt_rx = true;
		if ((!strcmp(p, BOARD_P01) && strlen(p) == strlen(BOARD_P01))
			|| (!strcmp(p, BOARD_P1) && strlen(p) == strlen(BOARD_P1))
			|| (!strcmp(p, BOARD_P11) && strlen(p) == strlen(BOARD_P11))
			)
			need_unconfig_pg = true;

	}
	return 0;
}

early_param("androidboot.hwlevel", early_parse_hw_level);

module_init(rx1619_init);
module_exit(rx1619_exit);

MODULE_AUTHOR("colin");
MODULE_DESCRIPTION("NUVOLTA Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL/BSD");
