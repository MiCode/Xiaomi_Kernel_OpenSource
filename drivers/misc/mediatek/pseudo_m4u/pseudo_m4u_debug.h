/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __PSEUDO_M4U_DEBUG_H__
#define __PSEUDO_M4U_DEBUG_H__

extern int __attribute__((weak)) m4u_display_fake_engine_test(
	unsigned long mva_rd, unsigned long mva_wr);
extern int __attribute__((weak)) __ddp_mem_test(unsigned long *pSrc,
	unsigned long pSrcPa,
	unsigned long *pDst, unsigned long pDstPa,
	int need_sync);

#ifdef M4U_TEE_SERVICE_ENABLE
extern int m4u_sec_init(void);
extern int m4u_config_port_tee(struct M4U_PORT_STRUCT *pM4uPort);
#endif

struct m4u_client_t *pseudo_get_m4u_client(void);
int __pseudo_alloc_mva(struct m4u_client_t *client,
	int port, unsigned long va, unsigned long size,
	struct sg_table *sg_table, unsigned int flags,
	unsigned long *retmva);
int pseudo_dealloc_mva(struct m4u_client_t *client,
	int port, unsigned long mva);
struct device *pseudo_get_larbdev(int portid);
int larb_clock_on(int larb, bool config_mtcmos);
int larb_clock_off(int larb, bool config_mtcmos);

#endif

