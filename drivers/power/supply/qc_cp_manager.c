/*
 * State machine for qc3 when it works on cp
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define pr_fmt(fmt)	"[FC2-PM]: %s: " fmt, __func__
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/power_supply.h>
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
#include "pmic_voter.h"
#include "mtk_charger.h"
#include "adapter_class.h"
#include "qc_cp_manager.h"
#include "sc8561_reg.h"
#include "bq28z610.h"


#define cut_cap(value, min, max)	((min > value) ? min : ((value > max) ? max : value))
#define is_between(left, right, value)				\
		(((left) >= (right) && (left) >= (value)	\
			&& (value) >= (right))			\
		|| ((left) <= (right) && (left) <= (value)	\
			&& (value) <= (right)))
 
enum qcm_sm_status {
	QCM_SM_CONTINUE,
	QCM_SM_HOLD,
	QCM_SM_EXIT,
};

enum qcm_sm_state {
	QCM_STATE_ENTRY,
	QCM_STATE_INIT_VBUS,
	QCM_STATE_ENABLE_CP,
	QCM_STATE_TUNE,
	QCM_STATE_EXIT,
};

struct qcm_chip {
	struct device *dev;

	struct charger_device *master_dev;
	struct charger_device *slave_dev;
	struct charger_device *charger_dev;
	struct charger_device *i350_dev;
	struct power_supply *batt_psy;
	struct power_supply *usb_psy;
	struct power_supply *bms_psy;

	struct delayed_work	main_sm_work;
	struct work_struct	psy_change_work;
	struct notifier_block	nb;
	spinlock_t		psy_change_lock;
	bool			psy_notify_busy;
	bool			qcm_sm_busy;

    struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable *icl_votable;
	enum qcm_sm_state sm_state;
	enum qcm_sm_status sm_status; 
	enum xmusb350_chg_type qc3_type;
	enum hvdcp3_type qc3_start_type;
	int	max_vbus;
	int	max_ibus;
	int	max_ibat;
	int	max_ibus_qc3_18w;
	int	max_ibat_qc3_18w;
	int	max_ibus_qc3_27w;
	int	max_ibat_qc3_27w;
	int	max_ibus_qc35;
	int	max_ibat_qc35;
	int	cv_vbat;
	int	cv_vbat_ffc;

	int	tune_step_ibus_qc3_27;
	int	tune_step_ibus_qc3_18;
	int	tune_step_ibus_qc35;
	int	tune_step_ibus;
	int	tune_step_ibat;
	int	tune_step_vbus;
	int	tune_gap_ibat;;

	int	vbus_step;
	int	ibus_step;
	int	ibat_step;
	int	max_step;
	int	final_step;
	int	anti_wave_count;

	int	high_soc; 
	bool	no_delay;
	bool	master_cp_enable;
	bool	slave_cp_enable;
	bool	disable_slave;
	bool	ffc_enable;
	bool	charge_full;
	bool	input_suspend;
	bool	typec_burn;

	int	master_cp_ibus;
	int	slave_cp_ibus;
	int	total_ibus;
	int	ibat;
	int	vbat;
	int	soc;
	int	jeita_chg_index;
	int	target_fcc;
	int	master_vbus;
	int	master_vbatt;
	int	mt_vbus;
	int	tune_vbus_count;
	int	enable_cp_count;
	int	taper_count;
	int bms_i2c_error_count;
};

static const unsigned char *qcm_sm_state_str[] = {
	"QCM_STATE_ENTRY",
	"QCM_STATE_INIT_VBUS",
	"QCM_STATE_ENABLE_CP",
	"QCM_STATE_TUNE",
	"QCM_STATE_EXIT",
};

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

#define qcm_err(fmt, ...)						\
do {									\
	if (log_level >= 0)						\
		printk(KERN_ERR "[XMCHG_QCM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define qcm_info(fmt, ...)						\
do {									\
	if (log_level >= 1)						\
		printk(KERN_ERR "[XMCHG_QCM] " fmt, ##__VA_ARGS__);	\
} while (0)

#define qcm_dbg(fmt, ...)						\
do {									\
	if (log_level >= 2)						\
		printk(KERN_ERR "[XMCHG_QCM] " fmt, ##__VA_ARGS__);	\
} while (0)

static bool qcm_check_charger_dev(struct qcm_chip *chip)
{
	chip->master_dev = get_charger_by_name("cp_master");
	if (!chip->master_dev) {
		qcm_err("failed to get master_dev\n");
		return false;
	}

	chip->slave_dev = get_charger_by_name("cp_slave");
	if (!chip->slave_dev) {
		qcm_err("failed to get slave_dev\n");
		return false;
	}

	chip->charger_dev = get_charger_by_name("primary_chg");
	if (!chip->charger_dev) {
		qcm_err("failed to get charger_dev\n");
		return false;
	} 

    chip->i350_dev = get_charger_by_name("xmusb350");
	if (!chip->i350_dev) {
		qcm_err("failed to get i350_dev\n");
		return false;
	} 
	return true;
}

static bool qcm_check_psy(struct qcm_chip *chip)
{ 
    if (!chip->usb_psy)
		chip->usb_psy = power_supply_get_by_name("usb");

	if (!chip->usb_psy) {
		qcm_err("failed to get usb_psy\n");
		return false;
	}

	if (!chip->bms_psy)
		chip->bms_psy = power_supply_get_by_name("bms");

	if (!chip->bms_psy) {
		qcm_err("failed to get bms_psy\n");
		return false;
	}

	if (!chip->batt_psy)
		chip->batt_psy = power_supply_get_by_name("battery");

	if (!chip->batt_psy) {
		qcm_err("failed to get batt_psy\n");
		return false;
	}
	return true;
}

static bool qcm_check_votable(struct qcm_chip *chip)
{ 
    if (!chip->fcc_votable)
		chip->fcc_votable = find_votable("CHARGER_FCC");

	if (!chip->fcc_votable) {
		qcm_err("failed to get fcc_votable\n");
		return false;
	}

	if (!chip->fv_votable)
		chip->fv_votable = find_votable("CHARGER_FV");

	if (!chip->fv_votable) {
		qcm_err("failed to get fv_votable\n");
		return false;
	}

	if (!chip->icl_votable)
		chip->icl_votable = find_votable("CHARGER_ICL");

	if (!chip->icl_votable) {
		qcm_err("failed to get fv_votable\n");
		return false;
	}
	return true;
}

static void qcm_pulse_dpdm(struct qcm_chip *chip, int target_vbus, int pulse_count)
{
	int delta_vbus = 0;

	if (target_vbus) {
		delta_vbus = target_vbus - chip->master_vbus;
		pulse_count = abs(delta_vbus) / chip->tune_step_vbus;
		qcm_err("qcm_pulse_dpdm delta_vbus =%d, pulse_count =%d", delta_vbus, pulse_count);
		if (pulse_count && chip->tune_step_vbus == QC3_VBUS_STEP) {
			if (delta_vbus > 0)
				charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC3_DP_PULSE, pulse_count);
			else
				charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC3_DM_PULSE, pulse_count);
		} else if (pulse_count && chip->tune_step_vbus == QC35_VBUS_STEP) {
			if (delta_vbus > 0)
				charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC35_DP_PULSE, pulse_count);
			else
				charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC35_DM_PULSE, pulse_count);
		}
	} else if (pulse_count > 0) {
		if (chip->tune_step_vbus == QC3_VBUS_STEP)
			charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC3_DP_PULSE, abs(pulse_count));
		else if (chip->tune_step_vbus == QC35_VBUS_STEP)
			charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC35_DP_PULSE, abs(pulse_count));
	} else if (pulse_count < 0) {
		if (chip->tune_step_vbus == QC3_VBUS_STEP)
			charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC3_DM_PULSE, abs(pulse_count));
		else if (chip->tune_step_vbus == QC35_VBUS_STEP)
			charger_dev_qc3_dpdm_pulse(chip->i350_dev, QC35_DM_PULSE, abs(pulse_count));
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

	if (chip->master_vbus > chip->max_vbus)
		chip->vbus_step = -((chip->master_vbus - chip->max_vbus) / chip->tune_step_vbus + 1);
	else
		chip->vbus_step = 0;

	if (chip->total_ibus > chip->max_ibus)
		chip->ibus_step = -((chip->total_ibus - chip->max_ibus) / chip->tune_step_ibus + 1);
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

static bool qcm_check_taper_charge(struct qcm_chip *chip)
{
	int cv_vbat = 0;
	int cv_vote = 0;
	if (chip->charge_full)
		return true;

	cv_vote = get_effective_result(chip->fv_votable);
	cv_vbat = chip->ffc_enable ? chip->cv_vbat_ffc : chip->cv_vbat;
	cv_vbat = cv_vbat < cv_vote ? cv_vbat : cv_vote;

	if (chip->vbat > cv_vbat)
		chip->taper_count++;
	else
		chip->taper_count = 0;
	qcm_err("qcm_check_taper_charge vbatt=%d, cv_vbat=%d, taper_count=%d\n", chip->vbat, cv_vbat, chip->taper_count);
	if (chip->taper_count > MAX_TAPER_COUNT)
		return true;
	else
		return false;
}

static int qcm_check_condition(struct qcm_chip *chip)
{
	if (chip->sm_state == QCM_STATE_TUNE && qcm_check_taper_charge(chip))
		return QCM_SM_EXIT;
	else if (chip->sm_state == QCM_STATE_TUNE && (!chip->master_cp_enable || (!chip->disable_slave && !chip->slave_cp_enable)))
		return QCM_SM_HOLD;
	else if (chip->input_suspend || chip->typec_burn)
		return QCM_SM_HOLD;
	else if (!is_between(MIN_JEITA_CHG_INDEX, MAX_JEITA_CHG_INDEX, chip->jeita_chg_index))
		return QCM_SM_HOLD;
	else if (chip->target_fcc < MIN_ENTRY_FCC)
		return QCM_SM_HOLD;
	else if (chip->sm_state == QCM_STATE_ENTRY && chip->soc > chip->high_soc)
		return QCM_SM_EXIT;
	else if(night_charging_get_flag() && chip->soc >= 80)
		return QCM_SM_HOLD;
	else if(chip->bms_i2c_error_count >= 10)
		return QCM_SM_EXIT;
	else
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
			charger_dev_cp_set_mode(chip->master_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
			charger_dev_cp_set_mode(chip->slave_dev, SC8561_FORWARD_2_1_CHARGER_MODE);
			charger_dev_cp_device_init(chip->master_dev, CP_FORWARD_2_TO_1);
			charger_dev_cp_device_init(chip->slave_dev, CP_FORWARD_2_TO_1);
			charger_dev_cp_enable_adc(chip->master_dev, true);
			charger_dev_cp_enable_adc(chip->slave_dev, true);
			msleep(500);
			qcm_move_state(chip, QCM_STATE_INIT_VBUS);
		}
		break;
	case QCM_STATE_INIT_VBUS: 
		chip->tune_vbus_count++;
		if (chip->tune_vbus_count == 1) {
			entry_vbus = chip->vbat * 2 + (vbus_low_gap + vbus_high_gap) / 2;
			qcm_info("QCM_STATE_INIT_VBUS entery vbus = %d\n", entry_vbus);
			qcm_pulse_dpdm(chip, entry_vbus, 0);
			if (chip->qc3_type == XMUSB350_TYPE_HVDCP_35_18 || chip->qc3_type == XMUSB350_TYPE_HVDCP_35_27)
				msleep(200);
			break;
		}

		if (chip->tune_vbus_count >= 25) {
			qcm_err("failed to tune VBUS to target window, exit QCM\n");
			qcm_move_state(chip, QCM_STATE_EXIT);
			break;
		}

		if (chip->master_vbus <= chip->vbat * 2 + vbus_low_gap) {
			qcm_pulse_dpdm(chip, 0, 1);
		} else if (chip->master_vbus >= chip->vbat * 2 + vbus_high_gap) {
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
			charger_dev_enable(chip->master_dev, true);
		if (!chip->disable_slave && !chip->slave_cp_enable)
			charger_dev_enable(chip->slave_dev, true);

		if (chip->master_cp_enable && (chip->disable_slave || (!chip->disable_slave && chip->slave_cp_enable))) {
			qcm_info("success to enable charge pump\n");
			charger_dev_enable_powerpath(chip->charger_dev, false);
			charger_dev_set_input_current(chip->charger_dev, 100000);
			charger_dev_enable_termination(chip->charger_dev, false);
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

		charger_dev_enable(chip->master_dev, false);
		charger_dev_enable(chip->slave_dev, false);
		charger_dev_cp_enable_adc(chip->master_dev, false);
		charger_dev_cp_enable_adc(chip->slave_dev, false);
		msleep(100);
		charger_dev_select_qc_mode(chip->i350_dev, QC_MODE_QC2_9); 

		if (chip->sm_status == QCM_SM_EXIT)
			msleep(250);
		if (!chip->input_suspend) {
			charger_dev_enable_powerpath(chip->charger_dev, true);
			charger_dev_set_input_current(chip->charger_dev, 3000000);
		}
		charger_dev_enable_termination(chip->charger_dev, true);
		qcm_move_state(chip, QCM_STATE_ENTRY);
		break;
	default:
		qcm_err("No sm_state defined! Move to stop charging\n");
		break;
	}
}

static void qcm_update_qc3_type(struct qcm_chip *chip)
{
	if (chip->qc3_type == XMUSB350_TYPE_HVDCP_3_18) {
		chip->disable_slave = disable_slave_qc3_18;
		chip->max_step = QC3_MAX_TUNE_STEP;
		chip->tune_step_vbus = QC3_VBUS_STEP;
		chip->tune_step_ibat = tune_step_ibat_qc3_18;
		chip->tune_step_ibus = chip->tune_step_ibus_qc3_18;
		chip->max_ibus = chip->max_ibus_qc3_18w;
		chip->max_ibat = chip->max_ibat_qc3_18w;
	} else if (chip->qc3_type == XMUSB350_TYPE_HVDCP_3_27) {
		chip->disable_slave = false;
		chip->max_step = QC3_MAX_TUNE_STEP;
		chip->tune_step_vbus = QC3_VBUS_STEP;
		chip->tune_step_ibat = tune_step_ibat_qc3_27;
		chip->tune_step_ibus = chip->tune_step_ibus_qc3_27;
		chip->max_ibus = chip->max_ibus_qc3_27w;
		chip->max_ibat = chip->max_ibat_qc3_27w;
	} else if (chip->qc3_type == XMUSB350_TYPE_HVDCP_35_18 || chip->qc3_type == XMUSB350_TYPE_HVDCP_35_27) {
		chip->disable_slave = false;
		chip->max_step = QC35_MAX_TUNE_STEP;
		chip->tune_step_vbus = QC35_VBUS_STEP;
		chip->tune_step_ibat = tune_step_ibat_qc35;
		chip->tune_step_ibus = chip->tune_step_ibus_qc35;
		chip->max_ibus = chip->max_ibus_qc35;
		chip->max_ibat = chip->max_ibat_qc35;
	}
}

static void qcm_update_charge_status(struct qcm_chip *chip)
{
    union power_supply_propval val = {0,};
	int temp = 0;

	charger_dev_get_vbus(chip->master_dev, &chip->master_vbus);
	charger_dev_cp_get_vbatt(chip->master_dev, &chip->master_vbatt);
	charger_dev_get_vbus(chip->charger_dev, &chip->mt_vbus); 
	qcm_err("get mt6375 vbus= %d\n", chip->mt_vbus);
	charger_dev_get_ibus(chip->master_dev, &chip->master_cp_ibus);
	charger_dev_get_ibus(chip->slave_dev, &chip->slave_cp_ibus);
	charger_dev_is_enabled(chip->master_dev, &chip->master_cp_enable);
	charger_dev_is_enabled(chip->slave_dev, &chip->slave_cp_enable);
	chip->total_ibus = chip->master_cp_ibus + chip->slave_cp_ibus;

	usb_get_property(USB_PROP_REAL_TYPE, &temp);
	chip->qc3_type = temp;

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	chip->soc = val.intval;

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	chip->ibat = val.intval / 1000;

	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	chip->vbat = val.intval / 1000;

	usb_get_property(USB_PROP_FFC_ENABLE, &temp);
	chip->ffc_enable = temp;

	usb_get_property(USB_PROP_CHARGE_FULL, &temp);
	chip->charge_full = temp;

	usb_get_property(USB_PROP_JEITA_CHG_INDEX, &temp);
	chip->jeita_chg_index = temp;

	usb_get_property(USB_PROP_INPUT_SUSPEND, &temp);
	chip->input_suspend = temp;

	usb_get_property(USB_PROP_TYPEC_BURN, &temp);
	chip->typec_burn = temp;

	bms_get_property(BMS_PROP_I2C_ERROR_COUNT, &temp);
	chip->bms_i2c_error_count = temp;

	qcm_update_qc3_type(chip);
	chip->target_fcc = get_effective_result(chip->fcc_votable);
	qcm_info("master_vbus=%d, master_ibus=%d, master_vbatt=%d, slave_ibus=%d, total_ibus=%d, master_enable=%d, slave_enable=%d, i2c_error_count=%d\n",
		chip->master_vbus,chip->master_cp_ibus, chip->master_vbatt, chip->slave_cp_ibus, chip->total_ibus,
		chip->master_cp_enable, chip->slave_cp_enable, chip->bms_i2c_error_count);
	qcm_info("soc=%d, vbat=%d, ibat=%d, jeita_index=%d, ibat_step=%d, vbus_step=%d, ibus_step=%d, final_step=%d, target_fcc=%d, ffc_enable=%d, charge_full=%d, input_suspend=%d, typec_burn=%d\n",
		chip->soc, chip->vbat, chip->ibat, chip->jeita_chg_index, 
		chip->ibat_step, chip->vbus_step, chip->ibus_step, chip->final_step,
		chip->target_fcc, chip->ffc_enable, chip->charge_full, chip->input_suspend, chip->typec_burn);
}

static void qcm_main_sm(struct work_struct *work)
{
	struct qcm_chip *chip = container_of(work, struct qcm_chip, main_sm_work.work);
	int sm_delay = QCM_SM_DELAY_500MS;

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
				sm_delay = QCM_SM_DELAY_500MS;
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
	struct qcm_chip *chip = container_of(work, struct qcm_chip, psy_change_work);
	int ret = 0, val = 0;

	ret = usb_get_property(USB_PROP_REAL_TYPE, &val);
	if (ret) {
		qcm_err("Failed to read psy_type\n"); 
	} else {
		chip->qc3_type = val;
	}

	ret = usb_get_property(USB_PROP_QC3_TYPE, &val);
	if (ret) {
		qcm_err("Failed to read psy_type\n"); 
	} else {
		chip->qc3_start_type = val;
	}
	qcm_err("get qc3_type = %x, qc3_start_type = %x\n", chip->qc3_type, chip->qc3_start_type);
	if (chip->qc3_start_type && !chip->qcm_sm_busy) { 
		chip->qcm_sm_busy = true;
		schedule_delayed_work(&chip->main_sm_work, 0);
	} else if (!chip->qc3_start_type && chip->qcm_sm_busy) {
		cancel_delayed_work_sync(&chip->main_sm_work);
		charger_dev_enable(chip->master_dev, false);
		charger_dev_enable(chip->slave_dev, false);
		if (!chip->input_suspend) {
			charger_dev_enable_powerpath(chip->charger_dev, true);
			charger_dev_set_input_current(chip->charger_dev, 3000000);
		}
        charger_dev_enable_termination(chip->charger_dev, true);
		chip->tune_vbus_count = 0;
		chip->enable_cp_count = 0;
		chip->taper_count = 0;
		chip->anti_wave_count = 0;
		chip->bms_i2c_error_count = 0;
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
		schedule_work(&chip->psy_change_work);
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
 
static const struct platform_device_id qcm_id[] = {
	{ "qc_cp_manager", 0},
	{},
};
MODULE_DEVICE_TABLE(platform, qcm_id);

static const struct of_device_id qcm_of_match[] = {
	{ .compatible = "qc_cp_manager", },
	{},
};
MODULE_DEVICE_TABLE(of, qcm_of_match); 

static int qcm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct qcm_chip *chip;
	const struct of_device_id *of_id; 

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
		return -ENODEV;
	}

	INIT_WORK(&chip->psy_change_work, qcm_psy_change);
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

	cancel_work_sync(&chip->psy_change_work);
	cancel_delayed_work(&chip->main_sm_work);
	return 0;
}

static struct platform_driver qcm_driver = {
	.driver = {
		.name = "qc_cp_manager",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(qcm_of_match),
	},
	.probe = qcm_probe,
	.remove = qcm_remove,
	.id_table = qcm_id,
};

module_platform_driver(qcm_driver);
MODULE_AUTHOR("liujiquan");
MODULE_DESCRIPTION("charge pump manager for QC");
MODULE_LICENSE("GPL");
