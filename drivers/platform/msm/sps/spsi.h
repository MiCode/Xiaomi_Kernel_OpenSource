/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/**
 * Smart-Peripheral-Switch (SPS) internal API.
 */

#ifndef _SPSI_H_
#define _SPSI_H_

#include <linux/types.h>	/* u32 */
#include <linux/list.h>		/* list_head */
#include <linux/kernel.h>	/* pr_info() */
#include <linux/compiler.h>
#include <linux/ratelimit.h>

#include <mach/sps.h>

#include "sps_map.h"

#define BAM_MAX_PIPES              31
#define BAM_MAX_P_LOCK_GROUP_NUM   31

/* Adjust for offset of struct sps_q_event */
#define SPS_EVENT_INDEX(e)    ((e) - 1)
#define SPS_ERROR -1

/* BAM identifier used in log messages */
#define BAM_ID(dev)       ((dev)->props.phys_addr)

/* "Clear" value for the connection parameter struct */
#define SPSRM_CLEAR     0xcccccccc

extern u32 d_type;

#ifdef CONFIG_DEBUG_FS
extern u8 debugfs_record_enabled;
extern u8 logging_option;
extern u8 debug_level_option;
extern u8 print_limit_option;

#define MAX_MSG_LEN 80
#define SPS_DEBUGFS(msg, args...) do {					\
		char buf[MAX_MSG_LEN];		\
		snprintf(buf, MAX_MSG_LEN, msg"\n", ##args);	\
		sps_debugfs_record(buf);	\
	} while (0)
#define SPS_ERR(msg, args...) do {					\
		if (unlikely(print_limit_option > 2))	\
			pr_err_ratelimited(msg, ##args);	\
		else	\
			pr_err(msg, ##args);	\
		if (unlikely(debugfs_record_enabled))	\
			SPS_DEBUGFS(msg, ##args);	\
	} while (0)
#define SPS_INFO(msg, args...) do {					\
		if (unlikely(print_limit_option > 1))	\
			pr_info_ratelimited(msg, ##args);	\
		else	\
			pr_info(msg, ##args);	\
		if (unlikely(debugfs_record_enabled))	\
			SPS_DEBUGFS(msg, ##args);	\
	} while (0)
#define SPS_DBG(msg, args...) do {					\
		if ((unlikely(logging_option > 1))	\
			&& (unlikely(debug_level_option > 3))) {\
			if (unlikely(print_limit_option > 0))	\
				pr_info_ratelimited(msg, ##args);	\
			else	\
				pr_info(msg, ##args);	\
		} else	\
			pr_debug(msg, ##args);	\
		if (unlikely(debugfs_record_enabled))	\
			SPS_DEBUGFS(msg, ##args);	\
	} while (0)
#define SPS_DBG1(msg, args...) do {					\
		if ((unlikely(logging_option > 1))	\
			&& (unlikely(debug_level_option > 2))) {\
			if (unlikely(print_limit_option > 0))	\
				pr_info_ratelimited(msg, ##args);	\
			else	\
				pr_info(msg, ##args);	\
		} else	\
			pr_debug(msg, ##args);	\
		if (unlikely(debugfs_record_enabled))	\
			SPS_DEBUGFS(msg, ##args);	\
	} while (0)
#define SPS_DBG2(msg, args...) do {					\
		if ((unlikely(logging_option > 1))	\
			&& (unlikely(debug_level_option > 1))) {\
			if (unlikely(print_limit_option > 0))	\
				pr_info_ratelimited(msg, ##args);	\
			else	\
				pr_info(msg, ##args);	\
		} else	\
			pr_debug(msg, ##args);	\
		if (unlikely(debugfs_record_enabled))	\
			SPS_DEBUGFS(msg, ##args);	\
	} while (0)
#define SPS_DBG3(msg, args...) do {					\
		if ((unlikely(logging_option > 1))	\
			&& (unlikely(debug_level_option > 0))) {\
			if (unlikely(print_limit_option > 0))	\
				pr_info_ratelimited(msg, ##args);	\
			else	\
				pr_info(msg, ##args);	\
		} else	\
			pr_debug(msg, ##args);	\
		if (unlikely(debugfs_record_enabled))	\
			SPS_DEBUGFS(msg, ##args);	\
	} while (0)
#else
#define	SPS_DBG3(x...)		pr_debug(x)
#define	SPS_DBG2(x...)		pr_debug(x)
#define	SPS_DBG1(x...)		pr_debug(x)
#define	SPS_DBG(x...)		pr_debug(x)
#define	SPS_INFO(x...)		pr_info(x)
#define	SPS_ERR(x...)		pr_err(x)
#endif

/* End point parameters */
struct sps_conn_end_pt {
	u32 dev;		/* Device handle of BAM */
	u32 bam_phys;		/* Physical address of BAM. */
	u32 pipe_index;		/* Pipe index */
	u32 event_threshold;	/* Pipe event threshold */
	u32 lock_group;	/* The lock group this pipe belongs to */
	void *bam;
};

/* Connection bookkeeping descriptor struct */
struct sps_connection {
	struct list_head list;

	/* Source end point parameters */
	struct sps_conn_end_pt src;

	/* Destination end point parameters */
	struct sps_conn_end_pt dest;

	/* Resource parameters */
	struct sps_mem_buffer desc;	/* Descriptor FIFO */
	struct sps_mem_buffer data;	/* Data FIFO (BAM-to-BAM mode only) */
	u32 config;		/* Client specified connection configuration */

	/* Connection state */
	void *client_src;
	void *client_dest;
	int refs;		/* Reference counter */

	/* Dynamically allocated resouces, if required */
	u32 alloc_src_pipe;	/* Source pipe index */
	u32 alloc_dest_pipe;	/* Destination pipe index */
	u32 alloc_desc_base;	/* Physical address of descriptor FIFO */
	u32 alloc_data_base;	/* Physical address of data FIFO */
};

/* Event bookkeeping descriptor struct */
struct sps_q_event {
	struct list_head list;
	/* Event payload data */
	struct sps_event_notify notify;
};

/* Memory heap statistics */
struct sps_mem_stats {
	u32 base_addr;
	u32 size;
	u32 blocks_used;
	u32 bytes_used;
	u32 max_bytes_used;
};

#ifdef CONFIG_DEBUG_FS
/* record debug info for debugfs */
void sps_debugfs_record(const char *);
#endif

/* output the content of BAM-level registers */
void print_bam_reg(void *);

/* output the content of BAM pipe registers */
void print_bam_pipe_reg(void *, u32);

/* output the content of selected BAM-level registers */
void print_bam_selected_reg(void *);

/* output the content of selected BAM pipe registers */
void print_bam_pipe_selected_reg(void *, u32);

/* output descriptor FIFO of a pipe */
void print_bam_pipe_desc_fifo(void *, u32);

/* output BAM_TEST_BUS_REG */
void print_bam_test_bus_reg(void *, u32);

/**
 * Translate physical to virtual address
 *
 * This Function translates physical to virtual address.
 *
 * @phys_addr - physical address to translate
 *
 * @return virtual memory pointer
 *
 */
void *spsi_get_mem_ptr(u32 phys_addr);

/**
 * Allocate I/O (pipe) memory
 *
 * This function allocates target I/O (pipe) memory.
 *
 * @bytes - number of bytes to allocate
 *
 * @return physical address of allocated memory, or SPS_ADDR_INVALID on error
 */
u32 sps_mem_alloc_io(u32 bytes);

/**
 * Free I/O (pipe) memory
 *
 * This function frees target I/O (pipe) memory.
 *
 * @phys_addr - physical address of memory to free
 *
 * @bytes - number of bytes to free.
 */
void sps_mem_free_io(u32 phys_addr, u32 bytes);

/**
 * Find matching connection mapping
 *
 * This function searches for a connection mapping that matches the
 * parameters supplied by the client.  If a match is found, the client's
 * parameter struct is updated with the values specified in the mapping.
 *
 * @connect - pointer to client connection parameters
 *
 * @return 0 if match is found, negative value otherwise
 *
 */
int sps_map_find(struct sps_connect *connect);

/**
 * Allocate a BAM DMA pipe
 *
 * This function allocates a BAM DMA pipe, and is intended to be called
 * internally from the BAM resource manager.  Allocation implies that
 * the pipe has been referenced by a client Connect() and is in use.
 *
 * BAM DMA is permissive with activations, and allows a pipe to be allocated
 * with or without a client-initiated allocation.  This allows the client to
 * specify exactly which pipe should be used directly through the Connect() API.
 * sps_dma_alloc_chan() does not allow the client to specify the pipes/channel.
 *
 * @bam - pointer to BAM device descriptor
 *
 * @pipe_index - pipe index
 *
 * @dir - pipe direction
 *
 * @return 0 on success, negative value on error
 */
int sps_dma_pipe_alloc(void *bam, u32 pipe_index, enum sps_mode dir);

/**
 * Enable a BAM DMA pipe
 *
 * This function enables the channel associated with a BAM DMA pipe, and
 * is intended to be called internally from the BAM resource manager.
 * Enable must occur *after* the pipe has been enabled so that proper
 * sequencing between pipe and DMA channel enables can be enforced.
 *
 * @bam - pointer to BAM device descriptor
 *
 * @pipe_index - pipe index
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_dma_pipe_enable(void *bam, u32 pipe_index);

/**
 * Free a BAM DMA pipe
 *
 * This function disables and frees a BAM DMA pipe, and is intended to be
 * called internally from the BAM resource manager.  This must occur *after*
 * the pipe has been disabled/reset so that proper sequencing between pipe and
 * DMA channel resets can be enforced.
 *
 * @bam_arg - pointer to BAM device descriptor
 *
 * @pipe_index - pipe index
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_dma_pipe_free(void *bam, u32 pipe_index);

/**
 * Initialize driver memory module
 *
 * This function initializes the driver memory module.
 *
 * @pipemem_phys_base - Pipe-Memory physical base.
 *
 * @pipemem_size - Pipe-Memory size.
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_mem_init(u32 pipemem_phys_base, u32 pipemem_size);

/**
 * De-initialize driver memory module
 *
 * This function de-initializes the driver memory module.
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_mem_de_init(void);

/**
 * Initialize BAM DMA module
 *
 * This function initializes the BAM DMA module.
 *
 * @bam_props - pointer to BAM DMA devices BSP configuration properties
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_dma_init(const struct sps_bam_props *bam_props);

/**
 * De-initialize BAM DMA module
 *
 * This function de-initializes the SPS BAM DMA module.
 *
 */
void sps_dma_de_init(void);

/**
 * Initialize BAM DMA device
 *
 * This function initializes a BAM DMA device.
 *
 * @h - BAM handle
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_dma_device_init(u32 h);

/**
 * De-initialize BAM DMA device
 *
 * This function de-initializes a BAM DMA device.
 *
 * @h - BAM handle
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_dma_device_de_init(u32 h);

/**
 * Initialize connection mapping module
 *
 * This function initializes the SPS connection mapping module.
 *
 * @map_props - pointer to connection mapping BSP configuration properties
 *
 * @options - driver options bitflags (see SPS_OPT_*)
 *
 * @return 0 on success, negative value on error
 *
 */

int sps_map_init(const struct sps_map *map_props, u32 options);

/**
 * De-initialize connection mapping module
 *
 * This function de-initializes the SPS connection mapping module.
 *
 */
void sps_map_de_init(void);

#endif	/* _SPSI_H_ */
