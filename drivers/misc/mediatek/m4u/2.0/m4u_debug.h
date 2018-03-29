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

extern unsigned long gM4U_ProtectVA;

extern __attribute__((weak)) int ddp_mem_test(void);
extern __attribute__((weak)) int __ddp_mem_test(unsigned int *pSrc, unsigned int pSrcPa,
			    unsigned int *pDst, unsigned int pDstPa,
			    int need_sync);

#ifdef M4U_TEE_SERVICE_ENABLE
extern int m4u_sec_init(void);
extern int m4u_config_port_tee(M4U_PORT_STRUCT *pM4uPort);
#endif
#endif
