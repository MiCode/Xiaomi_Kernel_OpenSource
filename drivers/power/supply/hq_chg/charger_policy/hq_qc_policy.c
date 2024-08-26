#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include "hq_qc_policy.h"


static int log_level = 1;

static bool disable_slave_qc3_18 = true;
module_param_named(disable_slave_qc3_18, disable_slave_qc3_18, bool, 0600);

static int tune_step_ibat_qc3_27 = 1400;
module_param_named(tune_step_ibat_qc3_27, tune_step_ibat_qc3_27, int, 0600);

static int tune_step_ibat_qc3_18 = 1900;
module_param_named(tune_step_ibat_qc3_18, tune_step_ibat_qc3_18, int, 0600);

static int tune_step_ibat_qc35 = 200;
module_param_named(tune_step_ibat_qc35, tune_step_ibat_qc35, int, 0600);

static int vbus_low_gap = 200;
module_param_named(vbus_low_gap, vbus_low_gap, int, 0600);

static int vbus_high_gap = 450;
module_param_named(vbus_high_gap, vbus_high_gap, int, 0600);

static bool qcm_check_charger_dev(struct qcm_chip *chip)
{

	chip->charger = charger_find_dev_by_name("primary_chg");
	if (IS_ERR_OR_NULL(chip->charger)) {
		qcm_err("charger is NULL.\n");
		return PTR_ERR(chip->charger);
	}

	chip->master_cp_chg = chargerpump_find_dev_by_name("master_cp_chg");
	if (IS_ERR_OR_NULL(chip->master_cp_chg)) {
		qcm_err("master_cp_chg is NULL.\n");
		return PTR_ERR(chip->master_cp_chg);
	}

	chip->slave_cp_chg = chargerpump_find_dev_by_name("slave_cp_chg");
	if (IS_ERR_OR_NULL(chip->slave_cp_chg))
		qcm_err("slave_cp_chg is %d.\n", PTR_ERR(chip->slave_cp_chg));

	return true;
}

static bool qcm_check_psy(struct qcm_chip *chip)
{
	chip->usb_psy = power_supply_get_by_name("usb");
	if (!chip->usb_psy) {
		qcm_err("failed to get usb_psy\n");
		return false;
	}

	chip->bms_psy = power_supply_get_by_name("bms");
	if (!chip->bms_psy) {
		qcm_err("failed to get bms_psy\n");
		return false;
	}

	return true;
}

static bool qcm_check_votable(struct qcm_chip *chip)
{
	chip->main_icl_votable = find_votable("MAIN_ICL");
	if (!chip->main_icl_votable) {
		qcm_err("failed to get main_icl_votable\n");
		return false;
	}

	chip->main_chg_disable = find_votable("MAIN_CHG_DISABLE");
	if (!chip->main_chg_disable) {
		qcm_err("failed to get main_chg_disable\n");
		return false;
	}

	chip->total_fcc_votable = find_votable("TOTAL_FCC");
	if (!chip->total_fcc_votable) {
		qcm_err("failed to get total_fcc_votable\n");
		return false;
	}

	return true;
}

static void qcm_pulse_dpdm(struct qcm_chip *chip, int target_vbus, int count)
{
	int delta_vbus = 0;

	qcm_info("target_vbus = %d, adapter_vbus = %d, chip->tune_step_vbus = %d, count = %d\n",
				target_vbus, chip->adapter_vbus, chip->tune_step_vbus, count);

	if (target_vbus) {
		delta_vbus = target_vbus - chip->adapter_vbus;
		count = abs(delta_vbus) / chip->tune_step_vbus;
		qcm_err("delta_vbus = %d, count = %d\n", delta_vbus, count);
		if (count && chip->tune_step_vbus == QC3_VBUS_STEP) {
			if (delta_vbus > 0)
				charger_qc3_vbus_puls(chip->charger, plus, count);
			else
				charger_qc3_vbus_puls(chip->charger, minus, count);
		} else if (count && chip->tune_step_vbus == QC35_VBUS_STEP) {
			if (delta_vbus > 0)
				charger_qc3_vbus_puls(chip->charger, plus, count);
			else
				charger_qc3_vbus_puls(chip->charger, minus, count);
		}
	} else if (count > 0) {
		if (chip->tune_step_vbus == QC3_VBUS_STEP)
			charger_qc3_vbus_puls(chip->charger, plus, abs(count));
		else if (chip->tune_step_vbus == QC35_VBUS_STEP)
			charger_qc3_vbus_puls(chip->charger, plus, abs(count));
	} else if (count < 0) {
		if (chip->tune_step_vbus == QC3_VBUS_STEP)
			charger_qc3_vbus_puls(chip->charger, minus, abs(count));
		else if (chip->tune_step_vbus == QC35_VBUS_STEP)
			charger_qc3_vbus_puls(chip->charger, minus, abs(count));
	}
}

static void qcm_charge_tune(struct qcm_chip *chip)
{
	if ((-chip->ibat) < chip->target_fcc)
		chip->final_step = chip->ibat_step = (chip->target_fcc - (-chip->ibat)) / chip->tune_step_ibat;
	else if ((-chip->ibat) > chip->target_fcc)
		chip->final_step = chip->ibat_step = -(((-chip->ibat) - chip->target_fcc) / chip->tune_step_ibat + 1);
	else
		chip->final_step = chip->ibat_step = 0;

	if (chip->adapter_vbus > chip->max_vbus)
		chip->vbus_step = -((chip->adapter_vbus - chip->max_vbus) / chip->tune_step_vbus + 1);
	else
		chip->vbus_step = 0;

	if ((chip->cp_total_ibus + 100)> chip->max_ibus)
		chip->ibus_step = -(((chip->cp_total_ibus + 100) - chip->max_ibus) / chip->tune_step_ibus + 1);
	else
		chip->ibus_step = 0;

	if (chip->vbus_step)
		chip->final_step = min(chip->final_step, chip->vbus_step);
	if (chip->ibus_step)
		chip->final_step = min(chip->final_step, chip->ibus_step);
	if (chip->final_step)
		chip->final_step = cut_cap(chip->final_step, (-chip->max_step), chip->max_step);

	if (chip->final_step) {
		if (!chip->anti_wave_count)
			qcm_pulse_dpdm(chip, 0, chip->final_step);
		chip->anti_wave_count++;
		if (chip->anti_wave_count >= (chip->tune_step_vbus == QC3_VBUS_STEP ? ANTI_WAVE_COUNT_QC3 : ANTI_WAVE_COUNT_QC35))
			chip->anti_wave_count = 0;
	} else {
		chip->anti_wave_count = 0;
	}
}

__maybe_unused
static bool qcm_check_taper_charge(struct qcm_chip *chip)
{
	int cv_vbat = 0;

	cv_vbat = chip->cv_vbat_ffc;
	if (chip->vbat > cv_vbat)
		chip->taper_count++;
	else
		chip->taper_count = 0;

	if (chip->taper_count > MAX_TAPER_COUNT)
		return true;
	else
		return false;
}

static int qcm_check_condition(struct qcm_chip *chip)
{
	#if 0
	if (chip->sm_state == QCM_STATE_TUNE && qcm_check_taper_charge(chip))
		return QCM_SM_EXIT;
	else if (chip->sm_state == QCM_STATE_TUNE && (!chip->master_cp_enable || (!chip->disable_slave && !chip->slave_cp_enable)))
		return QCM_SM_HOLD;
	else if (chip->sm_state == QCM_STATE_TUNE && chip->master_cp_ibus <= MIN_CP_IBUS && chip->slave_cp_ibus <= MIN_CP_IBUS)
		return QCM_SM_EXIT;
	else if (chip->input_suspend)
		return QCM_SM_HOLD;
	else if (0/*add jeita judge*/)
		return QCM_SM_HOLD;
	else if (chip->target_fcc < MIN_ENTRY_FCC)
		return QCM_SM_HOLD;
	else if (chip->sm_state == QCM_STATE_ENTRY && chip->soc > chip->high_soc)
		return QCM_SM_EXIT;
	else
	#endif
	return QCM_SM_CONTINUE;
}

static void qcm_move_state(struct qcm_chip *chip, enum qcm_sm_state new_state)
{
	qcm_info("sm_state change:%s -> %s\n", qcm_sm_state_str[chip->sm_state], qcm_sm_state_str[new_state]);
	chip->sm_state = new_state;
	chip->no_delay = true;
}

static void qcm_handle_sm(struct qcm_chip *chip)
{
	int entry_vbus = 0;

	switch (chip->sm_state) {
		case QCM_STATE_ENTRY:
			chip->tune_vbus_count = 0;
			chip->enable_cp_count = 0;
			chip->taper_count = 0;
			chip->anti_wave_count = 0;

			chip->sm_status = qcm_check_condition(chip);
			if (chip->sm_status == QCM_SM_EXIT) {
				qcm_info("QCM_SM_EXIT, don't start sm\n");
				break;
			} else if (chip->sm_status == QCM_SM_HOLD) {
				break;
			} else if (chip->sm_status == QCM_SM_CONTINUE) {
				vote(chip->main_icl_votable, QC_POLICY_VOTER, true, QCM_MAIN_CHG_ICL);
				qcm_move_state(chip, QCM_STATE_INIT_VBUS);
			}
			break;
		case QCM_STATE_INIT_VBUS:
			chip->tune_vbus_count++;
			if (chip->tune_vbus_count == 1) {
				entry_vbus = chip->vbat * 2 + (vbus_low_gap + vbus_high_gap) / 2;
				qcm_pulse_dpdm(chip, entry_vbus, 0);
				break;
			}

			if (chip->tune_vbus_count >= 15) {
				qcm_err("failed to tune VBUS to target window, exit QCM\n");
				qcm_move_state(chip, QCM_STATE_EXIT);
				break;
			}

			if (chip->adapter_vbus <= chip->vbat * 2 + vbus_low_gap) {
				qcm_pulse_dpdm(chip, 0, 1);
			} else if (chip->adapter_vbus >= chip->vbat * 2 + vbus_high_gap) {
				qcm_pulse_dpdm(chip, 0, -1);
			} else {
				qcm_info("success to tune VBUS to target window\n");
				qcm_move_state(chip, QCM_STATE_ENABLE_CP);
				break;
			}
			break;
		case QCM_STATE_ENABLE_CP:
			chip->enable_cp_count++;
			if (chip->enable_cp_count >= 5) {
				qcm_err("failed to enable charge pump, exit PDM\n");
				qcm_move_state(chip, QCM_STATE_EXIT);
				break;
			}

			if (!chip->master_cp_enable)
				chargerpump_set_enable(chip->master_cp_chg, true);
			if (!chip->disable_slave && !chip->slave_cp_enable)
				chargerpump_set_enable(chip->slave_cp_chg, true);

			if (chip->master_cp_enable && (chip->disable_slave || (!chip->disable_slave && chip->slave_cp_enable))) {
				qcm_info("success to enable charge pump\n");
				charger_set_term(chip->charger, false);
				qcm_move_state(chip, QCM_STATE_TUNE);
			} else {
				if (chip->enable_cp_count != 1)
					qcm_err("failed to enable charge pump, try again\n");
				break;
			}
			break;
		case QCM_STATE_TUNE:
			chip->sm_status = qcm_check_condition(chip);
			if (chip->sm_status == QCM_SM_EXIT) {
				qcm_info("taper charge done\n");
				qcm_move_state(chip, QCM_STATE_EXIT);
			} else if (chip->sm_status == QCM_SM_HOLD) {
				qcm_move_state(chip, QCM_STATE_EXIT);
			} else {
				qcm_charge_tune(chip);
			}
			break;
		case QCM_STATE_EXIT:
			chip->tune_vbus_count = 0;
			chip->enable_cp_count = 0;
			chip->taper_count = 0;
			chip->anti_wave_count = 0;

			charger_set_hiz(chip->charger, true);
			chargerpump_set_enable(chip->master_cp_chg, false);
			chargerpump_set_enable(chip->slave_cp_chg, false);
			msleep(100);
			charger_qc2_vbus_mode(chip->charger, QC2_VBUS_9V);
			vote(chip->main_icl_votable, QC_POLICY_VOTER, false, 0);

			if (chip->sm_status == QCM_SM_EXIT)
				msleep(500);
			charger_set_term(chip->charger, true);
			charger_set_hiz(chip->charger, false);
			qcm_move_state(chip, QCM_STATE_ENTRY);
			break;
		default:
			qcm_err("No sm_state defined! Move to stop charging\n");
			break;
		}
}

static bool qcom_get_qc_type(struct qcm_chip *chip)
{
	enum vbus_type vbus_type;

	charger_get_vbus_type(chip->charger, &vbus_type);
	if (vbus_type == VBUS_TYPE_HVDCP_3 || vbus_type == VBUS_TYPE_HVDCP_3P5)
		return true;
	else
		return false;
}

static void qcm_update_qc3_type(struct qcm_chip *chip)
{
	if (chip->qc3_type ) {
		chip->disable_slave = false;
		chip->max_step = QC3_MAX_TUNE_STEP;
		chip->tune_step_vbus = QC3_VBUS_STEP;
		chip->tune_step_ibat = tune_step_ibat_qc3_27;
		chip->tune_step_ibus = chip->tune_step_ibus_qc3_27;
		chip->max_ibus = chip->max_ibus_qc3_27w;
		chip->max_ibat = chip->max_ibat_qc3_27w;
	}
}

static void qcm_update_charge_status(struct qcm_chip *chip)
{
	union power_supply_propval val = {0,};

	charger_get_adc(chip->charger, ADC_GET_VBUS, &chip->adapter_vbus);
	charger_get_adc(chip->charger, ADC_GET_IBUS, &chip->sw_ibus);
	chargerpump_get_adc_value(chip->master_cp_chg, ADC_GET_IBUS, &chip->master_cp_ibus);
	chargerpump_get_adc_value(chip->slave_cp_chg, ADC_GET_IBUS, &chip->slave_cp_ibus);
	chargerpump_get_is_enable(chip->master_cp_chg, &chip->master_cp_enable);
	chargerpump_get_is_enable(chip->slave_cp_chg, &chip->slave_cp_enable);
	chip->cp_total_ibus = chip->master_cp_ibus + chip->slave_cp_ibus;

	chip->qc3_type = qcom_get_qc_type(chip);

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	chip->soc = val.intval;

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	chip->ibat = val.intval / 1000;

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	chip->vbat = val.intval / 1000;

	chip->input_suspend = get_effective_result(chip->main_chg_disable);

	qcm_update_qc3_type(chip);
	chip->target_fcc = 6000; /*get_effective_result(chip->total_fcc_votable);*/

	qcm_info("BUS = [%d %d %d %d %d], CP = [%d %d], BAT = [%d %d %d], STEP = [%d %d %d %d], FCC = [%d], FFC_CMD = [%d]\n",
		chip->adapter_vbus, chip->sw_ibus, chip->master_cp_ibus, chip->slave_cp_ibus, chip->cp_total_ibus,
		chip->master_cp_enable, chip->slave_cp_enable, chip->soc, chip->vbat, chip->ibat,
		chip->ibat_step, chip->vbus_step, chip->ibus_step, chip->final_step,
		chip->target_fcc, chip->input_suspend);
}

static void qcm_main_sm(struct work_struct *work)
{
	struct qcm_chip *chip = container_of(work, struct qcm_chip, main_sm_work.work);
	int sm_delay = QCM_SM_DELAY_400MS;

	qcm_update_charge_status(chip);
	qcm_handle_sm(chip);

	if (chip->sm_state == QCM_STATE_ENTRY && chip->sm_status == QCM_SM_EXIT) {
		qcm_info("exit QCM\n");
	} else {
		if (chip->no_delay) {
			sm_delay = 0;
			chip->no_delay = false;
		} else {
			switch (chip->sm_state) {
			case QCM_STATE_ENTRY:
				sm_delay = QCM_SM_DELAY_500MS;
				break;
			case QCM_STATE_INIT_VBUS:
				sm_delay = QCM_SM_DELAY_500MS;
				break;
			case QCM_STATE_EXIT:
			case QCM_STATE_ENABLE_CP:
				sm_delay = QCM_SM_DELAY_200MS;
				break;
			case QCM_STATE_TUNE:
				sm_delay = QCM_SM_DELAY_400MS;
				break;
			default:
				qcm_err("not supportted qcm_sm_state\n");
				break;
			}
		}
		schedule_delayed_work(&chip->main_sm_work, msecs_to_jiffies(sm_delay));
	}
}

static void qcm_psy_change(struct work_struct *work)
{
	struct qcm_chip *chip = container_of(work, struct qcm_chip, psy_change_work.work);

	chip->qc3_type = qcom_get_qc_type(chip);

	if (chip->qc3_type && !chip->qcm_sm_busy) {
		chip->qcm_sm_busy = true;
		schedule_delayed_work(&chip->main_sm_work, 0);
	} else if (!chip->qc3_type && chip->qcm_sm_busy) {
		cancel_delayed_work_sync(&chip->main_sm_work);
		vote(chip->main_icl_votable, QC_POLICY_VOTER, false, 0);
		chargerpump_set_enable(chip->master_cp_chg, false);
		chargerpump_set_enable(chip->slave_cp_chg, false);
		chip->tune_vbus_count = 0;
		chip->enable_cp_count = 0;
		chip->taper_count = 0;
		chip->anti_wave_count = 0;
		qcm_move_state(chip, QCM_STATE_ENTRY);
		chip->qcm_sm_busy = false;
	}

	chip->psy_notify_busy = false;
	return;
}

static int qcm_psy_notifier_cb(struct notifier_block *nb, unsigned long ev, void *data)
{
	struct qcm_chip *chip = container_of(nb, struct qcm_chip, nb);
	struct power_supply *psy = data;
	unsigned long flags;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	spin_lock_irqsave(&chip->psy_change_lock, flags);
	if (strcmp(psy->desc->name, "usb") == 0 && !chip->psy_notify_busy) {
		chip->psy_notify_busy = true;
		schedule_delayed_work(&chip->psy_change_work, 0);
	}
	spin_unlock_irqrestore(&chip->psy_change_lock, flags);

	return NOTIFY_OK;
}

static int qcm_parse_dt(struct qcm_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	int ret = 0;

	if (!node) {
		qcm_err("device tree node missing\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "max_vbus", &chip->max_vbus);
	ret = of_property_read_u32(node, "max_ibus_qc3_18w", &chip->max_ibus_qc3_18w);
	ret = of_property_read_u32(node, "max_ibat_qc3_18w", &chip->max_ibat_qc3_18w);
	ret = of_property_read_u32(node, "max_ibus_qc3_27w", &chip->max_ibus_qc3_27w);
	ret = of_property_read_u32(node, "max_ibat_qc3_27w", &chip->max_ibat_qc3_27w);
	ret = of_property_read_u32(node, "max_ibus_qc35", &chip->max_ibus_qc35);
	ret = of_property_read_u32(node, "max_ibat_qc35", &chip->max_ibat_qc35);
	ret = of_property_read_u32(node, "tune_step_ibus_qc3_27", &chip->tune_step_ibus_qc3_27);
	ret = of_property_read_u32(node, "tune_step_ibus_qc3_18", &chip->tune_step_ibus_qc3_18);
	ret = of_property_read_u32(node, "tune_step_ibus_qc35", &chip->tune_step_ibus_qc35);
	ret = of_property_read_u32(node, "tune_step_ibat_qc3_27", &tune_step_ibat_qc3_27);
	ret = of_property_read_u32(node, "tune_step_ibat_qc3_18", &tune_step_ibat_qc3_18);
	ret = of_property_read_u32(node, "tune_step_ibat_qc35", &tune_step_ibat_qc35);
	ret = of_property_read_u32(node, "high_soc", &chip->high_soc);
	ret = of_property_read_u32(node, "cv_vbat", &chip->cv_vbat);
	ret = of_property_read_u32(node, "cv_vbat_ffc", &chip->cv_vbat_ffc);

	return ret;
}

static const struct of_device_id qcm_of_match[] = {
	{ .compatible = "huaqin,hq_qc_policy",},
	{},
};
MODULE_DEVICE_TABLE(of, qcm_of_match);

static int qcm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct qcm_chip *chip;
	const struct of_device_id *of_id;

	qcm_info("QCM probe enter\n");

	of_id = of_match_device(qcm_of_match, &pdev->dev);
	pdev->id_entry = of_id->data;

	chip = devm_kzalloc(dev, sizeof(struct qcm_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = dev;
	chip->tune_vbus_count = 0;
	chip->enable_cp_count = 0;
	chip->taper_count = 0;
	chip->sm_state = QCM_STATE_ENTRY;
	chip->qcm_sm_busy = false;
	chip->psy_notify_busy = false;
	spin_lock_init(&chip->psy_change_lock);
	platform_set_drvdata(pdev, chip);

	ret = qcm_parse_dt(chip);
	if (ret) {
		qcm_err("Couldn't parse device tree rc=%d\n", ret);
		return ret;
	}

	if (!qcm_check_charger_dev(chip)) {
		qcm_err("failed to check charger device\n");
		return -ENODEV;
	}

	if (!qcm_check_psy(chip)) {
		qcm_err("failed to check psy\n");
		return -ENODEV;
	}

	if (!qcm_check_votable(chip)) {
		qcm_err("failed to check votable\n");
	}

	INIT_DELAYED_WORK(&chip->psy_change_work, qcm_psy_change);
	INIT_DELAYED_WORK(&chip->main_sm_work, qcm_main_sm);

	chip->nb.notifier_call = qcm_psy_notifier_cb;
	ret = power_supply_reg_notifier(&chip->nb);
	if (ret < 0) {
		qcm_err("failed to register psy notifier rc = %d\n", ret);
		return ret;
	}

	qcm_info("QCM probe success\n");
	return ret;
}

static int qcm_remove(struct platform_device *pdev)
{
	struct qcm_chip *chip = platform_get_drvdata(pdev);

	cancel_delayed_work(&chip->psy_change_work);
	cancel_delayed_work(&chip->main_sm_work);
	return 0;
}

static struct platform_driver qcm_driver = {
	.driver = {
		.name = "hq_qc_policy",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(qcm_of_match),
	},
	.probe = qcm_probe,
	.remove = qcm_remove,
};

static int __init hq_qcm_init(void)
{
	printk("%s\n", __func__);
	return platform_driver_register(&qcm_driver);
}

static void __exit hq_qcm_exit(void)
{
	printk("%s\n", __func__);
	platform_driver_unregister(&qcm_driver);
}

module_init(hq_qcm_init);
module_exit(hq_qcm_exit);

MODULE_DESCRIPTION("huaqin driver");
MODULE_LICENSE("GPL v2");

/*
 * HQ QC_POLICY Release Note
 * 1.0.0
 * (1) Add HVDCP3 policy
 * (2) TODO:JEITA/THERMAL/ONLY MAIN CHG MODE
 */