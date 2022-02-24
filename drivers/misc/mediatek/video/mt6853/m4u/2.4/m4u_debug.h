// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __M4U_DEBUG_H__
#define __M4U_DEBUG_H__

extern unsigned long gM4U_ProtectVA;

/* extern int __attribute__((weak)) ddp_mem_test(void); */
extern int __attribute__((weak)) __ddp_mem_test(
		unsigned int *pSrc, unsigned int pSrcPa,
			    unsigned int *pDst, unsigned int pDstPa,
			    int need_sync);

#ifdef M4U_TEE_SERVICE_ENABLE
extern int m4u_sec_init(void);
extern int m4u_config_port_tee(
	struct M4U_PORT_STRUCT *pM4uPort);
#endif
/*verify m4u 2.4*/
extern void m4u_mvaGraph_dump(void);
extern void test_case_check_mva_region(void);
extern void test_case_m4u_do_mva_alloc(void);
extern void test_case_m4u_do_mva_alloc_fix(void);
extern void test_case_m4u_do_mva_alloc_start_from(void);
extern void test_case_m4u_do_mva_free(void);
extern void test_dummy(void);
extern void test_m4u_do_mva_alloc_stage3(void);
extern void test_m4u_do_mva_alloc_start_from_V2p4(void);
extern void test_m4u_do_mva_alloc_start_from_V2p4_case1(void);
#endif
