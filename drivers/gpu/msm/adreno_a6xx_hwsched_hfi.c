// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/iommu.h>
#include <linux/sched/clock.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_hwsched.h"
#include "adreno_hwsched.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"
#include "kgsl_device.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_trace.h"

#define HFI_QUEUE_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_MAX_CNT)

/* Use a kmem cache to speed up allocations for f2h packets */
static struct kmem_cache *f2h_cache;

#define DEFINE_QHDR(gmuaddr, id, prio) \
	{\
		.status = 1, \
		.start_addr = GMU_QUEUE_START_ADDR(gmuaddr, id), \
		.type = QUEUE_HDR_TYPE(id, prio, 0, 0), \
		.queue_size = SZ_4K >> 2, \
		.msg_size = 0, \
		.unused0 = 0, \
		.unused1 = 0, \
		.unused2 = 0, \
		.unused3 = 0, \
		.unused4 = 0, \
		.read_index = 0, \
		.write_index = 0, \
}

static struct dq_info {
	/** @max_dq: Maximum number of dispatch queues per RB level */
	u32 max_dq;
	/** @base_dq_id: Base dqid for level */
	u32 base_dq_id;
	/** @offset: Next dqid to use for roundrobin context assignment */
	u32 offset;
} a6xx_hfi_dqs[KGSL_PRIORITY_MAX_RB_LEVELS] = {
	{ 4, 0, }, /* RB0 */
	{ 4, 4, }, /* RB1 */
	{ 3, 8, }, /* RB2 */
	{ 3, 11, }, /* RB3 */
};

static const char * const memkind_strings[] = {
	[MEMKIND_GENERIC] = "GMU GENERIC",
	[MEMKIND_RB] =  "GMU RB",
	[MEMKIND_SCRATCH] = "GMU SCRATCH",
	[MEMKIND_CSW_SMMU_INFO] = "GMU SMMU INFO",
	[MEMKIND_CSW_PRIV_NON_SECURE] = "GMU CSW PRIV NON SECURE",
	[MEMKIND_CSW_PRIV_SECURE] = "GMU CSW PRIV SECURE",
	[MEMKIND_CSW_NON_PRIV] = "GMU CSW NON PRIV",
	[MEMKIND_CSW_COUNTER] = "GMU CSW COUNTER",
	[MEMKIND_CTXTREC_PREEMPT_CNTR] = "GMU PREEMPT CNTR",
	[MEMKIND_SYS_LOG] = "GMU SYS LOG",
	[MEMKIND_CRASH_DUMP] = "GMU CRASHDUMP",
	[MEMKIND_MMIO_DPU] =  "GMU MMIO DPU",
	[MEMKIND_MMIO_TCSR] = "GMU MMIO TCSR",
	[MEMKIND_MMIO_QDSS_STM] = "GMU MMIO QDSS STM",
	[MEMKIND_PROFILE] = "GMU KERNEL PROFILING",
	[MEMKIND_USER_PROFILE_IBS] = "GMU USER PROFILING",
};

struct a6xx_hwsched_hfi *to_a6xx_hwsched_hfi(
	struct adreno_device *adreno_dev)
{
	struct a6xx_device *a6xx_dev = container_of(adreno_dev,
					struct a6xx_device, adreno_dev);
	struct a6xx_hwsched_device *a6xx_hwsched = container_of(a6xx_dev,
					struct a6xx_hwsched_device, a6xx_dev);

	return &a6xx_hwsched->hwsched_hfi;
}

static void add_waiter(struct a6xx_hwsched_hfi *hfi, u32 hdr,
	struct pending_cmd *ack)
{
	memset(ack, 0x0, sizeof(*ack));

	init_completion(&ack->complete);
	write_lock_irq(&hfi->msglock);
	list_add_tail(&ack->node, &hfi->msglist);
	write_unlock_irq(&hfi->msglock);

	ack->sent_hdr = hdr;
}

static void del_waiter(struct a6xx_hwsched_hfi *hfi, struct pending_cmd *ack)
{
	write_lock_irq(&hfi->msglock);
	list_del(&ack->node);
	write_unlock_irq(&hfi->msglock);
}

static void a6xx_receive_ack_async(struct adreno_device *adreno_dev, void *rcvd)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct pending_cmd *cmd = NULL;
	u32 waiters[64], num_waiters = 0, i;
	u32 *ack = rcvd;
	u32 hdr = ack[0];
	u32 req_hdr = ack[1];
	u32 size_bytes = MSG_HDR_GET_SIZE(hdr) << 2;

	trace_kgsl_hfi_receive(MSG_HDR_GET_ID(req_hdr),
		MSG_HDR_GET_SIZE(req_hdr), MSG_HDR_GET_SEQNUM(req_hdr));

	if (size_bytes > sizeof(cmd->results))
		dev_err_ratelimited(&gmu->pdev->dev,
			"Ack result too big: %d Truncating to: %d\n",
			size_bytes, sizeof(cmd->results));

	read_lock(&hfi->msglock);

	list_for_each_entry(cmd, &hfi->msglist, node) {
		if (HDR_CMP_SEQNUM(cmd->sent_hdr, req_hdr)) {
			memcpy(cmd->results, ack,
				min_t(u32, size_bytes,
					sizeof(cmd->results)));
			complete(&cmd->complete);
			read_unlock(&hfi->msglock);
			return;
		}

		if (num_waiters < ARRAY_SIZE(waiters))
			waiters[num_waiters++] = cmd->sent_hdr;
	}

	read_unlock(&hfi->msglock);

	/* Didn't find the sender, list the waiter */
	dev_err_ratelimited(&gmu->pdev->dev,
		"Unexpectedly got id %d seqnum %d. Total waiters: %d Top %d Waiters:\n",
		MSG_HDR_GET_ID(req_hdr), MSG_HDR_GET_SEQNUM(req_hdr),
		num_waiters, min_t(u32, num_waiters, 5));

	for (i = 0; i < num_waiters && i < 5; i++)
		dev_err_ratelimited(&gmu->pdev->dev,
			" id %d seqnum %d\n",
			MSG_HDR_GET_ID(waiters[i]),
			MSG_HDR_GET_SEQNUM(waiters[i]));
}

static void log_profiling_info(struct adreno_device *adreno_dev, u32 *rcvd)
{
	struct hfi_ts_retire_cmd *cmd = (struct hfi_ts_retire_cmd *)rcvd;
	struct kgsl_context *context;
	struct retire_info info = {0};

	context = kgsl_context_get(KGSL_DEVICE(adreno_dev), cmd->ctxt_id);
	if (context == NULL)
		return;

	info.timestamp = cmd->ts;
	info.rb_id = adreno_get_level(context->priority);
	info.gmu_dispatch_queue = context->gmu_dispatch_queue;
	info.submitted_to_rb = cmd->submitted_to_rb;
	info.sop = cmd->sop;
	info.eop = cmd->eop;
	info.retired_on_gmu = cmd->retired_on_gmu;

	trace_adreno_cmdbatch_retired(context, &info, 0, 0, 0);
	kgsl_context_put(context);
}

struct f2h_packet {
	/** @rcvd: the contents of the fw to host packet */
	u32 rcvd[MAX_RCVD_SIZE];
	/** @node: To add to the fw to host msg list */
	struct llist_node node;
};

static void add_f2h_packet(struct adreno_device *adreno_dev, u32 *msg)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct f2h_packet *pkt = kmem_cache_alloc(f2h_cache, GFP_ATOMIC);
	u32 size = MSG_HDR_GET_SIZE(msg[0]) << 2;

	if (!pkt)
		return;

	if (size > sizeof(pkt->rcvd))
		dev_err_ratelimited(&gmu->pdev->dev,
			"f2h packet too big: %d allowed: %d\n",
			size, sizeof(pkt->rcvd));

	memcpy(pkt->rcvd, msg, min_t(u32, size, sizeof(pkt->rcvd)));

	llist_add(&pkt->node, &hfi->f2h_msglist);
}

static void process_msgq_irq(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 rcvd[MAX_RCVD_SIZE];
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);

	while (a6xx_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd)) > 0) {

		/*
		 * We are assuming that there is only one outstanding ack
		 * because hfi sending thread waits for completion while
		 * holding the device mutex
		 */
		if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
			a6xx_receive_ack_async(adreno_dev, rcvd);
		} else {
			add_f2h_packet(adreno_dev, rcvd);
			wake_up_interruptible(&hfi->f2h_wq);
		}
	}
}

static void adreno_a6xx_add_log_block(struct adreno_device *adreno_dev, u32 *msg)
{
	struct f2h_packet *pkt = kmem_cache_alloc(f2h_cache, GFP_ATOMIC);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	u32 size = MSG_HDR_GET_SIZE(msg[0]) << 2;

	if (!pkt)
		return;

	memcpy(pkt->rcvd, msg, min_t(u32, size, sizeof(pkt->rcvd)));

	/*
	 * Add the log block packets from GMU to a secondary list to ensure
	 * the time critical TS_RETIRE packet processing on the primary list
	 * is not delayed
	 */
	llist_add(&pkt->node, &hfi->f2h_secondary_list);

	wake_up_interruptible(&hfi->f2h_wq);
}

static void process_dbgq_irq(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 rcvd[MAX_RCVD_SIZE];
	bool recovery = false;

	while (a6xx_hfi_queue_read(gmu, HFI_DBG_ID, rcvd, sizeof(rcvd)) > 0) {

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_ERR) {
			adreno_a6xx_receive_err_req(gmu, rcvd);
			recovery = true;
		}

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_DEBUG)
			adreno_a6xx_receive_debug_req(gmu, rcvd);

		if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_LOG_BLOCK)
			adreno_a6xx_add_log_block(adreno_dev, rcvd);
	}

	if (!recovery)
		return;

	adreno_get_gpu_halt(adreno_dev);
	adreno_hwsched_set_fault(adreno_dev);
}

/* HFI interrupt handler */
static irqreturn_t a6xx_hwsched_hfi_handler(int irq, void *data)
{
	struct adreno_device *adreno_dev = data;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status = 0;

	gmu_core_regread(device, A6XX_GMU_GMU2HOST_INTR_INFO, &status);
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, hfi->irq_mask);

	/*
	 * If interrupts are not enabled on the HFI message queue,
	 * the inline message processing loop will process it,
	 * else, process it here.
	 */
	if (!(hfi->irq_mask & HFI_IRQ_MSGQ_MASK))
		status &= ~HFI_IRQ_MSGQ_MASK;

	if (status & HFI_IRQ_MSGQ_MASK)
		process_msgq_irq(adreno_dev);
	if (status & HFI_IRQ_DBGQ_MASK)
		process_dbgq_irq(adreno_dev);
	if (status & HFI_IRQ_CM3_FAULT_MASK) {
		atomic_set(&gmu->cm3_fault, 1);

		/* make sure other CPUs see the update */
		smp_wmb();

		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU CM3 fault interrupt received\n");

		adreno_get_gpu_halt(adreno_dev);
		adreno_hwsched_set_fault(adreno_dev);
	}

	/* Ignore OOB bits */
	status &= GENMASK(31 - (oob_max - 1), 0);

	if (status & ~hfi->irq_mask)
		dev_err_ratelimited(&gmu->pdev->dev,
			"Unhandled HFI interrupts 0x%lx\n",
			status & ~hfi->irq_mask);

	return IRQ_HANDLED;
}

#define HFI_IRQ_MSGQ_MASK BIT(0)
#define HFI_RSP_TIMEOUT   100 /* msec */

static int wait_ack_completion(struct adreno_device *adreno_dev,
		struct pending_cmd *ack)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int rc;

	rc = wait_for_completion_timeout(&ack->complete,
		HFI_RSP_TIMEOUT);
	if (!rc) {
		dev_err(&gmu->pdev->dev,
			"Ack timeout for id:%d sequence=%d\n",
			MSG_HDR_GET_ID(ack->sent_hdr),
			MSG_HDR_GET_SEQNUM(ack->sent_hdr));
		gmu_fault_snapshot(KGSL_DEVICE(adreno_dev));
		return -ETIMEDOUT;
	}

	return 0;
}

static int check_ack_failure(struct adreno_device *adreno_dev,
	struct pending_cmd *ack)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (ack->results[2] != 0xffffffff)
		return 0;

	dev_err(&gmu->pdev->dev,
		"ACK error: sender id %d seqnum %d\n",
		MSG_HDR_GET_ID(ack->sent_hdr),
		MSG_HDR_GET_SEQNUM(ack->sent_hdr));

	return -EINVAL;
}

int a6xx_hfi_send_cmd_async(struct adreno_device *adreno_dev, void *data)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	u32 *cmd = data;
	u32 seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	int rc;
	struct pending_cmd pending_ack;

	*cmd = MSG_HDR_SET_SEQNUM(*cmd, seqnum);

	add_waiter(hfi, *cmd, &pending_ack);

	rc = a6xx_hfi_cmdq_write(adreno_dev, cmd);
	if (rc)
		goto done;

	rc = wait_ack_completion(adreno_dev, &pending_ack);
	if (rc)
		goto done;

	rc = check_ack_failure(adreno_dev, &pending_ack);

done:
	del_waiter(hfi, &pending_ack);

	return rc;
}

static void init_queues(struct a6xx_hfi *hfi)
{
	u32 gmuaddr = hfi->hfi_mem->gmuaddr;
	struct hfi_queue_table hfi_table = {
		.qtbl_hdr = {
			.version = 0,
			.size = sizeof(struct hfi_queue_table) >> 2,
			.qhdr0_offset =
				sizeof(struct hfi_queue_table_header) >> 2,
			.qhdr_size = sizeof(struct hfi_queue_header) >> 2,
			.num_q = HFI_QUEUE_MAX,
			.num_active_q = HFI_QUEUE_MAX,
		},
		.qhdr = {
			DEFINE_QHDR(gmuaddr, HFI_CMD_ID, 0),
			DEFINE_QHDR(gmuaddr, HFI_MSG_ID, 0),
			DEFINE_QHDR(gmuaddr, HFI_DBG_ID, 0),
			/* 4 DQs for RB priority 0 */
			DEFINE_QHDR(gmuaddr, 3, 0),
			DEFINE_QHDR(gmuaddr, 4, 0),
			DEFINE_QHDR(gmuaddr, 5, 0),
			DEFINE_QHDR(gmuaddr, 6, 0),
			/* 4 DQs for RB priority 1 */
			DEFINE_QHDR(gmuaddr, 7, 1),
			DEFINE_QHDR(gmuaddr, 8, 1),
			DEFINE_QHDR(gmuaddr, 9, 1),
			DEFINE_QHDR(gmuaddr, 10, 1),
			/* 3 DQs for RB priority 2 */
			DEFINE_QHDR(gmuaddr, 11, 2),
			DEFINE_QHDR(gmuaddr, 12, 2),
			DEFINE_QHDR(gmuaddr, 13, 2),
			/* 3 DQs for RB priority 3 */
			DEFINE_QHDR(gmuaddr, 14, 3),
			DEFINE_QHDR(gmuaddr, 15, 3),
			DEFINE_QHDR(gmuaddr, 16, 3),
		},
	};

	memcpy(hfi->hfi_mem->hostptr, &hfi_table, sizeof(hfi_table));
}

/* Total header sizes + queue sizes + 16 for alignment */
#define HFIMEM_SIZE (sizeof(struct hfi_queue_table) + 16 + \
	(SZ_4K * HFI_QUEUE_MAX))

int a6xx_hwsched_hfi_init(struct adreno_device *adreno_dev)
{
	struct a6xx_hfi *hfi = to_a6xx_hfi(adreno_dev);

	if (IS_ERR_OR_NULL(hfi->hfi_mem)) {
		hfi->hfi_mem = reserve_gmu_kernel_block(to_a6xx_gmu(adreno_dev),
				0, HFIMEM_SIZE, GMU_NONCACHED_KERNEL);
		if (!IS_ERR(hfi->hfi_mem))
			init_queues(hfi);
	}

	return PTR_ERR_OR_ZERO(hfi->hfi_mem);
}

static int get_attrs(u32 flags)
{
	int attrs = IOMMU_READ;

	if (flags & MEMFLAG_GMU_PRIV)
		attrs |= IOMMU_PRIV;

	if (flags & MEMFLAG_GMU_WRITEABLE)
		attrs |= IOMMU_WRITE;

	return attrs;
}

static int gmu_import_buffer(struct adreno_device *adreno_dev,
	struct mem_alloc_entry *entry, u32 flags)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int attrs = get_attrs(flags);
	struct sg_table *sgt;
	struct kgsl_memdesc *gpu_md = entry->gpu_md;
	int mapped = 0;
	struct gmu_vma_entry *vma = &gmu->vma[GMU_NONCACHED_KERNEL];
	struct hfi_mem_alloc_desc *desc = &entry->desc;

	if (flags & MEMFLAG_GMU_CACHEABLE)
		vma = &gmu->vma[GMU_CACHE];

	if ((vma->next_va + desc->size) > (vma->start + vma->size)) {
		dev_err(&gmu->pdev->dev,
			"GMU mapping too big. available: %d required: %d\n",
			vma->next_va - vma->start, desc->size);
		return -ENOMEM;
	}

	/* Alloc sgt for map and then free it */
	if (gpu_md->pages != NULL)
		sgt = kgsl_alloc_sgt_from_pages(gpu_md);
	else
		sgt = gpu_md->sgt;

	if (IS_ERR(sgt))
		return -ENOMEM;

	desc->gmu_addr = vma->next_va;

	mapped = iommu_map_sg(gmu->domain,
			desc->gmu_addr, sgt->sgl, sgt->nents, attrs);
	if (mapped == 0)
		dev_err(&gmu->pdev->dev, "gmu map sg err: 0x%08x, %d, %x, %zd\n",
			desc->gmu_addr, sgt->nents, attrs, mapped);
	else
		vma->next_va += desc->size;

	if (gpu_md->pages != NULL)
		kgsl_free_sgt(sgt);

	return ((mapped == 0) ? -ENOMEM : 0);
}

static struct mem_alloc_entry *lookup_mem_alloc_table(
	struct adreno_device *adreno_dev, struct hfi_mem_alloc_desc *desc)
{
	struct a6xx_hwsched_hfi *hw_hfi = to_a6xx_hwsched_hfi(adreno_dev);
	int i;

	for (i = 0; i < hw_hfi->mem_alloc_entries; i++) {
		struct mem_alloc_entry *entry = &hw_hfi->mem_alloc_table[i];

		if ((entry->desc.mem_kind == desc->mem_kind) &&
		(entry->desc.gmu_mem_handle == desc->gmu_mem_handle) &&
			entry->gpu_md->gpuaddr)
			return entry;
	}

	return NULL;
}

static struct mem_alloc_entry *get_mem_alloc_entry(
	struct adreno_device *adreno_dev, struct hfi_mem_alloc_desc *desc)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct mem_alloc_entry *entry =
		lookup_mem_alloc_table(adreno_dev, desc);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u64 flags = 0;
	u32 priv = 0;
	int ret;
	const char *memkind_string = desc->mem_kind < NUM_HFI_MEMKINDS ?
			memkind_strings[desc->mem_kind] : "UNKNOWN";

	if (entry)
		return entry;

	if (hfi->mem_alloc_entries == ARRAY_SIZE(hfi->mem_alloc_table)) {
		dev_err(&gmu->pdev->dev,
			"Reached max mem alloc entries\n");
		return ERR_PTR(-ENOMEM);
	}

	entry = &hfi->mem_alloc_table[hfi->mem_alloc_entries];

	memcpy(&entry->desc, desc, sizeof(*desc));

	entry->desc.host_mem_handle = desc->gmu_mem_handle;

	if (desc->flags & MEMFLAG_GFX_PRIV)
		priv |= KGSL_MEMDESC_PRIVILEGED;

	if (!(desc->flags & MEMFLAG_GFX_WRITEABLE))
		flags |= KGSL_MEMFLAGS_GPUREADONLY;

	if (desc->flags & MEMFLAG_GFX_SECURE)
		flags |= KGSL_MEMFLAGS_SECURE;

	entry->gpu_md = kgsl_allocate_global(device, desc->size, 0, flags, priv,
		memkind_string);
	if (IS_ERR(entry->gpu_md)) {
		int ret = PTR_ERR(entry->gpu_md);

		memset(entry, 0, sizeof(*entry));
		return ERR_PTR(ret);
	}

	entry->desc.size = entry->gpu_md->size;

	 /*
	  * Map all buffers in GMU. If this fails, then we have to live with
	  * leaking the gpu global buffer allocated above.
	  */
	ret = gmu_import_buffer(adreno_dev, entry, desc->flags);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"gpuaddr: 0x%llx size: %zd bytes lost\n",
			entry->gpu_md->gpuaddr, entry->gpu_md->size);
		memset(entry, 0, sizeof(*entry));
		return ERR_PTR(ret);
	}

	hfi->mem_alloc_entries++;

	return entry;
}

static int process_mem_alloc(struct adreno_device *adreno_dev,
	struct hfi_mem_alloc_desc *mad)
{
	struct mem_alloc_entry *entry;

	entry = get_mem_alloc_entry(adreno_dev, mad);
	if (IS_ERR(entry))
		return PTR_ERR(entry);

	mad->gpu_addr = entry->gpu_md->gpuaddr;
	mad->gmu_addr = entry->desc.gmu_addr;

	/*
	 * GMU uses the host_mem_handle to check if this memalloc was
	 * successful
	 */
	mad->host_mem_handle = mad->gmu_mem_handle;

	return 0;
}

static int mem_alloc_reply(struct adreno_device *adreno_dev, void *rcvd)
{
	struct hfi_mem_alloc_cmd *in = (struct hfi_mem_alloc_cmd *)rcvd;
	struct hfi_mem_alloc_reply_cmd out = {0};
	int ret;

	ret = process_mem_alloc(adreno_dev, &in->desc);
	if (ret)
		return ret;

	memcpy(&out.desc, &in->desc, sizeof(out.desc));

	out.hdr = ACK_MSG_HDR(F2H_MSG_MEM_ALLOC, sizeof(out));
	out.req_hdr = in->hdr;

	return a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&out);
}

static int send_start_msg(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	int rc = 0;
	struct hfi_start_cmd cmd;
	u32 rcvd[MAX_RCVD_SIZE];
	struct pending_cmd pending_ack = {0};

	CMD_MSG_HDR(cmd, H2F_MSG_START);

	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr, seqnum);

	pending_ack.sent_hdr = cmd.hdr;

	rc = a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&cmd);
	if (rc)
		return rc;

poll:
	rc = timed_poll_check(device, A6XX_GMU_GMU2HOST_INTR_INFO,
		HFI_IRQ_MSGQ_MASK, HFI_RSP_TIMEOUT, HFI_IRQ_MSGQ_MASK);

	if (rc) {
		dev_err(&gmu->pdev->dev,
			"Timed out processing MSG_START seqnum: %d\n",
			seqnum);
		gmu_fault_snapshot(device);
		return rc;
	}

	/* Clear the interrupt */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR,
		HFI_IRQ_MSGQ_MASK);

	if (a6xx_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd)) <= 0) {
		dev_err(&gmu->pdev->dev, "MSG_START: no payload\n");
		gmu_fault_snapshot(device);
		return -EINVAL;
	}

	if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK) {
		rc = a6xx_receive_ack_cmd(gmu, rcvd, &pending_ack);
		if (rc)
			return rc;

		return check_ack_failure(adreno_dev, &pending_ack);
	}

	if (MSG_HDR_GET_ID(rcvd[0]) == F2H_MSG_MEM_ALLOC) {
		rc = mem_alloc_reply(adreno_dev, rcvd);
		if (rc)
			return rc;

		goto poll;
	}

	dev_err(&gmu->pdev->dev,
		"MSG_START: unexpected response id:%d, type:%d\n",
		MSG_HDR_GET_ID(rcvd[0]),
		MSG_HDR_GET_TYPE(rcvd[0]));

	gmu_fault_snapshot(device);

	return rc;
}

static void reset_hfi_queues(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_queue_table *tbl = gmu->hfi.hfi_mem->hostptr;
	u32 i;

	/* Flush HFI queues */
	for (i = 0; i < HFI_QUEUE_MAX; i++) {
		struct hfi_queue_header *hdr = &tbl->qhdr[i];

		if (hdr->status == HFI_QUEUE_STATUS_DISABLED)
			continue;

		if (hdr->read_index != hdr->write_index) {
			dev_err(&gmu->pdev->dev,
			"HFI queue[%d] is not empty before close: rd=%d,wt=%d\n",
				i, hdr->read_index, hdr->write_index);
			hdr->read_index = hdr->write_index;

			gmu_fault_snapshot(KGSL_DEVICE(adreno_dev));
		}
	}
}

void a6xx_hwsched_hfi_stop(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);

	hfi->irq_mask &= ~HFI_IRQ_MSGQ_MASK;

	reset_hfi_queues(adreno_dev);

	kgsl_pwrctrl_axi(KGSL_DEVICE(adreno_dev), KGSL_PWRFLAGS_OFF);

	clear_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

}

static void enable_async_hfi(struct adreno_device *adreno_dev)
{
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);

	hfi->irq_mask |= HFI_IRQ_MSGQ_MASK;

	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), A6XX_GMU_GMU2HOST_INTR_MASK,
		(u32)~hfi->irq_mask);
}

static int enable_preemption(struct adreno_device *adreno_dev)
{
	u32 data;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	/*
	 * Bits [0:1] contains the preemption level
	 * Bit 2 is to enable/disable gmem save/restore
	 * Bit 3 is to enable/disable skipsaverestore
	 */
	data = FIELD_PREP(GENMASK(1, 0), adreno_dev->preempt.preempt_level) |
			FIELD_PREP(BIT(2), adreno_dev->preempt.usesgmem) |
			FIELD_PREP(BIT(3), adreno_dev->preempt.skipsaverestore);

	return a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_PREEMPTION, 1,
			data);
}

int a6xx_hwsched_hfi_start(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	ret = a6xx_gmu_hfi_start(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_generic_req(adreno_dev, &gmu->hfi.dcvs_table);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_generic_req(adreno_dev, &gmu->hfi.bw_table);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_acd_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_lm_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_bcl_feature_ctrl(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_HWSCHED, 1, 0);
	if (ret)
		goto err;

	ret = a6xx_hfi_send_feature_ctrl(adreno_dev, HFI_FEATURE_KPROF, 1, 0);
	if (ret)
		goto err;

	ret = enable_preemption(adreno_dev);
	if (ret)
		goto err;

	if (gmu->log_stream_enable)
		a6xx_hfi_send_set_value(adreno_dev,
			HFI_VALUE_LOG_STREAM_ENABLE, 0, 1);

	ret = a6xx_hfi_send_core_fw_start(adreno_dev);
	if (ret)
		goto err;

	ret = send_start_msg(adreno_dev);
	if (ret)
		goto err;

	enable_async_hfi(adreno_dev);

	set_bit(GMU_PRIV_HFI_STARTED, &gmu->flags);

	/* Request default DCVS level */
	ret = kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
	if (ret)
		goto err;

	/* Request default BW vote */
	ret = kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);

err:
	if (ret)
		a6xx_hwsched_hfi_stop(adreno_dev);

	return ret;
}

static int submit_raw_cmds(struct adreno_device *adreno_dev, void *cmds,
	const char *str)
{
	int ret;

	ret = a6xx_hfi_send_cmd_async(adreno_dev, cmds);
	if (ret)
		return ret;

	ret = timed_poll_check(KGSL_DEVICE(adreno_dev),
			A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, 0, 200, BIT(23));
	if (ret)
		a6xx_spin_idle_debug(adreno_dev, str);

	return ret;
}

static int cp_init(struct adreno_device *adreno_dev)
{
	u32 cmds[A6XX_CP_INIT_DWORDS + 1];

	cmds[0] = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW,
		(A6XX_CP_INIT_DWORDS + 1) << 2, HFI_MSG_CMD);

	memcpy(&cmds[1], adreno_dev->cp_init_cmds, A6XX_CP_INIT_DWORDS << 2);

	return submit_raw_cmds(adreno_dev, cmds,
			"CP initialization failed to idle\n");
}

static int send_switch_to_unsecure(struct adreno_device *adreno_dev)
{
	u32 cmds[3];

	cmds[0] = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW, sizeof(cmds),
			HFI_MSG_CMD);

	cmds[1] = cp_type7_packet(CP_SET_SECURE_MODE, 1);
	cmds[2] = 0;

	return  submit_raw_cmds(adreno_dev, cmds,
			"Switch to unsecure failed to idle\n");
}

int a6xx_hwsched_cp_init(struct adreno_device *adreno_dev)
{
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	int ret;

	a6xx_unhalt_sqe(adreno_dev);

	ret = cp_init(adreno_dev);
	if (ret)
		return ret;

	ret = adreno_zap_shader_load(adreno_dev, a6xx_core->zap_name);
	if (ret)
		return ret;

	if (!adreno_dev->zap_loaded)
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			A6XX_RBBM_SECVID_TRUST_CNTL, 0x0);
	else
		ret = send_switch_to_unsecure(adreno_dev);

	return ret;
}

static void process_ts_retire(struct adreno_device *adreno_dev, u32 *rcvd)
{
	log_profiling_info(adreno_dev, rcvd);
	adreno_hwsched_trigger(adreno_dev);
}

static void process_ctx_bad(struct adreno_device *adreno_dev, void *rcvd)
{
	struct hfi_context_bad_cmd *cmd = rcvd;

	/* Block dispatcher to submit more commands */
	adreno_get_gpu_halt(adreno_dev);

	adreno_hwsched_mark_drawobj(adreno_dev, cmd->ctxt_id, cmd->ts);
}

static void process_log_block(struct adreno_device *adreno_dev, void *data)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_log_block *cmd = data;
	u32 *log_event = gmu->gmu_log->hostptr;
	u32 start, end;

	start = cmd->start_index;
	end = cmd->stop_index;

	log_event += start * 4;
	while (start != end) {
		trace_gmu_event(log_event);
		log_event += 4;
		start++;
	}
}

static int hfi_f2h_main(void *arg)
{
	struct adreno_device *adreno_dev = arg;
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct llist_node *list;
	struct f2h_packet *pkt, *tmp;

	while (!kthread_should_stop()) {
		wait_event_interruptible(hfi->f2h_wq,
			((!llist_empty(&hfi->f2h_msglist) ||
			  !llist_empty(&hfi->f2h_secondary_list))
			 && !kthread_should_stop()));

		if (kthread_should_stop())
			break;

		list = llist_del_all(&hfi->f2h_msglist);

		list = llist_reverse_order(list);

		llist_for_each_entry_safe(pkt, tmp, list, node) {
			if (MSG_HDR_GET_ID(pkt->rcvd[0]) == F2H_MSG_TS_RETIRE)
				process_ts_retire(adreno_dev, pkt->rcvd);

			if (MSG_HDR_GET_ID(pkt->rcvd[0]) == F2H_MSG_CONTEXT_BAD)
				process_ctx_bad(adreno_dev, pkt->rcvd);

			kmem_cache_free(f2h_cache, pkt);
		}

		/* Process packets on the secondary list after the primary list */
		list = llist_del_all(&hfi->f2h_secondary_list);
		list = llist_reverse_order(list);
		llist_for_each_entry_safe(pkt, tmp, list, node) {
			if (MSG_HDR_GET_ID(pkt->rcvd[0]) == F2H_MSG_LOG_BLOCK)
				process_log_block(adreno_dev, pkt->rcvd);

			kmem_cache_free(f2h_cache, pkt);
		}
	}

	return 0;
}

int a6xx_hwsched_hfi_probe(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hw_hfi = to_a6xx_hwsched_hfi(adreno_dev);

	gmu->hfi.irq = kgsl_request_irq(gmu->pdev, "kgsl_hfi_irq",
		a6xx_hwsched_hfi_handler, adreno_dev);

	if (gmu->hfi.irq < 0)
		return gmu->hfi.irq;

	hw_hfi->irq_mask = HFI_IRQ_MASK;

	disable_irq(gmu->hfi.irq);

	rwlock_init(&hw_hfi->msglock);

	INIT_LIST_HEAD(&hw_hfi->msglist);

	init_llist_head(&hw_hfi->f2h_msglist);

	init_waitqueue_head(&hw_hfi->f2h_wq);

	hw_hfi->f2h_task = kthread_run(hfi_f2h_main, adreno_dev, "gmu_f2h");

	f2h_cache = KMEM_CACHE(f2h_packet, 0);

	return 0;
}

static void add_profile_events(struct adreno_device *adreno_dev,
	struct kgsl_drawobj *drawobj, struct adreno_submit_time *time)
{
	unsigned long flags;
	u64 time_in_s;
	unsigned long time_in_ns;
	struct kgsl_context *context = drawobj->context;
	struct submission_info info = {0};

	/*
	 * Here we are attempting to create a mapping between the
	 * GPU time domain (alwayson counter) and the CPU time domain
	 * (local_clock) by sampling both values as close together as
	 * possible. This is useful for many types of debugging and
	 * profiling. In order to make this mapping as accurate as
	 * possible, we must turn off interrupts to avoid running
	 * interrupt handlers between the two samples.
	 */

	local_irq_save(flags);

	/* Read always on registers */
	time->ticks = a6xx_read_alwayson(adreno_dev);

	/* Trace the GPU time to create a mapping to ftrace time */
	trace_adreno_cmdbatch_sync(context->id, context->priority,
		drawobj->timestamp, time->ticks);

	/* Get the kernel clock for time since boot */
	time->ktime = local_clock();

	/* Get the timeofday for the wall time (for the user) */
	getnstimeofday(&time->utime);

	local_irq_restore(flags);

	/* Return kernel clock time to the client if requested */
	time_in_s = time->ktime;
	time_in_ns = do_div(time_in_s, 1000000000);

	info.inflight = -1;
	info.rb_id = adreno_get_level(context->priority);
	info.gmu_dispatch_queue = context->gmu_dispatch_queue;

	trace_adreno_cmdbatch_submitted(drawobj, &info, time->ticks,
		(unsigned long) time_in_s, time_in_ns / 1000, 0);
}

#define CTXT_FLAG_PMODE                 0x00000001
#define CTXT_FLAG_SWITCH_INTERNAL       0x00000002
#define CTXT_FLAG_SWITCH                0x00000008
#define CTXT_FLAG_NOTIFY                0x00000020
#define CTXT_FLAG_NO_FAULT_TOLERANCE    0x00000200
#define CTXT_FLAG_PWR_RULE              0x00000800
#define CTXT_FLAG_PRIORITY_MASK         0x0000f000
#define CTXT_FLAG_IFH_NOP               0x00010000
#define CTXT_FLAG_SECURE                0x00020000
#define CTXT_FLAG_TYPE_MASK             0x01f00000
#define CTXT_FLAG_TYPE_SHIFT            20
#define CTXT_FLAG_TYPE_ANY              0
#define CTXT_FLAG_TYPE_GL               1
#define CTXT_FLAG_TYPE_CL               2
#define CTXT_FLAG_TYPE_C2D              3
#define CTXT_FLAG_TYPE_RS               4
#define CTXT_FLAG_TYPE_VK               5
#define CTXT_FLAG_TYPE_UNKNOWN          0x1e
#define CTXT_FLAG_PREEMPT_STYLE_MASK    0x0e000000
#define CTXT_FLAG_PREEMPT_STYLE_SHIFT   25
#define CTXT_FLAG_PREEMPT_STYLE_ANY     0
#define CTXT_FLAG_PREEMPT_STYLE_RB      1
#define CTXT_FLAG_PREEMPT_STYLE_FG      2

static u32 get_next_dq(u32 priority)
{
	struct dq_info *info = &a6xx_hfi_dqs[priority];
	u32 next = info->base_dq_id + info->offset;

	info->offset = (info->offset + 1) % info->max_dq;

	return next;
}

static u32 get_dq_id(u32 priority)
{
	u32 level = adreno_get_level(priority);

	return get_next_dq(level);
}

static int send_context_register(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct hfi_register_ctxt_cmd cmd;
	struct kgsl_pagetable *pt = context->proc_priv->pagetable;

	CMD_MSG_HDR(cmd, H2F_MSG_REGISTER_CONTEXT);

	cmd.ctxt_id = context->id;
	cmd.flags = CTXT_FLAG_NOTIFY | context->flags;
	cmd.pt_addr = kgsl_mmu_pagetable_get_ttbr0(pt);
	cmd.ctxt_idr = kgsl_mmu_pagetable_get_contextidr(pt);
	cmd.ctxt_bank = kgsl_mmu_pagetable_get_context_bank(pt);

	return a6xx_hfi_send_cmd_async(adreno_dev, &cmd);
}

static int send_context_pointers(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_context_pointers_cmd cmd;

	CMD_MSG_HDR(cmd, H2F_MSG_CONTEXT_POINTERS);
	cmd.ctxt_id = context->id;
	cmd.sop_addr = MEMSTORE_ID_GPU_ADDR(device, context->id, soptimestamp);
	cmd.eop_addr = MEMSTORE_ID_GPU_ADDR(device, context->id, eoptimestamp);
	if (context->user_ctxt_record)
		cmd.user_ctxt_record_addr =
			context->user_ctxt_record->memdesc.gpuaddr;
	else
		cmd.user_ctxt_record_addr = 0;

	return a6xx_hfi_send_cmd_async(adreno_dev, &cmd);
}

static int hfi_context_register(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	if (context->gmu_registered)
		return 0;

	ret = send_context_register(adreno_dev, context);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to register context %d: %d\n",
			context->id, ret);
		return ret;
	}

	ret = send_context_pointers(adreno_dev, context);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to register context %d pointers: %d\n",
			context->id, ret);
		return ret;
	}

	context->gmu_registered = true;
	context->gmu_dispatch_queue = get_dq_id(context->priority);

	return 0;
}

#define HFI_DSP_IRQ_BASE 2

#define DISPQ_IRQ_BIT(_idx) BIT((_idx) + HFI_DSP_IRQ_BASE)

int a6xx_hwsched_submit_cmdobj(struct adreno_device *adreno_dev,
	struct kgsl_drawobj_cmd *cmdobj)
{
	struct a6xx_hfi *hfi = to_a6xx_hfi(adreno_dev);
	struct kgsl_memobj_node *ib;
	int ret = 0;
	u32 numibs = 0, cmd_sizebytes;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct hfi_issue_ib *issue_ib;
	struct hfi_submit_cmd *cmd;
	struct adreno_submit_time time = {0};

	ret = hfi_context_register(adreno_dev, drawobj->context);
	if (ret)
		return ret;

	/* Get the total IBs in the list */
	list_for_each_entry(ib, &cmdobj->cmdlist, node)
		numibs++;

	/* We need to dispatch a marker object but not execute it on the GPU */
	if (test_bit(CMDOBJ_SKIP, &cmdobj->priv))
		numibs = 0;

	/* Add a *issue_ib struct for each IB */
	cmd_sizebytes = sizeof(*cmd) + (sizeof(*issue_ib) * numibs);

	if (WARN_ON(cmd_sizebytes > HFI_MAX_MSG_SIZE))
		return -EMSGSIZE;

	cmd = kvmalloc(cmd_sizebytes, GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;

	cmd->hdr = CREATE_MSG_HDR(H2F_MSG_ISSUE_CMD, cmd_sizebytes,
			HFI_MSG_CMD);
	cmd->hdr = MSG_HDR_SET_SEQNUM(cmd->hdr,
			atomic_inc_return(&hfi->seqnum));

	cmd->ctxt_id = drawobj->context->id;
	cmd->flags = CTXT_FLAG_NOTIFY;
	cmd->ts = drawobj->timestamp;
	cmd->numibs = numibs;

	if (!numibs)
		goto skipib;

	if ((drawobj->flags & KGSL_DRAWOBJ_PROFILING) &&
		!cmdobj->profiling_buf_entry) {

		time.drawobj = drawobj;

		cmd->profile_gpuaddr_lo =
			lower_32_bits(cmdobj->profiling_buffer_gpuaddr);
		cmd->profile_gpuaddr_hi =
			upper_32_bits(cmdobj->profiling_buffer_gpuaddr);

		/* Indicate to GMU to do user profiling for this submission */
		cmd->flags |= BIT(4);
	}

	issue_ib = (struct hfi_issue_ib *)&cmd[1];

	list_for_each_entry(ib, &cmdobj->cmdlist, node) {
		issue_ib->addr = ib->gpuaddr;
		issue_ib->size = ib->size;
		issue_ib++;
	}

skipib:
	ret = a6xx_hfi_queue_write(adreno_dev,
		HFI_DSP_ID_0 + drawobj->context->gmu_dispatch_queue,
		(u32 *)cmd);
	if (ret)
		goto free;

	/*
	 * Memory barrier to make sure packet and write index are written before
	 * an interrupt is raised
	 */
	wmb();

	add_profile_events(adreno_dev, drawobj, &time);

	cmdobj->submit_ticks = time.ticks;

	/* Send interrupt to GMU to receive the message */
	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), A6XX_GMU_HOST2GMU_INTR_SET,
		DISPQ_IRQ_BIT(drawobj->context->gmu_dispatch_queue));

	/* Put the profiling information in the user profiling buffer */
	adreno_profile_submit_time(&time);

free:
	kvfree(cmd);

	return ret;
}

static int send_context_unregister_hfi(struct adreno_device *adreno_dev,
	struct kgsl_context *context, u32 ts)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct pending_cmd pending_ack;
	struct hfi_unregister_ctxt_cmd cmd;
	u32 seqnum;
	int rc;

	CMD_MSG_HDR(cmd, H2F_MSG_UNREGISTER_CONTEXT);
	cmd.ctxt_id = context->id,
	cmd.ts = ts,

	seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr, seqnum);

	add_waiter(hfi, cmd.hdr, &pending_ack);

	rc = a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&cmd);
	if (rc)
		goto done;

	mutex_unlock(&device->mutex);

	rc = wait_for_completion_timeout(&pending_ack.complete,
			msecs_to_jiffies(30 * 1000));
	if (!rc) {
		dev_err(&gmu->pdev->dev,
			"Ack timeout for context unregister seq: %d ctx: %d ts: %d\n",
			MSG_HDR_GET_SEQNUM(pending_ack.sent_hdr),
			context->id, ts);
		rc = -ETIMEDOUT;

		mutex_lock(&device->mutex);

		gmu_fault_snapshot(device);

		/*
		 * Trigger dispatcher based reset and recovery. Invalidate the
		 * context so that any un-finished inflight submissions are not
		 * replayed after recovery.
		 */
		adreno_mark_guilty_context(device, context->id);

		adreno_drawctxt_invalidate(device, context);

		adreno_get_gpu_halt(adreno_dev);

		adreno_hwsched_set_fault(adreno_dev);

		goto done;
	}

	mutex_lock(&device->mutex);

	rc = check_ack_failure(adreno_dev, &pending_ack);
done:
	del_waiter(hfi, &pending_ack);

	return rc;
}

void a6xx_hwsched_context_detach(struct adreno_context *drawctxt)
{
	struct kgsl_context *context = &drawctxt->base;
	struct kgsl_device *device = context->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret = 0;

	mutex_lock(&device->mutex);

	/* Only send HFI if device is not in SLUMBER */
	if (context->gmu_registered &&
		test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		ret = send_context_unregister_hfi(adreno_dev, context,
			drawctxt->internal_timestamp);

	if (!ret) {
		kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

		kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

		adreno_profile_process_results(adreno_dev);
	}

	context->gmu_registered = false;

	mutex_unlock(&device->mutex);
}

int a6xx_hwsched_preempt_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_get_value_cmd cmd;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	u32 seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	struct pending_cmd pending_ack;
	int rc;

	if (device->state != KGSL_STATE_ACTIVE)
		return 0;

	CMD_MSG_HDR(cmd, H2F_MSG_GET_VALUE);

	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr, seqnum);
	cmd.type = HFI_VALUE_PREEMPT_COUNT;
	cmd.subtype = 0;

	add_waiter(hfi, cmd.hdr, &pending_ack);

	rc = a6xx_hfi_cmdq_write(adreno_dev, (u32 *)&cmd);
	if (rc)
		goto done;

	rc = wait_ack_completion(adreno_dev, &pending_ack);
	if (rc)
		goto done;

	rc = check_ack_failure(adreno_dev, &pending_ack);

done:
	del_waiter(hfi, &pending_ack);

	return rc ? rc : pending_ack.results[2];
}
