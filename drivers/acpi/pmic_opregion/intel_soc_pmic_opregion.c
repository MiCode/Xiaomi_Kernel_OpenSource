/*
 * intel_soc_pmic_opregion.c - Intel SoC PMIC operation region Driver
 *
 * Copyright (C) 2013, 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/acpi.h>
#include "intel_soc_pmic_opregion.h"

#define PMIC_PMOP_OPREGION_ID   0x8d
#define PMIC_DPTF_OPREGION_ID   0x8c

struct acpi_lpat {
	int temp;
	int raw;
};

struct intel_soc_pmic_opregion {
	struct mutex lock;
	struct acpi_lpat *lpat;
	int lpat_count;
	struct intel_soc_pmic_opregion_data *data;
};

static struct pmic_pwr_reg *
pmic_get_pwr_reg(int address, struct pmic_pwr_table *table, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (table[i].address == address)
			return &table[i].pwr_reg;
	}
	return NULL;
}

static int
pmic_get_dptf_reg(int address, struct pmic_dptf_table *table, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (table[i].address == address)
			return table[i].reg;
	}
	return -ENOENT;
}

/* Return temperature from raw value through LPAT table */
static int raw_to_temp(struct acpi_lpat *lpat, int count, int raw)
{
	int i, delta_temp, delta_raw, temp;

	for (i = 0; i < count - 1; i++) {
		if ((raw >= lpat[i].raw && raw <= lpat[i+1].raw) ||
				(raw <= lpat[i].raw && raw >= lpat[i+1].raw))
			break;
	}

	if (i == count - 1)
		return -ENOENT;

	delta_temp = lpat[i+1].temp - lpat[i].temp;
	delta_raw = lpat[i+1].raw - lpat[i].raw;
	temp = lpat[i].temp + (raw - lpat[i].raw) * delta_temp / delta_raw;

	return temp;
}

/* Return raw value from temperature through LPAT table */
static int temp_to_raw(struct acpi_lpat *lpat, int count, int temp)
{
	int i, delta_temp, delta_raw, raw;

	for (i = 0; i < count - 1; i++) {
		if (temp >= lpat[i].temp && temp <= lpat[i+1].temp)
			break;
	}

	if (i == count - 1)
		return -ENOENT;

	delta_temp = lpat[i+1].temp - lpat[i].temp;
	delta_raw = lpat[i+1].raw - lpat[i].raw;
	raw = lpat[i].raw + (temp - lpat[i].temp) * delta_raw / delta_temp;

	return raw;
}

static void
pmic_dptf_lpat(struct intel_soc_pmic_opregion *opregion, acpi_handle handle,
		struct device *dev)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj_p, *obj_e;
	int *lpat, i;
	acpi_status status;

	status = acpi_evaluate_object(handle, "LPAT", NULL, &buffer);
	if (ACPI_FAILURE(status))
		return;

	obj_p = (union acpi_object *)buffer.pointer;
	if (!obj_p || (obj_p->type != ACPI_TYPE_PACKAGE) ||
		(obj_p->package.count % 2) || (obj_p->package.count < 4))
		goto out;

	lpat = devm_kmalloc(dev, sizeof(*lpat) * obj_p->package.count,
			GFP_KERNEL);
	if (!lpat)
		goto out;

	for (i = 0; i < obj_p->package.count; i++) {
		obj_e = &obj_p->package.elements[i];
		if (obj_e->type != ACPI_TYPE_INTEGER)
			goto out;
		lpat[i] = obj_e->integer.value;
	}

	opregion->lpat = (struct acpi_lpat *)lpat;
	opregion->lpat_count = obj_p->package.count / 2;

out:
	kfree(buffer.pointer);
}

static acpi_status
intel_soc_pmic_pmop_handler(u32 function, acpi_physical_address address,
				u32 bits, u64 *value64,
				void *handler_context, void *region_context)
{
	struct intel_soc_pmic_opregion *opregion = region_context;
	struct intel_soc_pmic_opregion_data *d = opregion->data;
	struct pmic_pwr_reg *preg;
	int result;

	if (bits != 32 || !value64)
		return AE_BAD_PARAMETER;

	if (function == ACPI_WRITE && !(*value64 == 0 || *value64 == 1))
		return AE_BAD_PARAMETER;

	preg = pmic_get_pwr_reg(address, d->pwr_table, d->pwr_table_count);
	if (!preg)
		return AE_BAD_PARAMETER;

	mutex_lock(&opregion->lock);

	if (function == ACPI_READ)
		result = d->get_power(preg, value64);
	else
		result = d->update_power(preg, *value64 == 1);

	mutex_unlock(&opregion->lock);

	return result ? AE_ERROR : AE_OK;
}

static acpi_status pmic_read_temp(struct intel_soc_pmic_opregion *opregion,
		int reg, u64 *value)
{
	int raw_temp, temp;

	if (!opregion->data->get_raw_temp)
		return AE_BAD_PARAMETER;

	raw_temp = opregion->data->get_raw_temp(reg);
	if (raw_temp < 0)
		return AE_ERROR;

	if (!opregion->lpat) {
		*value = raw_temp;
		return AE_OK;
	}

	temp = raw_to_temp(opregion->lpat, opregion->lpat_count, raw_temp);
	if (temp < 0) {
		pr_err("opregion: (LPAT) temp outside range!!\n");
		return AE_ERROR;
	}

	*value = temp;
	return AE_OK;
}

static acpi_status pmic_dptf_temp(struct intel_soc_pmic_opregion *opregion,
		int reg, u32 function, u64 *value)
{
	if (function != ACPI_READ)
		return AE_BAD_PARAMETER;

	return pmic_read_temp(opregion, reg, value);
}

static acpi_status pmic_dptf_aux(struct intel_soc_pmic_opregion *opregion,
		int reg, u32 function, u64 *value)
{
	int raw_temp;

	if (function == ACPI_READ)
		return pmic_read_temp(opregion, reg, value);

	if (!opregion->data->update_aux)
		return AE_BAD_PARAMETER;

	if (opregion->lpat) {
		raw_temp = temp_to_raw(opregion->lpat, opregion->lpat_count,
				*value);
		if (raw_temp < 0)
			return AE_ERROR;
	} else {
		raw_temp = *value;
	}

	return opregion->data->update_aux(reg, raw_temp) ?
		AE_ERROR : AE_OK;
}

static acpi_status pmic_dptf_pen(struct intel_soc_pmic_opregion *opregion,
		int reg, u32 function, u64 *value)
{
	struct intel_soc_pmic_opregion_data *d = opregion->data;

	if (!d->get_policy || !d->update_policy)
		return AE_BAD_PARAMETER;

	if (function == ACPI_READ)
		return d->get_policy(reg, value) ? AE_ERROR : AE_OK;

	if (*value != 0 || *value != 1)
		return AE_BAD_PARAMETER;

	return d->update_policy(reg, *value) ? AE_ERROR : AE_OK;
}

static bool pmic_dptf_is_temp(int address)
{
	return (address <= 0x3c) && !(address % 12);
}

static bool pmic_dptf_is_aux(int address)
{
	return (address >= 4 && address <= 0x40 && !((address - 4) % 12)) ||
		(address >= 8 && address <= 0x44 && !((address - 8) % 12));
}

static bool pmic_dptf_is_pen(int address)
{
	return address >= 0x48 && address <= 0x5c;
}

static acpi_status
intel_soc_pmic_dptf_handler(u32 function, acpi_physical_address address,
				u32 bits, u64 *value64,
				void *handler_context, void *region_context)
{
	struct intel_soc_pmic_opregion *opregion = region_context;
	int reg;
	int result;

	if (bits != 32 || !value64)
		return AE_BAD_PARAMETER;

	reg = pmic_get_dptf_reg(address, opregion->data->dptf_table,
			opregion->data->dptf_table_count);
	if (!reg)
		return AE_BAD_PARAMETER;

	mutex_lock(&opregion->lock);

	result = AE_BAD_PARAMETER;
	if (pmic_dptf_is_temp(address))
		result = pmic_dptf_temp(opregion, reg, function, value64);
	else if (pmic_dptf_is_aux(address))
		result = pmic_dptf_aux(opregion, reg, function, value64);
	else if (pmic_dptf_is_pen(address))
		result = pmic_dptf_pen(opregion, reg, function, value64);

	mutex_unlock(&opregion->lock);

	return result;
}

int
intel_soc_pmic_install_opregion_handler(struct device *dev,
			acpi_handle handle,
			struct intel_soc_pmic_opregion_data *d)
{
	acpi_status status;
	struct intel_soc_pmic_opregion *opregion;

	if (!dev || !d)
		return -EINVAL;

	if (!handle)
		return -ENODEV;

	opregion = devm_kzalloc(dev, sizeof(*opregion), GFP_KERNEL);
	if (!opregion)
		return -ENOMEM;

	mutex_init(&opregion->lock);
	pmic_dptf_lpat(opregion, handle, dev);

	status = acpi_install_address_space_handler(handle,
			PMIC_PMOP_OPREGION_ID,
			intel_soc_pmic_pmop_handler,
			NULL, opregion);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	status = acpi_install_address_space_handler(handle,
			PMIC_DPTF_OPREGION_ID,
			intel_soc_pmic_dptf_handler,
			NULL, opregion);
	if (ACPI_FAILURE(status)) {
		acpi_remove_address_space_handler(handle, PMIC_PMOP_OPREGION_ID,
				intel_soc_pmic_pmop_handler);
		return -ENODEV;
	}

	opregion->data = d;
	return 0;
}
EXPORT_SYMBOL_GPL(intel_soc_pmic_install_opregion_handler);

void intel_soc_pmic_remove_opregion_handler(acpi_handle handle)
{
	acpi_remove_address_space_handler(handle, PMIC_PMOP_OPREGION_ID,
			intel_soc_pmic_pmop_handler);
	acpi_remove_address_space_handler(handle, PMIC_DPTF_OPREGION_ID,
			intel_soc_pmic_dptf_handler);
}
EXPORT_SYMBOL_GPL(intel_soc_pmic_remove_opregion_handler);

MODULE_LICENSE("GPL");
