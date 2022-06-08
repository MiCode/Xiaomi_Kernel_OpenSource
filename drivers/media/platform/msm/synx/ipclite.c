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

#include <soc/qcom/secure_buffer.h>

#include "ipclite_client.h"
#include "ipclite.h"

#define VMID_HLOS       3
#define VMID_SSC_Q6     5
#define VMID_ADSP_Q6    6
#define VMID_CDSP       30
#define GLOBAL_ATOMICS_ENABLED	1
#define GLOBAL_ATOMICS_DISABLED	0

static struct ipclite_info *ipclite;
static struct ipclite_client synx_client;
static struct ipclite_client test_client;
struct ipclite_hw_mutex_ops *ipclite_hw_mutex;

u32 global_atomic_support = GLOBAL_ATOMICS_ENABLED;

#define FIFO_FULL_RESERVE 8
#define FIFO_ALIGNMENT 8

static void ipclite_hw_mutex_acquire(void)
{
	int32_t ret;

	if (ipclite != NULL) {
		if (!ipclite->ipcmem.toc->ipclite_features.global_atomic_support) {
			ret = hwspin_lock_timeout_irqsave(ipclite->hwlock,
					HWSPINLOCK_TIMEOUT,
					&ipclite->ipclite_hw_mutex->flags);
			if (ret)
				pr_err("Hw mutex lock acquire failed\n");

			pr_debug("Hw mutex lock acquired\n");
		}
	}
}

static void ipclite_hw_mutex_release(void)
{
	if (ipclite != NULL) {
		if (!ipclite->ipcmem.toc->ipclite_features.global_atomic_support) {
			hwspin_unlock_irqrestore(ipclite->hwlock,
				&ipclite->ipclite_hw_mutex->flags);
			pr_debug("Hw mutex lock release\n");
		}
	}
}

void ipclite_atomic_init_u32(ipclite_atomic_uint32_t *addr, uint32_t data)
{
	atomic_set(addr, data);
	pr_debug("%s new_val = %d\n", __func__, (*(uint32_t *)addr));
}
EXPORT_SYMBOL(ipclite_atomic_init_u32);

void ipclite_atomic_init_i32(ipclite_atomic_int32_t *addr, int32_t data)
{
	atomic_set(addr, data);
	pr_debug("%s new_val = %d\n", __func__, (*(int32_t *)addr));
}
EXPORT_SYMBOL(ipclite_atomic_init_i32);

void ipclite_global_atomic_store_u32(ipclite_atomic_uint32_t *addr, uint32_t data)
{
	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	atomic_set(addr, data);
	pr_debug("%s new_val = %d\n", __func__, (*(uint32_t *)addr));

	/* callback to release hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->release();
}
EXPORT_SYMBOL(ipclite_global_atomic_store_u32);

void ipclite_global_atomic_store_i32(ipclite_atomic_int32_t *addr, int32_t data)
{
	/* callback to acquire hw mutex lock if atomic support is not enabled */
	ipclite->ipclite_hw_mutex->acquire();

	atomic_set(addr, data);
	pr_debug("%s new_val = %d\n", __func__, (*(int32_t *)addr));

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
	pr_debug("%s ret = %d, new_val = %d\n", __func__,  ret, (*(uint32_t *)addr));

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
	pr_debug("%s ret = %d, new_val = %d\n", __func__, ret, (*(int32_t *)addr));

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
	pr_debug("%s ret = %d, new_val = %d\n", __func__, ret, (*(uint32_t *)addr));

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
	pr_debug("%s ret = %d, new_val = %d\n", __func__, ret, (*(uint32_t *)addr));

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
	pr_debug("%s ret = %d new_val = %d\n", __func__, ret, (*(int32_t *)addr));

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
	pr_debug("%s ret = %d new_val = %d\n", __func__, ret, (*(int32_t *)addr));

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
	pr_debug("head=%d, tail=%d\n", head, tail);
	if (head < tail)
		len = rx_fifo->length - tail + head;
	else
		len = head - tail;

	if (WARN_ON_ONCE(len > rx_fifo->length))
		len = 0;
	pr_debug("len=%d\n", len);
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
				  size_t count)
{
	u32 tail;

	tail = le32_to_cpu(*rx_fifo->tail);

	tail += count;
	if (tail >= rx_fifo->length)
		tail %= rx_fifo->length;

	*rx_fifo->tail = cpu_to_le32(tail);
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
				const void *data, size_t dlen)
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
	pr_debug("head = %d\n", *tx_fifo->head);
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
	channel->rx_fifo->advance(channel->rx_fifo, count);
}

static size_t ipclite_tx_avail(struct ipclite_channel *channel)
{
	return channel->tx_fifo->avail(channel->tx_fifo);
}

static void ipclite_tx_write(struct ipclite_channel *channel,
				const void *data, size_t dlen)
{
	channel->tx_fifo->write(channel->tx_fifo, data, dlen);
}

static int ipclite_rx_data(struct ipclite_channel *channel, size_t avail)
{
	uint64_t data;
	int ret = 0;

	if (avail < sizeof(data)) {
		pr_err("Not enough data in fifo\n");
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
		pr_err("Not enough data in fifo\n");
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

	pr_debug("Interrupt received\n");
	irq_info = (struct ipclite_irq_info *)data;
	channel = container_of(irq_info, struct ipclite_channel, irq_info[irq_info->signal_id]);

	if (irq_info->signal_id == IPCLITE_MSG_SIGNAL) {
		for (;;) {
			avail = ipclite_rx_avail(channel);
			if (avail < sizeof(msg))
				break;

			ret = ipclite_rx_data(channel, avail);
		}
		pr_debug("checking messages in rx_fifo done\n");
	} else if (irq_info->signal_id == IPCLITE_VERSION_SIGNAL) {
		/* check_version_compatibility();*/
		pr_debug("version matching sequence completed\n");
	} else if (irq_info->signal_id == IPCLITE_TEST_SIGNAL) {
		for (;;) {
			avail = ipclite_rx_avail(channel);
			if (avail < sizeof(msg))
				break;

			ret = ipclite_rx_test_data(channel, avail);
		}
		pr_debug("checking messages in rx_fifo done\n");
	} else {
		pr_err("wrong interrupt signal received, signal_id =%d\n", irq_info->signal_id);
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

int ipclite_msg_send(int32_t proc_id, uint64_t data)
{
	int ret = 0;

	if (proc_id < 0 || proc_id >= IPCMEM_NUM_HOSTS) {
		pr_err("Invalid proc_id %d\n", proc_id);
		return -EINVAL;
	}

	if (ipclite->channel[proc_id].channel_status != ACTIVE_CHANNEL) {
		pr_err("Cannot send msg to remote client. Channel inactive\n");
		return -ENXIO;
	}

	ret = ipclite_tx(&ipclite->channel[proc_id], data, sizeof(data),
								IPCLITE_MSG_SIGNAL);
	pr_debug("Message send completed with ret=%d\n", ret);
	return ret;
}
EXPORT_SYMBOL(ipclite_msg_send);

int ipclite_register_client(IPCLite_Client cb_func_ptr, void *priv)
{
	if (!cb_func_ptr) {
		pr_err("Invalid callback pointer\n");
		return -EINVAL;
	}
	synx_client.callback = cb_func_ptr;
	synx_client.priv_data = priv;
	synx_client.reg_complete = 1;
	pr_debug("Client Registration completed\n");
	return 0;
}
EXPORT_SYMBOL(ipclite_register_client);

int ipclite_test_msg_send(int32_t proc_id, uint64_t data)
{
	int ret = 0;

	if (proc_id < 0 || proc_id >= IPCMEM_NUM_HOSTS) {
		pr_err("Invalid proc_id %d\n", proc_id);
		return -EINVAL;
	}

	/* Limit Message Sending without Client Registration */
	if (ipclite->channel[proc_id].channel_status != ACTIVE_CHANNEL) {
		pr_err("Cannot send msg to remote client. Channel inactive\n");
		return -ENXIO;
	}

	ret = ipclite_tx(&ipclite->channel[proc_id], data, sizeof(data),
									IPCLITE_TEST_SIGNAL);
	pr_debug("Message send completed with ret=%d\n", ret);
	return ret;
}
EXPORT_SYMBOL(ipclite_test_msg_send);

int ipclite_register_test_client(IPCLite_Client cb_func_ptr, void *priv)
{
	if (!cb_func_ptr) {
		pr_err("Invalid callback pointer\n");
		return -EINVAL;
	}
	test_client.callback = cb_func_ptr;
	test_client.priv_data = priv;
	test_client.reg_complete = 1;
	pr_debug("Test Client Registration Completed\n");
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
		pr_err("No %s specified\n", name);
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

	pr_debug("aux_base = %lx, size=%d,virt_base=%p\n",
			ipclite->ipcmem.mem.aux_base, ipclite->ipcmem.mem.size,
			ipclite->ipcmem.mem.virt_base);

	return ret;
}

static void ipcmem_init(struct ipclite_mem *ipcmem)
{
	int host0, host1;
	int i = 0;

	ipcmem->toc = ipcmem->mem.virt_base;
	pr_debug("toc_base = %p\n", ipcmem->toc);

	ipcmem->toc->hdr.size = IPCMEM_TOC_SIZE;
	pr_debug("toc->hdr.size = %d\n", ipcmem->toc->hdr.size);

	/*Fill in global partition details*/
	ipcmem->toc->toc_entry_global = ipcmem_toc_global_partition_entry;
	ipcmem->global_partition = (struct ipcmem_global_partition *)
								((char *)ipcmem->mem.virt_base +
						ipcmem_toc_global_partition_entry.base_offset);

	pr_debug("base_offset =%x,ipcmem->global_partition = %p\n",
				ipcmem_toc_global_partition_entry.base_offset,
				ipcmem->global_partition);

	ipcmem->global_partition->hdr = global_partition_hdr;

	pr_debug("hdr.type = %x,hdr.offset = %x,hdr.size = %d\n",
				ipcmem->global_partition->hdr.partition_type,
				ipcmem->global_partition->hdr.region_offset,
				ipcmem->global_partition->hdr.region_size);

	/* Fill in each IPCMEM TOC entry from ipcmem_toc_partition_entries config*/
	for (i = 0; i < MAX_PARTITION_COUNT; i++) {
		host0 = ipcmem_toc_partition_entries[i].host0;
		host1 = ipcmem_toc_partition_entries[i].host1;
		pr_debug("host0 = %d, host1=%d\n", host0, host1);

		ipcmem->toc->toc_entry[host0][host1] = ipcmem_toc_partition_entries[i];
		ipcmem->toc->toc_entry[host1][host0] = ipcmem_toc_partition_entries[i];

		ipcmem->partition[i] = (struct ipcmem_partition *)
								((char *)ipcmem->mem.virt_base +
						ipcmem_toc_partition_entries[i].base_offset);

		pr_debug("partition[%d] = %p,partition_base_offset[%d]=%lx\n",
					i, ipcmem->partition[i],
					i, ipcmem_toc_partition_entries[i].base_offset);

		if (host0 == host1)
			ipcmem->partition[i]->hdr = loopback_partition_hdr;
		else
			ipcmem->partition[i]->hdr = default_partition_hdr;

		pr_debug("hdr.type = %x,hdr.offset = %x,hdr.size = %d\n",
					ipcmem->partition[i]->hdr.type,
					ipcmem->partition[i]->hdr.desc_offset,
					ipcmem->partition[i]->hdr.desc_size);
	}

	/*Making sure all writes for ipcmem configurations are completed*/
	wmb();

	ipcmem->toc->hdr.init_done = IPCMEM_INIT_COMPLETED;
	pr_debug("Ipcmem init completed\n");
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
	char strs[4][9] = {"msg", "mem-init", "version", "test"};
	struct ipclite_irq_info *irq_info;
	struct device *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->parent = parent;
	dev->of_node = node;
	dev_set_name(dev, "%s:%pOFn", dev_name(parent->parent), node);
	pr_debug("Registering %s device\n", dev_name(parent->parent));
	ret = device_register(dev);
	if (ret) {
		pr_err("failed to register ipclite child node\n");
		put_device(dev);
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "index",
				   &index);
	if (ret) {
		pr_err("failed to parse index\n");
		goto err_dev;
	}

	irq_info = &channel->irq_info[index];
	pr_debug("irq_info[%d]=%p\n", index, irq_info);

	irq_info->mbox_client.dev = dev;
	irq_info->mbox_client.knows_txdone = true;
	irq_info->mbox_chan = mbox_request_channel(&irq_info->mbox_client, 0);
	pr_debug("irq_info[%d].mbox_chan=%p\n", index, irq_info->mbox_chan);
	if (IS_ERR(irq_info->mbox_chan)) {
		if (PTR_ERR(irq_info->mbox_chan) != -EPROBE_DEFER)
			pr_err("failed to acquire IPC channel\n");
		goto err_dev;
	}

	snprintf(irq_info->irqname, 32, "ipclite-signal-%s", strs[index]);
	irq_info->irq = of_irq_get(dev->of_node, 0);
	pr_debug("irq[%d] = %d\n", index, irq_info->irq);
	irq_info->signal_id = index;
	ret = devm_request_irq(dev, irq_info->irq,
			       ipclite_intr,
			       IRQF_NO_SUSPEND | IRQF_SHARED,
			       irq_info->irqname, irq_info);
	if (ret) {
		pr_err("failed to request IRQ\n");
		goto err_dev;
	}
	pr_debug("Interrupt init completed, ret = %d\n", ret);
	return 0;

err_dev:
	device_unregister(dev);
	kfree(dev);
	return ret;
}

int32_t get_global_partition_info(struct global_region_info *global_ipcmem)
{
	struct ipcmem_global_partition *global_partition;

	if (!global_ipcmem)
		return -EINVAL;

	global_partition = ipclite->ipcmem.global_partition;
	global_ipcmem->virt_base = (void *)((char *)global_partition +
							global_partition->hdr.region_offset);
	global_ipcmem->size = (size_t)(global_partition->hdr.region_size);

	pr_debug("base = %p, size=%lx\n", global_ipcmem->virt_base,
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
	pr_info("Releasing ipclite channel\n");
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
	pr_debug("Registering %s device\n", dev_name(parent->parent));
	ret = device_register(dev);
	if (ret) {
		pr_err("failed to register ipclite device\n");
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
	pr_debug("remote_pid = %d, local_pid=%d\n", remote_pid, local_pid);

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
	pr_debug("rx_fifo = %p, tx_fifo=%p\n", rx_fifo, tx_fifo);

	partition_hdr = get_ipcmem_partition_hdr(ipclite->ipcmem,
						local_pid, remote_pid);
	pr_debug("partition_hdr = %p\n", partition_hdr);
	descs = (u32 *)((char *)partition_hdr + partition_hdr->desc_offset);
	pr_debug("descs = %p\n", descs);

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
			pr_err("irq setup for ipclite channel failed\n");
			goto err_put_dev;
		}
	}
	ipclite->channel[remote_pid].channel_status = ACTIVE_CHANNEL;
	pr_debug("Channel init completed, ret = %d\n", ret);
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
		pr_err("IPCLite Channel init failed\n");
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
	pr_debug("Hwlock id retrieved, hwlock_id=%d\n", hwlock_id);

	ipclite->hwlock = hwspin_lock_request_specific(hwlock_id);
	if (!ipclite->hwlock) {
		pr_err("Failed to assign hwlock_id\n");
		ret = -ENXIO;
		goto error;
	}
	pr_debug("Hwlock id assigned successfully, hwlock=%p\n", ipclite->hwlock);

	ret = map_ipcmem(ipclite, "memory-region");
	if (ret) {
		pr_err("failed to map ipcmem\n");
		goto release;
	}
	mem = &(ipclite->ipcmem.mem);
	memset(mem->virt_base, 0, mem->size);

	ret = set_ipcmem_access_control(ipclite);
	if (ret) {
		pr_err("failed to set access control policy\n");
		goto release;
	}

	ipcmem_init(&ipclite->ipcmem);

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

	pr_debug("global_atomic_support : %d\n",
		ipclite->ipcmem.toc->ipclite_features.global_atomic_support);

	/* hw mutex callbacks */
	ipclite_hw_mutex->acquire = ipclite_hw_mutex_acquire;
	ipclite_hw_mutex->release = ipclite_hw_mutex_release;

	/* store to ipclite structure */
	ipclite->ipclite_hw_mutex = ipclite_hw_mutex;

	pr_info("IPCLite probe completed successfully\n");
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
	pr_err("IPCLite probe failed\n");
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
