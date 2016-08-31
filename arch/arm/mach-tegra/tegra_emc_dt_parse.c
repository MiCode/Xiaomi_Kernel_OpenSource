/*
 * arch/arm/mach-tegra/tegra_emc_dt_parse.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/platform_data/tegra_emc.h>

#include "common.h"

#ifdef CONFIG_OF
static struct device_node *tegra_emc_ramcode_devnode(
	struct device_node *np)
{
	struct device_node *iter;
	u32 reg;

	for_each_child_of_node(np, iter) {
		if (of_property_read_u32(iter, "nvidia,ram-code", &reg))
			continue;
		if (reg == tegra_get_bct_strapping())
			return of_node_get(iter);
	}

	return NULL;
}

void *tegra_emc_dt_parse_pdata_comp(const char *emc_mode,
				    const char *comp,
				    void *pdata,
				    struct device_node *tnp,
				    struct platform_device *pdev,
				    int num_tables, int *table_count)
{
	int i = 0, ret = 0;
	struct device_node *iter;
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	struct tegra12_emc_table *tables;
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
	struct tegra11_emc_table *tables;
#endif

	tables = devm_kzalloc(&pdev->dev,
			sizeof(*tables) * num_tables, GFP_KERNEL);

	if (!tables) {
		of_node_put(tnp);
		return tables;
	}

	for_each_child_of_node(tnp, iter) {
		if (of_device_is_compatible(iter, comp)) {
			u32 u;
			const char *source_name;
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
			const char *dvfs_ver;
#endif

			ret = of_property_read_u32(iter, "nvidia,revision", &u);
			if (ret) {
				dev_err(&pdev->dev, "no revision in %s\n",
						iter->full_name);
				continue;
			}
			tables[i].rev = u;

			ret = of_property_read_u32(iter, "clock-frequency", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"no clock-frequency in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].rate = u;

			ret = of_property_read_u32(iter, "nvidia,emc-min-mv",
						   &u);
			if (ret) {
				dev_err(&pdev->dev, "no emc-min-mv in %s\n",
						iter->full_name);
				continue;
			}
			tables[i].emc_min_mv = u;

			ret = of_property_read_string(iter,
					"nvidia,source", &source_name);
			if (ret) {
				dev_err(&pdev->dev, "no source name in %s\n",
						iter->full_name);
				continue;
			}
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
			strncpy(tables[i].src_name, source_name, 16);
#else
			tables[i].src_name = source_name;
#endif
			ret = of_property_read_u32(iter, "nvidia,src-sel-reg",
						   &u);
			if (ret) {
				dev_err(&pdev->dev, "no src-sel-reg in %s\n",
						iter->full_name);
				continue;
			}
			tables[i].src_sel_reg = u;

			ret = of_property_read_u32(iter,
						   "nvidia,burst-regs-num", &u);
			if (ret) {
				dev_err(&pdev->dev, "no burst-regs-num in %s\n",
						iter->full_name);
				continue;
			}
			tables[i].burst_regs_num = u;

			ret = of_property_read_u32(iter,
					"nvidia,burst-up-down-regs-num", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"no burst-up-down-regs-num in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].burst_up_down_regs_num = u;

			ret = of_property_read_u32_array(iter,
					"nvidia,emc-registers",
					tables[i].burst_regs,
					tables[i].burst_regs_num);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-registers property "
					"in %s\n", iter->full_name);
				continue;
			}

			ret = of_property_read_u32_array(iter,
					"nvidia,emc-burst-up-down-regs",
					tables[i].burst_up_down_regs,
					tables[i].burst_up_down_regs_num);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-burst-up-down-regs "
					"property in %s\n",
					iter->full_name);
				continue;
			}

			ret = of_property_read_u32(iter,
					"nvidia,emc-zcal-cnt-long", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-zcal-cnt-long property "
					"in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].emc_zcal_cnt_long = u;

			ret = of_property_read_u32(iter,
					"nvidia,emc-acal-interval", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-acal-interval property "
					"in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].emc_acal_interval = u;

			ret = of_property_read_u32(iter, "nvidia,emc-cfg", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-cfg property in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].emc_cfg = u;

			ret = of_property_read_u32(iter, emc_mode, &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed %s property in %s\n",
					emc_mode, iter->full_name);
				continue;
			}
			tables[i].emc_mode_reset = u;

			ret = of_property_read_u32(iter, "nvidia,emc-mode-1",
						   &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-mode-1 property in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].emc_mode_1 = u;

			ret = of_property_read_u32(iter, "nvidia,emc-mode-2",
						   &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-mode-2 property in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].emc_mode_2 = u;

			ret = of_property_read_u32(iter, "nvidia,emc-mode-4",
						   &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-mode-4 property in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].emc_mode_4 = u;

			ret = of_property_read_u32(iter,
					"nvidia,emc-clock-latency-change", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-clock-latency-change "
					"in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].clock_change_latency = u;
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)

			ret = of_property_read_string(iter,
					"nvidia,dvfs-version", &dvfs_ver);
			if (ret) {
				dev_err(&pdev->dev, "no dvfs version in %s\n",
						iter->full_name);
				continue;
			}
			strncpy(tables[i].table_id, dvfs_ver,
					TEGRA12_MAX_TABLE_ID_LEN);

			ret = of_property_read_u32(iter, "nvidia,gk20a-min-mv",
						   &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed gk20a-min-mv property "
					"in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].gk20a_min_mv = u;

			ret = of_property_read_u32(iter,
					"nvidia,emc-ctt-term_ctrl", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-ctt-term_ctrl property "
					"in %s\n", iter->full_name);
				continue;
			}
			tables[i].emc_ctt_term_ctrl = u;

			ret = of_property_read_u32(iter,
						   "nvidia,emc-cfg-2", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-cfg-2 property in %s\n",
					iter->full_name);
				continue;
			}
			tables[i].emc_cfg_2 = u;

			ret = of_property_read_u32(iter,
						 "nvidia,emc-sel-dpd-ctrl", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-sel-dpd-ctrl property "
					"in %s\n", iter->full_name);
				continue;
			}
			tables[i].emc_sel_dpd_ctrl = u;

			ret = of_property_read_u32(iter,
						  "nvidia,emc-cfg-dig-dll", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-cfg-dig-dll property "
					"in %s\n", iter->full_name);
				continue;
			}
			tables[i].emc_cfg_dig_dll = u;

			ret = of_property_read_u32(iter,
						  "nvidia,emc-bgbias-ctl0", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-bgbias-ctl0 property "
					"in %s\n", iter->full_name);
				continue;
			}
			tables[i].emc_bgbias_ctl0 = u;

			ret = of_property_read_u32(iter,
					"nvidia,emc-auto-cal-config2", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-auto-cal-config2 "
					"property in %s\n", iter->full_name);
				continue;
			}
			tables[i].emc_auto_cal_config2 = u;

			ret = of_property_read_u32(iter,
						  "nvidia,emc-auto-cal-config3",
						  &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-auto-cal-config3 "
					"property in %s\n", iter->full_name);
				continue;
			}
			tables[i].emc_auto_cal_config3 = u;

			ret = of_property_read_u32(iter,
					"nvidia,emc-auto-cal-config", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-auto-cal-config "
					"property in %s\n", iter->full_name);
				continue;
			}
			tables[i].emc_auto_cal_config = u;
#endif

#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
			ret = of_property_read_u32(iter,
						 "nvidia,emc-trimmers-num", &u);
			if (ret) {
				dev_err(&pdev->dev,
					"no emc-trimmers-num in %s\n",
					 iter->full_name);
				continue;
			}
			tables[i].emc_trimmers_num = u;

			ret = of_property_read_u32_array(iter,
					"nvidia,emc-trimmers-0",
					tables[i].emc_trimmers_0,
					tables[i].emc_trimmers_num);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-trimmers-0 property "
					"in %s\n", iter->full_name);
				continue;
			}
			ret = of_property_read_u32_array(iter,
					"nvidia,emc-trimmers-1",
					tables[i].emc_trimmers_1,
					tables[i].emc_trimmers_num);
			if (ret) {
				dev_err(&pdev->dev,
					"malformed emc-trimmers-1 property "
					"in %s\n", iter->full_name);
				continue;
			}
#endif
			i++;
		}
	}
	*table_count = i;
	return tables;
}

void *tegra_emc_dt_parse_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *tnp, *iter;
	int num_tables, table_count;
	u32 tegra_bct_strapping;
#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	struct tegra12_emc_pdata *pdata = NULL;
	const char *comp = "nvidia,tegra12-emc-table";
	const char *comp_derated = "nvidia,tegra12-emc-table-derated";
	const char *emc_mode = "nvidia,emc-mode-0";
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
	struct tegra11_emc_pdata *pdata = NULL;
	const char *comp = "nvidia,tegra11-emc-table";
	const char *emc_mode = "nvidia,emc-mode-reset";
#endif

	if (!np) {
		dev_err(&pdev->dev,
			"Unable to find memory-controller node\n");
		return NULL;
	}

	tegra_bct_strapping = tegra_get_bct_strapping();

	if (of_find_property(np, "nvidia,use-ram-code", NULL)) {
		tnp = tegra_emc_ramcode_devnode(np);

		if (!tnp) {
			dev_warn(&pdev->dev,
				"can't find emc table for ram-code 0x%02x\n",
				tegra_bct_strapping);
			return NULL;
		}
	} else
		tnp = of_node_get(np);

	num_tables = 0;
	for_each_child_of_node(tnp, iter) {
		if (of_device_is_compatible(iter, comp))
			num_tables++;
	}

	if (!num_tables) {
		pdata = NULL;
		goto out;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	pdata->tables = (struct tegra12_emc_table *)
			tegra_emc_dt_parse_pdata_comp(emc_mode, comp,
			pdata, tnp, pdev, num_tables, &table_count);
#elif defined(CONFIG_ARCH_TEGRA_11x_SOC)
	pdata->tables = (struct tegra11_emc_table *)
			tegra_emc_dt_parse_pdata_comp(emc_mode, comp,
			pdata, tnp, pdev, num_tables, &table_count);
#endif
	pdata->num_tables = table_count;

#if defined(CONFIG_ARCH_TEGRA_12x_SOC)
	/* populate the derated tables */
	num_tables = 0;
	for_each_child_of_node(tnp, iter) {
		if (of_device_is_compatible(iter, comp_derated))
			num_tables++;
	}

	if (!num_tables) {
		pdata->tables_derated = NULL;
		goto out;
	}

	pdata->tables_derated = (struct tegra12_emc_table *)
				tegra_emc_dt_parse_pdata_comp(emc_mode,
				comp_derated, pdata, tnp, pdev, num_tables,
				&table_count);
#endif

out:
	of_node_put(tnp);
	return pdata;
}
#else
void *tegra_emc_dt_parse_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif
