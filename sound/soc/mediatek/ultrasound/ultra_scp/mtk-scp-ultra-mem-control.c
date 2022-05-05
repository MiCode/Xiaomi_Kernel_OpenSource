// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.

#include "mtk-scp-ultra-mem-control.h"
#include "mtk-scp-ultra-common.h"
#include "mtk-base-afe.h"
#include "mtk-base-scp-ultra.h"
#include "mtk-afe-fe-dai.h"
#include <sound/soc.h>
#include <linux/device.h>
#include <linux/compat.h>
#include <linux/io.h>
#include "mtk-sram-manager.h"
#include "audio_ultra_msg_id.h"
#include "scp.h"
#include "mtk-scp-ultra.h"


int mtk_scp_ultra_reserved_dram_init(void)
{
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct audio_ultra_dram *ultra_resv_mem = &scp_ultra->ultra_reserve_dram;
	struct audio_ultra_dram *dump_resv_mem =
		&scp_ultra->ultra_dump.dump_resv_mem;

	ultra_resv_mem->phy_addr =
		scp_get_reserve_mem_phys(ULTRA_MEM_ID);
	ultra_resv_mem->vir_addr =
		(unsigned char *)scp_get_reserve_mem_virt(ULTRA_MEM_ID);
	ultra_resv_mem->size =
		scp_get_reserve_mem_size(ULTRA_MEM_ID);

	if (!ultra_resv_mem->phy_addr) {
		pr_err("%s(), scp ultra_resv_mem phy_addr error\n", __func__);
		return -1;
	}

	if (!ultra_resv_mem->vir_addr) {
		pr_err("%s(), scp ultra_resv_mem phy_addr vir_addr error\n", __func__);
		return -1;
	}

	if (!ultra_resv_mem->size) {
		pr_err("%s(), scp ultra_resv_mem phy_addr size error\n", __func__);
		return -1;
	}

	memset_io((void *)ultra_resv_mem->vir_addr, 0, ultra_resv_mem->size);

	dev_info(scp_ultra->dev,
		 "%s(), sce reserve mem pa=0x%llx, va=0x%llx, size=0x%llx\n",
		 __func__,
		 ultra_resv_mem->phy_addr,
		 ultra_resv_mem->vir_addr,
		 ultra_resv_mem->size);

	dump_resv_mem->phy_addr =
		scp_get_reserve_mem_phys(ULTRA_MEM_ID);
	dump_resv_mem->vir_addr =
		(unsigned char *)scp_get_reserve_mem_virt
				 (ULTRA_MEM_ID);
	dump_resv_mem->size =
		scp_get_reserve_mem_size(ULTRA_MEM_ID);

	if (!dump_resv_mem->phy_addr) {
		pr_err("%s(), dump_resv_mem phy_addr error\n", __func__);
		return -1;
	}
	if (!dump_resv_mem->vir_addr) {
		pr_err("%s(), dump_resv_mem vir_addr error\n", __func__);
		return -1;
	}
	if (!dump_resv_mem->size) {
		pr_err("%s(), dump_resv_mem size error\n", __func__);
		return -1;
	}
	dev_info(scp_ultra->dev,
		 "%s(), dump pa=0x%llx, va=0x%llx, size=0x%llx\n",
		 __func__,
		 dump_resv_mem->phy_addr,
		 dump_resv_mem->vir_addr,
		 dump_resv_mem->size);

	memset_io((void *)dump_resv_mem->vir_addr, 0, dump_resv_mem->size);

	return 0;
}

