/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "hab.h"

/*
 * set valid mmid value in tbl to show this is valid entry. All inputs here are
 * normalized to 1 based integer
 */
static int fill_vmid_mmid_tbl(struct vmid_mmid_desc *tbl, int32_t vm_start,
				   int32_t vm_range, int32_t mmid_start,
				   int32_t mmid_range, int32_t be)
{
	int ret = 0;
	int i, j;

	for (i = vm_start; i < vm_start+vm_range; i++) {
		tbl[i].vmid = i; /* set valid vmid value to make it usable */
		for (j = mmid_start; j < mmid_start + mmid_range; j++) {
			/* sanity check */
			if (tbl[i].mmid[j] != HABCFG_VMID_INVALID) {
				pr_err("overwrite previous setting, i %d, j %d, be %d\n",
					i, j, tbl[i].is_listener[j]);
			}
			tbl[i].mmid[j] = j;
			tbl[i].is_listener[j] = be; /* BE IS listen */
		}
	}

	return ret;
}

void dump_settings(struct local_vmid *settings)
{
	int i, j;

	pr_debug("self vmid is %d\n", settings->self);
	for (i = 0; i < HABCFG_VMID_MAX; i++) {
		pr_debug("remote vmid %d\n",
			settings->vmid_mmid_list[i].vmid);
		for (j = 0; j <= HABCFG_MMID_AREA_MAX; j++) {
			pr_debug("mmid %d, is_be %d\n",
				settings->vmid_mmid_list[i].mmid[j],
				settings->vmid_mmid_list[i].is_listener[j]);
		}
	}
}

int fill_default_gvm_settings(struct local_vmid *settings, int vmid_local,
				  int mmid_start, int mmid_end) {
	settings->self = vmid_local;
	/* default gvm always talks to host as vm0 */
	return fill_vmid_mmid_tbl(settings->vmid_mmid_list, 0, 1,
		mmid_start/100, (mmid_end-mmid_start)/100+1, HABCFG_BE_FALSE);
}
