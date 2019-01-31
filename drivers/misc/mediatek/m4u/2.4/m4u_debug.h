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

#ifndef __M4U_DEBUG_H__
#define __M4U_DEBUG_H__

#ifdef M4U_TEE_SERVICE_ENABLE
extern int m4u_sec_init(void);
extern int m4u_config_port_tee(M4U_PORT_STRUCT *pM4uPort);
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
#endif
