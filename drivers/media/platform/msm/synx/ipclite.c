// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/platform_device.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <asm/memory.h>
#include <linux/sizes.h>

#include <linux/hwspinlock.h>
#include <soc/qcom/secure_buffer.h>

#include <linux/sysfs.h>

#include "ipclite_client.h"
#include "ipclite.h"

#define VMID_HLOS       3
#define VMID_SSC_Q6     5
#define VMID_ADSP_Q6    6
#define VMID_CDSP       30
#define GLOBAL_ATOMICS_ENABLED	1
#define GLOBAL_ATOMICS_DISABLED	0
#define FIFO_FULL_RESERVE 8
#define FIFO_ALIGNMENT 8

static struct ipclite_info *ipclite;
static struct ipclite_client synx_client;
static struct ipclite_client test_client;
static struct ipclite_hw_mutex_ops *ipclite_hw_mutex;
static struct ipclite_debug_info *ipclite_dbg_info;
static struct ipclite_debug_struct *ipclite_dbg_struct;
static struct ipclite_debug_inmem_buf *ipclite_dbg_inmem;
static struct mutex ssr_mutex;
static struct kobject *sysfs_kobj;

static uint32_t channel_status_info[IPCMEM_NUM_HOSTS];
static u32 global_atomic_support = GLOBAL_ATOMICS_ENABLED;
static uint32_t ipclite_debug_level = IPCLITE_ERR | IPCLITE_WARN | IPCLITE_INFO;
static uint32_t ipclite_debug_control = IPCLITE_DMESG_LOG, ipclite_debug_dump;

static void IPCLITE_OS_INMEM_LOG(const char *psztStr, ...)
{
	uint32_t local_index = 0;
	va_list pArgs;

	va_start(pArgs, psztStr);

	/* Incrementing the index atomically and storing the index in local variable */
	local_index = ipclite_global_atomic_inc((ipclite_atomic_int32_t *)
							&ipclite_dbg_info->debug_log_index);
	local_index %= IPCLITE_LOG_BUF_SIZE;

	/* Writes data on the index location */
	vsnprintf(ipclite_dbg_inmem->IPCLITELog[local_index], IPCLITE_LOG_MSG_SIZE, psztStr, pArgs);

	va_end(pArgs);
}

static void ipclite_dump_debug_struct(void)
{
	int i, host;
	struct ipclite_debug_struct *temp_dbg_struct;

	/* Check if debug structures are initialized */
	if (!ipclite_dbg_info || !ipclite_dbg_struct) {
		pr_err("Debug Structures not initialized\n");
		return;
	}

	/* Check if debug structures are enabled before printing */
	if (!(ipclite_debug_control & IPCLITE_DBG_STRUCT)) {
		pr_err("Debug Structures not enabled\n");
		return;
	}

	/* Dumping the debug structures */
	pr_info("------------------- Dumping IPCLite Debug Structure -------------------\n");

	for (host = 0; host < IPCMEM_NUM_HOSTS; host++) {
		if (ipclite->ipcmem.toc->recovery.configured_core[host]) {
			temp_dbg_struct = (struct ipclite_debug_struct *)
						(((char *)ipclite_dbg_struct) +
						(sizeof(*temp_dbg_struct) * host));

			pr_info("---------- Host ID: %d dbg_mem:%p ----------\n",
					host, temp_dbg_struct);
			pr_info("Total Signals Sent : %d Total Signals Received : %d\n",
					temp_dbg_struct->dbg_info_overall.total_numsig_sent,
					temp_dbg_struct->dbg_info_overall.total_numsig_recv);
			pr_info("Last Signal Sent to Host ID : %d Last Signal Received from Host ID : %d\n",
					temp_dbg_struct->dbg_info_overall.last_sent_host_id,
					temp_dbg_struct->dbg_info_overall.last_recv_host_id);
			pr_info("Last Signal ID Sent : %d Last Signal ID Received : %d\n",
					temp_dbg_struct->dbg_info_overall.last_sigid_sent,
					temp_dbg_struct->dbg_info_overall.last_sigid_recv);

			for (i = 0; i < IPCMEM_NUM_HOSTS; i++) {
				if (ipclite->ipcmem.toc->recovery.configured_core[i]) {
					pr_info("----------> Host ID : %d Host ID : %d Channel State: %d\n",
					host, i, ipclite->ipcmem.toc->toc_entry[host][i].status);
					pr_info("No. of Messages Sent : %d No. of Messages Received : %d\n",
					temp_dbg_struct->dbg_info_host[i].numsig_sent,
					temp_dbg_struct->dbg_info_host[i].numsig_recv);
					pr_info("No. of Interrupts Received : %d\n",
					temp_dbg_struct->dbg_info_host[i].num_intr);
					pr_info("TX Write Index : %d TX Read Index : %d\n",
					temp_dbg_struct->dbg_info_host[i].tx_wr_index,
					temp_dbg_struct->dbg_info_host[i].tx_rd_index);
					pr_info("TX Write Index[0] : %d TX Read Index[0] : %d\n",
					temp_dbg_struct->dbg_info_host[i].prev_tx_wr_index[0],
					temp_dbg_struct->dbg_info_host[i].prev_tx_rd_index[0]);
					pr_info("TX Write Index[1] : %d TX Read Index[1] : %d\n",
					temp_dbg_struct->dbg_info_host[i].prev_tx_wr_index[1],
					temp_dbg_struct->dbg_info_host[i].prev_tx_rd_index[1]);
					pr_info("RX Write Index : %d RX Read Index : %d\n",
					temp_dbg_struct->dbg_info_host[i].rx_wr_index,
					temp_dbg_struct->dbg_info_host[i].rx_rd_index);
					pr_info("RX Write Index[0] : %d RX Read Index[0] : %d\n",
					temp_dbg_struct->dbg_info_host[i].prev_rx_wr_index[0],
					temp_dbg_struct->dbg_info_host[i].prev_rx_rd_index[0]);
					pr_info("RX Write Index[1] : %d RX Read Index[1] : %d\n",
					temp_dbg_struct->dbg_info_host[i].prev_rx_wr_index[1],
					temp_dbg_struct->dbg_info_host[i].prev_rx_rd_index[1]);
				}
			}
		}
	}
	return;
}

static void ipclite_dump_inmem_logs(void)
{
	int i;
	uint32_t local_index = 0;

	/* Check if debug and inmem structures are initialized */
	if (!ipclite_dbg_info || !ipclite_dbg_inmem) {
		pr_err("Debug structures not initialized\n");
		return;
	}

	/* Check if debug structures are enabled before printing */
	if (!(ipclite_debug_control & IPCLITE_INMEM_LOG)) {
		pr_err("In-Memory Logs not enabled\n");
		return;
	}

	/* Dumping the debug in-memory logs */
	pr_info("------------------- Dumping In-Memory Logs -------------------\n");

	/* Storing the index atomically in local variable */
	local_index = ipclite_global_atomic_load_u32((ipclite_atomic_uint32_t *)
							&ipclite_dbg_info->debug_log_index);

	/* Printing from current index till the end of buffer */
	for (i = local_index % IPCLITE_LOG_BUF_SIZE; i < IPCLITE_LOG_BUF_SIZE; i++) {
		if (ipclite_dbg_inmem->IPCLITELog[i][0])
			pr_info("%s\n", ipclite_dbg_inmem->IPCLITELog[i]);
	}

	/* Printing from 0th index to current-1 index */
	for (i = 0; i < local_index % IPCLITE_LOG_BUF_SIZE; i++) {
		if (ipclite_dbg_inmem->IPCLITELog[i][0])
			pr_info("%s\n", ipclite_dbg_inmem->IPCLITELog[i]);
	}

	return;
}

static void ipclite_hw_mutex_acquire(void)
{
	int32_t ret;

	if (ipclite != NULL) {
		if (!ipclite->ipcmem.toc->ipclite_features.global_atomic_support) {
			ret = hwspin_lock_timeout_irqsave(ipclite->hwlock,
					HWSPINLOCK_TIMEOUT,
					&ipclite->ipclite_hw_mutex->flags);
			if (ret) {
				IPCLITE_OS_LOG(IPCLITE_ERR, "Hw mutex lock acquire failed\n");
				return;
			}

			ipclite->ipcmem.toc->recovery.global_atomic_hwlock_owner = IPCMEM_APPS;

			IPCLITE_OS_LOG(IPCLITE_DBG, "Hw mutex lock acquired\n");
		}
	}
}

static void ipclite_hw_mutex_release(void)
{
	if (ipclite != NULL) {
		if (!ipclite->ipcmem.toc->ipclite_features.global_atomic_support) {
			ipclite->ipcmem.toc->recovery.global_atomic_hwlock_owner =
									IPCMEM_INVALID_HOST;
			hwspin_unlock_irqrestore(ipclite->hwlock,
				&ipclite->ipclite_hw_mutex->flags);
			IPCLITE_OS_LOG(IPCLITE_DBG, "Hw mutex lock release\n");
		}
	}
}

void ipclite_atomic_init_u32(ipclite_atomic_uint32_t *addr, uint32_t data)
{
	atomic_set(addr, data);
}
EXPORT_SYMBOL(ipclite_atomic_init_u32);

void ipclite_atomic_init_i32(ipclite_atomic_int32_t *addr, int32_t data)
{
	atomic_set(addr, data);
}
EXPORT_SYMBOL(ipclite_atomic_init_i32);

void ipclite_global_atomic_store_u32(ipclite_atomic_uint32_t *addr, uint32_t data)
{
	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	atomic_set(addr, data);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();
}
EXPORT_SYMBOL(ipclite_global_atomic_store_u32);

void ipclite_global_atomic_store_i32(ipclite_atomic_int32_t *addr, int32_t data)
{
	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	atomic_set(addr, data);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();
}
EXPORT_SYMBOL(ipclite_global_atomic_store_i32);

uint32_t ipclite_global_atomic_load_u32(ipclite_atomic_uint32_t *addr)
{
	uint32_t ret;

	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	ret = atomic_read(addr);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();

	return ret;
}
EXPORT_SYMBOL(ipclite_global_atomic_load_u32);

int32_t ipclite_global_atomic_load_i32(ipclite_atomic_int32_t *addr)
{
	int32_t ret;

	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	ret = atomic_read(addr);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();

	return ret;
}
EXPORT_SYMBOL(ipclite_global_atomic_load_i32);

uint32_t ipclite_global_test_and_set_bit(uint32_t nr, ipclite_atomic_uint32_t *addr)
{
	uint32_t ret;
	uint32_t mask = (1 << nr);

	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	ret = atomic_fetch_or(mask, addr);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();

	return ret;
}
EXPORT_SYMBOL(ipclite_global_test_and_set_bit);

uint32_t ipclite_global_test_and_clear_bit(uint32_t nr, ipclite_atomic_uint32_t *addr)
{
	uint32_t ret;
	uint32_t mask = (1 << nr);

	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	ret = atomic_fetch_and(~mask, addr);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();

	return ret;
}
EXPORT_SYMBOL(ipclite_global_test_and_clear_bit);

int32_t ipclite_global_atomic_inc(ipclite_atomic_int32_t *addr)
{
	int32_t ret = 0;

	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	ret = atomic_fetch_add(1, addr);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();

	return ret;
}
EXPORT_SYMBOL(ipclite_global_atomic_inc);

int32_t ipclite_global_atomic_dec(ipclite_atomic_int32_t *addr)
{
	int32_t ret = 0;

	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	ret = atomic_fetch_sub(1, addr);

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();

	return ret;
}
EXPORT_SYMBOL(ipclite_global_atomic_dec);

static size_t ipcmem_rx_avail(struct ipclite_fifo *rx_fifo)
{
	size_t len;
	u32 head;
	u32 tail;

	head = le32_to_cpu(*rx_fifo->head);
	tail = le32_to_cpu(*rx_fifo->tail);

	IPCLITE_OS_LOG(IPCLITE_DBG, "head=%d, tail=%d\n", head, tail);

	if (head < tail)
		len = rx_fifo->length - tail + head;
	else
		len = head - tail;

	if (WARN_ON_ONCE(len > rx_fifo->length))
		len = 0;

	IPCLITE_OS_LOG(IPCLITE_DBG, "len=%d\n", len);

	return len;
}

static void ipcmem_rx_peak(struct ipclite_fifo *rx_fifo,
			       void *data, size_t count)
{
	size_t len;
	u32 tail;

	tail = le32_to_cpu(*rx_fifo->tail);

	if (WARN_ON_ONCE(tail > rx_fifo->length))
		return;

	if (tail >= rx_fifo->length)
		tail -= rx_fifo->length;

	len = min_t(size_t, count, rx_fifo->length - tail);
	if (len)
		memcpy_fromio(data, rx_fifo->fifo + tail, len);

	if (len != count)
		memcpy_fromio(data + len, rx_fifo->fifo, (count - len));
}

static void ipcmem_rx_advance(struct ipclite_fifo *rx_fifo,
				  size_t count, uint32_t core_id)
{
	u32 tail;

	tail = le32_to_cpu(*rx_fifo->tail);

	tail += count;
	if (tail >= rx_fifo->length)
		tail %= rx_fifo->length;

	*rx_fifo->tail = cpu_to_le32(tail);

	/* Storing the debug data in debug structures */
	if (ipclite_debug_control & IPCLITE_DBG_STRUCT) {
		ipclite_dbg_struct->dbg_info_host[core_id].prev_rx_wr_index[1] =
				ipclite_dbg_struct->dbg_info_host[core_id].prev_rx_wr_index[0];
		ipclite_dbg_struct->dbg_info_host[core_id].prev_rx_wr_index[0] =
				ipclite_dbg_struct->dbg_info_host[core_id].rx_wr_index;
		ipclite_dbg_struct->dbg_info_host[core_id].rx_wr_index = *rx_fifo->head;

		ipclite_dbg_struct->dbg_info_host[core_id].prev_rx_rd_index[1] =
				ipclite_dbg_struct->dbg_info_host[core_id].prev_rx_rd_index[0];
		ipclite_dbg_struct->dbg_info_host[core_id].prev_rx_rd_index[0] =
				ipclite_dbg_struct->dbg_info_host[core_id].rx_rd_index;
		ipclite_dbg_struct->dbg_info_host[core_id].rx_rd_index = *rx_fifo->tail;

		ipclite_dbg_struct->dbg_info_overall.total_numsig_recv++;
		ipclite_dbg_struct->dbg_info_host[core_id].numsig_recv++;
	}
}

static size_t ipcmem_tx_avail(struct ipclite_fifo *tx_fifo)
{
	u32 head;
	u32 tail;
	u32 avail;

	head = le32_to_cpu(*tx_fifo->head);
	tail = le32_to_cpu(*tx_fifo->tail);

	if (tail <= head)
		avail = tx_fifo->length - head + tail;
	else
		avail = tail - head;

	if (avail < FIFO_FULL_RESERVE)
		avail = 0;
	else
		avail -= FIFO_FULL_RESERVE;

	if (WARN_ON_ONCE(avail > tx_fifo->length))
		avail = 0;

	return avail;
}

static unsigned int ipcmem_tx_write_one(struct ipclite_fifo *tx_fifo,
					    unsigned int head,
					    const void *data, size_t count)
{
	size_t len;

	if (WARN_ON_ONCE(head > tx_fifo->length))
		return head;

	len = min_t(size_t, count, tx_fifo->length - head);
	if (len)
		memcpy(tx_fifo->fifo + head, data, len);

	if (len != count)
		memcpy(tx_fifo->fifo, data + len, count - len);

	head += count;
	if (head >= tx_fifo->length)
		head -= tx_fifo->length;

	return head;
}

static void ipcmem_tx_write(struct ipclite_fifo *tx_fifo,
			const void *data, size_t dlen, uint32_t core_id, uint32_t signal_id)
{
	unsigned int head;

	head = le32_to_cpu(*tx_fifo->head);
	head = ipcmem_tx_write_one(tx_fifo, head, data, dlen);

	head = ALIGN(head, 8);
	if (head >= tx_fifo->length)
		head -= tx_fifo->length;

	/* Ensure ordering of fifo and head update */
	wmb();

	*tx_fifo->head = cpu_to_le32(head);

	IPCLITE_OS_LOG(IPCLITE_DBG, "head : %d core_id : %d signal_id : %d\n",
						*tx_fifo->head, core_id, signal_id);

	/* Storing the debug data in debug structures */
	if (ipclite_debug_control & IPCLITE_DBG_STRUCT) {
		ipclite_dbg_struct->dbg_info_host[core_id].prev_tx_wr_index[1] =
				ipclite_dbg_struct->dbg_info_host[core_id].prev_tx_wr_index[0];
		ipclite_dbg_struct->dbg_info_host[core_id].prev_tx_wr_index[0] =
				ipclite_dbg_struct->dbg_info_host[core_id].tx_wr_index;
		ipclite_dbg_struct->dbg_info_host[core_id].tx_wr_index = *tx_fifo->head;

		ipclite_dbg_struct->dbg_info_host[core_id].prev_tx_rd_index[1] =
				ipclite_dbg_struct->dbg_info_host[core_id].prev_tx_rd_index[0];
		ipclite_dbg_struct->dbg_info_host[core_id].prev_tx_rd_index[0] =
				ipclite_dbg_struct->dbg_info_host[core_id].tx_rd_index;
		ipclite_dbg_struct->dbg_info_host[core_id].tx_rd_index = *tx_fifo->tail;

		ipclite_dbg_struct->dbg_info_overall.total_numsig_sent++;
		ipclite_dbg_struct->dbg_info_host[core_id].numsig_sent++;
		ipclite_dbg_struct->dbg_info_overall.last_sent_host_id = core_id;
		ipclite_dbg_struct->dbg_info_overall.last_sigid_sent = signal_id;
	}
}

static size_t ipclite_rx_avail(struct ipclite_channel *channel)
{
	return channel->rx_fifo->avail(channel->rx_fifo);
}

static void ipclite_rx_peak(struct ipclite_channel *channel,
			       void *data, size_t count)
{
	channel->rx_fifo->peak(channel->rx_fifo, data, count);
}

static void ipclite_rx_advance(struct ipclite_channel *channel,
					size_t count)
{
	channel->rx_fifo->advance(channel->rx_fifo, count, channel->remote_pid);
}

static size_t ipclite_tx_avail(struct ipclite_channel *channel)
{
	return channel->tx_fifo->avail(channel->tx_fifo);
}

static void ipclite_tx_write(struct ipclite_channel *channel,
				const void *data, size_t dlen)
{
	channel->tx_fifo->write(channel->tx_fifo, data, dlen, channel->remote_pid,
								channel->irq_info->signal_id);
}

static int ipclite_rx_data(struct ipclite_channel *channel, size_t avail)
{
	uint64_t data;
	int ret = 0;

	if (avail < sizeof(data)) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Not enough data in fifo, Core : %d Signal : %d\n",
						channel->remote_pid, channel->irq_info->signal_id);
		return -EAGAIN;
	}

	ipclite_rx_peak(channel, &data, sizeof(data));

	if (synx_client.reg_complete == 1) {
		if (synx_client.callback)
			synx_client.callback(channel->remote_pid, data,
								synx_client.priv_data);
	}
	ipclite_rx_advance(channel, ALIGN(sizeof(data), 8));
	return ret;
}

static int ipclite_rx_test_data(struct ipclite_channel *channel, size_t avail)
{
	uint64_t data;
	int ret = 0;

	if (avail < sizeof(data)) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Not enough data in fifo, Core : %d Signal : %d\n",
						channel->remote_pid, channel->irq_info->signal_id);
		return -EAGAIN;
	}

	ipclite_rx_peak(channel, &data, sizeof(data));

	if (test_client.reg_complete == 1) {
		if (test_client.callback)
			test_client.callback(channel->remote_pid, data,
								test_client.priv_data);
	}
	ipclite_rx_advance(channel, ALIGN(sizeof(data), 8));
	return ret;
}

static irqreturn_t ipclite_intr(int irq, void *data)
{
	struct ipclite_channel *channel;
	struct ipclite_irq_info *irq_info;
	unsigned int avail = 0;
	int ret = 0;
	uint64_t msg;

	irq_info = (struct ipclite_irq_info *)data;
	channel = container_of(irq_info, struct ipclite_channel, irq_info[irq_info->signal_id]);

	IPCLITE_OS_LOG(IPCLITE_DBG, "Interrupt received from Core : %d Signal : %d\n",
							channel->remote_pid, irq_info->signal_id);

	/* Storing the debug data in debug structures */
	if (ipclite_debug_control & IPCLITE_DBG_STRUCT) {
		ipclite_dbg_struct->dbg_info_host[channel->remote_pid].num_intr++;
		ipclite_dbg_struct->dbg_info_overall.last_recv_host_id = channel->remote_pid;
		ipclite_dbg_struct->dbg_info_overall.last_sigid_recv = irq_info->signal_id;
	}

	if (irq_info->signal_id == IPCLITE_MSG_SIGNAL) {
		for (;;) {
			avail = ipclite_rx_avail(channel);
			if (avail < sizeof(msg))
				break;

			ret = ipclite_rx_data(channel, avail);
		}
		IPCLITE_OS_LOG(IPCLITE_DBG, "checking messages in rx_fifo done\n");
	} else if (irq_info->signal_id == IPCLITE_VERSION_SIGNAL) {
		IPCLITE_OS_LOG(IPCLITE_DBG, "Versioning is currently not enabled\n");
	} else if (irq_info->signal_id == IPCLITE_TEST_SIGNAL) {
		for (;;) {
			avail = ipclite_rx_avail(channel);
			if (avail < sizeof(msg))
				break;

			ret = ipclite_rx_test_data(channel, avail);
		}
		IPCLITE_OS_LOG(IPCLITE_DBG, "checking messages in rx_fifo done\n");
	} else {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Wrong Interrupt Signal from core : %d signal : %d\n",
							channel->remote_pid, irq_info->signal_id);
	}

	return IRQ_HANDLED;
}

static int ipclite_tx(struct ipclite_channel *channel,
			uint64_t data, size_t dlen, uint32_t ipclite_signal)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&channel->tx_lock, flags);
	if (ipclite_tx_avail(channel) < dlen) {
		spin_unlock_irqrestore(&channel->tx_lock, flags);
		ret = -EAGAIN;
		return ret;
	}

	ipclite_tx_write(channel, &data, dlen);

	mbox_send_message(channel->irq_info[ipclite_signal].mbox_chan, NULL);
	mbox_client_txdone(channel->irq_info[ipclite_signal].mbox_chan, 0);

	spin_unlock_irqrestore(&channel->tx_lock, flags);

	return ret;
}

static int ipclite_send_debug_info(int32_t proc_id)
{
	int ret = 0;

	if (proc_id < 0 || proc_id >= IPCMEM_NUM_HOSTS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Invalid proc_id : %d\n", proc_id);
		return -EINVAL;
	}

	if (channel_status_info[proc_id] != CHANNEL_ACTIVE) {
		if (ipclite->ipcmem.toc->toc_entry[IPCMEM_APPS][proc_id].status == CHANNEL_ACTIVE) {
			channel_status_info[proc_id] = CHANNEL_ACTIVE;
		} else {
			IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot Send, Core %d is Inactive\n", proc_id);
			return -IPCLITE_EINCHAN;
		}
	}

	ret = mbox_send_message(ipclite->channel[proc_id].irq_info[IPCLITE_DEBUG_SIGNAL].mbox_chan,
											NULL);
	if (ret < IPCLITE_SUCCESS) {
		IPCLITE_OS_LOG(IPCLITE_ERR,
				"Debug Signal sending failed to Core : %d Signal : %d ret : %d\n",
							proc_id, IPCLITE_DEBUG_SIGNAL, ret);
		return -IPCLITE_FAILURE;
	}

	IPCLITE_OS_LOG(IPCLITE_DBG,
				"Debug Signal send completed to core : %d signal : %d ret : %d\n",
							proc_id, IPCLITE_DEBUG_SIGNAL, ret);
	return IPCLITE_SUCCESS;
}

int ipclite_ssr_update(int32_t proc_id)
{
	int ret = 0;

	if (proc_id < 0 || proc_id >= IPCMEM_NUM_HOSTS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Invalid proc_id : %d\n", proc_id);
		return -EINVAL;
	}

	if (channel_status_info[proc_id] != CHANNEL_ACTIVE) {
		if (ipclite->ipcmem.toc->toc_entry[IPCMEM_APPS][proc_id].status == CHANNEL_ACTIVE) {
			channel_status_info[proc_id] = CHANNEL_ACTIVE;
		} else {
			IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot Send, Core %d is Inactive\n", proc_id);
			return -IPCLITE_EINCHAN;
		}
	}

	ret = mbox_send_message(ipclite->channel[proc_id].irq_info[IPCLITE_SSR_SIGNAL].mbox_chan,
											NULL);
	if (ret < IPCLITE_SUCCESS) {
		IPCLITE_OS_LOG(IPCLITE_ERR,
				"SSR Signal sending failed to Core : %d Signal : %d ret : %d\n",
							proc_id, IPCLITE_SSR_SIGNAL, ret);
		return -IPCLITE_FAILURE;
	}

	IPCLITE_OS_LOG(IPCLITE_DBG,
				"SSR Signal send completed to core : %d signal : %d ret : %d\n",
							proc_id, IPCLITE_SSR_SIGNAL, ret);
	return IPCLITE_SUCCESS;
}

void ipclite_recover(enum ipcmem_host_type core_id)
{
	int ret, i, host, host0, host1;

	IPCLITE_OS_LOG(IPCLITE_DBG, "IPCLite Recover - Crashed Core : %d\n", core_id);

	/* verify and reset the hw mutex lock */
	if (core_id == ipclite->ipcmem.toc->recovery.global_atomic_hwlock_owner) {
		ipclite->ipcmem.toc->recovery.global_atomic_hwlock_owner = IPCMEM_INVALID_HOST;
		hwspin_unlock_raw(ipclite->hwlock);
		IPCLITE_OS_LOG(IPCLITE_DBG, "HW Lock Reset\n");
	}

	mutex_lock(&ssr_mutex);
	/* Set the Global Channel Status to 0 to avoid Race condition */
	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		host0 = ipcmem_toc_partition_entries[i].host0;
		host1 = ipcmem_toc_partition_entries[i].host1;

		if (host0 == core_id || host1 == core_id) {

			ipclite_global_atomic_store_i32((ipclite_atomic_int32_t *)
				(&(ipclite->ipcmem.toc->toc_entry[host0][host1].status)), 0);
			ipclite_global_atomic_store_i32((ipclite_atomic_int32_t *)
				(&(ipclite->ipcmem.toc->toc_entry[host1][host0].status)), 0);

			channel_status_info[core_id] =
					ipclite->ipcmem.toc->toc_entry[host0][host1].status;
		}
		IPCLITE_OS_LOG(IPCLITE_DBG, "Global Channel Status : [%d][%d] : %d\n", host0, host1,
					ipclite->ipcmem.toc->toc_entry[host0][host1].status);
		IPCLITE_OS_LOG(IPCLITE_DBG, "Global Channel Status : [%d][%d] : %d\n", host1, host0,
					ipclite->ipcmem.toc->toc_entry[host1][host0].status);
	}

	/* Resets the TX/RX queue */
	*(ipclite->channel[core_id].tx_fifo->head) = 0;
	*(ipclite->channel[core_id].rx_fifo->tail) = 0;

	IPCLITE_OS_LOG(IPCLITE_DBG, "TX Fifo Reset : %d\n",
						*(ipclite->channel[core_id].tx_fifo->head));
	IPCLITE_OS_LOG(IPCLITE_DBG, "RX Fifo Reset : %d\n",
						*(ipclite->channel[core_id].rx_fifo->tail));

	/* Increment the Global Channel Status for APPS and crashed core*/
	ipclite_global_atomic_inc((ipclite_atomic_int32_t *)
			(&(ipclite->ipcmem.toc->toc_entry[IPCMEM_APPS][core_id].status)));
	ipclite_global_atomic_inc((ipclite_atomic_int32_t *)
			(&(ipclite->ipcmem.toc->toc_entry[core_id][IPCMEM_APPS].status)));

	channel_status_info[core_id] =
			ipclite->ipcmem.toc->toc_entry[IPCMEM_APPS][core_id].status;

	/* Update other cores about SSR */
	for (host = 1; host < IPCMEM_NUM_HOSTS; host++) {
		if (host != core_id && ipclite->ipcmem.toc->recovery.configured_core[host]) {
			ret = ipclite_ssr_update(host);
			if (ret < IPCLITE_SUCCESS)
				IPCLITE_OS_LOG(IPCLITE_ERR,
					"Failed to send SSR update to core : %d\n", host);
			else
				IPCLITE_OS_LOG(IPCLITE_DBG, "SSR update sent to core %d\n", host);
		}
	}
	mutex_unlock(&ssr_mutex);

	/* Dump the debug information */
	if (ipclite_debug_dump & IPCLITE_DUMP_SSR) {
		ipclite_dump_debug_struct();
		ipclite_dump_inmem_logs();
	}

	return;
}
EXPORT_SYMBOL(ipclite_recover);

int ipclite_msg_send(int32_t proc_id, uint64_t data)
{
	int ret = 0;

	if (proc_id < 0 || proc_id >= IPCMEM_NUM_HOSTS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Invalid proc_id : %d\n", proc_id);
		return -EINVAL;
	}

	if (channel_status_info[proc_id] != CHANNEL_ACTIVE) {
		if (ipclite->ipcmem.toc->toc_entry[IPCMEM_APPS][proc_id].status == CHANNEL_ACTIVE) {
			channel_status_info[proc_id] = CHANNEL_ACTIVE;
		} else {
			IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot Send, Core %d is Inactive\n", proc_id);
			return -IPCLITE_EINCHAN;
		}
	}

	ret = ipclite_tx(&ipclite->channel[proc_id], data, sizeof(data),
								IPCLITE_MSG_SIGNAL);

	IPCLITE_OS_LOG(IPCLITE_DBG, "Message send complete to core : %d signal : %d ret : %d\n",
								proc_id, IPCLITE_MSG_SIGNAL, ret);
	return ret;
}
EXPORT_SYMBOL(ipclite_msg_send);

int ipclite_register_client(IPCLite_Client cb_func_ptr, void *priv)
{
	if (!cb_func_ptr) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Invalid callback pointer\n");
		return -EINVAL;
	}
	synx_client.callback = cb_func_ptr;
	synx_client.priv_data = priv;
	synx_client.reg_complete = 1;
	IPCLITE_OS_LOG(IPCLITE_DBG, "Client Registration completed\n");
	return 0;
}
EXPORT_SYMBOL(ipclite_register_client);

int ipclite_test_msg_send(int32_t proc_id, uint64_t data)
{
	int ret = 0;

	if (proc_id < 0 || proc_id >= IPCMEM_NUM_HOSTS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Invalid proc_id : %d\n", proc_id);
		return -EINVAL;
	}

	if (channel_status_info[proc_id] != CHANNEL_ACTIVE) {
		if (ipclite->ipcmem.toc->toc_entry[IPCMEM_APPS][proc_id].status == CHANNEL_ACTIVE) {
			channel_status_info[proc_id] = CHANNEL_ACTIVE;
		} else {
			IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot Send, Core %d is Inactive\n", proc_id);
			return -IPCLITE_EINCHAN;
		}
	}

	ret = ipclite_tx(&ipclite->channel[proc_id], data, sizeof(data),
									IPCLITE_TEST_SIGNAL);

	IPCLITE_OS_LOG(IPCLITE_DBG, "Test Msg send complete to core : %d signal : %d ret : %d\n",
								proc_id, IPCLITE_TEST_SIGNAL, ret);
	return ret;
}
EXPORT_SYMBOL(ipclite_test_msg_send);

int ipclite_register_test_client(IPCLite_Client cb_func_ptr, void *priv)
{
	if (!cb_func_ptr) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Invalid callback pointer\n");
		return -EINVAL;
	}
	test_client.callback = cb_func_ptr;
	test_client.priv_data = priv;
	test_client.reg_complete = 1;
	IPCLITE_OS_LOG(IPCLITE_DBG, "Test Client Registration Completed\n");
	return 0;
}
EXPORT_SYMBOL(ipclite_register_test_client);

static int map_ipcmem(struct ipclite_info *ipclite, const char *name)
{
	struct device *dev;
	struct device_node *np;
	struct resource r;
	int ret = 0;

	dev = ipclite->dev;

	np = of_parse_phandle(dev->of_node, name, 0);
	if (!np) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "No %s specified\n", name);
		return -EINVAL;
	}

	ret = of_address_to_resource(np, 0, &r);
	of_node_put(np);
	if (ret)
		return ret;

	ipclite->ipcmem.mem.aux_base = (u64)r.start;
	ipclite->ipcmem.mem.size = resource_size(&r);
	ipclite->ipcmem.mem.virt_base = devm_ioremap_wc(dev, r.start,
					resource_size(&r));
	if (!ipclite->ipcmem.mem.virt_base)
		return -ENOMEM;

	IPCLITE_OS_LOG(IPCLITE_DBG, "aux_base = %lx, size=%d,virt_base=%p\n",
			ipclite->ipcmem.mem.aux_base, ipclite->ipcmem.mem.size,
			ipclite->ipcmem.mem.virt_base);

	return ret;
}

static void ipcmem_init(struct ipclite_mem *ipcmem)
{
	int host, host0, host1;
	int i = 0;

	ipcmem->toc = ipcmem->mem.virt_base;
	IPCLITE_OS_LOG(IPCLITE_DBG, "toc_base = %p\n", ipcmem->toc);

	ipcmem->toc->hdr.size = IPCMEM_TOC_SIZE;
	IPCLITE_OS_LOG(IPCLITE_DBG, "toc->hdr.size = %d\n", ipcmem->toc->hdr.size);

	/*Fill in global partition details*/
	ipcmem->toc->toc_entry_global = ipcmem_toc_global_partition_entry;
	ipcmem->global_partition = (struct ipcmem_global_partition *)
								((char *)ipcmem->mem.virt_base +
						ipcmem_toc_global_partition_entry.base_offset);

	IPCLITE_OS_LOG(IPCLITE_DBG, "base_offset =%x,ipcmem->global_partition = %p\n",
				ipcmem_toc_global_partition_entry.base_offset,
				ipcmem->global_partition);

	ipcmem->global_partition->hdr = global_partition_hdr;

	IPCLITE_OS_LOG(IPCLITE_DBG, "hdr.type = %x,hdr.offset = %x,hdr.size = %d\n",
				ipcmem->global_partition->hdr.partition_type,
				ipcmem->global_partition->hdr.region_offset,
				ipcmem->global_partition->hdr.region_size);

	/* Fill in each IPCMEM TOC entry from ipcmem_toc_partition_entries config*/
	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		host0 = ipcmem_toc_partition_entries[i].host0;
		host1 = ipcmem_toc_partition_entries[i].host1;
		IPCLITE_OS_LOG(IPCLITE_DBG, "host0 = %d, host1=%d\n", host0, host1);

		ipcmem->toc->toc_entry[host0][host1] = ipcmem_toc_partition_entries[i];
		ipcmem->toc->toc_entry[host1][host0] = ipcmem_toc_partition_entries[i];

		if (host0 == IPCMEM_APPS && host1 == IPCMEM_APPS) {
			/* Updating the Global Channel Status for APPS Loopback */
			ipcmem->toc->toc_entry[host0][host1].status = CHANNEL_ACTIVE;
			ipcmem->toc->toc_entry[host1][host0].status = CHANNEL_ACTIVE;

			/* Updating Local Channel Status */
			channel_status_info[host1] = ipcmem->toc->toc_entry[host0][host1].status;

		} else if (host0 == IPCMEM_APPS || host1 == IPCMEM_APPS) {
			/* Updating the Global Channel Status */
			ipcmem->toc->toc_entry[host0][host1].status = CHANNEL_ACTIVATE_IN_PROGRESS;
			ipcmem->toc->toc_entry[host1][host0].status = CHANNEL_ACTIVATE_IN_PROGRESS;

			/* Updating Local Channel Status */
			if (host0 == IPCMEM_APPS)
				host = host1;
			else if (host1 == IPCMEM_APPS)
				host = host0;

			channel_status_info[host] = ipcmem->toc->toc_entry[host0][host1].status;
		}

		ipcmem->partition[i] = (struct ipcmem_partition *)
								((char *)ipcmem->mem.virt_base +
						ipcmem_toc_partition_entries[i].base_offset);

		IPCLITE_OS_LOG(IPCLITE_DBG, "partition[%d] = %p,partition_base_offset[%d]=%lx\n",
					i, ipcmem->partition[i],
					i, ipcmem_toc_partition_entries[i].base_offset);

		if (host0 == host1)
			ipcmem->partition[i]->hdr = loopback_partition_hdr;
		else
			ipcmem->partition[i]->hdr = default_partition_hdr;

		IPCLITE_OS_LOG(IPCLITE_DBG, "hdr.type = %x,hdr.offset = %x,hdr.size = %d\n",
					ipcmem->partition[i]->hdr.type,
					ipcmem->partition[i]->hdr.desc_offset,
					ipcmem->partition[i]->hdr.desc_size);
	}

	/*Making sure all writes for ipcmem configurations are completed*/
	wmb();

	ipcmem->toc->hdr.init_done = IPCMEM_INIT_COMPLETED;
	IPCLITE_OS_LOG(IPCLITE_DBG, "Ipcmem init completed\n");
}


/*Add VMIDs corresponding to EVA, CDSP and VPU to set IPCMEM access control*/
static int set_ipcmem_access_control(struct ipclite_info *ipclite)
{
	int ret = 0;
	int srcVM[1] = {VMID_HLOS};
	int destVM[2] = {VMID_HLOS, VMID_CDSP};
	int destVMperm[2] = {PERM_READ | PERM_WRITE,
				PERM_READ | PERM_WRITE};

	ret = hyp_assign_phys(ipclite->ipcmem.mem.aux_base,
				ipclite->ipcmem.mem.size, srcVM, 1,
				destVM, destVMperm, 2);
	return ret;
}

static int ipclite_channel_irq_init(struct device *parent, struct device_node *node,
								struct ipclite_channel *channel)
{
	int ret = 0;
	u32 index;
	struct ipclite_irq_info *irq_info;
	struct device *dev;
	char strs[MAX_CHANNEL_SIGNALS][IPCLITE_SIGNAL_LABEL_SIZE] = {
			"msg", "mem-init", "version", "test", "ssr", "debug"};

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->parent = parent;
	dev->of_node = node;
	dev_set_name(dev, "%s:%pOFn", dev_name(parent->parent), node);
	IPCLITE_OS_LOG(IPCLITE_DBG, "Registering %s device\n", dev_name(parent->parent));
	ret = device_register(dev);
	if (ret) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "failed to register ipclite child node\n");
		put_device(dev);
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "index",
				   &index);
	if (ret) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "failed to parse index\n");
		goto err_dev;
	}

	irq_info = &channel->irq_info[index];
	IPCLITE_OS_LOG(IPCLITE_DBG, "irq_info[%d]=%p\n", index, irq_info);

	irq_info->mbox_client.dev = dev;
	irq_info->mbox_client.knows_txdone = true;
	irq_info->mbox_chan = mbox_request_channel(&irq_info->mbox_client, 0);
	IPCLITE_OS_LOG(IPCLITE_DBG, "irq_info[%d].mbox_chan=%p\n", index, irq_info->mbox_chan);
	if (IS_ERR(irq_info->mbox_chan)) {
		if (PTR_ERR(irq_info->mbox_chan) != -EPROBE_DEFER)
			IPCLITE_OS_LOG(IPCLITE_ERR, "failed to acquire IPC channel\n");
		goto err_dev;
	}

	snprintf(irq_info->irqname, 32, "ipclite-signal-%s", strs[index]);
	irq_info->irq = of_irq_get(dev->of_node, 0);
	IPCLITE_OS_LOG(IPCLITE_DBG, "irq[%d] = %d\n", index, irq_info->irq);
	irq_info->signal_id = index;
	ret = devm_request_irq(dev, irq_info->irq,
			       ipclite_intr,
			       IRQF_NO_SUSPEND | IRQF_SHARED,
			       irq_info->irqname, irq_info);
	if (ret) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "failed to request IRQ\n");
		goto err_dev;
	}
	IPCLITE_OS_LOG(IPCLITE_DBG, "Interrupt init completed, ret = %d\n", ret);
	return 0;

err_dev:
	device_unregister(dev);
	kfree(dev);
	return ret;
}

int32_t get_global_partition_info(struct global_region_info *global_ipcmem)
{
	struct ipcmem_global_partition *global_partition;

	/* Check added to verify ipclite is initialized */
	if (!ipclite) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "IPCLite not initialized\n");
		return -ENOMEM;
	}

	if (!global_ipcmem)
		return -EINVAL;

	global_partition = ipclite->ipcmem.global_partition;
	global_ipcmem->virt_base = (void *)((char *)global_partition +
							global_partition->hdr.region_offset);
	global_ipcmem->size = (size_t)(global_partition->hdr.region_size);

	IPCLITE_OS_LOG(IPCLITE_DBG, "base = %p, size=%lx\n", global_ipcmem->virt_base,
									global_ipcmem->size);
	return 0;
}
EXPORT_SYMBOL(get_global_partition_info);

static struct ipcmem_partition_header *get_ipcmem_partition_hdr(struct ipclite_mem ipcmem, int local_pid,
								int remote_pid)
{
	return (struct ipcmem_partition_header *)((char *)ipcmem.mem.virt_base +
				ipcmem.toc->toc_entry[local_pid][remote_pid].base_offset);
}

static void ipclite_channel_release(struct device *dev)
{
	IPCLITE_OS_LOG(IPCLITE_INFO, "Releasing ipclite channel\n");
	kfree(dev);
}

/* Sets up following fields of IPCLite channel structure:
 *	remote_pid,tx_fifo, rx_fifo
 */
static int ipclite_channel_init(struct device *parent,
								struct device_node *node)
{
	struct ipclite_fifo *rx_fifo;
	struct ipclite_fifo *tx_fifo;

	struct device *dev;
	u32 local_pid, remote_pid, global_atomic;
	u32 *descs;
	int ret = 0;

	struct device_node *child;

	struct ipcmem_partition_header *partition_hdr;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->parent = parent;
	dev->of_node = node;
	dev->release = ipclite_channel_release;
	dev_set_name(dev, "%s:%pOFn", dev_name(parent->parent), node);
	IPCLITE_OS_LOG(IPCLITE_DBG, "Registering %s device\n", dev_name(parent->parent));
	ret = device_register(dev);
	if (ret) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "failed to register ipclite device\n");
		put_device(dev);
		kfree(dev);
		return ret;
	}

	local_pid = LOCAL_HOST;

	ret = of_property_read_u32(dev->of_node, "qcom,remote-pid",
				   &remote_pid);
	if (ret) {
		dev_err(dev, "failed to parse qcom,remote-pid\n");
		goto err_put_dev;
	}
	IPCLITE_OS_LOG(IPCLITE_DBG, "remote_pid = %d, local_pid=%d\n", remote_pid, local_pid);

	ipclite_hw_mutex = devm_kzalloc(dev, sizeof(*ipclite_hw_mutex), GFP_KERNEL);
	if (!ipclite_hw_mutex) {
		ret = -ENOMEM;
		goto err_put_dev;
	}

	ret = of_property_read_u32(dev->of_node, "global_atomic", &global_atomic);
	if (ret) {
		dev_err(dev, "failed to parse global_atomic\n");
		goto err_put_dev;
	}
	if (global_atomic == 0)
		global_atomic_support = GLOBAL_ATOMICS_DISABLED;

	rx_fifo = devm_kzalloc(dev, sizeof(*rx_fifo), GFP_KERNEL);
	tx_fifo = devm_kzalloc(dev, sizeof(*tx_fifo), GFP_KERNEL);
	if (!rx_fifo || !tx_fifo) {
		ret = -ENOMEM;
		goto err_put_dev;
	}
	IPCLITE_OS_LOG(IPCLITE_DBG, "rx_fifo = %p, tx_fifo=%p\n", rx_fifo, tx_fifo);

	partition_hdr = get_ipcmem_partition_hdr(ipclite->ipcmem,
						local_pid, remote_pid);
	IPCLITE_OS_LOG(IPCLITE_DBG, "partition_hdr = %p\n", partition_hdr);
	descs = (u32 *)((char *)partition_hdr + partition_hdr->desc_offset);
	IPCLITE_OS_LOG(IPCLITE_DBG, "descs = %p\n", descs);

	if (local_pid < remote_pid) {
		tx_fifo->fifo = (char *)partition_hdr + partition_hdr->fifo0_offset;
		tx_fifo->length = partition_hdr->fifo0_size;
		rx_fifo->fifo = (char *)partition_hdr + partition_hdr->fifo1_offset;
		rx_fifo->length = partition_hdr->fifo1_size;

		tx_fifo->tail = &descs[0];
		tx_fifo->head = &descs[1];
		rx_fifo->tail = &descs[2];
		rx_fifo->head = &descs[3];

	} else {
		tx_fifo->fifo = (char *)partition_hdr + partition_hdr->fifo1_offset;
		tx_fifo->length = partition_hdr->fifo1_size;
		rx_fifo->fifo = (char *)partition_hdr + partition_hdr->fifo0_offset;
		rx_fifo->length = partition_hdr->fifo0_size;

		rx_fifo->tail = &descs[0];
		rx_fifo->head = &descs[1];
		tx_fifo->tail = &descs[2];
		tx_fifo->head = &descs[3];
	}

	if (partition_hdr->type == LOOPBACK_PARTITION_TYPE) {
		rx_fifo->tail = tx_fifo->tail;
		rx_fifo->head = tx_fifo->head;
	}

	/* rx_fifo->reset = ipcmem_rx_reset;*/
	rx_fifo->avail = ipcmem_rx_avail;
	rx_fifo->peak = ipcmem_rx_peak;
	rx_fifo->advance = ipcmem_rx_advance;

	/* tx_fifo->reset = ipcmem_tx_reset;*/
	tx_fifo->avail = ipcmem_tx_avail;
	tx_fifo->write = ipcmem_tx_write;

	*rx_fifo->tail = 0;
	*tx_fifo->head = 0;

	/*Store Channel Information*/
	ipclite->channel[remote_pid].remote_pid = remote_pid;
	ipclite->channel[remote_pid].tx_fifo = tx_fifo;
	ipclite->channel[remote_pid].rx_fifo = rx_fifo;

	spin_lock_init(&ipclite->channel[remote_pid].tx_lock);

	for_each_available_child_of_node(dev->of_node, child) {
		ret = ipclite_channel_irq_init(dev, child,
				&ipclite->channel[remote_pid]);
		if (ret) {
			IPCLITE_OS_LOG(IPCLITE_ERR, "irq setup for ipclite channel failed\n");
			goto err_put_dev;
		}
	}

	ipclite->ipcmem.toc->recovery.configured_core[remote_pid] = CONFIGURED_CORE;
	IPCLITE_OS_LOG(IPCLITE_DBG, "Channel init completed, ret = %d\n", ret);
	return ret;

err_put_dev:
	ipclite->channel[remote_pid].channel_status = 0;
	device_unregister(dev);
	kfree(dev);
	return ret;
}

static void probe_subsystem(struct device *dev, struct device_node *np)
{
	int ret = 0;

	ret = ipclite_channel_init(dev, np);
	if (ret)
		IPCLITE_OS_LOG(IPCLITE_ERR, "IPCLite Channel init failed\n");
}

static ssize_t ipclite_dbg_lvl_write(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, host = 0;

	/* Parse the string from Sysfs Interface */
	ret = kstrtoint(buf, 0, &ipclite_debug_level);
	if (ret < IPCLITE_SUCCESS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Error parsing the sysfs value");
		return -IPCLITE_FAILURE;
	}

	/* Check if debug structure is initialized */
	if (!ipclite_dbg_info) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Debug structures not initialized\n");
		return -ENOMEM;
	}

	/* Update the Global Debug variable for FW cores */
	ipclite_dbg_info->debug_level = ipclite_debug_level;

	/* Memory Barrier to make sure all writes are completed */
	wmb();

	/* Signal other cores for updating the debug information */
	for (host = 1; host < IPCMEM_NUM_HOSTS; host++) {
		if (ipclite->ipcmem.toc->recovery.configured_core[host]) {
			ret = ipclite_send_debug_info(host);
			if (ret < IPCLITE_SUCCESS)
				IPCLITE_OS_LOG(IPCLITE_ERR, "Failed to send the debug info %d\n",
											host);
			else
				IPCLITE_OS_LOG(IPCLITE_DBG, "Debug info sent to host %d\n", host);
		}
	}

	return count;
}

static ssize_t ipclite_dbg_ctrl_write(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0, host = 0;

	/* Parse the string from Sysfs Interface */
	ret = kstrtoint(buf, 0, &ipclite_debug_control);
	if (ret < IPCLITE_SUCCESS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Error parsing the sysfs value");
		return -IPCLITE_FAILURE;
	}

	/* Check if debug structures are initialized */
	if (!ipclite_dbg_info || !ipclite_dbg_struct || !ipclite_dbg_inmem) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Debug structures not initialized\n");
		return -ENOMEM;
	}

	/* Update the Global Debug variable for FW cores */
	ipclite_dbg_info->debug_control = ipclite_debug_control;

	/* Memory Barrier to make sure all writes are completed */
	wmb();

	/* Signal other cores for updating the debug information */
	for (host = 1; host < IPCMEM_NUM_HOSTS; host++) {
		if (ipclite->ipcmem.toc->recovery.configured_core[host]) {
			ret = ipclite_send_debug_info(host);
			if (ret < IPCLITE_SUCCESS)
				IPCLITE_OS_LOG(IPCLITE_ERR, "Failed to send the debug info %d\n",
											host);
			else
				IPCLITE_OS_LOG(IPCLITE_DBG, "Debug info sent to host %d\n", host);
		}
	}

	return count;
}

static ssize_t ipclite_dbg_dump_write(struct kobject *kobj,
				struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	/* Parse the string from Sysfs Interface */
	ret = kstrtoint(buf, 0, &ipclite_debug_dump);
	if (ret < IPCLITE_SUCCESS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Error parsing the sysfs value");
		return -IPCLITE_FAILURE;
	}

	/* Check if debug structures are initialized */
	if (!ipclite_dbg_info || !ipclite_dbg_struct || !ipclite_dbg_inmem) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Debug structures not initialized\n");
		return -ENOMEM;
	}

	/* Dump the debug information */
	if (ipclite_debug_dump & IPCLITE_DUMP_DBG_STRUCT)
		ipclite_dump_debug_struct();

	if (ipclite_debug_dump & IPCLITE_DUMP_INMEM_LOG)
		ipclite_dump_inmem_logs();

	return count;
}

struct kobj_attribute sysfs_dbg_lvl = __ATTR(ipclite_debug_level, 0660,
					NULL, ipclite_dbg_lvl_write);
struct kobj_attribute sysfs_dbg_ctrl = __ATTR(ipclite_debug_control, 0660,
					NULL, ipclite_dbg_ctrl_write);
struct kobj_attribute sysfs_dbg_dump = __ATTR(ipclite_debug_dump, 0660,
					NULL, ipclite_dbg_dump_write);

static int ipclite_debug_sysfs_setup(void)
{
	/* Creating a directory in /sys/kernel/ */
	sysfs_kobj = kobject_create_and_add("ipclite", kernel_kobj);
	if (!sysfs_kobj) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot create and add sysfs directory\n");
		return -IPCLITE_FAILURE;
	}

	/* Creating sysfs files/interfaces for debug */
	if (sysfs_create_file(sysfs_kobj, &sysfs_dbg_lvl.attr)) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot create sysfs debug level file\n");
		return -IPCLITE_FAILURE;
	}

	if (sysfs_create_file(sysfs_kobj, &sysfs_dbg_ctrl.attr)) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot create sysfs debug control file\n");
		return -IPCLITE_FAILURE;
	}

	if (sysfs_create_file(sysfs_kobj, &sysfs_dbg_dump.attr)) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Cannot create sysfs debug dump file\n");
		return -IPCLITE_FAILURE;
	}

	return IPCLITE_SUCCESS;
}

static int ipclite_debug_info_setup(void)
{
	/* Setting up the Debug Structures */
	ipclite_dbg_info = (struct ipclite_debug_info *)(((char *)ipclite->ipcmem.mem.virt_base +
						ipclite->ipcmem.mem.size) - IPCLITE_DEBUG_SIZE);
	if (!ipclite_dbg_info)
		return -EADDRNOTAVAIL;

	ipclite_dbg_struct = (struct ipclite_debug_struct *)
					(((char *)ipclite_dbg_info + IPCLITE_DEBUG_INFO_SIZE) +
					(sizeof(*ipclite_dbg_struct) * IPCMEM_APPS));
	if (!ipclite_dbg_struct)
		return -EADDRNOTAVAIL;

	ipclite_dbg_inmem = (struct ipclite_debug_inmem_buf *)
					(((char *)ipclite_dbg_info + IPCLITE_DEBUG_INFO_SIZE) +
					(sizeof(*ipclite_dbg_struct) * IPCMEM_NUM_HOSTS));

	if (!ipclite_dbg_inmem)
		return -EADDRNOTAVAIL;

	IPCLITE_OS_LOG(IPCLITE_DBG, "virtual_base_ptr = %p total_size : %d debug_size : %d\n",
		ipclite->ipcmem.mem.virt_base, ipclite->ipcmem.mem.size, IPCLITE_DEBUG_SIZE);
	IPCLITE_OS_LOG(IPCLITE_DBG, "dbg_info : %p dbg_struct : %p dbg_inmem : %p\n",
					ipclite_dbg_info, ipclite_dbg_struct, ipclite_dbg_inmem);

	return IPCLITE_SUCCESS;
}

static int ipclite_probe(struct platform_device *pdev)
{
	int ret = 0;
	int hwlock_id;
	struct ipcmem_region *mem;
	struct device_node *cn;
	struct device_node *pn = pdev->dev.of_node;
	struct ipclite_channel broadcast;

	ipclite = kzalloc(sizeof(*ipclite), GFP_KERNEL);
	if (!ipclite) {
		ret = -ENOMEM;
		goto error;
	}

	ipclite->dev = &pdev->dev;

	hwlock_id = of_hwspin_lock_get_id(pn, 0);
	if (hwlock_id < 0) {
		if (hwlock_id != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to retrieve hwlock\n");
		ret = hwlock_id;
		goto error;
	}
	IPCLITE_OS_LOG(IPCLITE_DBG, "Hwlock id retrieved, hwlock_id=%d\n", hwlock_id);

	ipclite->hwlock = hwspin_lock_request_specific(hwlock_id);
	if (!ipclite->hwlock) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Failed to assign hwlock_id\n");
		ret = -ENXIO;
		goto error;
	}
	IPCLITE_OS_LOG(IPCLITE_DBG, "Hwlock id assigned successfully, hwlock=%p\n",
									ipclite->hwlock);

	/* Initializing Local Mutex Lock for SSR functionality */
	mutex_init(&ssr_mutex);

	ret = map_ipcmem(ipclite, "memory-region");
	if (ret) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "failed to map ipcmem\n");
		goto release;
	}
	mem = &(ipclite->ipcmem.mem);
	memset(mem->virt_base, 0, mem->size);

	ret = set_ipcmem_access_control(ipclite);
	if (ret) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "failed to set access control policy\n");
		goto release;
	}

	ipcmem_init(&ipclite->ipcmem);

	/* Set up sysfs for debug  */
	ret = ipclite_debug_sysfs_setup();
	if (ret != IPCLITE_SUCCESS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Failed to Set up IPCLite Debug Sysfs\n");
		goto release;
	}

	/* Mapping Debug Memory */
	ret = ipclite_debug_info_setup();
	if (ret != IPCLITE_SUCCESS) {
		IPCLITE_OS_LOG(IPCLITE_ERR, "Failed to Set up IPCLite Debug Structures\n");
		goto release;
	}

	/* Setup Channel for each Remote Subsystem */
	for_each_available_child_of_node(pn, cn)
		probe_subsystem(&pdev->dev, cn);
	/* Broadcast init_done signal to all subsystems once mbox channels
	 * are set up
	 */
	broadcast = ipclite->channel[IPCMEM_APPS];
	ret = mbox_send_message(broadcast.irq_info[IPCLITE_MEM_INIT_SIGNAL].mbox_chan,
								 NULL);
	if (ret < 0)
		goto mem_release;

	mbox_client_txdone(broadcast.irq_info[IPCLITE_MEM_INIT_SIGNAL].mbox_chan, 0);

	if (global_atomic_support) {
		ipclite->ipcmem.toc->ipclite_features.global_atomic_support =
							GLOBAL_ATOMICS_ENABLED;
	} else {
		ipclite->ipcmem.toc->ipclite_features.global_atomic_support =
							GLOBAL_ATOMICS_DISABLED;
	}

	IPCLITE_OS_LOG(IPCLITE_DBG, "global_atomic_support : %d\n",
		ipclite->ipcmem.toc->ipclite_features.global_atomic_support);

	/* hw mutex callbacks */
	ipclite_hw_mutex->acquire = ipclite_hw_mutex_acquire;
	ipclite_hw_mutex->release = ipclite_hw_mutex_release;

	/* store to ipclite structure */
	ipclite->ipclite_hw_mutex = ipclite_hw_mutex;

	/* initialize hwlock owner to invalid host */
	ipclite->ipcmem.toc->recovery.global_atomic_hwlock_owner = IPCMEM_INVALID_HOST;

	/* Update the Global Debug variable for FW cores */
	ipclite_dbg_info->debug_level = ipclite_debug_level;
	ipclite_dbg_info->debug_control = ipclite_debug_control;

	IPCLITE_OS_LOG(IPCLITE_INFO, "IPCLite probe completed successfully\n");
	return ret;

mem_release:
	/* If the remote subsystem has already completed the init and actively
	 * using IPCMEM, re-assigning IPCMEM memory back to HLOS can lead to crash
	 * Solution: Either we don't take back the memory or make sure APPS completes
	 * init before any other subsystem initializes IPCLite (we won't have to send
	 * braodcast)
	 */
release:
	kfree(ipclite);
error:
	IPCLITE_OS_LOG(IPCLITE_ERR, "IPCLite probe failed\n");
	return ret;
}

static const struct of_device_id ipclite_of_match[] = {
	{ .compatible = "qcom,ipclite"},
	{}
};
MODULE_DEVICE_TABLE(of, ipclite_of_match);

static struct platform_driver ipclite_driver = {
	.probe = ipclite_probe,
	.driver = {
		.name = "ipclite",
		.of_match_table = ipclite_of_match,
	},
};

module_platform_driver(ipclite_driver);

MODULE_DESCRIPTION("IPCLite Driver");
MODULE_LICENSE("GPL v2");
MODULE_SOFTDEP("pre: qcom_hwspinlock");
