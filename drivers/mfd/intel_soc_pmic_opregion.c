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

#include <linux/acpi.h>
#include <linux/mfd/intel_soc_pmic.h>
#include "intel_soc_pmic_core.h"

struct pmic_table {
	int address;	/* operation region address */
	int reg;	/* corresponding PMIC register */
};

static struct pmic_table pwr_regmap[] = {
	{ 0x04, 0x63 }, /* SYSX -> VSYS_SX */
	{ 0x08, 0x62 }, /* SYSU -> VSYS_U */
	{ 0x0c, 0x64 }, /* SYSS -> VSYS_S */
	{ 0x10, 0x6a }, /* V50S -> V5P0S */
	{ 0x14, 0x6b }, /* HOST -> VHOST, USB2/3 host */
	{ 0x18, 0x6c }, /* VBUS -> VBUS, USB2/3 OTG */
	{ 0x1c, 0x6d }, /* HDMI -> VHDMI */
	/*0x20, S285 */
	{ 0x24, 0x66 }, /* X285 -> V2P85SX, camara */
	/*0x28, V33A */
	{ 0x2c, 0x69 }, /* V33S -> V3P3S, display/ssd/audio */
	{ 0x30, 0x68 }, /* V33U -> V3P3U, SDIO wifi&bt */
	/*0x34, V33I */
	/*0x38, V18A */
	/*0x3c, REFQ */
	/*0x40, V12A */
	{ 0x44, 0x5c }, /* V18S -> V1P8S, SOC/USB PHY/SIM */
	{ 0x48, 0x5d }, /* V18X -> V1P8SX, eMMC/camara/audio */
	{ 0x4c, 0x5b }, /* V18U -> V1P8U, LPDDR */
	{ 0x50, 0x61 }, /* V12X -> V1P2SX, SOC SFR */
	{ 0x54, 0x60 }, /* V12S -> V1P2S, MIPI */
	/*0x58, V10A */
	{ 0x5c, 0x56 }, /* V10S -> V1P0S, SOC GFX */
	{ 0x60, 0x57 }, /* V10X -> V1P0SX, SOC display/DDR IO/PCIe */
	{ 0x64, 0x59 }, /* V105 -> V1P05S, L2 SRAM */
};

static struct pmic_table dptf_regmap[] = {
	{ 0x00, 0x75 }, /* TMP0 -> SYS0_THRM_RSLT_L */
	{ 0x04, 0x95 }, /* AX00 -> SYS0_THRMALRT0_L */
	{ 0x08, 0x97 }, /* AX01 -> SYS0_THRMALRT1_L */
	{ 0x0c, 0x77 }, /* TMP1 -> SYS1_THRM_RSLT_L */
	{ 0x10, 0x9a }, /* AX10 -> SYS1_THRMALRT0_L */
	{ 0x14, 0x9c }, /* AX11 -> SYS1_THRMALRT1_L */
	{ 0x18, 0x79 }, /* TMP2 -> SYS2_THRM_RSLT_L */
	{ 0x1c, 0x9f }, /* AX20 -> SYS2_THRMALRT0_L */
	{ 0x20, 0xa1 }, /* AX21 -> SYS2_THRMALRT1_L */
	{ 0x24, 0x7b }, /* TMP3 -> BAT0_THRM_RSLT_L */
	{ 0x28, 0xa4 }, /* AX30 -> BAT0_THRMALRT0_L */
	{ 0x2c, 0xa6 }, /* AX31 -> BAT0_THRMALRT1_L */
	{ 0x30, 0x7d }, /* TMP4 -> BAT1_THRM_RSLT_L */
	{ 0x34, 0xaa }, /* AX40 -> BAT1_THRMALRT0_L */
	{ 0x38, 0xac }, /* AX41 -> BAT1_THRMALRT1_L */
	{ 0x3c, 0x7f }, /* TMP5 -> PMIC_THRM_RSLT_L */
	{ 0x40, 0xb0 }, /* AX50 -> PMIC_THRMALRT0_L */
	{ 0x44, 0xb2 }, /* AX51 -> PMIC_THRMALRT1_L */
	{ 0x48, 0x94 }, /* PEN0 -> SYS0_THRMALRT0_H */
	{ 0x4c, 0x99 }, /* PEN1 -> SYS1_THRMALRT1_H */
	{ 0x50, 0x9e }, /* PEN2 -> SYS2_THRMALRT2_H */
	{ 0x54, 0xa3 }, /* PEN3 -> BAT0_THRMALRT0_H */
	{ 0x58, 0xa9 }, /* PEN4 -> BAT1_THRMALRT0_H */
	{ 0x5c, 0xaf }, /* PEN5 -> PMIC_THRMALRT0_H */
};

static int pmic_opregion_readb(int reg)
{
	int ret = intel_soc_pmic_readb(reg);
	if (ret < 0)
		dev_err(intel_soc_pmic_dev(), "read reg 0x%x failed, 0x%x\n",
			reg, ret);
	return ret;
}

static int pmic_opregion_writeb(int reg, u8 value)
{
	int ret = intel_soc_pmic_writeb(reg, value);
	if (ret < 0)
		dev_err(intel_soc_pmic_dev(), "write reg 0x%x failed, 0x%x\n",
			reg, ret);
	return ret;
}

static int pmic_get_reg(int address, struct pmic_table *regmap, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		if (regmap[i].address == address)
			return regmap[i].reg;
	}
	return -1;
}

static acpi_status
pmic_power_space_handler(u32 function, acpi_physical_address address,
			 u32 bits, u64 *value64,
			 void *handler_context, void *region_context)
{
	int pmic_reg, ret;

	if (bits != 32 || !value64)
		return AE_BAD_PARAMETER;

	if (function == ACPI_WRITE && !(*value64 == 0 || *value64 == 1))
		return AE_BAD_PARAMETER;

	pmic_reg = pmic_get_reg(address, pwr_regmap, ARRAY_SIZE(pwr_regmap));
	if (pmic_reg == -1) {
		dev_err(intel_soc_pmic_dev(),
			"opregion address 0x%x not supported\n", (int)address);
		return AE_BAD_PARAMETER;
	}

	ret = pmic_opregion_readb(pmic_reg);
	if (ret < 0)
		return AE_BAD_DATA;

	if (function == ACPI_READ) {
		*value64 = ret & 0x1;
	} else {
		if (*value64 == 1)
			ret |= 0x3;
		else {
			ret &= ~0x3;
			ret |= 0x2;
		}

		ret = pmic_opregion_writeb(pmic_reg, ret);
		if (ret < 0)
			return AE_BAD_DATA;
	}

	return AE_OK;
}

/* Return temperature from raw value through LPAT table */
static int acpi_lpat_tmp(struct acpi_lpat *lpat, int count, int raw)
{
	int i, delta_tmp, delta_raw, tmp;

	for (i = 0; i < count - 1; i++) {
		if ((raw >= lpat[i].raw && raw <= lpat[i+1].raw) ||
		    (raw <= lpat[i].raw && raw >= lpat[i+1].raw))
			break;
	}

	if (i == count - 1) {
		dev_err(intel_soc_pmic_dev(), "Not in LPAT range\n");
		return -1;
	}

	delta_tmp = lpat[i+1].tmp - lpat[i].tmp;
	delta_raw = lpat[i+1].raw - lpat[i].raw;
	tmp = lpat[i].tmp + (raw - lpat[i].raw) * delta_tmp / delta_raw;

	return tmp;
}

/* Return raw value from temperature through LPAT table */
static int acpi_lpat_raw(struct acpi_lpat *lpat, int count, int tmp)
{
	int i, delta_tmp, delta_raw, raw;

	for (i = 0; i < count - 1; i++) {
		if (tmp >= lpat[i].tmp && tmp <= lpat[i+1].tmp)
			break;
	}

	if (i == count - 1) {
		dev_err(intel_soc_pmic_dev(), "Not in LPAT range\n");
		return -1;
	}

	delta_tmp = lpat[i+1].tmp - lpat[i].tmp;
	delta_raw = lpat[i+1].raw - lpat[i].raw;
	raw = lpat[i].raw + (tmp - lpat[i].tmp) * delta_raw / delta_tmp;

	return raw;
}

static struct intel_soc_pmic *intel_soc_pmic(void)
{
	return &crystal_cove_pmic;
}

static int pmic_read_tmp(int pmic_reg, u64 *value)
{
	struct intel_pmic_opregion *opregion = intel_soc_pmic()->opregion;
	int tmp_l, tmp_h, raw_tmp, tmp;

	tmp_l = pmic_opregion_readb(pmic_reg);
	tmp_h = pmic_opregion_readb(pmic_reg - 1);
	if (tmp_l < 0 || tmp_h < 0)
		return -1;

	raw_tmp = tmp_l | ((tmp_h & 0x3) << 8);

	if (!opregion->lpat) {
		*value = raw_tmp;
		return 0;
	}

	tmp = acpi_lpat_tmp(opregion->lpat, opregion->lpat_count, raw_tmp);
	if (tmp == -1)
		return -1;
	*value = tmp;
	return 0;
}

static acpi_status pmic_dptf_tmp(int pmic_reg, u32 function, u64 *value)
{
	if (function != ACPI_READ)
		return AE_BAD_PARAMETER;

	if (pmic_read_tmp(pmic_reg, value) == -1)
		return AE_ERROR;

	return AE_OK;
}

static acpi_status pmic_dptf_aux(int pmic_reg, u32 function, u64 *value)
{
	struct intel_pmic_opregion *opregion;
	int tmp, raw, aux_h, ret;

	if (function == ACPI_READ)
		return pmic_read_tmp(pmic_reg, value) == -1 ? AE_ERROR : AE_OK;

	if (function == ACPI_WRITE) {
		opregion = intel_soc_pmic()->opregion;
		tmp = *value;
		raw = acpi_lpat_raw(opregion->lpat, opregion->lpat_count, tmp);
		if (raw == -1)
			return AE_ERROR;
		ret = pmic_opregion_writeb(pmic_reg, raw & 0xff);
		if (ret < 0)
			return AE_ERROR;
		aux_h = pmic_opregion_readb(pmic_reg - 1);
		if (aux_h < 0)
			return AE_ERROR;
		aux_h &= ~0x3;
		aux_h |= (raw >> 8) & 0x3;
		ret = pmic_opregion_writeb(pmic_reg - 1, aux_h);
		if (ret < 0)
			return AE_ERROR;
		return AE_OK;
	}

	return AE_ERROR;
}

static acpi_status pmic_dptf_pen(int pmic_reg, u32 function, u64 *value)
{
	int pen, alert0, alert0_orig;

	if (function == ACPI_READ) {
		pen = pmic_opregion_readb(pmic_reg);
		if (pen < 0)
			return AE_ERROR;
		*value = pen >> 7;
		return AE_OK;
	}

	if (function == ACPI_WRITE) {
		if (*value != 0 || *value != 1)
			return AE_BAD_PARAMETER;
		pen = pmic_opregion_readb(pmic_reg);
		if (pen < 0)
			return AE_ERROR;
		/* unlock a0lock */
		alert0_orig = pmic_opregion_readb(0xc5);
		alert0 = alert0_orig;
		alert0 &= ~0x1;
		if (pmic_opregion_writeb(0xc5, alert0) < 0)
			return AE_ERROR;
		pen &= 0x7f;
		pen |= *value << 7;
		if (pmic_opregion_writeb(pmic_reg, pen) < 0)
			return AE_ERROR;
		/* restore alert0 */
		if (pmic_opregion_writeb(0xc5, alert0_orig) < 0)
			return AE_ERROR;
		return AE_OK;
	}

	return AE_ERROR;
}

static bool pmic_dptf_is_tmp(int address)
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
pmic_dptf_space_handler(u32 function, acpi_physical_address address,
			u32 bits, u64 *value64,
			void *handler_context, void *region_context)
{
	int pmic_reg;

	if (bits != 32 || !value64)
		return AE_BAD_PARAMETER;

	pmic_reg = pmic_get_reg(address, dptf_regmap, ARRAY_SIZE(dptf_regmap));
	if (pmic_reg == -1) {
		dev_err(intel_soc_pmic_dev(),
			"opregion address 0x%x not supported\n", (int)address);
		return AE_BAD_PARAMETER;
	}

	if (pmic_dptf_is_tmp(address))
		return pmic_dptf_tmp(pmic_reg, function, value64);
	else if (pmic_dptf_is_aux(address))
		return pmic_dptf_aux(pmic_reg, function, value64);
	else if (pmic_dptf_is_pen(address))
		return pmic_dptf_pen(pmic_reg, function, value64);
	return AE_BAD_PARAMETER;
}

static acpi_handle pmic_handle(void)
{
	return ACPI_HANDLE(intel_soc_pmic_dev());
}

static void pmic_dptf_lpat(void)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct intel_pmic_opregion *opregion;
	union acpi_object *obj_p, *obj_e;
	int *lpat, i;
	acpi_status status;

	opregion = devm_kmalloc(intel_soc_pmic_dev(), sizeof(*opregion),
				GFP_KERNEL);
	if (!opregion) {
		dev_err(intel_soc_pmic_dev(), "No mem for opregion\n");
		return;
	}
	intel_soc_pmic()->opregion = opregion;

	if (!acpi_has_method(pmic_handle(), "LPAT")) {
		opregion->lpat = NULL;
		return;
	}

	status = acpi_evaluate_object(pmic_handle(), "LPAT", NULL, &buffer);
	if (ACPI_FAILURE(status)) {
		dev_err(intel_soc_pmic_dev(), "evaluate LPAT failed\n");
		return;
	}
	obj_p = (union acpi_object *)buffer.pointer;
	if (!obj_p || (obj_p->type != ACPI_TYPE_PACKAGE) ||
	    (obj_p->package.count % 2) || (obj_p->package.count < 4)) {
		dev_err(intel_soc_pmic_dev(), "Invalid LPAT data\n");
		goto err;
	}

	lpat = devm_kmalloc(intel_soc_pmic_dev(),
			    sizeof(*lpat) * obj_p->package.count, GFP_KERNEL);
	if (!lpat) {
		dev_err(intel_soc_pmic_dev(), "No mem for lpat\n");
		goto err;
	}

	for (i = 0; i < obj_p->package.count; i++) {
		obj_e = &obj_p->package.elements[i];
		if (obj_e->type != ACPI_TYPE_INTEGER) {
			dev_err(intel_soc_pmic_dev(), "LPAT invalid data\n");
			goto err;
		}
		lpat[i] = obj_e->integer.value;
	}

	opregion->lpat = (struct acpi_lpat *)lpat;
	opregion->lpat_count = obj_p->package.count / 2;

err:
	kfree(buffer.pointer);
}

void intel_pmic_install_handlers(struct intel_soc_pmic *pmic)
{
	acpi_status status;
	if (!pmic_handle())
		return;

	pmic_dptf_lpat();

	status = acpi_install_address_space_handler(pmic_handle(),
						    0x8d,
						    &pmic_power_space_handler,
						    NULL, NULL);
	if (ACPI_FAILURE(status))
		dev_err(intel_soc_pmic_dev(),
			"install power space handler failed 0x%x\n", status);

	status = acpi_install_address_space_handler(pmic_handle(),
						    0x8c,
						    &pmic_dptf_space_handler,
						    NULL, NULL);
	if (ACPI_FAILURE(status))
		dev_err(intel_soc_pmic_dev(),
			"install DPTF space handler failed 0x%x\n", status);
}
