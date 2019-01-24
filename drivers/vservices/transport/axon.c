/*
 * drivers/vservices/transport/axon.c
 *
 * Copyright (c) 2015-2018 General Dynamics
 * Copyright (c) 2015 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This is the OKL4 Virtual Services transport driver for OKL4 Microvisor
 * Axons (virtual inter-Cell DMA engines).
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/log2.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/dma-contiguous.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0)
#include <asm/dma-contiguous.h>
#endif
#include <linux/vmalloc.h>
#include <linux/mmzone.h>
#include <asm-generic/okl4_virq.h>
#include <asm/byteorder.h>

#include <vservices/transport.h>
#include <vservices/session.h>
#include <vservices/service.h>

#include <microvisor/microvisor.h>

#include "../transport.h"
#include "../session.h"
#include "../debug.h"

#define DRIVER_AUTHOR "Cog Systems Pty Ltd"
#define DRIVER_DESC "OKL4 vServices Axon Transport Driver"
#define DRIVER_NAME "vtransport_axon"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0) || \
	defined(CONFIG_NO_DEPRECATED_MEMORY_BARRIERS)
#define smp_mb__before_atomic_dec smp_mb__before_atomic
#define smp_mb__before_atomic_inc smp_mb__before_atomic
#define smp_mb__after_atomic_dec smp_mb__after_atomic
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
#define DMA_ATTRS unsigned long
#else
#define DMA_ATTRS struct dma_attrs *
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 12, 0) && \
	!defined(CONFIG_CMA)
static inline struct cma *dev_get_cma_area(struct device *dev)
{
	return NULL;
}
#endif

static struct kmem_cache *mbuf_cache;

struct child_device {
	struct device *dev;
	struct list_head list;
};

/* Number of services in the transport array to allocate at a time */
#define SERVICES_ALLOC_CHUNK	16
#define MSG_SEND_FREE_BUFS	VS_SERVICE_ID_RESERVED_1

/* The maximum value we allow for the free_bufs_balance counter */
#define MAX_BALANCE		1

/*
 * The free bufs quota must be enough to take free_bufs_balance from its
 * minimum to its maximum.
 */
#define FREE_BUFS_QUOTA		(MAX_BALANCE * 2)

/*
 * The free bufs retry delay is the period in jiffies that we delay retrying
 * after an out-of-memory condition when trying to send a free bufs message.
 */
#define FREE_BUFS_RETRY_DELAY	2

/* The minimum values we permit for queue and message size. */
#define MIN_QUEUE_SIZE		((size_t)4)
#define MIN_MSG_SIZE		(32 - sizeof(vs_service_id_t))

/*
 * The maximum size for a batched receive. This should be larger than the
 * maximum message size, and large enough to avoid excessive context switching
 * overheads, yet small enough to avoid blocking the tasklet queue for too
 * long.
 */
#define MAX_TRANSFER_CHUNK	65536

#define INC_MOD(x, m) {						\
	x++;							\
	if (x == m) x = 0;					\
}

/* Local Axon cleanup workqueue */
struct workqueue_struct *work_queue;

/*
 * True if there is only one physical segment being used for kernel memory
 * allocations. If this is false, the device must have a usable CMA region.
 */
static bool okl4_single_physical_segment;

/* OKL4 MMU capability. */
static okl4_kcap_t okl4_mmu_cap;

/*
 * Per-service TX buffer allocation pool.
 *
 * We cannot use a normal DMA pool for TX buffers, because alloc_mbuf can be
 * called with GFP_ATOMIC, and a normal DMA pool alloc will take pages from
 * a global emergency pool if GFP_WAIT is not set. The emergency pool is not
 * guaranteed to be in the same physical segment as this device's DMA region,
 * so it might not be usable by the axon.
 *
 * Using a very simple allocator with preallocated memory also speeds up the
 * TX path.
 *
 * RX buffers use a standard Linux DMA pool, shared between all services,
 * rather than this struct. They are preallocated by definition, so the speed
 * of the allocator doesn't matter much for them. Also, they're always
 * allocated with GFP_KERNEL (which includes GFP_WAIT) so the normal DMA pool
 * will use memory from the axon's contiguous region.
 */
struct vs_axon_tx_pool {
	struct vs_transport_axon *transport;
	struct kref kref;

	void *base_vaddr;
	dma_addr_t base_laddr;

	unsigned alloc_order;
	unsigned count;

	struct work_struct free_work;
	unsigned long alloc_bitmap[];
};

struct vs_axon_rx_freelist_entry {
	struct list_head list;
	dma_addr_t laddr;
};

/* Service info */
struct vs_mv_service_info {
	struct vs_service_device *service;

	/* True if the session has started the service */
	bool ready;

	/* Number of send buffers we have allocated, in total. */
	atomic_t send_inflight;

	/*
	 * Number of send buffers we have allocated but not yet sent.
	 * This should always be zero if ready is false.
	 */
	atomic_t send_alloc;

	/*
	 * Number of receive buffers we have received and not yet freed.
	 * This should always be zero if ready is false.
	 */
	atomic_t recv_inflight;

	/*
	 * Number of receive buffers we have freed, but not told the other end
	 * about yet.
	 *
	 * The watermark is the maximum number of freed buffers we can
	 * accumulate before we send a dummy message to the remote end to ack
	 * them. This is used in situations where the protocol allows the remote
	 * end to reach its send quota without guaranteeing a reply; the dummy
	 * message lets it make progress even if our service driver doesn't send
	 * an answer that we can piggy-back the acks on.
	 */
	atomic_t recv_freed;
	unsigned int recv_freed_watermark;

	/*
	 * Number of buffers that have been left allocated after a reset. If
	 * this count is nonzero, then the service has been disabled by the
	 * session layer, and needs to be re-enabled when it reaches zero.
	 */
	atomic_t outstanding_frees;

	/* TX allocation pool */
	struct vs_axon_tx_pool *tx_pool;

	/* RX allocation count */
	unsigned rx_allocated;

	/* Reference count for this info struct. */
	struct kref kref;

	/* RCU head for cleanup */
	struct rcu_head rcu_head;
};

/*
 * Transport readiness state machine
 *
 * This is similar to the service readiness state machine, but simpler,
 * because there are fewer transition triggers.
 *
 * The states are:
 * INIT: Initial state. This occurs transiently during probe.
 * LOCAL_RESET: We have initiated a reset at this end, but the remote end has
 * not yet acknowledged it. We will enter the RESET state on receiving
 * acknowledgement.
 * RESET: The transport is inactive at both ends, and the session layer has
 * not yet told us to start activating.
 * LOCAL_READY: The session layer has told us to start activating, and we
 * have notified the remote end that we're ready.
 * REMOTE_READY: The remote end has notified us that it is ready, but the
 * local session layer hasn't decided to become ready yet.
 * ACTIVE: Both ends are ready to communicate.
 * SHUTDOWN: The transport is shutting down and should not become ready.
 */
enum vs_transport_readiness {
	VS_TRANSPORT_INIT = 0,
	VS_TRANSPORT_LOCAL_RESET,
	VS_TRANSPORT_RESET,
	VS_TRANSPORT_LOCAL_READY,
	VS_TRANSPORT_REMOTE_READY,
	VS_TRANSPORT_ACTIVE,
	VS_TRANSPORT_SHUTDOWN,
};

/*
 * Transport reset / ready VIRQ payload bits
 */
enum vs_transport_reset_virq {
	VS_TRANSPORT_VIRQ_RESET_REQ = (1 << 0),
	VS_TRANSPORT_VIRQ_RESET_ACK = (1 << 1),
	VS_TRANSPORT_VIRQ_READY = (1 << 2),
};

/*
 * Internal definitions of the transport and message buffer structures.
 */
#define MAX_NOTIFICATION_LINES 16 /* Enough for 512 notifications each way */

struct vs_transport_axon {
	struct device *axon_dev;

	struct okl4_axon_tx *tx;
	struct okl4_axon_queue_entry *tx_descs;
	struct vs_axon_tx_pool **tx_pools;
	struct okl4_axon_rx *rx;
	struct okl4_axon_queue_entry *rx_descs;
	void **rx_ptrs;

	dma_addr_t tx_phys, rx_phys;
	size_t tx_size, rx_size;

	okl4_kcap_t segment;
	okl4_laddr_t segment_base;

	okl4_kcap_t tx_cap, rx_cap, reset_cap;
	unsigned int tx_irq, rx_irq, reset_irq;
	okl4_interrupt_number_t reset_okl4_irq;

	unsigned int notify_tx_nirqs;
	okl4_kcap_t notify_cap[MAX_NOTIFICATION_LINES];
	unsigned int notify_rx_nirqs;
	unsigned int notify_irq[MAX_NOTIFICATION_LINES];

	bool is_server;
	size_t msg_size, queue_size;

	/*
	 * The handle to the device tree node for the virtual-session node
	 * associated with the axon.
	 */
	struct device_node *of_node;

	struct list_head child_dev_list;

	/*
	 * Hold queue and tx tasklet used to buffer and resend mbufs blocked
	 * by a full outgoing axon queue, due to a slow receiver or a halted
	 * axon.
	 */
	struct list_head tx_queue;
	struct tasklet_struct tx_tasklet;
	u32 tx_uptr_freed;

	/*
	 * The readiness state of the transport, and a spinlock protecting it.
	 * Note that this is different to the session's readiness state
	 * machine, though it has the same basic purpose.
	 */
	enum vs_transport_readiness readiness;
	spinlock_t readiness_lock;

	struct tasklet_struct rx_tasklet;
	struct timer_list rx_retry_timer;
	struct list_head rx_freelist;
	u32 rx_alloc_extra;
	struct dma_pool *rx_pool;
	spinlock_t rx_alloc_lock;
	u32 rx_uptr_allocated;

	struct vs_session_device *session_dev;
	struct vs_transport transport;

	DECLARE_BITMAP(service_bitmap, VS_SERVICE_ID_BITMAP_BITS);

	struct delayed_work free_bufs_work;

	/*
	 * Freed buffers messages balance counter. This counter is incremented
	 * when we send a freed buffers message and decremented when we receive
	 * one. If the balance is negative then we need to send a message
	 * as an acknowledgement to the other end, even if there are no
	 * freed buffers to acknowledge.
	 */
	atomic_t free_bufs_balance;

	/*
	 * Flag set when a service exceeds its freed buffers watermark,
	 * telling free_bufs_work to send a message when the balance
	 * counter is non-negative. This is ignored, and a message is
	 * sent in any case, if the balance is negative.
	 */
	bool free_bufs_pending;

	/* Pool for allocating outgoing free bufs messages */
	struct vs_axon_tx_pool *free_bufs_pool;
};

#define to_vs_transport_axon(t) \
	container_of(t, struct vs_transport_axon, transport)

struct vs_mbuf_axon {
	struct vs_mbuf base;
	struct vs_transport_axon *owner;
	dma_addr_t laddr;
	struct vs_axon_tx_pool *pool;
};

#define to_vs_mbuf_axon(b) container_of(b, struct vs_mbuf_axon, base)

/*
 * Buffer allocation
 *
 * Buffers used by axons must be allocated within a single contiguous memory
 * region, backed by a single OKL4 physical segment. This is similar to how
 * the DMA allocator normally works, but we can't use the normal DMA allocator
 * because the platform code will remap the allocated memory with caching
 * disabled.
 *
 * We borrow the useful parts of the DMA allocator by providing our own DMA
 * mapping ops which don't actually remap the memory.
 */
static void *axon_dma_alloc(struct device *dev, size_t size,
		dma_addr_t *handle, gfp_t gfp, DMA_ATTRS attrs)
{
	unsigned long order;
	size_t count;
	struct page *page;
	void *ptr;

	*handle = DMA_ERROR_CODE;
	size = PAGE_ALIGN(size);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	if (!(gfp & __GFP_WAIT))
#else
	if (!(gfp & __GFP_RECLAIM))
#endif
		return NULL;

	order = get_order(size);
	count = size >> PAGE_SHIFT;

	if (dev_get_cma_area(dev)) {
		page = dma_alloc_from_contiguous(dev, count, order);

		if (!page)
			return NULL;
	} else {
		struct page *p, *e;
		page = alloc_pages(gfp, order);

		if (!page)
			return NULL;

		/* Split huge page and free any excess pages */
		split_page(page, order);
		for (p = page + count, e = page + (1 << order); p < e; p++)
			__free_page(p);
	}

	if (PageHighMem(page)) {
		struct vm_struct *area = get_vm_area(size, VM_USERMAP);
		if (!area)
			goto free_pages;
		ptr = area->addr;
		area->phys_addr = __pfn_to_phys(page_to_pfn(page));

		if (ioremap_page_range((unsigned long)ptr,
					(unsigned long)ptr + size,
					area->phys_addr, PAGE_KERNEL)) {
			vunmap(ptr);
			goto free_pages;
		}
	} else {
		ptr = page_address(page);
	}

	*handle = (dma_addr_t)page_to_pfn(page) << PAGE_SHIFT;

	dev_dbg(dev, "dma_alloc: %#tx bytes at %pK (%#llx), %s cma, %s high\n",
			size, ptr, (long long)*handle,
			dev_get_cma_area(dev) ? "is" : "not",
			PageHighMem(page) ? "is" : "not");

	return ptr;

free_pages:
	if (dev_get_cma_area(dev)) {
		dma_release_from_contiguous(dev, page, count);
	} else {
		struct page *e = page + count;

		while (page < e) {
			__free_page(page);
			page++;
		}
	}

	return NULL;
}

static void axon_dma_free(struct device *dev, size_t size, void *cpu_addr,
		dma_addr_t handle, DMA_ATTRS attrs)
{
	struct page *page = pfn_to_page(handle >> PAGE_SHIFT);

	size = PAGE_ALIGN(size);

	if (PageHighMem(page)) {
		unmap_kernel_range((unsigned long)cpu_addr, size);
		vunmap(cpu_addr);
	}

	if (dev_get_cma_area(dev)) {
		dma_release_from_contiguous(dev, page, size >> PAGE_SHIFT);
	} else {
		struct page *e = page + (size >> PAGE_SHIFT);

		while (page < e) {
			__free_page(page);
			page++;
		}
	}
}

struct dma_map_ops axon_dma_ops = {
	.alloc		= axon_dma_alloc,
	.free		= axon_dma_free,
};

/*
 * Quotas
 * ------
 *
 * Each service has two quotas, one for send and one for receive. The
 * send quota is incremented when we allocate an mbuf. The send quota
 * is decremented by receiving an freed buffer ack from the remove
 * end, either in the reserved bits of the service id or in a special
 * free bufs message.
 *
 * The receive quota is incremented whenever we receive a message and
 * decremented when we free the mbuf. Exceeding the receive quota
 * indicates that something bad has happened since the other end's
 * send quota should have prevented it from sending the
 * message. Exceeding the receive quota indicates a driver bug since
 * the two ends are disagreeing about the quotas. If this happens then
 * a warning is printed and the offending service is reset.
 */

/*
 * The base of the mbuf has the destination service id, but we pass the
 * data pointer starting after the service id. The following helper
 * functions are used to avoid ugly pointer arithmetic when handling
 * mbufs.
 */
static size_t mbuf_real_size(struct vs_mbuf_axon *mbuf)
{
	return mbuf->base.size + sizeof(vs_service_id_t);
}

static void *mbuf_real_base(struct vs_mbuf_axon *mbuf)
{
	return mbuf->base.data - sizeof(vs_service_id_t);
}
/*
 * Get the service_id and reserved bits from a message buffer and the
 * clear the reserved bits so the upper layers don't see them.
 */
vs_service_id_t
transport_get_mbuf_service_id(struct vs_transport_axon *transport,
		void *data, unsigned int *freed_acks)
{
	unsigned int reserved_bits;
	vs_service_id_t id;

	/* Get the real service id and reserved bits */
	id = *(vs_service_id_t *)data;
	reserved_bits = vs_get_service_id_reserved_bits(id);
	id = vs_get_real_service_id(id);

	/* Clear the reserved bits in the service id */
	vs_set_service_id_reserved_bits(&id, 0);
	if (freed_acks) {
		*(vs_service_id_t *)data = id;
		*freed_acks = reserved_bits;
	}
	return id;
}

static void
__transport_get_service_info(struct vs_mv_service_info *service_info)
{
	kref_get(&service_info->kref);
}

static struct vs_mv_service_info *
transport_get_service_info(struct vs_service_device *service)
{
	struct vs_mv_service_info *service_info;

	rcu_read_lock();
	service_info = rcu_dereference(service->transport_priv);
	if (service_info)
		__transport_get_service_info(service_info);
	rcu_read_unlock();

	return service_info;
}

static struct vs_mv_service_info *
transport_get_service_id_info(struct vs_transport_axon *transport,
		vs_service_id_t service_id)
{
	struct vs_service_device *service;
	struct vs_mv_service_info *service_info;

	service = vs_session_get_service(transport->session_dev, service_id);
	if (!service)
		return NULL;

	service_info = transport_get_service_info(service);

	vs_put_service(service);
	return service_info;
}

static void transport_info_free(struct rcu_head *rcu_head)
{
	struct vs_mv_service_info *service_info =
		container_of(rcu_head, struct vs_mv_service_info, rcu_head);

	vs_put_service(service_info->service);
	kfree(service_info);
}

static void transport_info_release(struct kref *kref)
{
	struct vs_mv_service_info *service_info =
		container_of(kref, struct vs_mv_service_info, kref);

	call_rcu(&service_info->rcu_head, transport_info_free);
}

static void transport_put_service_info(struct vs_mv_service_info *service_info)
{
	kref_put(&service_info->kref, transport_info_release);
}

static bool transport_axon_reset(struct vs_transport_axon *transport);

static void transport_fatal_error(struct vs_transport_axon *transport,
		const char *msg)
{
	dev_err(transport->axon_dev, "Fatal transport error (%s); resetting\n",
			msg);
#ifdef DEBUG
	dump_stack();
#endif
	transport_axon_reset(transport);
}

static unsigned int reduce_send_quota(struct vs_transport_axon *transport,
		struct vs_mv_service_info *service_info, unsigned int count,
		bool allow_tx_ready)
{
	int new_inflight, send_alloc;
	bool was_over_quota, is_over_quota;

        /* FIXME: Redmine issue #1303 - philip. */
	spin_lock_irq(&transport->readiness_lock);
	/*
	 * We read the current send_alloc for error checking *before*
	 * decrementing send_inflight. This avoids any false positives
	 * due to send_alloc being incremented by a concurrent alloc_mbuf.
	 *
	 * Note that there is an implicit smp_mb() before atomic_sub_return(),
	 * matching the explicit one in alloc_mbuf.
	 */
	send_alloc = atomic_read(&service_info->send_alloc);
	new_inflight = atomic_sub_return(count, &service_info->send_inflight);

	spin_unlock_irq(&transport->readiness_lock);
	if (WARN_ON(new_inflight < send_alloc)) {
		dev_err(transport->axon_dev,
				"inflight sent messages for service %d is less than the number of allocated messages (%d < %d, was reduced by %d)\n",
				service_info->service->id, new_inflight,
				send_alloc, count);
		transport_fatal_error(transport, "sent msg count underrun");
		return 0;
	}

	was_over_quota = (new_inflight + count >=
			service_info->service->send_quota);
	is_over_quota = (new_inflight > service_info->service->send_quota);

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev,
			"Service %d quota %d -> %d (over_quota: %d -> %d)\n",
			service_info->service->id, new_inflight + count,
			new_inflight, was_over_quota, is_over_quota);

	/*
	 * Notify the service that a buffer has been freed. We call tx_ready
	 * if this is a notification from the remote end (i.e. not an unsent
	 * buffer) and the quota has just dropped below the maximum.
	 */
	vs_session_quota_available(transport->session_dev,
			service_info->service->id, count,
			!is_over_quota && was_over_quota && allow_tx_ready);

	return count;
}

static void __transport_tx_pool_free(struct vs_axon_tx_pool *pool,
		dma_addr_t laddr);

static void
__transport_tx_cleanup(struct vs_transport_axon *transport)
{
	u32 uptr;
	struct okl4_axon_queue_entry *desc;

	lockdep_assert_held(&transport->readiness_lock);

	uptr = transport->tx_uptr_freed;
	desc = &transport->tx_descs[uptr];

	while (!okl4_axon_data_info_getpending(&desc->info)) {
		if (!transport->tx_pools[uptr])
			break;

		__transport_tx_pool_free(transport->tx_pools[uptr],
				okl4_axon_data_info_getladdr(&desc->info));
		transport->tx_pools[uptr] = NULL;

		INC_MOD(uptr, transport->tx->queues[0].entries);
		desc = &transport->tx_descs[uptr];
		transport->tx_uptr_freed = uptr;
	}
}

static void
transport_axon_free_tx_pool(struct work_struct *work)
{
	struct vs_axon_tx_pool *pool = container_of(work,
			struct vs_axon_tx_pool, free_work);
	struct vs_transport_axon *transport = pool->transport;

	dmam_free_coherent(transport->axon_dev,
			pool->count << pool->alloc_order,
			pool->base_vaddr, pool->base_laddr);
	devm_kfree(transport->axon_dev, pool);
}

static void
transport_axon_queue_free_tx_pool(struct kref *kref)
{
	struct vs_axon_tx_pool *pool = container_of(kref,
			struct vs_axon_tx_pool, kref);

	/*
	 * Put the task on the axon local work queue for running in
	 * a context where IRQ is enabled.
	 */
	INIT_WORK(&pool->free_work, transport_axon_free_tx_pool);
	queue_work(work_queue, &pool->free_work);
}

static void
transport_axon_put_tx_pool(struct vs_axon_tx_pool *pool)
{
	kref_put(&pool->kref, transport_axon_queue_free_tx_pool);
}

/* Low-level tx buffer allocation, without quota tracking. */
static struct vs_mbuf_axon *
__transport_alloc_mbuf(struct vs_transport_axon *transport,
		vs_service_id_t service_id, struct vs_axon_tx_pool *pool,
		size_t size, gfp_t gfp_flags)
{
	size_t real_size = size + sizeof(vs_service_id_t);
	struct vs_mbuf_axon *mbuf;
	unsigned index;

	if (WARN_ON(real_size > (1 << pool->alloc_order))) {
		dev_err(transport->axon_dev, "Message too big (%zu > %zu)\n",
				real_size, (size_t)1 << pool->alloc_order);
		goto fail_message_size;
	}

	kref_get(&pool->kref);

	do {
		index = find_first_zero_bit(pool->alloc_bitmap, pool->count);
		if (unlikely(index >= pool->count)) {
			/*
			 * No buffers left. This can't be an out-of-quota
			 * situation, because we've already checked the quota;
			 * it must be because there's a buffer left over in
			 * the tx queue. Clean out the tx queue and retry.
			 */
			spin_lock_irq(&transport->readiness_lock);
			__transport_tx_cleanup(transport);
			spin_unlock_irq(&transport->readiness_lock);

			index = find_first_zero_bit(pool->alloc_bitmap,
					pool->count);
		}
		if (unlikely(index >= pool->count))
			goto fail_buffer_alloc;
	} while (unlikely(test_and_set_bit_lock(index, pool->alloc_bitmap)));

	mbuf = kmem_cache_alloc(mbuf_cache, gfp_flags & ~GFP_ZONEMASK);
	if (!mbuf)
		goto fail_mbuf_alloc;

	mbuf->base.is_recv = false;
	mbuf->base.data = pool->base_vaddr + (index << pool->alloc_order);
	mbuf->base.size = size;
	mbuf->owner = transport;
	mbuf->laddr = pool->base_laddr + (index << pool->alloc_order);
	mbuf->pool = pool;

	/*
	 * We put the destination service id in the mbuf, but increment the
	 * data pointer past it so the receiver doesn't always need to skip
	 * the service id.
	 */
	*(vs_service_id_t *)mbuf->base.data = service_id;
	mbuf->base.data += sizeof(vs_service_id_t);

	return mbuf;

fail_mbuf_alloc:
	clear_bit_unlock(index, pool->alloc_bitmap);
fail_buffer_alloc:
	transport_axon_put_tx_pool(pool);
fail_message_size:
	return NULL;
}

/* Allocate a tx buffer for a specified service. */
static struct vs_mbuf *transport_alloc_mbuf(struct vs_transport *_transport,
		struct vs_service_device *service, size_t size, gfp_t gfp_flags)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	size_t real_size = size + sizeof(vs_service_id_t);
	struct vs_mv_service_info *service_info = NULL;
	struct vs_mbuf_axon *mbuf;
	vs_service_id_t service_id = service->id;

	if (real_size > transport->msg_size) {
		dev_err(transport->axon_dev, "Message too big (%zu > %zu)\n",
				real_size, transport->msg_size);
		return ERR_PTR(-EINVAL);
	}

	if (WARN_ON(service_id == MSG_SEND_FREE_BUFS))
		return ERR_PTR(-ENXIO);

	service_info = transport_get_service_info(service);
	if (WARN_ON(!service_info))
		return ERR_PTR(-EINVAL);

	if (!service_info->tx_pool) {
		transport_put_service_info(service_info);
		return ERR_PTR(-ECONNRESET);
	}

	if (!atomic_add_unless(&service_info->send_inflight, 1,
			service_info->service->send_quota)) {
		/* Service has reached its quota */
		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev,
				"Service %d is at max send quota %d\n",
				service_id, service_info->service->send_quota);
		transport_put_service_info(service_info);
		return ERR_PTR(-ENOBUFS);
	}

	/*
	 * Increment the count of allocated but unsent mbufs. This is done
	 * *after* the send_inflight increment (with a barrier to enforce
	 * ordering) to ensure that send_inflight is never less than
	 * send_alloc - see reduce_send_quota().
	 */
	smp_mb__before_atomic_inc();
	atomic_inc(&service_info->send_alloc);

	mbuf = __transport_alloc_mbuf(transport, service_id,
			service_info->tx_pool, size, gfp_flags);
	if (!mbuf) {
		/*
		 * Failed to allocate a buffer - decrement our quota back to
		 * where it was.
		 */
		atomic_dec(&service_info->send_alloc);
		smp_mb__after_atomic_dec();
		atomic_dec(&service_info->send_inflight);

		transport_put_service_info(service_info);

		return ERR_PTR(-ENOMEM);
	}

	transport_put_service_info(service_info);

	return &mbuf->base;
}

static void transport_free_sent_mbuf(struct vs_transport_axon *transport,
		struct vs_mbuf_axon *mbuf)
{
	kmem_cache_free(mbuf_cache, mbuf);
}

static void __transport_tx_pool_free(struct vs_axon_tx_pool *pool,
		dma_addr_t laddr)
{
	unsigned index = (laddr - pool->base_laddr) >> pool->alloc_order;

	if (WARN_ON(index >= pool->count)) {
		printk(KERN_DEBUG "free %#llx base %#llx order %d count %d\n",
				(long long)laddr, (long long)pool->base_laddr,
				pool->alloc_order, pool->count);
		return;
	}

	clear_bit_unlock(index, pool->alloc_bitmap);
	transport_axon_put_tx_pool(pool);
}

static int transport_rx_queue_buffer(struct vs_transport_axon *transport,
		void *ptr, dma_addr_t laddr);

static void transport_rx_recycle(struct vs_transport_axon *transport,
		struct vs_mbuf_axon *mbuf)
{
	void *data = mbuf_real_base(mbuf);
	dma_addr_t laddr = mbuf->laddr;
	unsigned long flags;

	spin_lock_irqsave(&transport->rx_alloc_lock, flags);

	if (transport->rx_alloc_extra) {
		transport->rx_alloc_extra--;
		dma_pool_free(transport->rx_pool, data, laddr);
	} else if (transport_rx_queue_buffer(transport, data, laddr) < 0) {
		struct vs_axon_rx_freelist_entry *buf = data;
		buf->laddr = laddr;
		list_add_tail(&buf->list, &transport->rx_freelist);
		tasklet_schedule(&transport->rx_tasklet);
	} else {
		tasklet_schedule(&transport->rx_tasklet);
	}

	spin_unlock_irqrestore(&transport->rx_alloc_lock, flags);
}

static void transport_free_mbuf_pools(struct vs_transport_axon *transport,
		struct vs_service_device *service,
		struct vs_mv_service_info *service_info)
{
	/*
	 * Free the TX allocation pool. This will also free any buffer
	 * memory allocated from the pool, so it is essential that
	 * this happens only after we have successfully freed all
	 * mbufs.
	 *
	 * Note that the pool will not exist if the core client is reset
	 * before it receives a startup message.
	 */
	if (!IS_ERR_OR_NULL(service_info->tx_pool))
		transport_axon_put_tx_pool(service_info->tx_pool);
	service_info->tx_pool = NULL;

	/* Mark the service's preallocated RX buffers as extra. */
	spin_lock_irq(&transport->rx_alloc_lock);
	transport->rx_alloc_extra += service_info->rx_allocated;
	service_info->rx_allocated = 0;
	spin_unlock_irq(&transport->rx_alloc_lock);
}

/* Low-level tx or rx buffer free, with no quota tracking */
static void __transport_free_mbuf(struct vs_transport_axon *transport,
		struct vs_mbuf_axon *mbuf, bool is_rx)
{
	if (is_rx) {
		transport_rx_recycle(transport, mbuf);
	} else {
		__transport_tx_pool_free(mbuf->pool, mbuf->laddr);
	}

	kmem_cache_free(mbuf_cache, mbuf);
}

static void transport_free_mbuf(struct vs_transport *_transport,
		struct vs_service_device *service, struct vs_mbuf *_mbuf)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	struct vs_mbuf_axon *mbuf = to_vs_mbuf_axon(_mbuf);
	struct vs_mv_service_info *service_info = NULL;
	void *data = mbuf_real_base(mbuf);
	vs_service_id_t service_id __maybe_unused =
		transport_get_mbuf_service_id(transport, data, NULL);
	bool is_recv = mbuf->base.is_recv;

	WARN_ON(!service);
	service_info = transport_get_service_info(service);

	__transport_free_mbuf(transport, mbuf, is_recv);

	/*
	 * If this message was left over from a service that has already been
	 * deleted, we don't need to do any quota accounting.
	 */
	if (!service_info)
		return;

	if (unlikely(atomic_read(&service_info->outstanding_frees))) {
		if (atomic_dec_and_test(&service_info->outstanding_frees)) {
			dev_dbg(transport->axon_dev,
				"service %d all outstanding frees done\n",
				service->id);
			transport_free_mbuf_pools(transport, service,
					service_info);
			vs_service_enable(service);
		} else {
			dev_dbg(transport->axon_dev,
				"service %d outstanding frees -> %d\n",
				service->id, atomic_read(
					&service_info->outstanding_frees));
		}
	} else if (is_recv) {
		smp_mb__before_atomic_dec();
		atomic_dec(&service_info->recv_inflight);
		if (atomic_inc_return(&service_info->recv_freed) >=
				service_info->recv_freed_watermark) {
			transport->free_bufs_pending = true;
			schedule_delayed_work(&transport->free_bufs_work, 0);
		}

		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev,
				"Freed recv buffer for service %d rq=%d/%d, freed=%d (watermark = %d)\n",
				service_id,
				atomic_read(&service_info->recv_inflight),
				service_info->service->recv_quota,
				atomic_read(&service_info->recv_freed),
				service_info->recv_freed_watermark);
	} else {
		/*
		 * We are freeing a message buffer that we allocated. This
		 * usually happens on error paths in application drivers if
		 * we allocated a buffer but failed to send it. In this case
		 * we need to decrement our own send quota since we didn't
		 * send anything.
		 */
		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev,
				"Freeing send buffer for service %d, send quota = %d\n",
				service_id, atomic_read(&service_info->send_inflight));

		smp_mb__before_atomic_dec();
		atomic_dec(&service_info->send_alloc);

		/*
		 * We don't allow the tx_ready handler to run when we are
		 * freeing an mbuf that we allocated.
		 */
		reduce_send_quota(transport, service_info, 1, false);
	}

	transport_put_service_info(service_info);
}

static size_t transport_mbuf_size(struct vs_mbuf *_mbuf)
{
	struct vs_mbuf_axon *mbuf = to_vs_mbuf_axon(_mbuf);

	return mbuf_real_size(mbuf);
}

static size_t transport_max_mbuf_size(struct vs_transport *_transport)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);

	return transport->msg_size - sizeof(vs_service_id_t);
}

static int okl4_error_to_errno(okl4_error_t err) {
	switch (err) {
	case OKL4_OK:
		return 0;
	case OKL4_ERROR_AXON_QUEUE_NOT_MAPPED:
		/* Axon has been reset locally */
		return -ECONNRESET;
	case OKL4_ERROR_AXON_QUEUE_NOT_READY:
		/* No message buffers in the queue. */
		return -ENOBUFS;
	case OKL4_ERROR_AXON_INVALID_OFFSET:
	case OKL4_ERROR_AXON_AREA_TOO_BIG:
		/* Buffer address is bad */
		return -EFAULT;
	case OKL4_ERROR_AXON_BAD_MESSAGE_SIZE:
	case OKL4_ERROR_AXON_TRANSFER_LIMIT_EXCEEDED:
		/* One of the Axon's message size limits has been exceeded */
		return -EMSGSIZE;
	default:
		/* Miscellaneous failure, probably a bad cap */
		return -EIO;
	}
}

static void queue_tx_mbuf(struct vs_mbuf_axon *mbuf, struct vs_transport_axon *priv,
		vs_service_id_t service_id)
{
	list_add_tail(&mbuf->base.queue, &priv->tx_queue);
}

static void free_tx_mbufs(struct vs_transport_axon *priv)
{
	struct vs_mbuf_axon *child, *tmp;

	list_for_each_entry_safe(child, tmp, &priv->tx_queue, base.queue) {
		list_del(&child->base.queue);
		__transport_free_mbuf(priv, child, false);
	}
}

static int __transport_flush(struct vs_transport_axon *transport)
{
	_okl4_sys_axon_trigger_send(transport->tx_cap);
	return 0;
}

static int transport_flush(struct vs_transport *_transport,
		struct vs_service_device *service)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);

	return __transport_flush(transport);
}

/*
 * Low-level transport message send function.
 *
 * The caller must hold the transport->readiness_lock, and is responsible for
 * freeing the mbuf on successful send (use transport_free_sent_mbuf). The
 * mbuf should _not_ be freed if this function fails. The Virtual Service
 * driver is responsible for freeing the mbuf in the failure case.
 */
static int __transport_send(struct vs_transport_axon *transport,
		struct vs_mbuf_axon *mbuf, vs_service_id_t service_id,
		unsigned long flags)
{
	u32 uptr;
	struct okl4_axon_queue_entry *desc;
	struct vs_axon_tx_pool *old_pool;
	dma_addr_t old_laddr;

	lockdep_assert_held(&transport->readiness_lock);

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev,
			"send %zu bytes to service %d\n",
			mbuf->base.size, service_id);
	vs_debug_dump_mbuf(transport->session_dev, &mbuf->base);

	uptr = ACCESS_ONCE(transport->tx->queues[0].uptr);
	desc = &transport->tx_descs[uptr];

	/* Is the descriptor ready to use? */
	if (okl4_axon_data_info_getpending(&desc->info))
		return -ENOSPC;
	mb();

	/* The descriptor is ours; save its old state and increment the uptr */
	old_pool = transport->tx_pools[uptr];
	if (old_pool != NULL)
		old_laddr = okl4_axon_data_info_getladdr(&desc->info);
	transport->tx_pools[uptr] = mbuf->pool;

	INC_MOD(uptr, transport->tx->queues[0].entries);
	ACCESS_ONCE(transport->tx->queues[0].uptr) = uptr;

	/* Set up the descriptor */
	desc->data_size = mbuf_real_size(mbuf);
	okl4_axon_data_info_setladdr(&desc->info, mbuf->laddr);

	/* Message is ready to go */
	wmb();
	okl4_axon_data_info_setpending(&desc->info, true);

	if (flags & VS_TRANSPORT_SEND_FLAGS_MORE) {
		/*
		 * This is a batched message, so we normally don't flush,
		 * unless we've filled the queue completely.
		 *
		 * Races on the queue descriptor don't matter here, because
		 * this is only an optimisation; the service should do an
		 * explicit flush when it finishes the batch anyway.
		 */
		desc = &transport->tx_descs[uptr];
		if (okl4_axon_data_info_getpending(&desc->info))
			__transport_flush(transport);
	} else {
		__transport_flush(transport);
	}

	/* Free any buffer previously in the descriptor */
	if (old_pool != NULL) {
		u32 uptr_freed = transport->tx_uptr_freed;
		INC_MOD(uptr_freed, transport->tx->queues[0].entries);
		WARN_ON(uptr_freed != uptr);
		__transport_tx_pool_free(old_pool, old_laddr);
		transport->tx_uptr_freed = uptr_freed;
	}

	return 0;
}

static int transport_send_might_queue(struct vs_transport_axon *transport,
		struct vs_mbuf_axon *mbuf, vs_service_id_t service_id,
		unsigned long flags, bool *queued)
{
	int ret = 0;

	lockdep_assert_held(&transport->readiness_lock);
	*queued = false;

	if (transport->readiness != VS_TRANSPORT_ACTIVE)
		return -ECONNRESET;

	if (!list_empty(&transport->tx_queue)) {
		*queued = true;
	} else {
		ret = __transport_send(transport, mbuf, service_id, flags);
		if (ret == -ENOSPC) {
			*queued = true;
			ret = 0;
		}
	}

	if (*queued)
		queue_tx_mbuf(mbuf, transport, service_id);

	return ret;
}

static int transport_send(struct vs_transport *_transport,
		struct vs_service_device *service, struct vs_mbuf *_mbuf,
		unsigned long flags)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	struct vs_mbuf_axon *mbuf = to_vs_mbuf_axon(_mbuf);
	struct vs_mv_service_info *service_info;
	vs_service_id_t service_id;
	int recv_freed, freed_acks;
	bool queued;
	int err;
	unsigned long irqflags;

	if (WARN_ON(!transport || !mbuf || mbuf->owner != transport))
		return -EINVAL;

	service_id = transport_get_mbuf_service_id(transport,
			mbuf_real_base(mbuf), NULL);

	if (WARN_ON(service_id != service->id))
		return -EINVAL;

	service_info = transport_get_service_info(service);
	if (!service_info)
		return -EINVAL;

	if (mbuf->base.is_recv) {
		/*
		 * This message buffer was allocated for receive. We don't
		 * allow receive message buffers to be reused for sending
		 * because it makes our quotas inconsistent.
		 */
		dev_err(&service_info->service->dev,
				"Attempted to send a received message buffer\n");
		transport_put_service_info(service_info);
		return -EINVAL;
	}

	if (!service_info->ready) {
		transport_put_service_info(service_info);
		return -ECOMM;
	}

	/*
	 * Set the message's service id reserved bits to the number of buffers
	 * we have freed. We can only ack 2 ^ VS_SERVICE_ID_RESERVED_BITS - 1
	 * buffers in one message.
	 */
	do {
		recv_freed = atomic_read(&service_info->recv_freed);
		freed_acks = min_t(int, recv_freed,
				VS_SERVICE_ID_TRANSPORT_MASK);
	} while (recv_freed != atomic_cmpxchg(&service_info->recv_freed,
				recv_freed, recv_freed - freed_acks));

	service_id = service_info->service->id;
	vs_set_service_id_reserved_bits(&service_id, freed_acks);
	*(vs_service_id_t *)mbuf_real_base(mbuf) = service_id;

	spin_lock_irqsave(&transport->readiness_lock, irqflags);
	err = transport_send_might_queue(transport, mbuf,
			service_info->service->id, flags, &queued);
	if (err) {
		/* We failed to send, so revert the freed acks */
		if (atomic_add_return(freed_acks,
				&service_info->recv_freed) >=
				service_info->recv_freed_watermark) {
			transport->free_bufs_pending = true;
			schedule_delayed_work(&transport->free_bufs_work, 0);
		}
		transport_put_service_info(service_info);
		spin_unlock_irqrestore(&transport->readiness_lock, irqflags);
		return err;
	}

	atomic_dec(&service_info->send_alloc);

	if (queued) {
		transport_put_service_info(service_info);
		spin_unlock_irqrestore(&transport->readiness_lock, irqflags);
		return 0;
	}

	/*
	 * The mbuf was sent successfully. We can free it locally since it is
	 * now owned by the remote end.
	 */
	transport_free_sent_mbuf(transport, mbuf);

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev,
			"Send okay: service %d (0x%.2x) sq=%d/%d, alloc--=%d, rq=%d/%d, freed=%d/%d, bc=%d\n",
			service_info->service->id, service_id,
			atomic_read(&service_info->send_inflight),
			service_info->service->send_quota,
			atomic_read(&service_info->send_alloc),
			atomic_read(&service_info->recv_inflight),
			service_info->service->recv_quota, freed_acks,
			atomic_read(&service_info->recv_freed),
			atomic_read(&transport->free_bufs_balance));

	transport_put_service_info(service_info);
	spin_unlock_irqrestore(&transport->readiness_lock, irqflags);

	return 0;
}

static void transport_free_bufs_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct vs_transport_axon *transport = container_of(dwork,
			struct vs_transport_axon, free_bufs_work);
	struct vs_mbuf_axon *mbuf;
	int i, err, count = 0, old_balance;
	bool queued;
	size_t size;
	u16 *p;

	/*
	 * Atomically decide whether to send a message, and increment
	 * the balance if we are going to.
	 *
	 * We don't need barriers before these reads because they're
	 * implicit in the work scheduling.
	 */
	do {
		old_balance = atomic_read(&transport->free_bufs_balance);

		/*
		 * We only try to send if the balance is negative,
		 * or if we have been triggered by going over a
		 * watermark.
		 */
		if (old_balance >= 0 && !transport->free_bufs_pending)
			return;

		/*
		 * If we've hit the max balance, we can't send. The
		 * tasklet will be rescheduled next time the balance
		 * is decremented, if free_bufs_pending is true.
		 */
		if (old_balance >= MAX_BALANCE)
			return;

	} while (old_balance != atomic_cmpxchg(&transport->free_bufs_balance,
			old_balance, old_balance + 1));

	/* Try to allocate a message buffer. */
	mbuf = __transport_alloc_mbuf(transport, MSG_SEND_FREE_BUFS,
			transport->free_bufs_pool,
			transport->msg_size - sizeof(vs_service_id_t),
			GFP_KERNEL | __GFP_NOWARN);
	if (!mbuf) {
		/* Out of memory at the moment; retry later. */
		atomic_dec(&transport->free_bufs_balance);
		schedule_delayed_work(dwork, FREE_BUFS_RETRY_DELAY);
		return;
	}

	/*
	 * Clear free_bufs_pending, because we are going to try to send.  We
	 * need a write barrier afterwards to guarantee that this write is
	 * ordered before any writes to the recv_freed counts, and therefore
	 * before any remote free_bufs_pending = true when a service goes
	 * over its watermark right after we inspect it.
	 *
	 * The matching barrier is implicit in the atomic_inc_return in
	 * transport_free_mbuf().
	 */
	transport->free_bufs_pending = false;
	smp_wmb();

	/*
	 * Fill in the buffer. Message format is:
	 *
	 *   u16: Number of services
	 *
	 *   For each service:
	 *       u16: Service ID
	 *       u16: Number of freed buffers
	 */
	p = mbuf->base.data;
	*(p++) = 0;

	for_each_set_bit(i, transport->service_bitmap,
			VS_SERVICE_ID_BITMAP_BITS) {
		struct vs_mv_service_info *service_info;
		int recv_freed;
		u16 freed_acks;

		service_info = transport_get_service_id_info(transport, i);
		if (!service_info)
			continue;

		/*
		 * Don't let the message exceed the maximum size for the
		 * transport.
		 */
		size = sizeof(vs_service_id_t) + sizeof(u16) +
				(count * (2 * sizeof(u16)));
		if (size > transport->msg_size) {
			/* FIXME: Jira ticket SDK-3131 - ryanm. */
			transport_put_service_info(service_info);
			transport->free_bufs_pending = true;
			break;
		}

		/*
		 * We decrement each service's quota immediately by up to
		 * USHRT_MAX. If we subsequently fail to send the message then
		 * we return the count to what it was previously.
		 */
		do {
			recv_freed = atomic_read(&service_info->recv_freed);
			freed_acks = min_t(int, USHRT_MAX, recv_freed);
		} while (recv_freed != atomic_cmpxchg(
				&service_info->recv_freed,
				recv_freed, recv_freed - freed_acks));

		if (freed_acks) {
			if (freed_acks < recv_freed)
				transport->free_bufs_pending = true;

			*(p++) = service_info->service->id;
			*(p++) = freed_acks;
			count++;

			vs_dev_debug(VS_DEBUG_TRANSPORT,
					transport->session_dev,
					transport->axon_dev,
					"  [%.2d] Freed %.2d buffers\n",
					service_info->service->id,
					freed_acks);
		} else {
			vs_dev_debug(VS_DEBUG_TRANSPORT,
					transport->session_dev,
					transport->axon_dev,
					"  [%.2d] No buffers to free\n",
					service_info->service->id);
		}

		transport_put_service_info(service_info);
	}

	if (transport->free_bufs_pending)
		schedule_delayed_work(dwork, 0);

	if (count == 0 && old_balance >= 0) {
		/*
		 * We are sending a new free bufs message, but we have no
		 * freed buffers to tell the other end about. We don't send
		 * an empty message unless the pre-increment balance was
		 * negative (in which case we need to ack a remote free_bufs).
		 *
		 * Note that nobody else can increase the balance, so we only
		 * need to check for a non-negative balance once before
		 * decrementing. However, if the incoming free-bufs handler
		 * concurrently decrements, the balance may become negative,
		 * in which case we reschedule ourselves immediately to send
		 * the ack.
		 */
		if (atomic_dec_return(&transport->free_bufs_balance) < 0)
			schedule_delayed_work(dwork, 0);

		__transport_free_mbuf(transport, mbuf, false);

		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev,
				"No services had buffers to free\n");

		return;
	}

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev,
			"Sending free bufs message for %d services\n", count);

	/* Fix up the message size */
	p = mbuf->base.data;
	*p = count;
	mbuf->base.size = sizeof(u16) * ((count * 2) + 1);

	spin_lock_irq(&transport->readiness_lock);
	err = transport_send_might_queue(transport, mbuf, MSG_SEND_FREE_BUFS,
			0, &queued);
	if (err) {
		spin_unlock_irq(&transport->readiness_lock);
		goto fail;
	}

	/* FIXME: Jira ticket SDK-4675 - ryanm. */
	if (!queued) {
		/*
		 * The mbuf was sent successfully. We can free it locally
		 * since it is now owned by the remote end.
		 */
		transport_free_sent_mbuf(transport, mbuf);
	}
	spin_unlock_irq(&transport->readiness_lock);

	return;

fail:
	dev_err(transport->axon_dev,
			"Failed to send free bufs message: %d\n", err);
	transport_fatal_error(transport, "free bufs send failed");
}

int transport_notify(struct vs_transport *_transport,
		struct vs_service_device *service, unsigned long bits)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	unsigned long bit_offset, bitmask, word;
	int first_set_bit, spilled_bits;

	BUG_ON(!transport);

	if (!bits)
		return -EINVAL;

	/* Check that the service isn't trying to raise bits it doesn't own */
	if (bits & ~((1UL << service->notify_send_bits) - 1))
		return -EINVAL;

	bit_offset = service->notify_send_offset;
	word = BIT_WORD(bit_offset);
	bitmask = bits << (bit_offset % BITS_PER_LONG);

	vs_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			"Sending notification %ld to service id %d\n", bitmask,
			service->id);

	_okl4_sys_vinterrupt_raise(transport->notify_cap[word], bitmask);

	/*
	* Bit range may spill into the next virqline.
	*
	* Check by adding the bit offset to the index of the highest set bit in
	* the requested bitmask. If we need to raise a bit that is greater than
	* bit 31, we have spilled into the next word and need to raise that too.
	*/
	first_set_bit = find_first_bit(&bits, BITS_PER_LONG);
	spilled_bits = first_set_bit + bit_offset - (BITS_PER_LONG - 1);
	if (spilled_bits > 0) {
		/*
		* Calculate the new bitmask for the spilled bits. We do this by
		* shifting the requested bits to the right. The number of shifts
		* is determined on where the first spilled bit is.
		*/
		int first_spilled_bit = first_set_bit - spilled_bits + 1;

		bitmask = bits >> first_spilled_bit;

		vs_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				"Sending notification %ld to service id %d\n", bitmask,
				service->id);

		_okl4_sys_vinterrupt_raise(transport->notify_cap[word + 1], bitmask);
	}

	return 0;
}

static void
transport_handle_free_bufs_message(struct vs_transport_axon *transport,
		struct vs_mbuf_axon *mbuf)
{
	struct vs_mv_service_info *service_info;
	vs_service_id_t service_id;
	u16 *p = mbuf->base.data;
	int i, count, freed_acks, new_balance;

	count = *(p++);
	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev,
			"Free bufs message received for %d services\n", count);
	for (i = 0; i < count; i++) {
		int old_quota __maybe_unused;

		service_id = *(p++);
		freed_acks = *(p++);

		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev, "  [%.2d] %.4d\n",
				service_id, freed_acks);

		service_info = transport_get_service_id_info(transport,
				service_id);
		if (!service_info) {
			vs_dev_debug(VS_DEBUG_TRANSPORT,
					transport->session_dev,
					transport->axon_dev,
					"Got %d free_acks for unknown service %d\n",
					freed_acks, service_id);
			continue;
		}

		old_quota = atomic_read(&service_info->send_inflight);
		freed_acks = reduce_send_quota(transport, service_info,
				freed_acks, service_info->ready);
		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev,
				"  [%.2d] Freed %.2d buffers (%d -> %d, quota = %d)\n",
				service_id, freed_acks, old_quota,
				atomic_read(&service_info->send_inflight),
				service_info->service->send_quota);

		transport_put_service_info(service_info);
	}

	__transport_free_mbuf(transport, mbuf, true);

	new_balance = atomic_dec_return(&transport->free_bufs_balance);
	if (new_balance < -MAX_BALANCE) {
		dev_err(transport->axon_dev,
				"Balance counter fell below -MAX_BALANCE (%d < %d)\n",
				atomic_read(&transport->free_bufs_balance),
				-MAX_BALANCE);
		transport_fatal_error(transport, "balance counter underrun");
		return;
	}

	/* Check if we need to send a freed buffers message back */
	if (new_balance < 0 || transport->free_bufs_pending)
		schedule_delayed_work(&transport->free_bufs_work, 0);
}

static int transport_rx_queue_buffer(struct vs_transport_axon *transport,
		void *ptr, dma_addr_t laddr)
{
	struct okl4_axon_queue_entry *desc;
	okl4_axon_data_info_t info;

	/* Select the buffer desc to reallocate */
	desc = &transport->rx_descs[transport->rx_uptr_allocated];
	info = ACCESS_ONCE(desc->info);

	/* If there is no space in the rx queue, fail */
	if (okl4_axon_data_info_getusr(&info))
		return -ENOSPC;

	/* Don't update desc before reading the clear usr bit */
	smp_mb();

	/* Update the buffer pointer in the desc and mark it valid. */
	transport->rx_ptrs[transport->rx_uptr_allocated] = ptr;
	okl4_axon_data_info_setladdr(&info, (okl4_laddr_t)laddr);
	okl4_axon_data_info_setpending(&info, true);
	okl4_axon_data_info_setusr(&info, true);
	mb();
	ACCESS_ONCE(desc->info) = info;

	/* Proceed to the next buffer */
	INC_MOD(transport->rx_uptr_allocated,
			transport->rx->queues[0].entries);

	/* Return true if the next desc has no buffer yet */
	desc = &transport->rx_descs[transport->rx_uptr_allocated];
	return !okl4_axon_data_info_getusr(&desc->info);
}

/* TODO: multiple queue support / small message prioritisation */
static int transport_process_msg(struct vs_transport_axon *transport)
{
	struct vs_mv_service_info *service_info;
	struct vs_mbuf_axon *mbuf;
	vs_service_id_t service_id;
	unsigned freed_acks;
	u32 uptr;
	struct okl4_axon_queue_entry *desc;
	void **ptr;
	okl4_axon_data_info_t info;

	/* Select the descriptor to receive from */
	uptr = ACCESS_ONCE(transport->rx->queues[0].uptr);
	desc = &transport->rx_descs[uptr];
	ptr = &transport->rx_ptrs[uptr];
	info = ACCESS_ONCE(desc->info);

	/* Have we emptied the whole queue? */
	if (!okl4_axon_data_info_getusr(&info))
		return -ENOBUFS;

	/* Has the next buffer been filled yet? */
	if (okl4_axon_data_info_getpending(&info))
		return 0;

	/* Don't read the buffer or desc before seeing a cleared pending bit */
	rmb();

	/* Is the message too small to be valid? */
	if (desc->data_size < sizeof(vs_service_id_t))
		return -EBADMSG;

	/* Allocate and set up the mbuf */
	mbuf = kmem_cache_alloc(mbuf_cache, GFP_ATOMIC);
	if (!mbuf)
		return -ENOMEM;

	mbuf->owner = transport;
	mbuf->laddr = okl4_axon_data_info_getladdr(&info);
	mbuf->pool = NULL;
	mbuf->base.is_recv = true;
	mbuf->base.data = *ptr + sizeof(vs_service_id_t);
	mbuf->base.size = desc->data_size - sizeof(vs_service_id_t);

	INC_MOD(uptr, transport->rx->queues[0].entries);
	ACCESS_ONCE(transport->rx->queues[0].uptr) = uptr;

	/* Finish reading desc before clearing usr bit */
	smp_mb();

	/* Re-check the pending bit, in case we've just been reset */
	info = ACCESS_ONCE(desc->info);
	if (unlikely(okl4_axon_data_info_getpending(&info))) {
		kmem_cache_free(mbuf_cache, mbuf);
		return 0;
	}

	/* Clear usr bit; after this point the buffer is owned by the mbuf */
	okl4_axon_data_info_setusr(&info, false);
	ACCESS_ONCE(desc->info) = info;

	/* Determine who to deliver the mbuf to */
	service_id = transport_get_mbuf_service_id(transport,
			mbuf_real_base(mbuf), &freed_acks);

	if (service_id == MSG_SEND_FREE_BUFS) {
		transport_handle_free_bufs_message(transport, mbuf);
		return 1;
	}

	service_info = transport_get_service_id_info(transport, service_id);
	if (!service_info) {
		vs_dev_debug(VS_DEBUG_TRANSPORT,
				transport->session_dev, transport->axon_dev,
				"discarding message for missing service %d\n",
				service_id);
		__transport_free_mbuf(transport, mbuf, true);
		return -EIDRM;
	}

	/*
	 * If the remote end has freed some buffers that we sent it, then we
	 * can decrement our send quota count by that amount.
	 */
	freed_acks = reduce_send_quota(transport, service_info,
			freed_acks, service_info->ready);

	/* If the service has been reset, drop the message. */
	if (!service_info->ready) {
		vs_dev_debug(VS_DEBUG_TRANSPORT,
				transport->session_dev, transport->axon_dev,
				"discarding message for reset service %d\n",
				service_id);

		__transport_free_mbuf(transport, mbuf, true);
		transport_put_service_info(service_info);

		return 1;
	}

	/*
	 * Increment our recv quota since we are now holding a buffer. We
	 * will decrement it when the buffer is freed in transport_free_mbuf.
	 */
	if (!atomic_add_unless(&service_info->recv_inflight, 1,
				service_info->service->recv_quota)) {
		/*
		 * Going over the recv_quota indicates that something bad
		 * has happened because either the other end has exceeded
		 * its send quota or the two ends have a disagreement about
		 * what the quota is.
		 *
		 * We free the buffer and reset the transport.
		 */
		dev_err(transport->axon_dev,
				"Service %d is at max receive quota %d - resetting\n",
				service_info->service->id,
				service_info->service->recv_quota);

		transport_fatal_error(transport, "rx quota exceeded");

		__transport_free_mbuf(transport, mbuf, true);
		transport_put_service_info(service_info);

		return 0;
	}

	WARN_ON(atomic_read(&service_info->recv_inflight) >
			service_info->service->recv_quota);

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev,
			"receive %zu bytes from service 0x%.2x (%d): sq=%d/%d, rq=%d/%d, freed_acks=%d, freed=%d/%d bc=%d\n",
			mbuf->base.size, service_info->service->id, service_id,
			atomic_read(&service_info->send_inflight),
			service_info->service->send_quota,
			atomic_read(&service_info->recv_inflight),
			service_info->service->recv_quota, freed_acks,
			atomic_read(&service_info->recv_freed),
			service_info->recv_freed_watermark,
			atomic_read(&transport->free_bufs_balance));
	vs_debug_dump_mbuf(transport->session_dev, &mbuf->base);

	if (vs_session_handle_message(transport->session_dev, &mbuf->base,
			service_id) < 0)
		transport_free_mbuf(&transport->transport,
				service_info->service, &mbuf->base);

	transport_put_service_info(service_info);

	return 1;
}

static void transport_flush_tx_queues(struct vs_transport_axon *transport)
{
	okl4_error_t err;
	int i;

	lockdep_assert_held(&transport->readiness_lock);

	/* Release any queued mbufs */
	free_tx_mbufs(transport);

	/*
	 * Re-attach the TX Axon's segment, which implicitly invalidates
	 * the queues and stops any outgoing message transfers. The queues
	 * will be reconfigured when the transport becomes ready again.
	 */
	err = _okl4_sys_axon_set_send_segment(transport->tx_cap,
			transport->segment, transport->segment_base);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "TX reattach failed: %d\n",
				(int)err);
	}

	/*
	 * The TX Axon has stopped, so we can safely clear the pending
	 * bit and free the buffer for any outgoing messages, and reset uptr
	 * and kptr to 0.
	 */
	for (i = 0; i < transport->tx->queues[0].entries; i++) {
		if (!transport->tx_pools[i])
			continue;

		okl4_axon_data_info_setpending(
				&transport->tx_descs[i].info, false);
		__transport_tx_pool_free(transport->tx_pools[i],
				okl4_axon_data_info_getladdr(
					&transport->tx_descs[i].info));
		transport->tx_pools[i] = NULL;
	}
	transport->tx->queues[0].uptr = 0;
	transport->tx->queues[0].kptr = 0;
	transport->tx_uptr_freed = 0;
}

static void transport_flush_rx_queues(struct vs_transport_axon *transport)
{
	okl4_error_t err;
	int i;

	lockdep_assert_held(&transport->readiness_lock);

	/*
	 * Re-attach the TX Axon's segment, which implicitly invalidates
	 * the queues and stops any incoming message transfers, though those
	 * should already have cancelled those at the sending end. The queues
	 * will be reconfigured when the transport becomes ready again.
	 */
	err = _okl4_sys_axon_set_recv_segment(transport->rx_cap,
			transport->segment, transport->segment_base);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "RX reattach failed: %d\n",
				(int)err);
	}

	/*
	 * The RX Axon has stopped, so we can reset the pending bit on all
	 * allocated message buffers to prepare them for reuse when the reset
	 * completes.
	 */
	for (i = 0; i < transport->rx->queues[0].entries; i++) {
		if (okl4_axon_data_info_getusr(&transport->rx_descs[i].info))
			okl4_axon_data_info_setpending(
					&transport->rx_descs[i].info, true);
	}

	/*
	 * Reset kptr to the current uptr.
	 *
	 * We use a barrier here to ensure the pending bits are reset before
	 * reading uptr, matching the barrier in transport_process_msg between
	 * the uptr update and the second check of the pending bit. This means
	 * that races with transport_process_msg() will end in one of two
	 * ways:
	 *
	 * 1. transport_process_msg() updates uptr before this barrier, so the
	 *    RX buffer is passed up to the session layer to be rejected there
	 *    and recycled; or
	 *
	 * 2. the reset pending bit is seen by the second check in
	 *    transport_process_msg(), which knows that it is being reset and
	 *    can drop the message before it claims the buffer.
	 */
	smp_mb();
	transport->rx->queues[0].kptr =
		ACCESS_ONCE(transport->rx->queues[0].uptr);

	/*
	 * Cancel any pending freed bufs work. We can't flush it here, but
	 * that is OK: we will do so before we become ready.
	 */
	cancel_delayed_work(&transport->free_bufs_work);
}

static bool transport_axon_reset(struct vs_transport_axon *transport)
{
	okl4_error_t err;
	unsigned long flags;
	bool reset_complete = false;

	spin_lock_irqsave(&transport->readiness_lock, flags);

	/*
	 * Reset the transport, dumping any messages in transit, and tell the
	 * remote end that it should do the same.
	 *
	 * We only do this if the transport is not already marked reset. Doing
	 * otherwise would be redundant.
	 */
	if ((transport->readiness != VS_TRANSPORT_RESET) &&
			transport->readiness != VS_TRANSPORT_LOCAL_RESET &&
			transport->readiness != VS_TRANSPORT_REMOTE_READY) {
		/*
		 * Flush the Axons' TX queues. We can't flush the RX queues
		 * until after the remote end has acknowledged the reset.
		 */
		transport_flush_tx_queues(transport);

		/*
		 * Raise a reset request VIRQ, and discard any incoming reset
		 * or ready notifications as they are now stale. Note that we
		 * must do this in a single syscall.
		 */
		err = _okl4_sys_vinterrupt_clear_and_raise(
				transport->reset_okl4_irq,
				transport->reset_cap, 0UL,
				VS_TRANSPORT_VIRQ_RESET_REQ).error;
		if (err != OKL4_OK) {
			dev_err(transport->axon_dev, "Reset raise failed: %d\n",
					(int)err);
		}

		/* Local reset is complete */
		if (transport->readiness != VS_TRANSPORT_SHUTDOWN)
			transport->readiness = VS_TRANSPORT_LOCAL_RESET;
	} else {
		/* Already in reset */
		reset_complete = true;
	}

	spin_unlock_irqrestore(&transport->readiness_lock, flags);

	return reset_complete;
}

static void transport_reset(struct vs_transport *_transport)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev, "reset\n");

	if (transport_axon_reset(transport)) {
		vs_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				"reset while already reset (no-op)\n");

		vs_session_handle_reset(transport->session_dev);
	}
}

static void transport_ready(struct vs_transport *_transport)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	okl4_error_t err;

	vs_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			"%s: becoming ready\n", __func__);

	/*
	 * Make sure any previously scheduled freed bufs work is cancelled.
	 * It should not be possible for this to be rescheduled later, as long
	 * as the transport is in reset.
	 */
	cancel_delayed_work_sync(&transport->free_bufs_work);
	spin_lock_irq(&transport->readiness_lock);

	atomic_set(&transport->free_bufs_balance, 0);
	transport->free_bufs_pending = false;

	switch(transport->readiness) {
	case VS_TRANSPORT_RESET:
		transport->readiness = VS_TRANSPORT_LOCAL_READY;
		break;
	case VS_TRANSPORT_REMOTE_READY:
		vs_session_handle_activate(transport->session_dev);
		transport->readiness = VS_TRANSPORT_ACTIVE;
		break;
	case VS_TRANSPORT_LOCAL_RESET:
		/*
		 * Session layer is confused; usually due to the reset at init
		 * time, which it did not explicitly request, not having
		 * completed yet. We just ignore it and wait for the reset. We
		 * could avoid this by not starting the session until the
		 * startup reset completes.
		 */
		spin_unlock_irq(&transport->readiness_lock);
		return;
	case VS_TRANSPORT_SHUTDOWN:
		/* Do nothing. */
		spin_unlock_irq(&transport->readiness_lock);
		return;
	default:
		/* Session layer is broken */
		WARN(1, "transport_ready() called in the wrong state: %d",
				transport->readiness);
		goto fail;
	}

	/* Raise a ready notification VIRQ. */
	err = _okl4_sys_vinterrupt_raise(transport->reset_cap,
			VS_TRANSPORT_VIRQ_READY);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "Ready raise failed: %d\n",
				(int)err);
		goto fail;
	}

	/*
	 * Set up the Axons' queue pointers.
	 */
	err = _okl4_sys_axon_set_send_area(transport->tx_cap,
			transport->tx_phys, transport->tx_size);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "TX set area failed: %d\n",
				(int)err);
		goto fail;
	}

	err = _okl4_sys_axon_set_send_queue(transport->tx_cap,
			transport->tx_phys);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "TX set queue failed: %d\n",
				(int)err);
		goto fail;
	}

	err = _okl4_sys_axon_set_recv_area(transport->rx_cap,
			transport->rx_phys, transport->rx_size);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "RX set area failed: %d\n",
				(int)err);
		goto fail;
	}

	err = _okl4_sys_axon_set_recv_queue(transport->rx_cap,
			transport->rx_phys);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "RX set queue failed: %d\n",
				(int)err);
		goto fail;
	}

	spin_unlock_irq(&transport->readiness_lock);
	return;

fail:
	spin_unlock_irq(&transport->readiness_lock);

	transport_axon_reset(transport);
}

static int transport_service_add(struct vs_transport *_transport,
		struct vs_service_device *service)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	struct vs_mv_service_info *service_info;

	/*
	 * We can't print out the core service add because the session
	 * isn't fully registered at that time.
	 */
	if (service->id != 0)
		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev,
				"Add service - id = %d\n", service->id);

	service_info = kzalloc(sizeof(*service_info), GFP_KERNEL);
	if (!service_info)
		return -ENOMEM;

	kref_init(&service_info->kref);

	/* Matching vs_put_service() is in transport_info_free */
	service_info->service = vs_get_service(service);

	/* Make the service_info visible */
	rcu_assign_pointer(service->transport_priv, service_info);

	__set_bit(service->id, transport->service_bitmap);

	return 0;
}

static void transport_service_remove(struct vs_transport *_transport,
		struct vs_service_device *service)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	struct vs_mv_service_info *service_info;

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev, "Remove service - id = %d\n",
			service->id);

	__clear_bit(service->id, transport->service_bitmap);

	service_info = service->transport_priv;
	rcu_assign_pointer(service->transport_priv, NULL);

	if (service_info->ready) {
		dev_err(transport->axon_dev,
				"Removing service %d while ready\n",
				service->id);
		transport_fatal_error(transport, "removing ready service");
	}

	transport_put_service_info(service_info);
}

static struct vs_axon_tx_pool *
transport_axon_init_tx_pool(struct vs_transport_axon *transport,
		size_t msg_size, unsigned send_quota)
{
	struct vs_axon_tx_pool *pool;

	pool = devm_kzalloc(transport->axon_dev, sizeof(*pool) +
			(sizeof(unsigned long) * BITS_TO_LONGS(send_quota)),
			GFP_KERNEL);
	if (!pool)
		return ERR_PTR(-ENOMEM);

	pool->transport = transport;
	pool->alloc_order = ilog2(msg_size + sizeof(vs_service_id_t));
	pool->count = send_quota;

	pool->base_vaddr = dmam_alloc_coherent(transport->axon_dev,
			send_quota << pool->alloc_order, &pool->base_laddr,
			GFP_KERNEL);
	if (!pool->base_vaddr) {
		dev_err(transport->axon_dev, "Couldn't allocate %lu times %zu bytes for TX\n",
				(unsigned long)pool->count, (size_t)1 << pool->alloc_order);
		devm_kfree(transport->axon_dev, pool);
		return ERR_PTR(-ENOMEM);
	}

	kref_init(&pool->kref);
	return pool;
}

static int transport_service_start(struct vs_transport *_transport,
		struct vs_service_device *service)
{
	struct vs_mv_service_info *service_info;
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	struct vs_notify_info *info;
	int i, ret;
	bool enable_rx;

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev, "Start service - id = %d\n",
			service->id);

	service_info = service->transport_priv;
	__transport_get_service_info(service_info);

	/* We shouldn't have any mbufs left from before the last reset. */
	if (WARN_ON(atomic_read(&service_info->outstanding_frees))) {
		transport_put_service_info(service_info);
		return -EBUSY;
	}

	/*
	 * The watermark is set to half of the received-message quota, rounded
	 * down, plus one. This is fairly arbitrary. The constant offset
	 * ensures that we don't set it to 0 for services with 1 quota (and
	 * thus trigger infinite free_bufs messages).
	 */
	service_info->recv_freed_watermark = (service->recv_quota + 1) / 2;

	if (WARN_ON(service->notify_recv_bits + service->notify_recv_offset >
				transport->notify_rx_nirqs * BITS_PER_LONG)) {
		transport_put_service_info(service_info);
		return -EINVAL;
	}

	if (WARN_ON(service->notify_send_bits + service->notify_send_offset >
				transport->notify_tx_nirqs * BITS_PER_LONG)) {
		transport_put_service_info(service_info);
		return -EINVAL;
	}

	/* This is called twice for the core client only. */
	WARN_ON(service->id != 0 && service_info->ready);

	if (!service_info->ready) {
		WARN_ON(atomic_read(&service_info->send_alloc));
		WARN_ON(atomic_read(&service_info->recv_freed));
		WARN_ON(atomic_read(&service_info->recv_inflight));
	}

	/* Create the TX buffer pool. */
	WARN_ON(service->send_quota && service_info->tx_pool);
	if (service->send_quota) {
		service_info->tx_pool = transport_axon_init_tx_pool(transport,
				transport->msg_size, service->send_quota);
		if (IS_ERR(service_info->tx_pool)) {
			ret = PTR_ERR(service_info->tx_pool);
			service_info->tx_pool = NULL;
			transport_put_service_info(service_info);
			return ret;
		}
	}

	/* Preallocate some RX buffers, if necessary. */
	spin_lock_irq(&transport->rx_alloc_lock);
	i = min(transport->rx_alloc_extra,
			service->recv_quota - service_info->rx_allocated);
	transport->rx_alloc_extra -= i;
	service_info->rx_allocated += i;
	spin_unlock_irq(&transport->rx_alloc_lock);

	for (; service_info->rx_allocated < service->recv_quota;
			service_info->rx_allocated++) {
		dma_addr_t laddr;
		struct vs_axon_rx_freelist_entry *buf =
			dma_pool_alloc(transport->rx_pool, GFP_KERNEL, &laddr);
		if (WARN_ON(!buf))
			break;
		buf->laddr = laddr;

		spin_lock_irq(&transport->rx_alloc_lock);
		list_add(&buf->list, &transport->rx_freelist);
		spin_unlock_irq(&transport->rx_alloc_lock);
	}

	for (i = 0; i < service->notify_recv_bits; i++) {
		unsigned bit = i + service->notify_recv_offset;
		info = &transport->transport.notify_info[bit];

		info->service_id = service->id;
		info->offset = service->notify_recv_offset;
	}

	atomic_set(&service_info->send_inflight, 0);

	/*
	 * If this is the core service and it wasn't ready before, we need to
	 * enable RX for the whole transport.
	 */
	enable_rx = service->id == 0 && !service_info->ready;

	service_info->ready = true;

	/* We're now ready to receive. */
	if (enable_rx)
		tasklet_enable(&transport->rx_tasklet);

	transport_put_service_info(service_info);

	return 0;
}

static int transport_service_reset(struct vs_transport *_transport,
		struct vs_service_device *service)
{
	struct vs_mv_service_info *service_info;
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);
	struct vs_mbuf_axon *child, *tmp;
	int ret = 0, service_id, send_remaining, recv_remaining;

	vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			transport->axon_dev, "Reset service - id = %d\n",
			service->id);

	service_info = service->transport_priv;
	__transport_get_service_info(service_info);

	/*
	 * Clear the ready bit with the tasklet disabled. After this point,
	 * incoming messages will be discarded by transport_process_msg()
	 * without incrementing recv_inflight, so we won't spuriously see
	 * nonzero recv_inflight values for messages that would be discarded
	 * in the session layer.
	 */
	tasklet_disable(&transport->rx_tasklet);
	service_info->ready = false;
	if (service->id)
		tasklet_enable(&transport->rx_tasklet);

	/*
	 * Cancel and free all pending outgoing messages for the service being
	 * reset; i.e. those that have been sent by the service but are not
	 * yet in the axon queue.
	 *
	 * Note that this does not clean out the axon queue; messages there
	 * are already visible to OKL4 and may be transferred at any time,
	 * so we treat those as already sent.
	 */
	spin_lock_irq(&transport->readiness_lock);
	list_for_each_entry_safe(child, tmp, &transport->tx_queue, base.queue) {
		service_id = transport_get_mbuf_service_id(transport,
				mbuf_real_base(child), NULL);
		if (service_id == service->id) {
			list_del(&child->base.queue);
			__transport_tx_pool_free(child->pool, child->laddr);
		}
	}
	spin_unlock_irq(&transport->readiness_lock);

	/*
	 * If any buffers remain allocated, we mark them as outstanding frees.
	 * The transport will remain disabled until this count goes to zero.
	 */
	send_remaining = atomic_read(&service_info->send_alloc);
	recv_remaining = atomic_read(&service_info->recv_inflight);
	ret = atomic_add_return(send_remaining + recv_remaining,
			&service_info->outstanding_frees);
	dev_dbg(transport->axon_dev, "reset service %d with %d outstanding (send %d, recv %d)\n",
			service->id, ret, send_remaining, recv_remaining);

	/*
	 * Reduce the send alloc count to 0, accounting for races with frees,
	 * which might have reduced either the alloc count or the outstanding
	 * count.
	 */
	while (send_remaining > 0) {
		unsigned new_send_remaining = atomic_cmpxchg(
				&service_info->send_alloc, send_remaining, 0);
		if (send_remaining == new_send_remaining) {
			smp_mb();
			break;
		}
		WARN_ON(send_remaining < new_send_remaining);
		ret = atomic_sub_return(send_remaining - new_send_remaining,
				&service_info->outstanding_frees);
		send_remaining = new_send_remaining;
		dev_dbg(transport->axon_dev, "failed to zero send quota, now %d outstanding (%d send)\n",
				ret, send_remaining);
	}

	/* Repeat the above for the recv inflight count. */
	while (recv_remaining > 0) {
		unsigned new_recv_remaining = atomic_cmpxchg(
				&service_info->recv_inflight, recv_remaining,
				0);
		if (recv_remaining == new_recv_remaining) {
			smp_mb();
			break;
		}
		WARN_ON(recv_remaining < new_recv_remaining);
		ret = atomic_sub_return(recv_remaining - new_recv_remaining,
				&service_info->outstanding_frees);
		recv_remaining = new_recv_remaining;
		dev_dbg(transport->axon_dev, "failed to zero recv quota, now %d outstanding (%d send)\n",
				ret, recv_remaining);
	}

	/* The outstanding frees count should never go negative */
	WARN_ON(ret < 0);

	/* Discard any outstanding freed buffer notifications. */
	atomic_set(&service_info->recv_freed, 0);

	/*
	 * Wait for any previously queued free_bufs work to finish. This
	 * guarantees that any freed buffer notifications that are already in
	 * progress will be sent to the remote end before we return, and thus
	 * before the reset is signalled.
	 */
	flush_delayed_work(&transport->free_bufs_work);

	if (!ret)
		transport_free_mbuf_pools(transport, service, service_info);

	transport_put_service_info(service_info);

	return ret;
}

static ssize_t transport_service_send_avail(struct vs_transport *_transport,
		struct vs_service_device *service)
{
	struct vs_mv_service_info *service_info;
	ssize_t count = 0;

	service_info = service->transport_priv;
	if (!service_info)
		return -EINVAL;

	__transport_get_service_info(service_info);

	count = service->send_quota -
		atomic_read(&service_info->send_inflight);

	transport_put_service_info(service_info);

	return count < 0 ? 0 : count;
}

static void transport_get_notify_bits(struct vs_transport *_transport,
		unsigned *send_notify_bits, unsigned *recv_notify_bits)
{
	struct vs_transport_axon *transport = to_vs_transport_axon(_transport);

	*send_notify_bits = transport->notify_tx_nirqs * BITS_PER_LONG;
	*recv_notify_bits = transport->notify_rx_nirqs * BITS_PER_LONG;
}

static void transport_get_quota_limits(struct vs_transport *_transport,
		unsigned *send_quota, unsigned *recv_quota)
{
	/*
	 * This driver does not need to enforce a quota limit, because message
	 * buffers are allocated from the kernel heap rather than a fixed
	 * buffer area. The queue length only determines the maximum size of
	 * a message batch, and the number of preallocated RX buffers.
	 *
	 * Note that per-service quotas are still enforced; there is simply no
	 * hard limit on the total of all service quotas.
	 */

	*send_quota = UINT_MAX;
	*recv_quota = UINT_MAX;
}

static const struct vs_transport_vtable tvt = {
	.alloc_mbuf		= transport_alloc_mbuf,
	.free_mbuf		= transport_free_mbuf,
	.mbuf_size		= transport_mbuf_size,
	.max_mbuf_size		= transport_max_mbuf_size,
	.send			= transport_send,
	.flush			= transport_flush,
	.notify			= transport_notify,
	.reset			= transport_reset,
	.ready			= transport_ready,
	.service_add		= transport_service_add,
	.service_remove		= transport_service_remove,
	.service_start		= transport_service_start,
	.service_reset		= transport_service_reset,
	.service_send_avail	= transport_service_send_avail,
	.get_notify_bits	= transport_get_notify_bits,
	.get_quota_limits	= transport_get_quota_limits,
};

/* Incoming notification handling for client */
static irqreturn_t transport_axon_notify_virq(int irq, void *priv)
{
	struct vs_transport_axon *transport = (struct vs_transport_axon *)priv;
	struct vs_notify_info *n_info;
	unsigned long offset, bit = 0, notification;
	int word;
	okl4_virq_flags_t payload = okl4_get_virq_payload(irq);

	for (word = 0; word < transport->notify_rx_nirqs; word++)
		if (irq == transport->notify_irq[word])
			break;

	if (word == transport->notify_rx_nirqs) {
		dev_err(transport->axon_dev, "Bad IRQ %d\n", irq);
		return IRQ_NONE;
	}

	vs_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
			"Got notification irq\n");

#if defined(__BIG_ENDIAN)
	/*
	 * We rely on being able to use the Linux bitmap operations directly
	 * on the VIRQ payload.
	 */
	BUILD_BUG_ON((sizeof(payload) % sizeof(unsigned long)) != 0);
#endif

	for_each_set_bit(bit, (unsigned long *)&payload, sizeof(payload) * 8) {
		offset = bit + word * BITS_PER_LONG;

		/*
		 * We need to know which service id is associated
		 * with which notification bit here. The transport is informed
		 * about notification bit - service id mapping during the
		 * initialhandshake protocol.
		 */
		n_info = &transport->transport.notify_info[offset];

		notification = 1UL << (offset - n_info->offset);
		vs_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				"Got notification bit %lu for service %d\n",
				notification, n_info->service_id);

		/* FIXME: Jira ticket SDK-2145 - shivanik. */
		vs_session_handle_notify(transport->session_dev, notification,
				n_info->service_id);
	}

	return IRQ_HANDLED;
}

static irqreturn_t transport_axon_reset_irq(int irq, void *priv)
{
	struct vs_transport_axon *transport = (struct vs_transport_axon *)priv;
	bool do_reset = false;

	u32 payload = okl4_get_virq_payload(irq);

	spin_lock(&transport->readiness_lock);

	if (payload & VS_TRANSPORT_VIRQ_RESET_REQ) {
		okl4_error_t err;

		transport->readiness = VS_TRANSPORT_RESET;

		/* Flush the queues in both directions */
		transport_flush_tx_queues(transport);
		transport_flush_rx_queues(transport);

		/*
		 * When sending an ack, it is important to cancel any earlier
		 * ready notification, so the recipient can safely assume that
		 * the ack precedes any ready it sees
		 */
		err = _okl4_sys_vinterrupt_modify(transport->reset_cap,
				~VS_TRANSPORT_VIRQ_READY,
				VS_TRANSPORT_VIRQ_RESET_ACK);
		if (err != OKL4_OK) {
			dev_warn(transport->axon_dev,
					"Error sending reset ack: %d\n", (int)err);
		}

		/*
		 * Discard any pending ready event; it must have happened
		 * before the reset request was raised, because we had not
		 * yet sent the reset ack.
		 */
		payload = 0;
		do_reset = true;
	} else if (payload & VS_TRANSPORT_VIRQ_RESET_ACK) {
		transport->readiness = VS_TRANSPORT_RESET;

		/*
		 * Flush the RX queues, as we know at this point that the
		 * other end has flushed its TX queues.
		 */
		transport_flush_rx_queues(transport);

		/*
		 * Preserve any pending ready event; it must have been
		 * generated after the ack (see above)
		 */
		payload &= VS_TRANSPORT_VIRQ_READY;
		do_reset = true;
	}

	if (do_reset) {
		/*
		 * Reset the session. Note that duplicate calls to this are
		 * expected if there are duplicate resets; they don't
		 * necessarily match activate calls.
		 */
		vs_session_handle_reset(transport->session_dev);
	}

	if (payload & VS_TRANSPORT_VIRQ_READY) {
		if (transport->readiness == VS_TRANSPORT_RESET) {
			transport->readiness = VS_TRANSPORT_REMOTE_READY;
		} else if (transport->readiness == VS_TRANSPORT_LOCAL_READY) {
			vs_session_handle_activate(transport->session_dev);
			transport->readiness = VS_TRANSPORT_ACTIVE;
		} else {
			/* Ready lost a race with reset; ignore it. */
		}
	}

	spin_unlock(&transport->readiness_lock);

	return IRQ_HANDLED;
}

/*
 * Axon VIRQ handling.
 */
static irqreturn_t transport_axon_rx_irq(int irq, void *priv)
{
	struct vs_transport_axon *transport = (struct vs_transport_axon *)priv;

	okl4_axon_virq_flags_t flags = okl4_get_virq_payload(irq);

	if (okl4_axon_virq_flags_getfault(&flags)) {
		dev_err_ratelimited(transport->axon_dev,
				"fault on RX axon buffer or queue; resetting\n");
		transport_axon_reset(transport);
	} else if (okl4_axon_virq_flags_getready(&flags)) {
		tasklet_schedule(&transport->rx_tasklet);
	}

	return IRQ_HANDLED;
}

static irqreturn_t transport_axon_tx_irq(int irq, void *priv)
{
	struct vs_transport_axon *transport = (struct vs_transport_axon *)priv;

	okl4_axon_virq_flags_t flags = okl4_get_virq_payload(irq);

	if (okl4_axon_virq_flags_getfault(&flags)) {
		dev_err_ratelimited(transport->axon_dev,
				"fault on TX axon buffer or queue; resetting\n");
		transport_axon_reset(transport);
	} else if (okl4_axon_virq_flags_getready(&flags)) {
		spin_lock(&transport->readiness_lock);
		if (!list_empty(&transport->tx_queue))
			tasklet_schedule(&transport->tx_tasklet);
		spin_unlock(&transport->readiness_lock);
	}

	return IRQ_HANDLED;
}

static void transport_rx_tasklet(unsigned long data)
{
	struct vs_transport_axon *transport = (struct vs_transport_axon *)data;
	int status;
	struct _okl4_sys_axon_process_recv_return recv_result;

	/* Refill the RX queue */
	spin_lock_irq(&transport->rx_alloc_lock);
	while (!list_empty(&transport->rx_freelist)) {
		struct vs_axon_rx_freelist_entry *buf;
		buf = list_first_entry(&transport->rx_freelist,
				struct vs_axon_rx_freelist_entry, list);
		list_del(&buf->list);
		status = transport_rx_queue_buffer(transport, buf, buf->laddr);
		if (status < 0)
			list_add(&buf->list, &transport->rx_freelist);
		if (status <= 0)
			break;
	}
	spin_unlock_irq(&transport->rx_alloc_lock);

	/* Start the transfer */
	recv_result = _okl4_sys_axon_process_recv(transport->rx_cap,
			MAX_TRANSFER_CHUNK);

	if (recv_result.error == OKL4_OK) {
		status = 1;
	} else {
		status = okl4_error_to_errno(recv_result.error);
		vs_dev_debug(VS_DEBUG_TRANSPORT, transport->session_dev,
				transport->axon_dev, "rx syscall fail: %d",
				status);
	}

	/* Process the received messages */
	while (status > 0)
		status = transport_process_msg(transport);

	if (status == -ENOMEM) {
		/* Give kswapd some time to reclaim pages */
		mod_timer(&transport->rx_retry_timer, jiffies + HZ);
	} else if (status == -ENOBUFS) {
		/*
		 * Reschedule ourselves if more RX buffers are available,
		 * otherwise do nothing until a buffer is freed
		 */
		spin_lock_irq(&transport->rx_alloc_lock);
		if (!list_empty(&transport->rx_freelist))
			tasklet_schedule(&transport->rx_tasklet);
		spin_unlock_irq(&transport->rx_alloc_lock);
	} else if (!status && !recv_result.send_empty) {
		/* There are more messages waiting; reschedule */
		tasklet_schedule(&transport->rx_tasklet);
	} else if (status < 0 && status != -ECONNRESET) {
		/* Something else went wrong, other than a reset */
		dev_err(transport->axon_dev, "Fatal RX error %d\n", status);
		transport_fatal_error(transport, "rx failure");
	} else {
		/* Axon is empty; wait for an RX interrupt */
	}
}

static void transport_tx_tasklet(unsigned long data)
{
	struct vs_transport_axon *transport = (struct vs_transport_axon *)data;
	struct vs_mbuf_axon *mbuf;
	vs_service_id_t service_id;
	int err;

	spin_lock_irq(&transport->readiness_lock);

	/* Check to see if there is anything in the queue to send */
	if (list_empty(&transport->tx_queue)) {
		/*
		 * Queue is empty, probably because a service reset cancelled
		 * some pending messages. Nothing to do.
		 */
		spin_unlock_irq(&transport->readiness_lock);
		return;
	}

	/*
	 * Try to send the mbuf.  If it can't, the channel must be
	 * full again so wait until the next can send event.
	 */
	mbuf = list_first_entry(&transport->tx_queue, struct vs_mbuf_axon,
			base.queue);

	service_id = transport_get_mbuf_service_id(transport,
			mbuf_real_base(mbuf), NULL);

	err = __transport_send(transport, mbuf, service_id,
			VS_TRANSPORT_SEND_FLAGS_MORE);
	if (err == -ENOSPC) {
		/*
		 * The channel is currently full. Leave the message in the
		 * queue and try again when it has emptied.
		 */
		__transport_flush(transport);
		goto out_unlock;
	}
	if (err) {
		/*
		 * We cannot properly handle a message send error here because
		 * we have already returned success for the send to the service
		 * driver when the message was queued. We don't want to leave
		 * the message in the queue, since it could cause a DoS if the
		 * error is persistent. Give up and force a transport reset.
		 */
		dev_err(transport->axon_dev,
				"Failed to send queued mbuf: %d\n", err);
		spin_unlock_irq(&transport->readiness_lock);
		transport_fatal_error(transport, "queued send failure");
		return;
	}

	/* Message sent, remove it from the queue and free the local copy */
	list_del(&mbuf->base.queue);
	transport_free_sent_mbuf(transport, mbuf);

	/* Check to see if we have run out of messages to send */
	if (list_empty(&transport->tx_queue)) {
		/* Nothing left in the queue; flush and return */
		__transport_flush(transport);
	} else {
		/* Reschedule to send the next message */
		tasklet_schedule(&transport->tx_tasklet);
	}

out_unlock:
	spin_unlock_irq(&transport->readiness_lock);
}

static void transport_rx_retry_timer(unsigned long data)
{
	struct vs_transport_axon *transport = (struct vs_transport_axon *)data;

	/* Try to receive again; hopefully we have memory now */
	tasklet_schedule(&transport->rx_tasklet);
}

/* Transport device management */

static int alloc_notify_info(struct device *dev, struct vs_notify_info **info,
		int *info_size, int virqs)
{
	/* Each VIRQ can handle BITS_PER_LONG notifications */
	*info_size = sizeof(struct vs_notify_info) * (virqs * BITS_PER_LONG);
	*info = devm_kzalloc(dev, *info_size, GFP_KERNEL);
	if (!(*info))
		return -ENOMEM;

	memset(*info, 0, *info_size);
	return 0;
}

static int transport_axon_probe_virqs(struct vs_transport_axon *transport)
{
	struct device *device = transport->axon_dev;
	struct device_node *axon_node = device->of_node;
	struct device_node *vs_node = transport->of_node;
	struct irq_data *irqd;
	struct property *irqlines;
	int ret, num_virq_lines;
	struct device_node *virq_node = NULL;
	u32 cap;
	int i, irq_count;

	if (of_irq_count(axon_node) < 2) {
		dev_err(device, "Missing axon interrupts\n");
		return -ENODEV;
	}

	irq_count = of_irq_count(vs_node);
	if (irq_count < 1) {
		dev_err(device, "Missing reset interrupt\n");
		return -ENODEV;
	} else if (irq_count > 1 + MAX_NOTIFICATION_LINES) {
		dev_warn(device,
			"Too many notification interrupts; only the first %d will be used\n",
			MAX_NOTIFICATION_LINES);
	}

	/* Find the TX and RX axon IRQs and the reset IRQ */
	transport->tx_irq = irq_of_parse_and_map(axon_node, 0);
	if (!transport->tx_irq) {
		dev_err(device, "No TX IRQ\n");
		return -ENODEV;
	}

	transport->rx_irq = irq_of_parse_and_map(axon_node, 1);
	if (!transport->rx_irq) {
		dev_err(device, "No RX IRQ\n");
		return -ENODEV;
	}

	transport->reset_irq = irq_of_parse_and_map(vs_node, 0);
	if (!transport->reset_irq) {
		dev_err(device, "No reset IRQ\n");
		return -ENODEV;
	}
	irqd = irq_get_irq_data(transport->reset_irq);
	if (!irqd) {
		dev_err(device, "No reset IRQ data\n");
		return -ENODEV;
	}
	transport->reset_okl4_irq = irqd_to_hwirq(irqd);

	/* Find the notification IRQs */
	transport->notify_rx_nirqs = irq_count - 1;
	for (i = 0; i < transport->notify_rx_nirqs; i++) {
		transport->notify_irq[i] = irq_of_parse_and_map(vs_node,
				i + 1);
		if (!transport->notify_irq[i]) {
			dev_err(device, "Bad notify IRQ\n");
			return -ENODEV;
		}
	}

	/* Find all outgoing virq lines */
	irqlines = of_find_property(vs_node, "okl,interrupt-lines", NULL);
	if (!irqlines || irqlines->length < sizeof(u32)) {
		dev_err(device, "No VIRQ sources found");
		return -ENODEV;
	}
	num_virq_lines = irqlines->length / sizeof(u32);

	virq_node = of_parse_phandle(vs_node, "okl,interrupt-lines", 0);
	if (!virq_node) {
		dev_err(device, "No reset VIRQ line object\n");
		return -ENODEV;
	}
	ret = of_property_read_u32(virq_node, "reg", &cap);
	if (ret || cap == OKL4_KCAP_INVALID) {
		dev_err(device, "Bad reset VIRQ line\n");
		return -ENODEV;
	}
	transport->reset_cap = cap;

	transport->notify_tx_nirqs = num_virq_lines - 1;
	for (i = 0; i < transport->notify_tx_nirqs; i++) {
		virq_node = of_parse_phandle(vs_node, "okl,interrupt-lines",
				i + 1);
		if (!virq_node) {
			dev_err(device, "No notify VIRQ line object\n");
			return -ENODEV;
		}
		ret = of_property_read_u32(virq_node, "reg", &cap);
		if (ret || cap == OKL4_KCAP_INVALID) {
			dev_err(device, "Bad notify VIRQ line\n");
			return -ENODEV;
		}
		transport->notify_cap[i] = cap;
	}

	return 0;
}

static int transport_axon_request_irqs(struct vs_transport_axon *transport)
{
	struct device *device = transport->axon_dev;
	int i, ret;

	ret = devm_request_irq(device, transport->reset_irq,
			transport_axon_reset_irq, IRQF_TRIGGER_HIGH,
			dev_name(transport->axon_dev), transport);
	if (ret < 0)
		return ret;

	ret = devm_request_irq(device, transport->tx_irq,
			transport_axon_tx_irq, IRQF_TRIGGER_HIGH,
			dev_name(transport->axon_dev), transport);
	if (ret < 0)
		return ret;

	ret = devm_request_irq(device, transport->rx_irq,
			transport_axon_rx_irq, IRQF_TRIGGER_HIGH,
			dev_name(transport->axon_dev), transport);
	if (ret < 0)
		return ret;

	for (i = 0; i < transport->notify_rx_nirqs; i++) {
		ret = devm_request_irq(device, transport->notify_irq[i],
				transport_axon_notify_virq, IRQF_TRIGGER_HIGH,
				dev_name(transport->axon_dev), transport);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int transport_axon_setup_descs(struct vs_transport_axon *transport)
{
	const int rx_buffer_order = ilog2(transport->msg_size +
			sizeof(vs_service_id_t));
	const size_t rx_queue_size = sizeof(*transport->rx) +
		(sizeof(*transport->rx_descs) * transport->queue_size) +
		(sizeof(*transport->rx_ptrs) * transport->queue_size);
	const size_t tx_queue_size = sizeof(*transport->tx) +
		(sizeof(*transport->tx_descs) * transport->queue_size);
	const size_t queue_size = ALIGN(rx_queue_size,
			__alignof__(*transport->tx)) + tx_queue_size;

	struct _okl4_sys_mmu_lookup_pn_return lookup_return;
	void *queue;
	struct device_node *seg_node;
	u32 seg_index;
	okl4_kcap_t seg_cap;
	okl4_error_t err;
	dma_addr_t dma_handle;
	const __be32 *prop;
	int len, ret;

	/*
	 * Allocate memory for the queue descriptors.
	 *
	 * We allocate one block for both rx and tx because the minimum
	 * allocation from dmam_alloc_coherent is usually a whole page.
	 */
	ret = -ENOMEM;
	queue = dmam_alloc_coherent(transport->axon_dev, queue_size,
			&dma_handle, GFP_KERNEL);
	if (queue == NULL) {
		dev_err(transport->axon_dev, "Failed to allocate %zd bytes for queue descriptors\n",
				queue_size);
		goto fail_alloc_dma;
	}
	memset(queue, 0, queue_size);

	/*
	 * Find the OKL4 physical segment object to attach to the axons.
	 *
	 * If the device has a CMA area, and the cell's memory segments have
	 * not been split unnecessarily, then all allocations through the DMA
	 * API for this device will be within a single segment. So, we can
	 * simply look up the segment that contains the queue.
	 *
	 * The location and size of the CMA area can be configured elsewhere.
	 * In 3.12 and later a device-specific area can be reserved via the
	 * standard device tree reserved-memory properties. Otherwise, the
	 * global area will be used, which has a size configurable on the
	 * kernel command line and defaults to 16MB.
	 */

	/* Locate the physical segment */
	ret = -ENODEV;
	lookup_return = _okl4_sys_mmu_lookup_pn(okl4_mmu_cap,
			dma_handle >> OKL4_DEFAULT_PAGEBITS, -1);
	err = okl4_mmu_lookup_index_geterror(&lookup_return.segment_index);
	if (err == OKL4_ERROR_NOT_IN_SEGMENT) {
		dev_err(transport->axon_dev,
				"No segment found for DMA address %pK (%#llx)!\n",
				queue, (unsigned long long)dma_handle);
		goto fail_lookup_segment;
	}
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev,
				"Could not look up segment for DMA address %pK (%#llx): OKL4 error %d\n",
				queue, (unsigned long long)dma_handle,
				(int)err);
		goto fail_lookup_segment;
	}
	seg_index = okl4_mmu_lookup_index_getindex(&lookup_return.segment_index);

	dev_dbg(transport->axon_dev, "lookup pn %#lx got error %ld segment %ld count %lu offset %#lx\n",
			(long)(dma_handle >> OKL4_DEFAULT_PAGEBITS),
			(long)err, (long)seg_index,
			(unsigned long)lookup_return.count_pn,
			(unsigned long)lookup_return.offset_pn);

	/* Locate the physical segment's OF node */
	for_each_compatible_node(seg_node, NULL, "okl,microvisor-segment") {
		u32 attach_index;
		ret = of_property_read_u32(seg_node, "okl,segment-attachment",
				&attach_index);
		if (attach_index == seg_index)
			break;
	}
	if (seg_node == NULL) {
		ret = -ENXIO;
		dev_err(transport->axon_dev, "No physical segment found for %pK\n",
				queue);
		goto fail_lookup_segment;
	}

	/* Determine the physical segment's cap */
	prop = of_get_property(seg_node, "reg", &len);
	ret = !!prop ? 0 : -EPERM;
	if (!ret)
		seg_cap = of_read_number(prop, of_n_addr_cells(seg_node));
	if (!ret && seg_cap == OKL4_KCAP_INVALID)
		ret = -ENXIO;
	if (ret < 0) {
		dev_err(transport->axon_dev, "missing physical-segment cap\n");
		goto fail_lookup_segment;
	}
	transport->segment = seg_cap;
	transport->segment_base =
		(round_down(dma_handle >> OKL4_DEFAULT_PAGEBITS,
			    lookup_return.count_pn) -
		 lookup_return.offset_pn) << OKL4_DEFAULT_PAGEBITS;

	dev_dbg(transport->axon_dev, "physical segment cap is %#lx, base %#llx\n",
			(unsigned long)transport->segment,
			(unsigned long long)transport->segment_base);

	/* Attach the segment to the Axon endpoints */
	err = _okl4_sys_axon_set_send_segment(transport->tx_cap,
			transport->segment, transport->segment_base);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "TX attach failed: %d\n",
				(int)err);
		ret = okl4_error_to_errno(err);
		goto fail_attach;
	}

	err = _okl4_sys_axon_set_recv_segment(transport->rx_cap,
			transport->segment, transport->segment_base);
	if (err != OKL4_OK) {
		dev_err(transport->axon_dev, "RX attach failed: %d\n",
				(int)err);
		ret = okl4_error_to_errno(err);
		goto fail_attach;
	}

	/* Array of pointers to the source TX pool for each outgoing buffer. */
	transport->tx_pools = devm_kzalloc(transport->axon_dev,
			sizeof(*transport->tx_pools) * transport->queue_size,
			GFP_KERNEL);
	if (!transport->tx_pools) {
		err = -ENOMEM;
		goto fail_alloc_tx_pools;
	}

	/* Set up the rx queue descriptors. */
	transport->rx = queue;
	transport->rx_phys = dma_handle;
	transport->rx_size = rx_queue_size;
	transport->rx_descs = (void *)(transport->rx + 1);
	transport->rx_ptrs = (void *)(transport->rx_descs + transport->queue_size);
	okl4_axon_queue_size_setallocorder(&transport->rx->queue_sizes[0],
			rx_buffer_order);
	transport->rx->queues[0].queue_offset = sizeof(*transport->rx);
	transport->rx->queues[0].entries = transport->queue_size;
	transport->rx->queues[0].uptr = 0;
	transport->rx->queues[0].kptr = 0;
	transport->rx_uptr_allocated = 0;

	/* Set up the tx queue descriptors. */
	transport->tx = queue + ALIGN(rx_queue_size,
			__alignof__(*transport->tx));
	transport->tx_phys = dma_handle + ((void *)transport->tx - queue);
	transport->tx_size = tx_queue_size;
	transport->tx_descs = (void *)(transport->tx + 1);
	transport->tx->queues[0].queue_offset = sizeof(*transport->tx);
	transport->tx->queues[0].entries = transport->queue_size;
	transport->tx->queues[0].uptr = 0;
	transport->tx->queues[0].kptr = 0;
	transport->tx_uptr_freed = 0;

	/* Create a DMA pool for the RX buffers. */
	transport->rx_pool = dmam_pool_create("vs_axon_rx_pool",
			transport->axon_dev, 1 << rx_buffer_order,
			max(dma_get_cache_alignment(),
				1 << OKL4_PRESHIFT_LADDR_AXON_DATA_INFO), 0);

	return 0;

fail_alloc_tx_pools:
fail_attach:
fail_lookup_segment:
	dmam_free_coherent(transport->axon_dev, queue_size, queue, dma_handle);
fail_alloc_dma:
	return ret;
}

static void transport_axon_free_descs(struct vs_transport_axon *transport)
{
	int i;

	tasklet_disable(&transport->rx_tasklet);
	tasklet_kill(&transport->rx_tasklet);

	tasklet_disable(&transport->tx_tasklet);
	tasklet_kill(&transport->tx_tasklet);

	cancel_delayed_work_sync(&transport->free_bufs_work);

	transport->tx = NULL;
	transport->tx_descs = NULL;

	for (i = 0; i < transport->rx->queues[0].entries; i++) {
		struct okl4_axon_queue_entry *desc = &transport->rx_descs[i];

		if (okl4_axon_data_info_getusr(&desc->info)) {
			void *ptr = transport->rx_ptrs[i];
			dma_addr_t dma = okl4_axon_data_info_getladdr(&desc->info);
			dma_pool_free(transport->rx_pool, ptr, dma);
		}
	}

	transport->rx = NULL;
	transport->rx_descs = NULL;
	transport->rx_ptrs = NULL;

	/* Let devm free the queues so we don't have to keep the dma handle */
}

static int transport_axon_probe(struct platform_device *dev)
{
	struct vs_transport_axon *priv = NULL;
	u32 cap[2];
	u32 queue_size, msg_size;
	int ret, i;
	const char* name;

	if (!dev_get_cma_area(&dev->dev) && !okl4_single_physical_segment) {
		dev_err(&dev->dev, "Multiple physical segments, but CMA is disabled\n");
		return -ENOSYS;
	}

	dev->dev.coherent_dma_mask = ~(u64)0;
	dev->dev.archdata.dma_ops = &axon_dma_ops;

	priv = devm_kzalloc(&dev->dev, sizeof(struct vs_transport_axon) +
			sizeof(unsigned long), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&dev->dev, "create transport object failed\n");
		ret = -ENOMEM;
		goto err_alloc_priv;
	}
	dev_set_drvdata(&dev->dev, priv);

	priv->of_node = of_get_child_by_name(dev->dev.of_node,
			"virtual-session");
	if ((!priv->of_node) ||
			(!of_device_is_compatible(priv->of_node,
					"okl,virtual-session"))) {
		dev_err(&dev->dev, "missing virtual-session node\n");
		ret = -ENODEV;
		goto error_of_node;
	}

	name = dev->dev.of_node->full_name;
	of_property_read_string(dev->dev.of_node, "label", &name);

	if (of_property_read_bool(priv->of_node, "okl,is-client")) {
		priv->is_server = false;
	} else if (of_property_read_bool(priv->of_node, "okl,is-server")) {
		priv->is_server = true;
	} else {
		dev_err(&dev->dev, "virtual-session node is not marked as client or server\n");
		ret = -ENODEV;
		goto error_of_node;
	}

	priv->transport.vt = &tvt;
	priv->transport.type = "microvisor";
	priv->axon_dev = &dev->dev;

	/* Read the Axon caps */
	ret = of_property_read_u32_array(dev->dev.of_node, "reg", cap, 2);
	if (ret < 0 || cap[0] == OKL4_KCAP_INVALID ||
			cap[1] == OKL4_KCAP_INVALID) {
		dev_err(&dev->dev, "missing axon endpoint caps\n");
		ret = -ENODEV;
		goto error_of_node;
	}
	priv->tx_cap = cap[0];
	priv->rx_cap = cap[1];

	/* Set transport properties; default to a 64kb buffer */
	queue_size = 16;
	(void)of_property_read_u32(priv->of_node, "okl,queue-length",
			&queue_size);
	priv->queue_size = max((size_t)queue_size, MIN_QUEUE_SIZE);

	msg_size = PAGE_SIZE - sizeof(vs_service_id_t);
	(void)of_property_read_u32(priv->of_node, "okl,message-size",
			&msg_size);
	priv->msg_size = max((size_t)msg_size, MIN_MSG_SIZE);

	/*
	 * Since the Axon API requires received message size limits to be
	 * powers of two, we must round up the message size (including the
	 * space reserved for the service ID).
	 */
	priv->msg_size = roundup_pow_of_two(priv->msg_size +
			sizeof(vs_service_id_t)) - sizeof(vs_service_id_t);
	if (priv->msg_size != msg_size)
		dev_info(&dev->dev, "message size rounded up from %zd to %zd\n",
				(size_t)msg_size, priv->msg_size);

	INIT_LIST_HEAD(&priv->tx_queue);

	/* Initialise the activation state, tasklets, and RX retry timer */
	spin_lock_init(&priv->readiness_lock);
	priv->readiness = VS_TRANSPORT_INIT;

	tasklet_init(&priv->rx_tasklet, transport_rx_tasklet,
		(unsigned long)priv);
	tasklet_init(&priv->tx_tasklet, transport_tx_tasklet,
		(unsigned long)priv);

	INIT_DELAYED_WORK(&priv->free_bufs_work, transport_free_bufs_work);
	spin_lock_init(&priv->rx_alloc_lock);
	priv->rx_alloc_extra = 0;
	INIT_LIST_HEAD(&priv->rx_freelist);

	setup_timer(&priv->rx_retry_timer, transport_rx_retry_timer,
			(unsigned long)priv);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,8,0)
	set_timer_slack(&priv->rx_retry_timer, HZ);
#endif

	/* Keep RX disabled until the core service is ready. */
	tasklet_disable(&priv->rx_tasklet);

	ret = transport_axon_probe_virqs(priv);
	if (ret < 0)
		goto err_probe_virqs;

	if (priv->notify_rx_nirqs) {
		ret = alloc_notify_info(&dev->dev, &priv->transport.notify_info,
				&priv->transport.notify_info_size,
				priv->notify_rx_nirqs);
		if (ret < 0) {
			dev_err(&dev->dev, "Alloc notify_info failed\n");
			goto err_alloc_notify;
		}
	} else {
		priv->transport.notify_info = NULL;
		priv->transport.notify_info_size = 0;
	}

	priv->free_bufs_pool = transport_axon_init_tx_pool(priv, priv->msg_size,
			FREE_BUFS_QUOTA);
	if (IS_ERR(priv->free_bufs_pool)) {
		ret = PTR_ERR(priv->free_bufs_pool);
		goto err_init_free_bufs_pool;
	}

	ret = transport_axon_setup_descs(priv);
	if (ret < 0)
		goto err_setup_descs;

	/* Allocate RX buffers for free bufs messages */
	for (i = 0; i < FREE_BUFS_QUOTA; i++) {
		dma_addr_t laddr;
		struct vs_axon_rx_freelist_entry *buf =
			dma_pool_alloc(priv->rx_pool, GFP_KERNEL, &laddr);
		if (!buf)
			goto err_alloc_rx_free_bufs;
		buf->laddr = laddr;

		spin_lock_irq(&priv->rx_alloc_lock);
		list_add_tail(&buf->list, &priv->rx_freelist);
		spin_unlock_irq(&priv->rx_alloc_lock);
	}

	/* Set up the session device */
	priv->session_dev = vs_session_register(&priv->transport, &dev->dev,
			priv->is_server, name);
	if (IS_ERR(priv->session_dev)) {
		ret = PTR_ERR(priv->session_dev);
		dev_err(&dev->dev, "failed to register session: %d\n", ret);
		goto err_session_register;
	}

	/*
	 * Start the core service. Note that it can't actually communicate
	 * until the initial reset completes.
	 */
	vs_session_start(priv->session_dev);

	/*
	 * Reset the transport. This will also set the Axons' segment
	 * attachments, and eventually the Axons' queue pointers (once the
	 * session marks the transport ready).
	 */
	transport_reset(&priv->transport);

	/*
	 * We're ready to start handling IRQs at this point, so register the
	 * handlers.
	 */
	ret = transport_axon_request_irqs(priv);
	if (ret < 0)
		goto err_irq_register;

	return 0;

err_irq_register:
	vs_session_unregister(priv->session_dev);
err_session_register:
err_alloc_rx_free_bufs:
	transport_axon_free_descs(priv);
err_setup_descs:
	transport_axon_put_tx_pool(priv->free_bufs_pool);
err_init_free_bufs_pool:
	if (priv->transport.notify_info)
		devm_kfree(&dev->dev, priv->transport.notify_info);
err_alloc_notify:
err_probe_virqs:
	del_timer_sync(&priv->rx_retry_timer);
	tasklet_kill(&priv->rx_tasklet);
	tasklet_kill(&priv->tx_tasklet);
	cancel_delayed_work_sync(&priv->free_bufs_work);
error_of_node:
	devm_kfree(&dev->dev, priv);
err_alloc_priv:
	return ret;
}

static int transport_axon_remove(struct platform_device *dev)
{
	struct vs_transport_axon *priv = dev_get_drvdata(&dev->dev);
	int i;

	for (i = 0; i < priv->notify_rx_nirqs; i++)
		devm_free_irq(&dev->dev, priv->notify_irq[i], priv);

	devm_free_irq(&dev->dev, priv->rx_irq, priv);
	irq_dispose_mapping(priv->rx_irq);
	devm_free_irq(&dev->dev, priv->tx_irq, priv);
	irq_dispose_mapping(priv->tx_irq);
	devm_free_irq(&dev->dev, priv->reset_irq, priv);
	irq_dispose_mapping(priv->reset_irq);

	del_timer_sync(&priv->rx_retry_timer);
	tasklet_kill(&priv->rx_tasklet);
	tasklet_kill(&priv->tx_tasklet);
	cancel_delayed_work_sync(&priv->free_bufs_work);

	priv->readiness = VS_TRANSPORT_SHUTDOWN;
	vs_session_unregister(priv->session_dev);
	WARN_ON(priv->readiness != VS_TRANSPORT_SHUTDOWN);

	transport_axon_free_descs(priv);
	transport_axon_put_tx_pool(priv->free_bufs_pool);

	if (priv->transport.notify_info)
		devm_kfree(&dev->dev, priv->transport.notify_info);

	free_tx_mbufs(priv);

	flush_workqueue(work_queue);

	while (!list_empty(&priv->rx_freelist)) {
		struct vs_axon_rx_freelist_entry *buf;
		buf = list_first_entry(&priv->rx_freelist,
				struct vs_axon_rx_freelist_entry, list);
		list_del(&buf->list);
		dma_pool_free(priv->rx_pool, buf, buf->laddr);
	}

	devm_kfree(&dev->dev, priv);
	return 0;
}

static const struct of_device_id transport_axon_of_match[] = {
	{ .compatible = "okl,microvisor-axon-transport", },
	{},
};
MODULE_DEVICE_TABLE(of, transport_axon_of_match);

static struct platform_driver transport_axon_driver = {
	.probe		= transport_axon_probe,
	.remove		= transport_axon_remove,
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.bus		= &platform_bus_type,
		.of_match_table = of_match_ptr(transport_axon_of_match),
	},
};

static int __init vs_transport_axon_init(void)
{
	int ret;
	okl4_error_t err;
	struct device_node *cpus;
	struct zone *zone;
	struct _okl4_sys_mmu_lookup_pn_return lookup_return;
	u32 last_seen_attachment = -1;
	bool first_attachment;

	printk(KERN_INFO "Virtual Services transport driver for OKL4 Axons\n");

	/* Allocate the Axon cleanup workqueue */
	work_queue = alloc_workqueue("axon_cleanup", 0, 0);
	if (!work_queue) {
		ret = -ENOMEM;
		goto fail_create_workqueue;
	}

	/* Locate the MMU capability, needed for lookups */
	cpus = of_find_node_by_path("/cpus");
	if (IS_ERR_OR_NULL(cpus)) {
		ret = -EINVAL;
		goto fail_mmu_cap;
	}
	ret = of_property_read_u32(cpus, "okl,vmmu-capability", &okl4_mmu_cap);
	if (ret) {
		goto fail_mmu_cap;
	}
	if (okl4_mmu_cap == OKL4_KCAP_INVALID) {
		printk(KERN_ERR "%s: OKL4 MMU capability not found\n", __func__);
		ret = -EPERM;
		goto fail_mmu_cap;
	}

	/*
	 * Determine whether there are multiple OKL4 physical memory segments
	 * in this Cell. If so, every transport device must have a valid CMA
	 * region, to guarantee that its buffer allocations all come from the
	 * segment that is attached to the axon endpoints.
	 *
	 * We assume that each zone is contiguously mapped in stage 2 with a
	 * constant physical-to-IPA offset, typically 0. The weaver won't
	 * violate this assumption for Linux (or other HLOS) guests unless it
	 * is explicitly told to.
	 */
	okl4_single_physical_segment = true;
	first_attachment = true;
	for_each_zone(zone) {
		u32 attachment;

		/* We only care about zones that the page allocator is using */
		if (!zone->managed_pages)
			continue;

		/* Find the segment at the start of the zone */
		lookup_return = _okl4_sys_mmu_lookup_pn(okl4_mmu_cap,
				zone->zone_start_pfn, -1);
		err = okl4_mmu_lookup_index_geterror(
				&lookup_return.segment_index);
		if (err != OKL4_OK) {
			printk(KERN_WARNING "%s: Unable to determine physical segment count, assuming >1\n",
					__func__);
			okl4_single_physical_segment = false;
			break;
		}
		attachment = okl4_mmu_lookup_index_getindex(
				&lookup_return.segment_index);

		if (first_attachment) {
			last_seen_attachment = attachment;
			first_attachment = false;
		} else if (last_seen_attachment != attachment) {
			okl4_single_physical_segment = false;
			break;
		}

		/* Find the segment at the end of the zone */
		lookup_return = _okl4_sys_mmu_lookup_pn(okl4_mmu_cap,
				zone_end_pfn(zone) - 1, -1);
		err = okl4_mmu_lookup_index_geterror(
				&lookup_return.segment_index);
		if (err != OKL4_OK) {
			printk(KERN_WARNING "%s: Unable to determine physical segment count, assuming >1\n",
					__func__);
			okl4_single_physical_segment = false;
			break;
		}
		attachment = okl4_mmu_lookup_index_getindex(
				&lookup_return.segment_index);

		/* Check that it's still the same segment */
		if (last_seen_attachment != attachment) {
			okl4_single_physical_segment = false;
			break;
		}
	}

#ifdef DEBUG
	printk(KERN_DEBUG "%s: physical segment count %s\n", __func__,
			okl4_single_physical_segment ? "1" : ">1");
#endif

	mbuf_cache = KMEM_CACHE(vs_mbuf_axon, 0UL);
	if (!mbuf_cache) {
		ret = -ENOMEM;
		goto kmem_cache_failed;
	}

	ret = platform_driver_register(&transport_axon_driver);
	if (ret)
		goto register_plat_driver_failed;

	return ret;

register_plat_driver_failed:
	kmem_cache_destroy(mbuf_cache);
	mbuf_cache = NULL;
kmem_cache_failed:
fail_mmu_cap:
	if (work_queue)
		destroy_workqueue(work_queue);
fail_create_workqueue:
	return ret;
}

static void __exit vs_transport_axon_exit(void)
{
	platform_driver_unregister(&transport_axon_driver);

	rcu_barrier();

	if (mbuf_cache)
		kmem_cache_destroy(mbuf_cache);
	mbuf_cache = NULL;

	if (work_queue)
		destroy_workqueue(work_queue);
}

module_init(vs_transport_axon_init);
module_exit(vs_transport_axon_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
