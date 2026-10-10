#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/rpmsg.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/thermal.h>
#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/soc/qcom/panel_event_notifier.h>

#include <linux/kernfs.h>
#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/kobject_ns.h>
#include <linux/stat.h>
#include <linux/atomic.h>
#include "lx_charger_manager.h"
#if IS_ENABLED(CONFIG_XIAOMI_SMART_CHG)
#include "xm_smart_chg.h"
#include "../common/lx_voter.h"
#endif
#include "../lx_printk.h"
#ifdef TAG
#undef TAG
#define  TAG "[LX_CHG][CM]"
#endif
#include "../fuelgauge/bq28z610.h"

//cqr vbat ctrol start

#define CQR_VBAT_MIM       3750000
#define CQR_VBAT_MAX       4300000
#define CQR_READ_VBAT_TIMES 5
#define CQR_VBAT_OUT_RANGE_CNT 2

//cqr vbat ctrol end

static ssize_t real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{

	/*return scnprintf(buf, PAGE_SIZE, "%s\n","c");*/
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	enum vbus_type vbus_type = VBUS_TYPE_NONE;
	int ret;

	if (IS_ERR_OR_NULL(manager)){
		lx_err("real_type_show manager  NULL\n");
		return PTR_ERR(manager);
	}

	if (IS_ERR_OR_NULL(manager->charger)) {
		lx_err("%s:manager->charger is_err_or_null\n");
		goto out;
	}

	ret = charger_get_vbus_type(manager->charger, &vbus_type);
	if (ret < 0){
		lx_err("Couldn't get usb type ret=%d\n", ret);
		goto out;
	}

out:
	if(manager->pd_active == CHARGE_PD_ACTIVE) {
		vbus_type = VBUS_TYPE_PD;
	} else if(manager->pd_active == CHARGE_PD_PPS_ACTIVE) {
		vbus_type = VBUS_TYPE_PPS;
	}

#if IS_ENABLED(CONFIG_XM_CHG_ANIMATION)
	xm_uevent_report(manager);
#endif

	if (manager->otg_status) {
		vbus_type = VBUS_TYPE_UNKNOW;
	}
	lx_debug("real_type = %d\n", vbus_type);

	return scnprintf(buf, PAGE_SIZE, "%s\n", real_type_txt[vbus_type]);
}
static CLASS_ATTR_RO(real_type);

static ssize_t typec_cc_orientation_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	bool usb_online = false;
	bool otg_value = false;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	charger_get_otg_status(manager->charger, &otg_value);
	charger_get_online(manager->charger, &usb_online);

	if (usb_online == false && otg_value == false)
		return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
	else if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			lx_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->tcpc->typec_polarity + 1);
}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t battery_id_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	int battery_id = 0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}
	battery_id = fuel_gauge_get_battery_id(manager->fuel_gauge);

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", battery_id);
}
static CLASS_ATTR_RO(battery_id);

static ssize_t batt_manufacturer_show(struct class *c,
    struct class_attribute *attr, char *buf)
{
	int battery_id = 0;
	const char *battery_type[] = {"Unknown", "NVT_ATL", "COS_COS", "SWD_LWN"};
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	battery_id = fuel_gauge_get_battery_id(manager->fuel_gauge);
	if (battery_id < 0) {
		lx_err("battery_id is_err %d\n", battery_id);
		goto out;
	}
	return scnprintf(buf, PAGE_SIZE, "%s\n", battery_type[battery_id]);

out:
	return scnprintf(buf, PAGE_SIZE, "%s\n", "Unknown");
}
static CLASS_ATTR_RO(batt_manufacturer);

static const char * const usb_typec_mode_text[] = {
	"Nothing attached", "Source attached", "Sink attached",
	"Audio Adapter", "Non compliant",
};

static ssize_t typec_mode_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);

	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			lx_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n",usb_typec_mode_text[manager->tcpc->typec_mode]);
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t chip_ok_show(struct class *c,
	struct class_attribute *attr, char *buf)
{
	bool chip_ok_status = false;
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	chip_ok_status = fuel_gauge_get_chip_ok(manager->fuel_gauge);

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", chip_ok_status);
}
static CLASS_ATTR_RO(chip_ok);

static ssize_t resistance_id_show(struct class *c,
	struct class_attribute *attr, char *buf)
{
	int resistance_id = 0;
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
		manager->fuel_gauge = fuel_gauge_find_dev_by_name("fuel_gauge");
		if (IS_ERR_OR_NULL(manager->fuel_gauge)) {
			lx_err("manager->fuel_gauge is_err_or_null\n");
			goto out;
		}
	}

	resistance_id = fuel_gauge_get_resistance_id(manager->fuel_gauge);

out:
	return scnprintf(buf, PAGE_SIZE, "%d\n", resistance_id);
}
static CLASS_ATTR_RO(resistance_id);

static ssize_t input_suspend_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	if (IS_ERR_OR_NULL(manager))
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%d\n", manager->input_suspend);
}

static ssize_t input_suspend_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	int val;
	struct bq_fg_chip *bp = NULL;

	lx_info("input_suspend_store start\n");

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bp = fuel_gauge_get_private(manager->fuel_gauge);
	if (IS_ERR_OR_NULL(bp))
		return PTR_ERR(bp);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	bp->input_suspend = val;
	manager->input_suspend = val;
	if (manager->input_suspend) {
		vote(manager->main_icl_votable, FACTORY_KIT_VOTER, true, 0);
	} else {
		vote(manager->main_icl_votable, FACTORY_KIT_VOTER, false, 0);
	}
	lx_info("manager->input_suspend = %d\n", manager->input_suspend);
	return count;
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t set_ship_mode_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->setshipmode);
}

static ssize_t set_ship_mode_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	int ret;
	int val;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->setshipmode = val;
	ret = charger_set_shipmode(manager->charger, manager->setshipmode);
	if (ret < 0)
		lx_err("can not set shipmode\n");

	lx_info("setshipmode = %d\n", manager->setshipmode);

	return count;
}
static CLASS_ATTR_RW(set_ship_mode);

static ssize_t shipmode_count_reset_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	return sprintf(buf, "%d\n", manager->shippingmode);
}

static ssize_t shipmode_count_reset_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);
	union power_supply_propval volt;
	int shipmode_cnt = 0;
	int i = 0;
	int ret;
	int val;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->batt_psy)) {
		lx_err("batt_psy is_err_or_null\n");
		return PTR_ERR(manager->batt_psy);
	}

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	manager->shippingmode = val;

//cqr vbat ctrol start

	for (i = 0; i < CQR_READ_VBAT_TIMES; i++) {
			ret = power_supply_get_property(manager->batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &volt);
			if (ret) {
				lx_err("get vbat fail\n");
				continue;
			}

			msleep(1);

			if (volt.intval < CQR_VBAT_MIM || volt.intval > CQR_VBAT_MAX) {
				shipmode_cnt++;
				lx_info("%s %d %d %d %d %d\n",
					__func__, manager->shippingmode, volt.intval, CQR_VBAT_MAX, CQR_VBAT_MIM, shipmode_cnt);
			}
		}

	if (shipmode_cnt >= CQR_VBAT_OUT_RANGE_CNT) {
		manager->shippingmode = false;
		lx_info("%s after %d\n", __func__, manager->shippingmode);
		ret = charger_shipmode_count_reset(manager->charger, manager->shippingmode);
		if (ret < 0)
			lx_err("can not set shipmode\n");
		return -EINVAL;
	}
//cqr vbat ctrol end
	ret = charger_shipmode_count_reset(manager->charger, manager->shippingmode);
	if (ret < 0)
		lx_err("can not set shipmode\n");

	lx_info("shippingmode = %d\n", manager->shippingmode);

	return count;
}
static CLASS_ATTR_RW(shipmode_count_reset);

static ssize_t mtbf_current_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);
	struct bq_fg_chip *bp = NULL;
	int val;

	lx_info("mtbf_mode_store start\n");
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	bp = fuel_gauge_get_private(manager->fuel_gauge);

	if (IS_ERR_OR_NULL(bp))
		return PTR_ERR(bp);

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	bp->mtbf_current = val;

	if (val != 0) {
		is_mtbf_mode = 1;
		lx_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		lx_info("is_mtbf_mode = 0\n");
	}
	return count;
}

static ssize_t mtbf_current_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_mtbf_mode);
}
static CLASS_ATTR_RW(mtbf_current);

static ssize_t mtbf_mode_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;

	lx_info("mtbf_mode_store start\n");

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val != 0) {
		is_mtbf_mode = 1;
		lx_info("is_mtbf_mode = 1\n");
	} else {
		is_mtbf_mode = 0;
		lx_info("is_mtbf_mode = 0\n");
	}
	return count;
}

static ssize_t mtbf_mode_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_mtbf_mode);
}
static CLASS_ATTR_RW(mtbf_mode);

static ssize_t cp_vendor_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	int cp_vendor = 0;
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->master_cp_chg)) {
		lx_err("manager->master_cp_chg is_err_or_null\n");
		goto out;
	}

	cp_vendor = chargerpump_get_cp_vendor(manager->master_cp_chg);

out:
	return scnprintf(buf, PAGE_SIZE, "%s\n", cp_vendor);
}
static CLASS_ATTR_RO(cp_vendor);

static ssize_t cp_bus_voltage_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	int cp_vbus = 0;
	int ret = 0;
	struct charger_manager *manager = container_of(c, 
			struct charger_manager, lx_charger_class);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->master_cp_chg)) {
		lx_err("manager->master_cp_chg is_err_or_null\n");
		goto out;
	}

	ret = chargerpump_get_adc_value(manager->master_cp_chg, ADC_GET_VBUS, &cp_vbus);
	if (ret < 0) {
		lx_err("failed to get cp_vbus\n");
	}

out:
	return sprintf(buf, "%d\n", cp_vbus);
}
static CLASS_ATTR_RO(cp_bus_voltage);

static ssize_t chg_vbat_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	int ret = 0;
	int  bat_volt = 0;
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	ret = charger_get_adc(manager->charger, ADC_GET_VBAT, &bat_volt);
	if (ret < 0) {
		lx_err("failed to get ADC_GET_VBAT\n");
	}

	return sprintf(buf, "%d\n", bat_volt*1000);
}
static CLASS_ATTR_RO(chg_vbat);

static ssize_t iterm_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct votable *iterm_votable;
	int  iterm_val = 0;
	
	iterm_votable = find_votable("MAIN_ITERM");
	if (!iterm_votable) {
		lx_err("%s failed to get iterm_votable\n", __func__);
        return -EINVAL;
	}

	iterm_val = get_effective_result(iterm_votable);

	return scnprintf(buf, PAGE_SIZE, "%d\n", iterm_val);
}
static CLASS_ATTR_RO(iterm);

static ssize_t iterm_voter_show(struct class *cls,
    struct class_attribute *attr, char *buf)
{
    struct votable *iterm_votable;
    const char *iterm_client;

    iterm_votable = find_votable("MAIN_ITERM");
    if (!iterm_votable) {
        lx_err("%s failed to get iterm_votable\n", __func__);
        return -EINVAL;
    }

    iterm_client = get_effective_client(iterm_votable);
    if (!iterm_client) {
        return scnprintf(buf, PAGE_SIZE, "Unknown\n");
    }

    return scnprintf(buf, PAGE_SIZE, "%s\n", iterm_client);
}
static CLASS_ATTR_RO(iterm_voter);

static ssize_t cp_bus_current_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	int cp_ibus = 0;
	int ret = 0;
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);
	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->master_cp_chg)) {
		lx_err("manager->master_cp_chg is_err_or_null\n");
		goto out;
	}

	ret = chargerpump_get_adc_value(manager->master_cp_chg, ADC_GET_IBUS, &cp_ibus);
	if (ret < 0) {
		lx_err("failed to get cp_ibus\n");
	}

out:
	return sprintf(buf, "%d\n", cp_ibus);
}
static CLASS_ATTR_RO(cp_bus_current);


static ssize_t manufacturer_show(struct class *c,
    struct class_attribute *attr, char *buf)
{
	const char * const subpmic_type[] = {
		"Unknown",
		"Southchip-SC6601",
		"NuVolta-NU6601"
	};
	int manufacturer_id = 0;
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->charger)) {
		lx_err("manager->master_cp_chg is_err_or_null\n");
		goto out;
	}

	manufacturer_id = charger_get_manufacturer(manager->charger);
	if (manufacturer_id == SC6601A_DEVICE_ID) {
		return scnprintf(buf, PAGE_SIZE, "%s\n", subpmic_type[SUBPMIC_SC6601]);
	} else if (manufacturer_id == NU6601_DEVICE_ID) {
		return scnprintf(buf, PAGE_SIZE, "%s\n", subpmic_type[SUBPMIC_NU6601]);
	} else {
		goto out;
	}

out:
	return scnprintf(buf, PAGE_SIZE, "%s\n", subpmic_type[SUBPMIC_UNKNOWN]);
}
static CLASS_ATTR_RO(manufacturer);

static ssize_t authentic_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct power_supply *psy_bms = NULL;
	struct bq_fg_chip *gm;

	psy_bms = power_supply_get_by_name("bms");
	if (!IS_ERR_OR_NULL(psy_bms)) {
		gm = power_supply_get_drvdata(psy_bms);
		if (!IS_ERR_OR_NULL(gm))
			return scnprintf(buf, PAGE_SIZE, "%d\n", gm->authenticate);
		else
			lx_err("get bms psy drv data failed\n");
	} else
		lx_err("get bms psy failed\n");

	return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static CLASS_ATTR_RO(authentic);

static int get_apdo_max(struct charger_manager *manager) {
	struct adapter_dev *adapter = NULL;
	int apdo_max = 0;
	int ret =0;

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
	if (manager->pd_active != CHARGE_PD_PPS_ACTIVE) {
		goto done;
	}

	adapter = adapter_find_dev_by_name("pd_adapter1");
	if (adapter == NULL || g_policy == NULL )
		goto done;

	ret = adapter_get_cap(adapter, &g_policy->cap);
		if (ret < 0) {
			lx_info("adapter get cap failed\n");
			goto done;
		}
	chargerpump_policy_check_adapter_cap(g_policy, &g_policy->cap);

	apdo_max = g_policy->cap.volt_max[g_policy->cap_nr] *
		g_policy->cap.curr_max[g_policy->cap_nr] / 1000000;

done:
	return apdo_max;
}

static ssize_t apdo_max_show(struct class *c,
    struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	manager->apdo_max = get_apdo_max(manager);

	lx_info("apdo_max = %d\n", manager->apdo_max);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->apdo_max);
}
static CLASS_ATTR_RO(apdo_max);

static ssize_t source_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val) {
		tcpm_typec_change_role_postpone(manager->tcpc, TYPEC_ROLE_SRC, true);
		manager->force_source = true;
	}

	return count;
}

static ssize_t source_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->force_source);
}
static CLASS_ATTR_RW(source);

static ssize_t pr_swap_test_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	if (IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}

	if (val)
		manager->pr_swap_test = true;
	else
		manager->pr_swap_test = false;

	return count;
}

static ssize_t pr_swap_test_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->pr_swap_test);
}
static CLASS_ATTR_RW(pr_swap_test);

static ssize_t fcc_show(struct class *cls,
    struct class_attribute *attr, char *buf)
{
    struct votable *fcc_votable;
    int fcc_val = 0;

    fcc_votable = find_votable("TOTAL_FCC");
    if (!fcc_votable) {
        lx_err("%s failed to get fcc_votable\n", __func__);
        return -EINVAL;
    }

    fcc_val = get_effective_result(fcc_votable);

    return scnprintf(buf, PAGE_SIZE, "%d\n", fcc_val);
}
static CLASS_ATTR_RO(fcc);

static ssize_t fcc_voter_show(struct class *cls,
    struct class_attribute *attr, char *buf)
{
    struct votable *fcc_votable;
    const char *fcc_client;

    fcc_votable = find_votable("TOTAL_FCC");
    if (!fcc_votable) {
        lx_err("%s failed to get fcc_votable\n", __func__);
        return -EINVAL;
    }

    fcc_client = get_effective_client(fcc_votable);
    if (!fcc_client) {
        return scnprintf(buf, PAGE_SIZE, "Unknown\n");
    }

    return scnprintf(buf, PAGE_SIZE, "%s\n", fcc_client);
}
static CLASS_ATTR_RO(fcc_voter);

static ssize_t icl_show(struct class *cls,
    struct class_attribute *attr, char *buf)
{
    struct votable *icl_votable;
    int icl_val = 0;

    icl_votable = find_votable("MAIN_ICL");
    if (!icl_votable) {
        lx_err("%s failed to get icl_votable\n", __func__);
        return -EINVAL;
    }

    icl_val = get_effective_result(icl_votable);

    return scnprintf(buf, PAGE_SIZE, "%d\n", icl_val);
}
static CLASS_ATTR_RO(icl);

static ssize_t icl_voter_show(struct class *cls,
    struct class_attribute *attr, char *buf)
{
    struct votable *icl_votable;
    const char *icl_client;

    icl_votable = find_votable("MAIN_ICL");
    if (!icl_votable) {
        lx_err("%s failed to get icl_votable\n", __func__);
        return -EINVAL;
    }

    icl_client = get_effective_client(icl_votable);
    if (!icl_client) {
        return scnprintf(buf, PAGE_SIZE, "Unknown\n");
    }

    return scnprintf(buf, PAGE_SIZE, "%s\n", icl_client);
}
static CLASS_ATTR_RO(icl_voter);

static ssize_t fv_show(struct class *cls,
    struct class_attribute *attr, char *buf)
{
    struct votable *fv_votable;
    int fv_val = 0;

    fv_votable = find_votable("MAIN_FV");
    if (!fv_votable) {
        lx_err("%s failed to get fv_votable\n", __func__);
        return -EINVAL;
    }

    fv_val = get_effective_result(fv_votable);

    return scnprintf(buf, PAGE_SIZE, "%d\n", fv_val);
}
static CLASS_ATTR_RO(fv);

static ssize_t fv_voter_show(struct class *cls,
    struct class_attribute *attr, char *buf)
{
    struct votable *fv_votable;
    const char *fv_client;

    fv_votable = find_votable("MAIN_FV");
    if (!fv_votable) {
        lx_err("%s failed to get fv_votable\n", __func__);
        return -EINVAL;
    }

    fv_client = get_effective_client(fv_votable);
    if (!fv_client) {
        return scnprintf(buf, PAGE_SIZE, "Unknown\n");
    }

    return scnprintf(buf, PAGE_SIZE, "%s\n", fv_client);
}
static CLASS_ATTR_RO(fv_voter);

static ssize_t audio_cctog_store(struct class *c,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);

	if (IS_ERR_OR_NULL(manager->tcpc)) {
		manager->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (IS_ERR_OR_NULL(manager->tcpc)) {
			lx_err("manager->tcpc is_err_or_null\n");
			return PTR_ERR(manager);
		}
	}

	if (kstrtoint(buf, 10, &val)) {
		lx_info("set buf error %s\n", buf);
		return -EINVAL;
	}
          
#ifndef FACTORY_BUILD
	manager->audio_cctog = !!val;
	lx_err("wxf audio_cctog %d \n", manager->audio_cctog);
	if (manager->swcid_bright == 0 && manager->cid_status ==0) {
		if (manager->audio_cctog == 0)
			tcpci_set_cc(manager->tcpc, TYPEC_CC_RD);
		else
			tcpci_set_cc(manager->tcpc, TYPEC_CC_DRP);
	}
#endif
	return count;
}

static ssize_t audio_cctog_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct charger_manager *manager = container_of(c,
			struct charger_manager, lx_charger_class);

	if(IS_ERR_OR_NULL(manager))
		return PTR_ERR(manager);
#ifndef FACTORY_BUILD
	return scnprintf(buf, PAGE_SIZE, "%d\n", manager->audio_cctog);
#else
	return scnprintf(buf, PAGE_SIZE, "%d\n", 0);
#endif
}
static CLASS_ATTR_RW(audio_cctog);
static struct attribute *lx_charger_attrs[] = {
	&class_attr_real_type.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_battery_id.attr,
	&class_attr_batt_manufacturer.attr,
	&class_attr_typec_mode.attr,
	&class_attr_input_suspend.attr,
	&class_attr_set_ship_mode.attr,
	&class_attr_shipmode_count_reset.attr,
	&class_attr_resistance_id.attr,
	&class_attr_chip_ok.attr,
	&class_attr_mtbf_current.attr,
	&class_attr_mtbf_mode.attr,
	&class_attr_cp_vendor.attr,
	&class_attr_cp_bus_voltage.attr,
	&class_attr_chg_vbat.attr,
	&class_attr_cp_bus_current.attr,
	&class_attr_manufacturer.attr,
	&class_attr_authentic.attr,
	&class_attr_apdo_max.attr,
	&class_attr_source.attr,
	&class_attr_pr_swap_test.attr,
	// vote node
	&class_attr_fcc.attr,
	&class_attr_fcc_voter.attr,
	&class_attr_icl.attr,
	&class_attr_icl_voter.attr,
	&class_attr_fv.attr,
	&class_attr_fv_voter.attr,
  	&class_attr_iterm.attr,
	&class_attr_iterm_voter.attr,
	&class_attr_audio_cctog.attr,
	NULL,
};

ATTRIBUTE_GROUPS(lx_charger);

int lx_charger_node_init(struct charger_manager *manager)
{
	int ret = 0;

	manager->lx_charger_class.name = "lx_charger";
	manager->lx_charger_class.class_groups = lx_charger_groups;
	ret = class_register(&manager->lx_charger_class);
	if (ret < 0) {
		lx_err("Failed to create lx_charger_class\n");
		goto out;
	}
out:
	return ret;
}
EXPORT_SYMBOL(lx_charger_node_init);