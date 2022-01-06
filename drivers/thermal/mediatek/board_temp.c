// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/thermal.h>


#define get_tia_rc_sel(val, offset, mask) (((val) & (mask)) >> (offset))
#define is_adc_data_valid(val, bit)       (((val) & BIT(bit)) != 0)
#define get_adc_data(val, bit)            ((val) & GENMASK(bit, 0))
#define adc_data_to_v_in(val)             (1900 - (((val) * 1000) >> 14))

/**
 * struct tia_data - parameters to parse the data from TIA
 * @valid_bit: valid bit in TIA DATA register
 * @rc_offset: RC bit offset in TIA DATA register
 * @rc_mask: bitmask for RC bit
 * @rc_sel_to_value: function to get default pullup resistance value
 */
struct tia_data {
	unsigned int valid_bit;
	unsigned int rc_offset;
	unsigned int rc_mask;
	unsigned int (*rc_sel_to_value)(unsigned int sel);
};

/**
 * struct pmic_auxadc_data - parameters and callback functions for NTC
 *                           resistance value calculation
 * @is_initialized: indicate the auxadc data was been initialized or not
 * @default_pullup_v: voltage of internal pullup resistance
 * @pullup_v: pullup voltage of each pullup resistance type. It should
 *            equal to default_pullup_v if no extra input buffer for SDM.
 * @pullup_r: pullup resistance value
 * @num_of_pullup_r_type: number of pullup resistance type
 * @pullup_r_calibration: calculate the parameters for actual NTC resitance
 *                        value calculation. Set to NULL if no extra input
 *                        buffer for SDM.
 * @tia_param: parameters to parse auxadc value from TIA DATA register
 */
struct pmic_auxadc_data {
	bool is_initialized;
	unsigned int default_pullup_v;
	unsigned int *pullup_v;
	unsigned int *pullup_r;
	unsigned int num_of_pullup_r_type;
	int (*pullup_r_calibration)(struct device *dev,
				struct pmic_auxadc_data *adc_data);
	unsigned long long (*adc2volt)(unsigned int adc_raw);
	struct tia_data *tia_param;
	bool is_print_tia_cg;
};

struct board_ntc_info {
	struct device *dev;
	int *lookup_table;
	int lookup_table_num;
	void __iomem *data_reg;
	void __iomem *dbg_reg;
	void __iomem *en_reg;
	struct pmic_auxadc_data *adc_data;
};

unsigned int tia2_rc_sel_to_value(unsigned int sel)
{
	unsigned int resistance;

	switch (sel) {
	case 1:
		resistance = 30000; /* 30K */
		break;
	case 2:
		resistance = 400000; /* 400K */
		break;
	case 0:
	default:
		resistance = 100000; /* 100K */
		break;
	}

	return resistance;
}

unsigned long long mt6685_adc2volt(unsigned int adc_raw)
{
	return ((unsigned long long)adc_raw * 184000) >> 15;
}

static struct tia_data tia2_data = {
	.valid_bit = 15,
	.rc_offset = 16,
	.rc_mask = GENMASK(17, 16),
	.rc_sel_to_value = tia2_rc_sel_to_value,
};

static struct pmic_auxadc_data mt6685_pmic_auxadc_data = {
	.default_pullup_v = 184000,
	.num_of_pullup_r_type = 3,
	.pullup_r_calibration = NULL,
	.adc2volt = mt6685_adc2volt,
	.tia_param = &tia2_data,
	.is_print_tia_cg = false,
};

static struct pmic_auxadc_data mt6685_pmic_auxadc_data_debug = {
	.default_pullup_v = 184000,
	.num_of_pullup_r_type = 3,
	.pullup_r_calibration = NULL,
	.adc2volt = mt6685_adc2volt,
	.tia_param = &tia2_data,
	.is_print_tia_cg = true,
};

static const struct of_device_id board_ntc_of_match[] = {
	{
		.compatible = "mediatek,mt6983-board-ntc",
		.data = (void *)&mt6685_pmic_auxadc_data,
	},
	{
		.compatible = "mediatek,mt6879-board-ntc",
		.data = (void *)&mt6685_pmic_auxadc_data_debug,
	},
	{
		.compatible = "mediatek,mt6895-board-ntc",
		.data = (void *)&mt6685_pmic_auxadc_data,
	},
	{
		.compatible = "mediatek,mt6855-board-ntc",
		.data = (void *)&mt6685_pmic_auxadc_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, board_ntc_of_match);

static int board_ntc_r_to_temp(struct board_ntc_info *ntc_info,
						int val)
{
	int temp, temp_hi, temp_lo, r_hi, r_lo;
	int i;

	for (i = 0; i < ntc_info->lookup_table_num; i++) {
		if (val >= ntc_info->lookup_table[2 * i + 1])
			break;
	}

	if (i == 0) {
		temp = ntc_info->lookup_table[0];
	} else if (i >= ntc_info->lookup_table_num) {
		temp = ntc_info->lookup_table[2 *
			(ntc_info->lookup_table_num - 1)];
	} else {
		r_hi = ntc_info->lookup_table[2 * i - 1];
		r_lo = ntc_info->lookup_table[2 * i + 1];

		temp_hi = ntc_info->lookup_table[2 * i - 2];
		temp_lo = ntc_info->lookup_table[2 * i];

		temp = temp_hi + mult_frac(temp_lo - temp_hi, val - r_hi,
					   r_lo - r_hi);
	}

	return temp;
}

static unsigned int calculate_r_ntc(unsigned long long v_in,
				unsigned int pullup_r, unsigned int pullup_v)
{
	unsigned int r_ntc;

	if (v_in >= pullup_v)
		return 0;

	r_ntc = (unsigned int)(v_in * pullup_r / (pullup_v - v_in));

	return r_ntc;
}


static void print_tia_reg(struct device *dev)
{
	void __iomem *addr = NULL;
	unsigned int clk_cg;
	unsigned int pmif_cg;

	/* clk cg */
	addr = ioremap(0x1C00C004, 0x4);
	if (!addr) {
		dev_err(dev, "%s clk cg ioremap failed\n", __func__);
		return;
	}

	clk_cg = readl(addr);
	iounmap(addr);

	/* PMIF cg */
	addr = ioremap(0x0d0a0088, 0x4);
	if (!addr) {
		dev_err(dev, "%s pmif ioremap failed\n", __func__);
		return;
	}

	pmif_cg = readl(addr);
	iounmap(addr);
	dev_notice(dev, "[board_temp debug]clk_cg:0x%x, pmif_cg:0x%x\n",
		clk_cg, pmif_cg);

}

static int board_ntc_get_temp(void *data, int *temp)
{
	struct board_ntc_info *ntc_info = (struct board_ntc_info *)data;
	struct pmic_auxadc_data *adc_data = ntc_info->adc_data;
	struct tia_data *tia_param = ntc_info->adc_data->tia_param;
	unsigned int val, r_type, r_ntc, dbg_reg, en_reg;
	unsigned long long v_in;

	if (adc_data->is_print_tia_cg == true)
		print_tia_reg(ntc_info->dev);

	val = readl(ntc_info->data_reg);
	r_type = get_tia_rc_sel(val, tia_param->rc_offset, tia_param->rc_mask);
	if (r_type >= adc_data->num_of_pullup_r_type) {
		dev_err(ntc_info->dev, "Invalid r_type = %d\n", r_type);
		return -EINVAL;
	}

	if (!is_adc_data_valid(val, tia_param->valid_bit)) {
		if (ntc_info->dbg_reg && ntc_info->en_reg) {
			dbg_reg = readl(ntc_info->dbg_reg);
			en_reg = readl(ntc_info->en_reg);
			dev_err(ntc_info->dev, "TIA data invalid, 0x%x, dbg=0x%x, en=0x%x\n",
				val, dbg_reg, en_reg);
		} else
			dev_err(ntc_info->dev, "TIA data invalid, 0x%x\n", val);

		return -EAGAIN;
	}

	if (!ntc_info->adc_data->adc2volt) {
		dev_err(ntc_info->dev, "adc2volt should exist\n");
		return -ENODEV;
	}

	v_in = ntc_info->adc_data->adc2volt(get_adc_data(val, tia_param->valid_bit - 1));
	r_ntc = calculate_r_ntc(v_in, adc_data->pullup_r[r_type],
				adc_data->pullup_v[r_type]);

	if (!r_ntc) {
		dev_err(ntc_info->dev,
			"r_ntc is 0! v_in/pullup_r/pullup_v=%d/%d/%d\n",
			v_in, adc_data->pullup_r[r_type],
			adc_data->pullup_v[r_type]);
		*temp = THERMAL_TEMP_INVALID;
	} else {
		*temp = board_ntc_r_to_temp(ntc_info, r_ntc);
	}

	dev_dbg_ratelimited(ntc_info->dev, "val=0x%x, v_in/r_type/r_ntc/t=%d/%d/%d/%d\n",
		val, v_in, r_type, r_ntc, *temp);

	return 0;
}

static const struct thermal_zone_of_device_ops board_ntc_ops = {
	.get_temp = board_ntc_get_temp,
};

static int board_ntc_init_auxadc_data(struct device *dev,
				struct pmic_auxadc_data *adc_data)
{
	int ret = 0, size, i;
	int num = adc_data->num_of_pullup_r_type;

	if (num <= 0)
		return -EINVAL;

	size = sizeof(*adc_data->pullup_v) * num * 2;
	adc_data->pullup_v = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!adc_data->pullup_v)
		return -ENOMEM;

	adc_data->pullup_r = (unsigned int *)(adc_data->pullup_v + num);
	if (adc_data->pullup_r_calibration) {
		ret = adc_data->pullup_r_calibration(dev, adc_data);
	} else {
		for (i = 0; i < num; i++) {
			adc_data->pullup_r[i] =
				adc_data->tia_param->rc_sel_to_value(i);
			adc_data->pullup_v[i] = adc_data->default_pullup_v;

			dev_info(dev, "%d: default pullup_r=%d, pullup_v=%d\n",
				i, adc_data->pullup_r[i],
				adc_data->pullup_v[i]);
		}
	}

	return ret;
}

static int board_ntc_parse_lookup_table(struct device *dev,
					struct board_ntc_info *ntc_info)
{
	struct device_node *np = dev->of_node;
	int num, ret;

	num = of_property_count_elems_of_size(np, "temperature-lookup-table",
						sizeof(unsigned int));
	if (num < 0) {
		dev_err(dev, "lookup table is not found\n");
		return num;
	}

	if (num % 2) {
		dev_err(dev, "temp vs ADC value in table are unpaired\n");
		return -EINVAL;
	}

	ntc_info->lookup_table = devm_kcalloc(dev, num,
					 sizeof(*ntc_info->lookup_table),
					 GFP_KERNEL);
	if (!ntc_info->lookup_table)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "temperature-lookup-table",
					(unsigned int *)ntc_info->lookup_table,
					num);
	if (ret < 0) {
		dev_err(dev, "Failed to read temperature lookup table: %d\n",
			ret);
		return ret;
	}

	ntc_info->lookup_table_num = num / 2;

	return 0;
}

static int board_ntc_probe(struct platform_device *pdev)
{
	struct board_ntc_info *ntc_info;
	struct resource *res;
	void __iomem *tia_reg;
	struct thermal_zone_device *tz_dev;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "Only DT based supported\n");
		return -ENODEV;
	}

	ntc_info = devm_kzalloc(&pdev->dev, sizeof(*ntc_info), GFP_KERNEL);
	if (!ntc_info)
		return -ENOMEM;

	ret = board_ntc_parse_lookup_table(&pdev->dev, ntc_info);
	if (ret < 0)
		return ret;

	ntc_info->dev = &pdev->dev;
	ntc_info->adc_data = (struct pmic_auxadc_data *)
		of_device_get_match_data(&pdev->dev);
	if (!ntc_info->adc_data->is_initialized) {
		ret = board_ntc_init_auxadc_data(&pdev->dev,
						ntc_info->adc_data);
		if (ret)
			return ret;

		ntc_info->adc_data->is_initialized = true;
	}

	platform_set_drvdata(pdev, ntc_info);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tia_reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tia_reg))
		return PTR_ERR(tia_reg);

	ntc_info->data_reg = tia_reg;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	tia_reg = devm_ioremap_resource(&pdev->dev, res);
	if (!IS_ERR(tia_reg))
		ntc_info->en_reg = tia_reg;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	tia_reg = devm_ioremap_resource(&pdev->dev, res);
	if (!IS_ERR(tia_reg))
		ntc_info->dbg_reg = tia_reg;

	tz_dev = devm_thermal_zone_of_sensor_register(
			&pdev->dev, 0, ntc_info, &board_ntc_ops);
	if (IS_ERR(tz_dev)) {
		ret = PTR_ERR(tz_dev);
		dev_err(&pdev->dev, "Thermal zone sensor register fail:%d\n",
			ret);
		return ret;
	}

	return 0;
}

static struct platform_driver board_ntc_driver = {
	.probe = board_ntc_probe,
	.driver = {
		.name = "mtk-board-ntc",
		.of_match_table = board_ntc_of_match,
	},
};

module_platform_driver(board_ntc_driver);

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek on board NTC driver via TIA HW");
MODULE_LICENSE("GPL v2");
