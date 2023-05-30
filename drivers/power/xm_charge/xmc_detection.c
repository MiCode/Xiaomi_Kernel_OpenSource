/*******************************************************************************************
* Description:	XIAOMI-BSP-CHARGE
* 		This xmc_detection.c is the monitor for plugin/plugout/charger_type notify.
*		The notifier usually comes from TCPC or BC1.2/QC IC.
* ------------------------------ Revision History: --------------------------------
* <version>	<date>		<author>			<desc>
* 1.0		2022-02-22	chenyichun@xiaomi.com		Created for new architecture
********************************************************************************************/

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "xmc_core.h"

static enum xmc_typec_mode xmc_get_typec_mode(struct tcp_notify *noti)
{
	switch (noti->typec_state.new_state) {
	case TYPEC_UNATTACHED:
		return POWER_SUPPLY_TYPEC_NONE;
	case TYPEC_ATTACHED_SNK:
	case TYPEC_ATTACHED_NORP_SRC:
		switch (noti->typec_state.rp_level) {
		case TYPEC_CC_VOLT_SNK_1_5:
			return POWER_SUPPLY_TYPEC_SOURCE_MEDIUM;
		case TYPEC_CC_VOLT_SNK_3_0:
			return POWER_SUPPLY_TYPEC_SOURCE_HIGH;
		default:
			return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
		}
	case TYPEC_ATTACHED_AUDIO:
		return POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER;
	case TYPEC_ATTACHED_CUSTOM_SRC:
		return POWER_SUPPLY_TYPEC_SOURCE_DEFAULT;
	default:
		return POWER_SUPPLY_TYPEC_NONE;
	}
}

static void xmc_charger_attach_detach(struct charge_chip *chip, bool plug)
{
	if (plug) {
		pm_stay_awake(chip->dev);
	} else {
		pm_relax(chip->dev);
	}
}

static int xmc_bc12_qc_notifier_func(struct notifier_block *nb, unsigned long event, void *data)
{
	struct charge_chip *chip = container_of(nb, struct charge_chip, bc12_qc_nb);
	struct power_supply *psy = data;
	union power_supply_propval val = {0,};
	enum xmc_bc12_type new_bc12_type = XMC_BC12_TYPE_NONE;
	enum xmc_qc_type new_qc_type = XMC_QC_TYPE_NONE;
	bool report_sysfs = false, schedule_monitor = false;

	if (strcmp(psy->desc->name, chip->bc12_qc_psy->desc->name) || event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	power_supply_get_property(psy, POWER_SUPPLY_PROP_USB_TYPE, &val);

	if (chip->chip_list.bc12_qc_chip == BC12_PMIC) {
		if (val.intval == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			new_bc12_type = XMC_BC12_TYPE_NONE;
			new_qc_type = XMC_QC_TYPE_NONE;
		} else if (val.intval <= POWER_SUPPLY_USB_TYPE_CDP) {
			new_bc12_type = val.intval;
			new_qc_type = chip->usb_typec.qc_type;
		} else if (val.intval == POWER_SUPPLY_USB_TYPE_ACA) {
			new_bc12_type = chip->usb_typec.bc12_type;
			new_qc_type = XMC_QC_TYPE_HVCHG;
		} else {
			xmc_err("[XMC_DETECT] not supported bc12_qc_type\n");
			return NOTIFY_OK;
		}
	} else if (chip->chip_list.bc12_qc_chip == BC12_XMUSB350_I350) {
		/* to do */
	}

	mutex_lock(&chip->charger_type_lock);
	if (chip->usb_typec.bc12_type != new_bc12_type || chip->usb_typec.qc_type != new_qc_type) {
		xmc_info("[XMC_DETECT] update BC12_QC type, new = [%d %d], old = [%d %d]\n", new_bc12_type, new_qc_type, chip->usb_typec.bc12_type, chip->usb_typec.qc_type);
		report_sysfs = true;
		if ((chip->usb_typec.pd_type || chip->usb_typec.bc12_type || chip->usb_typec.qc_type) != (new_bc12_type || new_qc_type))
			schedule_monitor = true;
		chip->usb_typec.bc12_type = new_bc12_type;
		chip->usb_typec.qc_type = new_qc_type;
	}
	mutex_unlock(&chip->charger_type_lock);

	if (report_sysfs) {
		power_supply_changed(chip->usb_psy);
		xmc_sysfs_report_uevent(chip->usb_psy);
	}

	if (schedule_monitor)
		mod_delayed_work(system_wq, &chip->main_monitor_work, 0);

	return NOTIFY_OK;
}

static void xmc_receive_pd_state(struct charge_chip *chip, struct tcp_notify *noti)
{
	enum xmc_pd_type new_pd_type = XMC_PD_TYPE_NONE;
	bool report_sysfs = false, schedule_monitor = false;

	xmc_info("[XMC_DETECT] pd_state notify, pd_state = %d\n", noti->pd_state.connected);

	switch (noti->pd_state.connected) {
	case PD_CONNECT_NONE: /* 0 */
		new_pd_type = XMC_PD_TYPE_NONE;
		memset(&chip->adapter, 0, sizeof(struct adapter_desc));
		break;
	case PD_CONNECT_HARD_RESET: /* 9 */
		new_pd_type = XMC_PD_TYPE_NONE;
		break;
	case PD_CONNECT_PE_READY_SNK: /* 4 */
		new_pd_type = XMC_PD_TYPE_PD2;
		break;
	case PD_CONNECT_PE_READY_SNK_PD30: /* 6 */
		new_pd_type = XMC_PD_TYPE_PD3;
		xmc_ops_get_pd_id(chip->adapter_dev);
		break;
	case PD_CONNECT_PE_READY_SNK_APDO: /* 8 */
		new_pd_type = XMC_PD_TYPE_PPS;
		break;
	default:
		return;
	};

	mutex_lock(&chip->charger_type_lock);
	if (chip->usb_typec.pd_type != new_pd_type) {
		xmc_info("[XMC_DETECT]  update PD type, new = %d, old = %d\n", new_pd_type, chip->usb_typec.pd_type);

		if (chip->usb_typec.pd_type <= XMC_PD_TYPE_PD2 && new_pd_type >= XMC_PD_TYPE_PD3)
			chip->adapter.uvdm_state = USBPD_UVDM_CONNECT;

		if (!chip->usb_typec.pd_type && !chip->usb_typec.bc12_type && !chip->usb_typec.qc_type && new_pd_type)
			schedule_monitor = true;
		report_sysfs = true;
		chip->usb_typec.pd_type = new_pd_type;
	}
	mutex_unlock(&chip->charger_type_lock);

	if (report_sysfs) {
		power_supply_changed(chip->usb_psy);
		xmc_sysfs_report_uevent(chip->usb_psy);
	}

	if (schedule_monitor)
		mod_delayed_work(system_wq, &chip->main_monitor_work, 0);
}

static void xmc_receive_uvdm(struct charge_chip *chip, struct tcp_notify *noti)
{
	int i = 0, cmd = 0;

	if (noti->uvdm_msg.uvdm_svid != USBPD_MI_SVID) {
		xmc_info("[XMC_AUTH] VID = 0x%04x is not 0x2717, abort\n", noti->uvdm_msg.uvdm_svid);
		return;
	}

	cmd = USBPD_UVDM_HDR_CMD(noti->uvdm_msg.uvdm_data[0]);
	xmc_info("[XMC_AUTH] receive UVDM, [cmd ack cnt svid] = [%d %d %d 0x%04x]\n", cmd, noti->uvdm_msg.ack, noti->uvdm_msg.uvdm_cnt, noti->uvdm_msg.uvdm_svid);

	if (noti->uvdm_msg.uvdm_svid != USBPD_MI_SVID)
		return;

	switch (cmd) {
	case USBPD_UVDM_CHARGER_VERSION:
		chip->adapter.version = noti->uvdm_msg.uvdm_data[1];
		xmc_info("[XMC_AUTH] receive UVDM, ta_version = %x\n", chip->adapter.version);
		break;
	case USBPD_UVDM_CHARGER_TEMP:
		chip->adapter.temp = (noti->uvdm_msg.uvdm_data[1] & 0x0000FFFF) * 10;
		xmc_info("[XMC_AUTH] receive UVDM, ta_temp = %d\n", chip->adapter.temp);
		break;
	case USBPD_UVDM_CHARGER_VOLTAGE:
		chip->adapter.voltage = (noti->uvdm_msg.uvdm_data[1] & 0x0000FFFF) * 10;
		xmc_info("[XMC_AUTH] receive UVDM, ta_voltage = %d\n", chip->adapter.voltage);
		break;
	case USBPD_UVDM_SESSION_SEED:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			chip->adapter.s_secert[i] = noti->uvdm_msg.uvdm_data[i + 1];
			xmc_info("[XMC_AUTH] receive UVDM, s_secert[%d]=0x%x", i + 1, noti->uvdm_msg.uvdm_data[i + 1]);
		}
		break;
	case USBPD_UVDM_AUTHENTICATION:
		for (i = 0; i < USBPD_UVDM_SS_LEN; i++) {
			chip->adapter.digest[i] = noti->uvdm_msg.uvdm_data[i + 1];
			xmc_info("[XMC_AUTH] receive UVDM, digest[%d]=0x%x", i + 1, noti->uvdm_msg.uvdm_data[i + 1]);
		}
		break;
	case USBPD_UVDM_REVERSE_AUTHEN:
		xmc_info("[XMC_AUTH] receive UVDM, reauth = 0x%08x\n", noti->uvdm_msg.uvdm_data[1]);
		chip->adapter.reauth = (noti->uvdm_msg.uvdm_data[1] & 0xFFFF);
		break;
	default:
		break;
	}
	chip->adapter.uvdm_state = cmd;
}

static int xmc_tcpc_notifier_func(struct notifier_block *nb, unsigned long event, void *data)
{
	struct charge_chip *chip = container_of(nb, struct charge_chip, tcpc_nb);
	struct tcp_notify *noti = data;

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE: /* 14 */
		chip->usb_typec.typec_mode = xmc_get_typec_mode(noti);
		chip->usb_typec.cc_orientation = noti->typec_state.polarity ? TYPEC_ORIENTATION_NORMAL : TYPEC_ORIENTATION_REVERSE;
		xmc_info("[XMC_DETECT] typec_state notify, typec_mode = %d, orientation = %d, new_state = %d, old_state = %d\n",
			chip->usb_typec.typec_mode, chip->usb_typec.cc_orientation, noti->typec_state.new_state, noti->typec_state.old_state);
		if (noti->typec_state.old_state == TYPEC_UNATTACHED && (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
			noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC || noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			xmc_charger_attach_detach(chip, true);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK || noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC) && noti->typec_state.new_state == TYPEC_UNATTACHED) {
			xmc_charger_attach_detach(chip, false);
		}
		break;
	case TCP_NOTIFY_PD_STATE: /* 15 */
		xmc_receive_pd_state(chip, noti);
		break;
	case TCP_NOTIFY_UVDM:
		xmc_receive_uvdm(chip, noti);
		break;
	case TCP_NOTIFY_WD_STATUS:
		chip->usb_typec.water_detect = noti->wd_status.water_detected;
		xmc_info("[XMC_DETECT] water_detect = %d\n", chip->usb_typec.water_detect);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

bool xmc_detection_init(struct charge_chip *chip)
{
	int rc = 0;
	struct device_node *chip_list_node = NULL;

	chip_list_node = of_find_node_by_name(chip->dev->of_node, "chip_list");
	chip->bc12_qc_psy = power_supply_get_by_phandle(chip_list_node, "bc12_qc_psy");
	if (IS_ERR_OR_NULL(chip->bc12_qc_psy)) {
		xmc_err("[XMC_PROBE] failed to get bc12_qc_psy\n");
		return false;
	}

	chip->tcpc_nb.notifier_call = xmc_tcpc_notifier_func;
	rc = register_tcp_dev_notifier(chip->tcpc_dev, &chip->tcpc_nb, TCP_NOTIFY_TYPE_USB | TCP_NOTIFY_TYPE_VBUS | TCP_NOTIFY_TYPE_MODE | TCP_NOTIFY_TYPE_MISC);
	if (rc < 0) {
		xmc_err("[XMC_PROBE] failed to register tcpc_notify\n");
		return false;
	}

	chip->bc12_qc_nb.notifier_call = xmc_bc12_qc_notifier_func;
	power_supply_reg_notifier(&chip->bc12_qc_nb);

	return true;
}
