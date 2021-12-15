/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "ccu_qos.h"
#include "ccu_cmn.h"
#include "ccu_hw.h"
#include <mmdvfs_pmqos.h>
#include "mt6885-larb-port.h"
static PLIST_HEAD(ccu_request_list);
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

	/*Add request for dram input, output and single access*/
	mm_qos_add_request(&ccu_request_list, &pccu_i_request,
		M4U_PORT_L13_CAM_CCUI_MDP);
	mm_qos_add_request(&ccu_request_list, &pccu_g_request,
		get_virtual_port(VIRTUAL_CCU_COMMON));
	mm_qos_add_request(&ccu_request_list, &pccu_o_request,
		M4U_PORT_L13_CAM_CCUO_MDP);

	mm_qos_set_request(&pccu_i_request, CCU_BW_I, CCU_BW_I, BW_COMP_NONE);
	mm_qos_set_request(&pccu_g_request, CCU_BW_G, CCU_BW_G, BW_COMP_NONE);
	mm_qos_set_request(&pccu_o_request, CCU_BW_O, CCU_BW_O, BW_COMP_NONE);

	mm_qos_update_all_request(&ccu_request_list);
	mutex_unlock(&ccu_qos_mutex);

}

void ccu_qos_update_req(uint32_t *ccu_bw)
{
	unsigned int i_request;
	unsigned int g_request;
	unsigned int o_request;

	mutex_lock(&ccu_qos_mutex);

	i_request = ccu_read_reg(ccu_base, SPREG_12_CCU_BW_I_REG);
	g_request = ccu_read_reg(ccu_base, SPREG_13_CCU_BW_O_REG);
	o_request = ccu_read_reg(ccu_base, SPREG_14_CCU_BW_G_REG);

	if ((i_request > CCU_BW_I) ||
	(o_request > CCU_BW_O) ||
	(g_request > CCU_BW_G)) {
		LOG_DBG_MUST("ccu qos update out+ i(%d) o(%d) g(%d)",
		i_request, o_request, g_request);

		i_request = CCU_BW_I;
		g_request = CCU_BW_G;
		o_request = CCU_BW_O;
	}

	LOG_DBG("ccu qos update+ i(%d) o(%d) g(%d)",
		i_request, o_request, g_request);
	mm_qos_set_request(&pccu_i_request, i_request, i_request, BW_COMP_NONE);
	mm_qos_set_request(&pccu_g_request, g_request, g_request, BW_COMP_NONE);
	mm_qos_set_request(&pccu_o_request, o_request, o_request, BW_COMP_NONE);

	ccu_bw[0] = i_request;
	ccu_bw[1] = o_request;
	ccu_bw[2] = g_request;
	if (!plist_head_empty(&ccu_request_list))
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
