/**
 * Copyright “Copyright (C) 2018 XiaoMi, Inc
 */

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
};

struct idtp9220_dt_props {
	unsigned int irq_gpio;
	unsigned int enable_gpio;
	unsigned int wpc_det_gpio;
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
	struct mutex	read_lock;
	struct mutex	write_lock;
	struct delayed_work	chg_monitor_work;
	struct delayed_work	chg_detect_work;
	struct delayed_work	bpp_connect_load_work;
	struct delayed_work	epp_connect_load_work;
	struct delayed_work	cmd_check_work;
	struct delayed_work	vout_regulator_work;
	struct delayed_work	rx_vout_work;
	struct delayed_work	dc_check_work;
	struct delayed_work	fast_operate_work;
	struct delayed_work	mophie_tx_work;
	struct delayed_work	bpp_e5_tx_work;
	struct delayed_work	qc2_f1_tx_work;
	struct delayed_work	qc3_epp_work;

	bool				screen_on;
	int tx_charger_type;
	int status;
	int count_5v;
	int count_9v;
	int count_12v;
	int count_15v;
	int exchange;
	int epp_exchange;
	int dcin_present;
	int epp;
	int vbuck;
	int power_off_mode;
	int power_max;
	u8  header;
	u8  cmd;
	int is_compatible_hwid;
	int last_vin;
	int last_vbuck;
	int is_car_tx;
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
};

void idtp922x_request_adapter(struct idtp9220_device_info *di);
static void idtp9220_set_charging_param(struct idtp9220_device_info *di);

/*static int idt_signal_strength = 0;
module_param_named(ss, idt_signal_strength, int, 0600);
*/

static int idt_signal_range = 2;
module_param_named(signal_range, idt_signal_range, int, 0644);

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
	di->bus.write_buf(di, REG_SSINTCLR, buf, size);
	di->bus.write(di, REG_SSCMND, CLRINT);
}

void idtp922x_sendPkt(struct idtp9220_device_info *di, ProPkt_Type *pkt) {
	u32 size = ExtractPacketSize(pkt->header)+1;
	di->bus.write_buf(di, REG_PROPPKT, (u8 *)pkt, size); // write data into proprietary packet buffer
	di->bus.write(di, REG_SSCMND, SENDPROPP); // send proprietary packet

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
	if(is_epp)
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

static void idtp9220_int_enable(struct idtp9220_device_info *di)
{
	u8 int_val;
	di->bus.read(di, REG_INTR_EN_L, &int_val);
	di->bus.write(di, REG_INTR_EN_L, (int_val | OVER_EVENT_OCCUR));
	dev_info(di->dev, "[idtp9220]: int enabled status 0x%x\n", (int_val | OVER_EVENT_OCCUR));
}


static int idtp9220_get_vout_regulator(struct idtp9220_device_info *di)
{
	u8 vout_l, vout_h;
	u16 vout;

	if(!di)
		return 0;

	di->bus.read(di, REG_REGULATOR_L, &vout_l);
	di->bus.read(di, REG_REGULATOR_H, &vout_h);
	vout = vout_l | ((vout_h & 0xff)<< 8);
	dev_info(di->dev, "vout regulator get vol: %d\n", vout);

	return vout;
}

void idtp9220_set_toggle_mode(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY18;
	pkt.cmd = BC_TX_TOGGLE;
	idtp922x_sendPkt(di, &pkt);
}

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

	if(!di)
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

static int idtp9220_get_vbuck(struct idtp9220_device_info *di)
{
	u8 vbuck_l, vbuck_h;
	u16 vbuck_ret;

	if(!di)
		return 0;

	di->bus.read(di, 0x08, &vbuck_l);
	di->bus.read(di, 0x09, &vbuck_h);
	vbuck_ret = vbuck_l | (vbuck_h << 8);

	return vbuck_ret;
}


static int idtp9220_get_vout(struct idtp9220_device_info *di)
{
	u8 vout_l, vout_h;

	if(!di)
		return 0;

	di->bus.read(di, REG_ADC_VOUT_L, &vout_l);
	di->bus.read(di, REG_ADC_VOUT_H, &vout_h);
	di->vout = vout_l | ((vout_h & 0xf)<< 8);
	di->vout = di->vout * 10 * 21 * 1000 / 40950 + ADJUST_METE_MV; //vout = val/4095*10*2.1

	return di->vout;
}

static void idtp9220_set_vout(struct idtp9220_device_info *di, int mv)
{
	u8 val;
	if(!di)
		return;
	val = (mv -3500)/100;
	di->bus.write(di, REG_VOUT_SET, val);
	dev_info(di->dev, "[idtp9220]: set vout voltage is 0x%x\n", val);
}

static void idtp9220_set_reset(struct idtp9220_device_info *di)
{
	if(!di)
		return;
	di->bus.write(di, REG_RX_RESET, 0x01);
	dev_info(di->dev, "[idtp9220]: set RX reset\n");
}
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

static int idtp9220_get_iout(struct idtp9220_device_info *di)
{
	u8 cout_l, cout_h;

	if(!di)
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
	di->vrect = di->vrect * 10 * 21 * 1000 / 40950;            //vrect = val/4095*10*2.1

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
	return sprintf(buf, "Vout ADC Value: %dMV\n", vout);

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
		dev_err(di->dev, "Store Val %s : %ld is invalid!\n", buf, vout );
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

	return sprintf(buf, "Output Current: %dMA\n", cout);
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

	return sprintf(buf, "Output Current: %dkHz\n", f);
}

static void idtp9220_charging_info(struct idtp9220_device_info *di)
{
	int vbuck_ret = 0;
	if(!di)
		return;

	idtp9220_get_vout(di);
	idtp9220_get_iout(di);
	idtp9220_get_freq(di);
	idtp9220_get_vrect(di);
	vbuck_ret = idtp9220_get_vbuck(di);

	dev_info(di->dev, "%s:Vout:%dmV,Iout:%dmA,Freq:%dKHz,Vrect:%dmV,SS:%d, Vbuck:%d\n", __func__,
			di->vout, di->iout, di->f, di->vrect, di->ss, vbuck_ret);
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

	dev_info(di->dev, "[idtp] dc plug %s\n", enable ? "in" : "out");
	if(enable)
	{
		di->dcin_present = true;
		di->ss = 1;
	} else {
		schedule_delayed_work(&di->fast_operate_work, msecs_to_jiffies(200));
		di->status = NORMAL_MODE;
		di->count_9v = 0;
		di->count_5v = 0;
		di->count_12v = 0;
		di->count_15v = 0;
		di->exchange = 0;
		di->epp_exchange = 0;
		di->dcin_present = false;
		idt_signal_range = 2;
		di->ss = 2;
		di->vbuck = 0;
		di->epp = 0;
		di->last_vin = 0;
		di->last_icl = 0;
		di->last_vbuck = 0;
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
		di->tx_charger_type = ADAPTER_NONE;
		cancel_delayed_work(&di->chg_monitor_work);
		cancel_delayed_work(&di->cmd_check_work);
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

static DEVICE_ATTR(chip_enable, S_IWUSR | S_IRUGO, chip_enable_show, chip_enable_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IWUSR | S_IRUGO, chip_iout_show, chip_iout_store);
static DEVICE_ATTR(chip_freq, S_IRUGO, chip_freq_show, NULL);
static DEVICE_ATTR(vout_regulator, S_IWUSR | S_IRUGO, vout_regulator_show, vout_regulator_store);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_chip_version.attr,
	&dev_attr_chip_vout.attr,
	&dev_attr_chip_iout.attr,
	&dev_attr_chip_freq.attr,
	&dev_attr_chip_enable.attr,
	&dev_attr_vout_regulator.attr,
	NULL,
};

static const struct attribute_group sysfs_group_attrs = {
	.attrs = sysfs_attrs,
};

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
	u8 int_buf[2];
	u16 int_val;
	int rc = -1;

	rc = di->bus.read_buf(di, REG_INTR_L, int_buf, 2);
	if (rc < 0) {
		dev_err(di->dev, "%s: read int state error\n", __func__);
		return true;
	}
	int_val = int_buf[0] | (int_buf[1] << 8);
	if (int_val != 0) {
		dev_info(di->dev, "irq not clear right: 0x%04x\n", int_val);
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
#define ADAPTER_BPP_QC_VOL	9000
#define ADAPTER_BPP_LIMIT_VOL	6500
#define BPP_VOL_THRESHOLD	8000
#define ADAPTER_BPP_VOL		5000
#define ADAPTER_EPP_MI_VOL	15000
#define EPP_VOL_THRESHOLD	12000
#define EPP_VOL_LIM_THRESHOLD	13000
#define BPP_VOL_LIM_THRESHOLD	10000
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
	power_supply_changed(di->idtp_psy);
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
	if(ret < 0)
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
	if(val.intval) {
		dev_info(di->dev, "[idt] usb_online:%d set chip disable\n", val.intval);
		idtp9220_set_enable_mode(di, false);
		return;
	}

	if(di->dc_psy) {
		power_supply_get_property(di->dc_psy,
			POWER_SUPPLY_PROP_ONLINE, &val);
		dev_info(di->dev, "[idt] dc_online %d\n", val.intval);
		if(val.intval && di->wireless_psy) {
			idtp9220_int_enable(di);
			wk_val.intval = 1;
			power_supply_set_property(di->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
			di->epp = idtp9220_get_power_profile(di);

			get_cmdline(di);
			if (!di->power_off_mode)
				idtp9220_set_reset(di);
			else
				schedule_delayed_work(&di->irq_work,
					msecs_to_jiffies(30));
		}
	}
}

#define DC_LOAD_CURRENT 2000000
#define DC_FUL_CURRENT 50000
#define SCREEN_OFF_FUL_CURRENT 220000
#define DC_LOW_CURRENT 350000
#define DC_SDP_CURRENT 500000
#define DC_DCP_CURRENT 500000
#define DC_PD_CURRENT 800000
#define DC_MI_CURRENT  2100000
#define DC_MI_STEP1_CURRENT  1200000
#define DC_QC3_CURRENT 1400000
#define DC_QC2_CURRENT 1100000
#define DC_BPP_CURRENT 850000
#define DC_BPP_AUTH_FAIL_CURRENT 800000
#define ICL_EXCHANGE_CURRENT 600000
#define ICL_EXCHANGE_COUNT   5 /*5 = 1min*/
#define EXCHANGE_9V          0x0
#define EXCHANGE_5V          0x1
#define EXCHANGE_15V         0x0
#define EXCHANGE_12V         0x1
#define LIMIT_EPP_IOUT 500
#define LIMIT_BPP_IOUT 400
#define LIMIT_SOC 80
#define TAPER_SOC 95
#define FULL_SOC 100
#define TAPER_VOL 4350000
#define TAPER_CUR -500000

static void idtp9220_bpp_connect_load_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
			container_of(work, struct idtp9220_device_info,
			bpp_connect_load_work.work);

	idtp922x_set_pmi_icl(di, DC_DCP_CURRENT);

}

static void idtp9220_epp_connect_load_work(struct work_struct *work)
{

	struct idtp9220_device_info *di =
			container_of(work, struct idtp9220_device_info,
			epp_connect_load_work.work);
	int dc_load_curr = 0;

	if (di->power_max <= 10000)
		dc_load_curr = (di->power_max - 1000)/5 * 1000;
	else
		dc_load_curr = DC_LOAD_CURRENT;

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
		schedule_delayed_work(&di->vout_regulator_work,
							msecs_to_jiffies(10));
	}

}

static void idtp9220_mophie_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
			container_of(work, struct idtp9220_device_info,
			mophie_tx_work.work);
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
			if (int_val == 0x0059) {
				dev_info(di->dev, "write vrect target\n");
				di->bus.write(di, 0x0090, 0x0);
				di->bus.write(di, 0x0091, 0x8);
				di->bus.write(di, 0x003E, 0x41);
			}
		}
	}

}

static void idtp9220_bpp_e5_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
			container_of(work, struct idtp9220_device_info,
			bpp_e5_tx_work.work);
	int icl_curr = 0, vbuck = 0, adapter_vol = 0;
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

	if (di->tx_charger_type == ADAPTER_QC3) {
		adapter_vol = ADAPTER_BPP_QC_VOL;
		vbuck = VBUCK_QC_VOL;
		icl_curr = DC_QC3_CURRENT;
	} else if (di->tx_charger_type == ADAPTER_QC2) {
		adapter_vol = ADAPTER_BPP_QC_VOL;
		vbuck = VBUCK_QC_VOL;
		icl_curr = DC_QC2_CURRENT;
	}

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
	if(di->count_5v > ICL_EXCHANGE_COUNT ||
			(di->exchange == EXCHANGE_5V && di->count_9v <= ICL_EXCHANGE_COUNT - 3))
	{
		dev_info(di->dev, "iout less than 500mA ,set vout to 6.5v\n");
		adapter_vol = ADAPTER_BPP_LIMIT_VOL;
		vbuck = VBUCK_DEFAULT_VOL;
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
				adapter_vol = ADAPTER_BPP_LIMIT_VOL;
				vbuck = VBUCK_DEFAULT_VOL;
				icl_curr = min(DC_SDP_CURRENT, icl_curr);
			}
			break;
		case TAPER_MODE:
			dev_info (di->dev, "[bpp] taper mode set vout to 6.5v\n");
			adapter_vol = ADAPTER_BPP_LIMIT_VOL;
			vbuck = VBUCK_DEFAULT_VOL;
			icl_curr = min(DC_SDP_CURRENT, icl_curr);

			if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
				di->status = FULL_MODE;
			else if (soc < TAPER_SOC - 1)
				di->status = NORMAL_MODE;
			break;
		case FULL_MODE:
			dev_info (di->dev, "[bpp] charge full set vout to 6.5v\n");
			adapter_vol = ADAPTER_BPP_LIMIT_VOL;
			vbuck = VBUCK_DEFAULT_VOL;
			icl_curr = SCREEN_OFF_FUL_CURRENT;

			if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
				di->status = RECHG_MODE;
				icl_curr = DC_LOW_CURRENT;
			}
			break;
		case RECHG_MODE:
			dev_info (di->dev, "[bpp] recharge mode set icl to 300mA\n");
			adapter_vol = ADAPTER_BPP_LIMIT_VOL;
			vbuck = VBUCK_DEFAULT_VOL;
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
			adapter_vol = ADAPTER_BPP_LIMIT_VOL;
			vbuck = VBUCK_DEFAULT_VOL;
			icl_curr = min(DC_SDP_CURRENT, icl_curr);
			break;
		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
			adapter_vol = ADAPTER_BPP_LIMIT_VOL;
			vbuck = VBUCK_DEFAULT_VOL;
			icl_curr = min(SCREEN_OFF_FUL_CURRENT, icl_curr);
			break;
		case POWER_SUPPLY_HEALTH_COLD:
		case POWER_SUPPLY_HEALTH_HOT:
			adapter_vol = ADAPTER_BPP_LIMIT_VOL;
			vbuck = VBUCK_DEFAULT_VOL;
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
			msleep(200);
			idtp9220_set_vout_regulator(di, vbuck);
			msleep(200);
		} else {
			idtp9220_set_vout_regulator(di, 5800);
			msleep(100);
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
	int vout = VOUT_VAL_6500_MV;
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

	if (vout != di->last_qc2_vout) {
		dev_info(di->dev, "qc2+f1_tx, set new vout: %d, last_vout: %d\n",
			vout, di->last_qc2_vout);
		di->last_qc2_vout = vout;
		di->bus.write(di, 0x003E, vout);
		msleep(100);
		idtp9220_set_vout_regulator(di, VBUCK_DEFAULT_VOL);
		msleep(100);
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
	int adapter_vol = 0;
	int dc_icl = DC_MI_STEP1_CURRENT;

	if (adapter_vol != di->last_qc3_vout) {
		dev_info(di->dev, "qc3 epp, set new vout: %d, last_vout: %d\n",
			adapter_vol, di->last_qc3_vout);
		di->last_qc3_vout = adapter_vol;
		idtp922x_set_adap_vol(di, adapter_vol);
		msleep(200);
	}

	if (dc_icl != di->last_qc3_icl) {
		dev_info(di->dev, "qc3_epp, set new icl: %d, last_icl: %d\n",
			dc_icl, di->last_qc3_icl);
		di->last_qc3_icl = dc_icl;
		if (dc_icl == DC_MI_STEP1_CURRENT) {
			schedule_delayed_work(&di->vout_regulator_work,
					msecs_to_jiffies(0));
		}
		idtp922x_set_pmi_icl(di, dc_icl);
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

	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;
	}

	if(di)
		idtp9220_get_vout(di);
	else
		return;

	if(di->epp)
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

	if(di->epp && (vinc || di->is_epp_qc3 || di->is_vin_limit) && soc < 95) {
		ret = idtp9220_get_property_names(di);
		if (di->wireless_psy) {
			val.intval = 1;
			power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
			msleep(200);
			if(!di->is_epp_qc3 && !di->is_vin_limit)
				idtp922x_set_pmi_icl(di, DC_MI_CURRENT);
		} else
			dev_err(di->dev, "[idt] no wireless psy\n");
	}
out:
	dev_info(di->dev, "[idt] %s: epp=%d vbuck=%d vout= %d\n", __func__, di->epp, di->vbuck, di->vout);
	return;
}

static void idtp9220_set_charging_param(struct idtp9220_device_info *di)
{
	int soc = 0, health = 0, batt_sts = 0;
	int adapter_vol = 0, icl_curr = 0;
	int cur_now = 0, vol_now = 0, vin_inc = 0;
	union power_supply_propval val = {0, };

		switch (di->tx_charger_type) {
		case ADAPTER_QC2:
			adapter_vol = ADAPTER_BPP_QC_VOL;
			di->vbuck = VBUCK_QC_VOL;
			icl_curr = DC_QC2_CURRENT;
			break;
		case ADAPTER_QC3:
			if(!di->epp) {
				adapter_vol = ADAPTER_BPP_QC_VOL;
				di->vbuck = VBUCK_QC_VOL;
				icl_curr = DC_QC3_CURRENT;
			} else {
				di->is_epp_qc3 = 1;
				icl_curr = DC_MI_STEP1_CURRENT;
			}
			break;
		case ADAPTER_XIAOMI_PD:
		case ADAPTER_PD:
			if(di->epp) {
				di->vbuck = VBUCK_QC_VOL;
				icl_curr = DC_QC3_CURRENT;
			} else {
				di->vbuck = VBUCK_DEFAULT_VOL;
				icl_curr = DC_PD_CURRENT;
			}
			break;
		case ADAPTER_AUTH_FAILED:
			if(di->epp) {
				di->vbuck = VBUCK_QC_VOL;
				if (di->power_max <= 10000)
					icl_curr = ((di->power_max - 1000) / (VBUCK_QC_VOL/1000)) * 1000;
				else
					icl_curr = DC_QC3_CURRENT;
			} else {
				di->vbuck = VBUCK_DEFAULT_VOL;
				icl_curr = DC_BPP_AUTH_FAIL_CURRENT;
			}
			break;
		case ADAPTER_DCP:
		case ADAPTER_CDP:
			di->vbuck = VBUCK_DEFAULT_VOL;
			if (di->is_compatible_hwid)
				icl_curr = DC_BPP_AUTH_FAIL_CURRENT;
			else
				icl_curr = DC_DCP_CURRENT;
			break;
		case ADAPTER_SDP:
			di->vbuck = VBUCK_DEFAULT_VOL;
			icl_curr = DC_SDP_CURRENT;
			break;
		case ADAPTER_XIAOMI_QC3:
		case ADAPTER_ZIMI_CAR_POWER:
		case ADAPTER_XIAOMI_PD_40W:
			if(di->epp) {
				adapter_vol = ADAPTER_EPP_MI_VOL;
				icl_curr = DC_MI_STEP1_CURRENT;
			} else {
				di->vbuck = VBUCK_QC_VOL;
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

	if((batt_sts == POWER_SUPPLY_STATUS_DISCHARGING)
		&& di->dcin_present
		&& di->power_good_flag) {
			dev_info(di->dev, "discharge when dc and pwr present, reset chip\n");
			schedule_delayed_work(&di->fast_operate_work, msecs_to_jiffies(0));
			return;
	}
	dev_info(di->dev, "[idtp] soc:%d,vol_now:%d,cur_now:%d,health:%d, bat_status:%d\n",
			soc, vol_now, cur_now, health, batt_sts);

	/* adapter:qc2/qc3; tx:e5/d5x_10W;
	 * vout/vbuck/psns is setted in delayed work
	 */
	if(adapter_vol == ADAPTER_BPP_QC_VOL && di->is_compatible_hwid) {
		schedule_delayed_work(&di->bpp_e5_tx_work, msecs_to_jiffies(0));
		goto out;
	}

	/* adapter:qc2; tx:F1_27W;
	 * vout/vbuck/psns is setted in delayed work
	 */
	if (di->tx_charger_type == ADAPTER_QC2 && di->is_f1_tx) {
		schedule_delayed_work(&di->qc2_f1_tx_work, msecs_to_jiffies(0));
		goto out;
	}

	/* adapter:qc3_epp; tx:F1_27W;
	 * vout/vbuck/psns is setted in delayed work
	 */
	if (di->is_epp_qc3) {
		schedule_delayed_work(&di->qc3_epp_work, msecs_to_jiffies(0));
		goto out;
	}

	if (adapter_vol > 0 && adapter_vol != di->last_vin) {
		idtp922x_set_adap_vol(di, adapter_vol);
		vin_inc = 1;
		di->last_vin = adapter_vol;
		if (adapter_vol == ADAPTER_EPP_MI_VOL || adapter_vol == EPP_VOL_THRESHOLD) {
			msleep(100);
			idtp922x_set_pmi_icl(di, icl_curr);
			di->last_icl = icl_curr;
			if (di->is_vin_limit)
				schedule_delayed_work(&di->vout_regulator_work,
										msecs_to_jiffies(0));
		}
	} else {
		if (((di->tx_charger_type == ADAPTER_QC2) || (di->tx_charger_type == ADAPTER_QC3))
			&& !di->epp && !di->bpp_vout_rise && icl_curr > DC_DCP_CURRENT)
			dev_info(di->dev, "don't set curr with qc2/qc3 of bpp when vout low\n");
		else {
			if (icl_curr > 0 && icl_curr != di->last_icl) {
				idtp922x_set_pmi_icl(di, icl_curr);
				di->last_icl = icl_curr;
			}
			msleep(150);
			if (di->vbuck > 0 && di->vbuck != di->last_vbuck) {
				idtp9220_set_vout_regulator(di, di->vbuck);
				di->last_vbuck = di->vbuck;
			}
		}
	}

	printk("[idtp] di->status:0x%x,adapter_vol =%d,icl_curr=%d, vbuck=%d, last_vin=%d, last_icl=%d\n",
			di->status, adapter_vol, icl_curr, di->vbuck, di->last_vin, di->last_icl);

	if (adapter_vol == ADAPTER_EPP_MI_VOL ||
		adapter_vol == ADAPTER_BPP_QC_VOL ||
			adapter_vol == VBUCK_FULL_VOL) {
		if (vin_inc && (!di->is_vin_limit && !di->is_epp_qc3))
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
			/* check if mophie tx after 100ms */
			schedule_delayed_work(&di->mophie_tx_work, msecs_to_jiffies(100));
			/* check dc present to judge device skewing */
			schedule_delayed_work(&di->dc_check_work, msecs_to_jiffies(2500));
			di->power_good_flag = 1;
			val.intval = 1;
			idtp9220_int_enable(di);
		}
		else {
			cancel_delayed_work(&di->dc_check_work);
			di->power_good_flag = 0;
			val.intval = 0;
			di->ss = 2;
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
	u8 int_buf[2] = {0};
	u16 int_val = 0;
	int rc = 0;
	u8 recive_data[5] = {0};
	static int retry = 0;
	static int retry_id = 0;
	static int retry_count = 0;
	int tx_vin = 0;
	int irq_level;

	if (gpio_is_valid(di->dt_props.irq_gpio))
		irq_level = gpio_get_value(di->dt_props.irq_gpio);
	else {
		dev_err(di->dev, "%s: irq gpio not provided\n", __func__);
		irq_level = -1;
		return;
	}
	if(irq_level) {
		return;
	}

	rc = di->bus.read_buf(di, REG_INTR_L, int_buf, 2);
	if (rc < 0) {
		dev_err(di->dev, "[idt]read int state error: %d\n", rc);
		goto out;
	}
	/* clear int and enable irq immediately when read int register*/
	idtp922x_clrInt(di, int_buf, 2);

	int_val = int_buf[0] | (int_buf[1] << 8);
	dev_info(di->dev, "[idt] int: 0x%04x\n", int_val);

	msleep(5);

	if (need_irq_cleared(di))
	{
		u8 clr_buf[2] = {0xFF, 0xFF};
		idtp922x_clrInt(di, clr_buf, 2);
		msleep(5);
	}
	if ((int_val & INT_OV_CURR) || (int_val & INT_OV_TEMP)) {
		dev_info(di->dev, "[idt] ocp or otp found \n");
		schedule_delayed_work(&di->chg_monitor_work,
						msecs_to_jiffies(0));
		goto out;
	}

	if(int_val & INT_VOUT_ON) {
		di->epp = idtp9220_get_power_profile(di);
		if(di->epp) {
			di->power_max = idtp9220_get_power_max(di);
			schedule_delayed_work(&di->epp_connect_load_work,
							msecs_to_jiffies(0));
		} else
			schedule_delayed_work(&di->bpp_connect_load_work,
							msecs_to_jiffies(0));
		if (int_val & INT_IDAUTH_SUCESS)
			idtp9220_send_device_auth(di);
		goto out;
	}

	if (int_val & INT_VSWITCH_SUCESS) {
		schedule_delayed_work(&di->vout_regulator_work,
							msecs_to_jiffies(50));
		cancel_delayed_work(&di->cmd_check_work);
		goto out;
	}

	if (int_val & INT_IDAUTH_SUCESS) {
		idtp9220_send_device_auth(di);
		//idtp922x_request_adapter(di);
		goto out;
	}


	if(int_val & INT_AUTH_SUCESS) {
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
		if(((int_val & INT_AUTH_FAIL) || (int_val == 0)) && (retry < 5)){
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
			if(recive_data[4] == 0x01 &&
				recive_data[2] == 0x2 &&
				recive_data[3] == 0x8 &&
				recive_data[1] == 0x6)
			{
				di->is_car_tx = 1;
			}
			idtp922x_request_adapter(di);
			break;
		case BC_TX_COMPATIBLE_HWID:
			if(recive_data[1] == 0x12 && recive_data[2])
				di->is_compatible_hwid = 1;
			if(recive_data[1] == 0x16 && recive_data[2] == 0x11)
				di->is_f1_tx = 1;
			idtp922x_request_adapter(di);
			break;
		case BC_ADAPTER_TYPE:
			if (di->is_car_tx && (recive_data[1] == ADAPTER_XIAOMI_QC3))

				di->tx_charger_type = ADAPTER_ZIMI_CAR_POWER;
			else
				di->tx_charger_type = recive_data[1];

			if(!di->epp && (di->tx_charger_type == ADAPTER_QC3 ||
						di->tx_charger_type == ADAPTER_QC2)) {
				idtp922x_set_adap_vol(di, ADAPTER_BPP_VOL);
			}
			schedule_delayed_work(&di->rx_vout_work,
								msecs_to_jiffies(100));
			schedule_delayed_work(&di->chg_monitor_work,
								msecs_to_jiffies(1000));
			if (di->wireless_psy)
				power_supply_changed(di->wireless_psy);
			break;
		case BC_READ_Vin:
			tx_vin = recive_data[1] | (recive_data[2] << 8);
			if(!di->power_off_mode) {
				if (di->epp)
					idtp922x_set_adap_vol(di, ADAPTER_EPP_MI_VOL);
				else
					idtp922x_set_adap_vol(di, ADAPTER_BPP_QC_VOL);
			}
			break;
		case BC_RX_ID_AUTH:
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
	if(ret) {
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
	if(ret) {
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
		val->intval = idtp9220_get_vout_regulator(di);
		break;
	case POWER_SUPPLY_PROP_TX_ADAPTER:
		val->intval = di->tx_charger_type;
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
		if(data >= 6000) {
			idtp9220_set_vout_regulator(di, data);
			dev_info(di->dev, "[idt] set buck %s\n", __func__);
		}
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
	di->status = NORMAL_MODE;
	di->regmap = devm_regmap_init_i2c(client, &i2c_idtp9220_regmap_config);
	if (!di->regmap)
	    return -ENODEV;
	di->bus.read = idtp9220_read;
	di->bus.write = idtp9220_write;
	di->bus.read_buf = idtp9220_read_buffer;
	di->bus.write_buf = idtp9220_write_buffer;
	INIT_DELAYED_WORK(&di->irq_work, idtp9220_irq_work);
	INIT_DELAYED_WORK(&di->wpc_det_work, idtp9220_wpc_det_work);
	mutex_init(&di->read_lock);
	mutex_init(&di->write_lock);
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

	if(sysfs_create_group(&client->dev.kobj, &sysfs_group_attrs)) {
		dev_err(&client->dev, "create sysfs attrs failed!\n");
		ret = -EIO;
		goto cleanup;
	}
	idtp_cfg.drv_data = di;
	di->idtp_psy = power_supply_register(di->dev,
			&idtp_psy_desc,
			&idtp_cfg);

	INIT_DELAYED_WORK(&di->chg_monitor_work,idtp9220_monitor_work);
	INIT_DELAYED_WORK(&di->chg_detect_work,idtp9220_chg_detect_work);
	INIT_DELAYED_WORK(&di->bpp_connect_load_work,idtp9220_bpp_connect_load_work);
	INIT_DELAYED_WORK(&di->epp_connect_load_work,idtp9220_epp_connect_load_work);
	INIT_DELAYED_WORK(&di->cmd_check_work,idtp9220_cmd_check_work);
	INIT_DELAYED_WORK(&di->vout_regulator_work,idtp9220_vout_regulator_work);
	INIT_DELAYED_WORK(&di->rx_vout_work,idtp9220_rx_vout_work);
	INIT_DELAYED_WORK(&di->dc_check_work,idtp9220_dc_check_work);
	INIT_DELAYED_WORK(&di->fast_operate_work,idtp9220_fast_operate_work);
	INIT_DELAYED_WORK(&di->mophie_tx_work, idtp9220_mophie_tx_work);
	INIT_DELAYED_WORK(&di->bpp_e5_tx_work, idtp9220_bpp_e5_tx_work);
	INIT_DELAYED_WORK(&di->qc2_f1_tx_work, idtp9220_qc2_f1_tx_work);
	INIT_DELAYED_WORK(&di->qc3_epp_work, idtp9220_qc3_epp_work);


	dev_info(di->dev, "[idt] success probe idtp922x driver\n");
	schedule_delayed_work(&di->chg_detect_work, 3 * HZ);

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
	cancel_delayed_work_sync(&di->irq_work);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static void idtp9220_shutdown(struct i2c_client *client)
{
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (di->power_good_flag)
		idtp9220_set_reset(di);
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

module_i2c_driver(idtp9220_driver);

MODULE_AUTHOR("bsp-charging@xiaomi.com");
MODULE_DESCRIPTION("IDTP9220 Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
