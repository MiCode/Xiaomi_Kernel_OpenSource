// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Spreadtrum Communications Inc.

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/platform_data/sprd_ump96xx_tsensor.h>

#define UMP96XX_TSEN_OSC_TEMP(h, l) \
	(((((h & GENMASK(5, 3)) >> 3 << 16) | (l & GENMASK(15, 0))) << 4) & 0x7FFFFF)
#define UMP96XX_TSEN_TSEN_TEMP(h, l) \
	(((((h & GENMASK(2, 0)) << 16) | (l & GENMASK(15, 0))) << 4) & 0x7FFFFF)

#define UMP96XX_TSEN_INDEX_MASK		GENMASK(7, 0)
#define UMP96XX_TSEN_INDEX_SHIFT	12
#define UMP96XX_TSEN_FRAC_MASK		GENMASK(11, 0)
#define ABNORMAL_TEMP			97697

/* Temperature query table according to integral index */
/* crystal name:TSX
 * manu name:TXC
 */
static int v2t_table_0[256] = {
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119, 298119,
	298119, 298119, 298119, 298119, 280421, 264933, 251238, 238928, 227748, 217492, 208038,
	199268, 191078, 183383, 176104, 169240, 162710, 156500, 150570, 144893, 139428, 134191,
	129138, 124253, 119525, 114943, 110500, 106187, 101990, 97905, 93919, 90026, 86224, 82506,
	78860, 75286, 71777, 68330, 64940, 61606, 58337, 55113, 51932, 48790, 45696, 42642, 39620,
	36627, 33675, 30750, 27848, 24974, 22128, 19300, 16490, 13707, 10937, 8180, 5444, 2717, 0,
	-2702, -5398, -8088, -10768, -13446, -16120, -18791, -21462, -24132, -26805, -29481, -32160,
	-34847, -37539, -40241, -42956, -45680, -48419, -51175, -53944, -56734, -59548, -62379,
	-65234, -68124, -71039, -73979, -76964, -79985, -83041, -86136, -89290, -92493, -95747,
	-99057, -102428, -105886, -109418, -113031, -116733, -120533, -124438,  -128458, -132609,
	-136903, -141357, -145991, -150823, -155881, -161216, -166830, -172781, -179140, -185969,
	-193354, -201441, -210423, -220528, -232181, -245984, -263191, -263191, -263191
};

/* crystal name: TSX
 * manu name:
 */
static int v2t_table_1[256] = {
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768, 297768,
	297768, 297768, 297768, 297768, 280022, 264541, 250827, 238540, 227437, 217248, 207794,
	199075, 190918, 183219, 175974, 169165, 162632, 156430, 150512, 144824, 139369, 134151,
	129116, 124240, 119517, 114923, 110488, 106189, 101977, 97891, 93912, 90023, 86216, 82497,
	78868, 75287, 71782, 68342, 64943, 61612, 58346, 55122, 51944, 48810, 45706, 42657, 39637,
	36640, 33683, 30762, 27866, 24991, 22141, 19308, 16495, 13716, 10947, 8187, 5448, 2719, 0,
	-2676, -5364, -8063, -10745, -13423, -16096, -18774, -21457, -24149, -26796, -29446, -32123,
	-34809, -37502, -40214, -42926, -45643, -48372, -51122, -53890, -56690, -59510, -62348,
	-65214, -68106, -71023, -73966, -76944, -79974, -83043, -86145, -89309, -92532, -95805,
	-99133, -102537, -106000, -109565, -113208, -116941, -120778, -124722, -128784, -132989,
	-137339, -141848, -146539, -151424, -156567, -161964, -167642, -173677, -180078, -187046,
	-194557, -202751, -211799, -222049, -233806, -247837, -265250, -265250, -265250
};

static bool cali_mode, modem_flag, gnss_flag;
static DEFINE_MUTEX(tsen_mutex);
static DEFINE_MUTEX(modem_gnss_mtx);

char *modem_tsen = "tsen_temp";

struct tsen_manager {
	struct device *dev;
	struct regmap *regmap;
	u32 base;
	int v2t_table_id;
} tsen_mager;

struct ump96xx_tsen {
	struct device *dev;
	struct thermal_zone_device *tz_dev;
	struct regmap *regmap;
	u32 base;
	int id;
	int v2t_table_id;
};

static int ump96xx_tsensor_config(struct regmap *regmap, u32 base)
{
	int ret;

	ret = regmap_update_bits(regmap, UMP96XX_XTL_WAIT_CTRL0,
				 UMP96XX_XTL_EN, UMP96XX_XTL_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL0,
				 UMP96XX_TSEN_CLK_SRC_SEL, UMP96XX_TSEN_CLK_SRC_SEL);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_26M_EN, UMP96XX_TSEN_26M_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_ADCLDO_EN, UMP96XX_TSEN_ADCLDO_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_SDADC_EN, UMP96XX_TSEN_SDADC_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_UGBUF_EN, UMP96XX_TSEN_UGBUF_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_EN, UMP96XX_TSEN_EN);

	return ret;
}

static int ump96xx_tsensor_osc_rawdata_read(struct regmap *regmap, u32 base, signed long *rawdata)
{
	int ret, th, tl;

	ret = ump96xx_tsensor_config(regmap, base);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
				 UMP96XX_TSEN_SEL_EN, UMP96XX_TSEN_SEL_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_SEL_CH, UMP96XX_TSEN_SEL_CH);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * trigger a data sample and wait data ready need wait 21ms
	 */
	msleep(21);

	ret = regmap_read(regmap, base + UMP96XX_TSEN_CTRL5, &tl);
	if (ret)
		return ret;

	ret = regmap_read(regmap, base + UMP96XX_TSEN_CTRL6, &th);
	if (ret)
		return ret;

	*rawdata = UMP96XX_TSEN_OSC_TEMP(th, tl);

	return ret;
}

static int ump96xx_tsensor_osc_temp_read(struct ump96xx_tsen *tsen, int *temp)
{
	int ret;
	signed long rawdata;

	mutex_lock(&tsen_mutex);
	ret = ump96xx_tsensor_osc_rawdata_read(tsen->regmap, tsen->base, &rawdata);
	mutex_unlock(&tsen_mutex);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * tsensor osc temp = 3641500 - (4856 * rawdata) / 1000
	 */
	*temp = (int)(3641500 - ((4856 * rawdata) / 1000));

	return ret;
}

static int ump96xx_tsensor_get_tsx_rawdata(struct regmap *regmap, u32 base, int *rawdata)
{
	int ret, th, tl;

	ret = regmap_read(regmap, base + UMP96XX_TSEN_CTRL4, &tl);
	if (ret)
		return ret;

	ret = regmap_read(regmap, base + UMP96XX_TSEN_CTRL6, &th);
	if (ret)
		return ret;

	*rawdata = UMP96XX_TSEN_TSEN_TEMP(th, tl);

	return ret;
}

static int ump96xx_tsensor_out_rawdata_read(struct regmap *regmap, u32 base, int *rawdata)
{
	int ret;

	ret = ump96xx_tsensor_config(regmap, base);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
				 UMP96XX_TSEN_SEL_EN, UMP96XX_TSEN_SEL_EN);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3, UMP96XX_TSEN_SEL_CH, 0x0);
	if (ret)
		return ret;

	/*
	 * According to the requirements of design document,
	 * trigger a data sample and wait data ready need wait 21ms
	 */
	msleep(21);

	ret = ump96xx_tsensor_get_tsx_rawdata(regmap, base, rawdata);

	return ret;
}

static void ump96xx_tsensor_cal_tsx_temp(int v2t_table[], int len, int rawdata, int *temp)
{
	int index, frac, t;

	index = (rawdata >> UMP96XX_TSEN_INDEX_SHIFT) & UMP96XX_TSEN_INDEX_MASK;
	frac = rawdata & UMP96XX_TSEN_FRAC_MASK;

	/*
	 * According to the requirements of design document,get the index accrding
	 * to the integral result, and query the current temperature value according to
	 * the index.
	 * t = (v2t_table[index] * (0x1000-frac) + v2t_table[index+1] * frac + 0x800) >> 12;
	 */
	if (index != len / 4 - 1)
		t = (v2t_table[index] * (0x1000 - frac) + v2t_table[index + 1] *
			frac + 0x800) >> 12;
	else
		t = v2t_table[index];

	/*
	 * According to the requirements of design document,
	 * tsensor out temp = ((t * 1000) >> 12) + 25000
	 */
	*temp = ((t * 1000) >> 12) + 25000;
}

static int ump96xx_tsensor_out_temp_read(struct ump96xx_tsen *tsen, int *temp,
					 int v2t_table[], int len)
{
	int ret, rawdata;

	mutex_lock(&tsen_mutex);
	ret = ump96xx_tsensor_out_rawdata_read(tsen->regmap, tsen->base, &rawdata);
	mutex_unlock(&tsen_mutex);
	if (ret)
		return ret;

	ump96xx_tsensor_cal_tsx_temp(v2t_table, len, rawdata, temp);
	return ret;
}

static int ump96xx_tsensor_disable(struct regmap *regmap, u32 base)
{
	int ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL0,
				 UMP96XX_TSEN_CLK_SRC_SEL, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_EN | UMP96XX_TSEN_SEL_CH, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
				 UMP96XX_TSEN_SEL_EN, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL1,
				 UMP96XX_TSEN_SDADC_EN, 0);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				 UMP96XX_TSEN_UGBUF_EN, 0);
	if (ret)
		return ret;

	return regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
				  UMP96XX_TSEN_ADCLDO_EN, 0);
}

static int get_boot_mode(void)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "sprdboot.mode=cali"))
		cali_mode = true;
	else if (strstr(cmd_line, "androidboot.mode=cali"))
		cali_mode = true;
	else
		cali_mode = false;

	return 0;
}

static int ump96xx_tsensor_get_temp(void *data, int *temp)
{
	struct ump96xx_tsen *tsen = data;
	int ret = -EOPNOTSUPP;
	int len = 0;

	if (tsen->id == 0) {
		ret = ump96xx_tsensor_osc_temp_read(tsen, temp);
		return ret;
	}
	if (tsen->v2t_table_id) {
		len = sizeof(v2t_table_1);
		ret = ump96xx_tsensor_out_temp_read(tsen, temp, v2t_table_1, len);
	} else {
		len = sizeof(v2t_table_0);
		ret = ump96xx_tsensor_out_temp_read(tsen, temp, v2t_table_0, len);
	}

	return ret;
}

static const struct thermal_zone_of_device_ops tsensor_thermal_ops = {
	.get_temp = ump96xx_tsensor_get_temp,
};

static int tsen_enable(struct regmap *regmap, u32 base)
{
	int ret = 0;

	if (gnss_flag)
		return ret;
	ret = ump96xx_tsensor_config(regmap, base);
	if (ret)
		return ret;

	ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
				 UMP96XX_TSEN_SEL_EN, 0x0);

	return ret;
}

static int tsen_disable(struct regmap *regmap, u32 base)
{
	int ret = 0;

	if (gnss_flag)
		return ret;
	ret = ump96xx_tsensor_disable(regmap, base);
	return ret;
}

#if IS_ENABLED(CONFIG_SPRD_UMP96XX_TSENSOR)
int gnss_tsen_control(struct regmap *regmap, u32 base, bool en)
{
	int ret = 0;

	mutex_lock(&modem_gnss_mtx);
	if (en && !gnss_flag) {
		ret = tsen_enable(regmap, base);
		if (!ret)
			gnss_flag = true;
	} else if (!en && gnss_flag) {
		/*whether it close success or not, flag need to be false,
		 * then modem can close
		 */
		gnss_flag = false;
		ret = tsen_disable(regmap, base);
	}
	mutex_unlock(&modem_gnss_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(gnss_tsen_control);
#endif

static int modem_tsen_control(struct regmap *regmap, u32 base, bool en)
{
	int ret = 0;

	if (en && !modem_flag) {
		mutex_lock(&modem_gnss_mtx);
		ret = tsen_enable(regmap, base);
		if (!ret)
			modem_flag = true;
	} else if (!en && modem_flag) {
		ret = tsen_disable(regmap, base);
		modem_flag = false;
		mutex_unlock(&modem_gnss_mtx);
	}
	return ret;
}

static void modem_tsen_read(struct regmap *regmap, u32 base, int *temp,
			   int v2t_table[], int len)
{
	int ret, rawdata, i = 0;

	mutex_lock(&tsen_mutex);
	if (!gnss_flag) {
		ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL6,
					 UMP96XX_TSEN_SEL_EN, UMP96XX_TSEN_SEL_EN);
		if (ret) {
			mutex_unlock(&tsen_mutex);
			return;
		}
		ret = regmap_update_bits(regmap, base + UMP96XX_TSEN_CTRL3,
					 UMP96XX_TSEN_SEL_CH, 0x0);
		if (ret) {
			mutex_unlock(&tsen_mutex);
			return;
		}
	}

	do {
		msleep(20);

		ret = ump96xx_tsensor_get_tsx_rawdata(regmap, base, &rawdata);
		if (ret) {
			mutex_unlock(&tsen_mutex);
			return;
		}

		ump96xx_tsensor_cal_tsx_temp(v2t_table, len, rawdata, temp);

		i++;
	} while ((*temp >= ABNORMAL_TEMP) && (i < 3));

	mutex_unlock(&tsen_mutex);
}

static ssize_t modem_tsen_show(struct device_driver *dev, char *buf)
{
	int ret,  tsen_temp = ABNORMAL_TEMP;
	int len;

	ret = modem_tsen_control(tsen_mager.regmap, tsen_mager.base, true);
	if (ret)
		return ret;

	if (tsen_mager.v2t_table_id == 0) {
		len = sizeof(v2t_table_0);
		modem_tsen_read(tsen_mager.regmap, tsen_mager.base,
				&tsen_temp, v2t_table_0, len);
	} else if (tsen_mager.v2t_table_id == 1) {
		len = sizeof(v2t_table_1);
		modem_tsen_read(tsen_mager.regmap, tsen_mager.base,
				&tsen_temp, v2t_table_1, len);
	}

	ret = modem_tsen_control(tsen_mager.regmap, tsen_mager.base, false);
	if (!ret)
		pr_info("close success\n");
	len = sizeof(buf);
	return snprintf(buf, len, "%d\n", tsen_temp);
}

static DRIVER_ATTR_RO(modem_tsen);

static int ump96xx_tsen_probe(struct platform_device *pdev);

static const struct of_device_id ump96xx_tsen_of_match[] = {
	{ .compatible = "sprd,ump96xx-tsensor",},
	{ }
};

static struct platform_driver ump96xx_tsen_driver = {
	.probe = ump96xx_tsen_probe,
	.driver = {
		.name = "ump96xx-tsensor",
		.of_match_table = ump96xx_tsen_of_match,
	},
};

static int ump96xx_tsen_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *sen_child;
	struct ump96xx_tsen *tsen;
	struct regmap *regmap;
	u32 base;
	int ret = 0;
	int tsx_table_id = 0;

	ret = get_boot_mode();
	if (ret) {
		pr_err("boot_mode can't parse bootargs property\n");
		return ret;
	}

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "failed to get regmap\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "reg", &base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get base address\n");
		return ret;
	}

	ret = of_property_read_u32(np, "v2t_table_id", &tsx_table_id);
	if (ret) {
		dev_info(&pdev->dev, "cannot get v2t table id, use default\n");
		tsx_table_id = 1;
		ret = 0;
	}

	tsen_mager.dev = &pdev->dev;
	tsen_mager.regmap = regmap;
	tsen_mager.base = base;
	tsen_mager.v2t_table_id = tsx_table_id;

	if (cali_mode) {
		for_each_child_of_node(np, sen_child) {
			tsen = devm_kzalloc(&pdev->dev, sizeof(*tsen), GFP_KERNEL);
			if (!tsen)
				return -ENOMEM;

			tsen->regmap = regmap;
			tsen->base = base;
			tsen->v2t_table_id = tsx_table_id;

			ret = of_property_read_u32(sen_child, "reg", &tsen->id);
			if (ret) {
				dev_err(&pdev->dev, "get sensor reg failed");
				return ret;
			}
			tsen->tz_dev = thermal_zone_of_sensor_register(&pdev->dev, tsen->id,
								     tsen, &tsensor_thermal_ops);
			if (IS_ERR(tsen->tz_dev)) {
				ret = PTR_ERR(tsen->tz_dev);
				dev_err(&pdev->dev, "Thermal zone register failed\n");
				return ret;
			}
		}
		return ump96xx_tsensor_disable(regmap, base);
	}

	ret = driver_create_file(&ump96xx_tsen_driver.driver, &driver_attr_modem_tsen);
	if (ret < 0)
		dev_err(&pdev->dev, "can not create sysfs file\n");

	return ret;
}

static int __init ump96xx_tsen_init(void)
{
	return platform_driver_register(&ump96xx_tsen_driver);
}
module_init(ump96xx_tsen_init);

static void __exit ump96xx_tsen_exit(void)
{
	driver_remove_file(&ump96xx_tsen_driver.driver, &driver_attr_modem_tsen);
	platform_driver_unregister(&ump96xx_tsen_driver);
}

module_exit(ump96xx_tsen_exit);

MODULE_DESCRIPTION("Spreadtrum UMP96xx tsensor driver");
MODULE_LICENSE("GPL");
