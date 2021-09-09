// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include "mtk_drm_drv.h"
#include "scp.h"
#include "mtk_log.h"

struct aod_scp_ipi_receive_info {
	unsigned int aod_id;
};

static struct aod_scp_ipi_receive_info aod_scp_msg;

int mtk_aod_scp_ipi_send(int value)
{
	unsigned int retry_cnt = 0;
	unsigned int aod_scp_send_data = 1;
	int ret;

	DDPMSG("%s+\n", __func__);

	for (retry_cnt = 0; retry_cnt <= 10; retry_cnt++) {
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_SCP_AOD,
					0, &aod_scp_send_data, 1, 0);

		if (ret == IPI_ACTION_DONE) {
			DDPMSG("%s ipi send msg done\n", __func__);
			break;
		}
	}

	if (ret != IPI_ACTION_DONE)
		DDPMSG("%s ipi send msg fail:%d\n", __func__, ret);

	DDPMSG("%s-\n", __func__);

	return ret;
}

static int mtk_aod_scp_recv_handler(unsigned int id, void *prdata, void *data, unsigned int len)
{
	DDPMSG("%s\n", __func__);

	return 0;
}

static int mtk_aod_scp_ipi_register(void)
{
	int ret;

	DDPMSG("%s+\n", __func__);

	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_AOD,
			(void *)mtk_aod_scp_recv_handler, NULL, &aod_scp_msg);

	if (ret != IPI_ACTION_DONE)
		DDPMSG("%s resigter ipi fail: %d\n", __func__, ret);
	else
		DDPMSG("%s register ipi done\n", __func__);

	DDPMSG("%s-\n", __func__);

	return ret;
}

static int __init mtk_aod_scp_init(void)
{
	struct device_node *aod_scp_node = NULL;
	void **ret;

	DDPMSG("%s+\n", __func__);

	aod_scp_node = of_find_node_by_name(NULL, "AOD_SCP_ON");
	if (aod_scp_node) {
		mtk_aod_scp_ipi_register();
		ret = mtk_aod_scp_ipi_init();
		*ret = (void *) mtk_aod_scp_ipi_send;

		DDPMSG("w/ mtk_aod_scp_ipi_register\n");
	}	else
		DDPMSG("w/o mtk_aod_scp_ipi_register\n");

	DDPMSG("%s-\n", __func__);

	return 0;
}

static void __exit mtk_aod_scp_exit(void)
{
	DDPMSG("%s+\n", __func__);

	DDPMSG("%s-\n", __func__);
}

module_init(mtk_aod_scp_init);
module_exit(mtk_aod_scp_exit);

MODULE_AUTHOR("Ahsin Chen <ahsin.chen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek AOD-SCP");
MODULE_LICENSE("GPL v2");
