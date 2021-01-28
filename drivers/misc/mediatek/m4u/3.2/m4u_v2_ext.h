/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __M4U_V2_EXT_H__
#define __M4U_V2_EXT_H__

#define M4U_PROT_READ	(1<<0)  /* buffer can be read by engine */
#define M4U_PROT_WRITE	(1<<1)  /* buffer can be write by engine */
/* buffer access will goto CCI to do cache snoop */
#define M4U_PROT_CACHE	(1<<2)
/* buffer access will goto CCI, but don't do cache snoop*/
/*(just for engines who wants to use CCI bandwidth) */
#define M4U_PROT_SHARE	(1<<3)
/* buffer can only be accessed by secure engine. */
#define M4U_PROT_SEC    (1<<4)

/* public flags */
/* engine access this buffer in sequncial way. */
#define M4U_FLAGS_SEQ_ACCESS (1<<0)
/* fix allocation, we will use mva user specified. */
#define M4U_FLAGS_FIX_MVA   (1<<1)
#define M4U_FLAGS_SEC_SHAREABLE   (1<<2)  /* the mva will share in SWd */
/* the allocator will search free mva from user specified.  */
#define M4U_FLAGS_START_FROM   (1<<3)
/* ion_alloc have allocated sg_table with m4u_create_sgtable.
 *For va2mva ion case
 */
#define M4U_FLAGS_SG_READY   (1<<4)


/* m4u internal flags (DO NOT use them for other purpers) */
#define M4U_FLAGS_MVA_IN_FREE (1<<8) /* this mva is in deallocating. */


enum M4U_RANGE_PRIORITY_ENUM {
	RT_RANGE_HIGH_PRIORITY = 0,
	SEQ_RANGE_LOW_PRIORITY = 1
};


/* port related: virtuality, security, distance */
struct M4U_PORT_STRUCT {
	int ePortID; /* hw port ID, defined in M4U_PORT_ID */
	unsigned int Virtuality;
	unsigned int Security;
	unsigned int domain;            /* domain : 0 1 2 3 */
	unsigned int Distance;
	unsigned int Direction;         /* 0:- 1:+ */
};


enum M4U_CACHE_SYNC_ENUM {
	M4U_CACHE_CLEAN_BY_RANGE,
	M4U_CACHE_INVALID_BY_RANGE,
	M4U_CACHE_FLUSH_BY_RANGE,

	M4U_CACHE_CLEAN_ALL,
	M4U_CACHE_INVALID_ALL,
	M4U_CACHE_FLUSH_ALL,
};

enum M4U_DMA_TYPE {
	M4U_DMA_MAP_AREA,
	M4U_DMA_UNMAP_AREA,
	M4U_DMA_FLUSH_BY_RANGE,
};

enum M4U_DMA_DIR {
	M4U_DMA_FROM_DEVICE,
	M4U_DMA_TO_DEVICE,
	M4U_DMA_BIDIRECTIONAL,
};

struct m4u_client_t {
    /* mutex to protect mvaList */
    /* should get this mutex whenever add/delete/interate mvaList */
	struct mutex dataMutex;
	pid_t open_pid;
	pid_t open_tgid;
	struct list_head mvaList;
};

struct port_mva_info_t {
	int module_id;
	unsigned long va;
	unsigned int BufSize;
	int security;
	int cache_coherent;
	unsigned int flags;
	unsigned int iova_start;
	unsigned int iova_end;
	unsigned int mva;
};

struct sg_table *m4u_create_sgtable(unsigned long va, unsigned int size);

int m4u_alloc_mva_sg(struct port_mva_info_t *port_info,
		struct sg_table *sg_table);

int m4u_dealloc_mva_sg(int module_id,
		       struct sg_table *sg_table,
		       const unsigned int BufSize, const unsigned int MVA);
int m4u_config_port_ext(struct M4U_PORT_STRUCT *pM4uPort);
int m4u_mva_map_kernel(unsigned int mva, unsigned int size,
		       unsigned long *map_va, unsigned int *map_size);
int m4u_mva_unmap_kernel(unsigned int mva,
	unsigned int size, unsigned long va);

void m4u_call_atf_debug(int m4u_debug_id);

enum m4u_callback_ret_t {
	M4U_CALLBACK_HANDLED,
	M4U_CALLBACK_NOT_HANDLED,
};

typedef enum m4u_callback_ret_t(m4u_fault_callback_t) (int port,
		unsigned int mva, void *data);
int m4u_register_fault_callback(int port, m4u_fault_callback_t *fn,
				void *data);
int m4u_unregister_fault_callback(int port);
int m4u_enable_tf(int port, bool fgenable);

#endif
