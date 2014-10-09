/*
 * intel_byt_dts_thermal.c
 * Copyright (c) 2014, Intel Corporation.
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
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/cpu_device_id.h>
#include <asm/iosf_mbi.h>
#include <linux/thermal.h>

#define BYT_AUX_DTS_OFFSET_ENABLE	0xB0
#define BYT_AUX_DTS_OFFSET_TEMP		0xB1

#define BYT_AUX_DTS_OFFSET_PTPS		0xB2
#define BYT_AUX_DTS_OFFSET_PTTS		0xB3
#define BYT_AUX_DTS_OFFSET_PTTSS	0xB4
#define BYT_AUX_DTS_OFFSET_PTMC		0x80
#define BYT_AUX_DTS_TE_AUX0		0xb5
#define BYT_AUX_DTS_TE_AUX1		0xb6

#define BYT_AUX_DTS_AUX0_ENABLE_BIT		BIT(0)
#define BYT_AUX_DTS_AUX1_ENABLE_BIT		BIT(1)
#define BYT_AUX_DTS_CPU_MODULE0_ENABLE_BIT	BIT(16)
#define BYT_AUX_DTS_CPU_MODULE1_ENABLE_BIT	BIT(17)
#define BYT_AUX_DTS_TE_SCI_ENABLE		BIT(9)
#define BYT_AUX_DTS_TE_SMI_ENABLE		BIT(10)
#define BYT_AUX_DTS_TE_MSI_ENABLE		BIT(11)
#define BYT_AUX_DTS_TE_APICA_ENABLE		BIT(14)

/* IRQ 86 is a fixed APIC interrupt for BYT DTS Aux threshold notifications */
#define BYT_AUX_DTS_APIC_IRQ			86

/* Only 2 out of 4 is allowed for OSPM */
#define BYT_MAX_AUX_TRIPS		2

/* DTS0 and DTS 1 */
#define BYT_MAX_AUX_SENSORS		2

#define CRITICAL_OFFSET_FROM_TJ_MAX	5000

struct byt_aux_sensor_entry {
	int id;
	u32 tj_max;
	u32 temp_mask;
	u32 temp_shift;
	u32 store_status;
	struct thermal_zone_device *tzone;
};

static struct byt_aux_sensor_entry *aux_dts[BYT_MAX_AUX_SENSORS];

static int crit_offset = CRITICAL_OFFSET_FROM_TJ_MAX;
module_param(crit_offset, int, 0644);
MODULE_PARM_DESC(crit_offset, "Critical Temperature offset from tj max.");

static DEFINE_MUTEX(aux_update_mutex);
static spinlock_t intr_notify_lock;

static int get_tj_max(u32 *tj_max)
{
	u32 eax, edx;
	u32 val;
	int err;

	err = rdmsr_safe(MSR_IA32_TEMPERATURE_TARGET, &eax, &edx);
	if (err)
		goto err_ret;
	else {
		val = (eax >> 16) & 0xff;
		if (val)
			*tj_max = val * 1000;
		else {
			err = -EINVAL;
			goto err_ret;
		}
	}

	return 0;
err_ret:
	*tj_max = 0;

	return err;
}

static int sys_get_trip_temp(struct thermal_zone_device *tzd,
					int trip, unsigned long *temp)
{
	int status;
	u32 out;
	struct byt_aux_sensor_entry *aux_entry;

	aux_entry = tzd->devdata;

	if (!trip) {
		/* Just return the critical temp */
		*temp = aux_entry->tj_max - crit_offset;
		return 0;
	}

	mutex_lock(&aux_update_mutex);
	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_OFFSET_PTPS, &out);
	mutex_unlock(&aux_update_mutex);
	if (status)
		return status;

	out = (out >> (trip * 8)) & 0x7f;

	if (!out)
		*temp = 0;
	else
		*temp = aux_entry->tj_max - out * 1000;

	return 0;
}

static int update_trip_temp(struct byt_aux_sensor_entry *aux_entry,
				int thres_index, unsigned long temp)
{
	int status;
	u32 temp_out;
	u32 out;
	u32 store_ptps;
	u32 store_ptmc;
	u32 store_te_out;
	u32 te_out;

	u32 int_enable_bit = BYT_AUX_DTS_TE_APICA_ENABLE |
						BYT_AUX_DTS_TE_MSI_ENABLE;

	temp_out = (aux_entry->tj_max - temp) / 1000;

	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
				BYT_AUX_DTS_OFFSET_PTPS, &store_ptps);
	if (status)
		return status;

	out = (store_ptps & ~(0xFF << (thres_index * 8)));
	out |= (temp_out & 0xff) << (thres_index * 8);
	status = iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
				BYT_AUX_DTS_OFFSET_PTPS, out);
	if (status)
		return status;
	pr_debug("update_trip_temp PTPS = %x\n", out);
	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_OFFSET_PTMC, &out);
	if (status)
		goto err_restore_ptps;

	store_ptmc = out;

	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_TE_AUX0 + thres_index,
					&te_out);
	if (status)
		goto err_restore_ptmc;

	store_te_out = te_out;

	/* Enable for CPU module 0 and module 1 */
	out |= (BYT_AUX_DTS_CPU_MODULE0_ENABLE_BIT |
					BYT_AUX_DTS_CPU_MODULE1_ENABLE_BIT);
	if (temp) {
		if (thres_index)
			out |= BYT_AUX_DTS_AUX1_ENABLE_BIT;
		else
			out |= BYT_AUX_DTS_AUX0_ENABLE_BIT;
		te_out |= int_enable_bit;
	} else {
		if (thres_index)
			out &= ~BYT_AUX_DTS_AUX1_ENABLE_BIT;
		else
			out &= ~BYT_AUX_DTS_AUX0_ENABLE_BIT;
		te_out &= ~int_enable_bit;
	}
	status = iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
					BYT_AUX_DTS_OFFSET_PTMC, out);
	if (status)
		goto err_restore_te_out;

	status = iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
					BYT_AUX_DTS_TE_AUX0 + thres_index,
					te_out);
	if (status)
		goto err_restore_te_out;

	return 0;

err_restore_te_out:
	iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
				BYT_AUX_DTS_OFFSET_PTMC, store_te_out);
err_restore_ptmc:
	iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
				BYT_AUX_DTS_OFFSET_PTMC, store_ptmc);
err_restore_ptps:
	iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
				BYT_AUX_DTS_OFFSET_PTPS, store_ptps);
	/* Nothing we can do if restore fails */

	return status;
}

static int sys_set_trip_temp(struct thermal_zone_device *tzd, int trip,
							unsigned long temp)
{
	struct byt_aux_sensor_entry *aux_entry = tzd->devdata;
	int status;

	if (temp > (aux_entry->tj_max - crit_offset))
		return -EINVAL;

	mutex_lock(&aux_update_mutex);
	status = update_trip_temp(tzd->devdata, trip, temp);
	mutex_unlock(&aux_update_mutex);

	return status;
}

static int sys_get_trip_type(struct thermal_zone_device *thermal,
		int trip, enum thermal_trip_type *type)
{
	if (trip)
		*type = THERMAL_TRIP_PASSIVE;
	else
		*type = THERMAL_TRIP_CRITICAL;

	return 0;
}

static int sys_get_curr_temp(struct thermal_zone_device *tzd,
						unsigned long *temp)
{
	int status;
	u32 out;
	struct byt_aux_sensor_entry *aux_entry;
	int retry_count = 0;
	unsigned long last_temp = 0;

	aux_entry = tzd->devdata;

read_temp_again:
	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_OFFSET_TEMP, &out);
	if (status)
		return status;

	out = (out & aux_entry->temp_mask) >> aux_entry->temp_shift;
	out -= 0x7F;
	*temp = aux_entry->tj_max - out * 1000;

	if (*temp > aux_entry->tj_max) {
		pr_err("Ignore junk temp %lu \n", *temp);
		return -EAGAIN; /* Let user space retry if required */
	}

	if ((*temp >= (aux_entry->tj_max - crit_offset)) &&
						retry_count++ < 2) {
		if (!last_temp)
			last_temp = *temp;
		else {
			if (abs(last_temp - *temp) > 2000)
				return -EINVAL;
		}
		udelay(100);
		goto read_temp_again;
	}

	return 0;
}

static struct thermal_zone_device_ops tzone_ops = {
	.get_temp = sys_get_curr_temp,
	.get_trip_temp = sys_get_trip_temp,
	.get_trip_type = sys_get_trip_type,
	.set_trip_temp = sys_set_trip_temp,
};

static void free_aux_dts(struct byt_aux_sensor_entry *aux_entry)
{
	if (aux_entry) {
		iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
			BYT_AUX_DTS_OFFSET_ENABLE, aux_entry->store_status);
		thermal_zone_device_unregister(aux_entry->tzone);
		kfree(aux_entry);
	}
}

static int aux_dts_enable(int id)
{
	u32 out;
	int ret;

	ret = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_OFFSET_ENABLE, &out);
	if (ret)
		return ret;

	if (!(out & BIT(id))) {
		out |= BIT(id);
		ret = iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
					BYT_AUX_DTS_OFFSET_ENABLE, out);
		if (ret)
			return ret;
	}

	return ret;
}

static struct byt_aux_sensor_entry *alloc_aux_dts(int id, u32 tj_max)
{
	struct byt_aux_sensor_entry *aux_entry;
	char name[10];
	int err;

	aux_entry = kzalloc(sizeof(*aux_entry), GFP_KERNEL);
	if (!aux_entry) {
		err = -ENOMEM;
		return ERR_PTR(-ENOMEM);
	}

	/* Store status to restor on exit */
	err = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_OFFSET_ENABLE,
					&aux_entry->store_status);
	if (err)
		goto err_ret;

	aux_entry->id = id;
	aux_entry->tj_max = tj_max;
	aux_entry->temp_mask = 0x00FF << (id * 8);
	aux_entry->temp_shift = id * 8;
	snprintf(name, sizeof(name), "aux_dts%d", id);
	aux_entry->tzone = thermal_zone_device_register(name,
			BYT_MAX_AUX_TRIPS,
			0x02,
			aux_entry, &tzone_ops, NULL, 0, 0);
	if (IS_ERR(aux_entry->tzone)) {
		err = PTR_ERR(aux_entry->tzone);
		goto err_ret;
	}

	err = aux_dts_enable(id);
	if (err)
		goto err_aux_status;

	return aux_entry;

err_aux_status:
	thermal_zone_device_unregister(aux_entry->tzone);
err_ret:
	kfree(aux_entry);
	return ERR_PTR(err);
}

static void proc_thermal_interrupt(void)
{
	u32 out, sticky_out;
	int status;
	u32 ptmc_out;

	/* Clear APIC interrupt */
	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
				BYT_AUX_DTS_OFFSET_PTMC, &ptmc_out);

	ptmc_out |= 0x10;
	status = iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
					BYT_AUX_DTS_OFFSET_PTMC, ptmc_out);

	/* Read status here */
	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_OFFSET_PTTS, &out);
	status = iosf_mbi_read(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_READ,
					BYT_AUX_DTS_OFFSET_PTTSS, &sticky_out);
	pr_debug("status %d PTTSS %x\n", status, sticky_out);
	if (sticky_out & 0x03) {
		int i;
		/* reset sticky bit */
		status = iosf_mbi_write(BT_MBI_UNIT_PMC, BT_MBI_BUNIT_WRITE,
					BYT_AUX_DTS_OFFSET_PTTSS, out);
		for (i = 0; i < BYT_MAX_AUX_SENSORS; ++i) {
			pr_debug("TZD update for zone %d\n", i);
			thermal_zone_device_update(aux_dts[i]->tzone);
		}
	}

}

static irqreturn_t byt_aux_dts_ist(int irq, void *dev_data)
{
	unsigned long flags;

	spin_lock_irqsave(&intr_notify_lock, flags);
	proc_thermal_interrupt();
	spin_unlock_irqrestore(&intr_notify_lock, flags);
	pr_debug("byt_aux_dts_ist\n");

	return IRQ_HANDLED;
}

static const struct x86_cpu_id byt_aux_thermal_ids[] = {
	{ X86_VENDOR_INTEL, X86_FAMILY_ANY, 0x37 },
	{}
};
MODULE_DEVICE_TABLE(x86cpu, byt_aux_thermal_ids);

static int __init byt_aux_thermal_init(void)
{
	u32 tj_max;
	int err = 0;
	int i;

	if (!x86_match_cpu(byt_aux_thermal_ids))
		return -ENODEV;

	if (get_tj_max(&tj_max))
		return -EINVAL;

	for (i = 0; i < BYT_MAX_AUX_SENSORS; ++i) {
		aux_dts[i] = alloc_aux_dts(i, tj_max);
		if (IS_ERR(aux_dts[i])) {
			err = PTR_ERR(aux_dts[i]);
			goto err_free;
		}
	}
	spin_lock_init(&intr_notify_lock);

	err = request_threaded_irq(BYT_AUX_DTS_APIC_IRQ, NULL, byt_aux_dts_ist,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"byt_aux_dts", aux_dts);
	if (err) {
		pr_err("request_threaded_irq ret %d\n", err);
		goto err_free;
	}

	for (i = 0; i < BYT_MAX_AUX_SENSORS; ++i) {
		err = update_trip_temp(aux_dts[i], 0, tj_max - crit_offset);
		if (err)
			goto err_trip_temp;
	}

	return 0;

err_trip_temp:
	free_irq(BYT_AUX_DTS_APIC_IRQ, aux_dts);
err_free:
	for (i = 0; i < BYT_MAX_AUX_SENSORS; ++i)
		free_aux_dts(aux_dts[i]);

	return err;
}

static void __exit byt_aux_thermal_exit(void)
{
	int i;

	for (i = 0; i < BYT_MAX_AUX_SENSORS; ++i)
		update_trip_temp(aux_dts[i], 0, 0);

	free_irq(BYT_AUX_DTS_APIC_IRQ, aux_dts);

	for (i = 0; i < BYT_MAX_AUX_SENSORS; ++i)
		free_aux_dts(aux_dts[i]);

}

module_init(byt_aux_thermal_init)
module_exit(byt_aux_thermal_exit)

MODULE_DESCRIPTION("Intel Baytrail auxiliary DTS Sensor Thermal Driver");
MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com>");
MODULE_LICENSE("GPL v2");
