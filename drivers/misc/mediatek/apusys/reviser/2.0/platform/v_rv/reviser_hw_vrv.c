// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/bitmap.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include "apusys_device.h"
#include "reviser_cmn.h"
#include "reviser_drv.h"
#include "reviser_hw_vrv.h"
#include "reviser_remote_cmd.h"



void reviser_print_rvr_exception(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;


	reviser_remote_print_hw_exception(drvinfo);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser exception\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "=============================\n");
	return;

}

void reviser_print_rvr_boundary(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	reviser_remote_print_hw_boundary(drvinfo);
	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser boundary\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "=============================\n");

}

void reviser_print_rvr_context_ID(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	reviser_remote_print_hw_ctx(drvinfo);

	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser ctx\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "=============================\n");

}

void reviser_print_rvr_remap_table(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	reviser_remote_print_hw_rmp_table(drvinfo);
	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser rmp_table\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "=============================\n");

}

void reviser_print_rvr_default_iova(void *drvinfo, void *s_file)
{
	struct reviser_dev_info *rdv = NULL;
	struct seq_file *s = (struct seq_file *)s_file;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	reviser_remote_print_hw_default_iova(drvinfo);
	LOG_CON(s, "=============================\n");
	LOG_CON(s, " reviser default iova\n");
	LOG_CON(s, "-----------------------------\n");
	LOG_CON(s, "=============================\n");
}







