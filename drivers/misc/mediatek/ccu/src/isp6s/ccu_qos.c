// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include "ccu_reg.h"
#include "ccu_cmn.h"
#include "ccu_hw.h"
#include "ccu_qos.h"
#ifdef CCU_QOS_SUPPORT_ENABLE
#include "mtk-interconnect.h"
#endif
static DEFINE_MUTEX(ccu_qos_mutex);

#define CCU_BW_I 60
#define CCU_BW_O 60
#define CCU_BW_G 15

void ccu_qos_init(struct ccu_device_s *ccu)
{
	mutex_lock(&ccu_qos_mutex);

	LOG_DBG_MUST("ccu qos init+");
#ifdef CCU_QOS_SUPPORT_ENABLE
	mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(CCU_BW_O), MBps_to_icc(CCU_BW_O));
	mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(CCU_BW_I), MBps_to_icc(CCU_BW_I));
	mtk_icc_set_bw(ccu->path_ccug, MBps_to_icc(CCU_BW_G), MBps_to_icc(CCU_BW_G));
#endif
	mutex_unlock(&ccu_qos_mutex);

}

void ccu_qos_update_req(struct ccu_device_s *ccu, uint32_t *ccu_bw)
{
#ifdef CCU_QOS_SUPPORT_ENABLE
	unsigned int i_request;
	unsigned int g_request;
	unsigned int o_request;
#endif

	mutex_lock(&ccu_qos_mutex);
#ifdef CCU_QOS_SUPPORT_ENABLE
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
	mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(o_request), MBps_to_icc(o_request));
	mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(i_request), MBps_to_icc(i_request));
	mtk_icc_set_bw(ccu->path_ccug, MBps_to_icc(g_request), MBps_to_icc(g_request));

	ccu_bw[0] = i_request;
	ccu_bw[1] = o_request;
	ccu_bw[2] = g_request;
#endif
	mutex_unlock(&ccu_qos_mutex);
}

void ccu_qos_uninit(struct ccu_device_s *ccu)
{
	mutex_lock(&ccu_qos_mutex);
#ifdef CCU_QOS_SUPPORT_ENABLE
	LOG_DBG_MUST("ccu qos uninit+");
	mtk_icc_set_bw(ccu->path_ccuo, MBps_to_icc(0), MBps_to_icc(0));
	mtk_icc_set_bw(ccu->path_ccui, MBps_to_icc(0), MBps_to_icc(0));
	mtk_icc_set_bw(ccu->path_ccug, MBps_to_icc(0), MBps_to_icc(0));
#endif
	mutex_unlock(&ccu_qos_mutex);
}
