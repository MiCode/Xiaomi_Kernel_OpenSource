/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <mmdvfs_pmqos.h>
#include "smi_port.h"
#include "ccu_qos.h"
#include "ccu_cmn.h"
#include "ccu_hw.h"
static struct plist_head ccu_request_list;
static struct mm_qos_request pccu_i_request;
static struct mm_qos_request pccu_g_request;
static struct mm_qos_request pccu_o_request;
static DEFINE_MUTEX(ccu_qos_mutex);

#define CCU_BW_I 60
#define CCU_BW_O 60
#define CCU_BW_G 15

void ccu_qos_init(void)
{
	mutex_lock(&ccu_qos_mutex);

	LOG_DBG_MUST("ccu qos init+");

	/*Initialize request list*/
	plist_head_init(&ccu_request_list);

	/*Add request for dram input, output and single access*/
	mm_qos_add_request(&ccu_request_list, &pccu_i_request, SMI_CCUI);
	mm_qos_add_request(&ccu_request_list, &pccu_g_request,
		get_virtual_port(VIRTUAL_CCU_COMMON));
	mm_qos_add_request(&ccu_request_list, &pccu_o_request, SMI_CCUO);

	mm_qos_set_request(&pccu_i_request, CCU_BW_I, CCU_BW_I, BW_COMP_NONE);
	mm_qos_set_request(&pccu_g_request, CCU_BW_G, CCU_BW_G, BW_COMP_NONE);
	mm_qos_set_request(&pccu_o_request, CCU_BW_O, CCU_BW_O, BW_COMP_NONE);

	mm_qos_update_all_request(&ccu_request_list);
	mutex_unlock(&ccu_qos_mutex);

}

void ccu_qos_uninit(void)
{
	mutex_lock(&ccu_qos_mutex);
	LOG_DBG_MUST("ccu qos uninit+");
	mm_qos_update_all_request_zero(&ccu_request_list);
	mm_qos_remove_all_request(&ccu_request_list);
	mutex_unlock(&ccu_qos_mutex);
}
