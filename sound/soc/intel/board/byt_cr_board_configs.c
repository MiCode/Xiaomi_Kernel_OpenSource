/*
 *  byt_cr_board_configs.h - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2014 Intel Corporation
 *  Authors:	Ola Lilja <ola.lilja@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  Board-specific hardware-configurations
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/dmi.h>

#include "byt_cr_board_configs.h"

static const struct mach_codec_link *mc_link;

static acpi_status sst_acpi_mach_match(acpi_handle handle, u32 level,
				       void *context, void **ret)
{
	*(bool *)context = true;
	return AE_OK;
}

static bool acpi_id_exists(const char *id)
{
	bool found = false;
	acpi_status status;

	status = acpi_get_devices(id, sst_acpi_mach_match, &found, NULL);
	return ACPI_SUCCESS(status) && found;
}

int set_mc_link(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mach_codec_links); i++)
		if (acpi_id_exists(mach_codec_links[i].codec_hid)) {
			pr_info("%s: %s-codec found! Registering machine-device %s.\n",
				__func__, mach_codec_links[i].codec_hid,
				mach_codec_links[i].mach_dev->name);
			platform_device_register(mach_codec_links[i].mach_dev);
			mc_link = &mach_codec_links[i];
			break;
		}
	if (i == ARRAY_SIZE(mach_codec_links)) {
		pr_err("%s: Unknown or no codec found!\n", __func__);
		return -ENOENT;
	}

	return 0;
}

const struct mach_codec_link *get_mc_link(void)
{
	if (!mc_link)
		if (set_mc_link())
			return NULL;

	return mc_link;
}

const struct board_config *get_board_config(
				const struct mach_codec_link *mc_link)
{
	const struct dmi_system_id *dmi;

	dmi = dmi_first_match(mc_link->dmi_system_ids);
	if (!dmi) {
		pr_info("Board is a default %s.\n", mc_link->mach_dev->name);
		return mc_link->board_config_default;
	}

	pr_info("%s: Board is a %s (%s)\n", __func__, dmi->ident,
		mc_link->mach_dev->name);

	return (const struct board_config *)dmi->driver_data;
}
