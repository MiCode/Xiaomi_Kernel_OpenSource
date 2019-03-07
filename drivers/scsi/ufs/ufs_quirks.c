/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ufshcd.h"
#include "ufs_quirks.h"

static struct ufs_card_fix ufs_fixups[] = {
	/* UFS cards deviations table */
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL, UFS_DEVICE_NO_VCCQ),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_NO_FASTAUTO),
	UFS_FIX(UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9C8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_TOSHIBA, "THGLF2G9D8KBADG",
		UFS_DEVICE_QUIRK_PA_TACTIVATE),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL, UFS_DEVICE_NO_VCCQ),
	UFS_FIX(UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL,
		UFS_DEVICE_QUIRK_WAIT_AFTER_REF_CLK_UNGATE),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hB8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hD8aL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8aM1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "h08aM1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8GL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),
	UFS_FIX(UFS_VENDOR_SKHYNIX, "hC8HL1",
		UFS_DEVICE_QUIRK_HS_G1_TO_HS_G3_SWITCH),

	END_FIX
};

void ufs_advertise_fixup_device(struct ufs_hba *hba)
{
	int err;
	u8 str_desc_buf[QUERY_DESC_MAX_SIZE + 1];
	char *model;
	struct ufs_card_fix *f;

	model = kmalloc(MAX_MODEL_LEN + 1, GFP_KERNEL);
	if (!model)
		goto out;

	memset(str_desc_buf, 0, QUERY_DESC_MAX_SIZE);
	err = ufshcd_read_string_desc(hba, hba->dev_info.i_product_name,
			str_desc_buf, QUERY_DESC_MAX_SIZE, ASCII_STD);
	if (err)
		goto out;

	str_desc_buf[QUERY_DESC_MAX_SIZE] = '\0';
	strlcpy(model, (str_desc_buf + QUERY_DESC_HDR_SIZE),
		min_t(u8, str_desc_buf[QUERY_DESC_LENGTH_OFFSET],
		      MAX_MODEL_LEN));
	/* Null terminate the model string */
	model[MAX_MODEL_LEN] = '\0';

	for (f = ufs_fixups; f->quirk; f++) {
		/* if same wmanufacturerid */
		if (((f->w_manufacturer_id ==
			hba->dev_info.w_manufacturer_id) ||
		     (f->w_manufacturer_id == UFS_ANY_VENDOR)) &&
		    /* and same model */
		    (STR_PRFX_EQUAL(f->model, model) ||
		     !strcmp(f->model, UFS_ANY_MODEL)))
			/* update quirks */
			hba->dev_info.quirks |= f->quirk;
	}
out:
	kfree(model);
}
