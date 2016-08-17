/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/edp.h>
#include <linux/edpdev.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define DEPL_INTERVAL	60000

struct depl_driver {
	struct edp_client client;
	struct psy_depletion_platform_data *pdata;
	struct delayed_work work;
	struct edp_manager *manager;
	struct power_supply *psy;
	bool emulator_mode;
	int capacity;
	int (*get_ocv)(struct depl_driver *drv, unsigned int capacity);
};

static int depl_psy_get_property(struct depl_driver *drv,
		enum power_supply_property psp, int *val)
{
	union power_supply_propval pv;

	if (!drv->psy)
		return -ENODEV;
	if (drv->psy->get_property(drv->psy, psp, &pv))
		return -EFAULT;
	if (val)
		*val = pv.intval;
	return 0;
}

static int depl_psy_ocv_from_chip(struct depl_driver *drv,
		unsigned int capacity)
{
	int val;
	if (depl_psy_get_property(drv, POWER_SUPPLY_PROP_VOLTAGE_OCV, &val))
		return drv->pdata->vsys_min;
	return val;
}

static int depl_psy_capacity(struct depl_driver *drv)
{
	int val;
	if (drv->emulator_mode)
		return drv->capacity;
	if (depl_psy_get_property(drv, POWER_SUPPLY_PROP_CAPACITY, &val))
		return 0;
	return val;
}

static unsigned int depl_psy_temp(struct depl_driver *drv)
{
	int val;
	if (depl_psy_get_property(drv, POWER_SUPPLY_PROP_TEMP, &val))
		return 25;
	return max(0, val);
}

static inline unsigned int depl_maximum(struct depl_driver *drv)
{
	return drv->client.states[0];
}

/* Given two points (x1, y1) and (x2, y2), find the y coord of x */
static int depl_interpolate(int x, int x1, int y1, int x2, int y2)
{
	if (x1 == x2)
		return y1;
	return (y2 * (x - x1) - y1 * (x - x2)) / (x2 - x1);
}

static int depl_psy_ocv_from_lut(struct depl_driver *drv,
		unsigned int capacity)
{
	struct psy_depletion_ocv_lut *p;
	struct psy_depletion_ocv_lut *q;

	p = drv->pdata->ocv_lut;

	while (p->capacity > capacity)
		p++;

	if (p == drv->pdata->ocv_lut)
		return p->ocv;

	q = p - 1;

	return depl_interpolate(capacity, p->capacity, p->ocv, q->capacity,
			q->ocv);
}

/* Calc RBAT for current capacity (SOC) */
static int depl_rbat(struct depl_driver *drv, unsigned int capacity)
{
	struct psy_depletion_rbat_lut *p;
	struct psy_depletion_rbat_lut *q;
	int rbat;

	rbat = drv->pdata->r_const;
	p = drv->pdata->rbat_lut;
	if (!p)
		return rbat;

	while (p->capacity > capacity)
		p++;

	if (p == drv->pdata->rbat_lut)
		return rbat + p->rbat;

	q = p - 1;

	rbat += depl_interpolate(capacity, p->capacity, p->rbat,
			q->capacity, q->rbat);
	return rbat;
}

static s64 depl_ibat_possible(struct depl_driver *drv, s64 ocv, s64 rbat)
{
	return div64_s64(1000 * (ocv - drv->pdata->vsys_min), rbat);
}

/* Calc IBAT for a given temperature */
static int depl_ibat(struct depl_driver *drv, unsigned int temp)
{
	struct psy_depletion_ibat_lut *p;
	struct psy_depletion_ibat_lut *q;
	int ibat;

	p = drv->pdata->ibat_lut;
	while (p->ibat && p->temp > temp)
		p++;

	if (p == drv->pdata->ibat_lut || !p->ibat)
		return p->ibat;

	q = p - 1;
	ibat = depl_interpolate(temp, p->temp, p->ibat, q->temp, q->ibat);

	return ibat;
}

static s64 depl_pbat(s64 ocv, s64 ibat, s64 rbat)
{
	s64 pbat;
	pbat = ocv - div64_s64(ibat * rbat, 1000);
	pbat = div64_s64(pbat * ibat, 1000000);
	return pbat;
}

static unsigned int depl_calc(struct depl_driver *drv)
{
	unsigned int capacity;
	s64 ocv;
	s64 rbat;
	s64 ibat_pos;
	s64 ibat_tbat;
	s64 ibat_lcm;
	s64 pbat_lcm;
	s64 pbat_nom;
	s64 pbat_gain;
	s64 depl;

	capacity = depl_psy_capacity(drv);
	if (capacity >= 100)
		return 0;

	ocv = drv->get_ocv(drv, capacity);
	rbat = depl_rbat(drv, capacity);

	ibat_pos = depl_ibat_possible(drv, ocv, rbat);
	ibat_tbat = depl_ibat(drv, depl_psy_temp(drv));
	ibat_lcm = min(ibat_pos, ibat_tbat);

	pbat_lcm = depl_pbat(ocv, ibat_lcm, rbat);
	pbat_nom = depl_pbat(drv->pdata->vcharge, drv->pdata->ibat_nom, rbat);
	pbat_gain = div64_s64(drv->manager->max * 1000, pbat_nom);

	depl = drv->manager->max - div64_s64(pbat_gain * pbat_lcm, 1000);

	pr_debug("capacity : %u\n", capacity);
	pr_debug("ocv      : %lld\n", ocv);
	pr_debug("rbat     : %lld\n", rbat);
	pr_debug("ibat_pos : %lld\n", ibat_pos);
	pr_debug("ibat_tbat: %lld\n", ibat_tbat);
	pr_debug("ibat_lcm : %lld\n", ibat_lcm);
	pr_debug("pbat_lcm : %lld\n", pbat_lcm);
	pr_debug("pbat_nom : %lld\n", pbat_nom);
	pr_debug("pbat_gain: %lld\n", pbat_gain);
	pr_debug("depletion: %lld\n", depl);

	depl = clamp_t(s64, depl, 0, depl_maximum(drv));
	return depl;
}

static void depl_update(struct work_struct *work)
{
	struct depl_driver *drv;
	struct edp_client *c;
	unsigned int depl;
	unsigned int i;

	drv = container_of(work, struct depl_driver, work.work);
	c = &drv->client;
	depl = depl_calc(drv);

	i = c->num_states - 1;
	while (i && c->states[i] < depl)
		i--;

	edp_update_client_request(c, i, NULL);

	schedule_delayed_work(to_delayed_work(work),
			msecs_to_jiffies(DEPL_INTERVAL));
}

/* Nothing to do */
static void depl_edp_callback(unsigned int new_state, void *priv_data)
{
}

static void depl_shutdown(struct platform_device *pdev)
{
	struct depl_driver *drv = platform_get_drvdata(pdev);
	cancel_delayed_work_sync(&drv->work);
}

static int depl_suspend(struct platform_device *pdev, pm_message_t state)
{
	depl_shutdown(pdev);
	return 0;
}

static int depl_resume(struct platform_device *pdev)
{
	struct depl_driver *drv = platform_get_drvdata(pdev);
	schedule_delayed_work(&drv->work, 0);
	return 0;
}

static __devinit int depl_init_ocv_reader(struct depl_driver *drv)
{
	if (!depl_psy_get_property(drv, POWER_SUPPLY_PROP_VOLTAGE_OCV, NULL))
		drv->get_ocv = depl_psy_ocv_from_chip;
	else if (drv->pdata->ocv_lut)
		drv->get_ocv = depl_psy_ocv_from_lut;
	else
		return -ENODEV;

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int capacity_set(void *data, u64 val)
{
	struct depl_driver *drv = data;

	drv->capacity = clamp_t(int, val, 0, 100);
	flush_delayed_work_sync(&drv->work);

	return 0;
}

static int capacity_get(void *data, u64 *val)
{
	struct depl_driver *drv = data;
	*val = drv->capacity;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(capacity_fops, capacity_get, capacity_set, "%lld\n");

static void init_debug(struct depl_driver *drv)
{
	struct dentry *d;

	if (!drv->client.dentry) {
		WARN_ON(1);
		return;
	}

	d = debugfs_create_file("capacity", S_IRUGO | S_IWUSR,
			drv->client.dentry, drv, &capacity_fops);
	WARN_ON(IS_ERR_OR_NULL(d));
}
#else
static inline void init_debug(struct depl_driver *drv) {}
#endif

static __devinit int depl_probe(struct platform_device *pdev)
{
	struct depl_driver *drv;
	struct edp_manager *m;
	struct edp_client *c;
	int r = -EFAULT;

	if (!pdev->dev.platform_data)
		return -EINVAL;

	m = edp_get_manager("battery");
	if (!m) {
		dev_err(&pdev->dev, "could not get EDP manager\n");
		return -ENODEV;
	}

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->pdata = pdev->dev.platform_data;
	drv->manager = m;
	drv->psy = power_supply_get_by_name(drv->pdata->power_supply);
	if (!drv->psy) {
		if (!drv->pdata->ocv_lut)
			goto fail;
		drv->capacity = 100;
		drv->emulator_mode = true;
	}

	r = depl_init_ocv_reader(drv);
	if (r)
		goto fail;

	c = &drv->client;
	strncpy(c->name, "depletion", EDP_NAME_LEN - 1);
	c->name[EDP_NAME_LEN - 1] = 0;
	c->priority = EDP_MAX_PRIO;
	c->throttle = depl_edp_callback;
	c->notify_promotion = depl_edp_callback;
	c->states = drv->pdata->states;
	c->num_states = drv->pdata->num_states;
	c->e0_index = drv->pdata->e0_index;

	r = edp_register_client(m, c);
	if (r) {
		dev_err(&pdev->dev, "failed to register: %d\n", r);
		goto fail;
	}

	platform_set_drvdata(pdev, drv);
	INIT_DELAYED_WORK_DEFERRABLE(&drv->work, depl_update);
	schedule_delayed_work(&drv->work, 0);

	if (drv->emulator_mode)
		init_debug(drv);

	return 0;

fail:
	devm_kfree(&pdev->dev, drv);
	return r;
}

static struct platform_driver depl_driver = {
	.probe = depl_probe,
	.shutdown = depl_shutdown,
	.suspend = depl_suspend,
	.resume = depl_resume,
	.driver = {
		.name = "psy_depletion",
		.owner = THIS_MODULE
	}
};

static __init int depl_init(void)
{
	return platform_driver_register(&depl_driver);
}
late_initcall(depl_init);
