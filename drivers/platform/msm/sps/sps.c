/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

/* Smart-Peripheral-Switch (SPS) Module. */

#include <linux/types.h>	/* u32 */
#include <linux/kernel.h>	/* pr_info() */
#include <linux/module.h>	/* module_init() */
#include <linux/slab.h>		/* kzalloc() */
#include <linux/mutex.h>	/* mutex */
#include <linux/device.h>	/* device */
#include <linux/fs.h>		/* alloc_chrdev_region() */
#include <linux/list.h>		/* list_head */
#include <linux/memory.h>	/* memset */
#include <linux/io.h>		/* ioremap() */
#include <linux/clk.h>		/* clk_enable() */
#include <linux/platform_device.h>	/* platform_get_resource_byname() */
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <mach/msm_sps.h>	/* msm_sps_platform_data */

#include "sps_bam.h"
#include "spsi.h"
#include "sps_core.h"

#define SPS_DRV_NAME "msm_sps"	/* must match the platform_device name */

/**
 *  SPS Driver state struct
 */
struct sps_drv {
	struct class *dev_class;
	dev_t dev_num;
	struct device *dev;
	struct clk *pmem_clk;
	struct clk *bamdma_clk;
	struct clk *dfab_clk;

	int is_ready;

	/* Platform data */
	u32 pipemem_phys_base;
	u32 pipemem_size;
	u32 bamdma_bam_phys_base;
	u32 bamdma_bam_size;
	u32 bamdma_dma_phys_base;
	u32 bamdma_dma_size;
	u32 bamdma_irq;
	u32 bamdma_restricted_pipes;

	/* Driver options bitflags (see SPS_OPT_*) */
	u32 options;

	/* Mutex to protect BAM and connection queues */
	struct mutex lock;

	/* BAM devices */
	struct list_head bams_q;

	char *hal_bam_version;

	/* Connection control state */
	struct sps_rm connection_ctrl;
};


/**
 *  SPS driver state
 */
static struct sps_drv *sps;

static void sps_device_de_init(void);

#ifdef CONFIG_DEBUG_FS
#define MAX_OUTPUT_MAGIC_NUM 777
u32 sps_debugfs_enabled;
u32 detailed_debug_on;
static char *debugfs_buf;
static int debugfs_buf_size;
static int debugfs_buf_used;
static int wraparound;

/* record debug info for debugfs */
void sps_debugfs_record(const char *msg)
{
	if (sps_debugfs_enabled) {
		if (debugfs_buf_used + MAX_MSG_LEN >= debugfs_buf_size) {
			debugfs_buf_used = 0;
			wraparound = true;
		}
		debugfs_buf_used += scnprintf(debugfs_buf + debugfs_buf_used,
				debugfs_buf_size - debugfs_buf_used, msg);

		if (wraparound)
			scnprintf(debugfs_buf + debugfs_buf_used,
					debugfs_buf_size - debugfs_buf_used,
					"\n**** end line of sps log ****\n\n");
	}
}

/* read the recorded debug info to userspace */
static ssize_t sps_read_info(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int ret;
	int size;

	if (wraparound)
		size = debugfs_buf_size - MAX_MSG_LEN;
	else
		size = debugfs_buf_used;

	ret = simple_read_from_buffer(ubuf, count, ppos,
			debugfs_buf, size);

	return ret;
}

/*
 * set the buffer size (in KB) for debug info
 * if input is 0, then stop recording debug info into buffer
 */
static ssize_t sps_set_info(struct file *file, const char __user *buf,
				 size_t count, loff_t *ppos)
{
	unsigned long missing;
	static char str[5];
	int i, buf_size_kb = 0;

	memset(str, 0, sizeof(str));
	missing = copy_from_user(str, buf, sizeof(str));
	if (missing)
		return -EFAULT;

	for (i = 0; i < sizeof(str) && (str[i] >= '0') && (str[i] <= '9'); ++i)
		buf_size_kb = (buf_size_kb * 10) + (str[i] - '0');

	pr_info("sps:debugfs buffer size is %dKB\n", buf_size_kb);

	if (sps_debugfs_enabled && (buf_size_kb == 0)) {
		sps_debugfs_enabled = false;
		detailed_debug_on = false;
		kfree(debugfs_buf);
		debugfs_buf = NULL;
		debugfs_buf_used = 0;
		debugfs_buf_size = 0;
		wraparound = false;
	} else if (!sps_debugfs_enabled && (buf_size_kb > 0)) {
		debugfs_buf_size = buf_size_kb * SZ_1K;

		debugfs_buf = kzalloc(sizeof(char) * debugfs_buf_size,
				GFP_KERNEL);
		if (!debugfs_buf) {
			debugfs_buf_size = 0;
			pr_err("sps:fail to allocate memory for debug_fs.\n");
			return -ENOMEM;
		}

		if (buf_size_kb == MAX_OUTPUT_MAGIC_NUM)
			detailed_debug_on = true;
		sps_debugfs_enabled = true;
		debugfs_buf_used = 0;
		wraparound = false;
	} else if (sps_debugfs_enabled && (buf_size_kb > 0))
		pr_info("sps:should disable debugfs before change "
				"buffer size.\n");

	return sps_debugfs_enabled;
}

const struct file_operations sps_info_ops = {
	.read = sps_read_info,
	.write = sps_set_info,
};

struct dentry *dent;
struct dentry *dfile;
static void sps_debugfs_init(void)
{
	sps_debugfs_enabled = false;
	detailed_debug_on = false;
	debugfs_buf_size = 0;
	debugfs_buf_used = 0;
	wraparound = false;

	dent = debugfs_create_dir("sps", 0);
	if (IS_ERR(dent)) {
		pr_err("sps:fail to create the folder for debug_fs.\n");
		return;
	}

	dfile = debugfs_create_file("info", 0444, dent, 0,
			&sps_info_ops);
	if (!dfile || IS_ERR(dfile)) {
		pr_err("sps:fail to create the file for debug_fs.\n");
		debugfs_remove(dent);
		return;
	}
}

static void sps_debugfs_exit(void)
{
	if (dfile)
		debugfs_remove(dfile);
	if (dent)
		debugfs_remove(dent);
	kfree(debugfs_buf);
	debugfs_buf = NULL;
}
#endif

/**
 * Initialize SPS device
 *
 * This function initializes the SPS device.
 *
 * @return 0 on success, negative value on error
 *
 */
static int sps_device_init(void)
{
	int result;
	int success;
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	struct sps_bam_props bamdma_props = {0};
#endif

	SPS_DBG("sps_device_init");

	success = false;

	result = sps_mem_init(sps->pipemem_phys_base, sps->pipemem_size);
	if (result) {
		SPS_ERR("SPS memory init failed");
		goto exit_err;
	}

	INIT_LIST_HEAD(&sps->bams_q);
	mutex_init(&sps->lock);

	if (sps_rm_init(&sps->connection_ctrl, sps->options)) {
		SPS_ERR("Failed to init SPS resource manager");
		goto exit_err;
	}

	result = sps_bam_driver_init(sps->options);
	if (result) {
		SPS_ERR("SPS BAM driver init failed");
		goto exit_err;
	}

	/* Initialize the BAM DMA device */
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	bamdma_props.phys_addr = sps->bamdma_bam_phys_base;
	bamdma_props.virt_addr = ioremap(sps->bamdma_bam_phys_base,
					 sps->bamdma_bam_size);

	if (!bamdma_props.virt_addr) {
		SPS_ERR("sps:Failed to IO map BAM-DMA BAM registers.\n");
		goto exit_err;
	}

	SPS_DBG("sps:bamdma_bam.phys=0x%x.virt=0x%x.",
		bamdma_props.phys_addr,
		(u32) bamdma_props.virt_addr);

	bamdma_props.periph_phys_addr =	sps->bamdma_dma_phys_base;
	bamdma_props.periph_virt_size = sps->bamdma_dma_size;
	bamdma_props.periph_virt_addr = ioremap(sps->bamdma_dma_phys_base,
						sps->bamdma_dma_size);

	if (!bamdma_props.periph_virt_addr) {
		SPS_ERR("sps:Failed to IO map BAM-DMA peripheral reg.\n");
		goto exit_err;
	}

	SPS_DBG("sps:bamdma_dma.phys=0x%x.virt=0x%x.",
		bamdma_props.periph_phys_addr,
		(u32) bamdma_props.periph_virt_addr);

	bamdma_props.irq = sps->bamdma_irq;

	bamdma_props.event_threshold = 0x10;	/* Pipe event threshold */
	bamdma_props.summing_threshold = 0x10;	/* BAM event threshold */

	bamdma_props.options = SPS_BAM_OPT_BAMDMA;
	bamdma_props.restricted_pipes =	sps->bamdma_restricted_pipes;

	result = sps_dma_init(&bamdma_props);
	if (result) {
		SPS_ERR("SPS BAM DMA driver init failed");
		goto exit_err;
	}
#endif /* CONFIG_SPS_SUPPORT_BAMDMA */

	result = sps_map_init(NULL, sps->options);
	if (result) {
		SPS_ERR("SPS connection mapping init failed");
		goto exit_err;
	}

	success = true;
exit_err:
	if (!success) {
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
		sps_device_de_init();
#endif
		return SPS_ERROR;
	}

	return 0;
}

/**
 * De-initialize SPS device
 *
 * This function de-initializes the SPS device.
 *
 * @return 0 on success, negative value on error
 *
 */
static void sps_device_de_init(void)
{
	SPS_DBG("%s.", __func__);

	if (sps != NULL) {
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
		sps_dma_de_init();
#endif
		/* Are there any remaining BAM registrations? */
		if (!list_empty(&sps->bams_q))
			SPS_ERR("SPS de-init: BAMs are still registered");

		sps_map_de_init();

		kfree(sps);
	}

	sps_mem_de_init();
}

/**
 * Initialize client state context
 *
 * This function initializes a client state context struct.
 *
 * @client - Pointer to client state context
 *
 * @return 0 on success, negative value on error
 *
 */
static int sps_client_init(struct sps_pipe *client)
{
	if (client == NULL)
		return -EINVAL;

	/*
	 * NOTE: Cannot store any state within the SPS driver because
	 * the driver init function may not have been called yet.
	 */
	memset(client, 0, sizeof(*client));
	sps_rm_config_init(&client->connect);

	client->client_state = SPS_STATE_DISCONNECT;
	client->bam = NULL;

	return 0;
}

/**
 * De-initialize client state context
 *
 * This function de-initializes a client state context struct.
 *
 * @client - Pointer to client state context
 *
 * @return 0 on success, negative value on error
 *
 */
static int sps_client_de_init(struct sps_pipe *client)
{
	if (client->client_state != SPS_STATE_DISCONNECT) {
		SPS_ERR("De-init client in connected state: 0x%x",
				   client->client_state);
		return SPS_ERROR;
	}

	client->bam = NULL;
	client->map = NULL;
	memset(&client->connect, 0, sizeof(client->connect));

	return 0;
}

/**
 * Find the BAM device from the physical address
 *
 * This function finds a BAM device in the BAM registration list that
 * matches the specified physical address.
 *
 * @phys_addr - physical address of the BAM
 *
 * @return - pointer to the BAM device struct, or NULL on error
 *
 */
static struct sps_bam *phy2bam(u32 phys_addr)
{
	struct sps_bam *bam;

	list_for_each_entry(bam, &sps->bams_q, list) {
		if (bam->props.phys_addr == phys_addr)
			return bam;
	}

	return NULL;
}

/**
 * Find the handle of a BAM device based on the physical address
 *
 * This function finds a BAM device in the BAM registration list that
 * matches the specified physical address, and returns its handle.
 *
 * @phys_addr - physical address of the BAM
 *
 * @h - device handle of the BAM
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_phy2h(u32 phys_addr, u32 *handle)
{
	struct sps_bam *bam;

	list_for_each_entry(bam, &sps->bams_q, list) {
		if (bam->props.phys_addr == phys_addr) {
			*handle = (u32) bam;
			return 0;
		}
	}

	SPS_INFO("sps: BAM device 0x%x is not registered yet.\n", phys_addr);

	return -ENODEV;
}
EXPORT_SYMBOL(sps_phy2h);

/**
 * Setup desc/data FIFO for bam-to-bam connection
 *
 * @mem_buffer - Pointer to struct for allocated memory properties.
 *
 * @addr - address of FIFO
 *
 * @size - FIFO size
 *
 * @use_offset - use address offset instead of absolute address
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_setup_bam2bam_fifo(struct sps_mem_buffer *mem_buffer,
		  u32 addr, u32 size, int use_offset)
{
	if ((mem_buffer == NULL) || (size == 0))
		return SPS_ERROR;

	if (use_offset) {
		if ((addr + size) <= sps->pipemem_size)
			mem_buffer->phys_base = sps->pipemem_phys_base + addr;
		else {
			SPS_ERR("sps: requested mem is out of "
					"pipe mem range.\n");
			return SPS_ERROR;
		}
	} else {
		if (addr >= sps->pipemem_phys_base &&
			(addr + size) <= (sps->pipemem_phys_base
						+ sps->pipemem_size))
			mem_buffer->phys_base = addr;
		else {
			SPS_ERR("sps: requested mem is out of "
					"pipe mem range.\n");
			return SPS_ERROR;
		}
	}

	mem_buffer->base = spsi_get_mem_ptr(mem_buffer->phys_base);
	mem_buffer->size = size;

	memset(mem_buffer->base, 0, mem_buffer->size);

	return 0;
}
EXPORT_SYMBOL(sps_setup_bam2bam_fifo);

/**
 * Find the BAM device from the handle
 *
 * This function finds a BAM device in the BAM registration list that
 * matches the specified device handle.
 *
 * @h - device handle of the BAM
 *
 * @return - pointer to the BAM device struct, or NULL on error
 *
 */
struct sps_bam *sps_h2bam(u32 h)
{
	struct sps_bam *bam;

	if (h == SPS_DEV_HANDLE_MEM || h == SPS_DEV_HANDLE_INVALID)
		return NULL;

	list_for_each_entry(bam, &sps->bams_q, list) {
		if ((u32) bam == (u32) h)
			return bam;
	}

	SPS_ERR("Can't find BAM device for handle 0x%x.", h);

	return NULL;
}

/**
 * Lock BAM device
 *
 * This function obtains the BAM spinlock on the client's connection.
 *
 * @pipe - pointer to client pipe state
 *
 * @return pointer to BAM device struct, or NULL on error
 *
 */
static struct sps_bam *sps_bam_lock(struct sps_pipe *pipe)
{
	struct sps_bam *bam;
	u32 pipe_index;

	bam = pipe->bam;
	if (bam == NULL) {
		SPS_ERR("Connection not in connected state");
		return NULL;
	}

	spin_lock_irqsave(&bam->connection_lock, bam->irqsave_flags);

	/* Verify client owns this pipe */
	pipe_index = pipe->pipe_index;
	if (pipe_index >= bam->props.num_pipes ||
	    pipe != bam->pipes[pipe_index]) {
		SPS_ERR("Client not owner of BAM 0x%x pipe: %d (max %d)",
			bam->props.phys_addr, pipe_index,
			bam->props.num_pipes);
		spin_unlock_irqrestore(&bam->connection_lock,
						bam->irqsave_flags);
		return NULL;
	}

	return bam;
}

/**
 * Unlock BAM device
 *
 * This function releases the BAM spinlock on the client's connection.
 *
 * @bam - pointer to BAM device struct
 *
 */
static inline void sps_bam_unlock(struct sps_bam *bam)
{
	spin_unlock_irqrestore(&bam->connection_lock, bam->irqsave_flags);
}

/**
 * Connect an SPS connection end point
 *
 */
int sps_connect(struct sps_pipe *h, struct sps_connect *connect)
{
	struct sps_pipe *pipe = h;
	u32 dev;
	struct sps_bam *bam;
	int result;

	if (sps == NULL)
		return -ENODEV;

	if (!sps->is_ready) {
		SPS_ERR("sps_connect.sps driver not ready.\n");
		return -EAGAIN;
	}

	mutex_lock(&sps->lock);
	/*
	 * Must lock the BAM device at the top level function, so must
	 * determine which BAM is the target for the connection
	 */
	if (connect->mode == SPS_MODE_SRC)
		dev = connect->source;
	else
		dev = connect->destination;

	bam = sps_h2bam(dev);
	if (bam == NULL) {
		SPS_ERR("Invalid BAM device handle: 0x%x", dev);
		result = SPS_ERROR;
		goto exit_err;
	}

	SPS_DBG("sps_connect: bam 0x%x src 0x%x dest 0x%x mode %s",
			BAM_ID(bam),
			connect->source,
			connect->destination,
			connect->mode == SPS_MODE_SRC ? "SRC" : "DEST");

	/* Allocate resources for the specified connection */
	pipe->connect = *connect;
	mutex_lock(&bam->lock);
	result = sps_rm_state_change(pipe, SPS_STATE_ALLOCATE);
	mutex_unlock(&bam->lock);
	if (result)
		goto exit_err;

	/* Configure the connection */
	mutex_lock(&bam->lock);
	result = sps_rm_state_change(pipe, SPS_STATE_CONNECT);
	mutex_unlock(&bam->lock);
	if (result) {
		sps_disconnect(h);
		goto exit_err;
	}

exit_err:
	mutex_unlock(&sps->lock);

	return result;
}
EXPORT_SYMBOL(sps_connect);

/**
 * Disconnect an SPS connection end point
 *
 * This function disconnects an SPS connection end point.
 * The SPS hardware associated with that end point will be disabled.
 * For a connection involving system memory (SPS_DEV_HANDLE_MEM), all
 * connection resources are deallocated.  For a peripheral-to-peripheral
 * connection, the resources associated with the connection will not be
 * deallocated until both end points are closed.
 *
 * The client must call sps_connect() for the handle before calling
 * this function.
 *
 * @h - client context for SPS connection end point
 *
 * @return 0 on success, negative value on error
 *
 */
int sps_disconnect(struct sps_pipe *h)
{
	struct sps_pipe *pipe = h;
	struct sps_pipe *check;
	struct sps_bam *bam;
	int result;

	if (pipe == NULL)
		return SPS_ERROR;

	bam = pipe->bam;
	if (bam == NULL)
		return SPS_ERROR;

	SPS_DBG("sps_disconnect: bam 0x%x src 0x%x dest 0x%x mode %s",
			BAM_ID(bam),
			pipe->connect.source,
			pipe->connect.destination,
			pipe->connect.mode == SPS_MODE_SRC ? "SRC" : "DEST");

	result = SPS_ERROR;
	/* Cross-check client with map table */
	if (pipe->connect.mode == SPS_MODE_SRC)
		check = pipe->map->client_src;
	else
		check = pipe->map->client_dest;

	if (check != pipe) {
		SPS_ERR("Client context is corrupt");
		goto exit_err;
	}

	/* Disconnect the BAM pipe */
	mutex_lock(&bam->lock);
	result = sps_rm_state_change(pipe, SPS_STATE_DISCONNECT);
	mutex_unlock(&bam->lock);
	if (result)
		goto exit_err;

	sps_rm_config_init(&pipe->connect);
	result = 0;

exit_err:

	return result;
}
EXPORT_SYMBOL(sps_disconnect);

/**
 * Register an event object for an SPS connection end point
 *
 */
int sps_register_event(struct sps_pipe *h, struct sps_register_event *reg)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	if (sps == NULL)
		return -ENODEV;

	if (!sps->is_ready) {
		SPS_ERR("sps_connect.sps driver not ready.\n");
		return -EAGAIN;
	}

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_pipe_reg_event(bam, pipe->pipe_index, reg);
	sps_bam_unlock(bam);
	if (result)
		SPS_ERR("Failed to register event for BAM 0x%x pipe %d",
			pipe->bam->props.phys_addr, pipe->pipe_index);

	return result;
}
EXPORT_SYMBOL(sps_register_event);

/**
 * Enable an SPS connection end point
 *
 */
int sps_flow_on(struct sps_pipe *h)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	/* Enable the pipe data flow */
	result = sps_rm_state_change(pipe, SPS_STATE_ENABLE);
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_flow_on);

/**
 * Disable an SPS connection end point
 *
 */
int sps_flow_off(struct sps_pipe *h, enum sps_flow_off mode)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	/* Disable the pipe data flow */
	result = sps_rm_state_change(pipe, SPS_STATE_DISABLE);
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_flow_off);

/**
 * Perform a DMA transfer on an SPS connection end point
 *
 */
int sps_transfer(struct sps_pipe *h, struct sps_transfer *transfer)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_pipe_transfer(bam, pipe->pipe_index, transfer);

	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_transfer);

/**
 * Perform a single DMA transfer on an SPS connection end point
 *
 */
int sps_transfer_one(struct sps_pipe *h, u32 addr, u32 size,
		     void *user, u32 flags)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_pipe_transfer_one(bam, pipe->pipe_index,
					   addr, size, user, flags);

	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_transfer_one);

/**
 * Read event queue for an SPS connection end point
 *
 */
int sps_get_event(struct sps_pipe *h, struct sps_event_notify *notify)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_pipe_get_event(bam, pipe->pipe_index, notify);
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_get_event);

/**
 * Determine whether an SPS connection end point FIFO is empty
 *
 */
int sps_is_pipe_empty(struct sps_pipe *h, u32 *empty)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_pipe_is_empty(bam, pipe->pipe_index, empty);
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_is_pipe_empty);

/**
 * Get number of free transfer entries for an SPS connection end point
 *
 */
int sps_get_free_count(struct sps_pipe *h, u32 *count)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_get_free_count(bam, pipe->pipe_index, count);
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_get_free_count);

/**
 * Reset an SPS BAM device
 *
 */
int sps_device_reset(u32 dev)
{
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s: dev = 0x%x", __func__, dev);

	mutex_lock(&sps->lock);
	/* Search for the target BAM device */
	bam = sps_h2bam(dev);
	if (bam == NULL) {
		SPS_ERR("Invalid BAM device handle: 0x%x", dev);
		result = SPS_ERROR;
		goto exit_err;
	}

	mutex_lock(&bam->lock);
	result = sps_bam_reset(bam);
	mutex_unlock(&bam->lock);
	if (result) {
		SPS_ERR("Failed to reset BAM device: 0x%x", dev);
		goto exit_err;
	}

exit_err:
	mutex_unlock(&sps->lock);

	return result;
}
EXPORT_SYMBOL(sps_device_reset);

/**
 * Get the configuration parameters for an SPS connection end point
 *
 */
int sps_get_config(struct sps_pipe *h, struct sps_connect *config)
{
	struct sps_pipe *pipe = h;

	if (config == NULL) {
		SPS_ERR("Config pointer is NULL");
		return SPS_ERROR;
	}

	/* Copy current client connection state */
	*config = pipe->connect;

	return 0;
}
EXPORT_SYMBOL(sps_get_config);

/**
 * Set the configuration parameters for an SPS connection end point
 *
 */
int sps_set_config(struct sps_pipe *h, struct sps_connect *config)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_pipe_set_params(bam, pipe->pipe_index,
					 config->options);
	if (result == 0)
		pipe->connect.options = config->options;
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_set_config);

/**
 * Set ownership of an SPS connection end point
 *
 */
int sps_set_owner(struct sps_pipe *h, enum sps_owner owner,
		  struct sps_satellite *connect)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	if (owner != SPS_OWNER_REMOTE) {
		SPS_ERR("Unsupported ownership state: %d", owner);
		return SPS_ERROR;
	}

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	result = sps_bam_set_satellite(bam, pipe->pipe_index);
	if (result)
		goto exit_err;

	/* Return satellite connect info */
	if (connect == NULL)
		goto exit_err;

	if (pipe->connect.mode == SPS_MODE_SRC) {
		connect->dev = pipe->map->src.bam_phys;
		connect->pipe_index = pipe->map->src.pipe_index;
	} else {
		connect->dev = pipe->map->dest.bam_phys;
		connect->pipe_index = pipe->map->dest.pipe_index;
	}
	connect->config = SPS_CONFIG_SATELLITE;
	connect->options = (enum sps_option) 0;

exit_err:
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_set_owner);

/**
 * Allocate memory from the SPS Pipe-Memory.
 *
 */
int sps_alloc_mem(struct sps_pipe *h, enum sps_mem mem,
		  struct sps_mem_buffer *mem_buffer)
{
	if (sps == NULL)
		return -ENODEV;

	if (!sps->is_ready) {
		SPS_ERR("sps_alloc_mem.sps driver not ready.\n");
		return -EAGAIN;
	}

	if (mem_buffer == NULL || mem_buffer->size == 0)
		return SPS_ERROR;

	mem_buffer->phys_base = sps_mem_alloc_io(mem_buffer->size);
	if (mem_buffer->phys_base == SPS_ADDR_INVALID)
		return SPS_ERROR;

	mem_buffer->base = spsi_get_mem_ptr(mem_buffer->phys_base);

	return 0;
}
EXPORT_SYMBOL(sps_alloc_mem);

/**
 * Free memory from the SPS Pipe-Memory.
 *
 */
int sps_free_mem(struct sps_pipe *h, struct sps_mem_buffer *mem_buffer)
{
	if (mem_buffer == NULL || mem_buffer->phys_base == SPS_ADDR_INVALID)
		return SPS_ERROR;

	sps_mem_free_io(mem_buffer->phys_base, mem_buffer->size);

	return 0;
}
EXPORT_SYMBOL(sps_free_mem);

/**
 * Register a BAM device
 *
 */
int sps_register_bam_device(const struct sps_bam_props *bam_props,
				u32 *dev_handle)
{
	struct sps_bam *bam = NULL;
	void *virt_addr = NULL;
	u32 manage;
	int ok;
	int result;

	if (sps == NULL)
		return SPS_ERROR;

	/* BAM-DMA is registered internally during power-up */
	if ((!sps->is_ready) && !(bam_props->options & SPS_BAM_OPT_BAMDMA)) {
		SPS_ERR("sps_register_bam_device.sps driver not ready.\n");
		return -EAGAIN;
	}

	if (bam_props == NULL || dev_handle == NULL)
		return SPS_ERROR;

	/* Check BAM parameters */
	manage = bam_props->manage & SPS_BAM_MGR_ACCESS_MASK;
	if (manage != SPS_BAM_MGR_NONE) {
		if (bam_props->virt_addr == NULL && bam_props->virt_size == 0) {
			SPS_ERR("Invalid properties for BAM: %x",
					   bam_props->phys_addr);
			return SPS_ERROR;
		}
	}
	if ((bam_props->manage & SPS_BAM_MGR_DEVICE_REMOTE) == 0) {
		/* BAM global is configured by local processor */
		if (bam_props->summing_threshold == 0) {
			SPS_ERR("Invalid device ctrl properties for BAM: %x",
			 bam_props->phys_addr);
			return SPS_ERROR;
		}
	}
	manage = bam_props->manage &
		  (SPS_BAM_MGR_PIPE_NO_CONFIG | SPS_BAM_MGR_PIPE_NO_CTRL);

	/* In case of error */
	*dev_handle = SPS_DEV_HANDLE_INVALID;
	result = SPS_ERROR;

	mutex_lock(&sps->lock);
	/* Is this BAM already registered? */
	bam = phy2bam(bam_props->phys_addr);
	if (bam != NULL) {
		mutex_unlock(&sps->lock);
		SPS_ERR("BAM already registered: %x", bam->props.phys_addr);
		result = -EEXIST;
		bam = NULL;   /* Avoid error clean-up kfree(bam) */
		goto exit_err;
	}

	/* Perform virtual mapping if required */
	if ((bam_props->manage & SPS_BAM_MGR_ACCESS_MASK) !=
	    SPS_BAM_MGR_NONE && bam_props->virt_addr == NULL) {
		/* Map the memory region */
		virt_addr = ioremap(bam_props->phys_addr, bam_props->virt_size);
		if (virt_addr == NULL) {
			SPS_ERR("Unable to map BAM IO memory: %x %x",
				bam_props->phys_addr, bam_props->virt_size);
			goto exit_err;
		}
	}

	bam = kzalloc(sizeof(*bam), GFP_KERNEL);
	if (bam == NULL) {
		SPS_ERR("Unable to allocate BAM device state: size 0x%x",
			sizeof(*bam));
		goto exit_err;
	}
	memset(bam, 0, sizeof(*bam));

	mutex_init(&bam->lock);
	mutex_lock(&bam->lock);

	/* Copy configuration to BAM device descriptor */
	bam->props = *bam_props;
	if (virt_addr != NULL)
		bam->props.virt_addr = virt_addr;

	if ((bam_props->manage & SPS_BAM_MGR_DEVICE_REMOTE) != 0 &&
	    (bam_props->manage & SPS_BAM_MGR_MULTI_EE) != 0 &&
	    bam_props->ee == 0) {
		/*
		 * BAM global is owned by a remote processor, so force EE index
		 * to a non-zero value to insure EE zero globals are not
		 * modified.
		 */
		SPS_INFO("Setting EE for BAM %x to non-zero",
				  bam_props->phys_addr);
		bam->props.ee = 1;
	}

	ok = sps_bam_device_init(bam);
	mutex_unlock(&bam->lock);
	if (ok) {
		SPS_ERR("Failed to init BAM device: phys 0x%0x",
			bam->props.phys_addr);
		goto exit_err;
	}

	/* Add BAM to the list */
	list_add_tail(&bam->list, &sps->bams_q);
	*dev_handle = (u32) bam;

	result = 0;
exit_err:
	mutex_unlock(&sps->lock);

	if (result) {
		if (bam != NULL) {
			if (virt_addr != NULL)
				iounmap(bam->props.virt_addr);
			kfree(bam);
		}

		return result;
	}

	/* If this BAM is attached to a BAM-DMA, init the BAM-DMA device */
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	if ((bam->props.options & SPS_BAM_OPT_BAMDMA)) {
		if (sps_dma_device_init((u32) bam)) {
			bam->props.options &= ~SPS_BAM_OPT_BAMDMA;
			sps_deregister_bam_device((u32) bam);
			SPS_ERR("Failed to init BAM-DMA device: BAM phys 0x%0x",
				bam->props.phys_addr);
			return SPS_ERROR;
		}
	}
#endif /* CONFIG_SPS_SUPPORT_BAMDMA */

	SPS_DBG("SPS registered BAM: phys 0x%x.", bam->props.phys_addr);

	return 0;
}
EXPORT_SYMBOL(sps_register_bam_device);

/**
 * Deregister a BAM device
 *
 */
int sps_deregister_bam_device(u32 dev_handle)
{
	struct sps_bam *bam;

	bam = sps_h2bam(dev_handle);
	if (bam == NULL)
		return SPS_ERROR;

	SPS_DBG("SPS deregister BAM: phys 0x%x.", bam->props.phys_addr);

	/* If this BAM is attached to a BAM-DMA, init the BAM-DMA device */
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	if ((bam->props.options & SPS_BAM_OPT_BAMDMA)) {
		mutex_lock(&bam->lock);
		(void)sps_dma_device_de_init((u32) bam);
		bam->props.options &= ~SPS_BAM_OPT_BAMDMA;
		mutex_unlock(&bam->lock);
	}
#endif

	/* Remove the BAM from the registration list */
	mutex_lock(&sps->lock);
	list_del(&bam->list);
	mutex_unlock(&sps->lock);

	/* De-init the BAM and free resources */
	mutex_lock(&bam->lock);
	sps_bam_device_de_init(bam);
	mutex_unlock(&bam->lock);
	if (bam->props.virt_size)
		(void)iounmap(bam->props.virt_addr);

	kfree(bam);

	return 0;
}
EXPORT_SYMBOL(sps_deregister_bam_device);

/**
 * Get processed I/O vector (completed transfers)
 *
 */
int sps_get_iovec(struct sps_pipe *h, struct sps_iovec *iovec)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	if (h == NULL || iovec == NULL)
		return SPS_ERROR;

	SPS_DBG("%s.", __func__);

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	/* Get an iovec from the BAM pipe descriptor FIFO */
	result = sps_bam_pipe_get_iovec(bam, pipe->pipe_index, iovec);
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_get_iovec);

/**
 * Perform timer control
 *
 */
int sps_timer_ctrl(struct sps_pipe *h,
			struct sps_timer_ctrl *timer_ctrl,
			struct sps_timer_result *timer_result)
{
	struct sps_pipe *pipe = h;
	struct sps_bam *bam;
	int result;

	SPS_DBG("%s.", __func__);

	if (h == NULL || timer_ctrl == NULL)
		return SPS_ERROR;

	bam = sps_bam_lock(pipe);
	if (bam == NULL)
		return SPS_ERROR;

	/* Perform the BAM pipe timer control operation */
	result = sps_bam_pipe_timer_ctrl(bam, pipe->pipe_index, timer_ctrl,
					 timer_result);
	sps_bam_unlock(bam);

	return result;
}
EXPORT_SYMBOL(sps_timer_ctrl);

/**
 * Allocate client state context
 *
 */
struct sps_pipe *sps_alloc_endpoint(void)
{
	struct sps_pipe *ctx = NULL;

	ctx = kzalloc(sizeof(struct sps_pipe), GFP_KERNEL);
	if (ctx == NULL) {
		SPS_ERR("Allocate pipe context fail.");
		return NULL;
	}

	sps_client_init(ctx);

	return ctx;
}
EXPORT_SYMBOL(sps_alloc_endpoint);

/**
 * Free client state context
 *
 */
int sps_free_endpoint(struct sps_pipe *ctx)
{
	int res;

	res = sps_client_de_init(ctx);

	if (res == 0)
		kfree(ctx);

	return res;
}
EXPORT_SYMBOL(sps_free_endpoint);

/**
 * Platform Driver.
 */
static int get_platform_data(struct platform_device *pdev)
{
	struct resource *resource;
	struct msm_sps_platform_data *pdata;

	pdata = pdev->dev.platform_data;

	if (pdata == NULL) {
		SPS_ERR("sps:inavlid platform data.\n");
		sps->bamdma_restricted_pipes = 0;
		return -EINVAL;
	} else {
		sps->bamdma_restricted_pipes = pdata->bamdma_restricted_pipes;
		SPS_DBG("sps:bamdma_restricted_pipes=0x%x.",
			sps->bamdma_restricted_pipes);
	}

	resource  = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						 "pipe_mem");
	if (resource) {
		sps->pipemem_phys_base = resource->start;
		sps->pipemem_size = resource_size(resource);
		SPS_DBG("sps:pipemem.base=0x%x,size=0x%x.",
			sps->pipemem_phys_base,
			sps->pipemem_size);
	}

#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	resource  = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						 "bamdma_bam");
	if (resource) {
		sps->bamdma_bam_phys_base = resource->start;
		sps->bamdma_bam_size = resource_size(resource);
		SPS_DBG("sps:bamdma_bam.base=0x%x,size=0x%x.",
			sps->bamdma_bam_phys_base,
			sps->bamdma_bam_size);
	}

	resource  = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						 "bamdma_dma");
	if (resource) {
		sps->bamdma_dma_phys_base = resource->start;
		sps->bamdma_dma_size = resource_size(resource);
		SPS_DBG("sps:bamdma_dma.base=0x%x,size=0x%x.",
			sps->bamdma_dma_phys_base,
			sps->bamdma_dma_size);
	}

	resource  = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						 "bamdma_irq");
	if (resource) {
		sps->bamdma_irq = resource->start;
		SPS_DBG("sps:bamdma_irq=%d.", sps->bamdma_irq);
	}
#endif

	return 0;
}

/**
 * Read data from device tree
 */
static int get_device_tree_data(struct platform_device *pdev)
{
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	struct resource *resource;

	if (of_property_read_u32((&pdev->dev)->of_node,
				"qcom,bam-dma-res-pipes",
				&sps->bamdma_restricted_pipes))
		return -EINVAL;
	else
		SPS_DBG("sps:bamdma_restricted_pipes=0x%x.",
			sps->bamdma_restricted_pipes);

	resource  = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (resource) {
		sps->bamdma_bam_phys_base = resource->start;
		sps->bamdma_bam_size = resource_size(resource);
		SPS_DBG("sps:bamdma_bam.base=0x%x,size=0x%x.",
			sps->bamdma_bam_phys_base,
			sps->bamdma_bam_size);
	} else {
		SPS_ERR("sps:BAM DMA BAM mem unavailable.");
		return -ENODEV;
	}

	resource  = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (resource) {
		sps->bamdma_dma_phys_base = resource->start;
		sps->bamdma_dma_size = resource_size(resource);
		SPS_DBG("sps:bamdma_dma.base=0x%x,size=0x%x.",
			sps->bamdma_dma_phys_base,
			sps->bamdma_dma_size);
	} else {
		SPS_ERR("sps:BAM DMA mem unavailable.");
		return -ENODEV;
	}

	resource  = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (resource) {
		sps->bamdma_irq = resource->start;
		SPS_DBG("sps:bamdma_irq=%d.", sps->bamdma_irq);
	} else {
		SPS_ERR("sps:BAM DMA IRQ unavailable.");
		return -ENODEV;
	}
#endif

	return 0;
}

static int __devinit msm_sps_probe(struct platform_device *pdev)
{
	int ret;

	SPS_DBG("sps:msm_sps_probe.");

	if (pdev->dev.of_node) {
		SPS_DBG("sps:get data from device tree.");
		ret = get_device_tree_data(pdev);

	} else {
		SPS_DBG("sps:get platform data.");
		ret = get_platform_data(pdev);
	}

	if (ret)
		return -ENODEV;

	/* Create Device */
	sps->dev_class = class_create(THIS_MODULE, SPS_DRV_NAME);

	ret = alloc_chrdev_region(&sps->dev_num, 0, 1, SPS_DRV_NAME);
	if (ret) {
		SPS_ERR("sps:alloc_chrdev_region err.");
		goto alloc_chrdev_region_err;
	}

	sps->dev = device_create(sps->dev_class, NULL, sps->dev_num, sps,
				SPS_DRV_NAME);
	if (IS_ERR(sps->dev)) {
		SPS_ERR("sps:device_create err.");
		goto device_create_err;
	}

	sps->dfab_clk = clk_get(sps->dev, "dfab_clk");
	if (IS_ERR(sps->dfab_clk)) {
		SPS_ERR("sps:fail to get dfab_clk.");
		goto clk_err;
	} else {
		ret = clk_set_rate(sps->dfab_clk, 64000000);
		if (ret) {
			SPS_ERR("sps:failed to set dfab_clk rate. "
					"ret=%d", ret);
			clk_put(sps->dfab_clk);
			goto clk_err;
		}
	}

	sps->pmem_clk = clk_get(sps->dev, "mem_clk");
	if (IS_ERR(sps->pmem_clk)) {
		SPS_ERR("sps:fail to get pmem_clk.");
		goto clk_err;
	} else {
		ret = clk_enable(sps->pmem_clk);
		if (ret) {
			SPS_ERR("sps:failed to enable pmem_clk. ret=%d", ret);
			goto clk_err;
		}
	}

#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	sps->bamdma_clk = clk_get(sps->dev, "dma_bam_pclk");
	if (IS_ERR(sps->bamdma_clk)) {
		SPS_ERR("sps:fail to get bamdma_clk.");
		goto clk_err;
	} else {
		ret = clk_enable(sps->bamdma_clk);
		if (ret) {
			SPS_ERR("sps:failed to enable bamdma_clk. ret=%d", ret);
			goto clk_err;
		}
	}

	ret = clk_enable(sps->dfab_clk);
	if (ret) {
		SPS_ERR("sps:failed to enable dfab_clk. ret=%d", ret);
		goto clk_err;
	}
#endif
	ret = sps_device_init();
	if (ret) {
		SPS_ERR("sps:sps_device_init err.");
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
		clk_disable(sps->dfab_clk);
#endif
		goto sps_device_init_err;
	}
#ifdef CONFIG_SPS_SUPPORT_BAMDMA
	clk_disable(sps->dfab_clk);
#endif
	sps->is_ready = true;

	SPS_INFO("sps is ready.");

	return 0;
clk_err:
sps_device_init_err:
	device_destroy(sps->dev_class, sps->dev_num);
device_create_err:
	unregister_chrdev_region(sps->dev_num, 1);
alloc_chrdev_region_err:
	class_destroy(sps->dev_class);

	return -ENODEV;
}

static int __devexit msm_sps_remove(struct platform_device *pdev)
{
	SPS_DBG("%s.", __func__);

	device_destroy(sps->dev_class, sps->dev_num);
	unregister_chrdev_region(sps->dev_num, 1);
	class_destroy(sps->dev_class);
	sps_device_de_init();

	clk_put(sps->dfab_clk);
	clk_put(sps->pmem_clk);
	clk_put(sps->bamdma_clk);

	return 0;
}

static struct of_device_id msm_sps_match[] = {
	{	.compatible = "qcom,msm_sps",
	},
	{}
};

static struct platform_driver msm_sps_driver = {
	.probe          = msm_sps_probe,
	.driver		= {
		.name	= SPS_DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = msm_sps_match,
	},
	.remove		= __exit_p(msm_sps_remove),
};

/**
 * Module Init.
 */
static int __init sps_init(void)
{
	int ret;

#ifdef CONFIG_DEBUG_FS
	sps_debugfs_init();
#endif

	SPS_DBG("%s.", __func__);

	/* Allocate the SPS driver state struct */
	sps = kzalloc(sizeof(*sps), GFP_KERNEL);
	if (sps == NULL) {
		SPS_ERR("sps:Unable to allocate driver state context.");
		return -ENOMEM;
	}

	ret = platform_driver_register(&msm_sps_driver);

	return ret;
}

/**
 * Module Exit.
 */
static void __exit sps_exit(void)
{
	SPS_DBG("%s.", __func__);

	platform_driver_unregister(&msm_sps_driver);

	if (sps != NULL) {
		kfree(sps);
		sps = NULL;
	}

#ifdef CONFIG_DEBUG_FS
	sps_debugfs_exit();
#endif
}

arch_initcall(sps_init);
module_exit(sps_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Smart Peripheral Switch (SPS)");

