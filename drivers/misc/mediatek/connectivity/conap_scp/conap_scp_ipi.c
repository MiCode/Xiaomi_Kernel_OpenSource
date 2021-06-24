// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/notifier.h>

#include "conap_scp.h"
#include "conap_scp_priv.h"
#include "conap_scp_shm.h"
#include "conap_scp_ipi.h"

#include "scp.h"
//#include "scp_ipi_pin.h"
//#include "scp_mbox_layout.h"
//#include "scp_helper.h"
#include "conap_platform_data.h"

struct ipi_data {
	uint16_t drv_type;
	uint16_t msg_id;
	uint32_t param0;
	uint32_t param1;
};

struct ipi_data g_ipi_data;
static char g_ipi_ack_data[12];

int scp_ctrl_event_handler(struct notifier_block *this,
	unsigned long event, void *ptr);

static struct notifier_block scp_ctrl_notifier = {
	.notifier_call = scp_ctrl_event_handler,
};

struct conap_scp_ipi_cb g_ipi_cb;

int scp_ctrl_event_handler(struct notifier_block *this,
	unsigned long event, void *ptr)
{

	switch(event) {
	case SCP_EVENT_STOP:
		pr_info("[%s] SCP STOP", __func__);
		if (g_ipi_cb.conap_scp_ipi_ctrl_notify)
			(*g_ipi_cb.conap_scp_ipi_ctrl_notify)(0);
		break;
	case SCP_EVENT_READY:
		pr_info("[%s] SCP READY", __func__);
		if (g_ipi_cb.conap_scp_ipi_ctrl_notify)
			(*g_ipi_cb.conap_scp_ipi_ctrl_notify)(1);
		break;
	default:
		pr_info("scp notify event error %lu", event);
	}
	return 0;
}


unsigned int conap_scp_ipi_is_scp_ready(void)
{
	return is_scp_ready(SCP_A_ID);
}

int ipi_recv_cb(unsigned int id, void *prdata, void *data, unsigned int len)
{
	struct ipi_data *idata;

	idata = (struct ipi_data*)data;
	if (g_ipi_cb.conap_scp_ipi_msg_notify)
		(*g_ipi_cb.conap_scp_ipi_msg_notify)(idata->drv_type, idata->msg_id, idata->param0, idata->param1);

	return 0;
}

int conap_scp_ipi_send(enum conap_scp_drv_type drv_type, uint16_t msg_id, uint32_t param0, uint32_t param1)
{
	unsigned int retry_time = 500;
	unsigned int retry_cnt = 0;
	int ipi_result = -1;

	if (!conap_scp_ipi_is_scp_ready()) {
		pr_warn("[%s] spc status fail", __func__);
		return -1;
	}
	g_ipi_data.drv_type = drv_type;
	g_ipi_data.msg_id = msg_id;
	g_ipi_data.param0 = param0;
	g_ipi_data.param1 = param1;

	for (retry_cnt = 0; retry_cnt <= retry_time; retry_cnt++) {
		ipi_result = mtk_ipi_send(&scp_ipidev,
							IPI_OUT_SCP_CONNSYS,
							0,
							&g_ipi_data,
							PIN_OUT_SIZE_SCP_CONNSYS,
							0);
		if (ipi_result == IPI_ACTION_DONE)
			break;
		msleep(1);
	}
	if (ipi_result != 0) {
		pr_err("[%s] ipi fail=[%d]", __func__, ipi_result);
		return -1;
	}

	return 0;
}


int conap_scp_ipi_is_drv_ready(enum conap_scp_drv_type drv_type)
{
	int ret;

	ret = conap_scp_ipi_send(DRV_TYPE_CORE, CONAP_SCP_CORE_DRV_QRY, drv_type, 0);

	return ret;
}

int conap_scp_ipi_handshake(void)
{
	pr_info("[%s]", __func__);
	conap_scp_ipi_send(DRV_TYPE_CORE, CONAP_SCP_CORE_INIT,
							connsys_scp_shm_get_addr(),
							connsys_scp_shm_get_size());
	return 0;
}

int conap_scp_ipi_init(struct conap_scp_ipi_cb *cb)
{
	int ret = IPI_ACTION_DONE;

	memcpy(&g_ipi_cb, cb, sizeof(struct conap_scp_ipi_cb));

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_CONNSYS,
					(void *) ipi_recv_cb, NULL, &g_ipi_ack_data[0]);
	if (ret != IPI_ACTION_DONE) {
		pr_info("[%s] ipi_register ret=[%d]", __func__, ret);
		return -1;
	}

	scp_A_register_notify(&scp_ctrl_notifier);

	return 0;
}

int conap_scp_ipi_deinit(void)
{
	int ret = 0;

	ret = mtk_ipi_unregister(&scp_ipidev, IPI_IN_SCP_CONNSYS);
	pr_info("[%s] ipi_unregister ret=[%d]", __func__, ret);
	return ret;
}
