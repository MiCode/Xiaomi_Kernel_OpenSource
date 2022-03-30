/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_PLATFORM_DATA_H__
#define __CONAP_SCP_PLATFORM_DATA_H__


struct conap_scp_shm_config {
	uint32_t conap_scp_shm_offset;
	uint32_t conap_scp_shm_size;
	uint32_t conap_scp_ipi_mbox_size;
};


int connsys_scp_platform_data_init(unsigned int chip_info, phys_addr_t emi_phy_addr);
struct conap_scp_shm_config *conap_scp_get_shm_info(void);

/* connsys share buffer */
uint32_t connsys_scp_shm_get_addr(void);
uint32_t connsys_scp_shm_get_size(void);

uint32_t connsys_scp_ipi_mbox_size(void);

#endif
