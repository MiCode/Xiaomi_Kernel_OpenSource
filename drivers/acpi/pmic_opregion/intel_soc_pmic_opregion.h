#ifndef __INTEL_SOC_PMIC_OPREGION_H__
#define __INTEL_SOC_PMIC_OPREGION_H__

struct pmic_pwr_reg {
	int reg;        /* corresponding PMIC register */
	int bit;        /* control bit for power */
};

struct pmic_pwr_table {
	int address;    /* operation region address */
	struct pmic_pwr_reg pwr_reg;
};

struct pmic_dptf_table {
	int address;    /* operation region address */
	int reg;        /* corresponding thermal register */
};

struct intel_soc_pmic_opregion_data {
	int (*get_power)(struct pmic_pwr_reg *preg, u64 *value);
	int (*update_power)(struct pmic_pwr_reg *preg, bool on);
	int (*get_raw_temp)(int reg);
	int (*update_aux)(int reg, int raw_temp);
	int (*get_policy)(int reg, u64 *value);
	int (*update_policy)(int reg, int enable);
	struct pmic_pwr_table *pwr_table;
	int pwr_table_count;
	struct pmic_dptf_table *dptf_table;
	int dptf_table_count;
};

int intel_soc_pmic_install_opregion_handler(struct device *dev,
		acpi_handle handle,
		struct intel_soc_pmic_opregion_data *d);
void intel_soc_pmic_remove_opregion_handler(acpi_handle handle);
#endif
