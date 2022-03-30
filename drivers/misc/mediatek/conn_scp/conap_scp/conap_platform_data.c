// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/io.h>
#include "conap_platform_data.h"

/* 6893 */
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6893)

struct conap_scp_shm_config g_adp_shm_mt6893 = {
	.conap_scp_shm_offset = 0x1E0000,
	.conap_scp_shm_size = 0x50000,
	.conap_scp_ipi_mbox_size = 40,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6983)
struct conap_scp_shm_config g_adp_shm_mt6983 = {
	.conap_scp_shm_offset = 0x7E0000,
	.conap_scp_shm_size = 0x20000,
	.conap_scp_ipi_mbox_size = 64,
};
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6895)
struct conap_scp_shm_config g_adp_shm_mt6895 = {
	.conap_scp_shm_offset = 0x7E0000,
	.conap_scp_shm_size = 0x20000,
	.conap_scp_ipi_mbox_size = 64,
};
#endif


uint32_t g_plt_chip_info;
phys_addr_t g_emi_phy_base;
struct conap_scp_shm_config *g_adp_shm_ptr;

uint32_t connsys_scp_shm_get_addr(void)
{
	if (g_adp_shm_ptr == NULL)
		return 0;
	return (g_emi_phy_base + g_adp_shm_ptr->conap_scp_shm_offset) & 0xFFFFFFFF;
}

uint32_t connsys_scp_shm_get_size(void)
{
	if (g_adp_shm_ptr == NULL)
		return 0;
	return g_adp_shm_ptr->conap_scp_shm_size;
}

struct conap_scp_shm_config *conap_scp_get_shm_info(void)
{
	return g_adp_shm_ptr;
}


uint32_t connsys_scp_ipi_mbox_size(void)
{
	if (g_adp_shm_ptr == NULL)
		return 0;
	return g_adp_shm_ptr->conap_scp_ipi_mbox_size;
}


int connsys_scp_platform_data_init(unsigned int chip_info, phys_addr_t emi_phy_addr)
{
	g_plt_chip_info = chip_info;
	g_emi_phy_base = emi_phy_addr;

#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6893)
	if (chip_info == 0x6893) {
		g_adp_shm_ptr = &g_adp_shm_mt6893;
		return 0;
	}
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6983)
	if (chip_info == 0x6983) {
		g_adp_shm_ptr = &g_adp_shm_mt6983;
		return 0;
	}
#endif
#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6895)
	if (chip_info == 0x6895) {
		g_adp_shm_ptr = &g_adp_shm_mt6895;
		return 0;
	}

#endif

	pr_info("[%s] chip=[%x] not support", __func__, chip_info);
	return -1;
}
