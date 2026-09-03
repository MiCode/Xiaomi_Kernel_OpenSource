// SPDX-License-Identifier: GPL-2.0+
/*
 * sprd-bc1p2.c -- USB BC1.2 handling
 *
 * Copyright (C) 2022 Chen Yongzhi <yongzhi.chen@unisoc.com>
 */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/power/sprd-bc1p2.h>
#include <linux/power_supply.h>
#include <linux/power/sprd_fchg_extcon.h>
#include <linux/power/charger-manager.h>
#include "../../usb/musb/musb_sprd.h"
#include "../../power/supply/lc_charger_class.h"

#define THIRDPART_BUCK_IC

static const struct sprd_bc1p2_data sc2720_data = {
	.charge_status = SC2720_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2720_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data sc2721_data = {
	.charge_status = SC2721_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2721_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data sc2730_data = {
	.charge_status = SC2730_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2730_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data sc2731_data = {
	.charge_status = SC2731_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2731_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = 0,
	.chg_int_delay_mask = SC27XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = SC27XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data ump9620_data = {
	.charge_status = UMP9620_CHARGE_STATUS,
	.chg_det_fgu_ctrl = UMP9620_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = UMP9620_CHG_BC1P2_CTRL2,
	.chg_int_delay_mask = UMP96XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = UMP96XX_CHG_INT_DELAY_OFFSET,
};

static const struct sprd_bc1p2_data ump518_data = {
	.charge_status = SC2730_CHARGE_STATUS,
	.chg_det_fgu_ctrl = SC2730_CHG_DET_FGU_CTRL,
	.chg_bc1p2_ctrl2 = UMP518_CHG_BC1P2_CTRL2,
	.chg_int_delay_mask = UMP96XX_CHG_INT_DELAY_MASK,
	.chg_int_delay_offset = UMP96XX_CHG_INT_DELAY_OFFSET,
};

static struct sprd_bc1p2 *bc1p2;

static int sprd_bc1p2_check_dev(struct sprd_bc1p2 *bc1p2)
{
	if(bc1p2->charger)
		return 0;

	bc1p2->charger = charger_find_dev_by_name("master_chg");
	if (!bc1p2->charger)
	{
		pr_err("%s failed to master_charge device\n", __func__);
		return 1;
	}
	return 0;
}

#ifndef THIRDPART_BUCK_IC
static int sprd_bc1p2_redetect_control(bool enable)
{
	int ret = 0;

	if (enable)
		ret = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_bc1p2_ctrl2,
					 SPRD_CHG_DET_EB_MASK,
					 SPRD_CHG_BC1P2_REDET_ENABLE);
	else
		ret = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_bc1p2_ctrl2,
					 SPRD_CHG_DET_EB_MASK,
					 SPRD_CHG_BC1P2_REDET_DISABLE);

	if (ret)
		pr_err("bc1p2:fail to enable/disable redetect, enable = %d\n", enable);
	return ret;
}
#endif

static bool sprd_bc1p2_vchg_is_present(unsigned int *chg_det_fgu_ctrl, unsigned int *bc1p2_status)
{
	int ret = 0;
#ifndef THIRDPART_BUCK_IC
	ret = regmap_read(bc1p2->regmap, bc1p2->data->chg_det_fgu_ctrl, chg_det_fgu_ctrl);
	if (ret)
		pr_warn("bc1p2:line%d: fail to get chg_det_fgu_ctrl, ret = %d\n", __LINE__, ret);

	ret = regmap_read(bc1p2->regmap, bc1p2->data->charge_status, bc1p2_status);
	if (ret) {
		pr_err("bc1p2:line%d: fail to get bc1p2_status, ret = %d\n", __LINE__, ret);
		return false;
	}

	if (*bc1p2_status & BIT_CHGR_INT)
		return true;
#else
	if(0 == sprd_bc1p2_check_dev(bc1p2))
	{
		charger_get_chg_present(bc1p2->charger, &ret);
	}
	if (ret)
		return true;
#endif
	return false;
}

static enum usb_charger_type sprd_bc1p2_detect(void)
{
	enum usb_charger_type type = UNKNOWN_TYPE;
#ifndef THIRDPART_BUCK_IC
	u32 status = 0, val = 0;
	int ret, vbus_cnt = 0, cnt = bc1p2->chg_detect_poll_count;

	if (bc1p2->chg_redet_poll_count)
		cnt = bc1p2->chg_redet_poll_count;

	bc1p2->chg_redet_poll_count = 0;

	do {
		if (bc1p2->shutdown)
			return UNKNOWN_TYPE;

		ret = regmap_read(bc1p2->regmap, bc1p2->data->charge_status, &val);
		if (ret) {
			type = UNKNOWN_TYPE;
			goto bc1p2_detect_end;
		}

		if (!(val & BIT_CHGR_INT) && vbus_cnt++ > SPRD_CHG_VBUS_POLL_COUNT) {
			pr_info("bc1p2:line%d absent bc1p2_status = 0x%x cnt = %d\n",
				__LINE__, val, cnt);
			type = UNKNOWN_TYPE;
			goto bc1p2_detect_end;
		}

		if (val & BIT_CHG_DET_DONE) {
			status = val & (BIT_CDP_INT | BIT_DCP_INT | BIT_SDP_INT);
			break;
		}

		msleep(SPRD_CHG_DET_POLL_DELAY);
	} while (--cnt > 0);

	switch (status) {
	case BIT_CDP_INT:
		type = CDP_TYPE;
		break;
	case BIT_DCP_INT:
		type = DCP_TYPE;
		break;
	case BIT_SDP_INT:
		type = SDP_TYPE;
		break;
	default:
		pr_info("bc1p2:line%d bc1p2_status = 0x%x, cnt = %d\n", __LINE__, val, cnt);
		type = UNKNOWN_TYPE;
	}

bc1p2_detect_end:
	if (bc1p2->redetect_enable)
		sprd_bc1p2_redetect_control(false);
#else
	union power_supply_propval val;
	int ret = 0;
	bc1p2->psy_tcpm = power_supply_get_by_name(SPRD_FCHG_TCPM_PD_NAME);

	if (!bc1p2->psy_tcpm) {
		pr_err("bc1p2:line%d psy_tcpm is NULL\n", __LINE__);
		goto next;
	}

	ret = power_supply_get_property(bc1p2->psy_tcpm, POWER_SUPPLY_PROP_USB_TYPE, &val);
	if (ret) {
		pr_err("bc1p2:line%d usb_type ret = %d\n", __LINE__, ret);
		goto next;
	}

	if (val.intval == POWER_SUPPLY_USB_TYPE_PD || val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS) {
		type = DCP_TYPE;
		return type;
	}

next:
	if(0 == sprd_bc1p2_check_dev(bc1p2))
	{
		charger_first_bc1p2(bc1p2->charger, (int *)&type);
		return type;
	}
#endif
	return type;
}

#ifndef THIRDPART_BUCK_IC
static int sprd_bc1p2_redetect_trigger(u32 time_ms)
{
	int ret = 0, redet_delay_ms = 0;
	u32 reg_val = 0;
	unsigned int chg_det_fgu_ctrl = 0, bc1p2_status = 0;

	if (time_ms > SPRD_CHG_REDET_DELAY_MS_MAX)
		time_ms = SPRD_CHG_REDET_DELAY_MS_MAX;

	reg_val = time_ms / SPRD_CHG_DET_DELAY_STEP_MS;
	ret = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_det_fgu_ctrl,
				 SPRD_CHG_REDET_DELAY_MASK,
				 reg_val << SPRD_CHG_REDET_DELAY_OFFSET);
	if (ret)
		return SPRD_ERROR_REGMAP_UPDATE;

	ret = sprd_bc1p2_redetect_control(true);
	if (ret)
		return SPRD_ERROR_REGMAP_UPDATE;

	msleep(SPRD_CHG_DET_POLL_DELAY);

	if (!sprd_bc1p2_vchg_is_present(&chg_det_fgu_ctrl, &bc1p2_status)) {
		pr_info("bc1p2:line%d: absent, chg_det_fgu_ctrl = 0x%x, bc1p2_status = 0x%x\n",
			__LINE__, chg_det_fgu_ctrl, bc1p2_status);
		return SPRD_ERROR_CHARGER_INIT;
	}

	if (bc1p2_status & BIT_CHG_DET_DONE)
		return SPRD_ERROR_CHARGER_DETDONE;

	redet_delay_ms = time_ms + SPRD_CHG_BC1P2_DETECT_TIME;
	bc1p2->chg_redet_poll_count = redet_delay_ms / SPRD_CHG_DET_POLL_DELAY;

	return SPRD_ERROR_NO_ERROR;
}
#endif

enum usb_charger_type sprd_bc1p2_charger_detect(struct usb_phy *x)
{
	int cnt = bc1p2->chg_detect_poll_count;

	do {
		if (bc1p2->shutdown)
			return UNKNOWN_TYPE;
		if (x->flags & CHARGER_DETECT_DONE)
			break;
		msleep(SPRD_CHG_DET_POLL_DELAY);
	} while (--cnt > 0);

	return x->chg_type;
}
EXPORT_SYMBOL_GPL(sprd_bc1p2_charger_detect);

enum usb_charger_type sprd_bc1p2_retry_detect(struct usb_phy *x)
{
	enum usb_charger_type type = UNKNOWN_TYPE;
#ifndef THIRDPART_BUCK_IC
	int ret = 0;
#endif
	if (bc1p2->shutdown)
		return UNKNOWN_TYPE;
#ifndef THIRDPART_BUCK_IC
	ret = sprd_bc1p2_redetect_trigger(SPRD_CHG_REDET_DELAY_MS);
	if (ret) {
		sprd_bc1p2_redetect_control(false);
		if (ret)
			dev_err(x->dev, "bc1p2:trigger redetect bc1p2 failed, ret= %d\n", ret);
		return UNKNOWN_TYPE;
	}
#endif
	if(0 == sprd_bc1p2_check_dev(bc1p2))
	{
		charger_retry_bc1p2(bc1p2->charger, (int *)&type);
		return type;
	}

	return type;
}
EXPORT_SYMBOL_GPL(sprd_bc1p2_retry_detect);

static void sprd_bc1p2_notify_charger(struct usb_phy *x)
{
	switch (bc1p2->detect_state) {
	case CHG_STATE_UNDETECT:
	case CHG_STATE_DETECTED:
		if (x->chg_type == SDP_TYPE)
			usb_phy_set_charger_current(x, DEFAULT_CUR_MIN);
		else
			schedule_work(&x->chg_work);
		break;
	case CHG_STATE_RETRY_DETECT:
		if (bc1p2->retry_chg_detect_count == 0 && x->chg_type == UNKNOWN_TYPE)
			schedule_work(&x->chg_work);
		break;
	case CHG_STATE_RETRY_DETECTED:
		if (x->chg_type == SDP_TYPE)
			usb_phy_set_charger_current(x, DEFAULT_CUR_MIN);
		else if (x->chg_type != UNKNOWN_TYPE)
			schedule_work(&x->chg_work);
		break;
	default:
		break;
	}
}

static int sprd_get_usb_device_state(int *state)
{
	struct device_node *np;
	struct platform_device *pdev;
	struct sprd_glue *glue;

	np = of_find_compatible_node(NULL, NULL, "sprd,qogirl6-musb");
	if (!np) {
		pr_err("%s: can not find udc node!\n", __func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("%s: can not find udc platform device!\n", __func__);
		return -ENODEV;
	}

	glue = platform_get_drvdata(pdev);
	if (!glue || !glue->musb) {
		pr_err("%s: glue or musb NULL!\n", __func__);
		return -ENODEV;
	}

	*state = glue->musb->g.state;
	pr_info("%s:get gadget state:%d\n", __func__, *state);

	return 0;
}

static void sprd_get_bc1p2_type(struct sprd_bc1p2_priv *bc1p2_info)
{
	struct usb_phy *x = bc1p2_info->phy;
	struct sprd_bc1p2 *bc1p2 = bc1p2_info->bc1p2;
	unsigned int chg_det_fgu_ctrl = 0, bc1p2_status = 0;
	int usb_device_state = USB_STATE_NOTATTACHED;

	switch (bc1p2->detect_state) {
	case CHG_STATE_UNDETECT:
		dev_info(x->dev, "bc1p2:chg entry\n");
		x->chg_type = UNKNOWN_TYPE;
		bc1p2->detect_state = CHG_STATE_DETECT;
		if (x->flags & CHARGER_DETECT_DONE)
			bc1p2->detect_state = CHG_STATE_DETECTED;
		fallthrough;
	case CHG_STATE_DETECT:
		dev_info(x->dev, "bc1p2:chg detect\n");
		bc1p2->type = sprd_bc1p2_detect();
		bc1p2->detect_state = CHG_STATE_DETECTED;
		fallthrough;
	case CHG_STATE_DETECTED:
		x->chg_type = bc1p2->type;
		dev_info(x->dev, "bc1p2:chg detect finished with %d\n", x->chg_type);

		if (x->flags & CHARGER_DETECT_DONE)
		{
			break;
		}

		if (x->chg_type == SDP_TYPE || x->chg_type == CDP_TYPE || x->chg_type == DCP_TYPE) {
			x->flags |= CHARGER_DETECT_DONE;
			if(false == sc27xx_get_try_sink_flag() && x->chg_type == SDP_TYPE) {
				bc1p2->detect_state = CHG_STATE_RETRY_DETECT;
				bc1p2_info->rework = true;
			}
			break;
		}

		if (!bc1p2->redetect_enable) {
			x->flags |= CHARGER_DETECT_DONE;
			break;
		}
#ifndef THIRDPART_BUCK_IC
		if (!sprd_bc1p2_vchg_is_present(&chg_det_fgu_ctrl, &bc1p2_status)) {
			pr_info("bc1p2:line%d: absent, chg_det_fgu_ctrl = 0x%x, bc1p2_status = 0x%x\n",
				__LINE__, chg_det_fgu_ctrl, bc1p2_status);
			break;
		}
#endif
		if (x->chg_type == UNKNOWN_TYPE) {
			bc1p2->detect_state = CHG_STATE_RETRY_DETECT;
			bc1p2_info->rework = true;
		}
		break;
	case CHG_STATE_RETRY_DETECT:
		dev_info(x->dev, "bc1p2:bc1.2 retry detect");
		sprd_get_usb_device_state(&usb_device_state);
		if (x->chg_type == SDP_TYPE && usb_device_state == USB_STATE_CONFIGURED) {
			pr_info("bc1p2:chg retry_detect finished with %d\n", x->chg_type);
			x->flags |= CHARGER_DETECT_DONE;
			break;
		}
		if (!sprd_bc1p2_vchg_is_present(&chg_det_fgu_ctrl, &bc1p2_status)) {
			pr_info("bc1p2:line%d: absent, chg_det_fgu_ctrl = 0x%x, bc1p2_status = 0x%x\n",
				__LINE__, chg_det_fgu_ctrl, bc1p2_status);
			break;
		}

		bc1p2->type = sprd_bc1p2_retry_detect(x);

		if (!sprd_bc1p2_vchg_is_present(&chg_det_fgu_ctrl, &bc1p2_status)) {
			pr_info("bc1p2:line%d: absent, chg_det_fgu_ctrl = 0x%x, bc1p2_status = 0x%x\n",
				__LINE__, chg_det_fgu_ctrl, bc1p2_status);
			break;
		}

		if (bc1p2->type == UNKNOWN_TYPE && !bc1p2->shutdown &&
		    bc1p2->retry_chg_detect_count < SPRD_CHG_MAX_REDETECT_COUNT) {
			bc1p2_info->rework = true;
			bc1p2->retry_chg_detect_count++;
			break;
		}

		bc1p2->detect_state = CHG_STATE_RETRY_DETECTED;
		fallthrough;
	case CHG_STATE_RETRY_DETECTED:
		x->chg_type = bc1p2->type;
		dev_info(x->dev, "bc1p2:chg retry_detected finished with %d\n", x->chg_type);
		x->flags |= CHARGER_DETECT_DONE;

		if (x->chg_type == SDP_TYPE)
			usb_phy_set_charger_current(x, DEFAULT_CUR_MIN);

		break;
	default:
		break;
	}
}

static void sprd_get_bc1p2_type_work(struct kthread_work *work)
{
	struct sprd_bc1p2_priv *bc1p2_info = container_of(work, struct sprd_bc1p2_priv,
							  bc1p2_kwork.work);
	struct usb_phy *x = bc1p2_info->phy;
	struct sprd_bc1p2 *bc1p2 = bc1p2_info->bc1p2;

	if (!bc1p2 || !x->charger_detect) {
		pr_err("bc1p2:line%d: charger_detect NULL pointer!!!\n", __LINE__);
		return;
	}

	switch (x->chg_state) {
	case USB_CHARGER_PRESENT:
		mutex_lock(&bc1p2->bc1p2_lock);
		sprd_get_bc1p2_type(bc1p2_info);
		mutex_unlock(&bc1p2->bc1p2_lock);

		if (bc1p2_info->bc1p2_thread && bc1p2_info->rework) {
			kthread_mod_delayed_work(&bc1p2_info->bc1p2_kworker,
						 &bc1p2_info->bc1p2_kwork, SPRD_CHG_DETECT_DELAY);
			bc1p2_info->rework = false;
		}
		break;
	case USB_CHARGER_ABSENT:
		mutex_lock(&bc1p2->bc1p2_lock);
		dev_info(x->dev, "bc1p2:usb_charger_absent\n");
		x->chg_type = UNKNOWN_TYPE;
		bc1p2->detect_state = CHG_STATE_UNDETECT;
		bc1p2->retry_chg_detect_count = 0;
		bc1p2->type = UNKNOWN_TYPE;
		x->flags &= ~CHARGER_DETECT_DONE;
		mutex_unlock(&bc1p2->bc1p2_lock);
		break;
	default:
		dev_warn(x->dev, "bc1p2:Unknown USB charger state: %d\n", x->chg_state);
		break;
	}

	sprd_bc1p2_notify_charger(x);
}

static void sprd_usb_change_work(struct kthread_work *work)
{
	struct sprd_bc1p2_priv *bc1p2_info = container_of(work, struct sprd_bc1p2_priv,
							  usb_change_kwork.work);

	spin_lock(&bc1p2_info->vbus_event_lock);
	while (bc1p2_info->vbus_events) {
		bc1p2_info->vbus_events = false;
		dev_dbg(bc1p2_info->phy->dev, "bc1p2:line%d: %s\n", __LINE__, __func__);
		spin_unlock(&bc1p2_info->vbus_event_lock);
		if (bc1p2_info->bc1p2_thread)
			kthread_cancel_delayed_work_sync(&bc1p2_info->bc1p2_kwork);
		kthread_queue_delayed_work(&bc1p2_info->bc1p2_kworker,
					   &bc1p2_info->bc1p2_kwork, 0);
		spin_lock(&bc1p2_info->vbus_event_lock);
	}

	spin_unlock(&bc1p2_info->vbus_event_lock);
}

void sprd_usb_changed(struct sprd_bc1p2_priv *bc1p2_info, enum usb_charger_state state)
{
	struct usb_phy *usb_phy = bc1p2_info->phy;

	if (bc1p2->shutdown)
		return;

	spin_lock(&bc1p2_info->vbus_event_lock);
	bc1p2_info->vbus_events = true;

	usb_phy->chg_state = state;
	dev_dbg(usb_phy->dev, "bc1p2:line%d: %s\n", __LINE__, __func__);
	spin_unlock(&bc1p2_info->vbus_event_lock);

	if (bc1p2_info->bc1p2_thread)
		kthread_queue_delayed_work(&bc1p2_info->bc1p2_kworker,
					   &bc1p2_info->usb_change_kwork, 0);
}
EXPORT_SYMBOL_GPL(sprd_usb_changed);

int usb_add_bc1p2_init(struct sprd_bc1p2_priv *bc1p2_info, struct usb_phy *x)
{
	struct sched_param param = { .sched_priority = 1 };
	int ret = 0;

	if (!x->dev) {
		dev_err(x->dev, "no device provided for PHY\n");
		return -EINVAL;
	}

	bc1p2_info->phy = x;
	bc1p2_info->bc1p2 = bc1p2;
	if (!bc1p2->redetect_enable)
		x->flags |= CHARGER_2NDDETECT_ENABLE;
	spin_lock_init(&bc1p2_info->vbus_event_lock);
	kthread_init_worker(&bc1p2_info->bc1p2_kworker);
	kthread_init_delayed_work(&bc1p2_info->usb_change_kwork, sprd_usb_change_work);
	kthread_init_delayed_work(&bc1p2_info->bc1p2_kwork, sprd_get_bc1p2_type_work);
	bc1p2_info->bc1p2_thread = kthread_run(kthread_worker_fn, &bc1p2_info->bc1p2_kworker,
					       "sprd_bc1p2_worker");
	if (IS_ERR(bc1p2_info->bc1p2_thread)) {
		ret = PTR_ERR(bc1p2_info->bc1p2_thread);
		bc1p2_info->bc1p2_thread = NULL;
		dev_err(x->dev, "failed to run bc1p2_thread\n");
		return ret;
	}

	sched_setscheduler(bc1p2_info->bc1p2_thread, SCHED_FIFO, &param);

	return 0;
}
EXPORT_SYMBOL_GPL(usb_add_bc1p2_init);

void usb_remove_bc1p2(struct sprd_bc1p2_priv *bc1p2_info)
{
	if (bc1p2_info->bc1p2_thread) {
		kthread_flush_worker(&bc1p2_info->bc1p2_kworker);
		kthread_stop(bc1p2_info->bc1p2_thread);
		bc1p2_info->bc1p2_thread = NULL;
	}
}
EXPORT_SYMBOL_GPL(usb_remove_bc1p2);

void usb_shutdown_bc1p2(struct sprd_bc1p2_priv *bc1p2_info)
{
	bc1p2->shutdown = true;

	if (bc1p2_info->bc1p2_thread) {
		kthread_cancel_delayed_work_sync(&bc1p2_info->usb_change_kwork);
		kthread_cancel_delayed_work_sync(&bc1p2_info->bc1p2_kwork);
		kthread_stop(bc1p2_info->bc1p2_thread);
		bc1p2_info->bc1p2_thread = NULL;
	}
}
EXPORT_SYMBOL_GPL(usb_shutdown_bc1p2);

static int sprd_bc1p2_probe(struct platform_device *pdev)
{
	int err = 0, chg_int_delay = 0, chg_int_delay_factor = 1;
	struct device *dev = &pdev->dev;

	bc1p2 = devm_kzalloc(dev, sizeof(struct sprd_bc1p2), GFP_KERNEL);
	if (!bc1p2)
		return -ENOMEM;

	bc1p2->data = of_device_get_match_data(dev);
	if (!bc1p2->data) {
		dev_err(dev, "no matching driver data found\n");
		return -EINVAL;
	}

	bc1p2->detect_state = CHG_STATE_UNDETECT;

	bc1p2->regmap = dev_get_regmap(dev->parent, NULL);
	if (!bc1p2->regmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}

	bc1p2->chg_detect_poll_count = SPRD_CHG_INT_DELAY_MS + SPRD_CHG_BC1P2_DETECT_TIME;
	bc1p2->chg_detect_poll_count += SPRD_CHG_VBUS_DEBOUNCE;
	bc1p2->chg_detect_poll_count = bc1p2->chg_detect_poll_count / SPRD_CHG_DET_POLL_DELAY;
	if (device_property_read_bool(dev, "vbus_ok_bybass"))
		chg_int_delay_factor = 7;
	chg_int_delay = (chg_int_delay_factor * SPRD_CHG_INT_DELAY_MS) / SPRD_CHG_DET_DELAY_STEP_MS;
	dev_info(dev, "chg_int_delay is %d * %d\n", chg_int_delay_factor, SPRD_CHG_INT_DELAY_MS);
	err = regmap_update_bits(bc1p2->regmap, bc1p2->data->chg_det_fgu_ctrl,
				 bc1p2->data->chg_int_delay_mask,
				 chg_int_delay << bc1p2->data->chg_int_delay_offset);
	if (err)
		return err;

#ifndef THIRDPART_BUCK_IC
	if (bc1p2->data->chg_bc1p2_ctrl2 == 0) {
		dev_warn(dev, "no support chg_bc1p2 redetect\n");
		bc1p2->redetect_enable = false;
	} else {
		bc1p2->redetect_enable = true;
	}
#else
	bc1p2->redetect_enable = true;
#endif

	mutex_init(&bc1p2->bc1p2_lock);
	bc1p2->shutdown = false;

	sprd_bc1p2_check_dev(bc1p2);

	return err;
}

static const struct of_device_id sprd_bc1p2_of_match[] = {
	{ .compatible = "sprd,sc2720-bc1p2", .data = &sc2720_data},
	{ .compatible = "sprd,sc2721-bc1p2", .data = &sc2721_data},
	{ .compatible = "sprd,sc2730-bc1p2", .data = &sc2730_data},
	{ .compatible = "sprd,sc2731-bc1p2", .data = &sc2731_data},
	{ .compatible = "sprd,ump9620-bc1p2", .data = &ump9620_data},
	{ .compatible = "sprd,ump518-bc1p2", .data = &ump518_data},
	{ }
};

MODULE_DEVICE_TABLE(of, sprd_bc1p2_of_match);

static struct platform_driver sprd_bc1p2_driver = {
	.driver = {
		.name = "sprd-bc1p2",
		.of_match_table = sprd_bc1p2_of_match,
	 },
	.probe = sprd_bc1p2_probe,
};

module_platform_driver(sprd_bc1p2_driver);

MODULE_AUTHOR("Yongzhi Chen <yongzhi.chen@unisoc.com>");
MODULE_DESCRIPTION("sprd bc1p2 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sprd_bc1p2");
