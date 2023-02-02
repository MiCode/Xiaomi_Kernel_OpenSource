/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved..
 */
#include <linux/hwspinlock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <dt-bindings/soc/qcom,ipcc.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include "ipclite_client.h"

#define IPCMEM_INIT_COMPLETED		0x1
#define ACTIVE_CHANNEL			0x1

#define IPCMEM_TOC_SIZE			(4*1024)
#define MAX_CHANNEL_SIGNALS		6

#define MAX_PARTITION_COUNT		7	/*7 partitions other than global partition*/

#define IPCLITE_MSG_SIGNAL		0
#define IPCLITE_MEM_INIT_SIGNAL		1
#define IPCLITE_VERSION_SIGNAL		2
#define IPCLITE_TEST_SIGNAL		3
#define IPCLITE_SSR_SIGNAL		4
#define IPCLITE_DEBUG_SIGNAL		5

/** Flag definitions for the entries */
#define IPCMEM_TOC_ENTRY_FLAGS_ENABLE_READ_PROTECTION   (0x01)
#define IPCMEM_TOC_ENTRY_FLAGS_ENABLE_WRITE_PROTECTION  (0x02)
#define IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION \
		(IPCMEM_TOC_ENTRY_FLAGS_ENABLE_READ_PROTECTION | \
		IPCMEM_TOC_ENTRY_FLAGS_ENABLE_WRITE_PROTECTION)

#define IPCMEM_TOC_ENTRY_FLAGS_IGNORE_PARTITION         (0x00000004)

/*Hardcoded macro to identify local host on each core*/
#define LOCAL_HOST		IPCMEM_APPS

/* Timeout (ms) for the trylock of remote spinlocks */
#define HWSPINLOCK_TIMEOUT	1000

#define CHANNEL_INACTIVE		0
#define CHANNEL_ACTIVATE_IN_PROGRESS    1
#define CHANNEL_ACTIVE			2

#define CONFIGURED_CORE		1

#define IPCLITE_DEBUG_SIZE		(64 * 1024)
#define IPCLITE_DEBUG_INFO_SIZE		256
#define IPCLITE_CORE_DBG_LABEL		"APSS:"
#define IPCLITE_LOG_MSG_SIZE		100
#define IPCLITE_LOG_BUF_SIZE		512
#define IPCLITE_DBG_LABEL_SIZE		5
#define IPCLITE_SIGNAL_LABEL_SIZE	10
#define PREV_INDEX			2

#define IPCLITE_OS_LOG(__level, __fmt, arg...) \
	do { \
		if (ipclite_debug_level & __level) { \
			if (ipclite_debug_control & IPCLITE_DMESG_LOG) { \
				pr_info(IPCLITE_CORE_DBG_LABEL "%s:"__fmt, \
							ipclite_dbg_label[__level], ## arg); \
			} \
			if (ipclite_debug_control & IPCLITE_INMEM_LOG) { \
				IPCLITE_OS_INMEM_LOG(IPCLITE_CORE_DBG_LABEL "%s:"__fmt, \
							ipclite_dbg_label[__level], ## arg); \
			} \
		} \
	} while (0)

/*IPCMEM Structure Definitions*/

enum ipclite_debug_level {
	IPCLITE_ERR  = 0x0001,
	IPCLITE_WARN = 0x0002,
	IPCLITE_INFO = 0x0004,
	IPCLITE_DBG  = 0x0008,
};

enum ipclite_debug_control {
	IPCLITE_DMESG_LOG = 0x0001,
	IPCLITE_DBG_STRUCT = 0x0002,
	IPCLITE_INMEM_LOG = 0x0004,
};

enum ipclite_debug_dump {
	IPCLITE_DUMP_DBG_STRUCT = 0x0001,
	IPCLITE_DUMP_INMEM_LOG = 0x0002,
	IPCLITE_DUMP_SSR = 0x0004,
};

static const char ipclite_dbg_label[][IPCLITE_DBG_LABEL_SIZE] = {
	[IPCLITE_ERR] = "err",
	[IPCLITE_WARN] = "warn",
	[IPCLITE_INFO] = "info",
	[IPCLITE_DBG] = "dbg"
};

struct ipclite_debug_info_host {
	uint32_t numsig_sent; //no. of signals sent from the core
	uint32_t numsig_recv; //no. of signals received on the core
	uint32_t tx_wr_index; //write index of tx queue
	uint32_t tx_rd_index; //read index of tx queue
	uint32_t rx_wr_index; //write index of rx queue
	uint32_t rx_rd_index; //read index of rx queue
	uint32_t num_intr; //no. of interrupts received on the core
	uint32_t prev_tx_wr_index[PREV_INDEX]; //previous write index of tx queue
	uint32_t prev_tx_rd_index[PREV_INDEX]; //previous read index of tx queue
	uint32_t prev_rx_wr_index[PREV_INDEX]; //previous write index of rx queue
	uint32_t prev_rx_rd_index[PREV_INDEX]; //previous read index of rx queue
};

struct ipclite_debug_info_overall {
	uint32_t total_numsig_sent; //total no. of signals sent
	uint32_t total_numsig_recv; //total no. of signals received
	uint32_t last_sent_host_id; //last signal sent to host
	uint32_t last_recv_host_id; //last signal received from host
	uint32_t last_sigid_sent; //last sent signal id
	uint32_t last_sigid_recv; //last received signal id
};

struct ipclite_debug_info {
	uint32_t debug_version;
	uint32_t debug_level;
	uint32_t debug_control;
	uint32_t debug_dump;
	uint32_t debug_log_index;
};

struct ipclite_debug_inmem_buf {
	char IPCLITELog[IPCLITE_LOG_BUF_SIZE][IPCLITE_LOG_MSG_SIZE];
};

struct ipclite_debug_struct {
	struct ipclite_debug_info_overall dbg_info_overall;
	struct ipclite_debug_info_host dbg_info_host[IPCMEM_NUM_HOSTS];
};

struct ipclite_features {
	uint32_t global_atomic_support;
	uint32_t version_finalised;
};

struct ipclite_recover {
	uint32_t global_atomic_hwlock_owner;
	uint32_t configured_core[IPCMEM_NUM_HOSTS];
};

struct ipcmem_partition_header {
	uint32_t type;			   /*partition type*/
	uint32_t desc_offset;      /*descriptor offset*/
	uint32_t desc_size;        /*descriptor size*/
	uint32_t fifo0_offset;     /*fifo 0 offset*/
	uint32_t fifo0_size;       /*fifo 0 size*/
	uint32_t fifo1_offset;     /*fifo 1 offset*/
	uint32_t fifo1_size;       /*fifo 1 size*/
};

struct ipcmem_toc_entry {
	uint32_t base_offset;	/*partition offset from IPCMEM base*/
	uint32_t size;			/*partition size*/
	uint32_t flags;			/*partition flags if required*/
	uint32_t host0;			/*subsystem 0 who can access this partition*/
	uint32_t host1;			/*subsystem 1 who can access this partition*/
	uint32_t status;		/*partition active status*/
};

struct ipcmem_toc_header {
	uint32_t size;
	uint32_t init_done;
};

struct ipcmem_toc {
	struct ipcmem_toc_header hdr;
	struct ipcmem_toc_entry toc_entry_global;
	struct ipcmem_toc_entry toc_entry[IPCMEM_NUM_HOSTS][IPCMEM_NUM_HOSTS];
	/* Need to have a better implementation here */
	/* as ipcmem is 4k and if host number increases */
	/* it would create problems*/
	struct ipclite_features ipclite_features;
	struct ipclite_recover recovery;
};

struct ipcmem_region {
	u64 aux_base;
	void __iomem *virt_base;
	uint32_t size;
};

struct ipcmem_partition {
	struct ipcmem_partition_header hdr;
};

struct global_partition_header {
	uint32_t partition_type;
	uint32_t region_offset;
	uint32_t region_size;
};

struct ipcmem_global_partition {
	struct global_partition_header hdr;
};

struct ipclite_mem {
	struct ipcmem_toc *toc;
	struct ipcmem_region mem;
	struct ipcmem_global_partition *global_partition;
	struct ipcmem_partition *partition[MAX_PARTITION_COUNT];
};

struct ipclite_fifo {
	uint32_t length;

	__le32 *tail;
	__le32 *head;

	void *fifo;

	size_t (*avail)(struct ipclite_fifo *fifo);

	void (*peak)(struct ipclite_fifo *fifo,
				 void *data, size_t count);

	void (*advance)(struct ipclite_fifo *fifo,
				  size_t count, uint32_t core_id);

	void (*write)(struct ipclite_fifo *fifo,
		const void *data, size_t dlen, uint32_t core_id, uint32_t signal_id);

	void (*reset)(struct ipclite_fifo *fifo);
};

struct ipclite_hw_mutex_ops {
	unsigned long flags;
	void (*acquire)(void);
	void (*release)(void);
};

struct ipclite_irq_info {
	struct mbox_client mbox_client;
	struct mbox_chan *mbox_chan;
	int irq;
	int signal_id;
	char irqname[32];
};

struct ipclite_client {
	IPCLite_Client callback;
	void *priv_data;
	int reg_complete;
};

struct ipclite_channel {
	uint32_t remote_pid;

	struct ipclite_fifo *tx_fifo;
	struct ipclite_fifo *rx_fifo;
	spinlock_t tx_lock;

	struct ipclite_irq_info irq_info[MAX_CHANNEL_SIGNALS];

	struct ipclite_client client;

	uint32_t channel_version;
	uint32_t version_finalised;

	uint32_t channel_status;
};

/*Single structure that defines everything about IPCLite*/
struct ipclite_info {
	struct device *dev;
	struct ipclite_channel channel[IPCMEM_NUM_HOSTS];
	struct ipclite_mem ipcmem;
	struct hwspinlock *hwlock;
	struct ipclite_hw_mutex_ops *ipclite_hw_mutex;
};

const struct ipcmem_toc_entry ipcmem_toc_global_partition_entry = {
	/* Global partition. */
	  4 * 1024,
	  128 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_GLOBAL_HOST,
	  IPCMEM_GLOBAL_HOST,
};

const struct ipcmem_toc_entry ipcmem_toc_partition_entries[] = {
	/* Global partition. */
	/* {
	 *   4 * 1024,
	 *   128 * 1024,
	 *   IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	 *   IPCMEM_GLOBAL_HOST,
	 *   IPCMEM_GLOBAL_HOST,
	 * },
	 */

	/* Apps<->CDSP partition. */
	{
	  132 * 1024,
	  32 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_APPS,
	  IPCMEM_CDSP,
	  CHANNEL_INACTIVE,
	},
	/* APPS<->CVP (EVA) partition. */
	{
	  164 * 1024,
	  32 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_APPS,
	  IPCMEM_CVP,
	  CHANNEL_INACTIVE,
	},
	/* APPS<->VPU partition. */
	{
	  196 * 1024,
	  32 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_APPS,
	  IPCMEM_VPU,
	  CHANNEL_INACTIVE,
	},
	/* CDSP<->CVP (EVA) partition. */
	{
	  228 * 1024,
	  32 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_CDSP,
	  IPCMEM_CVP,
	  CHANNEL_INACTIVE,
	},
	/* CDSP<->VPU partition. */
	{
	  260 * 1024,
	  32 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_CDSP,
	  IPCMEM_VPU,
	  CHANNEL_INACTIVE,
	},
	/* VPU<->CVP (EVA) partition. */
	{
	  292 * 1024,
	  32 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_VPU,
	  IPCMEM_CVP,
	  CHANNEL_INACTIVE,
	},
	/* APPS<->APPS partition. */
	{
	  326 * 1024,
	  32 * 1024,
	  IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	  IPCMEM_APPS,
	  IPCMEM_APPS,
	  CHANNEL_INACTIVE,
	}
	/* Last entry uses invalid hosts and no protections to signify the end. */
	/* {
	 *   0,
	 *   0,
	 *   IPCMEM_TOC_ENTRY_FLAGS_ENABLE_RW_PROTECTION,
	 *   IPCMEM_INVALID_HOST,
	 *   IPCMEM_INVALID_HOST,
	 * }
	 */
};

/*D:wefault partition parameters*/
#define	DEFAULT_PARTITION_TYPE			0x0
#define	DEFAULT_PARTITION_HDR_SIZE		1024

#define	DEFAULT_DESCRIPTOR_OFFSET		1024
#define	DEFAULT_DESCRIPTOR_SIZE			(3*1024)
#define DEFAULT_FIFO0_OFFSET			(4*1024)
#define DEFAULT_FIFO0_SIZE				(8*1024)
#define DEFAULT_FIFO1_OFFSET			(12*1024)
#define DEFAULT_FIFO1_SIZE				(8*1024)

/*Loopback partition parameters*/
#define	LOOPBACK_PARTITION_TYPE			0x1

/*Global partition parameters*/
#define	GLOBAL_PARTITION_TYPE			0xFF
#define GLOBAL_PARTITION_HDR_SIZE		(4*1024)

#define GLOBAL_REGION_OFFSET			(4*1024)
#define GLOBAL_REGION_SIZE				(124*1024)


const struct ipcmem_partition_header default_partition_hdr = {
	DEFAULT_PARTITION_TYPE,
	DEFAULT_DESCRIPTOR_OFFSET,
	DEFAULT_DESCRIPTOR_SIZE,
	DEFAULT_FIFO0_OFFSET,
	DEFAULT_FIFO0_SIZE,
	DEFAULT_FIFO1_OFFSET,
	DEFAULT_FIFO1_SIZE,
};

/* TX and RX FIFO point to same location for such loopback partition type
 * (FIFO0 offset = FIFO1 offset)
 */
const struct ipcmem_partition_header loopback_partition_hdr = {
	LOOPBACK_PARTITION_TYPE,
	DEFAULT_DESCRIPTOR_OFFSET,
	DEFAULT_DESCRIPTOR_SIZE,
	DEFAULT_FIFO0_OFFSET,
	DEFAULT_FIFO0_SIZE,
	DEFAULT_FIFO0_OFFSET,
	DEFAULT_FIFO0_SIZE,
};

const struct global_partition_header global_partition_hdr = {
	GLOBAL_PARTITION_TYPE,
	GLOBAL_REGION_OFFSET,
	GLOBAL_REGION_SIZE,
};
