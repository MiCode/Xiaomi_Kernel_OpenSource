/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

#ifdef M4U_GZ_SERVICE_ENABLE
int m4u_gz_sec_init(int mtk_iommu_sec_id);
#endif
struct m4u_client_t *pseudo_get_m4u_client(void);
void pseudo_put_m4u_client(void);
int __pseudo_alloc_mva(struct m4u_client_t *client,
	int port, unsigned long va, unsigned long size,
	struct sg_table *sg_table, unsigned int flags,
	unsigned long *retmva);
int pseudo_dealloc_mva(struct m4u_client_t *client,
	int port, unsigned long mva);
struct device *pseudo_get_larbdev(int portid);
int larb_clock_on(int larb, bool config_mtcmos);
int larb_clock_off(int larb, bool config_mtcmos);
#if BITS_PER_LONG == 32
void m4u_find_max_port_size(unsigned long long base, unsigned long long max,
	unsigned int *err_port, unsigned int *err_size);
#else
void m4u_find_max_port_size(unsigned long base, unsigned long max,
	unsigned int *err_port, unsigned int *err_size);
#endif
void pseudo_m4u_bank_irq_debug(bool enable);

#endif

