/*				*/
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
#include <linux/usb/ucsi_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include "ps5169.h"

#define PS5169_DRIVER_NAME	"ps5169"
#define PULLUP_WORKER_DELAY_US	500000

static struct ps5169_info *g_info;
static int log_level = 1;
static bool ps5169_present_flag = false;

static const struct regmap_config ps5169_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
};

static int ps5169_write_reg(struct ps5169_info *info, u8 reg, u8 data)
{
	int ret = 0;

	if (!info->regmap)
		return ret;

	mutex_lock(&info->i2c_lock);
	ret = regmap_write(info->regmap, reg, data);
	if (ret < 0)
		ps5169_err("%s: failed-write, reg(0x%02X), ret(%d)\n",
				__func__, reg, ret);
	mutex_unlock(&info->i2c_lock);

	return ret;
}

static int ps5169_read_reg(struct ps5169_info *info, u8 reg, u8 *data)
{
	unsigned int temp;
	int ret = 0;

	if (!info->regmap)
		return ret;

	mutex_lock(&info->i2c_lock);
	ret = regmap_read(info->regmap, reg, &temp);
	if (ret >= 0)
		*data = (u16)temp;
	mutex_unlock(&info->i2c_lock);

	return ret;
}

static int ps5169_update_reg(struct ps5169_info *info, u8 reg, u8 data)
{
	u8 temp;
	int ret = 0;

	ret = ps5169_write_reg(info, reg, data);

	ret = ps5169_read_reg(info, reg, &temp);

	if (data != (u8)temp) {
		ps5169_err("%s: update reg:%02x err, wdata:%02x, rdata:%02x.\n",
				__func__, reg, data, temp);
		return -1;
	}

	return 0;
}

static bool ps5169_present_check(void)
{
	if (!g_info || !ps5169_present_flag) {
		ps5169_err("%s: false.\n", __func__);
		return false;
	}

	return true;
}

static void ps5169_set_config(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return;

	ret |= ps5169_update_reg(info, 0x9d, 0x80);

	msleep(10);

	ret |= ps5169_update_reg(info, 0x9d, 0x00);

	ret |= ps5169_update_reg(info, 0x40, 0x80);		//auto power down
	if(!info->host){
		ps5169_err("%s: ps5169 in device .\n", __func__);
		ret |= ps5169_update_reg(info, 0x04, 0x44);		//disable U1 status Rx_Det
		}
	else {
		ps5169_err("%s: ps5169 in host .\n", __func__);
	}

	ret |= ps5169_update_reg(info, 0xA0, 0x02);		//disable AUX channel

	ret |= ps5169_update_reg(info, 0x8d, 0x01);		//Fine tune LFPS swing

	ret |= ps5169_update_reg(info, 0x90, 0x01);		//Fine tune LFPS swing

	ret |= ps5169_update_reg(info, 0x51, 0x87);

	ret |= ps5169_update_reg(info, 0x50, 0x20);

	ret |= ps5169_update_reg(info, 0x54, 0x11);

	ret |= ps5169_update_reg(info, 0x5d, 0x66);

	ret |= ps5169_update_reg(info, 0x52, 0x50);		//DP EQ 8.5dB

	ret |= ps5169_update_reg(info, 0x55, 0x00);

	ret |= ps5169_update_reg(info, 0x56, 0x00);

	ret |= ps5169_update_reg(info, 0x57, 0x00);

	ret |= ps5169_update_reg(info, 0x58, 0x00);

	ret |= ps5169_update_reg(info, 0x59, 0x00);

	ret |= ps5169_update_reg(info, 0x5a, 0x00);

	ret |= ps5169_update_reg(info, 0x5b, 0x00);

	ret |= ps5169_update_reg(info, 0x5e, 0x06);

	ret |= ps5169_update_reg(info, 0x5f, 0x00);

	ret |= ps5169_update_reg(info, 0x60, 0x00);

	ret |= ps5169_update_reg(info, 0x61, 0x03);

	ret |= ps5169_update_reg(info, 0x65, 0x40);

	ret |= ps5169_update_reg(info, 0x66, 0x00);

	ret |= ps5169_update_reg(info, 0x67, 0x03);

	ret |= ps5169_update_reg(info, 0x75, 0x0c);

	ret |= ps5169_update_reg(info, 0x77, 0x00);

	ret |= ps5169_update_reg(info, 0x78, 0x7c);

	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
	else
		ps5169_info("%s: cfg set end.\n", __func__);
}

static void ps5169_enable_work(struct ps5169_info *info, int enable)
{

	int ret;

	if (!ps5169_present_check())
		return;

	if (IS_ERR_OR_NULL(info->ps5169_pinctrl) || IS_ERR_OR_NULL(info->ps5169_gpio_active) || IS_ERR_OR_NULL(info->ps5169_gpio_suspend)) {
		ps5169_err("%s: No pinctrl config specified\n", __func__);
		ret = PTR_ERR(info->dev);
		return;
	}

	if (enable) {
		ret = pinctrl_select_state(info->ps5169_pinctrl, info->ps5169_gpio_active);
		if (ret < 0) {
			ps5169_err("%s: fail to select pinctrl active rc=%d\n", __func__, ret);
			return;
		}
		ps5169_info("%s: select gpio_active.\n", __func__);
		msleep(50);
		ps5169_info("%s: true, set cfg.\n", __func__);
		ps5169_set_config(info);
	} else {
		ret = pinctrl_select_state(info->ps5169_pinctrl, info->ps5169_gpio_suspend);
		if (ret < 0) {
			ps5169_err("%s: fail to select pinctrl suspend rc=%d\n", __func__, ret);
			return;
		}
		ps5169_info("%s: select gpio_suspend.\n", __func__);
	}
}

static int ps5169_get_CCorientation(void)
{
	int ret = 0, cc = 0;

	ret = qti_battery_charger_get_prop("battery", USB_CC_ORIENTATION, &cc);
	if (ret < 0) {
		ps5169_err("%s: get cc orientation is fail.\n", __func__);
		return ret;
	}
	//ps5169_info("%s: cc orientation: %d.\n", __func__, cc);

	return cc;
}

static int ps5169_get_has_dp(void)
{
	int ret = 0, has_dp = 0;

	ret = qti_battery_charger_get_prop("battery", HAS_DP_PS5169, &has_dp);
	if (ret < 0) {
		ps5169_err("%s: get has_dp is fail.\n", __func__);
		return ret;
	}
	ps5169_info("%s: has_dp: %d.\n", __func__, has_dp);

	return has_dp;
}

static int ps5169_get_chipid_revision(struct ps5169_info *info)
{
	u8 chip_id_l, chip_id_h;
	u8 revision_l = 0, revision_h = 0;
	int ret = 0;

	ret |= ps5169_read_reg(info, REG_CHIP_ID_L, &chip_id_l);
	ret |= ps5169_read_reg(info, REG_CHIP_ID_H, &chip_id_h);
	ps5169_info("%s: Chip_ID: 0x%02x, 0x%02x", __func__, chip_id_h, chip_id_l);

	ret |= ps5169_read_reg(info, REG_REVISION_L, &revision_l);
	ret |= ps5169_read_reg(info, REG_REVISION_L, &revision_l);
	ps5169_info("%s: Revision: 0x%02x, 0x%02x", __func__, revision_h, revision_l);

	if ((chip_id_h == 0x69) && (chip_id_l == 0x87))  {
		ps5169_present_flag = true;
		ps5169_info("%s: ps5169_present_flag: true.\n", __func__);
	} else {
		ps5169_present_flag = false;
		ps5169_err("%s: chip id no match(%d), return: %d.\n", __func__, ps5169_present_flag, ret);
		return -1;
	}

	return 0;
}

void ps5169_get_chipcfg_and_modeselection(void)
{
	u8 cfg_mode, aux_enable, hpd_plug;
	int ret;

	if (!ps5169_present_check())
		return;

	ret = ps5169_read_reg(g_info, REG_CONFIG_MODE, &cfg_mode);
	ret = ps5169_read_reg(g_info, REG_AUX_ENABLE, &aux_enable);
	ret = ps5169_read_reg(g_info, REG_HPD_PLUG, &hpd_plug);

	if ((cfg_mode == 0xc0) || (cfg_mode == 0xd0)) {
		ps5169_info("%s: USB_ONLY_MODE.\n", __func__);
	} else if (((cfg_mode == 0xe0) || (cfg_mode == 0xf0)) && (aux_enable == 0x00) && (hpd_plug == 0x04)) {
		ps5169_info("%s: USB_DP_MODE.\n", __func__);
	} else if (((cfg_mode == 0xa0) || (cfg_mode == 0xb0)) && (aux_enable == 0x00) && (hpd_plug == 0x04)) {
		ps5169_info("%s: DP_ONLY_MODE.\n", __func__);
		if (!(ps5169_get_has_dp())) {
			ps5169_info("%s: WA.\n", __func__);
			g_info->op_mode = OP_MODE_NONE;
			ps5169_enable_work(g_info, 0);
		}
	}
}
EXPORT_SYMBOL(ps5169_get_chipcfg_and_modeselection);

static int ps5169_cfg_usb(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return -1;

	if (info->flip == 0)
		info->flip = ps5169_get_CCorientation();

	ps5169_info("%s: start.\n", __func__);
	if (info->flip == 1)
		ret |= ps5169_update_reg(info, 0x40, 0xc0);     //USB mode, TX1
	else if (info->flip == 2)
		ret |= ps5169_update_reg(info, 0x40, 0xd0);     //USB mode, TX2
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);

	ps5169_get_chipcfg_and_modeselection();
	return ret;
}

static int ps5169_config_flip(struct ps5169_info *info, int flip)
{
	if (!ps5169_present_check())
		return -1;

	info->flip = flip;
	ps5169_info("%s: flip:%d.\n", __func__, info->flip);
	return 0;
}


static int ps5169_config_dp_only_mode(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return -1;

	if (info->flip == 0)
		info->flip = ps5169_get_CCorientation();

	ps5169_info("%s: flip:%d.\n", __func__, info->flip);
	if (info->flip == 1)
		ret |= ps5169_update_reg(info, 0x40, 0xa0);		//DP mode, TX1
	else if (info->flip == 2)
		ret |= ps5169_update_reg(info, 0x40, 0xb0);		//DP mode, TX2

	ret |= ps5169_update_reg(info, 0xa0, 0x00);		//Enable AUX channel
	ret |= ps5169_update_reg(info, 0xa1, 0x04);		//HPD
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);

	ps5169_get_chipcfg_and_modeselection();
	return ret;
}

static int ps5169_config_usb_dp_mode(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return -1;

	if (info->flip == 0)
		info->flip = ps5169_get_CCorientation();

	ps5169_info("%s: flip:%d.\n", __func__, info->flip);
	if (info->flip == 1)
		ret |= ps5169_update_reg(info, 0x40, 0xe0);		//DP mode, TX1
	else if (info->flip == 2)
		ret |= ps5169_update_reg(info, 0x40, 0xf0);		//DP mode, TX2

	ret |= ps5169_update_reg(info, 0xa0, 0x00);		//Enable AUX channel
	ret |= ps5169_update_reg(info, 0xa1, 0x04);		//HPD
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);

	ps5169_get_chipcfg_and_modeselection();
	return ret;
}

static void ps5169_remove_usb(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return;

	info->flip = 0;
	info->cc_flag = 0;
	info->initCFG_flag = 0;

	ret |= ps5169_update_reg(info, 0x40, 0x80);		//auto power down

	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
}

static void ps5169_remove_dp(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return;

	info->flip = 0;

	ret |= ps5169_update_reg(info, 0x40, 0x80);		//DP mode, no FLIP
	ret |= ps5169_update_reg(info, 0xa0, 0x02);		//Disable AUX channel
	ret |= ps5169_update_reg(info, 0xa1, 0x00);		//HPD low
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
}

static void ps5169_remove_usb_dp(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return;

	info->flip = 0;
	info->cc_flag = 0;
	info->initCFG_flag = 0;

	ret |= ps5169_update_reg(info, 0x40, 0x80);		//DP mode, no FLIP
	ret |= ps5169_update_reg(info, 0xa0, 0x02);		//Disable AUX channel
	ret |= ps5169_update_reg(info, 0xa1, 0x00);		//HPD low
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
}

static void ps5169_channel_update(struct ps5169_info *info)
{
	int ret = 0;

	if (!ps5169_present_check())
		return;

	switch (info->op_mode) {
	case OP_MODE_USB:
		ret = ps5169_cfg_usb(info);
		break;
	case OP_MODE_USB_AND_DP:
		ret = ps5169_config_usb_dp_mode(info);
		break;
	case OP_MODE_DP:
		ret = ps5169_config_dp_only_mode(info);
		break;
	case OP_MODE_DEFAULT:
	default:
		return;
	}

	if (ret < 0) {
		ps5169_err("channel parameters update failure(%d).\n", ret);
		return;
	}

	ps5169_info("config channel parameters update success.\n");
	return;

}

void ps5169_notify_connect(bool host)
{

	if (!ps5169_present_check() || (g_info->op_mode == OP_MODE_DEFAULT) ||
	    (g_info->op_mode == OP_MODE_DP))
		return;

	/* if ucsi ppm can't return status with Connector Partner Changed
	 * bit set, redriver will not process the notification,
	 * but ucsi still start usb host/device mode,
	 * then redriver stay in disabled state, super speed (plus)
	 * will not work.
	 * fix should come from ucsi ppm, it is a enhancement here.
	 * TODO: redriver controlled by dwc3, remove ucsi notification
	 */
	if (g_info->op_mode != OP_MODE_DP &&  g_info->op_mode != OP_MODE_USB_AND_DP) {
		g_info->op_mode = OP_MODE_USB;
	}
	if (host)
	g_info->host =true;
	else g_info->host=false;

	ps5169_enable_work(g_info, 1);
	ps5169_info("connect op mode %s\n", OPMODESTR(g_info->op_mode));

	ps5169_channel_update(g_info);

}
EXPORT_SYMBOL(ps5169_notify_connect);

void ps5169_notify_disconnect(void)
{

	/* 1. no operation in recovery mode.
	 * 2. there is case for 4 lane display, first report usb mode,
	 * second call usb release super speed lanes,
	 * then stop usb host and call this disconnect,
	 * it should not disable chip.
	 * 3. if already disabled, no need to disable again.
	 */

	if (!ps5169_present_check() || (g_info->op_mode == OP_MODE_DEFAULT) ||
	    (g_info->op_mode == OP_MODE_NONE))
		return;
	ps5169_info("disconnect op mode %s\n", OPMODESTR(g_info->op_mode));

	switch (g_info->op_mode) {
	case OP_MODE_USB:
		ps5169_remove_usb(g_info);
		break;
	case OP_MODE_USB_AND_DP:
		ps5169_remove_usb_dp(g_info);
		break;
	case OP_MODE_DP:
		if(!(ps5169_get_CCorientation()))
			ps5169_remove_dp(g_info);
		else
			return;
		break;
	case OP_MODE_DEFAULT:
	default:
		return;
	}

	g_info->op_mode = OP_MODE_NONE;

	if (!(ps5169_get_has_dp()))
		ps5169_enable_work(g_info, 0);

	return;
}
EXPORT_SYMBOL(ps5169_notify_disconnect);

void ps5169_release_usb_lanes(int lanes)
{

	if (!ps5169_present_check() || g_info->op_mode == OP_MODE_DP)
		return;

	ps5169_info("display notify  lane mode: %d\n", lanes);
	if (lanes == 2)
		g_info->op_mode = OP_MODE_USB_AND_DP;
	else if (lanes == 4)
		g_info->op_mode = OP_MODE_DP;
	else
		g_info->op_mode = OP_MODE_USB;

	ps5169_channel_update(g_info);

	return;
}
EXPORT_SYMBOL(ps5169_release_usb_lanes);


/* For tuning usb3.0 eye */
static void ps5169_set_eq0_tx(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data = 0xff;

	if (!ps5169_present_check())
		return;

	if ((level > 7) || (level < 0)) {
		ps5169_err("%s: level err:%d.\n", __func__, level);
		return;
	}

	data &= 0x8F;
	level_tmp = data | (level << 4);
	ps5169_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x50, level_tmp);
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
}

static int ps5169_get_eq0_tx(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level = 0xff;

	if (!ps5169_present_check())
		return 0;

	ret |= ps5169_read_reg(info, 0x50, &data);

	level = (data & 0x70) >> 4;

	return level;
}

static void ps5169_set_eq1_tx(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data = 0xff;

	if (!ps5169_present_check())
		return;

	if ((level > 7) || (level < 0)) {
		ps5169_err("%s:level err:%d.\n", __func__, level);
		return;
	}

	data &= 0x8F;
	level_tmp = data | (level << 4);
	ps5169_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x5D, level_tmp);
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
}

static int ps5169_get_eq1_tx(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level = 0xff;

	if (!ps5169_present_check())
		return 0;

	ret |= ps5169_read_reg(info, 0x5D, &data);

	level = (data & 0x70) >> 4;

	return level;
}

static void ps5169_set_eq2_tx(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data = 0xff;

	if (!ps5169_present_check())
		return;

	if ((level > 15) || (level < 0)) {
		ps5169_err("%s: level err:%d.\n", __func__, level);
		return;
	}

	data &= 0x0F;
	level_tmp = data | (level << 4);
	ps5169_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x54, level_tmp);
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
}

static int ps5169_get_eq2_tx(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level = 0xff;

	if (!ps5169_present_check())
		return 0;

	ret |= ps5169_read_reg(info, 0x54, &data);

	level = (data & 0xf0) >> 4;

	return level;
}

static void ps5169_set_tx_gain(struct ps5169_info *info, int level)
{
	int ret = 0;
	u8 level_tmp, data = 0xff;

	if (!ps5169_present_check())
		return;

	if ((level > 1) || (level < 0)) {
		ps5169_err("%s:level err:%d.\n", __func__, level);
		return;
	}

	data &= 0xFE;
	level_tmp = data | level;
	ps5169_err("%s: level:%d, level_tmp:%02x.\n", __func__, level, level_tmp);

	ret |= ps5169_update_reg(info, 0x5C, level_tmp);
	if (ret < 0)
		ps5169_err("%s: crc err.\n", __func__);
}

static int ps5169_get_tx_gain(struct ps5169_info *info)
{
	int ret = 0;
	u8 data, level = 0xff;

	if (!ps5169_present_check())
		return 0;

	ret |= ps5169_read_reg(info, 0x5C, &data);

	level = data & 0x01;
	ps5169_get_CCorientation();

	return level;
}

static ssize_t tx_gain_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);
	int ret = 0;

	ret = ps5169_get_tx_gain(info);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ret);

}

static ssize_t tx_gain_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	ps5169_set_tx_gain(info, val);

	return count;

}
static CLASS_ATTR_RW(tx_gain);

static ssize_t eq0_tx_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);
	int ret = 0;

	ret = ps5169_get_eq0_tx(info);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ret);

}

static ssize_t eq0_tx_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	ps5169_set_eq0_tx(info, val);

	return count;

}
static CLASS_ATTR_RW(eq0_tx);

static ssize_t eq1_tx_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);
	int ret = 0;

	ret = ps5169_get_eq1_tx(info);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ret);

}

static ssize_t eq1_tx_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	ps5169_set_eq1_tx(info, val);

	return count;

}
static CLASS_ATTR_RW(eq1_tx);

static ssize_t eq2_tx_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);
	int ret = 0;

	ret = ps5169_get_eq2_tx(info);

	return scnprintf(buf, PAGE_SIZE, "%u\n", ret);

}

static ssize_t eq2_tx_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	ps5169_set_eq2_tx(info, val);

	return count;

}
static CLASS_ATTR_RW(eq2_tx);

static ssize_t cfg_flip_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	ps5169_config_flip(info, val);

	return count;

}
static CLASS_ATTR_WO(cfg_flip);


static ssize_t move_usb_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val)
		ps5169_remove_usb(info);

	return count;

}
static CLASS_ATTR_WO(move_usb);

static ssize_t move_dp_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val)
		ps5169_remove_dp(info);

	return count;

}
static CLASS_ATTR_WO(move_dp);

static ssize_t move_Usbdp_store(struct class *c,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int val = 0;
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	if (val)
		ps5169_remove_usb_dp(info);

	return count;

}
static CLASS_ATTR_WO(move_Usbdp);

static ssize_t cc_orientation_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct ps5169_info *info = container_of(c, struct ps5169_info,
						ps_class);
	int ret = 0;

	if (gpio_is_valid(info->cc_gpio)) {
		ret = gpio_get_value(info->cc_gpio);

		ps5169_info("%s: Using gpio way get cc orientation: %d.\n", __func__, ret);
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", ret);

}
static CLASS_ATTR_RO(cc_orientation);

static struct attribute *ps5169_class_attrs[] = {
	&class_attr_tx_gain.attr,
	&class_attr_eq0_tx.attr,
	&class_attr_eq1_tx.attr,
	&class_attr_eq2_tx.attr,
	&class_attr_cfg_flip.attr,
	&class_attr_move_usb.attr,
	&class_attr_move_dp.attr,
	&class_attr_move_Usbdp.attr,
	&class_attr_cc_orientation.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ps5169_class);

static int ps5169_parse_dt(struct ps5169_info *info)
{
	struct device_node *node = info->dev->of_node;

	if (!node) {
		ps5169_err("device tree node missing\n");
		return -EINVAL;
	}

	info->enable_gpio = of_get_named_gpio(node, "mi,ps5169-enable", 0);
	if ((!gpio_is_valid(info->enable_gpio)))
		return -EINVAL;

	info->cc_gpio = of_get_named_gpio(node, "qcom,cc_orientatoin", 0);
	if ((!gpio_is_valid(info->cc_gpio)))
		return -EINVAL;


	ps5169_info("%s: Using-1 gpio way get cc orientation: %d.\n", __func__, gpio_get_value(info->cc_gpio));
/*	if (gpio_request(info->cc_gpio, "cc-direction")) {
		ps5169_err("%s: unable to request cc-direction gpio\n", __func__);
		return -EINVAL;
	}*/

	return 0;
}

static int ps5169_gpio_init(struct ps5169_info *info)
{
	int ret = 0;

	info->ps5169_pinctrl = devm_pinctrl_get(info->dev);
	if (IS_ERR_OR_NULL(info->ps5169_pinctrl)) {
		ps5169_err("%s: No pinctrl config specified\n", __func__);
		ret = PTR_ERR(info->dev);
		return ret;
	}

	info->ps5169_gpio_active =
		pinctrl_lookup_state(info->ps5169_pinctrl, "ps5169_active");
	if (IS_ERR_OR_NULL(info->ps5169_gpio_active)) {
		ps5169_err("%s: No active config specified\n", __func__);
		ret = PTR_ERR(info->ps5169_gpio_active);
		return ret;
	}
	info->ps5169_gpio_suspend =
		pinctrl_lookup_state(info->ps5169_pinctrl, "ps5169_suspend");
	if (IS_ERR_OR_NULL(info->ps5169_gpio_suspend)) {
		ps5169_err("%s: No suspend config specified\n", __func__);
		ret = PTR_ERR(info->ps5169_gpio_suspend);
		return ret;
	}
	ret = pinctrl_select_state(info->ps5169_pinctrl, info->ps5169_gpio_active);
	if (ret < 0) {
		ps5169_err("%s: fail to select pinctrl active rc=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int ps5169_ucsi_notifier(struct notifier_block *nb,
  		unsigned long action, void *data)
{
	struct ps5169_info *info =
			container_of(nb, struct ps5169_info, ps5169_nb);

	u8 cci = 0;
	cci = *(u8*)(data);

	ps5169_info("%s: cci 0x%08x\n", __func__, cci);
	msleep(50);
	ps5169_info("%s: true, set cfg.\n", __func__);
	ps5169_set_config(info);
	return 0;
}

static void ps5169_gadget_pullup_work(struct work_struct *w)
{
	if (!ps5169_present_check())
		return ;
	ps5169_notify_disconnect();
	usleep_range(1000, 1500);
	ps5169_notify_connect(false);
	g_info->work_ongoing = false;
}

 int ps5169_gadget_pullup_enter( int is_on)
{
	u64 time = 0;
	if (!ps5169_present_check())
		return -EINVAL;
	ps5169_err("%s: mode %s, %d, %d\n", __func__,
	OPMODESTR(g_info->op_mode), is_on, g_info->work_ongoing);
	if (g_info->op_mode != OP_MODE_USB)
		return -EINVAL;
	if (!is_on)
		return 0;
	while (g_info->work_ongoing) {
		udelay(1);
		if (time++ > PULLUP_WORKER_DELAY_US) {
			dev_err(g_info->dev, "pullup timeout\n");
			break;
		}
	}
	ps5169_err("pull-up disable work took %llu us\n", time);
	return 0;
}
EXPORT_SYMBOL(ps5169_gadget_pullup_enter);


 int ps5169_gadget_pullup_exit( int is_on)
{
	if (!ps5169_present_check())
		return -EINVAL;;
	ps5169_err("%s: mode %s, %d, %d\n", __func__,
		OPMODESTR(g_info->op_mode), is_on, g_info->work_ongoing);

	if (g_info->op_mode != OP_MODE_USB)
		return -EINVAL;

	if (is_on)
		return 0;

	g_info->work_ongoing = true;
	queue_work(g_info->pullup_wq, &g_info->pullup_work);
	return 0;
}
EXPORT_SYMBOL(ps5169_gadget_pullup_exit);


static int retry_count = 0;
static int ps5169_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = 0;
	struct ps5169_info *info;

	ps5169_info("%s: =/START-PROBE retry: %d/=\n", __func__, retry_count);

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	info->pullup_wq = alloc_workqueue("%s:pullup",
			WQ_UNBOUND | WQ_HIGHPRI, 0,
 				dev_name(&client->dev));
	if (!info->pullup_wq) {
		ps5169_err("Failed to create pullup workqueue\n");
		return -ENOMEM;
	}

	mutex_init(&info->i2c_lock);
	info->name = PS5169_DRIVER_NAME;
	info->client  = client;
	info->dev = &client->dev;
	ps5169_present_flag = false;
	info->pre_ps_enable = 0;
	info->ps_enable = 0;
	info->flip = 0;
	info->cc_flag = 0;
	info->initCFG_flag = 0;
  	info->host = false;
	i2c_set_clientdata(client, info);


	info->regmap = devm_regmap_init_i2c(client, &ps5169_regmap_config);
	if (IS_ERR(info->regmap)) {
		ps5169_err("%s: failed to initialize regmap\n", __func__);
		return PTR_ERR(info->regmap);
	}

	ret = ps5169_parse_dt(info);
	if (ret < 0) {
		ps5169_err("%s: parse dt error [%d]\n", __func__, ret);
		goto cleanup;
	}

	ret = ps5169_gpio_init(info);
	if (ret < 0) {
		ps5169_err("%s: gpio init error [%d]\n", __func__, ret);
		goto cleanup;
	}

	/*ret = ps5169_init_psy(info);
	if (ret < 0) {
		ps5169_err("%s: psy init error [%d]\n", __func__, ret);
		goto cleanup;
	}*/

	ret = ps5169_get_chipid_revision(info);
	if (ret < 0) {
		if (retry_count < 3) {
			ps5169_err("%s: chipid i2c err, probe retry count:%d.\n",
					__func__, retry_count);
			retry_count++;
			msleep(100);
			return -EPROBE_DEFER;
		} else {
			ps5169_err("%s: chipid i2c err, retry count max, no find ps5169.\n", __func__);
			goto cleanup;
		}
	}
	INIT_WORK(&info->pullup_work, ps5169_gadget_pullup_work);
	ps5169_set_config(info);
	ps5169_info("%s: set cfg.\n", __func__);

	info->ps_class.name = "qcom-ps5169";
	info->ps_class.class_groups = ps5169_class_groups;
	ret = class_register(&info->ps_class);
	if (ret < 0) {
		ps5169_err("%s: Failed to create battery_class rc=%d.\n", __func__, ret);
		goto cleanup;
	}

	info->ps5169_nb.notifier_call = ps5169_ucsi_notifier;

	//INIT_DELAYED_WORK(&info->ps_en_work, ps5169_enable_work);
	g_info = info;

	ps5169_info("%s: success probe!\n", __func__);

	return 0;


cleanup:
	i2c_set_clientdata(client, NULL);

	return ret;
}

static int ps5169_remove(struct i2c_client *client)
{
	if (!ps5169_present_check())
		return -EINVAL;
	ps5169_err("%s: driver remove\n", __func__);
	ps5169_present_flag = false;
	//cancel_delayed_work_sync(&info->ps_en_work);
	//gpio_free(info->enable_gpio);
	//class_destroy(&info->ps_class);
	g_info->work_ongoing = false;
	i2c_set_clientdata(client, NULL);
	return 0;
}


static const struct of_device_id ps5169_dt_match[] = {
	{ .compatible = "mi,ps5169" },
	{ },
};
MODULE_DEVICE_TABLE(of, ps5169_dt_match);

static const struct i2c_device_id ps5169_id[] = {
	{ "ps5169", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ps5169_id);

static struct i2c_driver ps5169_driver = {
	.driver   = {
			.name = PS5169_DRIVER_NAME,
			.of_match_table = of_match_ptr(ps5169_dt_match),
			},
	.probe    = ps5169_probe,
	.remove   = ps5169_remove,
	.id_table = ps5169_id,
};

static int __init ps5169_init(void)
{
	int ret = 0;
	ps5169_info("%s.\n", __func__);
	ret = i2c_add_driver(&ps5169_driver);
	if (ret)
		ps5169_err("ps5169 i2c driver init failed!\n");

	return ret;
}
static void __exit ps5169_exit(void)
{
	i2c_del_driver(&ps5169_driver);
}

module_init(ps5169_init);
module_exit(ps5169_exit);

MODULE_DESCRIPTION("PS5169 driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.1");

