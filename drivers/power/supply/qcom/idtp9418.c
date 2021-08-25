#include <linux/module.h>
#include <linux/alarmtimer.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/sysfs.h>
#include <asm/unaligned.h>
#include <idtp9418.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/memory.h>
#include <linux/pmic-voter.h>
#include <soc/qcom/socinfo.h>

static struct idtp9220_device_info *g_di;
#ifdef CONFIG_FACTORY_BUILD
#define REVERSE_TEST_READY_CHECK_DELAY_MS 12000
#define REVERSE_DPING_CHECK_DELAY_MS 15000
#else
#define REVERSE_TEST_READY_CHECK_DELAY_MS 8000
#define REVERSE_DPING_CHECK_DELAY_MS 10000
#endif
#define REVERSE_CHG_CHECK_DELAY_MS 10000
#define BPP_QC_7W_CURRENT 700000
#define BPP_DEFAULT_CURRENT 800000
#define EPP_DEFAULT_SWITCH_CURRENT 1800000
#define EPP_DEFAULT_BYPASS_CURRENT 800000
#define DEFAULT_40V_PSNS_CURRENT 100000
#define CURRENT_CHANGE_2_TO_1 30000
#define DC_LOAD_CURRENT 2000000
#define DC_FUL_CURRENT 50000
#define SCREEN_OFF_FUL_CURRENT 250000
#define DC_LOW_CURRENT 350000
#define DC_SDP_CURRENT 500000
#define DC_DCP_CURRENT 500000
#define DC_PD_CURRENT 800000
#define DC_MI_CURRENT_20W  2100000
#define DC_MI_CURRENT_30W  3100000
#define WIRELESS_BY_USBIN_MI_CURRENT  2000000
#define DC_MI_STEP1_CURRENT  1200000
#define DC_QC3_CURRENT 500000
#define DC_EPP_QC3_CURRENT 900000
#define DC_QC2_CURRENT 1000000
#define DC_BPP_CURRENT 850000
#define DC_BPP_AUTH_FAIL_CURRENT 850000
#define DC_PAN_CURRENT_UA 1200000
#define ICL_EXCHANGE_CURRENT 600000
#define ICL_EXCHANGE_COUNT   10	/*5 = 2min */

#define ICL_CP_2SBATT_40W_MA 1000000
#define ICL_CP_2SBATT_30W_MA 1000000
#define ICL_CP_2SBATT_20W_MA 1000000

#define ICL_TAPER_2SBATT_40W_MA 2000000
#define ICL_TAPER_2SBATT_30W_MA 1800000
#define ICL_TAPER_2SBATT_20W_MA 1200000

#define EXCHANGE_9V          0x0
#define EXCHANGE_5V          0x1
#define EXCHANGE_15V         0x0
#define EXCHANGE_10V         0x1
#define LIMIT_EPP_IOUT 500
#define LIMIT_BPP_IOUT 500
#define LIMIT_SOC 95
#define TAPER_SOC 86
#define EPP_TAPER_SOC 95
#define FULL_SOC 100
#define TAPER_VOL 4350000
#define TAPER_CUR -500000
#define HIGH_THERMAL_LEVEL_THR		12
#define OCP_PROTECTION_VOTER "OCP_PROTECTION_VOTER"
#define VOICE_LIMIT_FCC_VOTER "VOICE_LIMIT_FCC_VOTER"
#define VOICE_LIMIT_FCC_1A_VOTER "VOICE_LIMIT_FCC_1A_VOTER"
#define IDTP_WAKE_VOTER "IDTP_WAKE_VOTER"
#define IDTP_DOWNLAOD_WAKE_VOTER "IDTP_DOWNLAOD_WAKE_VOTER"

#define MAX_PMIC_ICL_UA 1500000
#define EPP_PLUS_VOL	15
#define BPP_PLUS_VOL	9
#define EPP_BASE_VOL	12
#define FULL_VOL       7
#define CP_CLOSE_VOUT_MV 14200
#define PAN_BBC_MAX_ICL_MA 1300000

//add for soc 100 retry
#define SOC_100_RETRY 6

#define MAC_LEN 6

struct idtp9220_access_func {
	int (*read) (struct idtp9220_device_info *di, u16 reg, u8 *val);
	int (*write) (struct idtp9220_device_info *di, u16 reg, u8 val);
	int (*read_buf) (struct idtp9220_device_info *di,
			 u16 reg, u8 *buf, u32 size);
	int (*write_buf) (struct idtp9220_device_info *di,
			  u16 reg, u8 *buf, u32 size);
	int (*mask_write) (struct idtp9220_device_info *di,
			   u16 reg, u8 mask, u8 val);
};

struct idtp9220_dt_props {
	unsigned int irq_gpio;
	unsigned int hall3_gpio;
	unsigned int hall4_gpio;
	unsigned int wpc_det_gpio;
	unsigned int reverse_gpio;
	unsigned int rx_ovp_ctl_gpio;
	unsigned int reverse_boost_enable_gpio;
	bool wireless_by_usbin;
	bool only_idt_on_cmi;
	bool is_urd_device;
};

struct idtp9220_device_info {
	int chip_enable;
	char *name;
	struct device *dev;
	struct idtp9220_access_func bus;
	struct regmap *regmap;
	struct idtp9220_dt_props dt_props;
	int irq;
	int hall3_irq;
	int hall4_irq;
	int wpc_irq;
	struct delayed_work irq_work;
	struct delayed_work hall3_irq_work;
	struct delayed_work hall4_irq_work;
	struct delayed_work wpc_det_work;
	struct pinctrl *idt_pinctrl;
	struct pinctrl_state *idt_gpio_active;
	struct pinctrl_state *idt_gpio_suspend;
	struct power_supply *usb_psy;
	struct power_supply *dc_psy;
	struct power_supply *batt_psy;
	struct power_supply *idtp_psy;
	struct power_supply *wireless_psy;
	struct power_supply *ln_psy;
	struct power_supply *pc_port_psy;
	struct mutex read_lock;
	struct mutex write_lock;
	struct mutex sysfs_op_lock;
	struct mutex reverse_op_lock;
	struct delayed_work chg_monitor_work;
	struct delayed_work chg_detect_work;
	struct delayed_work bpp_connect_load_work;
	struct delayed_work epp_connect_load_work;
	struct delayed_work cmd_check_work;
	struct delayed_work vout_regulator_work;
	struct delayed_work load_fod_param_work;
	struct delayed_work rx_vout_work;
	struct delayed_work dc_check_work;
	struct delayed_work initial_tx_work;
	struct delayed_work bpp_e5_tx_work;
	struct delayed_work pan_tx_work;
	struct delayed_work	voice_tx_work;
	struct delayed_work qc2_f1_tx_work;
	struct delayed_work qc3_epp_work;
	struct delayed_work oob_set_cep_work;
	struct delayed_work oob_set_ept_work;
	struct delayed_work rx_ready_check_work;
	struct delayed_work rx_vout_cp_close_work;
	struct delayed_work keep_awake_work;
	struct delayed_work oob_clean_work;
	struct delayed_work reverse_chg_work;
	struct votable		*icl_votable;
	struct mutex		wpc_det_lock;
	struct votable	*ocp_disable_votable;
	bool 				wait_for_reverse_test;
/*
reverse work
*/
	struct delayed_work reverse_ept_type_work;
	struct delayed_work reverse_chg_state_work;
	struct delayed_work reverse_dping_state_work;
	struct delayed_work reverse_test_state_work;
	struct delayed_work reverse_sent_state_work;
	struct alarm reverse_dping_alarm;
	struct alarm reverse_chg_alarm;
	struct alarm reverse_test_ready_alarm;


//end
#ifdef IDTP9220_SRAM_UPDATE
	struct delayed_work sram_update_work;
#endif

/*
Factory Test Work
*/
	struct delayed_work get_vout_work;
	struct delayed_work get_iout_work;
	struct delayed_work fw_download_work;
	struct delayed_work idt_first_boot;

	/*
	   #ifdef CONFIG_DRM
	   struct notifier_block                wireless_fb_notif;
	   struct mutex                 screen_lock;
	   bool                         screen_icl_status;
	   #endif
	 */
	bool screen_on;
	int tx_charger_type;
	int status;
	int count_5v;
	int count_9v;
	int exchange;
	int dcin_present;
	int epp;
	int power_off_mode;
	int power_max;
	u8 header;
	u8 cmd;
	int is_compatible_hwid;
	int last_vin;
	int is_car_tx;
	int is_ble_tx;
	int is_zm_mobil_tx;
	int is_zm_20w_tx;
	int is_voice_box_tx;
	int is_pan_tx;
	int last_icl;
	int power_good_flag;
	int vout_on;

	int vout;
	int iout;
	int f;
	int vrect;
	int ss;
	int is_epp_qc3;
	int last_bpp_icl;
	int last_bpp_vout;
	int is_f1_tx;
	int bpp_vout_rise;
	int last_qc2_vout;
	int last_qc2_icl;
	int last_qc3_vout;
	int last_qc3_icl;
	bool fw_update;
	int adapter_voltage;
	bool first_rise_flag;
	bool enable_ext5v;
	bool vswitch_ok;
	u8 mac_data[6];
	u8 pen_mac_data[6];
	int rpp_val;
	int cep_val;
	int is_reverse_mode;
	int is_reverse_chg;
	int is_boost_mode;
	int fw_version;
	int bpp_icl;
	int chip_ok;
	int otg_insert;
	bool disable_cp;
	struct votable		*fcc_votable;
	bool bpp_break;
	bool cp_regulator_done;
	int gpio_enable_val;
	int reverse_pen_soc;
	int reverse_vout;
	int reverse_iout;
	int hall3_online;
	int hall4_online;
};

void idtp922x_request_adapter(struct idtp9220_device_info *di);
static void idtp9220_set_charging_param(struct idtp9220_device_info *di);
int program_fw(struct idtp9220_device_info *di, u16 destAddr, u8 *src,
	       u32 size);
static int program_crc_verify(struct idtp9220_device_info *di);
static int program_bootloader(struct idtp9220_device_info *di);
static int idtp9220_set_rpp(struct idtp9220_device_info *di);
static int idtp9220_set_cep(struct idtp9220_device_info *di);
static int idtp9220_set_ept(struct idtp9220_device_info *di);
static u8 oob_check_sum(u8 *buf, u32 size);
static int idtp9220_set_reverse_enable(struct idtp9220_device_info *di,
				       int enable);
static void idt_get_reverse_soc(struct idtp9220_device_info *di);

/*static int idt_signal_strength = 0;
  module_param_named(ss, idt_signal_strength, int, 0600);
 */

static int idt_signal_range = 2;
static int idt_first_flag;
static int pen_soc_count = 0;

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
	di->pc_port_psy = power_supply_get_by_name("pc_port");
	if (!di->pc_port_psy) {
		dev_err(di->dev, "[idt] no pc_port_psy,return\n");
		return -EINVAL;
	}
	return 0;
}

int idtp9220_read(struct idtp9220_device_info *di, u16 reg, u8 *val)
{
	unsigned int temp;
	int rc;

	mutex_lock(&di->read_lock);
	rc = regmap_read(di->regmap, reg, &temp);
	if (rc >= 0)
		*val = (u8) temp;

	mutex_unlock(&di->read_lock);
	return rc;
}

int idtp9220_write(struct idtp9220_device_info *di, u16 reg, u8 val)
{
	int rc = 0;

	mutex_lock(&di->write_lock);
	rc = regmap_write(di->regmap, reg, val);
	if (rc < 0)
		dev_err(di->dev, "[idt] idtp9220 write error: %d\n", rc);

	mutex_unlock(&di->write_lock);
	return rc;
}

int idtp9220_masked_write(struct idtp9220_device_info *di, u16 reg, u8 mask,
			  u8 val)
{
	int rc = 0;

	mutex_lock(&di->write_lock);

	rc = regmap_update_bits(di->regmap, reg, mask, val);
	if (rc < 0)
		dev_err(di->dev, "[idt] idtp9220 write mask error: %d\n", rc);

	mutex_unlock(&di->write_lock);
	return rc;
}

int idtp9220_read_buffer(struct idtp9220_device_info *di, u16 reg, u8 *buf,
			 u32 size)
{
	int rc = 0;

	while (size--) {
		rc = di->bus.read(di, reg++, buf++);
		if (rc < 0) {
			dev_err(di->dev, "[idt] read buf error: %d\n", rc);
			return rc;
		}
	}

	return rc;
}

int idtp9220_write_buffer(struct idtp9220_device_info *di, u16 reg, u8 *buf,
			  u32 size)
{
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

u32 ExtractPacketSize(u8 hdr)
{
	if (hdr < 0x20)
		return 1;
	if (hdr < 0x80)
		return (2 + ((hdr - 0x20) >> 4));
	if (hdr < 0xe0)
		return (8 + ((hdr - 0x80) >> 3));
	return (20 + ((hdr - 0xe0) >> 2));
}

void idtp922x_clrInt(struct idtp9220_device_info *di, u8 *buf, u32 size)
{
	di->bus.write_buf(di, REG_SYS_INT_CLR, buf, size);
	di->bus.write(di, REG_SSCMND, CLRINT);
}

void reverse_clrInt(struct idtp9220_device_info *di, u8 *buf, u32 size)
{
	di->bus.write_buf(di, REG_SYS_INT_CLR, buf, size);
	di->bus.write(di, REG_TX_CMD, TX_FOD_EN | TX_CLRINT);
}

void idtp922x_enable_ext5v(struct idtp9220_device_info *di)
{
	dev_info(di->dev, "enable external 5v\n");
	di->bus.write(di, REG_EXTERNAL_5V, BIT(0));
}

void idtp922x_sendPkt(struct idtp9220_device_info *di, ProPkt_Type *pkt)
{
	u32 size = ExtractPacketSize(pkt->header) + 1;
	di->bus.write_buf(di, REG_PROPPKT, (u8 *) pkt, size);	// write data into proprietary packet buffer
	di->bus.write(di, REG_SSCMND, SENDPROPP);	// send proprietary packet

	dev_info(di->dev, "pkt header: 0x%x and cmd: 0x%x\n",
		 pkt->header, pkt->cmd);
	di->header = pkt->header;
	di->cmd = pkt->cmd;
}

void idtp922x_receivePkt(struct idtp9220_device_info *di, u8 *buf)
{
	u8 header;
	int rc;
	u32 size;

	di->bus.read(di, REG_BCHEADER, &header);
	size = ExtractPacketSize(header) + 1;
	rc = di->bus.read_buf(di, REG_BCDATA, buf, size);
	if (rc < 0)
		dev_err(di->dev, "[idt] read Tx data error: %d\n", rc);
}

void idtp922x_receivePkt2(struct idtp9220_device_info *di, u8 *buf)
{
	int rc;

	rc = di->bus.read_buf(di, REG_PROPPKT, buf, 2);
	if (rc < 0)
		dev_err(di->dev, "[idt] read Tx data error: %d\n", rc);
}


void idtp922x_set_adap_vol(struct idtp9220_device_info *di, u16 mv)
{
	dev_info(di->dev, "set adapter vol to %d\n", mv);
	di->bus.write(di, REG_FC_VOLTAGE_L, mv & 0xff);
	di->bus.write(di, REG_FC_VOLTAGE_H, (mv >> 8) & 0xff);
	di->bus.write(di, REG_SSCMND, VSWITCH);
}


void idtp922x_set_pmi_icl(struct idtp9220_device_info *di, int mA)
{
	union power_supply_propval val = { 0, };
	int rc;
	if (!di->power_good_flag && val.intval)
		return;

	rc = idtp9220_get_property_names(di);
	val.intval = mA;
	if (di->dt_props.wireless_by_usbin) {
		if (di->usb_psy)
			power_supply_set_property(di->usb_psy,
						  POWER_SUPPLY_PROP_CURRENT_MAX,
						  &val);
	} else {
		if (di->dc_psy)
			power_supply_set_property(di->dc_psy,
						  POWER_SUPPLY_PROP_CURRENT_MAX,
						  &val);

	}
	dev_info(di->dev, "[%s] set wls icl %d\n", __func__, val.intval);
}

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
	vout = vout_l | ((vout_h & 0xff) << 8);
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

	dev_info(di->dev, "vout regulator vout_l: 0x%x and vout_h: 0x%x\n",
		 vout_l, vout_h);
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

static void idtp9220_dump_regs(struct idtp9220_device_info *di)
{
	u8 VOut_l;
	u8 VOut_h;
	u8 VOutSet;
	u8 VrectAdc;
	u8 IOut_l;
	u8 IOut_h;
	u8 VrectTarget;

	di->bus.read(di, REG_ADC_VOUT_L, &VOut_l);
	di->bus.read(di, REG_ADC_VOUT_H, &VOut_h);
	di->bus.read(di, REG_VOUT_SET, &VOutSet);
	di->bus.read(di, REG_ADC_VRECT, &VrectAdc);
	di->bus.read(di, REG_RX_LOUT_L, &IOut_l);
	di->bus.read(di, REG_RX_LOUT_H, &IOut_h);
	di->bus.read(di, REG_VRECT_TAEGET, &VrectTarget);

	dev_info(di->dev,
		 "vout_l:%d, vout_h:%d, vout_set:%d, Vrect_adc:%d , iout_l:%d, iout_h:%d, Vrect_target:%d",
		 VOut_l, VOut_h, VOutSet, VrectAdc, IOut_l, IOut_h,
		 VrectTarget);
}
#endif

static int idtp9220_get_vout(struct idtp9220_device_info *di)
{
	u8 vout_l, vout_h;

	if (!di)
		return 0;

	di->bus.read(di, REG_ADC_VOUT_L, &vout_l);
	di->bus.read(di, REG_ADC_VOUT_H, &vout_h);
	di->vout = vout_l | ((vout_h & 0xf) << 8);
	di->vout = di->vout * 10 * 21 * 1000 / 40950 + ADJUST_METE_MV;	//vout = val/4095*10*2.1
	dev_info(di->dev, "%s: vout is %d\n", __func__, di->vout);
	return di->vout;
}

static void idtp9220_set_vout(struct idtp9220_device_info *di, int mv)
{
	u16 val;
	u8 vout_l, vout_h;
	if (!di)
		return;
	if (!di->power_good_flag)
		return;
	val = (mv - 2800) * 10 / 84;
	vout_l = val & 0xff;
	vout_h = val >> 8;
	di->bus.write(di, REG_VOUT_SET, vout_l);
	di->bus.write(di, REG_VRECT_ADJ, vout_h);
	dev_info(di->dev, "[idtp9220]: set vout voltage: %d %d\n", mv, val);
}

static void idt_set_reverse_fod(struct idtp9220_device_info *di, int mw)
{
	u8 mw_l, mw_h;
	if (!di)
		return;
	mw_l = mw & 0xff;
	mw_h = mw >> 8;
	di->bus.write(di, REG_FOD_LOW, mw_l);
	di->bus.write(di, REG_FOD_HIGH, mw_h);
	dev_info(di->dev, "set reverse fod: %d\n", mw);
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

#if 0
static int get_board_version(void)
{
	char boot[5] = { '\0' };
	char *match = (char *)strnstr(saved_command_line,
				      "androidboot.hwlevel=",
				      strlen(saved_command_line));
	if (match) {
		memcpy(boot, (match + strlen("androidboot.hwlevel=")),
		       sizeof(boot) - 1);
		printk("%s: hwlevel is %s\n", __func__, boot);
		if (!strncmp(boot, "P1.2", strlen("P1.2")))
			return 0;
		else if (!strncmp(boot, "P1.6", strlen("P1.6")))
			return 0;
	}
	return 1;
}
#endif
static int idtp9220_get_tx_id(struct idtp9220_device_info *di)
{
	if (di->tx_charger_type == ADAPTER_XIAOMI_PD_40W && di->is_zm_mobil_tx)
		return 1;
	else
		return 0;
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
	di->f = 64 * HSCLK / (data_list[0] | (data_list[1] << 8)) / 10;

	return di->f;
}

static int idtp9220_get_vrect(struct idtp9220_device_info *di)
{
	u8 data_list[2];

	if (!di->power_good_flag)
		return 0;
	di->bus.read_buf(di, REG_ADC_VRECT, data_list, 2);
	di->vrect = data_list[0] | ((data_list[1] & 0xf) << 8);
	di->vrect = di->vrect * 125 * 21 * 100 / 40950;	//vrect = val/4095*12.5*2.1

	return di->vrect;
}

static int idtp9220_get_power_max(struct idtp9220_device_info *di)
{
	int power_max;
	u8 val;

	di->bus.read(di, REG_POWER_MAX, &val);
	power_max = (val / 2) * 1000;
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

/*Factory Function*/

void idtp922x_sent_vout(struct idtp9220_device_info *di, u8 data_l, u8 data_h)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY38;
	pkt.cmd = BC_READ_VOUT;
	pkt.data[0] = data_l;
	pkt.data[1] = data_h;
	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_sent_iout(struct idtp9220_device_info *di, u8 data_l, u8 data_h)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY38;
	pkt.cmd = BC_READ_IOUT;
	pkt.data[0] = data_l;
	pkt.data[1] = data_h;
	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_request_low_addr(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY18;
	pkt.cmd = CMD_GET_BLEMAC_2_0;

	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_request_high_addr(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY18;
	pkt.cmd = CMD_GET_BLEMAC_5_3;

	idtp922x_sendPkt(di, &pkt);
}

static void idtp9220_test_vout_work(struct work_struct *work)
{

	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 get_vout_work.work);
	int vol = 0;
	u8 vout_l, vout_h;

	vol = idtp9220_get_vout(di);
	dev_info(di->dev, "[Factory test] RX VOUT %d mV \n", vol);
	vout_h = vol >> 8;
	vout_l = vol & 0xff;
	idtp922x_sent_vout(di, vout_l, vout_h);

}

static void idtp9220_test_iout_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 get_iout_work.work);
	int curr = 0;
	u8 cout_l, cout_h;

	curr = idtp9220_get_iout(di);
	dev_info(di->dev, " [Factory test] RX IOUT %d mA \n", curr);
	cout_h = curr >> 8;
	cout_l = curr & 0xff;
	idtp922x_sent_iout(di, cout_l, cout_h);
}

void idtp922x_sent_tx_mac(struct idtp9220_device_info *di)
{
	int64_t ble_mac = 0;
	union power_supply_propval val = { 0, };

	memcpy(&ble_mac, di->mac_data, MAC_LEN);
	val.int64val = ble_mac;
	if (di->wireless_psy)
		power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_TX_MAC, &val);
	else
		dev_err(di->dev, "BLE mac addr get error:\n");
}

void idtp922x_sent_pen_mac(struct idtp9220_device_info *di)
{
	int64_t ble_mac = 0;
	union power_supply_propval val = { 0, };

	memcpy(&ble_mac, di->pen_mac_data, MAC_LEN);
	val.int64val = ble_mac;
	if (di->wireless_psy)
		power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_PEN_MAC, &val);
	else
		dev_err(di->dev, "pen ble mac addr get error:\n");
}

void idtp922x_sent_chip_version(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	u8 chip_id_l, chip_id_h;

	di->bus.read(di, REG_CHIP_ID_L, &chip_id_l);
	di->bus.read(di, REG_CHIP_ID_H, &chip_id_h);

	dev_info(di->dev, "Chip_ID: %02x%02x\n", chip_id_h, chip_id_l);

	pkt.header = PROPRIETARY38;
	pkt.cmd = BC_RX_CHIP_VERSION;
	pkt.data[0] = chip_id_h;
	pkt.data[1] = chip_id_l;
	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_sent_fw_version(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	u8 fw_app_ver[4];

	di->bus.read_buf(di, REG_EPRFWVER_ADDR, fw_app_ver, 4);

	dev_info(di->dev, "RX FW version %x.%x.%x.%x\n",
		 fw_app_ver[3], fw_app_ver[2], fw_app_ver[1], fw_app_ver[0]);

	pkt.header = PROPRIETARY58;
	pkt.cmd = BC_RX_FW_VERSION;
	pkt.data[0] = fw_app_ver[3];
	pkt.data[1] = fw_app_ver[2];
	pkt.data[2] = fw_app_ver[1];
	pkt.data[3] = fw_app_ver[0];
	idtp922x_sendPkt(di, &pkt);
}

void idtp922x_sent_reverse_cnf(struct idtp9220_device_info *di)
{
	ProPkt_Type pkt;
	pkt.header = PROPRIETARY18;
	pkt.cmd = CMD_FACOTRY_REVERSE;
	idtp922x_sendPkt(di, &pkt);
}
/*Factory end*/

/*******************************************************
 * SET GPIO STATE TO SMB FOR IGNORE DC_POWER_ON IRQ*
 *******************************************************/
  /* define for reverse state of wireless charging */
#define REVERSE_GPIO_STATE_UNSET 0
#define REVERSE_GPIO_STATE_START 1
#define REVERSE_GPIO_STATE_END     2
static int idtp9220_set_reverse_gpio_state(struct idtp9220_device_info *di,
					   int enable)
{
	union power_supply_propval reverse_val = { 0, };

	if (!di->wireless_psy)
		di->wireless_psy = power_supply_get_by_name("wireless");

	if (di->wireless_psy) {
		dev_dbg(di->dev, "set_reverse_gpio_state\n",
			reverse_val.intval);
		if (enable) {
			reverse_val.intval = REVERSE_GPIO_STATE_START;
		} else {
			reverse_val.intval = REVERSE_GPIO_STATE_END;
		}

		power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_REVERSE_GPIO_STATE,
					  &reverse_val);

	} else {
		dev_err(di->dev, "no wireless_psy,return\n");
		return -EINVAL;
	}
	return 0;

}
static int idtp9229_set_reverse_boost_enable_gpio(struct idtp9220_device_info *di,
				     int enable)
{
   int ret;
   if (gpio_is_valid(di->dt_props.reverse_boost_enable_gpio)) {
	   ret = gpio_request(di->dt_props.reverse_boost_enable_gpio,
				  "reverse-boost-enable-gpio");
	   if (ret) {
		   dev_err(di->dev,
			   "%s: unable to reverse_boost_enable_gpio [%d]\n",
			   __func__, di->dt_props.reverse_boost_enable_gpio);
	   }

	   ret = gpio_direction_output(di->dt_props.reverse_boost_enable_gpio, !!enable);
	   if (ret) {
		   dev_err(di->dev,
			   "%s: cannot set direction for reverse_boost_enable_gpio  gpio [%d]\n",
			   __func__, di->dt_props.reverse_boost_enable_gpio);
	   }
	   gpio_free(di->dt_props.reverse_boost_enable_gpio);
   } else
	   dev_err(di->dev, "%s: unable to set reverse_boost_enable_gpio\n");

	return ret;
}

static int idtp9220_set_reverse_gpio(struct idtp9220_device_info *di,
				     int enable)
{
	int ret;
	union power_supply_propval val = { 0, };
	if (!di->wireless_psy)
		di->wireless_psy = power_supply_get_by_name("wireless");

	if (!di->wireless_psy) {
		dev_err(di->dev, "no wireless_psy,return\n");
		return -EINVAL;
	}

	val.intval = !!enable;

	if (gpio_is_valid(di->dt_props.reverse_gpio)) {

		if (enable) {
			idtp9220_set_reverse_gpio_state(di, enable);
			power_supply_set_property(di->wireless_psy,
						  POWER_SUPPLY_PROP_SW_DISABLE_DC_EN,
						  &val);
			idtp9229_set_reverse_boost_enable_gpio(di, enable);
		}
		ret = gpio_request(di->dt_props.reverse_gpio,
				   "reverse-enable-gpio");
		if (ret) {
			dev_err(di->dev,
				"%s: unable to request reverse gpio [%d]\n",
				__func__, di->dt_props.reverse_gpio);
		}

		ret = gpio_direction_output(di->dt_props.reverse_gpio, !!enable);
		if (ret) {
			dev_err(di->dev,
				"%s: cannot set direction for reverse enable gpio [%d]\n",
				__func__, di->dt_props.reverse_gpio);
		}
		gpio_free(di->dt_props.reverse_gpio);
		if (!enable) {
			idtp9229_set_reverse_boost_enable_gpio(di, enable);
			idtp9220_set_reverse_gpio_state(di, enable);
			power_supply_set_property(di->wireless_psy,
						  POWER_SUPPLY_PROP_SW_DISABLE_DC_EN,
						  &val);
		}

	} else
		dev_err(di->dev, "%s: unable to set tx_on\n");

	di->wireless_psy = power_supply_get_by_name("wireless");
	if (!di->wireless_psy)
		dev_err(di->dev, "[idt] no wireless_psy,return\n");
	else
		power_supply_changed(di->wireless_psy);
	return ret;
}

static ssize_t chip_version_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	u8 chip_id_l, chip_id_h, chip_rev, cust_id, status, vset;
	u8 fw_otp_ver[4], fw_app_ver[4];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	if (!di->fw_update) {
		di->fw_update = true;
		idtp9220_set_reverse_gpio(di, true);
		msleep(100);
		di->bus.read(di, REG_STATUS_L, &status);
		di->bus.read(di, REG_VOUT_SET, &vset);

		di->bus.read(di, REG_CHIP_ID_L, &chip_id_l);
		di->bus.read(di, REG_CHIP_ID_H, &chip_id_h);
		di->bus.read(di, REG_CHIP_REV, &chip_rev);
		chip_rev = chip_rev >> 4;
		di->bus.read(di, REG_CTM_ID, &cust_id);
		di->bus.read_buf(di, REG_OTPFWVER_ADDR, fw_otp_ver, 4);
		di->bus.read_buf(di, REG_EPRFWVER_ADDR, fw_app_ver, 4);

		if (!program_crc_verify(di)) {
			idtp9220_set_reverse_gpio(di, false);
			dev_err(di->dev, "crc verify failed, fw: %x\n", fw_app_ver[0]);
			fw_app_ver[0] = 0xFE;
			di->fw_update = false;
			return snprintf(buf, PAGE_SIZE, "chip_id_l:%02x\nchip_id_h:%02x\nchip_rev:%02x\ncust_id:%02x status:%02x vset:%02x\n otp_ver:%x.%x.%x.%x\n app_ver:%x.%x.%x.%x\n",
					chip_id_l, chip_id_h, chip_rev, cust_id, status, vset,
					fw_otp_ver[0], fw_otp_ver[1], fw_otp_ver[2], fw_otp_ver[3],
					fw_app_ver[0], fw_app_ver[1], fw_app_ver[2], fw_app_ver[3]);
		} else {
			dev_info(di->dev, "crc verify success.\n");
		}

		idtp9220_set_reverse_gpio(di, false);
		di->fw_update = false;
		return snprintf(buf, PAGE_SIZE,
			       "chip_id_l:%02x\nchip_id_h:%02x\nchip_rev:%02x\ncust_id:%02x status:%02x vset:%02x\n otp_ver:%x.%x.%x.%x\n app_ver:%x.%x.%x.%x\n",
			       chip_id_l, chip_id_h, chip_rev, cust_id, status,
			       vset, fw_otp_ver[0], fw_otp_ver[1],
			       fw_otp_ver[2], fw_otp_ver[3], fw_app_ver[0],
			       fw_app_ver[1], fw_app_ver[2], fw_app_ver[3]);
	} else {
		return snprintf(buf, PAGE_SIZE,
			       "fw update is ongoing ,can not get version\n");
	}
}

/* voltage limit attrs */
static ssize_t chip_vout_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	int vout;

	vout = idtp9220_get_vout(di);
	return snprintf(buf, PAGE_SIZE, "%d\n", vout);

}

static ssize_t chip_vout_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
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
				   struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	int vout;

	vout = idtp9220_get_vout_regulator(di);

	return snprintf(buf, PAGE_SIZE, "Vout ADC Value: %dMV\n", vout);
}

#define VOUT_MIN_4900_MV	4900
#define VOUT_MAX_10000_MV	10000
static ssize_t vout_regulator_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
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
			      struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	int cout;

	cout = idtp9220_get_iout(di);

	return snprintf(buf, PAGE_SIZE, "%d\n", cout);
}

static ssize_t chip_iout_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
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
			      struct device_attribute *attr, char *buf)
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

	dev_info(di->dev,
		 "%s:Vout:%dmV,Iout:%dmA,Freq:%dKHz,Vrect:%dmV,SS:%d\n",
		 __func__, di->vout, di->iout, di->f, di->vrect, di->ss);
}

static int idtp9220_reverse_charge_enable(struct idtp9220_device_info *di)
{
	u8 mode = 0;
	int ret = 0, i = 0;
	int fod_set = REVERSE_FOD;

	if (!di->fw_update) {
		di->bus.read(di, REG_WPC_MODE, &mode);
		dev_info(di->dev, "wpc mode(0x004D): 0x%x\n", mode);
		ret = mode & BIT(0);
		if (ret) {
			dev_info(di->dev,
				 "rx mode, can not enable reverse charge\n");
			di->is_reverse_mode = 0;
			di->is_reverse_chg = 1;
			idtp9220_set_reverse_enable(di, false);
			schedule_delayed_work(&di->reverse_sent_state_work, 0);
			return 0;
		}
	}

	if (!di->fw_update) {
		/* set reverse fod to REVERSE_FOD */
		idt_set_reverse_fod(di, fod_set);
		for (i = 0; i < 3; i++) {
			di->bus.write(di, REG_TX_CMD, TX_EN | TX_FOD_EN);
			msleep(50);
			di->bus.read(di, REG_TX_DATA, &mode);
			dev_info(di->dev, "tx data(0078): 0x%x\n", mode);
			if (mode & BIT(0)) {
				di->is_reverse_mode = 1;
				dev_info(di->dev, "start reverse charging\n");
				break;
			} else {
				dev_err(di->dev, "idt set reverse charge failed, retry: %d\n", i);
			}
		}

		if (i < 3) {
			dev_info(di->dev, "reverse charging start success\n");
			return 1;
		} else {
			dev_info(di->dev, "reverse charging failed start\n");
			idtp9220_set_reverse_enable(di, false);
			return 0;
		}
	} else {
		return 0;
	}
}

static int idtp9220_set_reverse_enable(struct idtp9220_device_info *di,
				       int enable)
{
	int ret = 0;
	union power_supply_propval wk_val = { 0, };

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
		goto out;
	}

	if (di->fw_update) {
		goto out;
	}
	idtp9220_set_reverse_gpio(di, enable);

	if (enable) {
		di->is_boost_mode = 1;
		ret = idtp9220_reverse_charge_enable(di);
		if (ret) {
			if (di->wireless_psy) {
				wk_val.intval = 1;
				power_supply_set_property(di->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							  &wk_val);
			}
			alarm_start_relative(&di->reverse_dping_alarm,
					     ms_to_ktime
					     (REVERSE_DPING_CHECK_DELAY_MS));
		}
	} else {
		di->is_boost_mode = 0;
		dev_info(di->dev, "disable reverse charging for wireless\n");
		if (di->wireless_psy) {
			wk_val.intval = 0;
			power_supply_set_property(di->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
						  &wk_val);
		}
		di->is_reverse_mode = 0;
		di->reverse_pen_soc = 255;
		cancel_delayed_work(&di->reverse_chg_state_work);
		cancel_delayed_work(&di->reverse_dping_state_work);
		cancel_delayed_work(&di->reverse_ept_type_work);
		cancel_delayed_work(&di->reverse_chg_work);

		ret = alarm_cancel(&di->reverse_dping_alarm);
		if (ret < 0)
			dev_err(di->dev,
				"Couldn't cancel reverse_dping_alarm\n");

		ret = alarm_cancel(&di->reverse_chg_alarm);
		if (ret < 0)
			dev_err(di->dev, "Couldn't cancel reverse_chg_alarm\n");
		pm_relax(di->dev);
	}
out:
	return ret;
}

/* reverse enable attrs */
static ssize_t reverse_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
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
				  const char *buf, size_t count)
{
	int ret, enable;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;

	idtp9220_set_reverse_gpio(di, enable);

	return count;
}

/*ovp cntl*/
static ssize_t ovp_gpio_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (gpio_is_valid(di->dt_props.rx_ovp_ctl_gpio))
		ret = gpio_get_value(di->dt_props.rx_ovp_ctl_gpio);
	else {
		dev_err(di->dev, "%s: ovp cntl gpio not provided\n", __func__);
		ret = -1;
	}

	dev_info(di->dev, "ovp enable gpio: %d\n", ret);

	return scnprintf(buf, PAGE_SIZE, "ovp enable: %d\n", ret);
}

static ssize_t ovp_gpio_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret, enable;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	ret = (int)simple_strtoul(buf, NULL, 10);
	enable = !!ret;

	if (gpio_is_valid(di->dt_props.rx_ovp_ctl_gpio)) {
		ret = gpio_request(di->dt_props.rx_ovp_ctl_gpio,
				   "ovp-enable-gpio");
		if (ret) {
			dev_err(di->dev,
				"%s: unable to request ovp_en_gpio [%d]\n",
				__func__, di->dt_props.rx_ovp_ctl_gpio);
		}

		ret =
		    gpio_direction_output(di->dt_props.rx_ovp_ctl_gpio, enable);
		if (ret) {
			dev_err(di->dev,
				"%s: cannot set direction for ovp_en_gpio [%d]\n",
				__func__, di->dt_props.rx_ovp_ctl_gpio);
		}
		gpio_free(di->dt_props.rx_ovp_ctl_gpio);
	} else
		dev_err(di->dev, "%s: unable to get ovp_en_gpio\n");

	return count;
}

static ssize_t reverse_gpio_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
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
				    const char *buf, size_t count)
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
				struct device_attribute *attr, char *buf)
{
	int ret = 0;

	return snprintf(buf, PAGE_SIZE, "Chip enable: %d\n", !ret);

/*
	if (gpio_is_valid(di->dt_props.enable_gpio))
		ret = gpio_get_value(di->dt_props.enable_gpio);
	else {
		dev_err(di->dev, "%s: sleep gpio not provided\n", __func__);
		ret = -1;
	}

	dev_info(di->dev, "chip enable gpio: %d\n", ret);

	return snprintf(buf, PAGE_SIZE, "Chip enable: %d\n", !ret);
*/
}

static ssize_t chip_die_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);
	u8 buff[2] = {0};
	int die_temp = 0;
	ret = di->bus.read_buf(di, REG_RX_DIE_TEMP, buff, 2);
	if (!ret) {
		die_temp = buff[0] | ((buff[1] & 0x0f) << 8);
		die_temp = (die_temp - 1350)*83/444 - 273;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", die_temp);
}

static enum alarmtimer_restart reverse_chg_alarm_cb(struct alarm *alarm,
						    ktime_t now)
{
	struct idtp9220_device_info *di =
	    container_of(alarm, struct idtp9220_device_info,
			 reverse_chg_alarm);

	dev_info(di->dev, " Reverse Chg Alarm Triggered %lld\n",
		 ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(di->dev);

	schedule_delayed_work(&di->reverse_chg_state_work, 0);

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart reverse_dping_alarm_cb(struct alarm *alarm,
						      ktime_t now)
{
	struct idtp9220_device_info *di =
	    container_of(alarm, struct idtp9220_device_info,
			 reverse_dping_alarm);

	dev_info(di->dev, "Reverse Dping Alarm Triggered %lld\n",
		 ktime_to_ms(now));

	/* Atomic context, cannot use voter */
	pm_stay_awake(di->dev);
	schedule_delayed_work(&di->reverse_dping_state_work, 0);

	return ALARMTIMER_NORESTART;
}

static enum alarmtimer_restart reverse_test_ready_alarm_cb(struct alarm *alarm,
							 ktime_t now)
{
  struct idtp9220_device_info *di = container_of(alarm, struct idtp9220_device_info,
						 reverse_test_ready_alarm);

  dev_info(di->dev, "[ factory reverse test ] reverse_rest_ready_alarm Triggered\n");

  /* Atomic context, cannot use voter */
  pm_stay_awake(di->dev);
  schedule_delayed_work(&di->reverse_test_state_work, 0);
  di->wait_for_reverse_test = false;

  return ALARMTIMER_NORESTART;
}

static int idtp9220_set_present(struct idtp9220_device_info *di, int enable)
{
	int ret = 0;

	dev_info(di->dev, "[idtp] dc plug %s\n", enable ? "in" : "out");
	if (enable) {
		di->dcin_present = true;
		di->ss = 1;
	} else {

		schedule_delayed_work(&di->oob_set_ept_work,
				      msecs_to_jiffies(10));
		di->status = NORMAL_MODE;
		di->count_9v = 0;
		di->count_5v = 0;
		di->exchange = 0;
		di->dcin_present = false;
		idt_signal_range = 2;
		di->ss = 2;
		di->epp = 0;
		di->last_vin = 0;
		di->last_icl = 0;
		di->is_car_tx = 0;
		di->is_voice_box_tx = 0;
		di->is_zm_20w_tx = 0;
		di->is_pan_tx = 0;
		di->is_ble_tx = 0;
		di->vout_on = 0;
		di->is_zm_mobil_tx = 0;
		di->power_off_mode = 0;
		di->is_epp_qc3 = 0;
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
		di->adapter_voltage = 0;
		di->first_rise_flag = false;
		di->enable_ext5v = false;
		di->vswitch_ok = false;
		di->bpp_icl = 0;
		di->otg_insert = 0;
		di->disable_cp = false;
		di->bpp_break = false;
		di->cp_regulator_done = false;
		cancel_delayed_work(&di->rx_vout_work);
		cancel_delayed_work(&di->chg_monitor_work);
		cancel_delayed_work(&di->cmd_check_work);
		cancel_delayed_work(&di->oob_set_cep_work);
		cancel_delayed_work(&di->rx_vout_cp_close_work);
		schedule_delayed_work(&di->oob_clean_work,
				      msecs_to_jiffies(100));
	}

	return ret;
}

static int idtp9220_set_enable_mode(struct idtp9220_device_info *di, int enable)
{
	int ret = 0;

	return ret;
/*
	printk("idtp9220_set_enable_mode: enable:%d\n", enable);
	if (gpio_is_valid(di->dt_props.enable_gpio)) {
		ret = gpio_request(di->dt_props.enable_gpio, "idt-enable-gpio");
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
		di->gpio_enable_val = gpio_get_value(di->dt_props.enable_gpio);
		pr_info("idtp9415 enable gpio val is :%d\n", di->gpio_enable_val);
		gpio_free(di->dt_props.enable_gpio);
	}
*/
}

static ssize_t chip_enable_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
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
			    struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int fw_ret = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (di->fw_update) {
		dev_err(&client->dev,
			"[idtp9415] [%s] Firmware Update is on going!\n",
			__func__);
		return snprintf(buf, PAGE_SIZE, "Firmware Update is on going!\n");
	}
	pm_stay_awake(di->dev);
	di->fw_update = true;

	idtp9220_set_reverse_gpio(di, true);
	msleep(100);

	if (!program_fw(di, 0x0000, idt_firmware, sizeof(idt_firmware))) {
		dev_err(&client->dev, "program fw failed.\n");
		fw_ret = 0;
	} else {
		dev_err(&client->dev, "program fw sucess.\n");
		di->fw_version = FW_VERSION;
		fw_ret = 1;
	}
	idtp9220_set_reverse_gpio(di, false);
	msleep(100);

	idtp9220_set_reverse_gpio(di, true);
	msleep(100);

	/* start crc verify */
	if (!program_crc_verify(di)) {
		dev_err(&client->dev, "crc verify failed.\n");
		ret = 0;
	} else {
		dev_err(&client->dev, "crc verify success.\n");
		ret = 1;
	}

	idtp9220_set_reverse_gpio(di, false);
	di->fw_update = false;
	pm_relax(di->dev);
	if (ret && fw_ret)
		return snprintf(buf, PAGE_SIZE, "Firmware Update:Success\n");
	else
		return snprintf(buf, PAGE_SIZE, "Firmware Update:Failed\n");
}

static DEVICE_ATTR(chip_enable, S_IWUSR | S_IRUGO, chip_enable_show,
		   chip_enable_store);
static DEVICE_ATTR(reverse_enable, S_IWUSR | S_IRUGO, reverse_enable_show,
		   reverse_enable_store);
static DEVICE_ATTR(chip_version, S_IRUGO, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IWUSR | S_IRUGO, chip_vout_show,
		   chip_vout_store);
static DEVICE_ATTR(chip_iout, S_IWUSR | S_IRUGO, chip_iout_show,
		   chip_iout_store);
static DEVICE_ATTR(chip_freq, S_IRUGO, chip_freq_show, NULL);
static DEVICE_ATTR(vout_regulator, S_IWUSR | S_IRUGO, vout_regulator_show,
		   vout_regulator_store);
static DEVICE_ATTR(chip_fw, S_IWUSR | S_IRUGO, chip_fw_show, NULL);
static DEVICE_ATTR(reverse_set_gpio, S_IWUSR | S_IRUGO, reverse_gpio_show,
		   reverse_gpio_store);
static DEVICE_ATTR(ovp_set_gpio, S_IWUSR | S_IRUGO, ovp_gpio_show,
		   ovp_gpio_store);
static DEVICE_ATTR(chip_die_temp, S_IWUSR | S_IRUGO, chip_die_temp_show, NULL);

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
	&dev_attr_ovp_set_gpio.attr,
	&dev_attr_chip_die_temp.attr,
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
		rc = di->bus.write(di, 0x0800 + i, bootloader_data[i]);
		if (rc)
			return rc;
	}

	return 0;
}

static int program_crc_verify(struct idtp9220_device_info *di)
{
	u8 val;
	int retry_cnt = 0;

	/* unlock system clock */
	if (di->bus.write(di, 0x3000, 0x5a))
		return false;
	/* configure clocks and timing */
	if (di->bus.write(di, 0x3004, 0x00))
		return false;
	if (di->bus.write(di, 0x3008, 0x09))
		return false;
	if (di->bus.write(di, 0x300C, 0x05))
		return false;
	if (di->bus.write(di, 0x300D, 0x1D))
		return false;
	if (di->bus.write(di, 0x304C, 0x00))
		return false;
	if (di->bus.write(di, 0x304D, 0x00))
		return false;
	/* pause the processor and enable MTP */
	if (di->bus.write(di, 0x3040, 0x11))
		return false;
	mdelay(10);
	/* halt the mpu */
	if (di->bus.write(di, 0x3040, 0x10))
		return false;
	mdelay(10);

	/* load the mtp downloader program */
	if (program_bootloader(di))
		return false;

	/* init the program structure and remap RAM */
	if (di->bus.write(di, 0x0400, 0x00))
		return false;
	if (di->bus.write(di, 0x3048, 0xD0))
		return false;
	/* reset mpu and run mtp downloader program, ignoreNAK */
	di->bus.write(di, 0x3040, 0x80);
	mdelay(100);

	/* configure MTP crc check utility */
	/* step1 load start address */
	if (di->bus.write(di, 0x402, 0x00))
		return false;
	if (di->bus.write(di, 0x403, 0x00))
		return false;
	/* step2 load mtp data size 0x6000 */
	if (di->bus.write(di, 0x404, 0x00))
		return false;
	if (di->bus.write(di, 0x405, 0x60))
		return false;
	/* step3 write crc valuve */
	if (di->bus.write(di, 0x406, CRC_VERIFY_LOW))
		return false;
	if (di->bus.write(di, 0x407, CRC_VERIFY_HIGH))
		return false;
	/* start crc check */
	if (di->bus.write(di, 0x400, 0x11)) {
		dev_err(di->dev, "on OTP buffer CRC validation error\n");
		return false;
	}

	do {
		mdelay(100);
		di->bus.read(di, 0x400, &val);
		if ((val & 1) != 0)
			dev_info(di->dev, "Programming crc check val:%02x\n",
				 val);
		if (retry_cnt++ > 50)
			break;
	} while ((val & 1) != 0);	//check if crc program finishes "OK"

	if (retry_cnt > 50) {
		dev_err(di->dev, "Programming CRC finished failed retry:%d\n",
			retry_cnt);
		return false;
	} else {
		di->bus.read(di, 0x401, &val);
		if (val != 2) {
			dev_err(di->dev, "crc check failed status:%d\n", val);
			return false;
		} else {
			dev_info(di->dev, "crc check success!\n");
			return true;
		}
	}
}

int program_fw(struct idtp9220_device_info *di, u16 destAddr, u8 *src,
	       u32 size)
{
	int i, j;
	u8 data = 0;
	//u8 data_array[512];

	//  === Step-1 ===
	// Transfer 9220 boot loader code "OTPBootloader" to 9220 SRAM
	// - Setup 9220 registers before transferring the boot loader code
	// - Transfer the boot loader code to 9220 SRAM
	// - Reset 9220 => 9220 M0 runs the boot loader
	//
	di->bus.read(di, 0x5870, &data);
	printk(KERN_EMERG "0x5870 %s:%d :%02x\n", __func__, __LINE__, data);
	di->bus.read(di, 0x5874, &data);
	printk(KERN_EMERG "0x5874 %s:%d :%02x\n", __func__, __LINE__, data);
	// configure the system
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
		di->bus.read(di, 0x1c00 + i, data_array[i]);
		if (data_array[i] != bootloader_data[i]) {
			printk(KERN_EMERG
			       "MAXUEYUE bootloader check err data[%d]:%02x != boot[%d]:%02x.\n",
			       i, data_array[i], i, bootloader_data[i]);
			return 1;
		}
		printk(KERN_EMERG "%02x ", data_array[i]);
		if (i + 1 % 16 == 0)
			printk(KERN_EMERG "\n");
	}
#endif
	//
	// === Step-2 ===
	// Program OTP image data to 9220 OTP memory
	//
	for (i = destAddr; i < destAddr + size; i += 128) {	// program pages of 128 bytes
		//
		// Build a packet
		//
		char sBuf[136];	// 136=8+128 --- 8-byte header plus 128-byte data
		u16 StartAddr = (u16) i;
		u16 CheckSum = StartAddr;
		u16 CodeLength = 128;
		int retry_cnt = 0;

		memset(sBuf, 0, 136);

		//(1) Copy the 128 bytes of the OTP image data to the packet data buffer
		//    Array.Copy(srcData, i + srcOffs, sBuf, 8, 128);// Copy 128 bytes from srcData (starting at i+srcOffs)
		memcpy(sBuf + 8, src, 128);	// Copy 128 bytes from srcData (starting at i+srcOffs)
		src += 128;
		// to sBuf (starting at 8)
		//srcData     --- source array
		//i + srcOffs     --- start index in source array
		//sBuf         --- destination array
		//8         --- start index in destination array
		//128         --- elements to copy

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
			CheckSum += sBuf[j + 8];	// add the nonzero values
		CheckSum += CodeLength;	// finish calculation of the check sum

		//(3) Fill up StartAddr, CodeLength, CheckSum of the current packet.
		memcpy(sBuf + 2, &StartAddr, 2);
		memcpy(sBuf + 4, &CodeLength, 2);
		memcpy(sBuf + 6, &CheckSum, 2);

		//
		// Send the current packet to 9220 SRAM via I2C
		//

		// read status is guaranteed to be != 1 at this point
		for (j = 0; j < CodeLength + 8; j++) {
			if (di->bus.write(di, 0x400 + j, sBuf[j])) {
				printk("on writing to OTP buffer error");
				return false;
			}
		}

		//
		// Write 1 to the Status in the SRAM. This informs the 9220 to start programming the new packet
		// from SRAM to OTP memory
		//
		if (di->bus.write(di, 0x400, 1)) {
			printk("on OTP buffer validation error");
			return false;
		}

		//
		// Wait for 9220 bootloader to complete programming the current packet image data from SRAM to the OTP.
		// The boot loader will update the Status in the SRAM as follows:
		//     Status:
		//     "0" - reset value (from AP)
		//     "1" - buffer validated / busy (from AP)
		//     "2" - finish "OK" (from the boot loader)
		//     "4" - programming error (from the boot loader)
		//     "8" - wrong check sum (from the boot loader)
		//     "16"- programming not possible (try to write "0" to bit location already programmed to "1")
		//         (from the boot loader)

		//        DateTime startT = DateTime.Now;
		do {
			mdelay(100);
			di->bus.read(di, 0x400, sBuf);
			if (sBuf[0] == 1) {
				printk
				    ("Programming OTP buffer status sBuf:%02x i:%d\n",
				     sBuf[0], i);
			}
			if (retry_cnt++ > 5)
				break;
		} while (sBuf[0] == 1);	//check if OTP programming finishes "OK"

		if (retry_cnt > 5) {
			printk("Programming OTP buffer retry failed :%d\n",
			       retry_cnt);
			return false;
		} else {
			di->bus.read(di, 0x401, sBuf);
			if (sBuf[0] != 2) {
				printk
				    ("buffer write to OTP returned status:%d :%s\n",
				     sBuf[0], "X4");
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

	// === Step-3 ===
	// Restore system (Need to reset or power cycle 9220 to run the OTP code)
	//
	if (di->bus.write(di, 0x3000, 0x5a))
		return false;	// write key
	if (di->bus.write(di, 0x3048, 0x00))
		return false;	// remove code remapping
	//if (!di->bus.write(0x3040, 0x80)) return false;    // reset M0
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

	di->dt_props.hall3_gpio = of_get_named_gpio(node, "hall,int3", 0);
	if ((!gpio_is_valid(di->dt_props.hall3_gpio)))
		return -EINVAL;

	di->dt_props.hall4_gpio = of_get_named_gpio(node, "hall,int4", 0);
	if ((!gpio_is_valid(di->dt_props.hall4_gpio)))
		return -EINVAL;

	di->dt_props.reverse_gpio =
	    of_get_named_gpio(node, "idt,reverse-enable", 0);
	if ((!gpio_is_valid(di->dt_props.reverse_gpio)))
		return -EINVAL;

	di->dt_props.wpc_det_gpio = of_get_named_gpio(node, "idt,wpc-det", 0);
	if ((!gpio_is_valid(di->dt_props.wpc_det_gpio)))
		return -EINVAL;
/*
	di->dt_props.enable_gpio = of_get_named_gpio(node, "idt,rx-sleep", 0);
	if ((!gpio_is_valid(di->dt_props.enable_gpio))) {
		dev_err(di->dev, "get enable_gpio fault\n");
		return -EINVAL;
	}
*/
	di->dt_props.reverse_boost_enable_gpio = of_get_named_gpio(node, "idt,reverse-booset-enable", 0);
	if ((!gpio_is_valid(di->dt_props.reverse_boost_enable_gpio))) {
		dev_err(di->dev, "get reverse_boost_enable_gpio fault\n");
		return -EINVAL;
	}

	di->dt_props.wireless_by_usbin = of_property_read_bool(node,
							       "mi,wireless-by-usbin");

	di->dt_props.only_idt_on_cmi = of_property_read_bool(node,
							     "mi,only-idt-on-cmi");

	di->dt_props.is_urd_device = of_property_read_bool(node,
							   "mi,urd-device");

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

	ret = pinctrl_select_state(di->idt_pinctrl, di->idt_gpio_active);
	if (ret < 0) {
		dev_err(di->dev, "fail to select pinctrl active rc=%d\n", ret);
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

	if (gpio_is_valid(di->dt_props.hall3_gpio)) {
		irqn = gpio_to_irq(di->dt_props.hall3_gpio);
		if (irqn < 0) {
			ret = irqn;
			goto err_irq_gpio;
		}
		di->hall3_irq = irqn;
	} else {
		dev_err(di->dev, "%s:hall3 irq gpio not provided\n", __func__);
		goto err_hall3_irq_gpio;
	}

	if (gpio_is_valid(di->dt_props.hall4_gpio)) {
		irqn = gpio_to_irq(di->dt_props.hall4_gpio);
		if (irqn < 0) {
			ret = irqn;
			goto err_irq_gpio;
		}
		di->hall4_irq = irqn;
	} else {
		dev_err(di->dev, "%s:hall4 irq gpio not provided\n", __func__);
		goto err_hall4_irq_gpio;
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

	if (ret)
		goto err_wpc_irq;

      err_wpc_irq:
	gpio_free(di->dt_props.wpc_det_gpio);
	err_hall4_irq_gpio:
	gpio_free(di->dt_props.hall4_gpio);
	err_hall3_irq_gpio:
	gpio_free(di->dt_props.hall3_gpio);
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
	int_val =
	    int_buf[0] | (int_buf[1] << 8) | (int_buf[2] << 16) | (int_buf[3] <<
								   24);
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

static bool reverse_need_irq_cleared(struct idtp9220_device_info *di, u32 val)
{
	u8 int_buf[4];
	u32 int_val;
	int rc = -1;

	rc = di->bus.read_buf(di, REG_SYS_INT, int_buf, 4);
	if (rc < 0) {
		dev_err(di->dev, "%s: read int state error\n", __func__);
		return false;
	}
	int_val =
	    int_buf[0] | (int_buf[1] << 8) | (int_buf[2] << 16) | (int_buf[3] <<
								   24);
	if (int_val && (int_val == val)) {
		dev_info(di->dev, "irq clear wrong, retry: 0x%08x\n", int_val);
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

	schedule_delayed_work(&di->chg_monitor_work, CHARGING_PERIOD_S * HZ);
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

	if (di->vout_on) {
		di->ss = 1;
		dev_info(di->dev, "dcin present, quit dc check work\n");
		return;
	} else {
		di->ss = 0;
		dev_info(di->dev, "dcin no present, continue dc check work\n");
		schedule_delayed_work(&di->dc_check_work,
				      msecs_to_jiffies(2500));
	}
}



static void idtp9220_chg_detect_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 chg_detect_work.work);

	union power_supply_propval val = { 0, };
	union power_supply_propval pc_val = { 0, };
	int rc;
	bool valid_typec_mode = false;

	dev_info(di->dev, "[idt] enter %s\n", __func__);

	rc = idtp9220_get_property_names(di);
	if (rc < 0)
		return;
	/*set idtp9220 into sleep mode when usbin */
	power_supply_get_property(di->usb_psy, POWER_SUPPLY_PROP_TYPEC_MODE, &val);
	power_supply_get_property(di->pc_port_psy,
				  POWER_SUPPLY_PROP_ONLINE, &pc_val);
	if (val.intval >= POWER_SUPPLY_TYPEC_SOURCE_DEFAULT && val.intval <= POWER_SUPPLY_TYPEC_SOURCE_HIGH)
		valid_typec_mode = true;

	if (valid_typec_mode || pc_val.intval) {
		dev_info(di->dev,
			 "typec_mode: %d or pc online: %d,set chip disable\n",
			 val.intval, pc_val.intval);
		//idtp9220_set_enable_mode(di, false);
		//schedule_delayed_work(&di->fw_download_work, 1 * HZ);
		return;
	}

	/* reset rx for bootup charging */
	idtp9220_set_enable_mode(di, false);
	msleep(10);
	idtp9220_set_enable_mode(di, true);

}

static void idtp9220_bpp_connect_load_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
			bpp_connect_load_work.work);
	dev_info(di->dev, "[idt] %s: \n", __func__);
	idtp922x_set_pmi_icl(di, BPP_DEFAULT_CURRENT / 3);
	if (di->bpp_break || !di->power_good_flag)
		return;
	msleep(300);
	idtp922x_set_pmi_icl(di, (BPP_DEFAULT_CURRENT / 3) * 2);
	if (di->bpp_break || !di->power_good_flag)
		return;
	msleep(300);
	idtp922x_set_pmi_icl(di, BPP_DEFAULT_CURRENT);
}

static void idtp9220_epp_connect_load_work(struct work_struct *work)
{

	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 epp_connect_load_work.work);
	int dc_load_curr = 0;

	if (di->power_max < 10000) {
		dc_load_curr = (di->power_max - 1000) / 11 * 1000;

	} else {
		dc_load_curr = EPP_DEFAULT_BYPASS_CURRENT;
	}

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
	u8 buf[2] = { 0 };
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
		schedule_delayed_work(&di->dc_check_work,
				      msecs_to_jiffies(8000));
	} else
		schedule_delayed_work(&di->dc_check_work,
				      msecs_to_jiffies(2500));
}

static int idt_get_effective_fcc(struct idtp9220_device_info *di)
{
	int effective_fcc_val = 0;

	if (!di->fcc_votable)
		di->fcc_votable = find_votable("FCC");

	if (!di->fcc_votable)
		return -EINVAL;

	effective_fcc_val = get_effective_result(di->fcc_votable);
	effective_fcc_val = effective_fcc_val / 1000;
	dev_info(di->dev, "effective_fcc: %d\n", effective_fcc_val);
	return effective_fcc_val;
}


#define VOICE_THERMAL_FCC_HOT_MA 500
#define VOICE_THERMAL_FCC_WARM_MA 1300
#define VOICE_TEMP_HOT_UP_LIMIT 402
#define VOICE_TEMP_HOT_DOWN_LIMIT 400
#define VOICE_TEMP_WARM_UP_LIMIT 375
#define VOICE_TEMP_WARM_DOWN_LIMIT 367
#define VOICE_MAX_ICL_UA  1500000
static void idt_voice_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				voice_tx_work.work);

	int soc = 0, batt_sts = 0, dc_level = 0, batt_temp = 0;
	int adapter_vol = ADAPTER_EPP_MI_VOL;
	int icl_curr = VOICE_MAX_ICL_UA;
	int effective_fcc = 0;
	bool vout_change = false;
	union power_supply_propval val = {0, };
	union power_supply_propval wk_val = {0, };
	int vout = 0;
	di->bpp_break = 1;
	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;

		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_DC_THERMAL_LEVELS, &val);
		dc_level = val.intval;

		power_supply_get_property(di->batt_psy,
				POWER_SUPPLY_PROP_TEMP, &val);
		batt_temp = val.intval;
	}

	dev_info(di->dev, "soc:%d, dc_level:%d, bat_status:%d, batt_temp: %d\n",
			soc, dc_level, batt_sts, batt_temp);

	switch (di->status) {
	case NORMAL_MODE:
		if (soc >= EPP_TAPER_SOC)
			di->status = TAPER_MODE;
		break;
	case TAPER_MODE:
		dev_info (di->dev, "[voice]taper mode set Vin 11V\n");
		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			di->status = FULL_MODE;
		else if (soc < 95)
			di->status = NORMAL_MODE;
		break;
	case FULL_MODE:
		dev_info (di->dev, "[voice]charge full set Vin 11V\n");
		adapter_vol = EPP_VOL_THRESHOLD;
		icl_curr = SCREEN_OFF_FUL_CURRENT;

		if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
			dev_info (di->dev, "[voice]full mode -> recharge mode\n");
			di->status = RECHG_MODE;
			icl_curr = DC_LOW_CURRENT;
		}
		break;
	case RECHG_MODE:
		if (batt_sts == POWER_SUPPLY_STATUS_FULL) {
			dev_info (di->dev, "[voice]recharge mode -> full mode\n");
			di->status = FULL_MODE;
			icl_curr = SCREEN_OFF_FUL_CURRENT;
			adapter_vol = EPP_VOL_THRESHOLD;
			if (di->wireless_psy) {
				wk_val.intval = 0;
				power_supply_set_property(di->wireless_psy,
						POWER_SUPPLY_PROP_WIRELESS_WAKELOCK, &wk_val);
			}
			break;
		}

		dev_info (di->dev, "[voice]recharge mode set icl to 350mA\n");
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
	effective_fcc = idt_get_effective_fcc(di);
/*
	if (batt_temp >= VOICE_TEMP_HOT_UP_LIMIT) {
		dev_info(di->dev, "[voice]Tbat limit fcc 1A\n");
		effective_fcc = idt_get_effective_fcc(di);
		if (di->fcc_votable) {
			effective_fcc = VOICE_THERMAL_FCC_HOT_MA;
			vote(di->fcc_votable, VOICE_LIMIT_FCC_1A_VOTER,
				true, effective_fcc * 1000);
		}
	} else if (batt_temp < VOICE_TEMP_HOT_DOWN_LIMIT) {
		if (di->fcc_votable)
			vote(di->fcc_votable, VOICE_LIMIT_FCC_1A_VOTER,
				false, 0);
	}

	if (batt_temp >= VOICE_TEMP_WARM_UP_LIMIT) {
		dev_info(di->dev, "[voice]Tbat limit fcc 3.2A\n");
		effective_fcc = idt_get_effective_fcc(di);
		if (di->fcc_votable) {
			effective_fcc = VOICE_THERMAL_FCC_WARM_MA;
			vote(di->fcc_votable, VOICE_LIMIT_FCC_VOTER,
				true, effective_fcc * 1000);
		}
	} else if (batt_temp < VOICE_TEMP_WARM_DOWN_LIMIT) {
		if (di->fcc_votable)
			vote(di->fcc_votable, VOICE_LIMIT_FCC_VOTER,
				false, 0);
	}
*/
	if (dc_level) {
		di->disable_cp = true;
		/* disable cp charger */
		if (di->wireless_psy) {
			val.intval = 0;
			power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
		}
		dev_info(di->dev, "[voice]Disable cp, dc_level:%d\n", dc_level);
	}

	if (di->cp_regulator_done)
			adapter_vol = EPP_VOL_THRESHOLD;

	if (!di->enable_ext5v) {
		di->enable_ext5v = true;
		idtp922x_enable_ext5v(di);
	}

	if (adapter_vol > 0 && adapter_vol != di->last_vin) {
		if ((adapter_vol == ADAPTER_EPP_MI_VOL)
			&& !di->first_rise_flag) {
			di->disable_cp = false;
			di->first_rise_flag = true;
			idtp922x_set_adap_vol(di, adapter_vol);
			di->vswitch_ok = false;
			msleep(110);
		} else if ((adapter_vol == ADAPTER_EPP_MI_VOL)
		 && di->first_rise_flag) {
			di->disable_cp = false;
			idtp9220_set_vout(di, adapter_vol);
			msleep(110);
			schedule_delayed_work(&di->load_fod_param_work,
					msecs_to_jiffies(500));
			schedule_delayed_work(&di->vout_regulator_work,
					msecs_to_jiffies(400));
		} else if (adapter_vol == EPP_VOL_THRESHOLD) {

			vout = idtp9220_get_vout(di);
			while (vout > EPP_VOL_THRESHOLD) {
				vout = vout - 1000;
				idtp9220_set_vout(di, vout);
				msleep(200);
			}
			idtp9220_set_vout(di, EPP_VOL_THRESHOLD);
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

	dev_info(di->dev, "di->status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d, bq_dis:%d\n",
			di->status, adapter_vol, icl_curr, di->last_vin, di->last_icl, di->disable_cp);

	return;
}


#define PAN_BBC_ICL_MA 1300000
static void idt_pan_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
			 pan_tx_work.work);

	int soc = 0, batt_sts = 0, dc_level = 0;
	int adapter_vol = ADAPTER_EPP_MI_VOL;
	int icl_curr = DC_PAN_CURRENT_UA;
	bool vout_change = false;
	union power_supply_propval val = { 0, };
	union power_supply_propval wk_val = { 0, };
	int vout = 0;
	di->bpp_break = 1;
	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
					  POWER_SUPPLY_PROP_STATUS, &val);
		batt_sts = val.intval;

		power_supply_get_property(di->batt_psy,
					  POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;

		power_supply_get_property(di->batt_psy,
					  POWER_SUPPLY_PROP_DC_THERMAL_LEVELS,
					  &val);
		dc_level = val.intval;
	}

	dev_info(di->dev, "soc:%d, dc_level:%d, bat_status:%d\n",
		 soc, dc_level, batt_sts);

	switch (di->status) {
	case NORMAL_MODE:
		if (soc >= FULL_SOC)
			di->status = TAPER_MODE;
		break;
	case TAPER_MODE:
		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			di->status = FULL_MODE;
		else if (soc < FULL_SOC - 1)
			di->status = NORMAL_MODE;
		break;
	case FULL_MODE:
		dev_info(di->dev, "[pan]charge full set Vin 12V\n");
		adapter_vol = EPP_VOL_THRESHOLD;
		icl_curr = SCREEN_OFF_FUL_CURRENT;

		if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
			dev_info(di->dev, "[pan]full mode -> recharge mode\n");
			di->status = RECHG_MODE;
			icl_curr = DC_LOW_CURRENT;
		}
		break;
	case RECHG_MODE:
		if (batt_sts == POWER_SUPPLY_STATUS_FULL) {
			dev_info(di->dev, "recharge mode -> full mode\n");
			di->status = FULL_MODE;
			icl_curr = SCREEN_OFF_FUL_CURRENT;
			adapter_vol = EPP_VOL_THRESHOLD;
			if (di->wireless_psy) {
				wk_val.intval = 0;
				power_supply_set_property(di->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							  &wk_val);
			}
			break;
		}

		dev_info(di->dev, "recharge mode set icl to 350mA\n");
		adapter_vol = EPP_VOL_THRESHOLD;
		icl_curr = DC_LOW_CURRENT;

		if (di->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(di->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
						  &wk_val);
		}
		break;
	default:
		break;
	}

	if (dc_level) {
		dev_info(di->dev, "Disable bq, dc_level:%d\n", dc_level);
		di->disable_cp = true;
		if (di->wireless_psy) {
			val.intval = 0;
			power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
		}

		if (di->cp_regulator_done) {
			adapter_vol = EPP_VOL_THRESHOLD;
			icl_curr = PAN_BBC_MAX_ICL_MA;
		}
	}

	if (!di->enable_ext5v) {
		di->enable_ext5v = true;
		idtp922x_enable_ext5v(di);
	}

	if (adapter_vol > 0 && adapter_vol != di->last_vin) {
		if ((adapter_vol == ADAPTER_EPP_MI_VOL)
			&& !di->first_rise_flag) {
			di->disable_cp = false;
			di->first_rise_flag = true;
			idtp922x_set_adap_vol(di, adapter_vol);
			di->vswitch_ok = false;
			msleep(110);
		} else if ((adapter_vol == ADAPTER_EPP_MI_VOL)
			   && di->first_rise_flag) {
			di->disable_cp = false;
			idtp9220_set_vout(di, adapter_vol);
			msleep(110);
			schedule_delayed_work(&di->load_fod_param_work,
						  msecs_to_jiffies(500));
			schedule_delayed_work(&di->vout_regulator_work,
						  msecs_to_jiffies(400));
		} else if (adapter_vol == EPP_VOL_THRESHOLD) {

			vout = idtp9220_get_vout(di);
			while (vout > EPP_VOL_THRESHOLD) {
				vout = vout - 1000;
				idtp9220_set_vout(di, vout);
				msleep(200);
			}
			idtp9220_set_vout(di, EPP_VOL_THRESHOLD);
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

	dev_info(di->dev,
		 "di->status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d, bq_dis:%d\n",
		 di->status, adapter_vol, icl_curr, di->last_vin, di->last_icl,
		 di->disable_cp);

	return;
}


static void idtp9220_bpp_e5_tx_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 bpp_e5_tx_work.work);
	int icl_curr = BPP_QC_7W_CURRENT, adapter_vol = ADAPTER_BPP_QC_VOL;
	int soc = 0, batt_sts = 0, health = 0;
	int icl_setted = 0;
	union power_supply_propval val = { 0, };
	union power_supply_propval wk_val = { 0, };

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
	    (di->exchange == EXCHANGE_5V
	     && di->count_9v <= ICL_EXCHANGE_COUNT - 3)) {
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
		if (soc >= EPP_TAPER_SOC) {
			di->status = TAPER_MODE;
			adapter_vol = ADAPTER_DEFAULT_VOL;
			icl_curr = min(DC_SDP_CURRENT, icl_curr);
		}
		break;
	case TAPER_MODE:
		dev_info(di->dev, "[bpp] taper mode set vout to 5.5v\n");
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = min(DC_SDP_CURRENT, icl_curr);

		if (soc == FULL_SOC && batt_sts == POWER_SUPPLY_STATUS_FULL)
			di->status = FULL_MODE;
		else if (soc < EPP_TAPER_SOC - 1)
			di->status = NORMAL_MODE;
		break;
	case FULL_MODE:
		dev_info(di->dev, "[bpp] charge full set vout to 5.5v\n");
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = SCREEN_OFF_FUL_CURRENT;

		if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
			di->status = RECHG_MODE;
			icl_curr = DC_LOW_CURRENT;
		}
		break;
	case RECHG_MODE:
		dev_info(di->dev, "[bpp] recharge mode set icl to 360mA\n");
		adapter_vol = ADAPTER_DEFAULT_VOL;
		icl_curr = DC_LOW_CURRENT;

		if (soc < EPP_TAPER_SOC - 1)
			di->status = NORMAL_MODE;
		else if (batt_sts == POWER_SUPPLY_STATUS_FULL)
			di->status = FULL_MODE;

		if (di->wireless_psy) {
			wk_val.intval = 1;
			power_supply_set_property(di->wireless_psy,
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
		di->bpp_break = true;
		cancel_delayed_work_sync(&di->bpp_connect_load_work);
		if ((adapter_vol == ADAPTER_BPP_QC_VOL) && (!di->bpp_vout_rise)) {
			dev_info(di->dev,
				 "bpp_10w, vout lower than 8v, set 500mA\n");
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
	union power_supply_propval val = { 0, };

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
		dev_info(di->dev,
			 "qc2+f1_tx, full mode set vout to 8V,icl to 250mA\n");
		dc_icl = SCREEN_OFF_FUL_CURRENT;
	}

	if (vout != di->last_qc2_vout) {
		dev_info(di->dev,
			 "qc2+f1_tx, set new vout: %d, last_vout: %d\n", vout,
			 di->last_qc2_vout);
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
	union power_supply_propval val = { 0, };

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

	union power_supply_propval val = { 0, };
	int ret;
	int vinc = 0;
	int soc = 0;
	int dc_level = 0;
	int need_cp = true;
	printk("idtp9220_vout_regulator_work\n");

	if (di->batt_psy) {
		power_supply_get_property(di->batt_psy,
					  POWER_SUPPLY_PROP_CAPACITY, &val);
		soc = val.intval;

		power_supply_get_property(di->batt_psy,
					POWER_SUPPLY_PROP_DC_THERMAL_LEVELS, &val);
		dc_level = val.intval;
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
	/* for pan and voice tx, do not open cp while thermal high */
	if ((di->is_pan_tx || di->is_voice_box_tx) && dc_level) {
		need_cp = false;
	}

	if (di->epp && vinc && soc < TAPER_SOC - 1 && need_cp) {
		dev_info(di->dev, "%s: open charge pump by wireless\n",
			 __func__);
		ret = idtp9220_get_property_names(di);
		if (di->wireless_psy) {
			val.intval = 1;
			power_supply_set_property(di->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
						  &val);
		} else {
			dev_err(di->dev, "[idt] no wireless psy\n");
		}
	} else {
		dev_info(di->dev, "%s: close charge pump by wireless\n",
			 __func__);
		if (di->wireless_psy) {
			val.intval = 0;
			power_supply_set_property(di->wireless_psy,
						  POWER_SUPPLY_PROP_WIRELESS_CP_EN,
						  &val);
		}
		// set vout to 14.2v
		msleep(200);
		idtp9220_set_vout(di, CP_CLOSE_VOUT_MV);
	}

      out:
	dev_info(di->dev, "%s: epp=%d vout= %d\n", __func__, di->epp, di->vout);
	return;
}

static int fod_over_write(struct idtp9220_device_info *di, int mode)
{
	int ret = 0;

	switch (mode) {
	case EPP_PLUS_VOL:
		if (di->is_zm_20w_tx) {
			di->bus.write(di, 0x68, 0x98);
			di->bus.write(di, 0x69, 0x46);
			di->bus.write(di, 0x6A, 0x98);
			di->bus.write(di, 0x6B, 0x46);
			di->bus.write(di, 0x6C, 0x87);
			di->bus.write(di, 0x6D, 0x69);
			di->bus.write(di, 0x6E, 0x87);
			di->bus.write(di, 0x6F, 0x69);
			di->bus.write(di, 0x70, 0x95);
			di->bus.write(di, 0x71, 0x1b);
			di->bus.write(di, 0x72, 0x87);
			di->bus.write(di, 0x73, 0x7D);
		} else if (!di->is_f1_tx) {
			di->bus.write(di, 0x68, 0x98);
			di->bus.write(di, 0x69, 0x46);
			di->bus.write(di, 0x6A, 0x98);
			di->bus.write(di, 0x6B, 0x46);
			di->bus.write(di, 0x6C, 0x87);
			di->bus.write(di, 0x6D, 0x69);
			di->bus.write(di, 0x6E, 0x87);
			di->bus.write(di, 0x6F, 0x69);
			di->bus.write(di, 0x70, 0x89);
			di->bus.write(di, 0x71, 0x1b);
			di->bus.write(di, 0x72, 0x87);
			di->bus.write(di, 0x73, 0x7D);
		} else {
			di->bus.write(di, 0x68, 0x82);
			di->bus.write(di, 0x69, 0x78);
			di->bus.write(di, 0x6A, 0x82);
			di->bus.write(di, 0x6B, 0x78);
			di->bus.write(di, 0x6C, 0x80);
			di->bus.write(di, 0x6D, 0x7d);
			di->bus.write(di, 0x6E, 0x80);
			di->bus.write(di, 0x6F, 0x7d);
			di->bus.write(di, 0x70, 0x89);
			di->bus.write(di, 0x71, 0x43);
			di->bus.write(di, 0x72, 0x88);
			di->bus.write(di, 0x73, 0x7f);
		}
		ret = 1;
		break;
	case EPP_BASE_VOL:
		di->bus.write(di, 0x68, 0x93);
		di->bus.write(di, 0x69, 0x44);
		di->bus.write(di, 0x6A, 0x93);
		di->bus.write(di, 0x6B, 0x44);
		di->bus.write(di, 0x6C, 0x96);
		di->bus.write(di, 0x6D, 0x17);
		di->bus.write(di, 0x6E, 0x96);
		di->bus.write(di, 0x6F, 0x17);
		di->bus.write(di, 0x70, 0x95);
		di->bus.write(di, 0x71, 0x16);
		di->bus.write(di, 0x72, 0x91);
		di->bus.write(di, 0x73, 0x39);
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
		di->bus.write(di, 0x68, 0x9C);
		di->bus.write(di, 0x69, 0x29);
		di->bus.write(di, 0x6A, 0x57);
		di->bus.write(di, 0x6B, 0x7B);
		di->bus.write(di, 0x6C, 0x57);
		di->bus.write(di, 0x6D, 0x7B);
		di->bus.write(di, 0x6E, 0x96);
		di->bus.write(di, 0x6F, 0x00);
		di->bus.write(di, 0x70, 0x93);
		di->bus.write(di, 0x71, 0x0A);
		di->bus.write(di, 0x72, 0x93);
		di->bus.write(di, 0x73, 0x0A);
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
		di->bus.write(di, 0x68, 0x9C);
		di->bus.write(di, 0x69, 0x29);
		di->bus.write(di, 0x6A, 0x57);
		di->bus.write(di, 0x6B, 0x7B);
		di->bus.write(di, 0x6C, 0x57);
		di->bus.write(di, 0x6D, 0x7B);
		di->bus.write(di, 0x6E, 0x9E);
		di->bus.write(di, 0x6F, 0x08);
		di->bus.write(di, 0x70, 0x9D);
		di->bus.write(di, 0x71, 0x05);
		di->bus.write(di, 0x72, 0x96);
		di->bus.write(di, 0x73, 0x28);
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
		if (data_l == 0x97 && data_h == 0xD1)
			ret = 1;
		else
			ret = 0;
		break;
	case EPP_BASE_VOL:
		if (data_l == 0x91 && data_h == 0x39)
			ret = 1;
		else
			ret = 0;
		break;
	case BPP_PLUS_VOL:
		if (data_l == 0x93 && data_h == 0x14)
			ret = 1;
		else
			ret = 0;
		break;
	case FULL_VOL:
		if (data_l == 0x96 && data_h == 0x28)
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
	union power_supply_propval val = { 0, };

	if (di->wireless_psy) {
		if (di->is_reverse_chg == 4)
			idt_get_reverse_soc(di);
		val.intval = di->is_reverse_chg;
		power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_REVERSE_PEN_CHG_STATE,
					  &val);
		power_supply_changed(di->wireless_psy);
		dev_info(di->dev, "uevent: %d for reverse charging state\n",
			 di->is_reverse_chg);
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
	mutex_unlock(&di->reverse_op_lock);
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
	mutex_unlock(&di->reverse_op_lock);
	schedule_delayed_work(&di->reverse_sent_state_work, 0);

	return;
}
static void reverse_test_state_set_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 reverse_test_state_work.work);

	dev_info(di->dev, "[ factory reverse test ]tx mode fault and disable reverse charging\n");

	mutex_lock(&di->reverse_op_lock);
	idtp9220_set_reverse_enable(di, false);
	di->is_reverse_mode = 0;
	di->is_reverse_chg = 2;
	mutex_unlock(&di->reverse_op_lock);
	schedule_delayed_work(&di->reverse_sent_state_work, 0);
	pm_relax(di->dev);
	return;
}

static void reverse_ept_type_get_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 reverse_ept_type_work.work);
	int rc;
	u8 buf[2] = { 0 };
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
			    (ept_val & EPT_OTP) || (ept_val & EPT_POCP)) {
				dev_info(di->dev,
					 "TX mode in ept and disable reverse charging\n");
				idtp9220_set_reverse_enable(di, false);
				di->is_reverse_mode = 0;
				di->is_reverse_chg = 2;
				schedule_delayed_work(&di->
						      reverse_sent_state_work,
						      0);
			} else if (ept_val & EPT_CEP_TIMEOUT) {
				dev_info(di->dev, "recheck ping state\n");
				di->is_reverse_chg = 5;
				schedule_delayed_work(&di->reverse_sent_state_work, 0);
			}
		}
	}

	return;
}

static void idtp9220_load_fod_param_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 load_fod_param_work.work);
	int fod_param = 0;
	int ret = 0, val = 0;

	if (!di->vout)
		idtp9220_get_vout(di);

	if (di->vout > EPP_VOL_LIM_THRESHOLD)
		fod_param = EPP_PLUS_VOL;
	else if (di->vout > BPP_VOL_LIM_THRESHOLD
		 && di->vout < EPP_VOL_LIM_THRESHOLD)
		fod_param = EPP_BASE_VOL;
	else if (di->vout > BPP_VOL_THRESHOLD
		 && di->vout < BPP_VOL_LIM_THRESHOLD)
		fod_param = BPP_PLUS_VOL;
	else if (di->vout > 0 && di->vout < BPP_VOL_THRESHOLD)
		fod_param = FULL_VOL;

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

static int idtp9220_get_charging_current(struct idtp9220_device_info *di)
{
	switch (di->tx_charger_type) {
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
		return ICL_CP_2SBATT_20W_MA;
	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_VOICE_BOX:
		return ICL_CP_2SBATT_30W_MA;
	case ADAPTER_XIAOMI_PD_45W:
	case ADAPTER_XIAOMI_PD_60W:
		return ICL_CP_2SBATT_40W_MA;
	default:
		break;
	}
	return DC_QC3_CURRENT;
}
static int idtp9220_get_taper_charging_current(struct idtp9220_device_info *di)
{
	switch (di->tx_charger_type) {
	case ADAPTER_XIAOMI_QC3:
	case ADAPTER_XIAOMI_PD:
	case ADAPTER_ZIMI_CAR_POWER:
		return ICL_TAPER_2SBATT_20W_MA;
	case ADAPTER_XIAOMI_PD_40W:
	case ADAPTER_VOICE_BOX:
		return ICL_TAPER_2SBATT_30W_MA;
	case ADAPTER_XIAOMI_PD_45W:
	case ADAPTER_XIAOMI_PD_60W:
		return ICL_TAPER_2SBATT_40W_MA;
	default:
		break;
	}
	return DC_QC3_CURRENT;
}

static void idtp9220_set_charging_param(struct idtp9220_device_info *di)
{
	int soc = 0, health = 0, batt_sts = 0;
	int adapter_vol = 0, icl_curr = 0;
	int cur_now = 0, vol_now = 0, vin_inc = 0;
	bool vout_change = false;
	union power_supply_propval val = { 0, };
	union power_supply_propval wk_val = { 0, };
	int vout = 0;

	switch (di->tx_charger_type) {
	case ADAPTER_QC2:
		adapter_vol = ADAPTER_BPP_QC2_VOL;	//6.5V
		di->bpp_break = 1;
		icl_curr = DC_QC2_CURRENT;	//1A
		break;
	case ADAPTER_QC3:
		if (!di->epp) {
			adapter_vol = ADAPTER_BPP_QC_VOL;
			icl_curr = DC_QC3_CURRENT;	//1A
		} else {
			di->is_epp_qc3 = 1;
			adapter_vol = ADAPTER_EPP_QC3_VOL;	//11V
			icl_curr = DC_EPP_QC3_CURRENT;
		}
		break;
	case ADAPTER_PD:
		if (di->epp) {
			adapter_vol = ADAPTER_EPP_QC3_VOL;
			icl_curr = DC_EPP_QC3_CURRENT;
		} else {
			adapter_vol = ADAPTER_DEFAULT_VOL;	//6V
			icl_curr = DC_PD_CURRENT;	//800mA
		}
		break;
	case ADAPTER_AUTH_FAILED:
		if (di->epp) {
			adapter_vol = ADAPTER_EPP_QC3_VOL;
			if (di->power_max < 10000) {
				icl_curr = ((di->power_max - 1000) / 11) * 1000;
			} else {
				icl_curr = EPP_DEFAULT_BYPASS_CURRENT;
			}
		} else {
			adapter_vol = ADAPTER_DEFAULT_VOL;	//6V
			icl_curr = di->bpp_icl;
		}
		break;
	case ADAPTER_DCP:
	case ADAPTER_CDP:
		adapter_vol = ADAPTER_DEFAULT_VOL;	//6V
		if (di->is_compatible_hwid)
			icl_curr = DC_BPP_AUTH_FAIL_CURRENT;	//860mA
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
	case ADAPTER_VOICE_BOX:
	case ADAPTER_XIAOMI_PD_45W:
	case ADAPTER_XIAOMI_PD_60W:
		if (di->epp) {
			adapter_vol = ADAPTER_EPP_MI_VOL;
			icl_curr = idtp9220_get_charging_current(di);
		} else {
			adapter_vol = ADAPTER_BPP_QC_VOL;
			icl_curr = DC_QC3_CURRENT;
		}
		break;
	default:
		icl_curr = 0;
		break;
	}

	power_supply_get_property(di->batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
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

	power_supply_get_property(di->batt_psy, POWER_SUPPLY_PROP_HEALTH, &val);
	health = val.intval;

	idtp9220_get_iout(di);

	if ((batt_sts == POWER_SUPPLY_STATUS_DISCHARGING)
	    && di->dcin_present && di->power_good_flag) {
		dev_info(di->dev,
			 "discharge when dc and pwr present, reset chip\n");
		return;
	}
	dev_info(di->dev,
		 "[idtp] soc:%d,vol_now:%d,cur_now:%d,health:%d, bat_status:%d\n",
		 soc, vol_now, cur_now, health, batt_sts);

	if (adapter_vol == ADAPTER_BPP_QC_VOL && di->is_compatible_hwid) {
		schedule_delayed_work(&di->bpp_e5_tx_work, msecs_to_jiffies(0));
		goto out;
	}

	if (di->tx_charger_type == ADAPTER_QC2 && di->is_f1_tx) {
		schedule_delayed_work(&di->qc2_f1_tx_work, msecs_to_jiffies(0));
		goto out;
	}

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

	if (adapter_vol == ADAPTER_EPP_MI_VOL && di->is_pan_tx) {
		schedule_delayed_work(&di->pan_tx_work, msecs_to_jiffies(0));
		goto out;
	}
	if (adapter_vol == ADAPTER_EPP_MI_VOL && di->is_voice_box_tx) {
		dev_info(di->dev, "voice box logic\n");
		schedule_delayed_work(&di->voice_tx_work, msecs_to_jiffies(0));
		goto out;
	}
	if (adapter_vol == ADAPTER_EPP_MI_VOL && di->vswitch_ok) {
		switch (di->status) {
		case NORMAL_MODE:
			if (soc >= EPP_TAPER_SOC) {
				if (di->wireless_psy) {
					power_supply_get_property(di->wireless_psy,
					POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
					if (!val.intval) {
						icl_curr = idtp9220_get_taper_charging_current(di);
					}
				} else {
					icl_curr = idtp9220_get_taper_charging_current(di);
				}
				if (di->wireless_psy) {
					val.intval = 0;
					power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_WIRELESS_CP_EN, &val);
				}
				icl_curr = min(icl_curr, MAX_PMIC_ICL_UA);
			}
			if (soc >= FULL_SOC) {
				di->status = TAPER_MODE;
			}
			break;
		case TAPER_MODE:
			if (soc == FULL_SOC
			    && batt_sts == POWER_SUPPLY_STATUS_FULL)
				di->status = FULL_MODE;
			else if (soc < FULL_SOC - 1)
				di->status = NORMAL_MODE;
			break;
		case FULL_MODE:
			dev_info(di->dev, "charge full set Vin 12V\n");
			adapter_vol = EPP_VOL_THRESHOLD;
			icl_curr = SCREEN_OFF_FUL_CURRENT;

			if (batt_sts == POWER_SUPPLY_STATUS_CHARGING) {
				dev_info(di->dev,
					 "full mode -> recharge mode\n");
				di->status = RECHG_MODE;
				icl_curr = DC_LOW_CURRENT;
			}
			break;
		case RECHG_MODE:
			if (batt_sts == POWER_SUPPLY_STATUS_FULL) {
				dev_info(di->dev,
					 "recharge mode -> full mode\n");
				di->status = FULL_MODE;
				icl_curr = SCREEN_OFF_FUL_CURRENT;
				adapter_vol = EPP_VOL_THRESHOLD;
				if (di->wireless_psy) {
					wk_val.intval = 0;
					power_supply_set_property(di->wireless_psy,
								  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
								  &wk_val);
				}
				break;
			}

			dev_info(di->dev, "recharge mode set icl to 350mA\n");
			adapter_vol = EPP_VOL_THRESHOLD;
			icl_curr = DC_LOW_CURRENT;

			if (di->wireless_psy) {
				wk_val.intval = 1;
				power_supply_set_property(di->wireless_psy,
							  POWER_SUPPLY_PROP_WIRELESS_WAKELOCK,
							  &wk_val);
			}
			break;
		default:
			break;
		}
	}

	if (!di->enable_ext5v) {
		di->enable_ext5v = true;
		idtp922x_enable_ext5v(di);
	}
	if (!di->power_good_flag)
		return;
	dev_info(di->dev, "%s: ready to set_vin vol\n", __func__);
	if (adapter_vol > 0 && adapter_vol != di->last_vin) {
		if ((adapter_vol == ADAPTER_EPP_MI_VOL)
		    && !di->first_rise_flag) {
			di->first_rise_flag = true;
			idtp922x_set_adap_vol(di, adapter_vol);
			di->vswitch_ok = false;
			vin_inc = 1;
			msleep(110);
			dev_info(di->dev, "%s:set adapter_vol %d\n", __func__,
				 adapter_vol);
		} else if ((adapter_vol == ADAPTER_EPP_MI_VOL)
			   && di->first_rise_flag) {
			idtp9220_set_vout(di, adapter_vol);
			msleep(110);
			schedule_delayed_work(&di->load_fod_param_work,
					      msecs_to_jiffies(500));
			schedule_delayed_work(&di->vout_regulator_work,
					      msecs_to_jiffies(400));
			dev_info(di->dev, "%s:set vout_vol %d\n", __func__,
				 adapter_vol);
		} else if (adapter_vol == EPP_VOL_THRESHOLD) {
			vout = idtp9220_get_vout(di);
			while (vout > EPP_VOL_THRESHOLD) {
				vout = vout - 1000;
				idtp9220_set_vout(di, vout);
				msleep(200);
			}
			idtp9220_set_vout(di, EPP_VOL_THRESHOLD);
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

	dev_info(di->dev,
		 "di->status:0x%x,adapter_vol=%d,icl_curr=%d,last_vin=%d,last_icl=%d\n",
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
	union power_supply_propval val = { 0, };
	int ret = 0;

	ret = idtp9220_get_property_names(di);
	if (ret < 0) {
		dev_err(di->dev, "get property error: %d\n", ret);
		return;
	}
	if (!di->power_good_flag)
		return;

	pm_stay_awake(di->dev);

	dev_info(di->dev,
		 "power_good low, wireless detached\n");
	cancel_delayed_work(&di->dc_check_work);
	di->power_good_flag = 0;
	val.intval = 0;
	di->ss = 2;
	di->bpp_break = true;
	cancel_delayed_work(&di->oob_set_cep_work);
	cancel_delayed_work_sync(&di->chg_monitor_work);
	cancel_delayed_work_sync(&di->epp_connect_load_work);
	cancel_delayed_work_sync(&di->rx_vout_cp_close_work);
	cancel_delayed_work_sync(&di->rx_vout_work);
	vote(di->fcc_votable, VOICE_LIMIT_FCC_VOTER, false, 0);
	vote(di->fcc_votable, VOICE_LIMIT_FCC_1A_VOTER, false, 0);
	idtp9220_set_present(di, val.intval);
	if (!di->icl_votable)
		di->icl_votable = find_votable("dc_icl_votable");

	if (di->icl_votable)
		vote(di->icl_votable, OCP_PROTECTION_VOTER, false, 0);

	idtp922x_set_pmi_icl(di, 0);

	power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN,
					  &val);

	if (di->wait_for_reverse_test) {
			msleep(2000);
			idtp9220_set_reverse_enable(di, true);
			dev_err(di->dev, "[ factory reverse test ] wait for factory reverse charging test, power good low\n");
			alarm_start_relative(&di->reverse_test_ready_alarm,
					     ms_to_ktime(REVERSE_TEST_READY_CHECK_DELAY_MS));
	}
	schedule_delayed_work(&di->keep_awake_work, msecs_to_jiffies(5000));
	return;
}

static void idtp9220_fw_download_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 fw_download_work.work);

	u8 fw_app_ver[4] = { 0 };
	union power_supply_propval val = { 0, };
	bool crc_ok = false;
	int rc = 0;

	dev_info(di->dev, "[idt] enter %s\n", __func__);

	if (di->fw_update) {
		dev_info(di->dev, "[idtp9415] [%s] FW Update is on going!\n",
			 __func__);
		return;
	}

	power_supply_get_property(di->dc_psy, POWER_SUPPLY_PROP_ONLINE, &val);

	if (!val.intval) {
		pm_stay_awake(di->dev);
		di->fw_update = true;
		idtp9220_set_reverse_gpio(di, true);
		msleep(100);
		rc = di->bus.read_buf(di, REG_EPRFWVER_ADDR, fw_app_ver, 4);
		dev_info(di->dev, "%s: RX_FW version %x.%x.%x.%x\n", __func__,
			 fw_app_ver[3], fw_app_ver[2], fw_app_ver[1],
			 fw_app_ver[0]);

		if (!rc)
			di->chip_ok = 1;
		else
			di->chip_ok = 0;

		di->fw_version = fw_app_ver[0];
		if (!program_crc_verify(di)) {
			dev_err(di->dev, "update crc verify failed.\n");
			crc_ok = false;
		} else {
			dev_info(di->dev, "update crc verify success.\n");
			crc_ok = true;
		}

		idtp9220_set_reverse_gpio(di, false);
		msleep(30);

		if ((fw_app_ver[3] == 0x2
		     && fw_app_ver[2] == 0x6
		     && fw_app_ver[1] == 0x1
		     && fw_app_ver[0] >= FW_VERSION) && (crc_ok)) {
			dev_info(di->dev, "FW: 0x%x, crc: %d so skip upgrade\n",
				 fw_app_ver[0], crc_ok);
		} else {
#ifndef CONFIG_FACTORY_BUILD
			idtp9220_set_reverse_gpio(di, true);
			msleep(100);

			dev_info(di->dev, "%s: FW download start\n", __func__);
			if (!program_fw
			    (di, 0x0000, idt_firmware, sizeof(idt_firmware))) {
				dev_err(di->dev, "program fw failed.\n");
			} else {
				dev_info(di->dev,
					 "[idt] %s: program fw %ld bytes done\n",
					 __func__, sizeof(idt_firmware));
				di->fw_version = FW_VERSION;
			}

			idtp9220_set_reverse_gpio(di, false);
			msleep(100);
			/* start crc verify */
			idtp9220_set_reverse_gpio(di, true);
			msleep(100);

			if (!program_crc_verify(di))
				dev_err(di->dev, "crc verify failed.\n");
			else
				dev_info(di->dev, "crc verify success.\n");
			idtp9220_set_reverse_gpio(di, false);
#else
			dev_info(di->dev, "%s: factory build, don't update\n",
				 __func__);
#endif
		}
		di->fw_update = false;
		pm_relax(di->dev);
	} else
		dev_info(di->dev,
			 "%s: Skip FW download due to wireless charging\n",
			 __func__);
}


static void idtp9220_rx_vout_cp_close_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
			 rx_vout_cp_close_work.work);
	int vout = 0;
	vout = idtp9220_get_vout(di);
	while (vout > CP_CLOSE_VOUT_MV) {
		vout = vout - 1000;
		if (!di->power_good_flag)
			return;
		idtp9220_set_vout(di, vout);
		msleep(300);
	}
	if (!di->power_good_flag)
		return;
	idtp9220_set_vout(di, CP_CLOSE_VOUT_MV);

	if (di->power_good_flag)
		di->cp_regulator_done = true;

}
static void idtp9220_keep_awake_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
			 keep_awake_work.work);
	pm_relax(di->dev);
}
static int idtp_get_effective_icl_val(struct idtp9220_device_info *di)
{
	int effective_icl_val = 0;

	if (!di->icl_votable)
		di->icl_votable = find_votable("DC_ICL");

	if (!di->icl_votable)
		return -EINVAL;

	effective_icl_val = get_effective_result_locked(di->icl_votable);
	pr_info("effective_icl_val: %d\n", effective_icl_val);
	return effective_icl_val;
}

static int idtp_set_effective_icl_val(struct idtp9220_device_info *di, int icl)
{
	int effective_icl_val = 0;

	if (!di->icl_votable)
		di->icl_votable = find_votable("DC_ICL");

	if (!di->icl_votable)
		return -EINVAL;

	vote(di->icl_votable, OCP_PROTECTION_VOTER, true, icl);
	pr_info("effective_icl_val: %d\n", effective_icl_val);
	return 0;
}


#ifndef CONFIG_FACTORY_BUILD
#define NEW_MAX_POWER_CMD		0x28
#define RENEGOTIATION_CMD		0x80
static void idtp9220_renegociation(struct idtp9220_device_info *di)
{
	if (di->tx_charger_type == ADAPTER_XIAOMI_PD_45W || di->tx_charger_type == ADAPTER_XIAOMI_PD_60W) {
		di->bus.write(di, REG_MAX_POWER, NEW_MAX_POWER_CMD);
		di->bus.write(di, REG_RX_RESET, RENEGOTIATION_CMD);
	}
}
#endif
static void idtp9220_start_to_load(struct idtp9220_device_info *di)
{
	union power_supply_propval val = { 0, };
	schedule_delayed_work(&di->rx_vout_work,
				  msecs_to_jiffies(100));
	schedule_delayed_work(&di->chg_monitor_work,
				  msecs_to_jiffies(1000));
	if (di->tx_charger_type == ADAPTER_XIAOMI_PD_40W ||
		di->tx_charger_type == ADAPTER_XIAOMI_PD_45W ||
			di->tx_charger_type == ADAPTER_XIAOMI_PD_60W) {
		if (di->wireless_psy) {
			val.intval = di->tx_charger_type;
			power_supply_set_property(di->wireless_psy, POWER_SUPPLY_PROP_QUICK_CHARGE_TYPE, &val);
		}
		msleep(100);
		if (di->usb_psy)
			power_supply_changed(di->usb_psy);
	}

	if (di->wireless_psy)
		power_supply_changed(di->wireless_psy);
}

static void idtp9220_idt_first_boot(struct work_struct *work)
{
	struct idtp9220_device_info *di =
		container_of(work, struct idtp9220_device_info,
				idt_first_boot.work);
	idt_first_flag = true ;
	if (di->idtp_psy)
		power_supply_changed(di->idtp_psy);
}

static int idt_trx_get_ble_mac(struct idtp9220_device_info *di)
{
	int rc = 0, i = 0;
	u8 mac_buf[MAC_LEN] = { 0 };

	rc = di->bus.read_buf(di, REG_MAC_ADDR, mac_buf, MAC_LEN);
	if (rc < 0) {
		dev_err(di->dev, "trx read mac addr error: %d\n", rc);
		return rc;
	}

	dev_info(di->dev, "mac addr of pen: 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
		mac_buf[0], mac_buf[1], mac_buf[2], mac_buf[3], mac_buf[4], mac_buf[5]);

	for (i = 0; i < MAC_LEN; i++)
		di->pen_mac_data[i] = mac_buf[i];

	idtp922x_sent_pen_mac(di);

	return rc;
}

static void idtp9220_hall3_irq_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 hall3_irq_work.work);
	if (di->hall3_online)
		idtp9220_set_reverse_enable(di, true);
	else if (!di->hall4_online) {
		idtp9220_set_reverse_enable(di, false);
		di->is_reverse_mode = 0;
		di->is_reverse_chg = 2;
		schedule_delayed_work(&di->reverse_sent_state_work, 0);
	} else
		dev_info(di->dev, "[hall3] hall4 online, don't disable reverse charge\n");

	return;
}

static void idtp9220_hall4_irq_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 hall4_irq_work.work);

	if (di->hall4_online)
		idtp9220_set_reverse_enable(di, true);
	else if (!di->hall3_online) {
		idtp9220_set_reverse_enable(di, false);
		di->is_reverse_mode = 0;
		di->is_reverse_chg = 2;
		schedule_delayed_work(&di->reverse_sent_state_work, 0);
	} else
		dev_info(di->dev, "[hall4] hall3 online, don't disable reverse charge\n");

	return;
}

#define OCP_LOWER_ICL_SETP_UA 500000;
static void idtp9220_irq_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 irq_work.work);
	union power_supply_propval val = { 0, };
	u8 int_buf[4] = { 0 };
	u32 int_val = 0;
	int rc = 0;
	u8 recive_data[5] = { 0 };
	static int retry;
	static int retry_id;
	static int retry_count;
#ifndef CONFIG_FACTORY_BUILD
	static int renego_retry_count;
#endif
	int tx_vin = 0;
	int irq_level;
	int i;
	u8 clr_buf[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
	int lower_icl = 0;
	if (gpio_is_valid(di->dt_props.irq_gpio))
		irq_level = gpio_get_value(di->dt_props.irq_gpio);
	else {
		dev_err(di->dev, "%s: irq gpio not provided\n", __func__);
		irq_level = -1;
		pm_relax(di->dev);
		return;
	}
	if (irq_level) {
		dev_info(di->dev, "irq is high level, ignore%d\n", irq_level);
		pm_relax(di->dev);
		return;
	}

	rc = di->bus.read_buf(di, REG_SYS_INT, int_buf, 4);
	if (rc < 0) {
		dev_err(di->dev, "[idt]read int state error: %d\n", rc);
		goto out;
	}

	if (di->is_reverse_mode || di->is_boost_mode) {
		reverse_clrInt(di, int_buf, 4);
		int_val =
		    int_buf[0] | (int_buf[1] << 8) | (int_buf[2] << 16) |
		    (int_buf[3] << 24);
		dev_info(di->dev, "[idt] TRX int: 0x%08x\n", int_val);
		/* add for confirm if irq is cleared start */
		msleep(5);
		if (reverse_need_irq_cleared(di, int_val)) {
			reverse_clrInt(di, clr_buf, 4);
			msleep(5);
		}
		/* add for confirm if irq is cleared end */
		if (int_val & INT_EPT_TYPE) {
			schedule_delayed_work(&di->reverse_ept_type_work, 0);
			goto reverse_out;
		}

		if (int_val & INT_GET_DPING) {
			dev_info(di->dev,
				 "[idt] TRX get dping and disable reverse charging \n");
			idtp9220_set_reverse_enable(di, false);
			di->is_reverse_mode = 0;
			di->is_reverse_chg = 2;
			schedule_delayed_work(&di->reverse_sent_state_work, 0);
			goto reverse_out;
		}

		if (int_val & INT_START_DPING) {
			alarm_start_relative(&di->reverse_chg_alarm,
					     ms_to_ktime
					     (REVERSE_CHG_CHECK_DELAY_MS));
			rc = alarm_cancel(&di->reverse_dping_alarm);
			if (rc < 0)
				dev_err(di->dev,
					"Couldn't cancel reverse_dping_alarm\n");
			pm_relax(di->dev);
		}
		if (int_val & INT_GET_CFG) {
			//cancel_delayed_work(&di->reverse_chg_state_work);
			dev_info(di->dev, "TRX get cfg, cancel 80s alarm\n");
			rc = alarm_cancel(&di->reverse_chg_alarm);
			if (rc < 0)
				dev_err(di->dev,
					"Couldn't cancel reverse_dping_alarm\n");
			pm_stay_awake(di->dev);
			schedule_delayed_work(&di->reverse_sent_state_work, 0);
			di->bus.write(di, REG_TX_CMD, TX_EN);
			msleep(50);
			//start reverse chg infor work
			cancel_delayed_work_sync(&di->reverse_chg_work);
			msleep(10);
			schedule_delayed_work(&di->reverse_chg_work, 0);
			/* set reverse charging state to started */
			if (di->is_reverse_mode || di->is_boost_mode) {
				di->is_reverse_chg = 4;
				schedule_delayed_work(&di->reverse_sent_state_work, 100);
			}
		}
		if (int_val & INT_GET_SS) {
			dev_info(di->dev, "TRX get ss\n");
		}
		if (int_val & INT_GET_ID) {
			dev_info(di->dev, "TRX get id\n");
		}
		if (int_val & INT_INIT_TX) {
			dev_info(di->dev, "[idt] TRX reset done\n");
		}
		if (int_val & INT_GET_BLE_ADDR) {
			dev_info(di->dev, "[idt] TRX get ble mac\n");
			idt_trx_get_ble_mac(di);
		}
		if (int_val & INT_GET_PPP) {
			idtp922x_receivePkt2(di, recive_data);
			dev_err(di->dev, "[ factory reverse test ] receive dates: 0x%02x, 0x%02x\n", recive_data[0], recive_data[1]);
			if (recive_data[0] == 0x18 && recive_data[1] == 0x31) {
				// done
				dev_err(di->dev, "[ factory reverse test ] receiver reverse test done\n");
				idtp9220_set_reverse_enable(di, false);
				di->wait_for_reverse_test = false;
			} else if (recive_data[0] == 0x18 && recive_data[1] == 0x3D) {
				//ready
				dev_err(di->dev, "[ factory reverse test ] receiver reverse test ready, cancel timer\n");
				alarm_cancel(&di->reverse_test_ready_alarm);
			}
		}
	      reverse_out:
		goto out;
	}
	int_val =
	    int_buf[0] | (int_buf[1] << 8) | (int_buf[2] << 16) | (int_buf[3] << 24);
	dev_info(di->dev, "[idt] int: 0x%08x\n", int_val);

	if (int_val & INT_MODE_CHANGE  || int_val & INT_VOUT_OFF) {
		if (di->power_good_flag) {
			cancel_delayed_work_sync(&di->rx_ready_check_work);
			schedule_delayed_work(&di->wpc_det_work, msecs_to_jiffies(0));
		}

		idtp922x_clrInt(di, int_buf, 4);
		msleep(5);
		if (need_irq_cleared(di)) {
			idtp922x_clrInt(di, clr_buf, 4);
			msleep(5);
		}
		goto out;
	}

	if (int_val & INT_OV_CURR) {
		lower_icl = idtp_get_effective_icl_val(di) - OCP_LOWER_ICL_SETP_UA ;

		if (lower_icl > 0)
			idtp_set_effective_icl_val(di, lower_icl);

		/* wait for icl works */
		msleep(1000);
		idtp922x_clrInt(di, int_buf, 4);
		msleep(5);
		if (need_irq_cleared(di)) {
			idtp922x_clrInt(di, clr_buf, 4);
			msleep(5);
		}
		goto out;
	}

	/* clear int and enable irq immediately when read int register */
	idtp922x_clrInt(di, int_buf, 4);
	msleep(5);
	if (need_irq_cleared(di)) {
		idtp922x_clrInt(di, clr_buf, 4);
		msleep(5);
	}

	if (int_val & INT_RXREADY) {
		cancel_delayed_work_sync(&di->rx_ready_check_work);
		if (di->power_good_flag) {
			schedule_delayed_work(&di->wpc_det_work, msecs_to_jiffies(0));
		}
		schedule_delayed_work(&di->initial_tx_work,
			msecs_to_jiffies(100));
		di->power_good_flag = 1;
		val.intval = 1;
		if (!di->wireless_psy)
			di->wireless_psy = power_supply_get_by_name("wireless");
		if (di->wireless_psy)
			power_supply_set_property(di->wireless_psy,
			POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN, &val);
		schedule_delayed_work(&di->rx_ready_check_work, msecs_to_jiffies(100));
	}
	if (int_val & INT_VOUT_ON) {
		if (!di->power_good_flag) {
			cancel_delayed_work_sync(&di->rx_ready_check_work);
			if (di->power_good_flag) {
				schedule_delayed_work(&di->wpc_det_work, msecs_to_jiffies(0));
			}
			schedule_delayed_work(&di->initial_tx_work,
				msecs_to_jiffies(100));
			di->power_good_flag = 1;
			val.intval = 1;
			if (!di->wireless_psy)
				di->wireless_psy = power_supply_get_by_name("wireless");
			if (di->wireless_psy)
				power_supply_set_property(di->wireless_psy,
				POWER_SUPPLY_PROP_WIRELESS_POWER_GOOD_EN, &val);
			schedule_delayed_work(&di->rx_ready_check_work, msecs_to_jiffies(100));
		}
		di->vout_on = true;
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
		if (di->is_ble_tx)
			idtp922x_request_low_addr(di);
		goto out;
	}

	if (int_val & INT_IDAUTH_SUCESS) {
		idtp9220_send_device_auth(di);
		//idtp922x_request_adapter(di);
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

	if ((int_val & INT_IDAUTH_FAIL) || (int_val & INT_AUTH_FAIL)
	    || int_val == 0) {
		if (((int_val & INT_AUTH_FAIL) || (int_val == 0))
		    && (retry < 5)) {
			idtp9220_send_device_auth(di);
			retry++;
			dev_info(di->dev, "[idtp] dev auth failed retry %d\n",
				 retry);
			goto out;
		} else if ((int_val & INT_IDAUTH_FAIL) && retry_id < 5) {
			idtp9220_retry_id_auth(di);
			retry_id++;
			dev_info(di->dev, "[idtp] id auth failed retry %d\n",
				 retry);
			goto out;
		} else {
			retry = 0;
			retry_id = 0;
		}
		di->tx_charger_type = ADAPTER_AUTH_FAILED;
		dev_info(di->dev, "[idtp] auth failed tx charger type set %d\n",
			 di->tx_charger_type);

		schedule_delayed_work(&di->rx_vout_work, msecs_to_jiffies(0));
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
			if (!di->tx_charger_type)
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
#ifndef CONFIG_FACTORY_BUILD
	if (int_val & RENEG_SUCCESS) {
		if (di->tx_charger_type == ADAPTER_XIAOMI_PD_45W || di->tx_charger_type == ADAPTER_XIAOMI_PD_60W) {
			dev_err(di->dev, "%s: max power renegociation success\n", __func__);
			idtp9220_start_to_load(di);
			renego_retry_count = 0;
		}
	}

	if (int_val & RENEG_FAIL) {
		if (di->tx_charger_type == ADAPTER_XIAOMI_PD_45W || di->tx_charger_type == ADAPTER_XIAOMI_PD_60W) {
			if (renego_retry_count < 3) {
				idtp9220_renegociation(di);
				dev_err(di->dev, "%s: retry renegociation\n", __func__);
				renego_retry_count++;
			} else {
				dev_err(di->dev, "%s: retry limit exceed, set adapter type to 40W\n", __func__);
				di->tx_charger_type = ADAPTER_XIAOMI_PD_40W;
				idtp9220_start_to_load(di);
				renego_retry_count = 0;
			}
		}
	}
#endif

	if (int_val & INT_TX_DATA_RECV) {
		idtp922x_receivePkt(di, recive_data);
		dev_info(di->dev, "[idt] cmd: %x\n", recive_data[0]);

		switch (recive_data[0]) {
		case BC_TX_HWID:
			dev_info(di->dev,
				 "[idt] TX chip_vendor:0x%x, module:0x%x, hw:0x%x and power:0x%x\n",
				 recive_data[4], recive_data[2], recive_data[3],
				 recive_data[1]);
			if (recive_data[4] == 0x07 && recive_data[2] == 0x1
			    && recive_data[3] == 0x4 && ((recive_data[1] == 0x9)
							 || (recive_data[1] ==
							     0x1))) {
				di->is_ble_tx = 1;
			} else if (recive_data[4] == 0x01 &&
				   recive_data[2] == 0x2 &&
				   recive_data[3] == 0x8 &&
				   recive_data[1] == 0x6) {
				di->is_car_tx = 1;
			} else if (recive_data[4] == 0x07 &&
				   recive_data[2] == 0x8 &&
				   recive_data[3] == 0x6 &&
				   recive_data[1] == 0x9) {
				di->is_voice_box_tx = 1;
				di->is_ble_tx = 1;
			} else if (recive_data[4] == 0x06 &&
				   recive_data[2] == 0x1 &&
				   recive_data[3] == 0x5 &&
				   recive_data[1] == 0x9) {
				di->is_pan_tx = 1;
			} else if (recive_data[4] == 0x0B &&
				   recive_data[2] == 0x1 &&
				   recive_data[3] == 0x1 &&
				   recive_data[1] == 0x9) {
				di->is_ble_tx = 1;
			} else if (recive_data[4] == 0x01 &&
				   recive_data[2] == 0x1 &&
				   recive_data[3] == 0x1 &&
				   recive_data[1] == 0x6) {
				di->is_f1_tx = 1;
			} else if (recive_data[4] == 0x01 &&
				   recive_data[2] == 0x3 &&
				   recive_data[3] == 0x8 &&
				   recive_data[1] == 0x7){
				di->is_zm_mobil_tx = 1;
			} else if (recive_data[4] == 0x08 &&
				   recive_data[2] == 0x8 &&
				   recive_data[3] == 0x1 &&
				   recive_data[1] == 0x9){
				di->is_zm_20w_tx = 1;
			}
#ifdef CONFIG_FACTORY_BUILD
			di->is_ble_tx = 0;
#endif
			idtp922x_request_adapter(di);
			break;
		case BC_TX_COMPATIBLE_HWID:
			dev_info(di->dev, "[idt] TX hwid: 0x%x 0x%x\n",
				 recive_data[1], recive_data[2]);
			if (recive_data[1] == 0x12 && recive_data[2])
				di->is_compatible_hwid = 1;
			if (recive_data[1] == 0x16 && recive_data[2] == 0x11)
				di->is_f1_tx = 1;
			idtp922x_request_adapter(di);
			break;
		case BC_ADAPTER_TYPE:
			dev_info(di->dev, "[idt]adapter type: %d\n",
				 recive_data[1]);

			if (di->is_car_tx
			    && (recive_data[1] == ADAPTER_XIAOMI_QC3))
				di->tx_charger_type = ADAPTER_ZIMI_CAR_POWER;
			else if (di->is_voice_box_tx)
				di->tx_charger_type = ADAPTER_VOICE_BOX;
			else
				di->tx_charger_type = recive_data[1];

			if (di->tx_charger_type == ADAPTER_XIAOMI_PD_40W && di->is_zm_mobil_tx) {
				//di->tx_charger_type = ADAPTER_VOICE_BOX;
				di->is_voice_box_tx = 1;
			}
			/*
			   if(!di->epp && (di->tx_charger_type == ADAPTER_QC3 ||
			   di->tx_charger_type == ADAPTER_QC2)) {
			   idtp922x_set_adap_vol(di, ADAPTER_BPP_VOL);
			   dev_info(di->dev, "[idt]bpp mode set 5v first\n");
			   }
			 */
#ifdef CONFIG_FACTORY_BUILD
			idtp9220_start_to_load(di);
#else
			if (di->tx_charger_type == ADAPTER_XIAOMI_PD_45W || di->tx_charger_type == ADAPTER_XIAOMI_PD_60W) {
				renego_retry_count = 0;
				idtp9220_renegociation(di);
			} else {
				idtp9220_start_to_load(di);
			}
#endif
			break;
		case BC_READ_Vin:
			tx_vin = recive_data[1] | (recive_data[2] << 8);
			if (!di->power_off_mode) {
				if (di->epp)
					idtp9220_set_vout(di,
							  di->adapter_voltage);
				else
					idtp9220_set_vout(di,
							  ADAPTER_BPP_QC_VOL);
			}
			dev_info(di->dev, "[idt] tx vin : %d\n", tx_vin);
			break;
		case BC_READ_VOUT:
			schedule_delayed_work(&di->get_vout_work,
					      msecs_to_jiffies(0));
			break;
		case BC_READ_IOUT:
			schedule_delayed_work(&di->get_iout_work,
					      msecs_to_jiffies(0));
			break;
		case BC_RX_CHIP_VERSION:
			dev_info(di->dev, "[idt]Detect RX Chip version\n");
			idtp922x_sent_chip_version(di);
			break;
		case BC_RX_FW_VERSION:
			dev_info(di->dev, "[idt]Detect RX FW version\n");
			idtp922x_sent_fw_version(di);
			break;
		case BC_RX_ID_AUTH:
			dev_info(di->dev,
				 "[idt] ID Auth retry success: 0x%x, 0x%x\n",
				 recive_data[1], recive_data[2]);
			idtp9220_send_device_auth(di);
			break;
		case CMD_GET_BLEMAC_2_0:
			dev_info(di->dev,
				 "[idt] get mac low 3 bytes: 0x%x, 0x%x, 0x%x\n",
				 recive_data[1], recive_data[2],
				 recive_data[3]);
			for (i = 0; i < 3; i++) {
				di->mac_data[i] = recive_data[i + 1];
			}
			idtp922x_request_high_addr(di);
			break;
		case CMD_GET_BLEMAC_5_3:
			dev_info(di->dev,
				 "[idt] get mac high 3 bytes: 0x%x, 0x%x, 0x%x\n",
				 recive_data[1], recive_data[2],
				 recive_data[3]);
			for (i = 0; i < 3; i++) {
				di->mac_data[i + 3] = recive_data[i + 1];
			}
			idtp922x_sent_tx_mac(di);
			break;
		case CMD_FACOTRY_REVERSE:
			dev_info(di->dev, "[ factory reverse test ] send factory reverse cnf\n");
			idtp922x_sent_reverse_cnf(di);
			di->wait_for_reverse_test = true;
			break;
		default:
			dev_info(di->dev, "[idt] unsupport cmd: %x\n",
				 recive_data[0]);
			break;
		}
	}
	if (int_val & INT_RPP_READ)
		rc = idtp9220_set_rpp(di);

      out:
    pm_relax(di->dev);
	return;
}

static irqreturn_t idtp9220_wpc_det_irq_handler(int irq, void *dev_id)
{
	//struct idtp9220_device_info *di = dev_id;

	//pm_stay_awake(di->dev);
	printk("idtp9220_wpc_det_irq_handler\n");
	//schedule_delayed_work(&di->wpc_det_work, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static int idtp9220_ocp_disable_vote_cb(struct votable *votable,
		void *data, int disable, const char *client)
{
	struct idtp9220_device_info *di = (struct idtp9220_device_info *)data;
	di->bus.write(di, REG_OCP_CONFIG, !disable);
	dev_err(di->dev, "idtp9220_ocp_disable_vote_cb enable:%d\n", !disable);
	return 0;
}

static irqreturn_t idtp9220_irq_handler(int irq, void *dev_id)
{
	struct idtp9220_device_info *di = dev_id;
	pm_stay_awake(di->dev);
	printk("idtp9220_irq_handler\n");
	schedule_delayed_work(&di->irq_work, msecs_to_jiffies(10));
	return IRQ_HANDLED;
}

static irqreturn_t idtp9220_hall3_irq_handler(int irq, void *dev_id)
{
	struct idtp9220_device_info *di = dev_id;

	if (gpio_is_valid(di->dt_props.hall3_gpio)) {
		if (gpio_get_value(di->dt_props.hall3_gpio)) {
			di->hall3_online = 0;
			if (di->hall4_online) {
				dev_info(di->dev, "idtp9220_hall3_irq_handler: hall4 online, return\n");
				return IRQ_HANDLED;
			}
			schedule_delayed_work(&di->hall3_irq_work, msecs_to_jiffies(0));
			return IRQ_HANDLED;
		} else {
			di->hall3_online = 1;
		}
	}

	if (di->hall4_online) {
		dev_info(di->dev, "[hall3] reverse charging already running, return\n");
		return IRQ_HANDLED;
	}
	else
		schedule_delayed_work(&di->hall3_irq_work, msecs_to_jiffies(10));

	return IRQ_HANDLED;
}

static irqreturn_t idtp9220_hall4_irq_handler(int irq, void *dev_id)
{
	struct idtp9220_device_info *di = dev_id;

	if (gpio_is_valid(di->dt_props.hall4_gpio)) {
		if (gpio_get_value(di->dt_props.hall4_gpio)) {
			di->hall4_online = 0;
			if (di->hall3_online) {
				dev_info(di->dev, "idtp9220_hall4_irq_handler: hall3 online, return\n");
				return IRQ_HANDLED;
			}
			schedule_delayed_work(&di->hall4_irq_work, msecs_to_jiffies(0));
			return IRQ_HANDLED;
		} else {
			di->hall4_online = 1;
		}
	}

	if (di->hall3_online) {
		dev_info(di->dev, "[hall4] reverse charging already running, return\n");
		return IRQ_HANDLED;
	}
	else
		schedule_delayed_work(&di->hall4_irq_work, msecs_to_jiffies(10));

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

	if (!di->hall3_irq) {
		dev_err(di->dev, "%s: hall3 irq is wrong\n", __func__);
		return -EINVAL;
	}

	ret = request_irq(di->hall3_irq, idtp9220_hall3_irq_handler,
			  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "hall3_irq", di);
	if (ret) {
		dev_err(di->dev, "%s: request hall3 irq failed\n", __func__);
		return ret;
	}

	ret = enable_irq_wake(di->hall3_irq);
	if (ret) {
		dev_err(di->dev, "%s: enable hall3 irq wake failed\n", __func__);
		return ret;
	}

	if (!di->hall4_irq) {
		dev_err(di->dev, "%s: hall4 irq is wrong\n", __func__);
		return -EINVAL;
	}

	ret = request_irq(di->hall4_irq, idtp9220_hall4_irq_handler,
			  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "hall4_irq", di);
	if (ret) {
		dev_err(di->dev, "%s: request hall4 irq failed\n", __func__);
		return ret;
	}

	ret = enable_irq_wake(di->hall4_irq);
	if (ret) {
		dev_err(di->dev, "%s: enable hall4 irq wake failed\n", __func__);
		return ret;
	}

	if (!di->wpc_irq) {
		dev_err(di->dev, "%s: wpc irq is wrong\n", __func__);
		return -EINVAL;
	}

	ret = request_irq(di->wpc_irq, idtp9220_wpc_det_irq_handler,
			  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "wpc_det",
			  di);
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
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = 0xFFFF,
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

static u8 oob_check_sum(u8 *buf, u32 size)
{
	u8 chksum = 0;
	while (size--) {
		chksum ^= *buf++;
	}

	return chksum;
}

static int idtp9220_set_ept(struct idtp9220_device_info *di)
{
	union power_supply_propval val = { 0, };
	int ept = 0;
	u8 ept_raw = 0x0b;
	u8 header[2] = { 0 };
	u8 chksum[4] = { 0 };
	int rc = 0;

	header[1] = 0x02;	//add the ept header

	chksum[0] = header[1];
	chksum[1] = ept_raw;
	header[0] = oob_check_sum(chksum, 2);
	ept = header[0] | ept_raw << 8 | (header[1] << 16);
	val.int64val = ept;

	if (di->wireless_psy) {
		mutex_lock(&di->sysfs_op_lock);
		power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_RX_CEP, &val);
		mutex_unlock(&di->sysfs_op_lock);
	} else {
		dev_err(di->dev, "BLE EPT set error:\n");
		rc = -EINVAL;
	}

	dev_info(di->dev, "Header: 0x%x, ept_raw: 0x%x, checksum: 0x%x\n",
		 header[1], ept_raw, header[0]);

	return rc;
}

static int idtp9220_set_rpp(struct idtp9220_device_info *di)
{
	int64_t rpp = 0;
	int64_t header_ul = 0;
	u8 rpp_raw[2] = { 0 };
	u8 header[2] = { 0 };
	u8 chksum[4] = { 0 };
	union power_supply_propval val = { 0, };
	int rc = 0;

	header[1] = 0x31;	//add the rpp header
	di->bus.read_buf(di, REG_RPP, rpp_raw, 2);

	chksum[0] = header[1];
	chksum[1] = rpp_raw[0];
	chksum[2] = rpp_raw[1];
	header[0] = oob_check_sum(chksum, 3);

	dev_info(di->dev,
		 "header: 0x%x, RPP_raw:  0x%x, 0x%x, checksum: 0x%x\n",
		 header[1], rpp_raw[1], rpp_raw[0], header[0]);
	rpp = header[0] | (rpp_raw[0] << 8) | (rpp_raw[1] << 16);
	header_ul = header[1];
	header_ul <<= 32;
	header_ul |= rpp;
	val.int64val = header_ul;

	if (di->wireless_psy) {
		mutex_lock(&di->sysfs_op_lock);
		power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_RX_CR, &val);
		mutex_unlock(&di->sysfs_op_lock);
	} else {
		dev_err(di->dev, "BLE RPP set error:\n");
		rc = -EINVAL;
	}
	return rc;
}

static int idtp9220_set_cep(struct idtp9220_device_info *di)
{
	int cep = 0;
	u8 cep_raw = 0;
	u8 header[2] = { 0 };
	u8 chksum[4] = { 0 };
	union power_supply_propval val = { 0, };
	int rc = 0;

	header[1] = 0x03;	//add the rpp header
	di->bus.read(di, REG_CEP, &cep_raw);

	chksum[0] = header[1];
	chksum[1] = cep_raw;
	header[0] = oob_check_sum(chksum, 2);

	dev_info(di->dev, "Header: 0x%x, CEP_raw: 0x%x, checksum: 0x%x\n",
		 header[1], cep_raw, header[0]);
	cep = header[0] | cep_raw << 8 | (header[1] << 16);
	val.int64val = cep;
	if (di->wireless_psy) {
		mutex_lock(&di->sysfs_op_lock);
		power_supply_set_property(di->wireless_psy,
					  POWER_SUPPLY_PROP_RX_CEP, &val);
		mutex_unlock(&di->sysfs_op_lock);
	} else {
		dev_err(di->dev, "BLE CEP set error:\n");
		rc = -EINVAL;
	}
	return rc;
}

static void idtp_oob_set_cep_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 oob_set_cep_work.work);
	int rc;
	u8 ble_ok;

	di->bus.read(di, REG_BLE_FLAG, &ble_ok);
	if (ble_ok & BIT(0))
		rc = idtp9220_set_cep(di);

	schedule_delayed_work(&di->oob_set_cep_work, 1 * HZ);
	return;
}

static void idt_get_reverse_vout(struct idtp9220_device_info *di)
{
	u8 vout_l, vout_h;
	int vout = 0;

	if (!di)
		return;

	di->bus.read(di, REG_REVERSE_VIN_LOW, &vout_l);
	di->bus.read(di, REG_REVERSE_VIN_HIGH, &vout_h);
	vout = vout_l | (vout_h << 8);
	dev_info(di->dev, "[reverse] vout is %d\n", vout);
	di->reverse_vout = vout;
	return;
}

static void idt_get_reverse_soc(struct idtp9220_device_info *di)
{
	u8 soc = 0;

	if (!di)
		return;

	di->bus.read(di, REG_CHG_STATUS, &soc);
	if ((soc < 0) || (soc > 0x64))
	{
		if (soc == 0xFF) {
			di->reverse_pen_soc = 0xFF;
		} else {
			return;
		}
	}
	di->reverse_pen_soc = soc;

	if ((di->reverse_pen_soc == 100) && (pen_soc_count < SOC_100_RETRY)) {
		pen_soc_count ++;
	} else {
		pen_soc_count = 0;
	}

	if (pen_soc_count == SOC_100_RETRY) {
		idtp9220_set_reverse_enable(di, false);
		di->is_reverse_mode = 0;
		di->is_reverse_chg = 2;
		pen_soc_count = 0;
		schedule_delayed_work(&di->reverse_sent_state_work, 0);
	}

	return;
}

static void idt_get_reverse_iin(struct idtp9220_device_info *di)
{
	u8 iin_l, iin_h;
	int iin = 0;

	if (!di)
		return;

	di->bus.read(di, REG_REVERSE_IIN_LOW, &iin_l);
	di->bus.read(di, REG_REVERSE_IIN_HIGH, &iin_h);
	iin = iin_l | (iin_h << 8);
	dev_info(di->dev, "[reverse] iin is %d\n", iin);
	di->reverse_iout = iin;
	return;
}

static void idt_get_reverse_temp(struct idtp9220_device_info *di)
{
	u8 temp = 0;

	if (!di)
		return;

	di->bus.read(di, REG_REVERSE_TEMP, &temp);
	dev_info(di->dev, "[reverse] temp is %d\n", temp);
	return;
}

static void idt_reverse_chg_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 reverse_chg_work.work);

	if (di->is_reverse_mode || di->is_boost_mode) {
		idt_get_reverse_vout(di);
		idt_get_reverse_iin(di);
		idt_get_reverse_temp(di);
		idt_get_reverse_soc(di);
	}
	else {
		di->reverse_pen_soc = 255;
		di->reverse_vout = 0;
		di->reverse_iout = 0;
		return;
	}
	if (di->reverse_pen_soc == 255)
		schedule_delayed_work(&di->reverse_chg_work, 100);
	else
		schedule_delayed_work(&di->reverse_chg_work, 10 * HZ);
	return;
}

static void idtp_oob_set_ept_work(struct work_struct *work)
{
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 oob_set_ept_work.work);

	idtp9220_set_ept(di);

	return;
}

static void idtp_oob_clean_work(struct work_struct *work)
{
	union power_supply_propval val = { 0, };
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 oob_clean_work.work);
	memset(di->mac_data, 0x0, sizeof(di->mac_data));
	idtp922x_sent_tx_mac(di);
	if (di->wireless_psy) {
		power_supply_set_property(di->wireless_psy,
			   POWER_SUPPLY_PROP_RX_CR, &val);
		power_supply_set_property(di->wireless_psy,
			   POWER_SUPPLY_PROP_RX_CEP, &val);
	}
	return;
}

#define WPC_MODE_BPP 0x1
#define WPC_MODE_EPP 0x9

static void idtp_rx_ready_check_work(struct work_struct *work)
{
	int rx_dead = 0;
	u8 mode;
	u8 buffer[2] = { 0, 0 };
	struct idtp9220_device_info *di =
	    container_of(work, struct idtp9220_device_info,
			 rx_ready_check_work.work);
	rx_dead = di->bus.read(di, REG_WPC_MODE, &mode);
	di->bus.read_buf(di, 0x2c, buffer, 2);
	if (rx_dead) {
		msleep(10);
		rx_dead = di->bus.read(di, REG_WPC_MODE, &mode);
		di->bus.read_buf(di, 0x2c, buffer, 2);
	}
	dev_info(di->dev, "rx status: %d mod:0x%x  2C_RISTER 0x%x 0x%x\n", rx_dead, mode, buffer[0], buffer[1]);
	if (rx_dead || (mode != WPC_MODE_BPP && mode != WPC_MODE_EPP)) {
		dev_err(di->dev, "rx not respond, power good low\n");
		if (di->power_good_flag) {
			schedule_delayed_work(&di->wpc_det_work, msecs_to_jiffies(0));
		}
	} else {
		schedule_delayed_work(&di->rx_ready_check_work, msecs_to_jiffies(500));
	}
	return;
}
static int idtp9220_get_temperature(struct idtp9220_device_info *di, u8 *temp)
{
	int rc = 0;
	rc = di->bus.read(di, REG_TEMPTER, temp);
	return rc;
}

static void idtp9220_power_off(struct idtp9220_device_info *di)
{
	dev_err(di->dev, "bbc or smb report power off, PG:%d\n", di->power_good_flag);
	if (di->power_good_flag && di->vout_on) {
		schedule_delayed_work(&di->wpc_det_work, msecs_to_jiffies(0));
	}
}

int idtp_op_ble_flag(int en)
{
	int rc;
	if (!g_di)
		return -EINVAL;

	dev_info(g_di->dev, "set ble flag: %d\n", en);

	if (en) {
		rc = g_di->bus.mask_write(g_di, REG_BLE_FLAG, BIT(0), BIT(0));
		schedule_delayed_work(&g_di->oob_set_cep_work, 0);
	} else {
		rc = g_di->bus.mask_write(g_di, REG_BLE_FLAG, BIT(0), 0);
		cancel_delayed_work_sync(&g_di->oob_set_cep_work);
	}

	return rc;
}

static int rx_set_otg_state(struct idtp9220_device_info *di, int plugin)
{
	u8 mode;
	di->otg_insert = plugin;
	dev_info(di->dev, "[otg]set otg state: %d\n", plugin);

	if (!plugin || !di->is_reverse_mode)
		return 0;

	di->bus.read(di, REG_WROK_MODE, &mode);
	if (mode & BIT(2)) {
		dev_info(di->dev, "[otg]already 148K: 0x%x\n", mode);
		return 0;
	} else {
		di->bus.write(di, REG_TX_CMD, TX_TOGGLE);
		msleep(10);
		di->bus.read(di, REG_WROK_MODE, &mode);
		dev_info(di->dev, "[otg]007B: 0x%x\n", mode);
		if (mode & BIT(2))
			dev_info(di->dev, "[otg]set 148K success: 0x%x\n",
				 mode);
		else
			dev_info(di->dev, "[otg]set 148K error: 0x%x\n", mode);
	}

	return 0;
}

static enum power_supply_property idtp9220_props[] = {
	POWER_SUPPLY_PROP_PIN_ENABLED,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_WIRELESS_VERSION,
	POWER_SUPPLY_PROP_WIRELESS_FW_VERSION,
	POWER_SUPPLY_PROP_SIGNAL_STRENGTH,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_VRECT,
	POWER_SUPPLY_PROP_RX_IOUT,
	POWER_SUPPLY_PROP_WIRELESS_TX_ID,
	POWER_SUPPLY_PROP_TX_ADAPTER,
	POWER_SUPPLY_PROP_REVERSE_CHG_MODE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_DIV_2_MODE,
	POWER_SUPPLY_PROP_OTG_STATE,
	POWER_SUPPLY_PROP_REVERSE_CHG_HALL3,
	POWER_SUPPLY_PROP_REVERSE_CHG_HALL4,
	POWER_SUPPLY_PROP_REVERSE_PEN_SOC,
	POWER_SUPPLY_PROP_REVERSE_VOUT,
	POWER_SUPPLY_PROP_REVERSE_IOUT,
};

static int idtp9220_get_prop(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	struct idtp9220_device_info *di = power_supply_get_drvdata(psy);
	u8 temp = 0;
	int rc = 0;
	switch (psp) {
	case POWER_SUPPLY_PROP_PIN_ENABLED:
		val->intval = di->gpio_enable_val;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->dcin_present;
		break;
	case POWER_SUPPLY_PROP_WIRELESS_VERSION:
		val->intval = di->epp;
		break;
	case POWER_SUPPLY_PROP_WIRELESS_FW_VERSION:
		val->intval = di->fw_version;
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
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_VRECT:
		if (!di->power_good_flag) {
			val->intval = 0;
			break;
		}
		val->intval = idtp9220_get_vrect(di);
		break;
	case POWER_SUPPLY_PROP_RX_IOUT:
		if (!di->power_good_flag) {
			val->intval = 0;
			break;
		}
		val->intval = idtp9220_get_iout(di);
		break;
	case POWER_SUPPLY_PROP_WIRELESS_TX_ID:
		val->intval = idtp9220_get_tx_id(di);
		break;
	case POWER_SUPPLY_PROP_TX_ADAPTER:
		val->intval = di->tx_charger_type;
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		val->intval = gpio_get_value(di->dt_props.reverse_gpio);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = di->chip_ok;
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_HALL3:
		if (gpio_is_valid(di->dt_props.hall3_gpio)) {
			val->intval = gpio_get_value(di->dt_props.hall3_gpio);
			dev_info(di->dev, "hall3 gpio:%d\n", val->intval);
		} else {
			val->intval = -1;
			dev_err(di->dev, "hall3 gpio is not provided\n");
		}
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_HALL4:
		if (gpio_is_valid(di->dt_props.hall4_gpio)) {
			val->intval = gpio_get_value(di->dt_props.hall4_gpio);
			dev_info(di->dev, "hall4 gpio:%d\n", val->intval);
		} else {
			val->intval = -1;
			dev_err(di->dev, "hall4 gpio is not provided\n");
		}
		break;
	case POWER_SUPPLY_PROP_REVERSE_PEN_SOC:
		if (di->is_reverse_mode || di->is_boost_mode)
			val->intval = di->reverse_pen_soc;
		else
			val->intval = 50;
		break;
	case POWER_SUPPLY_PROP_REVERSE_VOUT:
		if (di->is_reverse_mode || di->is_boost_mode)
			val->intval = di->reverse_vout;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_REVERSE_IOUT:
		if (di->is_reverse_mode || di->is_boost_mode)
			val->intval = di->reverse_iout;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_OTG_STATE:
		if (!di->power_good_flag) {
			val->intval = 0;
			break;
		}
		rc = idtp9220_get_temperature(di, &temp);
		if (!rc)
			val->intval = temp;
		else
			val->intval = 0;
	default:
		return -EINVAL;
	}
	return 0;
}

#define RX_VOUT_MAX_MV 20000
#define RX_VOUT_MIN_MV  12000
#define RX_VOUT_OFFSET_MV 100

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
		if (di->idtp_psy)
			power_supply_changed(di->idtp_psy);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		data = val->intval / 1000;

		if (data > RX_VOUT_MAX_MV)
			data = RX_VOUT_MAX_MV;
		else if (data < RX_VOUT_MIN_MV)
			data = RX_VOUT_MIN_MV + RX_VOUT_OFFSET_MV;

		if (di->power_good_flag) {
			if (data == CP_CLOSE_VOUT_MV)
				schedule_delayed_work(&di->rx_vout_cp_close_work, 0);
			else
				idtp9220_set_vout(di, data);
		}
		break;
	case POWER_SUPPLY_PROP_REVERSE_CHG_MODE:
		if (di->fw_update) {
			break;
		}

		if (di->hall3_online || di->hall4_online) {
			if (di->is_reverse_mode || di->is_boost_mode) {
				break;
			}
		} else {
			break;
		}

		di->is_reverse_chg = 0;
		schedule_delayed_work(&di->reverse_sent_state_work, 0);
		if (!di->power_good_flag) {
			idtp9220_set_reverse_enable(di, val->intval);
		} else {
			di->is_reverse_chg = 3;
			schedule_delayed_work(&di->reverse_sent_state_work, 0);
		}
		break;
	case POWER_SUPPLY_PROP_OTG_STATE:
		rx_set_otg_state(di, val->intval);
		break;
	case POWER_SUPPLY_PROP_DIV_2_MODE:
		idtp9220_power_off(di);
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
	dev_info(di->dev, "[idtp] %s: 0x4D data:%x, (data & BIT(4)):%lu\n",
		 __func__, data, (data & BIT(4)));
	if (!(data & BIT(4)))
		return;

	di->bus.write_buf(di, 0x0600, idt_firmware_sram, size);
	di->bus.read_buf(di, 0x0600, buffer, size);

	while (size--) {
		if (idt_firmware_sram[i] == buffer[i]) {
			printk("buffer[%d]:0x%x", i, buffer[i]);
		} else {
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
	int hall3_val = 1, hall4_val = 1;
	struct idtp9220_device_info *di;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct power_supply_config idtp_cfg = { };

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		dev_err(&client->dev, "i2c check functionality failed!\n");
		return -EIO;
	}

	di = devm_kzalloc(&client->dev, sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev,
			"i2c allocated device info data failed!\n");
		return -ENOMEM;
	}

	di->name = IDT_DRIVER_NAME;
	di->dev = &client->dev;
	di->chip_enable = 1;
	di->ss = 2;
	di->fw_update = false;
	di->fw_version = 0;
	di->status = NORMAL_MODE;
	di->reverse_pen_soc = 255;
	di->regmap = devm_regmap_init_i2c(client, &i2c_idtp9220_regmap_config);
	if (!di->regmap)
		return -ENODEV;
	di->bus.read = idtp9220_read;
	di->bus.write = idtp9220_write;
	di->bus.read_buf = idtp9220_read_buffer;
	di->bus.write_buf = idtp9220_write_buffer;
	di->bus.mask_write = idtp9220_masked_write;
	INIT_DELAYED_WORK(&di->irq_work, idtp9220_irq_work);
	INIT_DELAYED_WORK(&di->hall3_irq_work, idtp9220_hall3_irq_work);
	INIT_DELAYED_WORK(&di->hall4_irq_work, idtp9220_hall4_irq_work);
	INIT_DELAYED_WORK(&di->wpc_det_work, idtp9220_wpc_det_work);
	mutex_init(&di->read_lock);
	mutex_init(&di->write_lock);
	mutex_init(&di->wpc_det_lock);
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
		dev_err(di->dev, "%s: parse dt error [%d]\n", __func__, ret);
		goto cleanup;
	}

	ret = idtp9220_gpio_init(di);
	if (ret < 0) {
		dev_err(di->dev, "%s: gpio init error [%d]\n", __func__, ret);
		goto cleanup;
	}

	ret = idtp9220_irq_request(di);
	if (ret < 0) {
		dev_err(di->dev, "%s: request irq error [%d]\n", __func__, ret);
		goto cleanup;
	}

	/*
	 *      this func write config to otp when init, due to the config will be
	 *      write to otp in factory, so delete it.
	 *

	 if (!program_fw(di, 0x0000, idt_firmware, sizeof(idt_firmware))) {
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
					     &idtp_psy_desc, &idtp_cfg);

	INIT_DELAYED_WORK(&di->chg_monitor_work, idtp9220_monitor_work);
	INIT_DELAYED_WORK(&di->chg_detect_work, idtp9220_chg_detect_work);
	INIT_DELAYED_WORK(&di->bpp_connect_load_work,
			  idtp9220_bpp_connect_load_work);
	INIT_DELAYED_WORK(&di->epp_connect_load_work,
			  idtp9220_epp_connect_load_work);
	INIT_DELAYED_WORK(&di->cmd_check_work, idtp9220_cmd_check_work);
	INIT_DELAYED_WORK(&di->vout_regulator_work,
			  idtp9220_vout_regulator_work);
	INIT_DELAYED_WORK(&di->load_fod_param_work,
			  idtp9220_load_fod_param_work);
	INIT_DELAYED_WORK(&di->rx_vout_work, idtp9220_rx_vout_work);
	INIT_DELAYED_WORK(&di->dc_check_work, idtp9220_dc_check_work);
	INIT_DELAYED_WORK(&di->initial_tx_work, idtp9220_initial_tx_work);
	INIT_DELAYED_WORK(&di->bpp_e5_tx_work, idtp9220_bpp_e5_tx_work);
	INIT_DELAYED_WORK(&di->pan_tx_work, idt_pan_tx_work);
	INIT_DELAYED_WORK(&di->voice_tx_work, idt_voice_tx_work);
	INIT_DELAYED_WORK(&di->qc2_f1_tx_work, idtp9220_qc2_f1_tx_work);
	INIT_DELAYED_WORK(&di->qc3_epp_work, idtp9220_qc3_epp_work);
	INIT_DELAYED_WORK(&di->oob_set_cep_work, idtp_oob_set_cep_work);
	INIT_DELAYED_WORK(&di->oob_set_ept_work, idtp_oob_set_ept_work);
	INIT_DELAYED_WORK(&di->rx_ready_check_work, idtp_rx_ready_check_work);
	INIT_DELAYED_WORK(&di->oob_clean_work, idtp_oob_clean_work);


	INIT_DELAYED_WORK(&di->reverse_ept_type_work,
			  reverse_ept_type_get_work);
	INIT_DELAYED_WORK(&di->reverse_chg_state_work,
			  reverse_chg_state_set_work);
	INIT_DELAYED_WORK(&di->reverse_dping_state_work,
			  reverse_dping_state_set_work);
	INIT_DELAYED_WORK(&di->reverse_test_state_work,
			  reverse_test_state_set_work);
	INIT_DELAYED_WORK(&di->reverse_sent_state_work,
			  reverse_chg_sent_state_work);

	INIT_DELAYED_WORK(&di->get_vout_work, idtp9220_test_vout_work);
	INIT_DELAYED_WORK(&di->get_iout_work, idtp9220_test_iout_work);
	INIT_DELAYED_WORK(&di->fw_download_work, idtp9220_fw_download_work);
	INIT_DELAYED_WORK(&di->idt_first_boot, idtp9220_idt_first_boot);
	INIT_DELAYED_WORK(&di->rx_vout_cp_close_work, idtp9220_rx_vout_cp_close_work);
	INIT_DELAYED_WORK(&di->keep_awake_work, idtp9220_keep_awake_work);
	INIT_DELAYED_WORK(&di->reverse_chg_work, idt_reverse_chg_work);
	mutex_init(&di->sysfs_op_lock);
	mutex_init(&di->reverse_op_lock);

	di->ocp_disable_votable = create_votable("IDTP_OCP_DISABLE",
		VOTE_SET_ANY, idtp9220_ocp_disable_vote_cb, di);
	if (IS_ERR_OR_NULL(di->ocp_disable_votable)) {
		PTR_ERR_OR_ZERO(di->ocp_disable_votable);
		dev_err(di->dev, "Failed to initialize ocp_disable_votable\n");
		return -ENODEV;
	}
	//ret = idtp9220_get_property_names(di);
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

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&di->reverse_dping_alarm,
			   ALARM_BOOTTIME, reverse_dping_alarm_cb);
	} else {
		dev_err(di->dev, "Failed to initialize reverse dping alarm\n");
		return -ENODEV;
	}

	if (alarmtimer_get_rtcdev()) {
		alarm_init(&di->reverse_chg_alarm,
			   ALARM_BOOTTIME, reverse_chg_alarm_cb);
	} else {
		dev_err(di->dev, "Failed to initialize reverse chg alarm\n");
		return -ENODEV;
	}
	if (alarmtimer_get_rtcdev()) {
		alarm_init(&di->reverse_test_ready_alarm,
			   ALARM_BOOTTIME, reverse_test_ready_alarm_cb);
	} else {
		dev_err(di->dev,
			"Failed to initialize reverse_test_ready_alarm alarm\n");
		return -ENODEV;
	}

	if (gpio_is_valid(di->dt_props.hall3_gpio)) {
		hall3_val = gpio_get_value(di->dt_props.hall3_gpio);
		if (!hall3_val) {
			schedule_delayed_work(&di->hall3_irq_work, msecs_to_jiffies(5000));
		}
	}
	else
		dev_err(di->dev, "%s: hall3 gpio not provided\n", __func__);

	if (gpio_is_valid(di->dt_props.hall4_gpio)) {
		hall4_val = gpio_get_value(di->dt_props.hall4_gpio);
		if (!hall4_val) {
			schedule_delayed_work(&di->hall4_irq_work, msecs_to_jiffies(5000));
		}
	}
	else
		dev_err(di->dev, "%s: hall4 gpio not provided\n", __func__);

	dev_info(di->dev, "[idt] success probe idtp922x driver\n");
	get_cmdline(di);
	if (!di->power_off_mode)
#ifdef CONFIG_FACTORY_BUILD
		schedule_delayed_work(&di->chg_detect_work, 3 * HZ);
#else
		schedule_delayed_work(&di->chg_detect_work, 8 * HZ);
#endif
	else {
		dev_info(di->dev, "off-chg mode, reset chip\n");
		idtp9220_set_enable_mode(di, false);
		msleep(10);
		idtp9220_set_enable_mode(di, true);
	}

	if (!idt_first_flag)
		schedule_delayed_work(&di->idt_first_boot, msecs_to_jiffies(30000));
#ifdef IDTP9220_SRAM_UPDATE
	INIT_DELAYED_WORK(&di->sram_update_work, idtp9220_sram_update_work);
	schedule_delayed_work(&di->sram_update_work, 10 * HZ);
#endif
	return 0;

      cleanup:
	free_irq(di->irq, di);
	cancel_delayed_work_sync(&di->irq_work);
	cancel_delayed_work_sync(&di->hall3_irq_work);
	cancel_delayed_work_sync(&di->hall4_irq_work);
	i2c_set_clientdata(client, NULL);

	return ret;
}

static int idtp9220_remove(struct i2c_client *client)
{
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	gpio_free(di->dt_props.reverse_gpio);
	gpio_free(di->dt_props.reverse_boost_enable_gpio);
	cancel_delayed_work_sync(&di->irq_work);
	cancel_delayed_work_sync(&di->hall3_irq_work);
	cancel_delayed_work_sync(&di->hall4_irq_work);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static void idtp9220_shutdown(struct i2c_client *client)
{
	struct idtp9220_device_info *di = i2c_get_clientdata(client);

	if (di->power_good_flag) {
		msleep(30);
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
	{.compatible = "idt,p9418"},
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
MODULE_DESCRIPTION("IDTP9220 Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
