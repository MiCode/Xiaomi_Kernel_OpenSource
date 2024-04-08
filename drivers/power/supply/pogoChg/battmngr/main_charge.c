#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/battmngr/battmngr_notifier.h>
#include <linux/battmngr/battmngr_voter.h>
#include <linux/regulator/driver.h>
#include <../extSOC/inc/virtual_fg.h>
#include <linux/battmngr/xm_charger_core.h>
#include <../extSOC/inc/nano_macro.h>

#define main_err(fmt, ...)							\
do {										\
	if (chg_log_level >= 0)							\
		printk(KERN_ERR "[main_charge] " fmt, ##__VA_ARGS__);	\
} while (0)

#define main_info(fmt, ...)							\
do {										\
	if (chg_log_level >= 1)							\
		printk(KERN_ERR "[main_charge] " fmt, ##__VA_ARGS__);	\
} while (0)

#define main_dbg(fmt, ...)							\
do {										\
	if (chg_log_level >= 2)							\
		printk(KERN_ERR "[main_charge] " fmt, ##__VA_ARGS__);	\
} while (0)

#define BQ_FCC_VOTER	"BQ_FCC_VOTER"
#define BQ_ICL_VOTER	"BQ_ICL_VOTER"
#define BQ_FV_VOTER	"BQ_FV_VOTER"
#define USER_VOTER	"USER_VOTER"

#define POGO_TERM_VOLT 4480*1000

struct xm_config {
	int		charge_current;
	int		term_current;
};

enum Scene {
	FOREIGN = -1,
	NONE,
	PAD_KEY,
	KEY_TYPEC_PAD,
	PAD_KEY_TYPEC,
	PAD_CAR,
};

struct mainchg {
	struct device *dev;
	struct i2c_client *client;

	int	vbus_type;
	int	charge_type;
	bool	vbat_prot_flag;
	struct	xm_config	cfg;
	struct delayed_work first_irq_work;
	struct work_struct irq_work;
	struct work_struct adapter_in_work;
	struct work_struct adapter_out_work;
	struct delayed_work monitor_work;
	struct delayed_work handler_mcu_work;
	struct delayed_work handler_adsp_work;

	struct power_supply *batt_psy;
	struct power_supply *usb_psy;

	struct votable *fcc_votable;
	struct votable *usb_icl_votable;
	struct votable *fv_votable;

	int old_type;
	int input_suspend;
	u8 power_state;
	unsigned int kb_ovp_en_gpio;
	unsigned int kb_boost_5v_gpio;
	struct pinctrl *kb_pinctrl;
	struct pinctrl_state *kb_gpio_active;
	struct pinctrl_state *kb_gpio_suspend;
	struct battmngr_device* battmg_dev;
	bool flag;
};

enum xm_vbus_type {
	XM_VBUS_NONE,
	XM_VBUS_USB_FLOAT = 4,
	XM_VBUS_USB_SDP = 6,
	XM_VBUS_USB_CDP = 9, /*CDP for bq25890, Adapter for bq25892*/
	XM_VBUS_USB_DCP = 10,
	XM_VBUS_USB_QC_5v = 16,
	XM_VBUS_USB_QC2,
	XM_VBUS_USB_QC2_12v,
	XM_VBUS_USB_QC3 = 20,
	XM_VBUS_UNKNOWN,
	XM_VBUS_NONSTAND,
	XM_VBUS_OTG,
	XM_VBUS_TYPE_NUM,
};

static struct mainchg* g_bq = NULL;
static struct read_mcu_data* g_mcu_data = NULL;

static int plug_flag;
static u8 dc_in;
static u8 kb_in;
static u8 currState;
static u8 object;

bool bq_check_vote(struct mainchg *bq)
{
	bool vote_flag;

	if (!bq->usb_icl_votable)
		bq->usb_icl_votable = find_votable("ICL");

	if (!bq->fcc_votable)
		bq->fcc_votable = find_votable("FCC");

	if (!bq->fv_votable)
		bq->fv_votable = find_votable("FV");

	if (!bq->usb_icl_votable || !bq->fcc_votable || !bq->fv_votable) {
		vote_flag = false;
	} else
		vote_flag = true;
	main_err("%s: vote_flag %d\n", __func__, vote_flag);

	return vote_flag;
}

static int charger_enable_keyboard_ovp(struct mainchg *bq, int enable)
{
	int ret = 0;

	if (gpio_is_valid(bq->kb_ovp_en_gpio)) {

		ret = gpio_direction_output(bq->kb_ovp_en_gpio, enable);
		if (ret) {
			main_err("%s: cannot set direction for kb_ovp_en_gpio[%d]\n", __func__,
					bq->kb_ovp_en_gpio);
			return -EINVAL;
		}
	} else {
		main_err("%s: unable to set kb_ovp_en_gpio\n");
		return -EINVAL;
	}

	return ret;
}
static int charger_enable_keyboard_boost(struct mainchg *bq, int enable)
{
	int ret = 0;

	main_info("%s: test-2 %d\n", __func__, ret);
	if (enable) {
		ret = charger_enable_keyboard_ovp(bq, enable);
		if (ret < 0) {
			main_err("%s: open kb_ovp_en_gpio[%d] is failed\n", __func__,
					bq->kb_ovp_en_gpio);
			return -EINVAL;
		}
	}

	main_info("%s: test-3 %d\n", __func__, ret);
	if (gpio_is_valid(bq->kb_boost_5v_gpio)) {

		ret = gpio_direction_output(bq->kb_boost_5v_gpio, enable);
		if (ret) {
			main_err("%s: cannot set direction for kb_boost_5v_gpio[%d], enable:%d\n", __func__,
					bq->kb_boost_5v_gpio, enable);
			return -EINVAL;
		}
	} else {
		main_err("%s: unable to set kb_boost_5v_gpio\n");
		return -EINVAL;
	}

	if (!enable) {
		ret = charger_enable_keyboard_ovp(bq, enable);
		if (ret < 0) {
			main_err("%s: close kb_ovp_en_gpio[%d] is failed\n", __func__,
					bq->kb_ovp_en_gpio);
			return -EINVAL;
		}
	}

	return ret;
}


int charger_process_event_irq(struct battmngr_notify *noti_data)
{
	int rc = 0;

	main_info("%s: msg_type %d, value:%d\n", __func__, noti_data->irq_msg.irq_type, noti_data->irq_msg.value);
	switch (noti_data->irq_msg.irq_type) {
	case DCIN_IRQ:
		g_battmngr_noti->mainchg_msg.dc_plugin = 1;
		dc_in = noti_data->irq_msg.value;

		if (g_bq) {
			schedule_delayed_work(&g_bq->handler_adsp_work, msecs_to_jiffies(200));
			main_info("%s: start irq_work\n", __func__);
		} else
			main_info("%s g_bq is null.\n", __func__);
		break;
	case CHARGER_DONE_IRQ:
		g_battmngr_noti->mainchg_msg.chg_done = noti_data->irq_msg.value;
		break;
	case RECHARGE_IRQ:
		g_battmngr_noti->fg_msg.recharge_flag = noti_data->irq_msg.value;
		break;

	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(charger_process_event_irq);

int charger_process_event_mcu(struct battmngr_notify *noti_data)
{
	int rc = 0;
	u8 typec_chg = 0;

	main_info("%s: msg_type %d, %d\n", __func__, noti_data->mcu_msg.msg_type, noti_data->mcu_msg.kb_attach);
	object = noti_data->mcu_msg.object_type;

	switch (noti_data->mcu_msg.msg_type) {
	case BATTMNGR_MSG_MCU_TYPE:
		if (object == 0x38) {
			g_mcu_data = &noti_data->mcu_msg.mcu_data;
		} else {
			g_mcu_data = &noti_data->mcu_msg.mcu_data;
			switch (noti_data->mcu_msg.mcu_data.addr)
			{
			case 0x10:
				g_mcu_data->typec_state = noti_data->mcu_msg.mcu_data.typec_state;
				break;
			case 0x15:
				g_mcu_data->vout_low = noti_data->mcu_msg.mcu_data.vout_low;
				break;
			case 0x16:
				g_mcu_data->vout_high = noti_data->mcu_msg.mcu_data.vout_high;
				break;
			default:
				break;
			}
			main_info("Address:%02x, typec_state:%02x, vout_low:%02x, vout_high:%02x\n",
                                 noti_data->mcu_msg.mcu_data.addr, g_mcu_data->typec_state, g_mcu_data->vout_low, g_mcu_data->vout_high);
		}
		main_info("%s: object %02x\n", __func__, object);

		typec_chg = g_mcu_data->typec_state & BIT(0);
		break;
	case BATTMNGR_MSG_MCU_BOOST:
		if (noti_data->mcu_msg.kb_attach && object == 0x38) {
			if (Nanosic_ADSP_Read_Charge_Status() < 0) {
				main_err("%s: read charge data is fail.\n", __func__);
			}
		}
		return rc;
		kb_in = noti_data->mcu_msg.kb_attach;
		if (g_bq)
			schedule_delayed_work(&g_bq->handler_mcu_work, msecs_to_jiffies(100));
		else
			main_info("%s g_bq is null.\n", __func__);

		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(charger_process_event_mcu);

int xm_set_input_volt_limit(int volt)
{
	int ret = -1;
	uint8_t val = 0xd0;

	if (object == 0x38) {
		val = volt / 100;
		ret = Nanosic_ADSP_Set_Charge_Status_Single(0x0C, 1, &val, sizeof(val));
	} else {
		ret = Nanosic_ADSP_Set_Charge_Status_Single(0x01, 1, &val, sizeof(val));
	}

	if (ret < 0) {
		main_err("%s: set input volt is fail\n", __func__);
	}
	main_info("%s: set input volt: %d, ret:%d\n", __func__, volt, ret);

	return ret;
}

int xm_set_term_current(struct mainchg *bq, int curr)
{
	int ret = 0;

	if (check_qti_ops(&bq->battmg_dev)) {
		ret = battmngr_qtiops_set_term_cur(bq->battmg_dev, curr);
		if (ret < 0)
			main_err("%s: set term curr is fail\n", __func__);

	} else {
		main_err("%s: qti ops is null\n", __func__);
		ret = -1;
	}

	return ret;
}

int xm_enable_charger(void)
{
	int ret = 0;

    if (check_g_bcdev_ops()) {
        ret = qti_enale_charge(true);

        if (ret < 0) {
            main_err("%s enable charge is fail. \n", __func__);
            return ret;
        }
        main_info("%s enable charge is successful. \n", __func__);
    } else {
        main_err("%s check_g_bcdev_ops is null. \n", __func__);
        return -1;
    }
	return ret;
}

int xm_disable_charger(void)
{
	int ret = 0;

    if (check_g_bcdev_ops()) {
        ret = qti_enale_charge(false);

        if (ret < 0) {
            main_err("%s disable charge is fail. \n", __func__);
            return ret;
        }
        main_info("%s disable charge is successful. \n", __func__);
    } else {
        main_err("%s check_g_bcdev_ops is null. \n", __func__);
        return -1;
    }
	return ret;
}

static int xm_get_vbus_type(struct mainchg *bq)
{
	u8 pd_flag = 0, dpdm_in_ready = 0;

	if (g_mcu_data) {
		if (object == 0x38) {
			pd_flag = (g_mcu_data->typec_state & BIT(2));
			dpdm_in_ready = (g_mcu_data->dpdm_in_state & BIT(7));
		} else {
			pd_flag = (g_mcu_data->typec_state & 0x10);
		}

		if (pd_flag) {
			g_battmngr_noti->pd_msg.pd_active = pd_flag ? 1 : 0;
		}

		if (!dpdm_in_ready) {
			main_err("%s need wait finish bc1.2 . \n", __func__);
			return XM_VBUS_UNKNOWN;
		}

		bq->vbus_type = g_mcu_data->dpdm0_state;
	} else {
		main_err("%s gobal mcu data is null. \n", __func__);
		return 0;
	}

	return bq->vbus_type;
}

int xm_get_chg_usb_type(struct mainchg *bq)
{
	u8 val;
	int type;

	val = xm_get_vbus_type(bq);
	type = (int)val;
	main_info("%s type=%d, old_type=%d \n", __func__, type, bq->old_type);

	if (object == 0x3B) {
		if (g_battmngr_noti->pd_msg.pd_active) {
			bq->charge_type = POWER_SUPPLY_USB_TYPE_PD;
		} else
			bq->charge_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	main_info("%s charge_type=%d\n", __func__, bq->charge_type);

	return 0;
}

static void xm_adapter_in_workfunc(struct work_struct *work)
{
	struct mainchg *bq = container_of(work, struct mainchg, adapter_in_work);

	if(!bq_check_vote(bq))
		return;

	if (xm_enable_charger() < 0) {
		return;
	}

	Nanosic_ADSP_Control_Charge_Power(true);
	if (g_battmngr_noti->pd_msg.pd_active) {
		main_info("%s:PD plugged in\n", __func__);
	} else {
		main_info("%s:other adapter plugged in,vbus_type is %d\n", __func__, bq->vbus_type);
	}

	cancel_delayed_work_sync(&bq->monitor_work);
	schedule_delayed_work(&bq->monitor_work, 0);
}

static void xm_adapter_out_workfunc(struct work_struct *work)
{
	struct mainchg *bq = container_of(work, struct mainchg, adapter_out_work);

	Nanosic_ADSP_Control_Charge_Power(false);
	bq->vbat_prot_flag = false;
	bq->flag = false;

	if (xm_disable_charger() < 0) {
		return;
	}
	vote(bq->usb_icl_votable, BQ_ICL_VOTER, true, 0);
	vote(bq->fcc_votable, BQ_FCC_VOTER, false, 0);
	vote(bq->fv_votable, BQ_FV_VOTER, false, 0);
	vote(bq->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
	vote(bq->fv_votable, JEITA_VOTER, false, 0);
	vote(bq->fcc_votable, JEITA_VOTER, false, 0);
}

#define HOLDER_ICL_MAX	2500000
static void xm_monitor_workfunc(struct work_struct *work)
{
	struct mainchg *bq = container_of(work, struct mainchg, monitor_work.work);
	static int ibus_limit = 500;
	static int ibat_limit = 500;
	static int last_chg_type;
	int rc, volt_now;
	union power_supply_propval pval = {0, };

	if(!bq_check_vote(bq))
		return;

	xm_set_input_volt_limit(9000);
	if (!bq->flag) {
		main_info("%s:first set current, delay 1s\n", __func__);
		msleep(1000);
		bq->flag = true;
	}

	ibus_limit = HOLDER_ICL_MAX;
	vote(bq->usb_icl_votable, BQ_ICL_VOTER, true, ibus_limit);

	ibat_limit = 7000*1000;
	vote(bq->fcc_votable, BQ_FCC_VOTER, true, ibat_limit);

	if (object == 0x38)
		Nanosic_ADSP_Read_Charge_Status();
	else {
		main_info("%s:check volt and curr! \n", __func__);
		Nanosic_ADSP_Read_Charge_Status_Single(0x13);
		msleep(5);
		Nanosic_ADSP_Read_Charge_Status_Single(0x14);
		msleep(5);

		Nanosic_ADSP_Read_Charge_Status_Single(0x1e);
		msleep(5);
		Nanosic_ADSP_Read_Charge_Status_Single(0x1f);
		msleep(5);

		Nanosic_ADSP_Read_Charge_Status_Single(0x01);
		msleep(5);

		Nanosic_ADSP_Read_Charge_Status_Single(0x12);
		msleep(5);
	}

	rc = power_supply_get_property(bq->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
			&pval);
	if (rc < 0) {
		main_info("%s: Get battery voltage failed, rc=%d\n", __func__, rc);
		return;
	}
	volt_now = pval.intval;
	if (volt_now >= POGO_TERM_VOLT && !bq->vbat_prot_flag) {
		bq->vbat_prot_flag = true;
		vote(bq->fv_votable, BQ_FV_VOTER, true, POGO_TERM_VOLT - 10*1000);
		main_info("%s: volt_now=%d, Voltage overvoltage, down 10mv\n", __func__, volt_now);
	}

	last_chg_type = bq->charge_type;

	main_info("%s:pd_active=%d ibus_limit=%d, ibat_limit:%d\n", __func__, g_battmngr_noti->pd_msg.pd_active, ibus_limit, ibat_limit);

	schedule_delayed_work(&bq->monitor_work, msecs_to_jiffies(3000));
}

static void xm_charger_irq_workfunc(struct work_struct *work)
{
	struct mainchg *bq = container_of(work, struct mainchg, irq_work);
	static int last_chg_type;
	static int last_plug_in;
    int  pWlsPluginSt = 0;

	int input_volt = 0, input_watt = 0;
	u8 typec_chg = 0, pd_flag = 0;
	u8 dpdm_in_ready = 0;
	int protocol_volt = 0, protocol_curr = 0, vbus_mon = 0;
	int typec_curr = 0;

	if (g_mcu_data) {
		if (object == 0x38) {
			input_volt = g_mcu_data->input_volt;
			input_watt = g_mcu_data->input_watt;
			typec_chg = (g_mcu_data->typec_state & BIT(0));
			pd_flag = (g_mcu_data->typec_state & BIT(2));
			dpdm_in_ready = (g_mcu_data->dpdm_in_state & BIT(7));
			protocol_volt = g_mcu_data->protocol_volt;
			protocol_curr = g_mcu_data->protocol_curr;
			vbus_mon = g_mcu_data->vbus_mon;
			typec_curr = g_mcu_data->typec_curr;
			bq->vbus_type = g_mcu_data->dpdm0_state;
		} else {
			if (dc_in) {
				typec_chg = 1;
				pd_flag = 1;
			} else {
				typec_chg = 0;
				pd_flag = 0;
			}
		}
	}

	g_battmngr_noti->pd_msg.pd_active = pd_flag ? 1 : 0;;
    pWlsPluginSt = dc_in; // the flag from adsp

    // read typec-vbus value
    bq->power_state = typec_chg;// type_state(0x11) --> CHARGE

	main_info("%s: power_good=%d, vbus_type=%d, dc_in=%d\n", __func__, bq->power_state, bq->vbus_type, dc_in);

	if (!pWlsPluginSt) {
		cancel_delayed_work_sync(&bq->monitor_work);
		msleep(10);
		bq->old_type = XM_VBUS_NONE;
		bq->vbus_type = XM_VBUS_NONE;
		bq->power_state = 0;
		plug_flag = 0;
		main_info("%s:adapter removed\n", __func__);
		g_battmngr_noti->mainchg_msg.chg_plugin = 0;
		g_battmngr_noti->mainchg_msg.chg_done = 0;
		cancel_delayed_work_sync(&bq->monitor_work);
		schedule_work(&bq->adapter_out_work);
	} else if (bq->power_state
		&& (g_battmngr_noti->pd_msg.pd_active || bq->vbus_type != XM_VBUS_NONE)
		&& pWlsPluginSt) {
		main_info("%s:adapter plugged in\n", __func__);
		g_battmngr_noti->mainchg_msg.chg_plugin = 1;
		schedule_work(&bq->adapter_in_work);
	}

	xm_get_chg_usb_type(bq);
	if ((last_chg_type != bq->charge_type)
		|| (last_plug_in != g_battmngr_noti->mainchg_msg.chg_plugin)) {
		mutex_lock(&g_battmngr_noti->notify_lock);
		g_battmngr_noti->mainchg_msg.chg_type = bq->charge_type;
		g_battmngr_noti->mainchg_msg.msg_type = BATTMNGR_MSG_MAINCHG_TYPE;
		battmngr_notifier_call_chain(BATTMNGR_EVENT_MAINCHG, g_battmngr_noti);
		mutex_unlock(&g_battmngr_noti->notify_lock);

		mutex_lock(&g_battmngr_noti->notify_lock);
		battmngr_notifier_call_chain(BATTMNGR_EVENT_FG, g_battmngr_noti);
		mutex_unlock(&g_battmngr_noti->notify_lock);
	}
	last_chg_type = bq->charge_type;
	last_plug_in = g_battmngr_noti->mainchg_msg.chg_plugin;

	return;
}

static void xm_handler_mcu_workfunc(struct work_struct *work)
{
	struct mainchg *bq = container_of(work, struct mainchg, handler_mcu_work.work);
	int rc = 0, retry = 0;

	if (kb_in) {
		if (dc_in) {
			currState = KEY_TYPEC_PAD;
			main_info("%s: detect KEY_TYPEC_PAD scene.\n", __func__);
			return;
		} else {
			do {
				if (!check_g_bcdev_ops()) {
					msleep(50);
					main_err("%s: check_g_bcdev_ops is null, wait retry %d\n", __func__, retry);
					if (retry++ > 10)
						break;
					continue;
				}

				retry = 0;
				rc = sc8651_wpc_gate_set(0);
				main_info("%s: test wpc.\n", __func__);
				if (rc < 0) {
					main_err("%s: close sc8651 wpc gate is failed %d\n", __func__, rc);
					return;
				}
			} while(!check_g_bcdev_ops());
			rc = charger_enable_keyboard_boost(bq, true);
			currState = PAD_KEY;
			main_info("%s: detect PAD_KEY scene.\n", __func__);
			main_info("%s: open keyboard boost %d\n", __func__, rc);
		}

	} // attach KB
	else {
		switch (currState)
		{
		case PAD_KEY:
			main_info("%s: test-1 %d\n", __func__, rc);
			rc = charger_enable_keyboard_boost(bq, false);
			retry = 0;
			// Restore sc8651 config
			if (check_g_bcdev_ops()) {
				rc = sc8651_wpc_gate_set(1);
				if (rc < 0) {
					main_info("%s: close sc8651 wpc gate is failed %d\n", __func__, rc);
					return;
				}
			}
			main_info("%s: close keyboard boost %d\n", __func__, rc);
			currState = NONE;
			break;

		default:
			break;
		}

	} // dettach KB
}

static void xm_handler_adsp_workfunc(struct work_struct *work)
{
	struct mainchg *bq = container_of(work, struct mainchg, handler_adsp_work.work);
	int rc  = 0, retry = 0;

	if (!bq) {
		main_info("%s: bq is null\n", __func__);
		return;
	}

	if (dc_in) {
		if (!kb_in) {
			currState = PAD_CAR;
			if (object == 0x38) {
				if (Nanosic_ADSP_Read_Charge_Status() < 0) {
					main_err("%s: read charge data is fail.\n", __func__);
				}
			} else {
				rc = Nanosic_ADSP_Read_Charge_Status_Single(0x15);
				msleep(5);
				rc = Nanosic_ADSP_Read_Charge_Status_Single(0x16);
				msleep(5);
				if (rc < 0) {
					main_err("%s: read holder charge data is fail.\n", __func__);
				}
			}
			main_info("%s: detect PAD_CAR scene\n", __func__);
		}
		else {
			if (currState == PAD_KEY) {

				currState = PAD_KEY_TYPEC;
				main_info("%s: detect PAD_KEY_TYPEC scene%d\n", __func__);

				// disable KB boost
				rc = charger_enable_keyboard_boost(bq, false);
				// Restore sc8651 config
				if (check_g_bcdev_ops()) {
					rc = sc8651_wpc_gate_set(1);
					if (rc < 0) {
						main_info("%s: close sc8651 wpc gate is failed %d\n", __func__, rc);
						return;
					}
				}
				main_info("%s: close keyboard boost %d\n", __func__, rc);
			}
		}

		schedule_work(&bq->irq_work);
	} // dc_in online
	else {
		schedule_work(&bq->irq_work);

		if (kb_in) {
			// open boost
			do {
				if (!check_g_bcdev_ops()) {
					msleep(50);
					main_err("%s: check_g_bcdev_ops is null, wait retry %d\n", __func__, retry);
					if (retry > 10)
						break;
					continue;
				}

				rc = sc8651_wpc_gate_set(0);
				if (rc < 0) {
					main_err("%s: close sc8651 wpc gate is failed %d\n", __func__, rc);
					return;
				}
			} while(!check_g_bcdev_ops());
			retry = 0;
			rc = charger_enable_keyboard_boost(g_bq, true);
			currState = PAD_KEY;
			main_info("%s: detect PAD_KEY scene%d\n", __func__);
			main_info("%s: open keyboard boost %d\n", __func__, rc);
		}

	} // dc_in not online
}

static void xm_first_irq_workfunc(struct work_struct *work)
{
	struct mainchg *bq = container_of(work, struct mainchg, first_irq_work.work);
	static int first_dcin_count, first_mcu_count;

	if ((first_dcin_count > 50) || (first_mcu_count > 50)) {
		main_info("%s get dcin or mcu over 50\n", __func__);
		return;
	}
	dc_in = qti_get_DCIN_STATE();
	g_battmngr_noti->irq_msg.value = dc_in;
	main_err("%s: start first_irq_work, dcin state:%d\n", __func__, dc_in);
	if (bq && dc_in) {
		if (g_mcu_data) {
			qti_deal_report();
			schedule_delayed_work(&bq->handler_adsp_work, 0);
			main_info("%s: start irq_work\n", __func__);
		} else {
			if (g_nodev) {
				if (g_nodev->gObject == 0x3B) {
					mutex_lock(&g_battmngr_noti->notify_lock);
					g_battmngr_noti->mcu_msg.object_type = 0x3B;
					g_battmngr_noti->mcu_msg.msg_type = BATTMNGR_MSG_MCU_TYPE;
					battmngr_notifier_call_chain(BATTMNGR_EVENT_MCU, g_battmngr_noti);
					mutex_unlock(&g_battmngr_noti->notify_lock);
				}
			}
			schedule_delayed_work(&bq->first_irq_work, msecs_to_jiffies(400));
			first_mcu_count++;
			main_info("%s g_mcu_data is null, send noti, first_mcu_count:%d\n", __func__, first_mcu_count);
		}
	} else if (g_nodev->gObject == 0x3B) {
		schedule_delayed_work(&bq->first_irq_work, msecs_to_jiffies(400));
		first_dcin_count++;
		main_info("%s dc_in is null, wait, first_dcin_count:%d\n", __func__, first_dcin_count);
	} else if (g_nodev->gObject == 0x38) {
		qti_set_keyboard_plugin(1);
		main_info("%s keyboard already plugin, move mos to manual\n", __func__);
	}
}

static int xm_parse_dt(struct device *dev, struct mainchg *bq)
{
	int ret;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32(np, "xm,charge-current",&bq->cfg.charge_current);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "xm,term-current",&bq->cfg.term_current);
	if (ret)
		return ret;

	/*bq->kb_ovp_en_gpio = of_get_named_gpio(np, "xm,kb_ovp_boost_en", 0);
	main_err("%s:bq->kb_ovp_en_gpio: %d\n", __func__, bq->kb_ovp_en_gpio);
	if ((!gpio_is_valid(bq->kb_ovp_en_gpio)))
		return -EINVAL;

	ret = gpio_request(bq->kb_ovp_en_gpio, "xm_kb_ovp_boost_en");
	if (ret) {
		main_err("%s: unable to kb_ovp_en_gpio [%d],ret:%d\n", __func__,
				bq->kb_ovp_en_gpio, ret);
		return -EINVAL;
	}

	bq->kb_boost_5v_gpio = of_get_named_gpio(np, "xm,kb_boost_5v", 0);
	main_err("%s:bq->kb_boost_5v_gpio: %d\n", __func__, bq->kb_boost_5v_gpio);
	if ((!gpio_is_valid(bq->kb_boost_5v_gpio)))
		return -EINVAL;

	ret = gpio_request(bq->kb_boost_5v_gpio, "xm_kb_boost_5v");
	if (ret) {
		main_err("%s: unable to kb_boost_5v_gpio [%d],ret:%d\n", __func__,
				bq->kb_boost_5v_gpio, ret);
		return -EINVAL;
	}*/
	return 0;
}

int xm_init_device(struct mainchg *bq)
{
	int ret;

	ret = xm_set_term_current(bq, bq->cfg.term_current);
	if (ret < 0) {
		main_err("%s:Failed to set termination current:%d\n", __func__, ret);
		return ret;
	}

	if (bq_check_vote(bq)) {
		vote(bq->usb_icl_votable, BQ_ICL_VOTER, true, bq->cfg.charge_current * 1000);
	}

	return ret;
}

/*static int bq_charger_gpio_init(struct mainchg *bq)
{
	int ret = 0;

	bq->kb_pinctrl = devm_pinctrl_get(bq->dev);
	if (IS_ERR_OR_NULL(bq->kb_pinctrl)) {
		main_err("No pinctrl config specified\n");
		ret = PTR_ERR(bq->dev);
		return ret;
	}
	bq->kb_gpio_active =
	    pinctrl_lookup_state(bq->kb_pinctrl, "mcu_active");
	if (IS_ERR_OR_NULL(bq->kb_gpio_active)) {
		main_err("No active config specified\n");
		ret = PTR_ERR(bq->kb_gpio_active);
		return ret;
	}
	bq->kb_gpio_suspend =
	    pinctrl_lookup_state(bq->kb_pinctrl, "mcu_suspend");
	if (IS_ERR_OR_NULL(bq->kb_gpio_suspend)) {
		main_err("No suspend config specified\n");
		ret = PTR_ERR(bq->kb_gpio_suspend);
		return ret;
	}

	ret = pinctrl_select_state(bq->kb_pinctrl, bq->kb_gpio_active);
	if (ret < 0) {
		main_err("fail to select pinctrl active rc=%d\n", ret);
		return ret;
	}

	return ret;
}*/

static int xm_pogocharge_probe(struct platform_device *pdev)
{
	struct mainchg *bq;
	int ret;
	static int probe_cnt = 0;

	main_info("%s probe_cnt = %d\n", __func__, ++probe_cnt);

	bq = devm_kzalloc(&pdev->dev, sizeof(*bq), GFP_KERNEL);
	if (!bq) {
		dev_err(&pdev->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	bq->dev = &pdev->dev;
	platform_set_drvdata(pdev, bq);

	bq->battmg_dev = NULL;
	bq->vbat_prot_flag = false;

	bq->batt_psy = power_supply_get_by_name("battery");
	bq->usb_psy = power_supply_get_by_name("usb");
	ret = bq_check_vote(bq);
	if (ret == 0) {
		main_err("Failed to initialize BQ VOTE, rc=%d\n", ret);
	}
	if (!check_g_bcdev_ops() || !bq->batt_psy || !bq->usb_psy || !ret || !g_battmngr_noti) {
		main_err("%s check_g_bcdev_ops or battery or usbor g_battmngr_noti not ready\n", __func__);
		ret = -EPROBE_DEFER;
		msleep(100);
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			return ret;
	}

	if (xm_parse_dt(&pdev->dev, bq) < 0) {
		main_err("%s: Couldn't init gpio rc=%d\n", __func__, ret);
	}

	/*ret = bq_charger_gpio_init(bq);
	if (ret < 0) {
		main_err("%s: Couldn't init gpio rc=%d\n", __func__, ret);
		return ret;
	}*/

	ret = xm_init_device(bq);
	if (ret < 0) {
		main_err("device init failure: %d\n", ret);
	}

	INIT_WORK(&bq->irq_work, xm_charger_irq_workfunc);
	INIT_WORK(&bq->adapter_in_work, xm_adapter_in_workfunc);
	INIT_WORK(&bq->adapter_out_work, xm_adapter_out_workfunc);
	INIT_DELAYED_WORK(&bq->monitor_work, xm_monitor_workfunc);
	INIT_DELAYED_WORK(&bq->first_irq_work, xm_first_irq_workfunc);
	INIT_DELAYED_WORK(&bq->handler_mcu_work, xm_handler_mcu_workfunc);
	INIT_DELAYED_WORK(&bq->handler_adsp_work, xm_handler_adsp_workfunc);

	g_bq = bq;

	schedule_delayed_work(&bq->first_irq_work, msecs_to_jiffies(100));

out:
	main_info("%s %s!!\n", __func__, ret == -EPROBE_DEFER ?
				"Over probe cnt max" : "OK");
	return 0;

}

static void xm_pogocharge_shutdown(struct platform_device *pdev)
{
    struct mainchg *bq = platform_get_drvdata(pdev);

	cancel_work_sync(&bq->irq_work);
	cancel_work_sync(&bq->adapter_in_work);
	cancel_work_sync(&bq->adapter_out_work);
	cancel_delayed_work_sync(&bq->monitor_work);
	cancel_delayed_work_sync(&bq->first_irq_work);
	cancel_delayed_work_sync(&bq->handler_mcu_work);
	cancel_delayed_work_sync(&bq->handler_adsp_work);
	bq->battmg_dev = NULL;
	platform_set_drvdata(pdev, NULL);
	return;
}

static const struct of_device_id match_table[] = {
	{.compatible = "xiaomi,pogocharge"},
	{},
};

static struct platform_driver xm_pogocharge_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "xm_pogocharge",
		.of_match_table = match_table,
	},
	.probe = xm_pogocharge_probe,
	.shutdown = xm_pogocharge_shutdown,
};

static int __init xm_pogocharge_init(void)
{
	return platform_driver_register(&xm_pogocharge_driver);
}
module_init(xm_pogocharge_init);

static void __exit xm_pogocharge_exit(void)
{
	platform_driver_unregister(&xm_pogocharge_driver);
}
module_exit(xm_pogocharge_exit);

MODULE_DESCRIPTION("POGO main charger");
MODULE_AUTHOR("yinshunan@xiaomi.com");
MODULE_AUTHOR("litianpeng6@xiaomi.com");
MODULE_LICENSE("GPL v2");
