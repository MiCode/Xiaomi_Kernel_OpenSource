/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/sysedp.h>
#include <linux/edpdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include "sysedp_internal.h"

#define UPDATE_INTERVAL	60000

static struct sysedp_batmon_calc_platform_data *pdata;
static struct delayed_work work;
static struct power_supply *psy;
static int esr;
int (*get_ocv)(unsigned int capacity);

static int psy_get_property(enum power_supply_property psp, int *val)
{
	union power_supply_propval pv;

	if (psy->get_property(psy, psp, &pv))
		return -EFAULT;
	if (val)
		*val = pv.intval;
	return 0;
}

static int psy_ocv_from_chip(unsigned int capacity)
{
	int val;
	if (psy_get_property(POWER_SUPPLY_PROP_VOLTAGE_OCV, &val))
		return pdata->vsys_min;
	return val;
}

static int psy_capacity(void)
{
	int val;
	if (psy_get_property(POWER_SUPPLY_PROP_CAPACITY, &val))
		return 0;
	return val;
}

static int psy_temp(void)
{
	int val;

	if (psy_get_property(POWER_SUPPLY_PROP_TEMP, &val))
		return 25;
	return val;
}

/* Given two points (x1, y1) and (x2, y2), find the y coord of x */
static int interpolate(int x, int x1, int y1, int x2, int y2)
{
	if (x1 == x2)
		return y1;
	return (y2 * (x - x1) - y1 * (x - x2)) / (x2 - x1);
}

/* bi-linearly interpolate from table */
static int bilinear_interpolate(int *array, int *xaxis, int *yaxis,
				int x_size, int y_size, int x, int y)
{
	s64 r;
	int d;
	int yi1, yi2;
	int xi1, xi2;
	int q11, q12, q21, q22;
	int x1, x2, y1, y2;

	if (x_size <= 0 || y_size <= 0)
		return 0;

	if (x_size == 1 && y_size == 1)
		return array[0];

	/* Given that x is within xaxis range, find x1 and x2 that
	 * satisfy x1 >= x >= x2 */
	for (xi2 = 1; xi2 < x_size - 1; xi2++)
		if (x > xaxis[xi2])
			break;
	xi1 = xi2 - 1;
	xi2 = x_size > 1 ? xi2 : 0;
	x1 = xaxis[xi1];
	x2 = xaxis[xi2];

	for (yi2 = 1; yi2 < y_size - 1; yi2++)
		if (y > yaxis[yi2])
			break;
	yi1 = yi2 - 1;
	yi2 = y_size > 1 ? yi2 : 0;
	y1 = yaxis[yi1];
	y2 = yaxis[yi2];

	if (x_size == 1)
		return interpolate(y, y1, array[yi1], y2, array[yi2]);
	if (y_size == 1)
		return interpolate(x, x1, array[xi1], x2, array[xi2]);

	q11 = array[xi1 + yi1 * x_size];
	q12 = array[xi1 + yi2 * x_size];
	q21 = array[xi2 + yi1 * x_size];
	q22 = array[xi2 + yi2 * x_size];

	r = (s64)q11 * (x2 - x) * (y2 - y);
	r += (s64)q21 * (x - x1) * (y2 - y);
	r += (s64)q12 * (x2 - x) * (y - y1);
	r += (s64)q22 * (x - x1) * (y - y1);
	d = ((x2-x1)*(y2-y1));
	r = d ? div64_s64(r, d) : 0;

	return r;
}

static int psy_ocv_from_lut(unsigned int capacity)
{
	struct sysedp_batmon_ocv_lut *p;
	struct sysedp_batmon_ocv_lut *q;

	p = pdata->ocv_lut;

	while (p->capacity > capacity)
		p++;

	if (p == pdata->ocv_lut)
		return p->ocv;

	q = p - 1;

	return interpolate(capacity, p->capacity, p->ocv, q->capacity,
			   q->ocv);
}

static int calc_esr(int capacity, int temp)
{
	struct sysedp_batmon_rbat_lut *lut = pdata->rbat_lut;
	int ret = pdata->r_const;
	ret += bilinear_interpolate(lut->data, lut->temp_axis,
				    lut->capacity_axis, lut->temp_size,
				    lut->capacity_size, temp, capacity);
	return ret;
}

/* calculate maximum allowed current (in mA) limited by equivalent
 * series resistance (esr) */
static s64 calc_ibat_esr(s64 ocv, s64 esr)
{
	if (ocv <= pdata->vsys_min)
		return 0;
	else if (esr <= 0)
		return 0;
	else
		return div64_s64(1000 * (ocv - pdata->vsys_min), esr);
}

/* Calc IBAT for a given temperature */
static int calc_ibat(int temp)
{
	struct sysedp_batmon_ibat_lut *p;
	struct sysedp_batmon_ibat_lut *q;
	int ibat;

	p = pdata->ibat_lut;
	while (p->ibat && p->temp > temp)
		p++;

	if (p == pdata->ibat_lut)
		return p->ibat;

	q = p - 1;
	ibat = interpolate(temp, p->temp, p->ibat, q->temp, q->ibat);

	return ibat > 0 ? ibat : 0;
}

static s64 calc_pbat(s64 ocv, s64 ibat, s64 esr)
{
	s64 vsys;
	vsys = ocv - div64_s64(ibat * esr, 1000);
	return div64_s64(vsys * ibat, 1000000);
}

static unsigned int calc_avail_budget(void)
{
	unsigned int capacity;
	int temp;
	s64 ocv;
	s64 ibat_esr;
	s64 ibat;
	s64 ibat_max;
	s64 pbat;

	capacity = psy_capacity();
	temp = psy_temp();
	ocv = get_ocv(capacity);
	esr = calc_esr(capacity, temp);

	ibat_esr = calc_ibat_esr(ocv, esr);
	ibat = calc_ibat(temp);
	ibat_max = min(ibat_esr, ibat);

	pbat = calc_pbat(ocv, ibat_max, esr);

	pr_debug("capacity : %u\n", capacity);
	pr_debug("ocv      : %lld\n", ocv);
	pr_debug("esr      : %d\n", esr);
	pr_debug("ibat_esr : %lld\n", ibat_esr);
	pr_debug("ibat     : %lld\n", ibat);
	pr_debug("ibat_max : %lld\n", ibat_max);
	pr_debug("pbat     : %lld\n", pbat);

	return pbat;
}

static void batmon_update(struct work_struct *work)
{
	unsigned int budget;
	unsigned int update_interval;
	budget = calc_avail_budget();
	sysedp_set_avail_budget(budget);

	update_interval = pdata->update_interval ?: UPDATE_INTERVAL;

	schedule_delayed_work(to_delayed_work(work),
			      msecs_to_jiffies(update_interval));
}

static void batmon_shutdown(struct platform_device *pdev)
{
	cancel_delayed_work_sync(&work);
}

static int batmon_suspend(struct platform_device *pdev, pm_message_t state)
{
	batmon_shutdown(pdev);
	return 0;
}

static int batmon_resume(struct platform_device *pdev)
{
	schedule_delayed_work(&work, 0);
	return 0;
}

static int init_ocv_reader(void)
{
	if (pdata->ocv_lut)
		get_ocv = psy_ocv_from_lut;
	else if (!psy_get_property(POWER_SUPPLY_PROP_VOLTAGE_OCV, NULL))
		get_ocv = psy_ocv_from_chip;
	else
		return -ENODEV;

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int rbat_show(struct seq_file *file, void *data)
{
	int t, c, i = 0;
	struct sysedp_batmon_rbat_lut *lut = pdata->rbat_lut;

	seq_printf(file, " %8s", "capacity");
	for (t = 0; t < lut->temp_size; t++)
		seq_printf(file, "%7dC", lut->temp_axis[t]);
	seq_puts(file, "\n");

	for (c = 0; c < lut->capacity_size; c++) {
		seq_printf(file, "%8d%%", lut->capacity_axis[c]);
		for (t = 0; t < lut->temp_size; t++)
			seq_printf(file, "%8d", lut->data[i++]);
		seq_puts(file, "\n");
	}
	return 0;
}

static int ibat_show(struct seq_file *file, void *data)
{
	struct sysedp_batmon_ibat_lut *lut = pdata->ibat_lut;

	if (lut) {
		do {
			seq_printf(file, "%7dC %7dmA\n", lut->temp, lut->ibat);
		} while ((lut++)->ibat);
	}
	return 0;
}

static int ocv_show(struct seq_file *file, void *data)
{
	struct sysedp_batmon_ocv_lut *lut = pdata->ocv_lut;

	if (lut) {
		do {
			seq_printf(file, "%7d%% %7duV\n", lut->capacity,
				   lut->ocv);
		} while ((lut++)->capacity);
	}
	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, inode->i_private, NULL);
}

static const struct file_operations debug_fops = {
	.open = debug_open,
	.read = seq_read,
};

static void init_debug(void)
{
	struct dentry *dd, *df;

	if (!sysedp_debugfs_dir)
		return;

	dd = debugfs_create_dir("batmon", sysedp_debugfs_dir);
	WARN_ON(IS_ERR_OR_NULL(dd));

	df = debugfs_create_file("rbat", S_IRUGO, dd, rbat_show, &debug_fops);
	WARN_ON(IS_ERR_OR_NULL(df));

	df = debugfs_create_file("ibat", S_IRUGO, dd, ibat_show, &debug_fops);
	WARN_ON(IS_ERR_OR_NULL(df));

	df = debugfs_create_file("ocv", S_IRUGO, dd, ocv_show, &debug_fops);
	WARN_ON(IS_ERR_OR_NULL(df));

	df = debugfs_create_u32("r_const", S_IRUGO, dd, &pdata->r_const);
	WARN_ON(IS_ERR_OR_NULL(df));

	df = debugfs_create_u32("vsys_min", S_IRUGO, dd, &pdata->vsys_min);
	WARN_ON(IS_ERR_OR_NULL(df));
}
#else
static inline void init_debug(void) {}
#endif


static void of_batmon_calc_get_pdata(struct platform_device *pdev,
	struct sysedp_batmon_calc_platform_data **pdata)
{
	struct device_node *np = pdev->dev.of_node;
	struct sysedp_batmon_calc_platform_data *obj_ptr;
	u32 *u32_ptr;
	const char *c_ptr;
	const void *ptr;
	u32 lenp, val;
	int n;
	int ret;
	int i;

	obj_ptr = devm_kzalloc(&pdev->dev,
		sizeof(struct sysedp_batmon_calc_platform_data), GFP_KERNEL);
	if (!obj_ptr)
		return;

	ptr = of_get_property(np, "power_supply", &lenp);
	if (!ptr) {
		dev_err(&pdev->dev, "Fail to get power_supply\n");
		return;
	} else {
		obj_ptr->power_supply = devm_kzalloc(&pdev->dev,
			sizeof(char) * lenp, GFP_KERNEL);
		if (!obj_ptr->power_supply)
			return;
		ret = of_property_read_string(np, "power_supply", &c_ptr);
		if (ret) {
			dev_err(&pdev->dev, "Fail to read power_supply\n");
			return;
		}
		strncpy(obj_ptr->power_supply, c_ptr, lenp);
	}

	ret = of_property_read_u32(np, "r_const", &val);
	if (ret)
		dev_info(&pdev->dev, "Fail to read r_const\n");
	else
		obj_ptr->r_const = val;

	ret = of_property_read_u32(np, "vsys_min", &val);
	if (ret)
		dev_info(&pdev->dev, "Fail to read vsys_min\n");
	else
		obj_ptr->vsys_min = val;

	ret = of_property_read_u32(np, "update_interval", &val);
	if (!ret)
		obj_ptr->update_interval = val;

	ptr = of_get_property(np, "ocv_lut", &lenp);
	if (ptr) {
		n = lenp / sizeof(u32);
		if (!n || (n % 2) != 0)
			return;
		obj_ptr->ocv_lut = devm_kzalloc(&pdev->dev,
			sizeof(struct sysedp_batmon_ocv_lut) * n / 2,
			GFP_KERNEL);
		if (!obj_ptr->ocv_lut)
			return;
		u32_ptr = kzalloc(sizeof(u32) * n, GFP_KERNEL);
		if (!u32_ptr)
			return;
		ret = of_property_read_u32_array(np, "ocv_lut", u32_ptr, n);
		if (ret) {
			dev_err(&pdev->dev, "Fail to read ocv_lut\n");
			kfree(u32_ptr);
			return;
		}
		for (i = 0; i < n / 2; ++i) {
			obj_ptr->ocv_lut[i].capacity = u32_ptr[2 * i];
			obj_ptr->ocv_lut[i].ocv = u32_ptr[2 * i + 1];
		}
		kfree(u32_ptr);
	}

	ptr = of_get_property(np, "ibat_lut", &lenp);
	if (!ptr) {
		dev_err(&pdev->dev, "Fail to get ibat_lut\n");
		return;
	}
	n = lenp / sizeof(u32);
	if (!n || (n % 2) != 0)
		return;
	obj_ptr->ibat_lut = devm_kzalloc(&pdev->dev,
		sizeof(struct sysedp_batmon_ibat_lut) * n / 2, GFP_KERNEL);
	if (!obj_ptr->ibat_lut)
		return;
	u32_ptr = kzalloc(sizeof(u32) * n, GFP_KERNEL);
	if (!u32_ptr)
		return;
	ret = of_property_read_u32_array(np, "ibat_lut", u32_ptr, n);
	if (ret) {
		dev_err(&pdev->dev, "Fail to read ibat_lut\n");
		kfree(u32_ptr);
		return;
	}
	for (i = 0; i < n / 2; ++i) {
		obj_ptr->ibat_lut[i].temp = (s32)u32_ptr[2 * i];
		obj_ptr->ibat_lut[i].ibat = u32_ptr[2 * i + 1];
	}
	kfree(u32_ptr);

	obj_ptr->rbat_lut = devm_kzalloc(&pdev->dev,
		sizeof(struct sysedp_batmon_rbat_lut), GFP_KERNEL);
	if (!obj_ptr->rbat_lut)
		return;

	ptr = of_get_property(np, "rbat_data", &lenp);
	if (!ptr) {
		dev_err(&pdev->dev, "Fail to get rbat_data\n");
		return;
	}
	n = lenp / sizeof(u32);
	if (!n)
		return;
	obj_ptr->rbat_lut->data = devm_kzalloc(&pdev->dev,
		sizeof(int) * n, GFP_KERNEL);
	if (!obj_ptr->rbat_lut->data)
		return;
	u32_ptr = kzalloc(sizeof(u32) * n, GFP_KERNEL);
	if (!u32_ptr)
		return;
	ret = of_property_read_u32_array(np, "rbat_data", u32_ptr, n);
	if (ret) {
		dev_err(&pdev->dev, "Fail to read rbat_data\n");
		kfree(u32_ptr);
		return;
	}
	for (i = 0; i < n; ++i)
		obj_ptr->rbat_lut->data[i] = u32_ptr[i];
	kfree(u32_ptr);
	obj_ptr->rbat_lut->data_size = n;

	ptr = of_get_property(np, "temp_axis", &lenp);
	if (!ptr) {
		dev_err(&pdev->dev, "Fail to get temp_axis\n");
		return;
	}
	n = lenp / sizeof(u32);
	if (!n)
		return;
	obj_ptr->rbat_lut->temp_axis = devm_kzalloc(&pdev->dev,
		sizeof(int) * n, GFP_KERNEL);
	if (!obj_ptr->rbat_lut->temp_axis)
		return;
	u32_ptr = kzalloc(sizeof(u32) * n, GFP_KERNEL);
	if (!u32_ptr)
		return;
	ret = of_property_read_u32_array(np, "temp_axis", u32_ptr, n);
	if (ret) {
		dev_err(&pdev->dev, "Fail to read temp_axis\n");
		kfree(u32_ptr);
		return;
	}
	for (i = 0; i < n; ++i)
		obj_ptr->rbat_lut->temp_axis[i] = (s32)u32_ptr[i];
	kfree(u32_ptr);
	obj_ptr->rbat_lut->temp_size = n;

	ptr = of_get_property(np, "capacity_axis", &lenp);
	if (!ptr) {
		dev_err(&pdev->dev, "Fail to get capacity_axis\n");
		return;
	}
	n = lenp / sizeof(u32);
	if (!n)
		return;
	obj_ptr->rbat_lut->capacity_axis = devm_kzalloc(&pdev->dev,
		sizeof(int) * n, GFP_KERNEL);
	if (!obj_ptr->rbat_lut->capacity_axis)
		return;
	u32_ptr = kzalloc(sizeof(u32) * n, GFP_KERNEL);
	if (!u32_ptr)
		return;
	ret = of_property_read_u32_array(np, "capacity_axis", u32_ptr, n);
	if (ret) {
		dev_err(&pdev->dev, "Fail to read capacity_axis\n");
		kfree(u32_ptr);
		return;
	}
	for (i = 0; i < n; ++i)
		obj_ptr->rbat_lut->capacity_axis[i] = u32_ptr[i];
	kfree(u32_ptr);
	obj_ptr->rbat_lut->capacity_size = n;

	*pdata = obj_ptr;
	return;
}

static int batmon_probe(struct platform_device *pdev)
{
	int i;
	struct sysedp_batmon_rbat_lut *rbat;

	if (pdev->dev.of_node)
		of_batmon_calc_get_pdata(pdev, &pdata);
	else
		pdata = pdev->dev.platform_data;

	if (!pdata)
		return -EINVAL;

	/* validate pdata->rbat_lut table */
	rbat = pdata->rbat_lut;
	if (!rbat)
		return -EINVAL;
	for (i = 1; i < rbat->temp_size; i++)
		if (rbat->temp_axis[i] >= rbat->temp_axis[i-1])
			return -EINVAL;
	for (i = 1; i < rbat->capacity_size; i++)
		if (rbat->capacity_axis[i] >= rbat->capacity_axis[i-1])
			return -EINVAL;
	if (rbat->capacity_size * rbat->temp_size != rbat->data_size)
		return -EINVAL;

	psy = power_supply_get_by_name(pdata->power_supply);

	if (!psy)
		return -EFAULT;

	if (init_ocv_reader())
		return -EFAULT;

	INIT_DEFERRABLE_WORK(&work, batmon_update);
	schedule_delayed_work(&work, 0);

	init_debug();

	return 0;
}

static const struct of_device_id batmon_calc_of_match[] = {
	{ .compatible = "nvidia,tegra124-sysedp_batmon_calc", },
};
MODULE_DEVICE_TABLE(of, batmon_calc_of_match);

static struct platform_driver batmon_driver = {
	.probe = batmon_probe,
	.shutdown = batmon_shutdown,
	.suspend = batmon_suspend,
	.resume = batmon_resume,
	.driver = {
		.name = "sysedp_batmon_calc",
		.owner = THIS_MODULE,
		.of_match_table = batmon_calc_of_match,
	}
};

static __init int batmon_init(void)
{
	return platform_driver_register(&batmon_driver);
}
late_initcall(batmon_init);
