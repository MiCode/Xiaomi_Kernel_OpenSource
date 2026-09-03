// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.Lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_wakeup.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_vchg_detect.h>

static enum power_supply_usb_type sprd_vchg_get_bc1p2_type(struct sprd_vchg_info *info)
{
	enum power_supply_usb_type usb_type  = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	enum usb_charger_type type;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return usb_type;
	}

	type = info->usb_phy->chg_type;

	switch (type) {
	case SDP_TYPE:
		usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;

	case DCP_TYPE:
		usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	case CDP_TYPE:
		usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;

	default:
		usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	return usb_type;
}

static bool sprd_vchg_is_charger_online(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return false;
	}

	return info->chgr_online;
}

static void sprd_vchg_work(struct work_struct *data)
{
	struct sprd_vchg_info *info = container_of(data, struct sprd_vchg_info,
						   sprd_vchg_work);
	int retry_cnt = 12;
	u32 events;

	spin_lock(&info->event_lock);
	while (info->notify_cmd) {
		events = info->notify_cmd;
		info->notify_cmd = 0;
		spin_unlock(&info->event_lock);

		if ((events & SPRD_VCHG_IGNORE_HARD_RESET_CMD) && info->chgr_online)
			cm_notify_event(info->psy, CM_EVENT_IGNORE_HARD_RESET, NULL);

		if (events & SPRD_VCHG_ONLINE_CHANGE_CMD) {
			if (info->use_typec_extcon && info->chgr_online) {
				/* if use typec extcon notify charger,
				 * wait for BC1.2 detect charger type.
				 */
				s64 curr = ktime_to_ms(ktime_get_boottime());

				while (info->usb_limit == -EINVAL && info->chgr_online &&
				       retry_cnt > 0 && curr >= 10000) {
					retry_cnt--;
					msleep(50);
				}
				dev_info(info->dev, "retry_cnt = %d\n", retry_cnt);
			}

			dev_info(info->dev, "%s: charger type = %d, charger online = %d, events = %d\n",
				 SPRD_VCHG_TAG, info->usb_phy->chg_type, info->chgr_online, events);
			cm_notify_event(info->psy, CM_EVENT_CHG_START_STOP, NULL);
		}

		if ((events & SPRD_VCHG_USB_LIMIT_CMD) && info->chgr_online)
			cm_notify_event(info->psy, CM_EVENT_UPDATE_USB_LIMINT, NULL);

		spin_lock(&info->event_lock);
	}
	spin_unlock(&info->event_lock);
}

static int sprd_vchg_change(struct notifier_block *nb, unsigned long limit, void *data)
{
	struct sprd_vchg_info *info = container_of(nb, struct sprd_vchg_info, usb_notify);
	bool chgr_online = false;
	u32 events = 0, events1 = 0;
	unsigned long usb_limit = limit * 1000;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return NOTIFY_OK;
	}

	if (info->shutdown_flag)
		return NOTIFY_OK;

	if (info->usb_phy->chg_state == USB_CHARGER_PRESENT)
		chgr_online = true;

	if (info->usb_phy->chg_type == CDP_TYPE && usb_limit <= SPRD_VCHG_SDP_LIMIT_CUR_UA)
		usb_limit = info->usb_limit;

	if (info->usb_limit != usb_limit) {
		if (chgr_online) {
			if (info->usb_limit == -EINVAL)
				events |= SPRD_VCHG_ONLINE_CHANGE_CMD;
			else if (info->usb_phy->chg_type == SDP_TYPE)
				events |= SPRD_VCHG_USB_LIMIT_CMD;
			info->usb_limit = usb_limit;
		} else {
			info->usb_limit = -EINVAL;
			events |= SPRD_VCHG_ONLINE_CHANGE_CMD;
		}

		dev_err(info->dev, "%s, usb_limit = %duA\n", SPRD_VCHG_TAG, info->usb_limit);
	}

	if (info->use_typec_extcon)
		goto done;

	if (!info->pd_hard_reset) {
		info->chgr_online = chgr_online;
		if (info->typec_online) {
			info->typec_online = false;
			dev_info(info->dev, "%s, typec not disconnect while pd hard reset.\n",
				 SPRD_VCHG_TAG);
		}

		__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
		events |= SPRD_VCHG_ONLINE_CHANGE_CMD;
	} else {
		info->pd_hard_reset = false;
		if (chgr_online) {
			dev_err(info->dev, "%s: The adapter is not PD adapter.\n", SPRD_VCHG_TAG);
			info->chgr_online = chgr_online;
			__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
			events |= SPRD_VCHG_ONLINE_CHANGE_CMD;
		} else if (!extcon_get_state(info->typec_extcon, EXTCON_USB)) {
			dev_err(info->dev, "%s: typec disconnect before pd hard reset.\n",
				SPRD_VCHG_TAG);
			info->chgr_online = false;
			__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
			events |= SPRD_VCHG_ONLINE_CHANGE_CMD;
		} else {
			info->typec_online = true;
			events |= SPRD_VCHG_IGNORE_HARD_RESET_CMD;
			dev_err(info->dev, "%s, USB PD hard reset, ignore vbus off\n",
				SPRD_VCHG_TAG);
			cancel_delayed_work_sync(&info->pd_hard_reset_work);
		}
	}

done:
	spin_lock(&info->event_lock);
	info->notify_cmd |= events;
	events1 = info->notify_cmd;
	spin_unlock(&info->event_lock);
	if (events1)
		schedule_work(&info->sprd_vchg_work);

	return NOTIFY_OK;
}

static void sprd_vchg_detect_status(struct sprd_vchg_info *info)
{
	int state = 0;
	unsigned int min, max;

	if (info->use_typec_extcon) {
		state = extcon_get_state(info->typec_extcon, SPRD_VCHG_EXTCON_SINK);
		if (state < 0) {
			dev_err(info->dev, "failed to get extcon sink state（%d）\n", state);
			return;
		}

		if (state == 0)
			return;

		info->is_sink = state;

		if (info->is_sink)
			info->chgr_online = true;
	} else {
		/*
		 * If the USB charger status has been USB_CHARGER_PRESENT before
		 * registering the notifier, we should start to charge with getting
		 * the charge current.
		 */
		if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
			return;

		info->chgr_online = true;
	}

	if (info->chgr_online) {
		usb_phy_get_charger_current(info->usb_phy, &min, &max);
		info->usb_limit = max;
	}

	spin_lock(&info->event_lock);
	info->notify_cmd |= SPRD_VCHG_ONLINE_CHANGE_CMD;
	spin_unlock(&info->event_lock);
	schedule_work(&info->sprd_vchg_work);
}

#if IS_ENABLED(CONFIG_SC27XX_PD)
static int sprd_vchg_pd_extcon_event(struct notifier_block *nb, unsigned long event, void *param)
{
	struct sprd_vchg_info *info = container_of(nb, struct sprd_vchg_info, pd_extcon_nb);
	int pd_extcon_status;

	if (info->shutdown_flag)
		return NOTIFY_OK;

	if (info->pd_hard_reset) {
		dev_info(info->dev, "%s: Already receive USB PD hardreset\n", SPRD_VCHG_TAG);
		return NOTIFY_OK;
	}

	pd_extcon_status = extcon_get_state(info->pd_extcon, EXTCON_CHG_USB_PD);
	if (pd_extcon_status == info->pd_extcon_status)
		return NOTIFY_OK;

	info->pd_extcon_status = pd_extcon_status;

	info->pd_hard_reset = true;

	dev_info(info->dev, "%s: receive USB PD hard reset request\n", SPRD_VCHG_TAG);

	schedule_delayed_work(&info->pd_hard_reset_work,
			      msecs_to_jiffies(SPRD_VCHG_PD_HARD_RESET_MS));

	return NOTIFY_OK;
}

static int sprd_vchg_typec_extcon_event(struct notifier_block *nb, unsigned long event, void *param)
{
	struct sprd_vchg_info *info = container_of(nb, struct sprd_vchg_info, typec_extcon_nb);
	int state = 0;

	if (info->shutdown_flag)
		return NOTIFY_OK;

	if (info->typec_online) {
		dev_info(info->dev, "%s: typec status change.\n", SPRD_VCHG_TAG);
		schedule_delayed_work(&info->typec_extcon_work, 0);
	}

	if (info->use_typec_extcon && !info->typec_online) {
		state = extcon_get_state(info->typec_extcon, SPRD_VCHG_EXTCON_SINK);
		if (state < 0) {
			dev_err(info->dev, "failed to get extcon sink state, state = %d\n", state);
			return NOTIFY_OK;
		}

		if (info->is_sink == state)
			return NOTIFY_OK;

		dev_info(info->dev, "is_sink = %d, state = %d\n", info->is_sink, state);

		info->is_sink = state;

		if (info->is_sink)
			info->chgr_online = true;
		else
			info->chgr_online = false;

		__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);

		spin_lock(&info->event_lock);
		info->notify_cmd |= SPRD_VCHG_ONLINE_CHANGE_CMD;
		spin_unlock(&info->event_lock);
		schedule_work(&info->sprd_vchg_work);
	}

	return NOTIFY_OK;
}

static void sprd_vchg_pd_hard_reset_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_vchg_info *info = container_of(dwork, struct sprd_vchg_info,
						   pd_hard_reset_work);

	if (!info->pd_hard_reset) {
		if (info->usb_phy->chg_state == USB_CHARGER_PRESENT)
			return;

		dev_info(info->dev, "%s: Not USB PD hard reset, charger out\n", SPRD_VCHG_TAG);

		info->chgr_online = false;
		spin_lock(&info->event_lock);
		info->notify_cmd |= SPRD_VCHG_ONLINE_CHANGE_CMD;
		spin_unlock(&info->event_lock);
		schedule_work(&info->sprd_vchg_work);
	}

	info->pd_hard_reset = false;
}

static void sprd_vchg_typec_extcon_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_vchg_info *info = container_of(dwork, struct sprd_vchg_info,
						   typec_extcon_work);

	if (!extcon_get_state(info->typec_extcon, EXTCON_USB)) {
		info->chgr_online = false;
		info->typec_online = false;
		__pm_wakeup_event(info->sprd_vchg_ws, SPRD_VCHG_WAKE_UP_MS);
		dev_info(info->dev, "%s: typec disconnect while pd hard reset.\n", SPRD_VCHG_TAG);
		spin_lock(&info->event_lock);
		info->notify_cmd |= SPRD_VCHG_ONLINE_CHANGE_CMD;
		spin_unlock(&info->event_lock);
		schedule_work(&info->sprd_vchg_work);
	}
}

static void sprd_vchg_detect_pd_extcon_status(struct sprd_vchg_info *info)
{
	if (!info->pd_extcon_enable)
		return;

	info->pd_extcon_status = extcon_get_state(info->pd_extcon, EXTCON_CHG_USB_PD);
	if (info->pd_extcon_status) {
		info->pd_hard_reset = true;
		dev_info(info->dev, "%s: Detect USB PD hard reset request\n", SPRD_VCHG_TAG);
		schedule_delayed_work(&info->pd_hard_reset_work,
				      msecs_to_jiffies(SPRD_VCHG_PD_HARD_RESET_MS));
	}
}

#else
static void sprd_vchg_detect_pd_extcon_status(struct sprd_vchg_info *info)
{
}
#endif

static void sprd_update_vchg_info(struct sprd_vchg_info *info,
				  enum sprd_vchg_info_struct_update_command cmd,
				  int updated_value)
{
	bool need_update_flag = false;

	if (!info) {
		pr_err("%s, %s[%d], info NULL pointer!!!\n", SPRD_VCHG_TAG, __func__, __LINE__);
		return;
	}

	if (!need_update_flag && (cmd & SPRD_VCHG_VAL_CMD_USB_LIMIT)) {
		need_update_flag = true;
		info->usb_limit = updated_value;
	}

	if (need_update_flag)
		dev_info(info->dev, "%s, %s, %s: %d\n",
			 SPRD_VCHG_TAG, __func__, sprd_vchg_info_struct_val_name[cmd],
			 updated_value);
}

static int sprd_vchg_detect_parse_dts(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: cm NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return -ENOMEM;
	}

	info->use_typec_extcon = device_property_read_bool(info->dev, "use-typec-extcon");
	dev_info(info->dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	info->usb_phy = devm_usb_get_phy_by_phandle(info->dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(info->dev, "%s: failed to find USB phy\n", SPRD_VCHG_TAG);
		return -EINVAL;
	}

	info->pd_extcon_enable = device_property_read_bool(info->dev, "pd-extcon-enable");

	return 0;
}

static int sprd_vchg_detect_init(struct sprd_vchg_info *info, struct power_supply *psy)
{
	struct device *dev;
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return -ENOMEM;
	}

	info->usb_limit = -EINVAL;
	dev = info->dev;
	info->sprd_vchg_ws = wakeup_source_create("sprd_vchg_wakelock");
	wakeup_source_add(info->sprd_vchg_ws);

	info->psy = psy;
	INIT_WORK(&info->sprd_vchg_work, sprd_vchg_work);
	spin_lock_init(&info->event_lock);

	info->usb_notify.notifier_call = sprd_vchg_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "%s:failed to register notifier:%d\n", SPRD_VCHG_TAG, ret);
		ret = -EINVAL;
		goto remove_wakeup;
	}

#if IS_ENABLED(CONFIG_SC27XX_PD)
	if (!info->pd_extcon_enable)
		return 0;

	if (!of_property_read_bool(dev->of_node, "extcon"))
		return 0;

	INIT_DELAYED_WORK(&info->pd_hard_reset_work, sprd_vchg_pd_hard_reset_work);

	info->pd_extcon = extcon_get_edev_by_phandle(dev, 1);
	if (IS_ERR(info->pd_extcon)) {
		dev_err(dev, "%s: failed to find pd extcon device.\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}

	info->pd_extcon_nb.notifier_call = sprd_vchg_pd_extcon_event;
	ret = devm_extcon_register_notifier_all(dev, info->pd_extcon, &info->pd_extcon_nb);
	if (ret) {
		dev_err(dev, "%s:Can't register pd extcon\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}

	INIT_DELAYED_WORK(&info->typec_extcon_work, sprd_vchg_typec_extcon_work);
	info->typec_extcon = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(info->typec_extcon)) {
		dev_err(dev, "%s: failed to find typec extcon device.\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}

	info->typec_extcon_nb.notifier_call = sprd_vchg_typec_extcon_event;
	ret = devm_extcon_register_notifier_all(dev, info->typec_extcon, &info->typec_extcon_nb);
	if (ret) {
		dev_err(dev, "%s:Can't register typec extcon\n", SPRD_VCHG_TAG);
		ret = -EINVAL;
		goto err_reg_usb;
	}
#endif

	sprd_vchg_detect_status(info);
	sprd_vchg_detect_pd_extcon_status(info);

	return 0;

#if IS_ENABLED(CONFIG_SC27XX_PD)
err_reg_usb:
	usb_unregister_notifier(info->usb_phy, &info->usb_notify);
#endif

remove_wakeup:
	wakeup_source_remove(info->sprd_vchg_ws);

	return ret;
}

static void sprd_vchg_suspend(struct sprd_vchg_info *info)
{
}

static void sprd_vchg_resume(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return;
	}
}

static void sprd_vchg_remove(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return;
	}

	if (!info->use_typec_extcon)
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);

#if IS_ENABLED(CONFIG_SC27XX_PD)
	cancel_delayed_work_sync(&info->pd_hard_reset_work);
	cancel_delayed_work_sync(&info->typec_extcon_work);
#endif
	cancel_work_sync(&info->sprd_vchg_work);
}

static void sprd_vchg_shutdown(struct sprd_vchg_info *info)
{
	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", SPRD_VCHG_TAG, __LINE__);
		return;
	}

	info->shutdown_flag = true;

	if (!info->use_typec_extcon)
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);

#if IS_ENABLED(CONFIG_SC27XX_PD)
	cancel_delayed_work_sync(&info->pd_hard_reset_work);
	cancel_delayed_work_sync(&info->typec_extcon_work);
#endif
	cancel_work_sync(&info->sprd_vchg_work);
}

struct sprd_vchg_info *sprd_vchg_info_register(struct device *dev)
{
	struct sprd_vchg_info *info = NULL;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "%s: Fail to malloc memory for sprd_vchg_info\n", SPRD_VCHG_TAG);
		return ERR_PTR(-ENOMEM);
	}

	info->ops = devm_kzalloc(dev, sizeof(*info->ops), GFP_KERNEL);
	if (!info->ops) {
		dev_err(dev, "%s: Fail to malloc memory for ops\n", SPRD_VCHG_TAG);
		return ERR_PTR(-ENOMEM);
	}

	info->ops->parse_dts = sprd_vchg_detect_parse_dts;
	info->ops->init = sprd_vchg_detect_init;
	info->ops->update_vchg_info = sprd_update_vchg_info;
	info->ops->is_charger_online = sprd_vchg_is_charger_online;
	info->ops->get_bc1p2_type = sprd_vchg_get_bc1p2_type;
	info->ops->suspend = sprd_vchg_suspend;
	info->ops->resume = sprd_vchg_resume;
	info->ops->remove = sprd_vchg_remove;
	info->ops->shutdown = sprd_vchg_shutdown;
	info->dev = dev;

	return info;
}
EXPORT_SYMBOL_GPL(sprd_vchg_info_register);
