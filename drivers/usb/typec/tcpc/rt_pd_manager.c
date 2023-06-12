/*
 * Copyright (C) 2020 Richtek Inc.
 *
 * Richtek RT PD Manager
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/extcon-provider.h>
#include <linux/iio/consumer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/usb/typec.h>
#include <linux/version.h>

#ifdef CONFIG_WT_QGKI
#include <drm/drm_notifier.h>
#endif
#include "inc/tcpci_typec.h"

#define RT_PD_MANAGER_VERSION	"0.0.8_G"

#ifdef CONFIG_WT_QGKI
#define PROBE_CNT_MAX			10
#else
#define PROBE_CNT_MAX			200
#endif
/* 10ms * 100 = 1000ms = 1s */
#define USB_TYPE_POLLING_INTERVAL	20
#define USB_TYPE_POLLING_CNT_MAX	200

enum dr {
	DR_IDLE,
	DR_DEVICE,
	DR_HOST,
	DR_DEVICE_TO_HOST,
	DR_HOST_TO_DEVICE,
	DR_MAX,
};

static char *dr_names[DR_MAX] = {
	"Idle", "Device", "Host", "Device to Host", "Host to Device",
};

struct rt_pd_manager_data {
	struct device *dev;
	struct extcon_dev *extcon;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	struct iio_channel **iio_channels;
	bool shutdown_flag;
#else
	struct power_supply *usb_psy;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */
	struct delayed_work usb_dwork;
	struct tcpc_device *tcpc;
	struct notifier_block pd_nb;
	enum dr usb_dr;
	int usb_type_polling_cnt;
	int sink_mv_new;
	int sink_ma_new;
	int sink_mv_old;
	int sink_ma_old;

	struct typec_capability typec_caps;
	struct typec_port *typec_port;
	struct typec_partner *partner;
	struct typec_partner_desc partner_desc;
	struct usb_pd_identity partner_identity;
};
#ifdef CONFIG_WT_QGKI
extern int charger_notifier_call_chain(unsigned long val, void *v);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#define POWER_SUPPLY_TYPE_USB_FLOAT	QTI_POWER_SUPPLY_TYPE_USB_FLOAT
#define POWER_SUPPLY_PD_INACTIVE	QTI_POWER_SUPPLY_PD_INACTIVE
#define POWER_SUPPLY_PD_ACTIVE		QTI_POWER_SUPPLY_PD_ACTIVE
#define POWER_SUPPLY_PD_PPS_ACTIVE	QTI_POWER_SUPPLY_PD_PPS_ACTIVE

enum iio_psy_property {
	RT1711_PD_ACTIVE = 0,
	RT1711_PD_USB_SUSPEND_SUPPORTED,
	RT1711_PD_IN_HARD_RESET,
	RT1711_PD_CURRENT_MAX,
	RT1711_PD_VOLTAGE_MIN,
	RT1711_PD_VOLTAGE_MAX,
	RT1711_USB_REAL_TYPE,
	RT1711_OTG_ENABLE,
	RT1711_TYPEC_CC_ORIENTATION,
	RT1711_TYPEC_MODE,
	POWER_SUPPLY_IIO_PROP_MAX,
};

static const char * const iio_channel_map[] = {
		[RT1711_PD_ACTIVE] = "pd_active",
        [RT1711_PD_USB_SUSPEND_SUPPORTED] = "pd_usb_suspend_supported",
        [RT1711_PD_IN_HARD_RESET] = "pd_in_hard_reset",
        [RT1711_PD_CURRENT_MAX] = "pd_current_max",
        [RT1711_PD_VOLTAGE_MIN] = "pd_voltage_min",
        [RT1711_PD_VOLTAGE_MAX] = "pd_voltage_max",
        [RT1711_USB_REAL_TYPE] = "real_type",
        [RT1711_OTG_ENABLE] = "otg_enable",
        [RT1711_TYPEC_CC_ORIENTATION] = "typec_cc_orientation",
        [RT1711_TYPEC_MODE] = "typec_mode",
};

static bool is_rt1711_chan_valid(struct rt_pd_manager_data *chip,
		enum iio_psy_property chan)
{
	int rc;

	if (IS_ERR(chip->iio_channels[chan]))
		return false;

	if(chip->shutdown_flag)
		return false;

	if (!chip->iio_channels[chan]) {
		chip->iio_channels[chan] = iio_channel_get(chip->dev,
					iio_channel_map[chan]);
		if (IS_ERR(chip->iio_channels[chan])) {
			rc = PTR_ERR(chip->iio_channels[chan]);
			if (rc == -EPROBE_DEFER)
				chip->iio_channels[chan] = NULL;

			pr_err("Failed to get IIO channel %s, rc=%d\n",
				iio_channel_map[chan], rc);
			return false;
		}
	}

	return true;
}


static int smblib_get_prop(struct rt_pd_manager_data *rpmd,
			   enum iio_psy_property ipp,
			   union power_supply_propval *val)
{
	int ret = 0, value = 0;

	if (!is_rt1711_chan_valid(rpmd, ipp))
		return -ENODEV;

	ret = iio_read_channel_processed(rpmd->iio_channels[ipp], &value);
	if (ret < 0) {
		dev_err(rpmd->dev, "%s fail(%d), ipp = %d\n",
				   __func__, ret, ipp);
		return ret;
	}

	val->intval = value;
	return 0;
}

static int smblib_set_prop(struct rt_pd_manager_data *rpmd,
			   enum iio_psy_property ipp,
			   const union power_supply_propval *val)
{
	int ret = 0;

	if (!is_rt1711_chan_valid(rpmd, ipp))
		return -ENODEV;

	ret = iio_write_channel_raw(rpmd->iio_channels[ipp], val->intval);
	if (ret < 0) {
		dev_err(rpmd->dev, "%s fail(%d), ipp = %d\n",
				   __func__, ret, ipp);
		return ret;
	}

	return 0;
}
#else
static inline int smblib_get_prop(struct rt_pd_manager_data *rpmd,
				  enum power_supply_property psp,
				  union power_supply_propval *val)
{
	return power_supply_get_property(rpmd->usb_psy, psp, val);
}

static inline int smblib_set_prop(struct rt_pd_manager_data *rpmd,
				  enum power_supply_property psp,
				  const union power_supply_propval *val)
{
	return power_supply_set_property(rpmd->usb_psy, psp, val);
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */

static const unsigned int rpm_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static int extcon_init(struct rt_pd_manager_data *rpmd)
{
	int ret = 0;

	/*
	 * associate extcon with the dev as it could have a DT
	 * node which will be useful for extcon_get_edev_by_phandle()
	 */
	rpmd->extcon = devm_extcon_dev_allocate(rpmd->dev, rpm_extcon_cable);
	if (IS_ERR(rpmd->extcon)) {
		ret = PTR_ERR(rpmd->extcon);
		dev_err(rpmd->dev, "%s extcon dev alloc fail(%d)\n",
				   __func__, ret);
		goto out;
	}

	ret = devm_extcon_dev_register(rpmd->dev, rpmd->extcon);
	if (ret) {
		dev_err(rpmd->dev, "%s extcon dev reg fail(%d)\n",
				   __func__, ret);
		goto out;
	}

	/* Support reporting polarity and speed via properties */
	extcon_set_property_capability(rpmd->extcon, EXTCON_USB,
				       EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(rpmd->extcon, EXTCON_USB,
				       EXTCON_PROP_USB_SS);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	extcon_set_property_capability(rpmd->extcon, EXTCON_USB,
				       EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)) */
	extcon_set_property_capability(rpmd->extcon, EXTCON_USB_HOST,
				       EXTCON_PROP_USB_TYPEC_POLARITY);
	extcon_set_property_capability(rpmd->extcon, EXTCON_USB_HOST,
				       EXTCON_PROP_USB_SS);
out:
	return ret;
}

static inline void stop_usb_host(struct rt_pd_manager_data *rpmd)
{
	extcon_set_state_sync(rpmd->extcon, EXTCON_USB_HOST, false);
}

static inline void start_usb_host(struct rt_pd_manager_data *rpmd)
{
	union extcon_property_value val = {.intval = 0};

	val.intval = tcpm_inquire_cc_polarity(rpmd->tcpc);
	extcon_set_property(rpmd->extcon, EXTCON_USB_HOST,
			    EXTCON_PROP_USB_TYPEC_POLARITY, val);

	val.intval = 1;
	extcon_set_property(rpmd->extcon, EXTCON_USB_HOST,
			    EXTCON_PROP_USB_SS, val);

	extcon_set_state_sync(rpmd->extcon, EXTCON_USB_HOST, true);
}

static inline void stop_usb_peripheral(struct rt_pd_manager_data *rpmd)
{
	extcon_set_state_sync(rpmd->extcon, EXTCON_USB, false);
}

static inline void start_usb_peripheral(struct rt_pd_manager_data *rpmd)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	int rp = 0;
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)) */
	union extcon_property_value val = {.intval = 0};

	val.intval = tcpm_inquire_cc_polarity(rpmd->tcpc);
	extcon_set_property(rpmd->extcon, EXTCON_USB,
			    EXTCON_PROP_USB_TYPEC_POLARITY, val);

	val.intval = 1;
	extcon_set_property(rpmd->extcon, EXTCON_USB, EXTCON_PROP_USB_SS, val);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	rp = tcpm_inquire_typec_remote_rp_curr(rpmd->tcpc);
	val.intval = rp > 500 ? 1 : 0;
	extcon_set_property(rpmd->extcon, EXTCON_USB,
			    EXTCON_PROP_USB_TYPEC_MED_HIGH_CURRENT, val);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)) */
	extcon_set_state_sync(rpmd->extcon, EXTCON_USB, true);
}

static void usb_dwork_handler(struct work_struct *work)
{
	int ret = 0;
	struct delayed_work *dwork = to_delayed_work(work);
	struct rt_pd_manager_data *rpmd =
		container_of(dwork, struct rt_pd_manager_data, usb_dwork);
	enum dr usb_dr = rpmd->usb_dr;
	union power_supply_propval val = {.intval = 0};

	if (usb_dr < DR_IDLE || usb_dr >= DR_MAX) {
		dev_err(rpmd->dev, "%s invalid usb_dr = %d\n",
				   __func__, usb_dr);
		return;
	}

	dev_info(rpmd->dev, "%s %s\n", __func__, dr_names[usb_dr]);

	switch (usb_dr) {
	case DR_IDLE:
	case DR_MAX:
		stop_usb_peripheral(rpmd);
		stop_usb_host(rpmd);
		break;
	case DR_DEVICE:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	ret = smblib_get_prop(rpmd, RT1711_USB_REAL_TYPE, &val);
#else
		ret = smblib_get_prop(rpmd, POWER_SUPPLY_PROP_REAL_TYPE, &val);
#endif
		dev_info(rpmd->dev, "%s polling_cnt = %d, ret = %d type = %d\n",
				    __func__, ++rpmd->usb_type_polling_cnt,
				    ret, val.intval);
		if (ret < 0 || val.intval == POWER_SUPPLY_TYPE_UNKNOWN) {
			if (rpmd->usb_type_polling_cnt <
			    USB_TYPE_POLLING_CNT_MAX)
				schedule_delayed_work(&rpmd->usb_dwork,
						msecs_to_jiffies(
						USB_TYPE_POLLING_INTERVAL));
			break;
		} else if (val.intval != POWER_SUPPLY_TYPE_USB &&
			   val.intval != POWER_SUPPLY_TYPE_USB_CDP &&
			   val.intval != POWER_SUPPLY_TYPE_USB_FLOAT &&
			   val.intval != POWER_SUPPLY_TYPE_USB_PD)
			break;

	case DR_HOST_TO_DEVICE:
		stop_usb_host(rpmd);
		start_usb_peripheral(rpmd);
		break;
	case DR_HOST:
	case DR_DEVICE_TO_HOST:
		stop_usb_peripheral(rpmd);
		start_usb_host(rpmd);
		break;
	}
}

static void pd_sink_set_vol_and_cur(struct rt_pd_manager_data *rpmd,
				    int mv, int ma, uint8_t type)
{
	const int micro_5v = 5000000;
	unsigned long sel = 0;
	union power_supply_propval val = {.intval = 0};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	/* Charger plug-in first time */
	smblib_get_prop(rpmd, RT1711_PD_ACTIVE, &val);

	if (val.intval == QTI_POWER_SUPPLY_PD_INACTIVE) {
		val.intval = QTI_POWER_SUPPLY_PD_ACTIVE;
		smblib_set_prop(rpmd, RT1711_PD_ACTIVE, &val);
	}
#else
	/* Charger plug-in first time */
	smblib_get_prop(rpmd, POWER_SUPPLY_PROP_PD_ACTIVE, &val);

	if (val.intval == POWER_SUPPLY_PD_INACTIVE) {
		val.intval = POWER_SUPPLY_PD_ACTIVE;
		smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_ACTIVE, &val);
	}
#endif
	switch (type) {
	case TCP_VBUS_CTRL_PD_HRESET:
	case TCP_VBUS_CTRL_PD_PR_SWAP:
	case TCP_VBUS_CTRL_PD_REQUEST:
		set_bit(0, &sel);
		set_bit(1, &sel);
		val.intval = mv * 1000;
		break;
	case TCP_VBUS_CTRL_PD_STANDBY_UP:
		set_bit(1, &sel);
		val.intval = mv * 1000;
		break;
	case TCP_VBUS_CTRL_PD_STANDBY_DOWN:
		set_bit(0, &sel);
		val.intval = mv * 1000;
		break;
	default:
		break;
	}
	if (val.intval < micro_5v)
		val.intval = micro_5v;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	if (test_bit(0, &sel))
		smblib_set_prop(rpmd, RT1711_PD_VOLTAGE_MIN, &val);
	if (test_bit(1, &sel))
		smblib_set_prop(rpmd, RT1711_PD_VOLTAGE_MAX, &val);

	val.intval = ma * 1000;
	smblib_set_prop(rpmd, RT1711_PD_CURRENT_MAX, &val);
#else
	if (test_bit(0, &sel))
		smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_VOLTAGE_MIN, &val);
	if (test_bit(1, &sel))
		smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_VOLTAGE_MAX, &val);

	val.intval = ma * 1000;
	smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_CURRENT_MAX, &val);
#endif
}

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	int ret = 0;
	struct tcp_notify *noti = data;
	struct rt_pd_manager_data *rpmd =
		container_of(nb, struct rt_pd_manager_data, pd_nb);
	uint8_t old_state = TYPEC_UNATTACHED, new_state = TYPEC_UNATTACHED;
	enum typec_pwr_opmode opmode = TYPEC_PWR_MODE_USB;
	uint32_t partner_vdos[VDO_MAX_NR];
	union power_supply_propval val = {.intval = 0};

	switch (event) {
	case TCP_NOTIFY_SINK_VBUS:
		rpmd->sink_mv_new = noti->vbus_state.mv;
		rpmd->sink_ma_new = noti->vbus_state.ma;
		dev_info(rpmd->dev, "%s sink vbus %dmV %dmA type(0x%02X)\n",
				    __func__, rpmd->sink_mv_new,
				    rpmd->sink_ma_new, noti->vbus_state.type);

		if ((rpmd->sink_mv_new != rpmd->sink_mv_old) ||
		    (rpmd->sink_ma_new != rpmd->sink_ma_old)) {
			rpmd->sink_mv_old = rpmd->sink_mv_new;
			rpmd->sink_ma_old = rpmd->sink_ma_new;
			if (rpmd->sink_mv_new && rpmd->sink_ma_new) {
				/* enable VBUS power path */
			} else {
				/* disable VBUS power path */
			}
		}

		if (noti->vbus_state.type & TCP_VBUS_CTRL_PD_DETECT)
			pd_sink_set_vol_and_cur(rpmd, rpmd->sink_mv_new,
						rpmd->sink_ma_new,
						noti->vbus_state.type);
		break;
	case TCP_NOTIFY_SOURCE_VBUS:
		dev_info(rpmd->dev, "%s source vbus %dmV\n",
				    __func__, noti->vbus_state.mv);
		/**** todo***/
		if (noti->vbus_state.mv)
			val.intval = 1;
		else
			val.intval = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		smblib_set_prop(rpmd, RT1711_OTG_ENABLE, &val);
#endif
		/**** todo ****/
		/* enable/disable OTG power output */
		break;
	case TCP_NOTIFY_TYPEC_STATE:
		old_state = noti->typec_state.old_state;
		new_state = noti->typec_state.new_state;
		if (old_state == TYPEC_UNATTACHED &&
		    (new_state == TYPEC_ATTACHED_SNK ||
		     new_state == TYPEC_ATTACHED_NORP_SRC ||
		     new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		     new_state == TYPEC_ATTACHED_DBGACC_SNK)) {
			dev_info(rpmd->dev,
				 "%s Charger plug in, polarity = %d\n",
				 __func__, noti->typec_state.polarity);
			/*
			 * start charger type detection,
			 * and enable device connection
			 */
			cancel_delayed_work_sync(&rpmd->usb_dwork);
			rpmd->usb_dr = DR_DEVICE;
			rpmd->usb_type_polling_cnt = 0;
			schedule_delayed_work(&rpmd->usb_dwork,
					      msecs_to_jiffies(
					      USB_TYPE_POLLING_INTERVAL));
			typec_set_data_role(rpmd->typec_port, TYPEC_DEVICE);
			typec_set_pwr_role(rpmd->typec_port, TYPEC_SINK);
			typec_set_pwr_opmode(rpmd->typec_port,
					     noti->typec_state.rp_level -
					     TYPEC_CC_VOLT_SNK_DFT);
			typec_set_vconn_role(rpmd->typec_port, TYPEC_SINK);
		} else if ((old_state == TYPEC_ATTACHED_SNK ||
			    old_state == TYPEC_ATTACHED_NORP_SRC ||
			    old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			    old_state == TYPEC_ATTACHED_DBGACC_SNK) &&
			    new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "%s Charger plug out\n", __func__);
			/*
			 * report charger plug-out,
			 * and disable device connection
			 */
			cancel_delayed_work_sync(&rpmd->usb_dwork);
			rpmd->usb_dr = DR_IDLE;
			schedule_delayed_work(&rpmd->usb_dwork, 0);
		} else if (old_state == TYPEC_UNATTACHED &&
			   (new_state == TYPEC_ATTACHED_SRC ||
			    new_state == TYPEC_ATTACHED_DEBUG)) {
			dev_info(rpmd->dev,
				 "%s OTG plug in, polarity = %d\n",
				 __func__, noti->typec_state.polarity);
			/* enable host connection */
			cancel_delayed_work_sync(&rpmd->usb_dwork);
			rpmd->usb_dr = DR_HOST;
			schedule_delayed_work(&rpmd->usb_dwork, 0);
			typec_set_data_role(rpmd->typec_port, TYPEC_HOST);
			typec_set_pwr_role(rpmd->typec_port, TYPEC_SOURCE);
			switch (noti->typec_state.local_rp_level) {
			case TYPEC_RP_3_0:
				opmode = TYPEC_PWR_MODE_3_0A;
				break;
			case TYPEC_RP_1_5:
				opmode = TYPEC_PWR_MODE_1_5A;
				break;
			case TYPEC_RP_DFT:
			default:
				opmode = TYPEC_PWR_MODE_USB;
				break;
			}
			typec_set_pwr_opmode(rpmd->typec_port, opmode);
			typec_set_vconn_role(rpmd->typec_port, TYPEC_SOURCE);
		} else if ((old_state == TYPEC_ATTACHED_SRC ||
			    old_state == TYPEC_ATTACHED_DEBUG) &&
			    new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "%s OTG plug out\n", __func__);
			/* disable host connection */
			cancel_delayed_work_sync(&rpmd->usb_dwork);
			rpmd->usb_dr = DR_IDLE;
			schedule_delayed_work(&rpmd->usb_dwork, 0);
		} else if (old_state == TYPEC_UNATTACHED &&
			   new_state == TYPEC_ATTACHED_AUDIO) {
			dev_info(rpmd->dev, "%s Audio plug in\n", __func__);
			/* enable AudioAccessory connection */
		} else if (old_state == TYPEC_ATTACHED_AUDIO &&
			   new_state == TYPEC_UNATTACHED) {
			dev_info(rpmd->dev, "%s Audio plug out\n", __func__);
			/* disable AudioAccessory connection */
		}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		if (new_state != TYPEC_UNATTACHED) {
			val.intval = noti->typec_state.polarity + 1;
#ifdef CONFIG_WT_QGKI
			//notify tp to charger mode
			charger_notifier_call_chain(CHARGER_STATE_ENABLE,NULL);
#endif
		} else {
			val.intval = 0;
#ifdef CONFIG_WT_QGKI
			//notify tp to charger mode
			charger_notifier_call_chain(CHARGER_STATE_DISABLE,NULL);
#endif
		}
		smblib_set_prop(rpmd, RT1711_TYPEC_CC_ORIENTATION, &val);
		dev_info(rpmd->dev, "%s USB plug. val.intval=%d\n", __func__, val.intval);

		if (new_state == TYPEC_ATTACHED_SRC) {
			val.intval = TYPEC_ATTACHED_SRC;
			dev_err(rpmd->dev, "%s TYPEC_ATTACHED_SRC(%d)\n",__func__, val.intval);
			smblib_set_prop(rpmd, RT1711_TYPEC_MODE, &val);
		} else if (new_state == TYPEC_UNATTACHED) {
			val.intval = TYPEC_UNATTACHED;
			dev_err(rpmd->dev, "%s TYPEC_UNATTACHED(%d)\n",__func__, val.intval);
			smblib_set_prop(rpmd, RT1711_TYPEC_MODE, &val);
		} else if (new_state == TYPEC_ATTACHED_SNK) {
			val.intval = TYPEC_ATTACHED_SNK;
			dev_err(rpmd->dev, "%s TYPEC_ATTACHED_SNK(%d)\n",__func__, val.intval);
			smblib_set_prop(rpmd, RT1711_TYPEC_MODE, &val);
		} else if (new_state == TYPEC_ATTACHED_AUDIO) {
			val.intval = TYPEC_ATTACHED_AUDIO;
			dev_err(rpmd->dev, "%s TYPEC_ATTACHED_AUDIO(%d)\n",__func__, val.intval);
			smblib_set_prop(rpmd, RT1711_TYPEC_MODE, &val);
		}
#endif

		if (new_state == TYPEC_UNATTACHED) {
			val.intval = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
			smblib_set_prop(rpmd, RT1711_PD_USB_SUSPEND_SUPPORTED, &val);
			smblib_set_prop(rpmd, RT1711_PD_ACTIVE, &val);
#else
			smblib_set_prop(rpmd,
				POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED,
					&val);
			smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_ACTIVE,
					&val);
#endif
			typec_unregister_partner(rpmd->partner);
			rpmd->partner = NULL;
			if (rpmd->typec_caps.prefer_role == TYPEC_SOURCE) {
				typec_set_data_role(rpmd->typec_port,
						    TYPEC_HOST);
				typec_set_pwr_role(rpmd->typec_port,
						   TYPEC_SOURCE);
				typec_set_pwr_opmode(rpmd->typec_port,
						     TYPEC_PWR_MODE_USB);
				typec_set_vconn_role(rpmd->typec_port,
						     TYPEC_SOURCE);
			} else {
				typec_set_data_role(rpmd->typec_port,
						    TYPEC_DEVICE);
				typec_set_pwr_role(rpmd->typec_port,
						   TYPEC_SINK);
				typec_set_pwr_opmode(rpmd->typec_port,
						     TYPEC_PWR_MODE_USB);
				typec_set_vconn_role(rpmd->typec_port,
						     TYPEC_SINK);
			}
		} else if (!rpmd->partner) {
			memset(&rpmd->partner_identity, 0,
			       sizeof(rpmd->partner_identity));
			rpmd->partner_desc.usb_pd = false;
			switch (new_state) {
			case TYPEC_ATTACHED_AUDIO:
				rpmd->partner_desc.accessory =
					TYPEC_ACCESSORY_AUDIO;
				break;
			case TYPEC_ATTACHED_DEBUG:
			case TYPEC_ATTACHED_DBGACC_SNK:
			case TYPEC_ATTACHED_CUSTOM_SRC:
				rpmd->partner_desc.accessory =
					TYPEC_ACCESSORY_DEBUG;
				break;
			default:
				rpmd->partner_desc.accessory =
					TYPEC_ACCESSORY_NONE;
				break;
			}
			rpmd->partner = typec_register_partner(rpmd->typec_port,
					&rpmd->partner_desc);
			if (IS_ERR(rpmd->partner)) {
				ret = PTR_ERR(rpmd->partner);
				dev_notice(rpmd->dev,
				"%s typec register partner fail(%d)\n",
					   __func__, ret);
			}
		}

		if (new_state == TYPEC_ATTACHED_SNK) {
			switch (noti->typec_state.rp_level) {
				/* SNK_RP_3P0 */
				case TYPEC_CC_VOLT_SNK_3_0:
					break;
				/* SNK_RP_1P5 */
				case TYPEC_CC_VOLT_SNK_1_5:
					break;
				/* SNK_RP_STD */
				case TYPEC_CC_VOLT_SNK_DFT:
				default:
					break;
			}
		} else if (new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			   new_state == TYPEC_ATTACHED_DBGACC_SNK) {
			switch (noti->typec_state.rp_level) {
				/* DAM_3000 */
				case TYPEC_CC_VOLT_SNK_3_0:
					break;
				/* DAM_1500 */
				case TYPEC_CC_VOLT_SNK_1_5:
					break;
				/* DAM_500 */
				case TYPEC_CC_VOLT_SNK_DFT:
				default:
					break;
			}
		} else if (new_state == TYPEC_ATTACHED_NORP_SRC) {
			/* Both CCs are open */
		}
		break;
	case TCP_NOTIFY_PR_SWAP:
		dev_info(rpmd->dev, "%s power role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_SINK) {
			dev_info(rpmd->dev, "%s swap power role to sink\n",
					    __func__);
			/*
			 * report charger plug-in without charger type detection
			 * to not interfering with USB2.0 communication
			 */

			/* toggle chg->pd_active to clean up the effect of
			 * smblib_uusb_removal() */
			val.intval = 10;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
			smblib_set_prop(rpmd, RT1711_PD_ACTIVE, &val);
#else
			smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_ACTIVE,
					&val);
#endif
			typec_set_pwr_role(rpmd->typec_port, TYPEC_SINK);
		} else if (noti->swap_state.new_role == PD_ROLE_SOURCE) {
			dev_info(rpmd->dev, "%s swap power role to source\n",
					    __func__);
			/* report charger plug-out */

			typec_set_pwr_role(rpmd->typec_port, TYPEC_SOURCE);
		}
		break;
	case TCP_NOTIFY_DR_SWAP:
		dev_info(rpmd->dev, "%s data role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role == PD_ROLE_UFP) {
			dev_info(rpmd->dev, "%s swap data role to device\n",
					    __func__);
			/*
			 * disable host connection,
			 * and enable device connection
			 */
			cancel_delayed_work_sync(&rpmd->usb_dwork);
			rpmd->usb_dr = DR_HOST_TO_DEVICE;
			schedule_delayed_work(&rpmd->usb_dwork, 0);
			typec_set_data_role(rpmd->typec_port, TYPEC_DEVICE);
		} else if (noti->swap_state.new_role == PD_ROLE_DFP) {
			dev_info(rpmd->dev, "%s swap data role to host\n",
					    __func__);
			/*
			 * disable device connection,
			 * and enable host connection
			 */
			cancel_delayed_work_sync(&rpmd->usb_dwork);
			rpmd->usb_dr = DR_DEVICE_TO_HOST;
			schedule_delayed_work(&rpmd->usb_dwork, 0);
			typec_set_data_role(rpmd->typec_port, TYPEC_HOST);
		}
		break;
	case TCP_NOTIFY_VCONN_SWAP:
		dev_info(rpmd->dev, "%s vconn role swap, new role = %d\n",
				    __func__, noti->swap_state.new_role);
		if (noti->swap_state.new_role) {
			dev_info(rpmd->dev, "%s swap vconn role to on\n",
					    __func__);
			typec_set_vconn_role(rpmd->typec_port, TYPEC_SOURCE);
		} else {
			dev_info(rpmd->dev, "%s swap vconn role to off\n",
					    __func__);
			typec_set_vconn_role(rpmd->typec_port, TYPEC_SINK);
		}
		break;
	case TCP_NOTIFY_EXT_DISCHARGE:
		dev_info(rpmd->dev, "%s ext discharge = %d\n",
				    __func__, noti->en_state.en);
		/* enable/disable VBUS discharge */
		break;
	case TCP_NOTIFY_PD_STATE:
		dev_info(rpmd->dev, "%s pd state = %d\n",
				    __func__, noti->pd_state.connected);
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			break;
		case PD_CONNECT_HARD_RESET:
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
		case PD_CONNECT_PE_READY_SNK_APDO:
			ret = tcpm_inquire_dpm_flags(rpmd->tcpc);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
			val.intval = ret & DPM_FLAGS_PARTNER_USB_SUSPEND ? 1 : 0;
			smblib_set_prop(rpmd, RT1711_PD_USB_SUSPEND_SUPPORTED, &val);
#else
			val.intval = ret & DPM_FLAGS_PARTNER_USB_SUSPEND ?
				     1 : 0;
			smblib_set_prop(rpmd,
				POWER_SUPPLY_PROP_PD_USB_SUSPEND_SUPPORTED,
					&val);
#endif
		case PD_CONNECT_PE_READY_SRC:
		case PD_CONNECT_PE_READY_SRC_PD30:
			/* update chg->pd_active */
			val.intval = noti->pd_state.connected ==
				     PD_CONNECT_PE_READY_SNK_APDO ?
				     POWER_SUPPLY_PD_PPS_ACTIVE :
				     POWER_SUPPLY_PD_ACTIVE;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
			smblib_set_prop(rpmd, RT1711_PD_ACTIVE, &val);
#else
			smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_ACTIVE,
					&val);
#endif
			pd_sink_set_vol_and_cur(rpmd, rpmd->sink_mv_old,
						rpmd->sink_ma_old,
						TCP_VBUS_CTRL_PD_STANDBY);

			typec_set_pwr_opmode(rpmd->typec_port,
					     TYPEC_PWR_MODE_PD);
			if (!rpmd->partner)
				break;
			ret = tcpm_inquire_pd_partner_inform(rpmd->tcpc,
							     partner_vdos);
			if (ret != TCPM_SUCCESS)
				break;
			rpmd->partner_identity.id_header = partner_vdos[0];
			rpmd->partner_identity.cert_stat = partner_vdos[1];
			rpmd->partner_identity.product = partner_vdos[2];
			typec_partner_set_identity(rpmd->partner);
			break;
		};
		break;
	case TCP_NOTIFY_HARD_RESET_STATE:
		switch (noti->hreset_state.state) {
		case TCP_HRESET_SIGNAL_SEND:
		case TCP_HRESET_SIGNAL_RECV:
			val.intval = 1;
			break;
		default:
			val.intval = 0;
			break;
		}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		smblib_set_prop(rpmd, RT1711_PD_IN_HARD_RESET, &val);
#else
		smblib_set_prop(rpmd, POWER_SUPPLY_PROP_PD_IN_HARD_RESET, &val);
#endif
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static int tcpc_typec_try_role(struct typec_port *port, int role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
#else
static int tcpc_typec_try_role(const struct typec_capability *cap, int role)
{
	struct rt_pd_manager_data *rpmd =
		container_of(cap, struct rt_pd_manager_data, typec_caps);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */
	uint8_t typec_role = TYPEC_ROLE_UNKNOWN;

	dev_info(rpmd->dev, "%s role = %d\n", __func__, role);

	switch (role) {
	case TYPEC_NO_PREFERRED_ROLE:
		typec_role = TYPEC_ROLE_DRP;
		break;
	case TYPEC_SINK:
		typec_role = TYPEC_ROLE_TRY_SNK;
		break;
	case TYPEC_SOURCE:
		typec_role = TYPEC_ROLE_TRY_SRC;
		break;
	default:
		return 0;
	}

	return tcpm_typec_change_role_postpone(rpmd->tcpc, typec_role, true);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static int tcpc_typec_dr_set(struct typec_port *port, enum typec_data_role role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
#else
static int tcpc_typec_dr_set(const struct typec_capability *cap,
			     enum typec_data_role role)
{
	struct rt_pd_manager_data *rpmd =
		container_of(cap, struct rt_pd_manager_data, typec_caps);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */
	int ret = 0;
	uint8_t data_role = tcpm_inquire_pd_data_role(rpmd->tcpc);
	bool do_swap = false;

	dev_info(rpmd->dev, "%s role = %d\n", __func__, role);

	if (role == TYPEC_HOST) {
		if (data_role == PD_ROLE_UFP) {
			do_swap = true;
			data_role = PD_ROLE_DFP;
		}
	} else if (role == TYPEC_DEVICE) {
		if (data_role == PD_ROLE_DFP) {
			do_swap = true;
			data_role = PD_ROLE_UFP;
		}
	} else {
		dev_err(rpmd->dev, "%s invalid role\n", __func__);
		return -EINVAL;
	}

	if (do_swap) {
		ret = tcpm_dpm_pd_data_swap(rpmd->tcpc, data_role, NULL);
		if (ret != TCPM_SUCCESS) {
			dev_err(rpmd->dev, "%s data role swap fail(%d)\n",
					   __func__, ret);
			return -EPERM;
		}
	}

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static int tcpc_typec_pr_set(struct typec_port *port, enum typec_role role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
#else
static int tcpc_typec_pr_set(const struct typec_capability *cap,
			     enum typec_role role)
{
	struct rt_pd_manager_data *rpmd =
		container_of(cap, struct rt_pd_manager_data, typec_caps);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */
	int ret = 0;
	uint8_t power_role = tcpm_inquire_pd_power_role(rpmd->tcpc);
	bool do_swap = false;

	dev_info(rpmd->dev, "%s role = %d\n", __func__, role);

	if (role == TYPEC_SOURCE) {
		if (power_role == PD_ROLE_SINK) {
			do_swap = true;
			power_role = PD_ROLE_SOURCE;
		}
	} else if (role == TYPEC_SINK) {
		if (power_role == PD_ROLE_SOURCE) {
			do_swap = true;
			power_role = PD_ROLE_SINK;
		}
	} else {
		dev_err(rpmd->dev, "%s invalid role\n", __func__);
		return -EINVAL;
	}

	if (do_swap) {
		ret = tcpm_dpm_pd_power_swap(rpmd->tcpc, power_role, NULL);
		if (ret == TCPM_ERROR_NO_PD_CONNECTED)
			ret = tcpm_typec_role_swap(rpmd->tcpc);
		if (ret != TCPM_SUCCESS) {
			dev_err(rpmd->dev, "%s power role swap fail(%d)\n",
					   __func__, ret);
			return -EPERM;
		}
	}

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static int tcpc_typec_vconn_set(struct typec_port *port, enum typec_role role)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
#else
static int tcpc_typec_vconn_set(const struct typec_capability *cap,
				enum typec_role role)
{
	struct rt_pd_manager_data *rpmd =
		container_of(cap, struct rt_pd_manager_data, typec_caps);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */
	int ret = 0;
	uint8_t vconn_role = tcpm_inquire_pd_vconn_role(rpmd->tcpc);
	bool do_swap = false;

	dev_info(rpmd->dev, "%s role = %d\n", __func__, role);

	if (role == TYPEC_SOURCE) {
		if (vconn_role == PD_ROLE_VCONN_OFF) {
			do_swap = true;
			vconn_role = PD_ROLE_VCONN_ON;
		}
	} else if (role == TYPEC_SINK) {
		if (vconn_role == PD_ROLE_VCONN_ON) {
			do_swap = true;
			vconn_role = PD_ROLE_VCONN_OFF;
		}
	} else {
		dev_err(rpmd->dev, "%s invalid role\n", __func__);
		return -EINVAL;
	}

	if (do_swap) {
		ret = tcpm_dpm_pd_vconn_swap(rpmd->tcpc, vconn_role, NULL);
		if (ret != TCPM_SUCCESS) {
			dev_err(rpmd->dev, "%s vconn role swap fail(%d)\n",
					   __func__, ret);
			return -EPERM;
		}
	}

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static int tcpc_typec_port_type_set(struct typec_port *port,
				    enum typec_port_type type)
{
	struct rt_pd_manager_data *rpmd = typec_get_drvdata(port);
	const struct typec_capability *cap = &rpmd->typec_caps;
#else
static int tcpc_typec_port_type_set(const struct typec_capability *cap,
				    enum typec_port_type type)
{
	struct rt_pd_manager_data *rpmd =
		container_of(cap, struct rt_pd_manager_data, typec_caps);
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */
	bool as_sink = tcpc_typec_is_act_as_sink_role(rpmd->tcpc);
	uint8_t typec_role = TYPEC_ROLE_UNKNOWN;

	dev_info(rpmd->dev, "%s type = %d, as_sink = %d\n",
			    __func__, type, as_sink);

	switch (type) {
	case TYPEC_PORT_SNK:
		if (as_sink)
			return 0;
		break;
	case TYPEC_PORT_SRC:
		if (!as_sink)
			return 0;
		break;
	case TYPEC_PORT_DRP:
		if (cap->prefer_role == TYPEC_SOURCE)
			typec_role = TYPEC_ROLE_TRY_SRC;
		else if (cap->prefer_role == TYPEC_SINK)
			typec_role = TYPEC_ROLE_TRY_SNK;
		else
			typec_role = TYPEC_ROLE_DRP;
		return tcpm_typec_change_role(rpmd->tcpc, typec_role);
	default:
		return 0;
	}

	return tcpm_typec_role_swap(rpmd->tcpc);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
const struct typec_operations tcpc_typec_ops = {
	.try_role = tcpc_typec_try_role,
	.dr_set = tcpc_typec_dr_set,
	.pr_set = tcpc_typec_pr_set,
	.vconn_set = tcpc_typec_vconn_set,
	.port_type_set = tcpc_typec_port_type_set,
};
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */

static int typec_init(struct rt_pd_manager_data *rpmd)
{
	int ret = 0;

	rpmd->typec_caps.type = TYPEC_PORT_DRP;
	rpmd->typec_caps.data = TYPEC_PORT_DRD;
	rpmd->typec_caps.revision = 0x0120;
	rpmd->typec_caps.pd_revision = 0x0300;
	switch (rpmd->tcpc->desc.role_def) {
	case TYPEC_ROLE_SRC:
	case TYPEC_ROLE_TRY_SRC:
		rpmd->typec_caps.prefer_role = TYPEC_SOURCE;
		break;
	case TYPEC_ROLE_SNK:
	case TYPEC_ROLE_TRY_SNK:
		rpmd->typec_caps.prefer_role = TYPEC_SINK;
		break;
	default:
		rpmd->typec_caps.prefer_role = TYPEC_NO_PREFERRED_ROLE;
		break;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	rpmd->typec_caps.driver_data = rpmd;
	rpmd->typec_caps.ops = &tcpc_typec_ops;
#else
	rpmd->typec_caps.try_role = tcpc_typec_try_role;
	rpmd->typec_caps.dr_set = tcpc_typec_dr_set;
	rpmd->typec_caps.pr_set = tcpc_typec_pr_set;
	rpmd->typec_caps.vconn_set = tcpc_typec_vconn_set;
	rpmd->typec_caps.port_type_set = tcpc_typec_port_type_set;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */

	rpmd->typec_port = typec_register_port(rpmd->dev, &rpmd->typec_caps);
	if (IS_ERR(rpmd->typec_port)) {
		ret = PTR_ERR(rpmd->typec_port);
		dev_err(rpmd->dev, "%s typec register port fail(%d)\n",
				   __func__, ret);
		goto out;
	}

	rpmd->partner_desc.identity = &rpmd->partner_identity;
out:
	return ret;
}

#ifdef CONFIG_TCPC_NOTIFIER_LATE_SYNC
#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
static int fg_bat_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct pd_port *pd_port = container_of(nb, struct pd_port, fg_bat_nb);
	struct tcpc_device *tcpc = pd_port->tcpc;

	switch (event) {
	case EVENT_BATTERY_PLUG_OUT:
		dev_info(&tcpc->dev, "%s: fg battery absent\n", __func__);
		schedule_work(&pd_port->fg_bat_work);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */

static int __tcpc_class_complete_work(struct device *dev, void *data)
{
	struct tcpc_device *tcpc = dev_get_drvdata(dev);
#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
	struct notifier_block *fg_bat_nb = &tcpc->pd_port.fg_bat_nb;
	int ret = 0;
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */

	if (tcpc != NULL) {
		pr_info("%s = %s\n", __func__, dev_name(dev));
#if 1
		tcpc_device_irq_enable(tcpc);
#else
		schedule_delayed_work(&tcpc->init_work,
			msecs_to_jiffies(1000));
#endif

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_RECV_BAT_ABSENT_NOTIFY
		fg_bat_nb->notifier_call = fg_bat_notifier_call;
		ret = register_battery_notifier(fg_bat_nb);
		if (ret < 0) {
			pr_notice("%s: register bat notifier fail\n", __func__);
			return -EINVAL;
		}
#endif /* CONFIG_RECV_BAT_ABSENT_NOTIFY */
#endif /* CONFIG_USB_POWER_DELIVERY */
	}
	return 0;
}

static int tcpc_class_complete_init(void)
{
	if (!IS_ERR(tcpc_class)) {
		class_for_each_device(tcpc_class, NULL, NULL,
			__tcpc_class_complete_work);
	}
	return 0;
}
#endif /* CONFIG_TCPC_NOTIFIER_LATE_SYNC */

static int rt_pd_manager_probe(struct platform_device *pdev)
{
	int ret = 0;
	static int probe_cnt = 0;
	struct rt_pd_manager_data *rpmd = NULL;

	dev_info(&pdev->dev, "%s (%s) probe_cnt = %d\n",
			     __func__, RT_PD_MANAGER_VERSION, ++probe_cnt);

	rpmd = devm_kzalloc(&pdev->dev, sizeof(*rpmd), GFP_KERNEL);
	if (!rpmd)
		return -ENOMEM;

	rpmd->dev = &pdev->dev;

	ret = extcon_init(rpmd);
	if (ret) {
		dev_err(rpmd->dev, "%s init extcon fail(%d)\n", __func__, ret);
		ret = -EPROBE_DEFER;
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_init_extcon;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	rpmd->iio_channels = devm_kcalloc(rpmd->dev, POWER_SUPPLY_IIO_PROP_MAX,
					  sizeof(rpmd->iio_channels[0]),
					  GFP_KERNEL);
	if (!rpmd->iio_channels) {
		dev_err(rpmd->dev, "%s kcalloc fail\n", __func__);
		ret = -EPROBE_DEFER;
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_get_iio_chan;
	}
#else
	rpmd->usb_psy = power_supply_get_by_name("usb");
	if (!rpmd->usb_psy) {
		dev_err(rpmd->dev, "%s get usb psy fail\n", __func__);
		ret = -EPROBE_DEFER;
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_get_usb_psy;
	}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */

	rpmd->tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (!rpmd->tcpc) {
		dev_err(rpmd->dev, "%s get tcpc dev fail\n", __func__);
		ret = -EPROBE_DEFER;
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_get_tcpc_dev;
	}

	INIT_DELAYED_WORK(&rpmd->usb_dwork, usb_dwork_handler);
	rpmd->usb_dr = DR_IDLE;
	rpmd->usb_type_polling_cnt = 0;
	rpmd->sink_mv_old = -1;
	rpmd->sink_ma_old = -1;
	rpmd->shutdown_flag = false;

	ret = typec_init(rpmd);
	if (ret < 0) {
		dev_err(rpmd->dev, "%s init typec fail(%d)\n", __func__, ret);
		ret = -EPROBE_DEFER;
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_init_typec;
	}

	rpmd->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(rpmd->tcpc, &rpmd->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		dev_err(rpmd->dev, "%s register tcpc notifier fail(%d)\n",
				   __func__, ret);
		ret = -EPROBE_DEFER;
		if (probe_cnt >= PROBE_CNT_MAX)
			goto out;
		else
			goto err_reg_tcpc_notifier;
	}
#ifdef CONFIG_TCPC_NOTIFIER_LATE_SYNC
	tcpc_class_complete_init();
#endif /* CONFIG_TCPC_NOTIFIER_LATE_SYNC */
out:
	platform_set_drvdata(pdev, rpmd);
	dev_info(rpmd->dev, "%s %s!!\n", __func__, ret == -EPROBE_DEFER ?
			    "Over probe cnt max" : "OK");
	return 0;

err_reg_tcpc_notifier:
	typec_unregister_port(rpmd->typec_port);
err_init_typec:
err_get_tcpc_dev:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
err_get_iio_chan:
#else
	power_supply_put(rpmd->usb_psy);
err_get_usb_psy:
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)) */
err_init_extcon:
	return ret;
}

static int rt_pd_manager_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct rt_pd_manager_data *rpmd = platform_get_drvdata(pdev);

	if (!rpmd)
		return -EINVAL;

	ret = unregister_tcp_dev_notifier(rpmd->tcpc, &rpmd->pd_nb,
					  TCP_NOTIFY_TYPE_ALL);
	if (ret < 0)
		dev_err(rpmd->dev, "%s unregister tcpc notifier fail(%d)\n",
				   __func__, ret);
	typec_unregister_port(rpmd->typec_port);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	if (rpmd->usb_psy)
		power_supply_put(rpmd->usb_psy);
#endif /* (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)) */

	return ret;
}

static void rt_pd_manager_shutdown(struct platform_device *pdev)
{
	struct rt_pd_manager_data *rpmd = platform_get_drvdata(pdev);

	dev_err(rpmd->dev, "%s rt_pd_manager_shutdown 11\n",
				   __func__);
	if (!rpmd)
		return;

	rpmd->shutdown_flag = true;

	return;
}

static const struct of_device_id rt_pd_manager_of_match[] = {
	{ .compatible = "richtek,rt-pd-manager" },
	{ }
};
MODULE_DEVICE_TABLE(of, rt_pd_manager_of_match);

static struct platform_driver rt_pd_manager_driver = {
	.driver = {
		.name = "rt-pd-manager",
		.of_match_table = of_match_ptr(rt_pd_manager_of_match),
	},
	.probe = rt_pd_manager_probe,
	.remove = rt_pd_manager_remove,
	.shutdown = rt_pd_manager_shutdown,
};
module_platform_driver(rt_pd_manager_driver);

MODULE_AUTHOR("Jeff Chang");
MODULE_DESCRIPTION("Richtek pd manager driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(RT_PD_MANAGER_VERSION);

/*
 * Release Note
 * 0.0.8
 * (1) Add support for msm-5.4
 *
 * 0.0.7
 * (1) Set properties of usb_psy
 *
 * 0.0.6
 * (1) Register typec_port
 *
 * 0.0.5
 * (1) Control USB mode in delayed work
 * (2) Remove param_lock because pd_tcp_notifier_call() is single-entry
 *
 * 0.0.4
 * (1) Limit probe count
 *
 * 0.0.3
 * (1) Add extcon for controlling USB mode
 *
 * 0.0.2
 * (1) Initialize old_state and new_state
 *
 * 0.0.1
 * (1) Add all possible notification events
 */
