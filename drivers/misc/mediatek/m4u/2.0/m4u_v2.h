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

#ifndef __M4U_V2_H__
#define __M4U_V2_H__
#include <linux/ioctl.h>
#include <linux/fs.h>
#include "m4u_port.h"
#include <linux/scatterlist.h>

typedef int M4U_PORT_ID;

#define M4U_PROT_READ	(1<<0)  /* buffer can be read by engine */
#define M4U_PROT_WRITE	(1<<1)  /* buffer can be write by engine */
#define M4U_PROT_CACHE	(1<<2)  /* buffer access will goto CCI to do cache snoop */
#define M4U_PROT_SHARE	(1<<3)  /* buffer access will goto CCI, but don't do cache snoop
								(just for engines who wants to use CCI bandwidth) */
#define M4U_PROT_SEC    (1<<4)  /* buffer can only be accessed by secure engine. */

/* public flags */
#define M4U_FLAGS_SEQ_ACCESS (1<<0) /* engine access this buffer in sequncial way. */
#define M4U_FLAGS_FIX_MVA   (1<<1)  /* fix allocation, we will use mva user specified. */
#define M4U_FLAGS_SEC_SHAREABLE   (1<<2)  /* the mva will share in SWd */

/* m4u internal flags (DO NOT use them for other purpers) */
#define M4U_FLAGS_MVA_IN_FREE (1<<8) /* this mva is in deallocating. */


typedef enum {
	RT_RANGE_HIGH_PRIORITY = 0,
	SEQ_RANGE_LOW_PRIORITY = 1
} M4U_RANGE_PRIORITY_ENUM;


/* port related: virtuality, security, distance */
typedef struct _M4U_PORT {
	M4U_PORT_ID ePortID;		   /* hardware port ID, defined in M4U_PORT_ID */
	unsigned int Virtuality;
	unsigned int Security;
	unsigned int domain;            /* domain : 0 1 2 3 */
	unsigned int Distance;
	unsigned int Direction;         /* 0:- 1:+ */
} M4U_PORT_STRUCT;

struct m4u_port_array {
	#define M4U_PORT_ATTR_EN		(1<<0)
	#define M4U_PORT_ATTR_VIRTUAL	(1<<1)
	#define M4U_PORT_ATTR_SEC		(1<<2)
	unsigned char ports[M4U_PORT_NR];
};


typedef enum {
	M4U_CACHE_CLEAN_BY_RANGE,
	M4U_CACHE_INVALID_BY_RANGE,
	M4U_CACHE_FLUSH_BY_RANGE,

	M4U_CACHE_CLEAN_ALL,
	M4U_CACHE_INVALID_ALL,
	M4U_CACHE_FLUSH_ALL,
} M4U_CACHE_SYNC_ENUM;

typedef enum {
	M4U_DMA_MAP_AREA,
	M4U_DMA_UNMAP_AREA,
	M4U_DMA_FLUSH_BY_RANGE,
} M4U_DMA_TYPE;

typedef enum {
	M4U_DMA_FROM_DEVICE,
	M4U_DMA_TO_DEVICE,
	M4U_DMA_BIDIRECTIONAL,
} M4U_DMA_DIR;

typedef struct {
    /* mutex to protect mvaList */
    /* should get this mutex whenever add/delete/interate mvaList */
	struct mutex dataMutex;
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head mvaList;
} m4u_client_t;

int m4u_dump_info(int m4u_index);
int m4u_power_on(int m4u_index);
int m4u_power_off(int m4u_index);

int m4u_alloc_mva(m4u_client_t *client, M4U_PORT_ID port,
					unsigned long va, struct sg_table *sg_table,
					unsigned int size, unsigned int prot, unsigned int flags,
					unsigned int *pMva);

int m4u_dealloc_mva(m4u_client_t *client, M4U_PORT_ID port, unsigned int mva);

int m4u_alloc_mva_sg(int eModuleID,
						struct sg_table *sg_table,
						const unsigned int BufSize,
						int security,
						int cache_coherent,
						unsigned int *pRetMVABuf);

int m4u_dealloc_mva_sg(int eModuleID,
						struct sg_table *sg_table,
						const unsigned int BufSize,
						const unsigned int MVA);

int m4u_config_port(M4U_PORT_STRUCT *pM4uPort);
int m4u_config_port_array(struct m4u_port_array *port_array);
int m4u_monitor_start(int m4u_id);
int m4u_monitor_stop(int m4u_id);

int m4u_cache_sync(m4u_client_t *client, M4U_PORT_ID port,
					unsigned long va, unsigned int size, unsigned int mva,
					M4U_CACHE_SYNC_ENUM sync_type);

int m4u_mva_map_kernel(unsigned int mva, unsigned int size,
		unsigned long *map_va, unsigned int *map_size);
int m4u_mva_unmap_kernel(unsigned int mva, unsigned int size, unsigned long va);
m4u_client_t *m4u_create_client(void);
int m4u_destroy_client(m4u_client_t *client);

int m4u_dump_reg_for_smi_hang_issue(void);
int m4u_display_fake_engine_test(unsigned long ulFakeReadAddr, unsigned long ulFakeWriteAddr);

void m4u_larb_backup(int larb_idx);
void m4u_larb_restore(int larb_idx);

typedef enum m4u_callback_ret {
	M4U_CALLBACK_HANDLED,
	M4U_CALLBACK_NOT_HANDLED,
} m4u_callback_ret_t;

typedef m4u_callback_ret_t (m4u_reclaim_mva_callback_t)(int alloc_port, unsigned int mva,
							unsigned int size, void *data);
int m4u_register_reclaim_callback(int port, m4u_reclaim_mva_callback_t *fn, void *data);
int m4u_unregister_reclaim_callback(int port);

typedef m4u_callback_ret_t (m4u_fault_callback_t)(int port, unsigned int mva, void *data);
int m4u_register_fault_callback(int port, m4u_fault_callback_t *fn, void *data);
int m4u_unregister_fault_callback(int port);

#ifdef CONFIG_PM
extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);
#endif

#ifdef M4U_TEE_SERVICE_ENABLE
extern int gM4U_L2_enable;
#endif

extern void show_pte(struct mm_struct *mm, unsigned long addr);

#ifdef M4U_PROFILE
extern void MMProfileEnable(int enable);
extern void MMProfileStart(int start);
extern MMP_Event M4U_MMP_Events[M4U_MMP_MAX];
#endif

#ifndef M4U_FPGAPORTING
extern void smp_inner_dcache_flush_all(void);
#endif
/* m4u driver internal use --------------------------------------------------- */
/*  */

#ifdef CONFIG_MTK_CACHE_FLUSH_RANGE_PARALLEL
int mt_smp_cache_flush_m4u(const void *va, const unsigned long size);
#endif

#endif
