/*******************************************************************************************
* Description:	XIAOMI-BSP-CHARGE
* 		This xmc_core.c is the entry of charge code.
*		Jump to respective drivers and ALGO initializations according to DTSI config
* ------------------------------ Revision History: --------------------------------
* <version>	<date>		<author>			<desc>
* 1.0		2022-02-22	chenyichun@xiaomi.com		Created for new architecture
********************************************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>

#include "xmc_core.h"

static const char *thermal_table_name[THERMAL_TABLE_NUM] = {
	"thermal_limit_dcp",
	"thermal_limit_qc2",
	"thermal_limit_qc3_18w",
	"thermal_limit_qc3_27w",
	"thermal_limit_qc35",
	"thermal_limit_pd",
};

enum copackage_type copackage_type = CHARGE_COPACKAGE_NORMAL;

static int log_level = 1;
module_param_named(log_level, log_level, int, 0600);

static int cycle_count = 0;
module_param_named(cycle_count, cycle_count, int, 0600);

bool __attribute__ ((weak)) max77932_init(void) { return true; }
bool __attribute__ ((weak)) mp2762_init(void) { return true; }
bool __attribute__ ((weak)) bq27z561_init(struct charge_chip *chip) { return true; }
bool __attribute__ ((weak)) sc8551_init(void) { return true; }
bool __attribute__ ((weak)) sc8561_init(void) { return true; }
bool __attribute__ ((weak)) ln8000_init(void) { return true; }
bool __attribute__ ((weak)) bq25980_init(void) { return true; }
bool __attribute__ ((weak)) xmusb350_init(void) { return true; }
bool __attribute__ ((weak)) xmc_pdm_init(struct charge_chip *chip) { return true; }
bool __attribute__ ((weak)) xmc_qcm_init(struct charge_chip *chip) { return true; }
bool __attribute__ ((weak)) xmc_adapter_init(struct charge_chip *chip) { return true; }

int xmc_get_log_level(void)
{
	return log_level;
}

/* Charger devices contain OPS_API to control IC driver */
static bool xmc_check_charger_dev(struct charge_chip *chip)
{
	if (!chip->master_cp_dev)
		chip->master_cp_dev = xmc_ops_find_device("cp_master");

	if (!chip->master_cp_dev) {
		xmc_err("[XMC_PROBE] failed to get master_cp_dev\n");
		return false;
	}

	if (!chip->slave_cp_dev)
		chip->slave_cp_dev = xmc_ops_find_device("cp_slave");

	if (!chip->slave_cp_dev) {
		xmc_err("[XMC_PROBE] failed to get slave_cp_dev\n");
		return false;
	}

	if (!chip->bbc_dev)
		chip->bbc_dev = xmc_ops_find_device("bbc");

	if (!chip->bbc_dev) {
		xmc_err("[XMC_PROBE] failed to get bbc_dev\n");
		return false;
	}

	if (!chip->tcpc_dev)
		chip->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");

	if (!chip->tcpc_dev) {
		xmc_err("[XMC_PROBE] failed to get tcpc_dev\n");
		return false;
	}

	return true;
}

/* Voter is used to set BUCK_BOOST's FCC/ICL/FV... */
static bool xmc_check_voter(struct charge_chip *chip)
{
	if (!chip->bbc_en_votable) {
		chip->bbc_en_votable = find_votable("BBC_ENABLE");
		if (!chip->bbc_en_votable) {
			xmc_err("[XMC_PROBE] failed to get bbc_en_votable\n");
			return false;
		}
	}

	if (!chip->bbc_icl_votable) {
		chip->bbc_icl_votable = find_votable("BBC_ICL");
		if (!chip->bbc_icl_votable) {
			xmc_err("[XMC_PROBE] failed to get bbc_icl_votable\n");
			return false;
		}
	}

	if (!chip->bbc_fcc_votable) {
		chip->bbc_fcc_votable = find_votable("BBC_FCC");
		if (!chip->bbc_fcc_votable) {
			xmc_err("[XMC_PROBE] failed to get bbc_fcc_votable\n");
			return false;
		}
	}

	if (!chip->bbc_fv_votable) {
		chip->bbc_fv_votable = find_votable("BBC_FV");
		if (!chip->bbc_fv_votable) {
			xmc_err("[XMC_PROBE] failed to get bbc_fv_votable\n");
			return false;
		}
	}

	if (!chip->bbc_vinmin_votable) {
		chip->bbc_vinmin_votable = find_votable("BBC_VINMIN");
		if (!chip->bbc_vinmin_votable) {
			xmc_err("[XMC_PROBE] failed to get bbc_vinmin_votable\n");
			return false;
		}
	}

	if (!chip->bbc_iterm_votable) {
		chip->bbc_iterm_votable = find_votable("BBC_ITERM");
		if (!chip->bbc_iterm_votable) {
			xmc_err("[XMC_PROBE] failed to get bbc_iterm_votable\n");
			return false;
		}
	}

	return true;
}

static bool xmc_reset_bbc(void)
{
	/* unregister the components registered in MTK PMIC driver, because we use the third part BBC chip */
	struct xmc_device *bbc_dev = xmc_ops_find_device("bbc");
	struct power_supply *bbc_psy = power_supply_get_by_name("bbc");
	struct votable *bbc_en_votable = find_votable("BBC_ENABLE");
	struct votable *bbc_icl_votable = find_votable("BBC_ICL");
	struct votable *bbc_fcc_votable = find_votable("BBC_FCC");
	struct votable *bbc_fv_votable = find_votable("BBC_FV");
	struct votable *bbc_vinmin_votable = find_votable("BBC_VINMIN");
	struct votable *bbc_iterm_votable = find_votable("BBC_ITERM");

	if (!bbc_dev || !bbc_psy || !bbc_en_votable || !bbc_icl_votable || !bbc_fcc_votable || !bbc_fv_votable || !bbc_vinmin_votable || !bbc_iterm_votable) {
		xmc_err("[XMC_PROBE] failed to get bbc components\n");
		return false;
	}

	power_supply_unregister(bbc_psy);
	xmc_device_unregister(bbc_dev);
	destroy_votable(bbc_en_votable);
	destroy_votable(bbc_icl_votable);
	destroy_votable(bbc_fcc_votable);
	destroy_votable(bbc_fv_votable);
	destroy_votable(bbc_vinmin_votable);
	destroy_votable(bbc_iterm_votable);

	return true;
}

static bool xmc_find_chip(int *list, int num, int target)
{
	int i = 0;

	for (i = 0; i < num; i++) {
		if (list[i] == target)
			return true;
	}

	return false;
}

/* For diffrent battery cycle_count, should use diffrent step_charge config. Charge faster if cycle_count is low. */
bool xmc_parse_step_chg_config(struct charge_chip *chip, bool force_update)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *step_jeita_node = NULL;
	union power_supply_propval pval = {0,};
	static bool low_cycle = true;
	int total_length = 0, i = 0;
	bool cycle_update = false, ret = false;

	if (node)
		step_jeita_node = of_find_node_by_name(node, "step_jeita");

	if (!node || !step_jeita_node) {
		xmc_err("[XMC_PROBE] device tree node missing\n");
		return false;
	}

	power_supply_get_property(chip->battery_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	chip->battery.cycle_count = cycle_count ? cycle_count : pval.intval;

	if (low_cycle) {
		if (chip->battery.cycle_count > 100) {
			low_cycle = false;
			cycle_update = true;
		}
	} else {
		if (chip->battery.cycle_count <= 100) {
			low_cycle = true;
			cycle_update = true;
		}
	}

	if (cycle_update || force_update) {
		if (low_cycle) {
			total_length = of_property_count_elems_of_size(step_jeita_node, "step_chg_cfg_low_cycle", sizeof(u32));
			ret |= of_property_read_u32_array(step_jeita_node, "step_chg_cfg_low_cycle", (u32 *)chip->step_chg_cfg, total_length);
			if (ret) {
				xmc_err("[XMC_PROBE] failed to parse step_chg_cfg_low_cycle\n");
				return false;
			}
		} else {
			total_length = of_property_count_elems_of_size(step_jeita_node, "step_chg_cfg_high_cycle", sizeof(u32));
			ret |= of_property_read_u32_array(step_jeita_node, "step_chg_cfg_high_cycle", (u32 *)chip->step_chg_cfg, total_length);
			if (ret) {
				xmc_err("[XMC_PROBE] failed to parse step_chg_cfg_high_cycle\n");
				return false;
			}
		}

		for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++)
			xmc_info("[XMC_PROBE] [STEP_CHG] %d %d %d\n", chip->step_chg_cfg[i].low_threshold, chip->step_chg_cfg[i].high_threshold, chip->step_chg_cfg[i].value);
	}

	return !ret;
}

static bool xmc_parse_charge_dt(struct charge_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *thermal_limit_node = NULL, *step_jeita_node = NULL;
	int total_length = 0, i = 0;
	bool ret = false;

	if (node) {
		thermal_limit_node = of_find_node_by_name(node, "thermal_limit");
		step_jeita_node = of_find_node_by_name(node, "step_jeita");
	}

	if (!node || !thermal_limit_node || !step_jeita_node) {
		xmc_err("[XMC_PROBE] device tree node missing\n");
		return false;
	}

	for (i = 0; i < THERMAL_TABLE_NUM; i++) {
		ret |= of_property_read_u32_array(thermal_limit_node, thermal_table_name[i], (u32 *)(chip->thermal_limit[i]), THERMAL_LEVEL_NUM);
		if (ret) {
			xmc_err("[XMC_PROBE] failed to parse thermal_limit[%d]\n", i);
			return false;
		}
	}

	for (i = 0; i < THERMAL_LEVEL_NUM; i++)
		xmc_info("[XMC_PROBE] [thermal_limit][%d %d %d %d %d %d]\n", chip->thermal_limit[0][i], chip->thermal_limit[1][i], chip->thermal_limit[2][i],
			chip->thermal_limit[3][i], chip->thermal_limit[4][i], chip->thermal_limit[5][i]);

	total_length = of_property_count_elems_of_size(step_jeita_node, "jeita_fcc_cfg", sizeof(u32));
	if (total_length < 0) {
		xmc_err("[XMC_PROBE] failed to read total_length of jeita_fcc_cfg\n");
		return false;
	}

	ret |= of_property_read_u32_array(step_jeita_node, "jeita_fcc_cfg", (u32 *)chip->jeita_fcc_cfg, total_length);
	if (ret) {
		xmc_err("[XMC_PROBE] failed to parse jeita_fcc_cfg\n");
		return false;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++)
		xmc_info("[XMC_PROBE] [JEITA_FCC] %d %d %d %d %d\n", chip->jeita_fcc_cfg[i].low_threshold, chip->jeita_fcc_cfg[i].high_threshold,
			chip->jeita_fcc_cfg[i].extra_threshold,chip->jeita_fcc_cfg[i].low_value, chip->jeita_fcc_cfg[i].high_value);

	total_length = of_property_count_elems_of_size(step_jeita_node, "jeita_fv_cfg", sizeof(u32));
	if (total_length < 0) {
		xmc_err("[XMC_PROBE] failed to read total_length of jeita_fv_cfg\n");
		return false;
	}

	ret |= of_property_read_u32_array(step_jeita_node, "jeita_fv_cfg", (u32 *)chip->jeita_fv_cfg, total_length);
	if (ret) {
		xmc_err("[XMC_PROBE] failed to parse jeita_fv_cfg\n");
		return false;
	}

	for (i = 0; i < STEP_JEITA_TUPLE_NUM; i++)
		xmc_info("[XMC_PROBE] [JEITA_FV] %d %d %d\n", chip->jeita_fv_cfg[i].low_threshold, chip->jeita_fv_cfg[i].high_threshold, chip->jeita_fv_cfg[i].value);

	ret |= of_property_read_u32(step_jeita_node, "step_fallback_hyst", &chip->step_fallback_hyst);
	ret |= of_property_read_u32(step_jeita_node, "step_forward_hyst", &chip->step_forward_hyst);
	ret |= of_property_read_u32(step_jeita_node, "jeita_fallback_hyst", &chip->jeita_fallback_hyst);
	ret |= of_property_read_u32(step_jeita_node, "jeita_forward_hyst", &chip->jeita_forward_hyst);
	ret |= of_property_read_u32(step_jeita_node, "jeita_forward_hyst", &chip->cycle_count_threshold);
	ret |= of_property_read_u32(step_jeita_node, "jeita_forward_hyst", &chip->jeita_hysteresis);

	if (!xmc_parse_step_chg_config(chip, true)) {
		xmc_err("[XMC_PROBE] failed to parse step_chg_config\n");
		return false;
	}

	return !ret;
}

static bool xmc_parse_basic_dt(struct charge_chip *chip)
{
	struct device_node *node = chip->dev->of_node;
	struct device_node *chip_list_node = NULL, *feature_list_node = NULL, *basic_charge_node = NULL;
	bool ret = false;

	if (node) {
		chip_list_node = of_find_node_by_name(node, "chip_list");
		feature_list_node = of_find_node_by_name(node, "feature_list");
		basic_charge_node = of_find_node_by_name(node, "basic_charge");
	}

	if (!node || !chip_list_node || !feature_list_node || !basic_charge_node) {
		xmc_err("[XMC_PROBE] device tree node missing\n");
		return false;
	}

	chip->usb_typec.vbus_control = devm_regulator_get(chip->dev, "vbus_control");
	if (IS_ERR(chip->usb_typec.vbus_control))
		xmc_err("[XMC_PROBE] failed to get vbus_control regulator\n");

	ret |= of_property_read_u32(chip_list_node, "battery_type", &chip->chip_list.battery_type);
	ret |= of_property_read_u32(chip_list_node, "gauge_chip", &chip->chip_list.gauge_chip);
	ret |= of_property_read_u32(chip_list_node, "buck_boost", &chip->chip_list.buck_boost);
	ret |= of_property_read_u32(chip_list_node, "cp_div_type", &chip->chip_list.cp_div_type);
	ret |= of_property_read_u32(chip_list_node, "cp_com_type", &chip->chip_list.cp_com_type);
	ret |= of_property_read_u32(chip_list_node, "third_cp", &chip->chip_list.third_cp);
	ret |= of_property_read_u32(chip_list_node, "bc12_qc_chip", &chip->chip_list.bc12_qc_chip);
	ret |= of_property_read_u32_array(chip_list_node, "charge_pump", (u32 *)(chip->chip_list.charge_pump), MAX_CP_DRIVER_NUM);
	if (ret) {
		xmc_err("[XMC_PROBE] failed to parse chip_list\n");
		return false;
	}

	xmc_info("[XMC_PROBE] [chip_list][BATTERY GAUGE BBC CP_DIV_TYPE CP_COM_TYPE THIRD_CP PROTOCOL_CHIP [CP_CHIP]] = [%d, %d, %d, %d, %d, %d, [%d %d %d]]\n",
		chip->chip_list.battery_type, chip->chip_list.gauge_chip, chip->chip_list.buck_boost, chip->chip_list.cp_div_type, chip->chip_list.cp_com_type,
		chip->chip_list.third_cp, chip->chip_list.bc12_qc_chip, chip->chip_list.charge_pump[0], chip->chip_list.charge_pump[1], chip->chip_list.charge_pump[2]);

	chip->feature_list.pdm_support = of_property_read_bool(feature_list_node, "pdm_support");
	chip->feature_list.qcm_support = of_property_read_bool(feature_list_node, "qcm_support");
	chip->feature_list.qc3_support = of_property_read_bool(feature_list_node, "qc3_support");
	chip->feature_list.bypass_support = of_property_read_bool(feature_list_node, "bypass_support");
	chip->feature_list.sic_support = of_property_read_bool(feature_list_node, "sic_support");

	xmc_info("[XMC_PROBE] [feature_list][PDM QCM QC3 BYPASS SIC] = [%d, %d, %d, %d]\n", chip->feature_list.pdm_support, chip->feature_list.qcm_support,
		chip->feature_list.qc3_support, chip->feature_list.bypass_support, chip->feature_list.sic_support);

	ret |= of_property_read_u32(basic_charge_node, "main_monitor_delay", &chip->main_monitor_delay);
	ret |= of_property_read_u32(basic_charge_node, "second_monitor_delay", &chip->second_monitor_delay);
	ret |= of_property_read_u32(basic_charge_node, "fv", &chip->fv);
	ret |= of_property_read_u32(basic_charge_node, "ffc_fv", &chip->ffc_fv);
	ret |= of_property_read_u32(basic_charge_node, "bbc_max_fcc", &chip->bbc_max_fcc);
	ret |= of_property_read_u32(basic_charge_node, "bbc_max_icl", &chip->bbc_max_icl);
	ret |= of_property_read_u32_array(basic_charge_node, "fcc", (u32 *)(chip->fcc), VOTE_CHARGER_TYPE_NUM);
	ret |= of_property_read_u32_array(basic_charge_node, "icl", (u32 *)(chip->icl), VOTE_CHARGER_TYPE_NUM);
	ret |= of_property_read_u32_array(basic_charge_node, "mivr", (u32 *)(chip->mivr), VOTE_CHARGER_TYPE_NUM);
	if (ret) {
		xmc_err("[XMC_PROBE] failed to parse basic_charge\n");
		return false;
	}

	return !ret;
}

/* If two different programs are CO-Package, should config "xiaomi_charge" and "xiaomi_charge_pro" node in DTSI.
   Use diffrent HWID, pass CMD line from LK. Before kernel probe, parse CMD line and decide which node to probe. */
static void xmc_parse_copackage_info(void)
{
	copackage_type = CHARGE_COPACKAGE_NORMAL;
}

static const struct platform_device_id xmc_id[] = {
	{ "xiaomi_charge", CHARGE_COPACKAGE_NORMAL },
	{ "xiaomi_charge_pro", CHARGE_COPACKAGE_PRO },
	{},
};
MODULE_DEVICE_TABLE(platform, xmc_id);

static const struct of_device_id xmc_of_match[] = {
	{ .compatible = "xiaomi_charge", .data = &xmc_id[0], },
	{ .compatible = "xiaomi_charge_pro", .data = &xmc_id[1], },
	{},
};
MODULE_DEVICE_TABLE(of, xmc_of_match);

static int xmc_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct charge_chip *chip;

	xmc_info("[XMC_PROBE] XMC probe start\n");

	if (!strncmp(CONFIG_CHARGE_PROJECT, "PISSARRO", 8) || !strncmp(CONFIG_CHARGE_PROJECT, "OTHER_COPACKAGE_PROJECT", 23)) {
		xmc_parse_copackage_info();

		of_id = of_match_device(xmc_of_match, &pdev->dev);
		pdev->id_entry = of_id->data;

		if (pdev->id_entry->driver_data == copackage_type) {
			xmc_info("[XMC_PROBE] copackage_type = %d\n", pdev->id_entry->driver_data);
		} else {
			xmc_info("[XMC_PROBE] copackage_type not match, don't probe, %d\n", pdev->id_entry->driver_data);
			return -ENODEV;
		}
	}

	chip = devm_kzalloc(&pdev->dev, sizeof(struct charge_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	platform_set_drvdata(pdev, chip);
	mutex_init(&chip->charger_type_lock);
	chip->usb_typec.burn_wakelock = wakeup_source_register(NULL, "typec_burn_wakelock");

	if (!xmc_parse_basic_dt(chip)) {
		xmc_err("[XMC_PROBE] failed to parse basic DTSI\n");
		return -ENODEV;
	}

	if (chip->chip_list.third_cp == TCP_MAX77932)
		max77932_init();

	if (chip->chip_list.gauge_chip == BQ27Z561_NFG1000_BQ28Z610)
		/* This driver is compatible for BQ27Z561/NFG1000A/NFG1000B/BQ28Z610 */
		bq27z561_init(chip);

	if (chip->chip_list.buck_boost != BBC_PMIC) {
		if (!xmc_reset_bbc()) {
			xmc_err("[XMC_PROBE] failed to reset BBC\n");
			return -ENODEV;
		}
		if (chip->chip_list.buck_boost == BBC_MP2762)
			mp2762_init();
	}

	if (xmc_find_chip(chip->chip_list.charge_pump, MAX_CP_DRIVER_NUM, CPC_BQ25970_SC8551))
		/* This driver is compatible for BQ25970/SC8551 */
		sc8551_init();

	if (xmc_find_chip(chip->chip_list.charge_pump, MAX_CP_DRIVER_NUM, CPC_SC8561))
		sc8561_init();

	if (xmc_find_chip(chip->chip_list.charge_pump, MAX_CP_DRIVER_NUM, CPC_LN8000))
		ln8000_init();

	if (xmc_find_chip(chip->chip_list.charge_pump, MAX_CP_DRIVER_NUM, CPC_BQ25980_SC8571))
		/* This driver is compatible for BQ25980/SC8571 */
		bq25980_init();

	if (chip->chip_list.bc12_qc_chip == BC12_XMUSB350_I350)
		/* This driver is compatible for XMUSB350/I350 */
		xmusb350_init();

	if (!xmc_check_charger_dev(chip)) {
		xmc_err("[XMC_PROBE] failed to check CHARGER_DEV\n");
		return -ENODEV;
	}

	if (!xmc_sysfs_init(chip)) {
		xmc_err("[XMC_PROBE] failed to init PSY\n");
		return -ENODEV;
	}

	if (!xmc_check_voter(chip)) {
		xmc_err("[XMC_PROBE] failed to check VOTER\n");
		return -ENODEV;
	}

	if (!xmc_adapter_init(chip)) {
		xmc_err("[XMC_PROBE] failed to init PD_ADAPTER\n");
		return -ENODEV;
	}

	if (!xmc_parse_charge_dt(chip)) {
		xmc_err("[XMC_PROBE] failed to parse charge DTSI\n");
		return -ENODEV;
	}

	if (chip->feature_list.pdm_support) {
		if (!xmc_pdm_init(chip)) {
			xmc_err("[XMC_PROBE] failed to init PDM\n");
			return -ENODEV;
		}
	}

	if (chip->feature_list.qcm_support) {
		if (!xmc_qcm_init(chip)) {
			xmc_err("[XMC_PROBE] failed to init QCM\n");
			return -ENODEV;
		}
	}

	if (!xmc_monitor_init(chip)) {
		xmc_err("[XMC_PROBE] failed to init MONITOR\n");
		return -ENODEV;
	}

	if (!xmc_detection_init(chip)) {
		xmc_err("[XMC_PROBE] failed to init DETECTION\n");
		return -ENODEV;
	}

	schedule_delayed_work(&chip->main_monitor_work, 0);
	xmc_info("[XMC_PROBE] XMC probe success\n");

	return 0;
}

static int xmc_suspend(struct device *dev)
{
	struct charge_chip *chip = dev_get_drvdata(dev);

	chip->resume = false;

	return 0;
}

static int xmc_resume(struct device *dev)
{
	struct charge_chip *chip = dev_get_drvdata(dev);

	chip->resume = true;

	return 0;
}

static const struct dev_pm_ops xmc_pm_ops = {
	.suspend	= xmc_suspend,
	.resume		= xmc_resume,
};

static void xmc_shutdown(struct platform_device *pdev)
{
	struct charge_chip *chip = platform_get_drvdata(pdev);

	chip->resume = false;

	return;
}

static struct platform_driver xiaomi_charge = {
	.driver = {
		.name = "xiaomi_charge",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(xmc_of_match),
		.pm = &xmc_pm_ops,
	},
	.probe = xmc_probe,
	.remove = NULL,
	.shutdown = xmc_shutdown,
	.id_table = xmc_id,
};

module_platform_driver(xiaomi_charge);
MODULE_AUTHOR("chenyichun@xiaomi.com");
MODULE_DESCRIPTION("xiaomi_charge");
MODULE_LICENSE("GPL");
