/**
 * Copyright “Copyright (C) 2019 XiaoMi, Inc
 *
**/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <asm/unaligned.h>
/*add for sdm845 request*/
#include <idtp9220.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/memory.h>

/* add for run ln8282 func*/
#include <linux/power/ln8282.h>

/*
#ifdef CONFIG_DRM
#include <drm/drm_notifier.h>
#endif
 */

static struct idtp9220_device_info *g_di;

struct idtp9220_access_func {
	int (*read)(struct idtp9220_device_info *di, u16 reg, u8 *val);
	int (*write)(struct idtp9220_device_info *di, u16 reg, u8 val);
	int (*read_buf)(struct idtp9220_device_info *di,
			u16 reg, u8 *buf, u32 size);
	int (*write_buf)(struct idtp9220_device_info *di,
			u16 reg, u8 *buf, u32 size);
	int (*mask_write)(struct idtp9220_device_info *di,
			u16 reg, u8 mask, u8 val);
};

struct idtp9220_dt_props {
	unsigned int irq_gpio;
	unsigned int enable_gpio;
	unsigned int wpc_det_gpio;
	unsigned int reverse_gpio;
};

struct idtp9220_device_info {
	int chip_enable;
	char *name;
	struct device *dev;
	struct idtp9220_access_func bus;
	struct regmap    *regmap;
	struct idtp9220_dt_props dt_props;
	int irq;
	int wpc_irq;
	struct delayed_work	irq_work;
	struct delayed_work wpc_det_work;
	struct pinctrl *idt_pinctrl;
	struct pinctrl_state *idt_gpio_active;
	struct pinctrl_state *idt_gpio_suspend;
	struct power_supply	*usb_psy;
	struct power_supply	*dc_psy;
	struct power_supply	*batt_psy;
	struct power_supply	*idtp_psy;
	struct power_supply	*wireless_psy;
	struct power_supply	*ln_psy;
	struct mutex	read_lock;
	struct mutex	write_lock;
	struct mutex	sysfs_op_lock;
	struct mutex	reverse_op_lock;
	struct delayed_work	chg_monitor_work;
	struct delayed_work	chg_detect_work;
	struct delayed_work	bpp_connect_load_work;
	struct delayed_work	epp_connect_load_work;
	struct delayed_work	cmd_check_work;
	struct delayed_work	vout_regulator_work;
	struct delayed_work	load_fod_param_work;
	struct delayed_work	rx_vout_work;
	struct delayed_work	dc_check_work;
	struct delayed_work	fast_operate_work;
	struct delayed_work	initial_tx_work;
	struct delayed_work	bpp_e5_tx_work;
	struct delayed_work	qc2_f1_tx_work;
	struct delayed_work	qc3_epp_work;
/*
reverse work
*/
	struct delayed_work	reverse_ept_type_work;
	struct delayed_work	reverse_chg_state_work;
	struct delayed_work	reverse_dping_state_work;
	struct delayed_work	reverse_sent_state_work;

#ifdef IDTP9220_SRAM_UPDATE
	struct delayed_work	sram_update_work;
#endif

/*
Factory Test Work
*/
	struct delayed_work	get_vout_work;
	struct delayed_work	get_iout_work;

	/*
#ifdef CONFIG_DRM
struct notifier_block		wireless_fb_notif;
struct mutex			screen_lock;
bool				screen_icl_status;
#endif
	 */
	bool				screen_on;
	int tx_charger_type;
	int status;
	int count_5v;
	int count_9v;
	int count_10v;
	int count_15v;
	int exchange;
	int epp_exchange;
	int dcin_present;
	int epp;
	int power_off_mode;
	int power_max;
	u8  header;
	u8  cmd;
	int is_compatible_hwid;
	int last_vin;
	int is_car_tx;
	int is_ble_tx;
	int last_icl;
	int power_good_flag;

	/*idt9220 charging info*/
	int vout;
	int iout;
	int f;
	int vrect;
	int ss;
	int is_vin_limit;
	int is_epp_qc3;
	/* bpp e5_tx info*/
	int last_bpp_icl;
	int last_bpp_vout;
	/* qc2+f1_tx info*/
	int is_f1_tx;
	int bpp_vout_rise;
	int last_qc2_vout;
	int last_qc2_icl;
	/* qc3_epp+f1_tx info*/
	int last_qc3_vout;
	int last_qc3_icl;
	bool fw_update;
	int op_mode;
	int adapter_voltage;
	bool first_rise_flag;
	bool enable_ext5v;
	bool disable_1390;
	bool vswitch_ok;
	int is_reverse_mode;
	int is_reverse_chg;
	int is_boost_mode;
};

void idtp922x_request_adapter(struct idtp9220_device_info *di);
static void idtp9220_set_charging_param(struct idtp9220_device_info *di);
int program_fw(struct idtp9220_device_info *di, u16 destAddr, u8 *src, u32 size);
static int program_bootloader(struct idtp9220_device_info *di);
static int idtp9220_set_reverse_enable(struct idtp9220_device_info *di, int enable);

/*static int idt_signal_strength = 0;
  module_param_named(ss, idt_signal_strength, int, 0600);
 */

static int idt_signal_range = 2;
module_param_named(signal_range, idt_signal_range, int, 0644);

/*
#ifdef CONFIG_DRM
static int wireless_fb_notifier_cb(struct notifier_block *self,
unsigned long event, void *data);
#endif
 */

static int idtp9220_get_property_names(struct idtp9220_device_info *di)
{
	di->batt_psy = power_supply_get_by_name("battery");
	if (!di->batt_psy) {
		dev_err(di->dev, "[idt] no batt_psy,return\n");
		return -EINVAL;
	}
	di->dc_psy = power_supply_get_by_name("dc");
	if (!di->dc_psy) {
		dev_err(di->dev, "[idt] no dc_psy,return\n");
		return -EINVAL;
	}
	di->usb_psy = power_supply_get_by_name("usb");
	if (!di->usb_psy) {
		dev_err(di->dev, "[idt] no usb_psy,return\n");
		return -EINVAL;
	}
	di->wireless_psy = power_supply_get_by_name("wireless");
	if (!di->wireless_psy) {
		dev_err(di->dev, "[idt] no wireless_psy,return\n");
		return -EINVAL;
	}
	return 0;
}


int idtp9220_read(struct idtp9220_device_info *di, u16 reg, u8 *val) {
	unsigned int temp;
	int rc;

	mutex_lock(&di->read_lock);
	rc = regmap_read(di->regmap, reg, &temp);
	if (rc >= 0)
		*val = (u8)temp;

	mutex_unlock(&di->read_lock);
	return rc;
}

int idtp9220_write(struct idtp9220_device_info *di, u16 reg, u8 val) {
	int rc = 0;

	mutex_lock(&di->write_lock);
	rc = regmap_write(di->regmap, reg, val);
	if (rc < 0)
		dev_err(di->dev, "[idt] idtp9220 write error: %d\n", rc);

	mutex_unlock(&di->write_lock);
	return rc;
}

int idtp9220_masked_write(struct idtp9220_device_info *di, u16 reg, u8 mask, u8 val)
{
	int rc = 0;

	mutex_lock(&di->write_lock);

	rc = regmap_update_bits(di->regmap, reg, mask, val);
	if (rc < 0)
		dev_err(di->dev, "[idt] idtp9220 write mask error: %d\n", rc);

	mutex_unlock(&di->write_lock);
	return rc;
}

int idtp9220_read_buffer(struct idtp9220_device_info *di, u16 reg, u8 *buf, u32 size) {
	int rc =0;

	while (size--) {
		rc = di->bus.read(di, reg++, buf++);
		if (rc < 0) {
			dev_err(di->dev, "[idt] write error: %d\n", rc);
			return rc;
		}
	}

	return rc;
}

int idtp9220_write_buffer(struct idtp9220_device_info *di, u16 reg, u8 *buf, u32 size) {
	int rc = 0;

	while (size--) {
		rc = di->bus.write(di, reg++, *buf++);
		if (rc < 0) {
			dev_err(di->dev, "[idt] write error: %d\n", rc);
			return rc;
		}
	}

	return rc;
}

u32 ExtractPacketSize(u8 hdr) {
	if (hdr < 0x20)
		return 1;
	if (hdr < 0x80)
		return (2 + ((hdr - 0x20) >> 4));
	if (hdr < 0xe0)
		return (8 + ((hdr - 0x80) >> 3));
	return (20 + ((hdr - 0xe0) >> 2));
}

void idtp922x_clrInt(struct idtp9220_device_info *di, u8 *buf, u32 size) {
	di->bus.write_buf(di, REG_SYS_INT_CLR, buf, size);
	di->bus.write(di, REG_SSCMND, CLRINT);
}

void reverse_clrInt(struct idtp9220_device_info *di, u8 *buf, u32 size) {
	di->bus.write_buf(di, REG_SYS_INT_CLR, buf, size);
	di->bus.write(di, REG_TX_CMD, TX_FOD_EN | TX_CLRINT);
}

void idtp922x_enable_ext5v(struct idtp9220_device_info *di)
{
	dev_info(di->dev, "enable external 5v\n");
	di->bus.write(di, REG_EXTERNAL_5V, BIT(0));
}

void idtp922x_sendPkt(struct idtp9220_device_info *di, ProPkt_Type *pkt) {
	u32 size = ExtractPacketSize(pkt->header)+1;
	di->bus.write_buf(di, REG_PROPPKT, (u8 *)pkt, size);
	di->bus.write(di, REG_SSCMND, SENDPROPP);

	dev_info(di->dev, "pkt header: 0x%x and cmd: 0x%x\n",
			pkt->header, pkt->cmd);
	di->header = pkt->header;
	di->cmd = pkt->cmd;
}

void idtp922x_receivePkt(struct idtp9220_device_info *di, u8 *buf) {
	u8 header;
	int rc;
	u32 size;

	di->bus.read(di, REG_BCHEADER, &header);
	size = ExtractPacketSize(header)+1;
	rc = di->bus.read_buf(di, REG_BCDATA, buf, size);
	if (rc < 0)
		dev_err(di->dev, "[idt] read Tx data error: %d\n", rc);
}

void idtp922x_set_adap_vol(struct idtp9220_device_info *di, u16 mv)
{
	dev_info(di->dev, "set adapter vol to %d\n", mv);
	di->bus.write(di, REG_FC_VOLTAGE_L, mv&0xff);
	di->bus.write(di, REG_FC_VOLTAGE_H, (mv>>8)&0xff);
	di->bus.write(di, REG_SSCMND, VSWITCH);
}


void idtp922x_set_pmi_icl(struct idtp9220_device_info *di, int mA)
{
	union power_supply_propval val = {0, };
	int rc;

	rc = idtp9220_get_property_names(di);
	val.intval = mA;
	power_supply_set_property(di->dc_psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
}

/* Adapter Type */
/* Adapter_list = {0x00:'ADAPTER_UNKNOWN',  */
/*            0x01:'SDP 500mA',  */
/*            0x02:'CDP 1.1A',  */
/*            0x03:'DCP 1.5A',  */
/*            0x05:'QC2.0',  */
/*            0x06:'QC3.0',  */
/*            0x07:'PD',} */
void idtp922x_request_adapter(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY18;
	pkt.cmd = BC_ADAPTER_TYPE;

	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_request_uuid(struct idtp9220_device_info *di, int is_epp)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY18;
	if (is_epp)
		pkt.cmd = BC_TX_HWID;
	else
		pkt.cmd = BC_TX_COMPATIBLE_HWID;
	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_get_tx_vin(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY18;
	pkt.cmd = BC_READ_Vin;

	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_retry_cmd(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = di->header;
	pkt.cmd = di->cmd;

	idtp922x_sendPkt(di, &pkt);
}


static int idtp9220_get_vout_regulator(struct idtp9220_device_info *di)
{
	u8 vout_l, vout_h;
	u16 vout;

	if (!di)
		return 0;

	di->bus.read(di, REG_REGULATOR_L, &vout_l);
	di->bus.read(di, REG_REGULATOR_H, &vout_h);
	vout = vout_l | ((vout_h & 0xff)<< 8);
	dev_info(di->dev, "vout regulator get vol: %d\n", vout);

	return vout;
}
/*
   void idtp9220_set_toggle_mode(struct idtp9220_device_info *di)
   {
   ProPkt_Type pkt;
   pkt.header = PROPRIETARY18;
   pkt.cmd = BC_TX_TOGGLE;
   idtp922x_sendPkt(di, &pkt);
   }
 */
void idtp9220_retry_id_auth(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY38;
	pkt.cmd = BC_RX_ID_AUTH;

	pkt.data[0] = 0x02;
	pkt.data[1] = 0xbb;

	idtp922x_sendPkt(di, &pkt);
}

static int idtp9220_set_vout_regulator(struct idtp9220_device_info *di, int mv)
{
	u8 vout_l, vout_h;
	u16 vout;

	if (!di)
		return 0;
	vout_l = mv & 0xff;
	vout_h = mv >> 8;

	dev_info(di->dev, "vout regulator vout_l: 0x%x and vout_h: 0x%x\n", vout_l, vout_h);
	di->bus.write(di, REG_REGULATOR_L, vout_l);
	di->bus.write(di, REG_REGULATOR_H, vout_h);

	vout = vout_l | ((vout_h & 0xff) << 8);
	dev_info(di->dev, "vout regulator set vol: %d\n", vout);

	return vout;
}

#if 0
static int idtp9220_get_vbuck(struct idtp9220_device_info *di)
{
	u8 vbuck_l, vbuck_h;
	u16 vbuck_ret;

	if (!di)
		return 0;

	di->bus.read(di, 0x08, &vbuck_l);
	di->bus.read(di, 0x09, &vbuck_h);
	vbuck_ret = vbuck_l | (vbuck_h << 8);

	return vbuck_ret;
}
#endif

static int idtp9220_get_vout(struct idtp9220_device_info *di)
{
	u8 vout_l, vout_h;

	if (!di)
		return 0;

	di->bus.read(di, REG_ADC_VOUT_L, &vout_l);
	di->bus.read(di, REG_ADC_VOUT_H, &vout_h);
	di->vout = vout_l | ((vout_h & 0xf)<< 8);
	di->vout = di->vout * 10 * 21 * 1000 / 40950 + ADJUST_METE_MV;

	dev_info(di->dev, "%s: vout is %d\n", __func__, di->vout);
	return di->vout;
}

static void idtp9220_set_vout(struct idtp9220_device_info *di, int mv)
{
	u16 val;
	u8 vout_l, vout_h;
	if (!di)
		return;
	val = (mv - 2800) * 10 / 84;
	vout_l = val & 0xff;
	vout_h = val >> 8;
	di->bus.write(di, REG_VOUT_SET, vout_l);
	di->bus.write(di, REG_VRECT_ADJ, vout_h);
	dev_info(di->dev, "[idtp9220]: set vout voltage: %d %d\n", mv, val);
}

/* delete because idt9415 can't set rx reset
static void idtp9220_set_reset(struct idtp9220_device_info *di)
{
	if(!di)
		return;
	di->bus.write(di, REG_RX_RESET, 0x01);
	dev_info(di->dev, "[idtp9220]: set RX reset\n");
}
*/

extern char *saved_command_line;

static int get_cmdline(struct idtp9220_device_info *di)
{
	if (strnstr(saved_command_line, "androidboot.mode=",
				strlen(saved_command_line))) {

		di->power_off_mode = 1;
		dev_info(di->dev, "[idtp9220]: enter power off charging app\n");
	} else {
		di->power_off_mode = 0;
		dev_info(di->dev, "[idtp9220]: enter normal boot mode\n");
	}
	return 1;
}

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
		if (!strncmp(boot, "7.9.1", strlen("7.9.1")))
			ret = 1;
	}

	return ret;
}

static int idtp9220_get_iout(struct idtp9220_device_info *di)
{
	u8 cout_l, cout_h;

	if (!di)
		return 0;

	di->bus.read(di, REG_RX_LOUT_L, &cout_l);
	di->bus.read(di, REG_RX_LOUT_H, &cout_h);
	di->iout = cout_l | (cout_h << 8);

	return di->iout;
}

static int idtp9220_get_power_profile(struct idtp9220_device_info *di)
{
	u8 mode;
	int ret;

	di->bus.read(di, REG_WPC_MODE, &mode);
	ret = mode & BIT(3);
	dev_info(di->dev, "tx is epp ? ret is 0x%x\n", mode);

	return ret;
}


static int idtp9220_get_freq(struct idtp9220_device_info *di)
{
	u8 data_list[2];

	di->bus.read_buf(di, REG_FREQ_ADDR, data_list, 2);
	di->f = 64*HSCLK/(data_list[0] | (data_list[1] << 8))/10;

	return di->f;
}

static int idtp9220_get_vrect(struct idtp9220_device_info *di)
{
	u8 data_list[2];

	di->bus.read_buf(di, REG_ADC_VRECT, data_list, 2);
	di->vrect = data_list[0] | ((data_list[1] & 0xf)<< 8);
	di->vrect = di->vrect * 125 * 21 * 100 / 40950;

	return di->vrect;
}

static int idtp9220_get_power_max(struct idtp9220_device_info *di)
{
	int power_max;
	u8 val;

	di->bus.read(di, REG_POWER_MAX, &val);
	power_max = (val/2)*1000;
	dev_info(di->dev, "rx power max is %dmW\n", power_max);

	return power_max;
}

/*
   static int idtp9220_get_signal_strength(struct idtp9220_device_info *di)
   {
   u8 ss;

   di->bus.read(di, REG_SIGNAL_STRENGTH, &ss);
   if(ss >= 95)
   idt_signal_range = 1;
   else
   idt_signal_range = 0;

   dev_info(di->dev, "[idt] signal strength: %d\n", ss);
   di->ss = idt_signal_range;

   return ss;
   }
 */

static void idtp9220_send_device_auth(struct idtp9220_device_info *di)
{
	di->bus.write(di, REG_SSCMND, SEND_DEVICE_AUTH);
}

static ssize_t chip_version_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	u8 chip_id_l, chip_id_h, chip_rev, cust_id, status, vset;
	u8 fw_otp_ver[4], fw_app_ver[4];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	di->bus.read(di, REG_STATUS_L, &status);
	di->bus.read(di, REG_VOUT_SET, &vset);

	di->bus.read(di, REG_CHIP_ID_L, &chip_id_l);
	di->bus.read(di, REG_CHIP_ID_H, &chip_id_h);
	di->bus.read(di, REG_CHIP_REV, &chip_rev);
	chip_rev = chip_rev  >> 4;
	di->bus.read(di, REG_CTM_ID, &cust_id);
	di->bus.read_buf(di, REG_OTPFWVER_ADDR, fw_otp_ver, 4);
	di->bus.read_buf(di, REG_EPRFWVER_ADDR, fw_app_ver, 4);

	return sprintf(buf, "chip_id_l:%02x\nchip_id_h:%02x\nchip_rev:%02x\ncust_id:%02x status:%02x vset:%02x\n otp_ver:%x.%x.%x.%x\n app_ver:%x.%x.%x.%x\n",
			chip_id_l, chip_id_h, chip_rev, cust_id, status, vset,
			fw_otp_ver[0], fw_otp_ver[1], fw_otp_ver[2], fw_otp_ver[3],
			fw_app_ver[0], fw_app_ver[1], fw_app_ver[2], fw_app_ver[3]);
}

/* voltage limit attrs */
static ssize_t chip_vout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	int vout;

	vout = idtp9220_get_vout(di);
	return sprintf(buf, "%d\n", vout);

}

static ssize_t chip_vout_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	index = (int)simple_strtoul(buf, NULL, 16);
	/*
	   if ((index < VOUT_VAL_3500_MV) || (index > VOUT_VAL_5000_MV)) {
	   dev_err(di->dev, "Store Val %s is invalid!\n", buf);
	   return count;
	   }
	 */

	idtp9220_set_vout(di, index);

	return count;
}

static ssize_t vout_regulator_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	int vout;

	vout = idtp9220_get_vout_regulator(di);

	return sprintf(buf, "Vout ADC Value: %dMV\n", vout);
}

#define VOUT_MIN_4900_MV	4900
#define VOUT_MAX_10000_MV	10000
static ssize_t vout_regulator_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int vout;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	vout = (int)simple_strtoul(buf, NULL, 10);
	if ((vout <= VOUT_MIN_4900_MV) || (vout > VOUT_MAX_10000_MV)) {
		dev_err(di->dev, "Store Val %s : %ld is invalid!\n", buf, vout);
		return count;
	}
	idtp9220_set_vout_regulator(di, vout);

	return count;
}

/* current attrs */
static ssize_t chip_iout_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	int cout;

	cout = idtp9220_get_iout(di);

	return sprintf(buf, "%d\n", cout);
}

static ssize_t chip_iout_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int index;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	index = (int)simple_strtoul(buf, NULL, 10);

	if ((index < CURR_VAL_100_MA) || (index > CURR_VAL_1300_MA)) {
		dev_err(di->dev, "Store Val %s is invalid", buf);
		return count;
	}

	di->bus.write(di, REG_ILIM_SET, index);

	return count;
}

static ssize_t chip_freq_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	int f;

	f = idtp9220_get_freq(di);

	return snprintf(buf, sizeof(buf), "Output Current: %dkHz\n", f);
}

static void idtp9220_charging_info(struct idtp9220_device_info *di)
{
	if (!di)
		return;

	idtp9220_get_vout(di);
	idtp9220_get_iout(di);
	idtp9220_get_freq(di);
	idtp9220_get_vrect(di);

	dev_info(di->dev, "%s:Vout:%dmV,Iout:%dmA,Freq:%dKHz,Vrect:%dmV,SS:%d\n", __func__,
			di->vout, di->iout, di->f, di->vrect, di->ss);
}

static int idtp9220_reverse_charge_enable(struct idtp9220_device_info *di)
{
	u8 mode;
	int ret = 0;
	union power_supply_propval cp_val = {0, };

	if (!di->fw_update) {
		di->bus.read(di, REG_WPC_MODE, &mode);
		dev_info(di->dev, "wpc mode(0x004D): 0x%x\n", mode);
		ret = mode & BIT(0);
		if (ret) {
			dev_info(di->dev, "rx mode, can not enable reverse charge\n");
			di->is_reverse_mode = 0;
			di->is_reverse_chg = 1;
			idtp9220_set_reverse_enable(di, false);
			schedule_delayed_work(&di->reverse_sent_state_work, 0);
			return 0;
		}
	}

	dev_info(di->dev, "reverse charge, set ln8282 powerpath and opmode\n");
	if (di->wireless_psy) {
		cp_val.intval = 3;
		power_supply_set_property(di->wireless_psy,
				POWER_SUPPLY_PROP_DIV_2_MODE, &cp_val);
	}
/*
	ln8282_set_powerpath_ext(false);
	ln8282_change_opmode_ext(LN8282_OPMODE_SWITCHING);
*/

	if (!di->fw_update) {
		di->bus.write(di, REG_TX_CMD, TX_FOD_EN | TX_EN);
		msleep(50);
		di->bus.read(di, REG_TX_DATA, &mode);
		di->is_reverse_mode = 1;
		schedule_delayed_work(&di->reverse_dping_state_work, 10 * HZ);
		dev_info(di->dev, "tx data(0078): 0x%x\n", mode);
		ret = mode & BIT(0);
		if (ret)
			dev_info(di->dev, "start reverse charging\n");
		else
			dev_err(di->dev, "idt set reverse charge failed\n");
	}

	return 0;
}

static int idtp9220_set_reverse_gpio(struct idtp9220_device_info *di, int enable)
{
	int ret;

	if (gpio_is_valid(di->dt_props.reverse_gpio)) {
		ret = gpio_request(di->dt_props.reverse_gpio,
				"reverse-enable-gpio");
		if (ret) {
			dev_err(di->dev,
					"%s: unable to request reverse gpio [%d]\n",
					__func__, di->dt_props.reverse_gpio);
		}

		ret = gpio_direction_output(di->dt_props.reverse_gpio, enable);
		if (ret) {
			dev_err(di->dev,
					"%s: cannot set direction for reverse enable gpio [%d]\n",
					__func__, di->dt_props.reverse_gpio);
		}
		gpio_free(di->dt_props.reverse_gpio);
	} else
		dev_err(di->dev, "%s: unable to set tx_on gpio_130\n");

	return ret;
}

static int idtp9220_set_reverse_enable(struct idtp9220_device_info *di, int enable)
{
	int ret = 0;
	union power_supply_propval val = {0, };

	di->wireless_psy = power_supply_get_by_name("wireless");
	if (!di->wireless_psy) {
		dev_err(di->dev, "[idt] no wireless_psy,return\n");
		if (!enable) {
			di->is_reverse_mode = 0;
			cancel_delayed_work(&di->reverse_chg_state_work);
			cancel_delayed_work(&di->reverse_dping_state_work);
			cancel_delayed_work(&di->reverse_ept_type_work);
			idtp9220_set_reverse_gpio(di, false);
		}
		return -EINVAL;
	}

	idtp9220_set_reverse_gpio(di, enable);

	if (di->fw_update) {
		msleep(100);
		if (di->wireless_psy) {
			val.intval = 4;
			power_supply_set_property(di->wireless_psy,
				POWER_SUPPLY_PROP_DIV_2_MODE, &val);
		}
		return ret;
	}

	if (enable) {
		di->is_boost_mode = 1;
		val.intval = 1;
		power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_SW_DISABLE_DC_EN, &val);
		idtp9220_reverse_charge_enable(di);
	} else {
		di->is_boost_mode = 0;
		dev_info(di->dev, "disable reverse charging for wireless\n");
		power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN, &val);
		di->is_reverse_mode = 0;
		cancel_delayed_work(&di->reverse_chg_state_work);
		cancel_delayed_work(&di->reverse_dping_state_work);
		cancel_delayed_work(&di->reverse_ept_type_work);
		return ret;
	}


	return ret;
}

/* reverse enable attrs */
static ssize_t reverse_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (gpio_is_valid(di->dt_props.reverse_gpio))
		ret = gpio_get_value(di->dt_props.reverse_gpio);
	else {
		dev_err(di->dev, "%s: reverse gpio not provided\n", __func__);
		ret = -1;
	}

	dev_info(di->dev, "reverse enable gpio: %d\n", ret);

	return snprintf(buf, sizeof(buf), "reverse enable: %d\n", ret);
}

static ssize_t reverse_gpio_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret, enable;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;

	idtp9220_set_reverse_gpio(di, enable);

	return count;
}

static ssize_t reverse_gpio_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (gpio_is_valid(di->dt_props.reverse_gpio))
		ret = gpio_get_value(di->dt_props.reverse_gpio);
	else {
		dev_err(di->dev, "%s: reverse gpio not provided\n", __func__);
		ret = -1;
	}

	dev_info(di->dev, "reverse enable gpio: %d\n", ret);

	return scnprintf(buf, PAGE_SIZE, "reverse enable: %d\n", ret);
}

static ssize_t reverse_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret, enable;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;
	di->is_reverse_chg = 0;
	schedule_delayed_work(&di->reverse_sent_state_work, 0);
	idtp9220_set_reverse_enable(di, enable);
	return count;
}


/* chip enable attrs */
static ssize_t chip_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (gpio_is_valid(di->dt_props.enable_gpio))
		ret = gpio_get_value(di->dt_props.enable_gpio);
	else {
		dev_err(di->dev, "%s: sleep gpio not provided\n", __func__);
		ret = -1;
	}

	dev_info(di->dev, "chip enable gpio: %d\n", ret);

	return sprintf(buf, "Chip enable: %d\n", !ret);
}

static int idtp9220_set_present(struct idtp9220_device_info *di, int enable)
{
	int ret = 0;
	union power_supply_propval val = {0, };

	dev_info(di->dev, "[idtp] dc plug %s\n", enable ? "in" : "out");
	if (enable)
	{
		di->dcin_present = true;
		di->ss = 1;
		cancel_delayed_work(&di->fast_operate_work);
	} else {

		schedule_delayed_work(&di->fast_operate_work, msecs_to_jiffies(200));
		di->status = NORMAL_MODE;
		di->count_9v = 0;
		di->count_5v = 0;
		di->count_10v = 0;
		di->count_15v = 0;
		di->exchange = 0;
		di->epp_exchange = 0;
		di->dcin_present = false;
		idt_signal_range = 2;
		di->ss = 2;
		di->epp = 0;
		di->last_vin = 0;
		di->last_icl = 0;
		di->is_car_tx = 0;
		di->power_off_mode = 0;
		di->is_epp_qc3 = 0;
		di->is_vin_limit = 0;
		di->bpp_vout_rise = 0;
		di->is_f1_tx = 0;
		di->last_bpp_vout = 0;
		di->last_bpp_icl = 0;
		di->last_qc2_vout = 0;
		di->last_qc2_icl = 0;
		di->last_qc3_vout = 0;
		di->last_qc3_icl = 0;
		di->fw_update = false;
		di->tx_charger_type = ADAPTER_NONE;
		di->op_mode = LN8282_OPMODE_UNKNOWN;
		di->adapter_voltage = 0;
		di->first_rise_flag = false;
		di->enable_ext5v = false;
		di->disable_1390 = true;
		di->vswitch_ok = false;
		cancel_delayed_work(&di->chg_monitor_work);
		cancel_delayed_work(&di->cmd_check_work);
		di->ln_psy = power_supply_get_by_name("lionsemi");
		if (di->ln_psy)
			power_supply_set_property(di->ln_psy,
				POWER_SUPPLY_PROP_RESET_DIV_2_MODE, &val);
	}

	return ret;
}

static int idtp9220_set_enable_mode(struct idtp9220_device_info *di, int enable)
{
	int ret = 0;

	if (gpio_is_valid(di->dt_props.enable_gpio)) {
		ret = gpio_request(di->dt_props.enable_gpio,
				"idt-enable-gpio");
		if (ret) {
			dev_err(di->dev,
					"%s: unable to request idt enable gpio [%d]\n",
					__func__, di->dt_props.enable_gpio);
		}

		ret = gpio_direction_output(di->dt_props.enable_gpio, !enable);
		if (ret) {
			dev_err(di->dev,
					"%s: cannot set direction for idt enable gpio [%d]\n",
					__func__, di->dt_props.enable_gpio);
		}
		gpio_free(di->dt_props.enable_gpio);
	}

	return ret;
}

static ssize_t chip_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	int ret, enable;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;

	idtp9220_set_enable_mode(di, enable);

	return count;
}

/*print the result of fw program*/
static ssize_t chip_fw_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);



	di->fw_update = true;
	idtp9220_set_reverse_gpio(di, true);
	msleep(100);
	if (!program_fw(di, 0x0000, idt_firmware_otp, sizeof(idt_firmware_otp))) {
		dev_err(&client->dev, "program fw failed.\n");
		ret = 0;
	} else {
		dev_err(&client->dev, "program fw sucess.\n");
		ret = 1;
	}

	msleep(100);
	idtp9220_set_reverse_gpio(di, false);
	di->fw_update = false;

	return sprintf(buf, "Chip enable: %d\n", ret);
}



static DEVICE_ATTR(chip_enable, S_IWUSR | S_IRUGO, chip_enable_show, chip_enable_store);
static DEVICE_ATTR(reverse_enable, S_IWUSR | S_IRUGO, reverse_enable_show, reverse_enable_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IWUSR | S_IRUGO, chip_iout_show, chip_iout_store);
static DEVICE_ATTR(chip_freq, S_IRUGO, chip_freq_show, NULL);
static DEVICE_ATTR(vout_regulator, S_IWUSR | S_IRUGO, vout_regulator_show, vout_regulator_store);
static DEVICE_ATTR(chip_fw, S_IWUSR | S_IRUGO, chip_fw_show, NULL);
static DEVICE_ATTR(reverse_set_gpio, S_IWUSR | S_IRUGO, reverse_gpio_show, reverse_gpio_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_chip_version.attr,
	&dev_attr_chip_vout.attr,
	&dev_attr_chip_iout.attr,
	&dev_attr_chip_freq.attr,
	&dev_attr_chip_enable.attr,
	&dev_attr_reverse_enable.attr,
	&dev_attr_vout_regulator.attr,
	&dev_attr_chip_fw.attr,
	&dev_attr_reverse_set_gpio.attr,
	NULL,
};

static const struct attribute_group sysfs_group_attrs = {
	.attrs = sysfs_attrs,
};

#if 1
static int program_bootloader(struct idtp9220_device_info *di)
{
	int i, rc = 0;
	int len;

	len = sizeof(bootloader_data);

	for (i = 0; i < len; i++) {
		rc = di->bus.write(di, 0x0800+i, bootloader_data[i]);
		if (rc)
			return rc;
	}

	return 0;
}

int program_fw(struct idtp9220_device_info *di, u16 destAddr, u8 *src, u32 size) {
	int i, j;
	u8 data = 0;








	di->bus.read(di, 0x5870, &data);
	printk(KERN_EMERG "0x5870 %s:%d :%02x\n", __func__, __LINE__, data);
	di->bus.read(di, 0x5874, &data);
	printk(KERN_EMERG "0x5874 %s:%d :%02x\n", __func__, __LINE__, data);

	if (di->bus.write(di, 0x3000, 0x5a))
		return false;
	if (di->bus.write(di, 0x3004, 0x00))
		return false;
	if (di->bus.write(di, 0x3008, 0x09))
		return false;
	if (di->bus.write(di, 0x300C, 0x05))
		return false;
	if (di->bus.write(di, 0x300D, 0x1D))
		return false;
	if (di->bus.write(di, 0x3040, 0x11))
		return false;
	if (di->bus.write(di, 0x3040, 0x10))
		return false;
	if (program_bootloader(di))
		return false;

	di->bus.write(di, 0x0400, 0x00);
	if (di->bus.write(di, 0x3048, 0xD0))
		return false;

	/* ignoreNAK */
	di->bus.write(di, 0x3040, 0x80);
	mdelay(100);
	printk(KERN_EMERG "%s:%d\n", __func__, __LINE__);

#if 0
	for (i = 0; i < 512; i++) {
		di->bus.read(di, 0x1c00+i, data_array[i]);
		if (data_array[i] != bootloader_data[i]) {
			printk(KERN_EMERG "MAXUEYUE bootloader check err data[%d]:%02x != boot[%d]:%02x.\n", i, data_array[i], i, bootloader_data[i]);
			return 1;
		}
		printk(KERN_EMERG "%02x ", data_array[i]);
		if (i+1 % 16 == 0)
			printk(KERN_EMERG "\n");
	}
#endif




	for (i = destAddr; i < destAddr+size; i += 128) {



		char sBuf[136];
		u16 StartAddr = (u16)i;
		u16 CheckSum = StartAddr;
		u16 CodeLength = 128;
		int retry_cnt = 0;

		memset(sBuf, 0, 136);



		memcpy(sBuf + 8, src, 128);
		src += 128;







		/*
		//(2) Calculate the packet checksum of the 128-byte data, StartAddr, and CodeLength
		for (j = 127; j >= 0; j--) {        // find the 1st non zero value byte from the end of the sBuf[] buffer
		if (sBuf[j + 8] != 0)
		break;
		else
		CodeLength--;
		}
		if (CodeLength == 0)
		continue;            // skip programming if nothing to program
		 */

		for (j = 127; j >= 0; j--)
			CheckSum += sBuf[j + 8];
		CheckSum += CodeLength;


		memcpy(sBuf+2, &StartAddr, 2);
		memcpy(sBuf+4, &CodeLength, 2);
		memcpy(sBuf+6, &CheckSum, 2);






		for (j = 0; j < CodeLength + 8; j++) {
			if (di->bus.write(di, 0x400 + j, sBuf[j])) {
				printk("on writing to OTP buffer error");
				return false;
			}
		}





		if (di->bus.write(di, 0x400, 1))    {
			printk("on OTP buffer validation error");
			return false;
		}















		do {
			mdelay(100);
			di->bus.read(di, 0x400, sBuf);
			if (sBuf[0] == 1) {
				printk("Programming OTP buffer status sBuf:%02x i:%d\n", sBuf[0], i);
			}
			if (retry_cnt++ > 5)
				break;
		} while (sBuf[0] == 1);

		if (retry_cnt > 5) {
			printk("Programming OTP buffer retry failed :%d\n", retry_cnt);
			return false;
		} else {
			di->bus.read(di, 0x401, sBuf);
			if (sBuf[0] != 2) {
				printk("buffer write to OTP returned status:%d :%s\n", sBuf[0], "X4");
				return false;
			} else {
				printk("Program OTP 0x%04x\n", i);
			}
		}

		/*
		   if (sBuf[0] != 2) {        // not OK
		   printk("buffer write to OTP returned status:%d :%s\n" , sBuf[0], "X4");
		   return false;
		   } else {
		   printk("Program OTP 0x%04x\n", i);
		   }
		 */
	}




	if (di->bus.write(di, 0x3000, 0x5a)) return false;
	if (di->bus.write(di, 0x3048, 0x00)) return false;

	return true;
}
#endif

static int idtp9220_parse_dt(struct idtp9220_device_info *di)
{
	struct device_node *node = di->dev->of_node;

	if (!node) {
		dev_err(di->dev, "device tree node missing\n");
		return -EINVAL;
	}

	di->dt_props.irq_gpio = of_get_named_gpio(node, "idt,irq", 0);
	if ((!gpio_is_valid(di->dt_props.irq_gpio)))
		return -EINVAL;

	di->dt_props.enable_gpio = of_get_named_gpio(node, "idt,enable", 0);
	if ((!gpio_is_valid(di->dt_props.enable_gpio)))
		return -EINVAL;

	di->dt_props.reverse_gpio = of_get_named_gpio(node, "idt,reverse-enable", 0);
	if ((!gpio_is_valid(di->dt_props.reverse_gpio)))
		return -EINVAL;

	di->dt_props.wpc_det_gpio = of_get_named_gpio(node, "idt,wpc-det", 0);
	if ((!gpio_is_valid(di->dt_props.irq_gpio)))
		return -EINVAL;

	return 0;
}

static int idtp9220_gpio_init(struct idtp9220_device_info *di)
{
	int ret = 0;
	int irqn = 0;
	int wpc_irq = 0;

	di->idt_pinctrl = devm_pinctrl_get(di->dev);
	if (IS_ERR_OR_NULL(di->idt_pinctrl)) {
		dev_err(di->dev, "No pinctrl config specified\n");
		ret = PTR_ERR(di->dev);
		return ret;
	}
	di->idt_gpio_active =
		pinctrl_lookup_state(di->idt_pinctrl, "idt_active");
	if (IS_ERR_OR_NULL(di->idt_gpio_active)) {
		dev_err(di->dev, "No active config specified\n");
		ret = PTR_ERR(di->idt_gpio_active);
		return ret;
	}
	di->idt_gpio_suspend =
		pinctrl_lookup_state(di->idt_pinctrl, "idt_suspend");
	if (IS_ERR_OR_NULL(di->idt_gpio_suspend)) {
		dev_err(di->dev, "No suspend config specified\n");
		ret = PTR_ERR(di->idt_gpio_suspend);
		return ret;
	}

	ret = pinctrl_select_state(di->idt_pinctrl,
			di->idt_gpio_active);
	if (ret < 0) {
		dev_err(di->dev, "fail to select pinctrl active rc=%d\n",
				ret);
		return ret;
	}

	if (gpio_is_valid(di->dt_props.irq_gpio)) {
		irqn = gpio_to_irq(di->dt_props.irq_gpio);
		if (irqn < 0) {
			ret = irqn;
			goto err_irq_gpio;
		}
		di->irq = irqn;
	} else {
		dev_err(di->dev, "%s: irq gpio not provided\n", __func__);
		goto err_irq_gpio;
	}

	if (gpio_is_valid(di->dt_props.wpc_det_gpio)) {
		wpc_irq = gpio_to_irq(di->dt_props.wpc_det_gpio);
		if (wpc_irq < 0) {
			ret = wpc_irq;
			goto err_wpc_irq;
		}
		di->wpc_irq = wpc_irq;
	} else {
		dev_err(di->dev, "%s: wpc irq gpio not provided\n", __func__);
		goto err_wpc_irq;
	}

err_wpc_irq:
	gpio_free(di->dt_props.wpc_det_gpio);
err_irq_gpio:
	gpio_free(di->dt_props.irq_gpio);
	return ret;
}

static bool need_irq_cleared(struct idtp9220_device_info *di)
{
	u8 int_buf[4];
	u32 int_val;
	int rc = -1;

	rc = di->bus.read_buf(di, REG_SYS_INT, int_buf, 4);
	if (rc < 0) {
		dev_err(di->dev, "%s: read int state error\n", __func__);
		return true;
	}
	int_val = int_buf[0] | (int_buf[1] << 8) | (int_buf[2] << 16) | (int_buf[3] << 24);
	if (int_val != 0) {
		dev_info(di->dev, "irq not clear right: 0x%08x\n", int_val);
		return true;
	}

	if (gpio_is_valid(di->dt_props.irq_gpio))
		rc = gpio_get_value(di->dt_props.irq_gpio);
	else {
		dev_err(di->dev, "%s: irq gpio not provided\n", __func__);
		rc = -1;
	}
	if (!rc) {
		dev_info(di->dev, "irq low, need clear int: %d\n", rc);
		return true;
	}
	return false;
}

#define VBUCK_QC_VOL		7000
#define VBUCK_FULL_VOL		7000
#define VBUCK_DEFAULT_VOL	5500
#define ADAPTER_DEFAULT_VOL	6000
#define ADAPTER_BPP_QC_VOL	10000
#define ADAPTER_BPP_QC2_VOL	6500
#define ADAPTER_EPP_QC3_VOL	11000
#define ADAPTER_BPP_LIMIT_VOL	6500
#define ADAPTER_VOUT_LIMIT_VOL	7000
#define BPP_VOL_THRESHOLD	8000
#define ADAPTER_BPP_VOL		5000
#define ADAPTER_EPP_MI_VOL	15000
#define EPP_VOL_THRESHOLD	12000
#define EPP_VOL_LIM_THRESHOLD	13000
#define BPP_VOL_LIM_THRESHOLD	10500
#define CHARGING_PERIOD_S	10
#define TAPER_CURR_Limit	950000

static void idtp9220_monitor_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				chg_monitor_work.work);

	idtp9220_charging_info(di);

	idtp9220_set_charging_param(di);

	schedule_delayed_work(&di->chg_monitor_work,
			CHARGING_PERIOD_S * HZ);
}

static void idtp9220_rx_vout_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				rx_vout_work.work);

	idtp9220_set_charging_param(di);

}

static void idtp9220_dc_check_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				dc_check_work.work);

	dev_info(di->dev, "[idt] dc present: %d\n", di->dcin_present);
	if (di->dcin_present) {
		di->ss = 1;
		dev_info(di->dev, "dcin present, quit dc check work\n");
		return;
	} else {
		di->ss = 0;
		dev_info(di->dev, "dcin no present, continue dc check work\n");
		schedule_delayed_work(&di->dc_check_work, msecs_to_jiffies(2500));
	}
	power_supply_changed(di->wireless_psy);
}

static void idtp9220_fast_operate_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				fast_operate_work.work);
	int ret = -1;
	int usb_present, typec_mode;
	union power_supply_propval val = {0, };

	ret = idtp9220_get_property_names(di);
	if (ret < 0)
		return;

	power_supply_get_property(di->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	usb_present = val.intval;

	power_supply_get_property(di->usb_psy,
			POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	typec_mode = val.intval;

	dev_info(di->dev, "usb present:%d typec mode:%d\n",
			usb_present, typec_mode);

	if (gpio_is_valid(di->dt_props.wpc_det_gpio)) {
		ret = gpio_get_value(di->dt_props.wpc_det_gpio);
		/* power good irq will not trigger after insert typec audio/charger
		 * connector while wireless charging. WR for this situation.
		 */
		if ((!usb_present && ret) || (usb_present && ret
					&& (typec_mode == POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER
						|| typec_mode == POWER_SUPPLY_TYPEC_NONE))){
			dev_info(di->dev, "dc out but power_good high, reset by sleep\n");
			idtp9220_set_enable_mode(di, false);
			msleep(10);
			idtp9220_set_enable_mode(di, true);
		}
	}
}

static void idtp9220_chg_detect_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				chg_detect_work.work);

	union power_supply_propval val = {0, };
	union power_supply_propval wk_val = {0, };
	int rc;

	dev_info(di->dev, "[idt] enter %s\n", __func__);

	rc = idtp9220_get_property_names(di);
	if(rc < 0)
		return;

	/*set idtp9220 into sleep mode when usbin*/

	power_supply_get_property(di->usb_psy,
			POWER_SUPPLY_PROP_ONLINE, &val);
	if (val.intval) {
		dev_info(di->dev, "[idt] usb_online:%d set chip disable\n", val.intval);
		idtp9220_set_enable_mode(di, false);
		return;
	}

	if (di->dc_psy) {
		power_supply_get_property(di->dc_psy,
				POWER_SUPPLY_PROP_ONLINE, &val);
		dev_info(di->dev, "[idt] dc_online %d\n", val.intval);
		if (val.intval && di->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(di->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
			di->epp = idtp9220_get_power_profile(di);

			get_cmdline(di);
			if (!di->power_off_mode) {
				idtp9220_set_enable_mode(di, false);
				msleep(10);
				idtp9220_set_enable_mode(di, true);
			}
			else
				schedule_delayed_work(&di->irq_work,
						msecs_to_jiffies(30));
		}
	}
}

#define BPP_QC_7W_CURRENT 750000
#define BPP_DEFAULT_CURRENT 860000
#define EPP_DEFAULT_SWITCH_CURRENT 1720000
#define EPP_DEFAULT_BYPASS_CURRENT 860000
#define DEFAULT_40V_PSNS_CURRENT 120000
#define DC_LOAD_CURRENT 2000000
#define DC_FUL_CURRENT 50000
#define SCREEN_OFF_FUL_CURRENT 250000
#define DC_LOW_CURRENT 360000
#define DC_SDP_CURRENT 500000
#define DC_DCP_CURRENT 500000
#define DC_PD_CURRENT 800000
#define DC_MI_CURRENT_20W  2200000
#define DC_MI_CURRENT_30W  3100000
#define DC_MI_STEP1_CURRENT  1200000
#define DC_QC3_CURRENT 1000000
#define DC_EPP_QC3_CURRENT 1800000
#define DC_QC2_CURRENT 1000000
#define DC_BPP_CURRENT 700000
#define DC_BPP_AUTH_FAIL_CURRENT 700000
#define ICL_EXCHANGE_CURRENT 600000
#define ICL_EXCHANGE_COUNT   5 /*5 = 1min*/
#define EXCHANGE_9V          0x0
#define EXCHANGE_5V          0x1
#define EXCHANGE_15V         0x0
#define EXCHANGE_10V         0x1
#define LIMIT_EPP_IOUT 500
#define LIMIT_BPP_IOUT 500
#define LIMIT_SOC 97
#define TAPER_SOC 95
#define FULL_SOC 100
#define TAPER_VOL 4350000
#define TAPER_CUR -500000

static void idtp9220_bpp_connect_load_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				bpp_connect_load_work.work);
	dev_info(di->dev, "[idt] %s: \n", __func__);
	idtp922x_set_pmi_icl(di, BPP_DEFAULT_CURRENT / 3);
	msleep(300);
	idtp922x_set_pmi_icl(di, (BPP_DEFAULT_CURRENT / 3) * 2);
	msleep(300);
	idtp922x_set_pmi_icl(di, BPP_DEFAULT_CURRENT);
}

static void idtp9220_epp_connect_load_work(struct work_struct *work)
{

	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				epp_connect_load_work.work);
	int dc_load_curr = 0;
	union power_supply_propval cp_val = {0, };

	if (di->wireless_psy) {
		cp_val.intval = 2;
		power_supply_set_property(di->wireless_psy,
			POWER_SUPPLY_PROP_DIV_2_MODE, &cp_val);
		msleep(10);
		power_supply_get_property(di->wireless_psy,
			POWER_SUPPLY_PROP_DIV_2_MODE, &cp_val);
			di->op_mode = cp_val.intval;
		dev_info(di->dev, "first ln8282 set switch and get: %d\n",
			di->op_mode);
	}

	if (di->power_max < 10000)
		dc_load_curr = (di->power_max - 1000)/11 * 1000;
	else {
		if (di->op_mode == LN8282_OPMODE_SWITCHING)
			dc_load_curr = EPP_DEFAULT_SWITCH_CURRENT;
		else
			dc_load_curr = EPP_DEFAULT_BYPASS_CURRENT;
	}
	idtp922x_set_pmi_icl(di, DEFAULT_40V_PSNS_CURRENT);
	msleep(200);
	idtp922x_set_pmi_icl(di, dc_load_curr);
}

static void idtp9220_cmd_check_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				cmd_check_work.work);

	dev_info(di->dev, "[idt] %s: \n", __func__);
	idtp922x_get_tx_vin(di);
	if (di->power_off_mode) {
		schedule_delayed_work(&di->load_fod_param_work,
				msecs_to_jiffies(500));
		schedule_delayed_work(&di->vout_regulator_work,
				msecs_to_jiffies(10));
	}

}

static void idtp9220_initial_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				initial_tx_work.work);
	int ret = 0;
	u8 buf[2] = {0};
	u16 int_val = 0;

	ret = idtp9220_get_power_profile(di);
	if (ret) {
		ret = di->bus.read_buf(di, REG_TX_TYPE, buf, 2);
		if (ret < 0)
			dev_err(di->dev, "read tx type error: %d\n", ret);
		else {
			int_val = buf[0] | (buf[1] << 8);
			dev_info(di->dev, "tx type: 0x%04x\n", int_val);
		}
	}

	/* check dc present to judge device skewing */
	if (int_val == 0x0059) {
		dev_info(di->dev, "mophie tx, start check dc with 8s\n");
		schedule_delayed_work(&di->dc_check_work, msecs_to_jiffies(8000));
	} else
		schedule_delayed_work(&di->dc_check_work, msecs_to_jiffies(2500));
}

static void idtp9220_bpp_e5_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				bpp_e5_tx_work.work);
	int icl_curr = BPP_QC_7W_CURRENT, adapter_vol = ADAPTER_BPP_QC_VOL;
	int soc = 0, batt_sts = 0, health = 0;
	int icl_setted = 0;
	union power_supply_propval val = {0, };
	union power_supply_propval wk_val = {0, };

	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;

		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_HEALTH, &val);
		health = val.intval;
	}

	if (di->tx_charger_type == ADAPTER_QC2)
		icl_curr = 750000;

	if (di->iout < LIMIT_BPP_IOUT && di->exchange == EXCHANGE_9V) {
		di->count_5v++;
		di->count_9v = 0;
	} else if (di->iout > LIMIT_BPP_IOUT && di->exchange == EXCHANGE_5V) {
		di->count_9v++;
		di->count_5v = 0;
	} else {
		di->count_5v = 0;
		di->count_9v = 0;
	}
	/*
	 * 9V-->5V check 6 times
	 * 5V-->9v check 3 times
	 */
	if (di->count_5v > ICL_EXCHANGE_COUNT ||
			(di->exchange == EXCHANGE_5V && di->count_9v <= ICL_EXCHANGE_COUNT - 3))
	{
		dev_info(di->dev, "iout less than 400mA ,set vout to 5.5v\n");
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = DC_BPP_CURRENT;
		di->exchange = EXCHANGE_5V;
	} else if (di->count_9v > (ICL_EXCHANGE_COUNT - 3))
		di->exchange = EXCHANGE_9V;

	if (soc > 90)
		icl_curr = DC_BPP_CURRENT;

	switch (di->status) {
	case NORMAL_MODE:
		if (soc >= TAPER_SOC) {
			di->status = TAPER_MODE;
			adapter_vol = ADAPTER_DEFAULT_VOL;
			icl_curr = min(DC_SDP_CURRENT, icl_curr);
		}
		break;
	case TAPER_MODE:
		dev_info (di->dev, "[bpp] taper mode set vout to 5.5v\n");
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = min(DC_SDP_CURRENT, icl_curr);

		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			di->status = FULL_MODE;
		else if (soc < TAPER_SOC - 1)
			di->status = NORMAL_MODE;
		break;
	case FULL_MODE:
		dev_info (di->dev, "[bpp] charge full set vout to 5.5v\n");
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = SCREEN_OFF_FUL_CURRENT;

		if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
			di->status = RECHG_MODE;
			icl_curr = DC_LOW_CURRENT;
		}
		break;
	case RECHG_MODE:
		dev_info (di->dev, "[bpp] recharge mode set icl to 360mA\n");
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = DC_LOW_CURRENT;

		if (soc < TAPER_SOC - 1)
			di->status = NORMAL_MODE;
		else if (batt_sts == POWER_SUPPLY_STATUS_FULL)
			di->status = FULL_MODE;

		if (di->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(di->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
		}
		break;
	default:
		break;
	}

	switch(health) {
	case POWER_SUPPLY_HEALTH_GOOD:
		break;
	case POWER_SUPPLY_HEALTH_COOL:
		break;
	case POWER_SUPPLY_HEALTH_WARM:
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = min(DC_SDP_CURRENT, icl_curr);
		break;
	case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = min(SCREEN_OFF_FUL_CURRENT, icl_curr);
		break;
	case POWER_SUPPLY_HEALTH_COLD:
	case POWER_SUPPLY_HEALTH_HOT:
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = SCREEN_OFF_FUL_CURRENT;
		break;
	default:
			break;
	}

	if (adapter_vol != di->last_bpp_vout) {
		dev_info(di->dev, "bpp_10w, set new vout: %d, last_vout: %d\n",
				adapter_vol, di->last_bpp_vout);
		if (adapter_vol > BPP_VOL_THRESHOLD) {
			idtp922x_set_adap_vol(di, adapter_vol);
			idtp922x_enable_ext5v(di);
			msleep(400);
		} else {
			idtp922x_set_pmi_icl(di, 100000);
			msleep(100);
			idtp922x_set_adap_vol(di, adapter_vol);
			msleep(2000);
			idtp922x_set_pmi_icl(di, DC_SDP_CURRENT);
			msleep(100);
			icl_setted = 1;
		}
		di->last_bpp_vout = adapter_vol;
	}

	if ((icl_curr != di->last_bpp_icl) || (icl_setted)) {
		dev_info(di->dev, "bpp_10w, set new icl: %d, last_icl: %d\n",
				icl_curr, di->last_bpp_icl);
		di->last_bpp_icl = icl_curr;
		if ((adapter_vol == ADAPTER_BPP_QC_VOL) && (!di->bpp_vout_rise)) {
			dev_info(di->dev, "bpp_10w, vout lower than 8v, set 500mA\n");
			icl_curr = min(DC_SDP_CURRENT, icl_curr);
		}
		idtp922x_set_pmi_icl(di, icl_curr);
	}
}

static void idtp9220_qc2_f1_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				qc2_f1_tx_work.work);
	int soc = 0, batt_sts = 0;
	int vout = ADAPTER_BPP_QC2_VOL;
	int dc_icl = DC_QC2_CURRENT;
	union power_supply_propval val = {0, };

	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;
	}

	if (soc >= 95)
		dc_icl = DC_DCP_CURRENT;
	if (soc == 100)
		dc_icl = DC_LOW_CURRENT;

	if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL) {
		dev_info(di->dev, "qc2+f1_tx, full mode set vout to 8V,icl to 250mA\n");
		dc_icl = SCREEN_OFF_FUL_CURRENT;
	}

	if (vout != di->last_qc2_vout) {
		dev_info(di->dev, "qc2+f1_tx, set new vout: %d, last_vout: %d\n",
				vout, di->last_qc2_vout);
		di->last_qc2_vout = vout;
		idtp9220_set_vout(di, vout);
		msleep(200);
		if (vout == ADAPTER_BPP_QC_VOL)
			idtp922x_enable_ext5v(di);
	}

	if (dc_icl != di->last_qc2_icl) {
		dev_info(di->dev, "qc2+f1_tx, set new icl: %d, last_icl: %d\n",
				dc_icl, di->last_qc2_icl);
		di->last_qc2_icl = dc_icl;
		idtp922x_set_pmi_icl(di, dc_icl);
	}
}

static void idtp9220_qc3_epp_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				qc3_epp_work.work);
	int soc = 0, batt_sts = 0;
	int adapter_vol = ADAPTER_EPP_QC3_VOL;
	int dc_icl = DC_EPP_QC3_CURRENT;
	union power_supply_propval val = {0, };

	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;
	}

	if (soc >= 97)
		dc_icl = DC_BPP_CURRENT;

	if (soc == 100)
		dc_icl = DC_SDP_CURRENT;

	if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
		dc_icl = SCREEN_OFF_FUL_CURRENT;

	if (adapter_vol != di->last_qc3_vout) {
		dev_info(di->dev, "qc3 epp, set new vout: %d, last_vout: %d\n",
				adapter_vol, di->last_qc3_vout);
		di->last_qc3_vout = adapter_vol;
		idtp9220_set_vout(di, adapter_vol);
		msleep(200);
		if (adapter_vol == ADAPTER_EPP_QC3_VOL)
			idtp922x_enable_ext5v(di);
	}

	if (dc_icl != di->last_qc3_icl) {
		dev_info(di->dev, "qc3_epp, set new icl: %d, last_icl: %d\n",
				dc_icl, di->last_qc3_icl);
		di->last_qc3_icl = dc_icl;
		idtp922x_set_pmi_icl(di, dc_icl);
		msleep(100);
	}
}

static void idtp9220_vout_regulator_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				vout_regulator_work.work);

	union power_supply_propval val = {0, };
	int ret;
	int vinc = 0;
	int soc = 0;
	int icl_set = DC_MI_CURRENT_20W;

	if (di->tx_charger_type == ADAPTER_XIAOMI_PD_40W)
		icl_set = DC_MI_CURRENT_30W;

	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;
	}

	if (di)
		idtp9220_get_vout(di);
	else
		return;

	if (di->epp)
		vinc = (di->vout > EPP_VOL_THRESHOLD) ? 1 : 0;
	else
		vinc = (di->vout > BPP_VOL_THRESHOLD) ? 1 : 0;

	if (vinc && !di->epp) {
		di->bpp_vout_rise = 1;
		goto out;
	} else if (!vinc && !di->epp) {
		di->bpp_vout_rise = 0;
		goto out;
	}

	if (di->epp && vinc && soc < 95) {
		dev_info(di->dev, "%s: open charge pump by wireless\n", __func__);
		ret = idtp9220_get_property_names(di);
		if (di->wireless_psy) {
			val.intval = 1;
			power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			msleep(300);
			idtp922x_set_pmi_icl(di, icl_set);
		} else
			dev_err(di->dev, "[idt] no wireless psy\n");
	} else {
		dev_info(di->dev, "%s: close charge pump by wireless\n", __func__);
		if (di->wireless_psy) {
			val.intval = 0;
			power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
		}
	}

out:
	dev_info(di->dev, "%s: epp=%d vout= %d\n", __func__, di->epp, di->vout);
	return;
}

#define EPP_PLUS_VOL	15
#define BPP_PLUS_VOL	9
#define EPP_BASE_VOL	12
#define FULL_VOL	7

static int fod_over_write(struct idtp9220_device_info *di, int mode)
{
	int ret = 0;

	switch (mode) {
	case EPP_PLUS_VOL:
		di->bus.write(di, 0x68, 0xA0);
		di->bus.write(di, 0x69, 0x61);
		di->bus.write(di, 0x6A, 0xA0);
		di->bus.write(di, 0x6B, 0x61);
		di->bus.write(di, 0x6C, 0xA0);
		di->bus.write(di, 0x6D, 0x61);
		di->bus.write(di, 0x6E, 0xA0);
		di->bus.write(di, 0x6F, 0xF6);
		di->bus.write(di, 0x70, 0x8C);
		di->bus.write(di, 0x71, 0x50);
		di->bus.write(di, 0x72, 0x8A);
		di->bus.write(di, 0x73, 0x5F);
		/*
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		*/
		ret = 1;
		break;
	case EPP_BASE_VOL:
		di->bus.write(di, 0x68, 0x98);
		di->bus.write(di, 0x69, 0x46);
		di->bus.write(di, 0x6A, 0x98);
		di->bus.write(di, 0x6B, 0x46);
		di->bus.write(di, 0x6C, 0x92);
		di->bus.write(di, 0x6D, 0x38);
		di->bus.write(di, 0x6E, 0x92);
		di->bus.write(di, 0x6F, 0x38);
		di->bus.write(di, 0x70, 0x8C);
		di->bus.write(di, 0x71, 0x5A);
		di->bus.write(di, 0x72, 0x8C);
		di->bus.write(di, 0x73, 0x5A);
		/*
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		*/
		ret = 1;
		break;
	case BPP_PLUS_VOL:
		di->bus.write(di, 0x68, 0x9A);
		di->bus.write(di, 0x69, 0x50);
		di->bus.write(di, 0x6A, 0x9A);
		di->bus.write(di, 0x6B, 0x50);
		di->bus.write(di, 0x6C, 0x9C);
		di->bus.write(di, 0x6D, 0x50);
		di->bus.write(di, 0x6E, 0x90);
		di->bus.write(di, 0x6F, 0x5A);
		di->bus.write(di, 0x70, 0x8C);
		di->bus.write(di, 0x71, 0x5A);
		di->bus.write(di, 0x72, 0x9A);
		di->bus.write(di, 0x73, 0x1E);
		/*
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		*/
		ret = 1;
		break;
	case FULL_VOL:
		di->bus.write(di, 0x68, 0xAC);
		di->bus.write(di, 0x69, 0x28);
		di->bus.write(di, 0x6A, 0x9C);
		di->bus.write(di, 0x6B, 0x24);
		di->bus.write(di, 0x6C, 0x9C);
		di->bus.write(di, 0x6D, 0x24);
		di->bus.write(di, 0x6E, 0x9C);
		di->bus.write(di, 0x6F, 0x1F);
		di->bus.write(di, 0x70, 0x9C);
		di->bus.write(di, 0x71, 0x1A);
		di->bus.write(di, 0x72, 0x9A);
		di->bus.write(di, 0x73, 0x24);
		/*
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		di->bus.write(di, 0xFF, 0x7F);
		*/
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static int fod_param_verity(struct idtp9220_device_info *di, int mode)
{

	u8 data_l = 0, data_h = 0;
	int ret = 0;

	di->bus.read(di, 0x72, &data_l);
	di->bus.read(di, 0x73, &data_h);

	switch (mode) {
	case EPP_PLUS_VOL:
			if (data_l == 0x8A && data_h == 0x55)
				ret = 1;
			else
				ret = 0;
			break;
	case EPP_BASE_VOL:
			if (data_l == 0x86 && data_h == 0x60)
				ret = 1;
			else
				ret = 0;
			break;
	case BPP_PLUS_VOL:
			if (data_l == 0x96 && data_h == 0xDD)
				ret = 1;
			else
				ret = 0;
			break;
	case FULL_VOL:
			if (data_l == 0x94 && data_h == 0x14)
				ret = 1;
			else
				ret = 0;
			break;
	default:
			ret = 0;
			break;
	}

	return ret;
}
static void reverse_chg_sent_state_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				reverse_sent_state_work.work);
	union power_supply_propval val = {0, };

	if (di->wireless_psy) {
		val.intval = di->is_reverse_chg;
		power_supply_set_property(di->wireless_psy,
				POWER_SUPPLY_PROP_REVERSE_CHG_STATE, &val);
		power_supply_changed(di->wireless_psy);
		dev_info(di->dev, "uevent: %d for reverse charging state\n", di->is_reverse_chg);
	} else
		dev_err(di->dev, "get wls property error\n");
}

static void reverse_chg_state_set_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				reverse_chg_state_work.work);

	dev_info(di->dev, "no rx found and disable reverse charging\n");

	mutex_lock(&di->reverse_op_lock);
	idtp9220_set_reverse_enable(di, false);
	di->is_reverse_mode = 0;
	di->is_reverse_chg = 1;
	mutex_lock(&di->reverse_op_lock);
	schedule_delayed_work(&di->reverse_sent_state_work, 0);

	return;
}

static void reverse_dping_state_set_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				reverse_dping_state_work.work);

	dev_info(di->dev, "tx mode fault and disable reverse charging\n");

	mutex_lock(&di->reverse_op_lock);
	idtp9220_set_reverse_enable(di, false);
	di->is_reverse_mode = 0;
	di->is_reverse_chg = 2;
	mutex_lock(&di->reverse_op_lock);
	schedule_delayed_work(&di->reverse_sent_state_work, 0);

	return;
}
static void reverse_ept_type_get_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				reverse_ept_type_work.work);
	int rc;
	u8 buf[2] = {0};
	u16 ept_val = 0;

	rc = di->bus.read_buf(di, REG_EPT_TYPE, buf, 2);
	if (rc < 0)
		dev_err(di->dev, "read tx ept type error: %d\n", rc);
	else {
		ept_val = buf[0] | (buf[1] << 8);
		dev_info(di->dev, "tx ept type: 0x%04x\n", ept_val);
		if (ept_val) {
			if ((ept_val & EPT_FOD) ||
				(ept_val & EPT_CMD) ||
				(ept_val & EPT_OCP) ||
				(ept_val & EPT_OVP) ||
				(ept_val & EPT_LVP) ||
				(ept_val & EPT_OTP) ||
				(ept_val & EPT_POCP)) {
				dev_info(di->dev, "TX mode in ept and disable reverse charging\n");
				idtp9220_set_reverse_enable(di, false);
				di->is_reverse_mode = 0;
				di->is_reverse_chg = 2;
				schedule_delayed_work(&di->reverse_sent_state_work, 0);
			} else if (ept_val & EPT_CEP_TIMEOUT) {
					dev_info(di->dev, "recheck ping state\n");
					schedule_delayed_work(&di->reverse_dping_state_work, 10 * HZ);
			}
		}
	}

	return;
}

static void idtp9220_load_fod_param_work(struct work_struct *work)
{
	struct idtp9220_device_info *di = container_of(work, struct idtp9220_device_info,
			load_fod_param_work.work);
	int fod_param = 0;
	int ret = 0, val = 0;

	if (!di->vout)
		idtp9220_get_vout(di);

	if (di->vout > EPP_VOL_LIM_THRESHOLD)
		fod_param = 15;
	else if (di->vout > BPP_VOL_LIM_THRESHOLD
			&& di->vout < EPP_VOL_LIM_THRESHOLD)
		fod_param = 12;
	else if (di->vout > BPP_VOL_THRESHOLD
			&& di->vout < BPP_VOL_LIM_THRESHOLD)
		fod_param = 9;
	else if (di->vout > 0 && di->vout < BPP_VOL_THRESHOLD)
		fod_param = 7;

	val = fod_over_write(di, fod_param);
	msleep(50);
	if (val) {
		ret = fod_param_verity(di, fod_param);
		if (!ret)
			fod_over_write(di, fod_param);
	} else
		fod_over_write(di, fod_param);

	dev_info(di->dev, "[idt] %d mV fod load %s\n", di->vout,
			(ret ? "success" : "fail"));
	return;
}
static void idtp9220_set_charging_param(struct idtp9220_device_info *di)
{
	int soc = 0, health = 0, batt_sts = 0;
	int adapter_vol = 0, icl_curr = 0, dc_level = 0;
	int cur_now = 0, vol_now = 0, vin_inc = 0;
	int vout = 0;
	bool vout_change = false;
	union power_supply_propval val = {0, };
	union power_supply_propval wk_val = {0, };

	switch (di->tx_charger_type) {
	case ADAPTER_QC2:
		adapter_vol = ADAPTER_BPP_QC2_VOL;
		icl_curr = DC_QC2_CURRENT;
		break;
	case ADAPTER_QC3:
		if (!di->epp) {
			adapter_vol = ADAPTER_BPP_QC_VOL;
			icl_curr = DC_QC3_CURRENT;
		} else {
			di->is_epp_qc3 = 1;
			adapter_vol = ADAPTER_EPP_QC3_VOL;
			icl_curr = DC_EPP_QC3_CURRENT;
		}
		break;
	case ADAPTER_PD:
		if (di->epp) {
			adapter_vol = ADAPTER_EPP_QC3_VOL;
			icl_curr = DC_EPP_QC3_CURRENT;
		} else {
			adapter_vol = ADAPTER_DEFAULT_VOL;
			icl_curr = DC_PD_CURRENT;
		}
		break;
	case ADAPTER_AUTH_FAILED:
		if (di->epp) {
			adapter_vol = ADAPTER_EPP_QC3_VOL;
			if (di->power_max < 10000)
				icl_curr = ((di->power_max - 1000) / 11) * 1000;
			else {
				if (di->op_mode == LN8282_OPMODE_SWITCHING)
					icl_curr = EPP_DEFAULT_SWITCH_CURRENT;
				else
					icl_curr = EPP_DEFAULT_BYPASS_CURRENT;
			}
		} else {
			adapter_vol = ADAPTER_DEFAULT_VOL;
			icl_curr = BPP_DEFAULT_CURRENT;
		}
		break;
	case ADAPTER_DCP:
	case ADAPTER_CDP:
		adapter_vol = ADAPTER_DEFAULT_VOL;
		if (di->is_compatible_hwid)
			icl_curr = DC_BPP_AUTH_FAIL_CURRENT;
		else
			icl_curr = DC_BPP_AUTH_FAIL_CURRENT;
		break;
	case ADAPTER_SDP:
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = 500000;
		break;
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
	case ADAPTER_XIAOMI_PD_40W:
		if (di->epp) {
			adapter_vol = ADAPTER_EPP_MI_VOL;
			icl_curr = DC_MI_STEP1_CURRENT;
		} else {
			adapter_vol = ADAPTER_BPP_QC_VOL;
			icl_curr = DC_QC3_CURRENT;
		}
		break;
	default:
		icl_curr = 0;
		break;
	}

	power_supply_get_property(di->batt_psy,
			POWER_SUPPLY_PROP_STATUS, &val);
	batt_sts = val.intval;

	power_supply_get_property(di->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &val);
	soc = val.intval;

	power_supply_get_property(di->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	vol_now = val.intval;

	power_supply_get_property(di->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	cur_now = val.intval;

	power_supply_get_property(di->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &val);
	health = val.intval;

	idtp9220_get_iout(di);

	if ((batt_sts == POWER_SUPPLY_STATUS_DISCHARGING)
			&& di->dcin_present
			&& di->power_good_flag) {
		dev_info(di->dev, "discharge when dc and pwr present, reset chip\n");
		schedule_delayed_work(&di->fast_operate_work, msecs_to_jiffies(0));
		return;
	}
	dev_info(di->dev, "[idtp] soc:%d,vol_now:%d,cur_now:%d,health:%d, bat_status:%d\n",
			soc, vol_now, cur_now, health, batt_sts);

	/* adapter:qc2/qc3; tx:e5/d5x_10W;
	 * vout/psns is setted in delayed work
	 */
	if (adapter_vol == ADAPTER_BPP_QC_VOL && di->is_compatible_hwid) {
		schedule_delayed_work(&di->bpp_e5_tx_work, msecs_to_jiffies(0));
		goto out;
	}

	/* adapter:qc2; tx:F1_27W;
	 * vout/psns is setted in delayed work
	 */
	if (di->tx_charger_type == ADAPTER_QC2 && di->is_f1_tx) {
		schedule_delayed_work(&di->qc2_f1_tx_work, msecs_to_jiffies(0));
		goto out;
	}

	/* adapter:qc3_epp; tx:F1_27W;
	 * vout/psns is setted in delayed work
	 */
	if (di->is_epp_qc3) {
		schedule_delayed_work(&di->qc3_epp_work, msecs_to_jiffies(0));
		goto out;
	}

	if ((adapter_vol == ADAPTER_EPP_QC3_VOL) && (di->epp)) {
		dev_info(di->dev, "standard epp logic\n");

		if (adapter_vol > 0 && adapter_vol != di->last_vin) {
			idtp922x_set_adap_vol(di, adapter_vol);
			di->last_vin = adapter_vol;
			msleep(200);
		}

		if ((icl_curr > 0) && (icl_curr != di->last_icl)) {
			idtp922x_set_pmi_icl(di, icl_curr);
			di->last_icl = icl_curr;
			msleep(100);
		}

		goto out;
	}

	if ((adapter_vol == ADAPTER_DEFAULT_VOL) && (!di->epp)) {
		dev_info(di->dev, "standard bpp logic\n");

		if (adapter_vol > 0 && adapter_vol != di->last_vin) {
			idtp922x_set_adap_vol(di, adapter_vol);
			di->last_vin = adapter_vol;
			msleep(200);
		}

		if ((icl_curr > 0) && (icl_curr != di->last_icl)) {
			idtp922x_set_pmi_icl(di, icl_curr);
			di->last_icl = icl_curr;
			msleep(100);
		}

		goto out;
	}

	if (adapter_vol == ADAPTER_EPP_MI_VOL) {
		if (di->batt_psy) {
			power_supply_get_property(di->batt_psy,
					POWER_SUPPLY_PROP_DC_THERMAL_LEVELS, &val);
			dc_level = val.intval;
			if ((dc_level >= 9) || (soc > LIMIT_SOC)) {
				dev_info(di->dev, "set vin 12V for dc_level:%d, soc:%d\n", dc_level, soc);
				adapter_vol = EPP_VOL_THRESHOLD;
				di->is_vin_limit = 1;
			} else
				di->is_vin_limit = 0;
		}

		/* function start
		 * set adapter vol by iout value, threshold is 500mA
		 * lower than threshold will reduce to 12V, more than
		 * threshold will lift to 15V:
		 * 15V-->12V check 6 times
		 * 12V-->15v check 3 times
		 */
		if (di->iout < LIMIT_EPP_IOUT && di->epp_exchange == EXCHANGE_15V) {
			di->count_10v++;
			di->count_15v = 0;
		} else if (di->iout > LIMIT_EPP_IOUT && di->epp_exchange == EXCHANGE_10V) {
			di->count_15v++;
			di->count_10v = 0;
		} else {
			di->count_10v = 0;
			di->count_15v = 0;
		}
		if (di->count_10v > ICL_EXCHANGE_COUNT ||
				(di->epp_exchange == EXCHANGE_10V && di->count_15v <= ICL_EXCHANGE_COUNT - 3)) {
			dev_info(di->dev, "iout less than 500mA ,set vin to 11V\n");

			di->epp_exchange = EXCHANGE_10V;
		} else if (di->count_15v > (ICL_EXCHANGE_COUNT - 3))
			di->epp_exchange = EXCHANGE_15V;
		/* function end */

		switch (di->status) {
		case NORMAL_MODE:
/*
			if (soc >= 97)
				icl_curr = min(DC_BPP_CURRENT, icl_curr);
*/
			if (soc >= FULL_SOC) {
				di->status = TAPER_MODE;


			}
			break;
		case TAPER_MODE:



			if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
				di->status = FULL_MODE;
			else if (soc < FULL_SOC - 1)
				di->status = NORMAL_MODE;
			break;
		case FULL_MODE:
			dev_info (di->dev, "charge full set Vin 10V\n");
			adapter_vol = EPP_VOL_THRESHOLD;
			icl_curr = SCREEN_OFF_FUL_CURRENT;

			if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
				di->status = RECHG_MODE;
				icl_curr = DC_LOW_CURRENT;
			}
			break;
		case RECHG_MODE:
			dev_info (di->dev, "recharge mode set icl to 360mA\n");
			if (batt_sts == POWER_SUPPLY_STATUS_FULL)
				di->status = FULL_MODE;

			adapter_vol = EPP_VOL_THRESHOLD;
			icl_curr = DC_LOW_CURRENT;

			if (di->wireless_psy) {
				wk_val.intval = 1;
				power_supply_set_property(di->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
			}
			break;
		default:
			break;
		}

		switch(health) {
		case POWER_SUPPLY_HEALTH_GOOD:
			break;
		case POWER_SUPPLY_HEALTH_COOL:
			break;
		case POWER_SUPPLY_HEALTH_WARM:
			adapter_vol = min(EPP_VOL_THRESHOLD, adapter_vol);
			icl_curr = min(DC_SDP_CURRENT, icl_curr);
			break;
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
			adapter_vol = min(EPP_VOL_THRESHOLD, adapter_vol);
			icl_curr = min(SCREEN_OFF_FUL_CURRENT, icl_curr);
			break;
		case POWER_SUPPLY_HEALTH_COLD:
		case POWER_SUPPLY_HEALTH_HOT:
			adapter_vol = min(EPP_VOL_THRESHOLD, adapter_vol);
			icl_curr = SCREEN_OFF_FUL_CURRENT;
			break;
		default:
			break;
		}
	}

	if (!di->enable_ext5v) {
		di->enable_ext5v = true;
		idtp922x_enable_ext5v(di);
	}

	if (di->op_mode != LN8282_OPMODE_SWITCHING) {
		dev_info (di->dev, "dont rise voltage because ln8282 isn't switch mode\n");
		goto out;
	}

	if (adapter_vol > 0 && adapter_vol != di->last_vin) {
		if ((adapter_vol == ADAPTER_EPP_MI_VOL)
			&& !di->first_rise_flag) {
			di->disable_1390 = false;
			di->first_rise_flag = true;
			idtp922x_set_adap_vol(di, adapter_vol);
			di->vswitch_ok = false;
			vin_inc = 1;
			msleep(110);
		} else if ((adapter_vol == ADAPTER_EPP_MI_VOL)
		 && di->first_rise_flag) {
			di->disable_1390 = false;
			idtp9220_set_vout(di, adapter_vol);
			msleep(110);
			schedule_delayed_work(&di->load_fod_param_work,
					msecs_to_jiffies(500));
			schedule_delayed_work(&di->vout_regulator_work,
					msecs_to_jiffies(400));
		} else if (adapter_vol == EPP_VOL_THRESHOLD) {
			di->disable_1390 = true;
			vout = idtp9220_get_vout(di);
			while (vout > EPP_VOL_THRESHOLD) {
				vout = vout - 1000;
				idtp9220_set_vout(di, vout);
				msleep(200);
			}
			idtp9220_set_vout(di, EPP_VOL_THRESHOLD);
			if (di->wireless_psy) {
				val.intval = 0;
				power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			}
			msleep(110);
			schedule_delayed_work(&di->load_fod_param_work,
					msecs_to_jiffies(500));
		}
		vout_change = true;
		di->last_vin = adapter_vol;
	}

	if ((icl_curr > 0 && icl_curr != di->last_icl)
		|| vout_change) {
		di->last_icl = icl_curr;
		idtp922x_set_pmi_icl(di, icl_curr);
		msleep(100);
	}

	dev_info(di->dev, "di->status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d\n",
			di->status, adapter_vol, icl_curr, di->last_vin, di->last_icl);

	if (!di->vswitch_ok && vin_inc) {
		di->adapter_voltage = adapter_vol;
		schedule_delayed_work(&di->cmd_check_work,
				msecs_to_jiffies(8000));
	}
out:
	return;
}

static void idtp9220_wpc_det_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				wpc_det_work.work);
	union power_supply_propval val = {0, };
	int ret = 0;

	ret = idtp9220_get_property_names(di);
	if (ret < 0) {
		dev_err(di->dev, "get property error: %d\n", ret);
		return;
	}

	if (gpio_is_valid(di->dt_props.wpc_det_gpio)) {
		ret = gpio_get_value(di->dt_props.wpc_det_gpio);
		if (ret) {
			dev_info(di->dev, "power_good high, wireless attached\n");
			/* initial tx work after 100ms */
			schedule_delayed_work(&di->initial_tx_work, msecs_to_jiffies(100));
			di->power_good_flag = 1;
			val.intval = 1;
		}
		else {
			dev_info(di->dev, "power_good low, wireless detached\n");
			cancel_delayed_work(&di->dc_check_work);
			di->power_good_flag = 0;
			val.intval = 0;
			di->ss = 2;
			di->ln_psy = power_supply_get_by_name("lionsemi");
			if (di->ln_psy)
				power_supply_set_property(di->ln_psy,
					POWER_SUPPLY_PROP_RESET_DIV_2_MODE, &val);
		}
		power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN, &val);
	}

	return;
}


static void idtp9220_irq_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				irq_work.work);
	u8 int_buf[4] = {0};
	u32 int_val = 0;
	int rc = 0;
	u8 recive_data[5] = {0};
	static int retry;
	static int retry_id;
	static int retry_count;
	int tx_vin = 0;
	int irq_level;

	if (gpio_is_valid(di->dt_props.irq_gpio))
		irq_level = gpio_get_value(di->dt_props.irq_gpio);
	else {
		dev_err(di->dev, "%s: irq gpio not provided\n", __func__);
		irq_level = -1;
		return;
	}
	if (irq_level) {
		dev_info(di->dev, "irq is high level, ignore%d\n", irq_level);
		return;
	}

	rc = di->bus.read_buf(di, REG_SYS_INT, int_buf, 4);
	if (rc < 0) {
		dev_err(di->dev, "[idt]read int state error: %d\n", rc);
		goto out;
	}

	if (di->is_reverse_mode || di->is_boost_mode) {
		reverse_clrInt(di, int_buf, 4);
		int_val = int_buf[0] | (int_buf[1] << 8) | (int_buf[2] << 16) | (int_buf[3] << 24);
		dev_info(di->dev, "[idt] TRX int: 0x%08x\n", int_val);
		if (int_val & INT_GET_DPING) {
			dev_info(di->dev, "[idt] TRX get dping and disable reverse charging \n");
			idtp9220_set_reverse_enable(di, false);
			di->is_reverse_mode = 0;
			di->is_reverse_chg = 3;
			schedule_delayed_work(&di->reverse_sent_state_work, 0);
			goto out;
		}
		if (int_val & INT_EPT_TYPE) {
			schedule_delayed_work(&di->reverse_ept_type_work, 0);
			goto out;
		}
		if (int_val & INT_START_DPING) {
			schedule_delayed_work(&di->reverse_chg_state_work, 80 * HZ);
			cancel_delayed_work(&di->reverse_dping_state_work);
			goto out;
		}
		if (int_val & INT_GET_CFG) {
			cancel_delayed_work(&di->reverse_chg_state_work);
			schedule_delayed_work(&di->reverse_sent_state_work, 0);
			goto out;
		}
		if (int_val & INT_INIT_TX) {
			dev_info(di->dev, "[idt] TRX reset done\n");
			goto out;
		}
		goto out;
	}

	/* clear int and enable irq immediately when read int register*/
	idtp922x_clrInt(di, int_buf, 4);

	int_val = int_buf[0] | (int_buf[1] << 8) | (int_buf[2] << 16) | (int_buf[3] << 24);
	dev_info(di->dev, "[idt] int: 0x%08x\n", int_val);

	msleep(5);

	if (need_irq_cleared(di))
	{
		u8 clr_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
		idtp922x_clrInt(di, clr_buf, 4);
		msleep(5);
	}


	if(int_val & INT_VOUT_ON) {
		di->epp = idtp9220_get_power_profile(di);
		if (di->epp) {
			di->power_max = idtp9220_get_power_max(di);
			schedule_delayed_work(&di->epp_connect_load_work,
					msecs_to_jiffies(200));
		} else
			schedule_delayed_work(&di->bpp_connect_load_work,
					msecs_to_jiffies(200));
		if (int_val & INT_IDAUTH_SUCESS)
			idtp9220_send_device_auth(di);
		goto out;
	}

	if (int_val & INT_VSWITCH_SUCESS) {
		di->vswitch_ok = true;
		schedule_delayed_work(&di->load_fod_param_work,
				msecs_to_jiffies(500));
		schedule_delayed_work(&di->vout_regulator_work,
				msecs_to_jiffies(50));
		cancel_delayed_work(&di->cmd_check_work);
		goto out;
	}

	if (int_val & INT_IDAUTH_SUCESS) {
		idtp9220_send_device_auth(di);

		goto out;
	}


	if (int_val & INT_AUTH_SUCESS) {
		idtp922x_request_uuid(di, di->epp);
		goto out;
	}
	/*
	   idtp9220_get_signal_strength(di);
	   di->tx_charger_type = ADAPTER_QC3;
	   schedule_delayed_work(&di->chg_monitor_work,
	   msecs_to_jiffies(0));
	   goto out;
	 */

	if ((int_val & INT_IDAUTH_FAIL) || (int_val & INT_AUTH_FAIL) || int_val == 0) {
		if (((int_val & INT_AUTH_FAIL) || (int_val == 0)) && (retry < 5)){
			idtp9220_send_device_auth(di);
			retry++;
			dev_info(di->dev, "[idtp] dev auth failed retry %d\n", retry);
			goto out;
		} else if ((int_val & INT_IDAUTH_FAIL) && retry_id < 5) {
			idtp9220_retry_id_auth(di);
			retry_id++;
			dev_info(di->dev, "[idtp] id auth failed retry %d\n", retry);
			goto out;
		} else {
			retry = 0;
			retry_id = 0;
		}
		di->tx_charger_type = ADAPTER_AUTH_FAILED;
		dev_info(di->dev, "[idtp] auth failed tx charger type set %d\n", di->tx_charger_type);

		schedule_delayed_work(&di->rx_vout_work,
				msecs_to_jiffies(0));
		schedule_delayed_work(&di->chg_monitor_work,
				msecs_to_jiffies(0));
		goto out;
	} else
		retry = 0;
	/*
	   if (int_val & INT_IDAUTH_FAIL) {
	   idtp922x_request_adapter(di);
	   goto out;
	   }
	 */

	if (int_val & INT_SEND_TIMEOUT) {
		if (retry_count < 3) {
			dev_info(di->dev, "timeout retry %d\n", retry_count);
			idtp922x_retry_cmd(di);
			retry_count++;
			goto out;
		} else {
			dev_err(di->dev, "%s: retry failed\n", __func__);
			di->tx_charger_type = ADAPTER_AUTH_FAILED;
			schedule_delayed_work(&di->rx_vout_work,
					msecs_to_jiffies(0));
			schedule_delayed_work(&di->chg_monitor_work,
					msecs_to_jiffies(0));
			retry_count = 0;
			goto out;
		}
	} else {
		retry_count = 0;

	}

	if (int_val & INT_TX_DATA_RECV) {
		idtp922x_receivePkt(di, recive_data);
		dev_info(di->dev, "[idt] cmd: %x\n", recive_data[0]);

		switch (recive_data[0]) {
		case BC_TX_HWID:
				dev_info(di->dev, "[idt] TX chip_vendor:0x%x, module:0x%x, hw:0x%x and power:0x%x\n",
						recive_data[4],recive_data[2],recive_data[3],recive_data[1]);
				if (recive_data[4] == 0x01 &&
						recive_data[2] == 0x2 &&
						recive_data[3] == 0x8 &&
						recive_data[1] == 0x6) {
					di->is_car_tx = 1;
				}
				idtp922x_request_adapter(di);
				break;
		case BC_TX_COMPATIBLE_HWID:
				dev_info(di->dev, "[idt] TX hwid: 0x%x 0x%x\n",
						recive_data[1],recive_data[2]);
				if (recive_data[1] == 0x12 && recive_data[2])
					di->is_compatible_hwid = 1;
				if (recive_data[1] == 0x16 && recive_data[2] == 0x11)
					di->is_f1_tx = 1;
				idtp922x_request_adapter(di);
				break;
		case BC_ADAPTER_TYPE:
				dev_info(di->dev, "[idt]adapter type: %d\n", recive_data[1]);

				if (di->is_car_tx && (recive_data[1] == ADAPTER_XIAOMI_QC3))

					di->tx_charger_type = ADAPTER_ZIMI_CAR_POWER;
				else
					di->tx_charger_type = recive_data[1];
				/*
				   if(!di->epp && (di->tx_charger_type == ADAPTER_QC3 ||
				   di->tx_charger_type == ADAPTER_QC2)) {
				   idtp922x_set_adap_vol(di, ADAPTER_BPP_VOL);
				   dev_info(di->dev, "[idt]bpp mode set 5v first\n");
				   }
				 */
				schedule_delayed_work(&di->rx_vout_work,
						msecs_to_jiffies(100));
				schedule_delayed_work(&di->chg_monitor_work,
						msecs_to_jiffies(1000));
				if (di->wireless_psy)
					power_supply_changed(di->wireless_psy);

				if (recive_data[1] == ADAPTER_XIAOMI_PD_40W) {
					if (di->usb_psy)
						power_supply_changed(di->usb_psy);
				}
				break;
		case BC_READ_Vin:
				tx_vin = recive_data[1] | (recive_data[2] << 8);
				if (!di->power_off_mode) {
					if (di->epp)
						idtp9220_set_vout(di, di->adapter_voltage);
					else
						idtp9220_set_vout(di, ADAPTER_BPP_QC_VOL);
				}
				dev_info(di->dev, "[idt] tx vin : %d\n", tx_vin);
				break;
		case BC_RX_ID_AUTH:
			dev_info(di->dev, "[idt] ID Auth retry success: 0x%x, 0x%x\n",
					recive_data[1], recive_data[2]);
			idtp9220_send_device_auth(di);
			break;

		default:
				dev_info(di->dev, "[idt] unsupport cmd: %x\n", recive_data[0]);
				break;
		}
	}

out:
	return;
}

static irqreturn_t idtp9220_wpc_det_irq_handler(int irq, void *dev_id)
{
	struct idtp9220_device_info *di = dev_id;

	schedule_delayed_work(&di->wpc_det_work, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static irqreturn_t idtp9220_irq_handler(int irq, void *dev_id)
{

	struct idtp9220_device_info *di = dev_id;

	schedule_delayed_work(&di->irq_work, msecs_to_jiffies(30));

	return IRQ_HANDLED;
}

static int idtp9220_irq_request(struct idtp9220_device_info *di)
{
	int ret = 0;

	if (!di->irq) {
		dev_err(di->dev, "%s: irq is wrong\n", __func__);
		return -EINVAL;
	}

	ret = request_irq(di->irq, idtp9220_irq_handler,
			IRQF_TRIGGER_FALLING, di->name, di);
	if (ret) {
		dev_err(di->dev, "%s: request_irq failed\n", __func__);
		return ret;
	}

	ret = enable_irq_wake(di->irq);
	if (ret) {
		dev_err(di->dev, "%s: enable_irq_wake failed\n", __func__);
		return ret;
	}

	if (!di->wpc_irq) {
		dev_err(di->dev, "%s: wpc irq is wrong\n", __func__);
		return -EINVAL;
	}

	ret = request_irq(di->wpc_irq, idtp9220_wpc_det_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "wpc_det", di);
	if (ret) {
		dev_err(di->dev, "%s: wpc request_irq failed\n", __func__);
		return ret;
	}

	ret = enable_irq_wake(di->wpc_irq);
	if (ret) {
		dev_err(di->dev, "%s: enable_irq_wake failed\n", __func__);
		return ret;
	}

	return 0;
}

static struct regmap_config i2c_idtp9220_regmap_config = {
	.reg_bits  = 16,
	.val_bits  = 8,
	.max_register  = 0xFFFF,
};

/*
   static int idtp9220_get_version(struct idtp9220_device_info *di)
   {
   int id_val = 0;
   u8 chip_id[2] = {0};

   di->bus.read_buf(di, REG_CHIP_ID_L, chip_id, 2);
   id_val = (chip_id[1] << 8) | chip_id[0];

   return id_val;
   }
 */

static enum power_supply_property idtp9220_props[] = {
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_WIRELESS_VERSION,
	POWER_SUPPLY_PROP_SIGNAL_STRENGTH,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_TX_ADAPTER,
	POWER_SUPPLY_PROP_REVERSE_CHG_MODE,
};

static int idtp9220_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct idtp9220_device_info *di = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		val->intval = gpio_get_value(di->dt_props.enable_gpio);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->dcin_present;
		break;
	case POWER_SUPPLY_PROP_WIRELESS_VERSION:
		val->intval = di->epp;
		break;
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		val->intval = di->ss;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		if (!di->power_good_flag) {
			val->intval = 0;
			break;
		}
		val->intval = idtp9220_get_vout(di);
		if (di->op_mode == LN8282_OPMODE_SWITCHING)
			val->intval = val->intval/2;
		break;
	case POWER_SUPPLY_PROP_TX_ADAPTER:
		val->intval = di->tx_charger_type;
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		val->intval = gpio_get_value(di->dt_props.reverse_gpio);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int idtp9220_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct idtp9220_device_info *di = power_supply_get_drvdata(psy);
	int rc = 0;

	int data;

	switch (psp) {
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		rc = idtp9220_set_enable_mode(di, val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		rc = idtp9220_set_present(di, val->intval);
		break;
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
		di->ss = val->intval;
		power_supply_changed(di->idtp_psy);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		data = val->intval/1000;
		if (di->op_mode == LN8282_OPMODE_SWITCHING)
			data *= 2;
		else if (di->epp)
			data = EPP_VOL_THRESHOLD;
		if (!di->disable_1390)
			idtp9220_set_vout(di, data);
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		di->is_reverse_chg = 0;
		schedule_delayed_work(&di->reverse_sent_state_work, 0);
		idtp9220_set_reverse_enable(di, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return rc;
}

static int idtp9220_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_PIN_ENABLED:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_SIGNAL_STRENGTH:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		return 1;
	default:
		rc = 0;
		break;
	}

	return rc;
}

static const struct power_supply_desc idtp_psy_desc = {
	.name = "idt",
	.type = POWER_SUPPLY_TYPE_WIRELESS,
	.properties = idtp9220_props,
	.num_properties = ARRAY_SIZE(idtp9220_props),
	.get_property = idtp9220_get_prop,
	.set_property = idtp9220_set_prop,
	.property_is_writeable = idtp9220_prop_is_writeable,
};

#ifdef IDTP9220_SRAM_UPDATE
static void idtp9220_sram_update_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				sram_update_work.work);
	u8 data;
	int size = sizeof(idt_firmware_sram);
	u8 buffer[size];
	int i = 0;


	di->bus.read(di, 0x4D, &data);
	dev_info(di->dev, "[idtp] %s: 0x4D data:%x, (data & BIT(4)):%lu\n", __func__, data, (data & BIT(4)));
	if (!(data & BIT(4)))
		return;

	di->bus.write_buf(di, 0x0600, idt_firmware_sram, size);
	di->bus.read_buf(di, 0x0600, buffer, size);

	while(size--)
	{
		if (idt_firmware_sram[i] == buffer[i])
		{
			printk("buffer[%d]:0x%x", i, buffer[i]);
		} else
		{
			printk("[idt] sram data is not right\n");
			return;
		}
		i++;
	}

	di->bus.write(di, 0x4F, 0x5A);

	di->bus.write(di, 0x4E, BIT(6));

	mdelay(1000);
	di->bus.read(di, 0x4D, &data);
	dev_info(di->dev, "[idtp] %s: 0x4D data:%x\n", __func__, data);
}
#endif

/*
#ifdef CONFIG_DRM
static int wireless_fb_notifier_cb(struct notifier_block *self,
unsigned long event, void *data)
{

struct drm_notify_data *evdata = data;
int *blank;

struct idtp9220_device_info *di =
container_of(self, struct idtp9220_device_info, wireless_fb_notif);

mutex_lock(&di->screen_lock);
if (evdata && evdata->data && di) {
if (event == DRM_EARLY_EVENT_BLANK) {
blank = evdata->data;
if (*blank == DRM_BLANK_UNBLANK) {
di->screen_on = true;
pr_info("%s: screen_on\n", __func__);
if(di->status & FULL_MODE)
idtp922x_set_pmi_icl(di, DC_FUL_CURRENT);
} else if (*blank == DRM_BLANK_POWERDOWN) {
di->screen_on = false;
pr_info("%s: screen_off\n", __func__);
if(di->status & FULL_MODE)
idtp922x_set_pmi_icl(di, SCREEN_OFF_FUL_CURRENT);
}
}
} else {
pr_err("%s: Couldn't get DRM event\n", __func__);
mutex_unlock(&di->screen_lock);
return 0;
}
mutex_unlock(&di->screen_lock);
return 0;
}
#endif
 */

static int idtp9220_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct idtp9220_device_info *di;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct power_supply_config idtp_cfg = {};

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "i2c check functionality failed!\n");
		return -EIO;
	}

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	di->name = IDT_DRIVER_NAME;
	di->dev = &client->dev;
	di->chip_enable = 1;
	di->ss = 2;
	di->fw_update = false;
	di->status = NORMAL_MODE;
	di->regmap = devm_regmap_init_i2c(client, &i2c_idtp9220_regmap_config);
	if (!di->regmap)
		return -ENODEV;
	di->bus.read = idtp9220_read;
	di->bus.write = idtp9220_write;
	di->bus.read_buf = idtp9220_read_buffer;
	di->bus.write_buf = idtp9220_write_buffer;
	di->bus.mask_write = idtp9220_masked_write;
	INIT_DELAYED_WORK(&di->irq_work, idtp9220_irq_work);
	INIT_DELAYED_WORK(&di->wpc_det_work, idtp9220_wpc_det_work);
	mutex_init(&di->read_lock);
	mutex_init(&di->write_lock);
	/*
#ifdef CONFIG_DRM
mutex_init(&di->screen_lock);
#endif
	 */
	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, di);
	g_di = di;

	ret = idtp9220_parse_dt(di);
	if (ret < 0) {
		dev_err(di->dev, "%s: parse dt error [%d]\n",
				__func__, ret);
		goto cleanup;
	}

	ret = idtp9220_gpio_init(di);
	if (ret < 0) {
		dev_err(di->dev, "%s: gpio init error [%d]\n",
				__func__, ret);
		goto cleanup;
	}

	ret = idtp9220_irq_request(di);
	if (ret < 0) {
		dev_err(di->dev, "%s: request irq error [%d]\n",
				__func__, ret);
		goto cleanup;
	}

	/*
	 *	this func write config to otp when init, due to the config will be
	 *	write to otp in factory, so delete it.
	 *


	 if (!program_fw(di, 0x0000, idt_firmware_otp, sizeof(idt_firmware_otp))) {
	 dev_err(&client->dev, "program fw failed.\n");
//goto cleanup;
}
	 */

	if (sysfs_create_group(&client->dev.kobj, &sysfs_group_attrs)) {
		dev_err(&client->dev, "create sysfs attrs failed!\n");
		ret = -EIO;
		goto cleanup;
	}
	idtp_cfg.drv_data = di;
	di->idtp_psy = power_supply_register(di->dev,
			&idtp_psy_desc,
			&idtp_cfg);

	INIT_DELAYED_WORK(&di->chg_monitor_work, idtp9220_monitor_work);
	INIT_DELAYED_WORK(&di->chg_detect_work, idtp9220_chg_detect_work);
	INIT_DELAYED_WORK(&di->bpp_connect_load_work, idtp9220_bpp_connect_load_work);
	INIT_DELAYED_WORK(&di->epp_connect_load_work, idtp9220_epp_connect_load_work);
	INIT_DELAYED_WORK(&di->cmd_check_work, idtp9220_cmd_check_work);
	INIT_DELAYED_WORK(&di->vout_regulator_work, idtp9220_vout_regulator_work);
	INIT_DELAYED_WORK(&di->load_fod_param_work, idtp9220_load_fod_param_work);
	INIT_DELAYED_WORK(&di->rx_vout_work, idtp9220_rx_vout_work);
	INIT_DELAYED_WORK(&di->dc_check_work, idtp9220_dc_check_work);
	INIT_DELAYED_WORK(&di->fast_operate_work, idtp9220_fast_operate_work);
	INIT_DELAYED_WORK(&di->initial_tx_work, idtp9220_initial_tx_work);
	INIT_DELAYED_WORK(&di->bpp_e5_tx_work, idtp9220_bpp_e5_tx_work);
	INIT_DELAYED_WORK(&di->qc2_f1_tx_work, idtp9220_qc2_f1_tx_work);
	INIT_DELAYED_WORK(&di->qc3_epp_work, idtp9220_qc3_epp_work);

	INIT_DELAYED_WORK(&di->reverse_ept_type_work, reverse_ept_type_get_work);
	INIT_DELAYED_WORK(&di->reverse_chg_state_work, reverse_chg_state_set_work);
	INIT_DELAYED_WORK(&di->reverse_dping_state_work, reverse_dping_state_set_work);
	INIT_DELAYED_WORK(&di->reverse_sent_state_work, reverse_chg_sent_state_work);

	mutex_init(&di->sysfs_op_lock);
	mutex_init(&di->reverse_op_lock);

	/*
#ifdef CONFIG_DRM
	if (&di->wireless_fb_notif) {
	di->wireless_fb_notif.notifier_call = wireless_fb_notifier_cb;
	rc = drm_register_client(&di->wireless_fb_notif);
	if (rc < 0) {
	dev_err(di->dev,
	"Couldn't register notifier rc=%d\n", rc);
	return rc;
	}
	//INIT_DELAYED_WORK(&di->screen_on_work, wireless_screen_on_work);
	} else
	dev_err(di->dev, "Unsupported fb notifier \n");
#endif
	 */

	dev_info(di->dev, "[idt] success probe idtp922x driver\n");
	schedule_delayed_work(&di->chg_detect_work, 3 * HZ);

#ifdef IDTP9220_SRAM_UPDATE
	INIT_DELAYED_WORK(&di->sram_update_work, idtp9220_sram_update_work);
	schedule_delayed_work(&di->sram_update_work, 10 * HZ);
#endif
	return 0;

	cleanup:
	free_irq(di->irq, di);
	cancel_delayed_work_sync(&di->irq_work);
	i2c_set_clientdata(client, NULL);

	return ret;
}

static int idtp9220_remove(struct i2c_client *client)
{
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	gpio_free(di->dt_props.enable_gpio);
	gpio_free(di->dt_props.reverse_gpio);
	cancel_delayed_work_sync(&di->irq_work);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static void idtp9220_shutdown(struct i2c_client *client)
{
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (di->power_good_flag) {
		idtp9220_set_enable_mode(di, false);
		msleep(10);
		idtp9220_set_enable_mode(di, true);
	}
}

static const struct i2c_device_id idtp9220_id[] = {
	{IDT_DRIVER_NAME, 0},
	{},
};

static const struct of_device_id idt_match_table[] = {
	{.compatible = "idt,p9220"},
	{}
};

MODULE_DEVICE_TABLE(i2c, idtp9220_id);

static struct i2c_driver idtp9220_driver = {
	.driver = {
		.name = IDT_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = idt_match_table,
	},
	.probe = idtp9220_probe,
	.remove = idtp9220_remove,
	.shutdown = idtp9220_shutdown,
	.id_table = idtp9220_id,
};

static int __init idt_init(void)
{
	int ret;
	int drv_load = 0;

	drv_load = get_board_version();
	if (!drv_load)
		return 0;

	ret = i2c_add_driver(&idtp9220_driver);
	if (ret)
		printk(KERN_ERR "idt i2c driver init failed!\n");

	return ret;
}

static void __exit idt_exit(void)
{
	i2c_del_driver(&idtp9220_driver);
}

module_init(idt_init);
module_exit(idt_exit);

MODULE_AUTHOR("bsp@xiaomi.com");
MODULE_DESCRIPTION("IDT Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
