// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/iommu.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_hwsched.h"
#include "adreno_pm4types.h"
#include "kgsl_device.h"
#include "kgsl_pwrctrl.h"
#include "kgsl_trace.h"

#define HFI_QUEUE_MAX (HFI_QUEUE_DEFAULT_CNT + HFI_QUEUE_DISPATCH_MAX_CNT)

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

static struct a6xx_hwsched_hfi *to_a6xx_hwsched_hfi(
	struct adreno_device *adreno_dev)
{
	struct a6xx_device *a6xx_dev = container_of(adreno_dev,
					struct a6xx_device, adreno_dev);
	struct a6xx_hwsched_device *a6xx_hwsched = container_of(a6xx_dev,
					struct a6xx_hwsched_device, a6xx_dev);

	return &a6xx_hwsched->hwsched_hfi;
}

static void a6xx_receive_ack_async(struct adreno_device *adreno_dev, void *rcvd,
	struct pending_cmd *ret_cmd)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 *ack = rcvd;
	u32 hdr = ack[0];
	u32 req_hdr = ack[1];
	u32 size_bytes = MSG_HDR_GET_SIZE(hdr) << 2;

	trace_kgsl_hfi_receive(MSG_HDR_GET_ID(req_hdr),
		MSG_HDR_GET_SIZE(req_hdr), MSG_HDR_GET_SEQNUM(req_hdr));

	if (size_bytes > sizeof(ret_cmd->results))
		dev_err(&gmu->pdev->dev,
			"Ack result too big: %d Truncating to: %d\n",
			size_bytes, sizeof(ret_cmd->results));

	if (HDR_CMP_SEQNUM(ret_cmd->sent_hdr, req_hdr)) {
		memcpy(ret_cmd->results, ack,
			min_t(u32, size_bytes, sizeof(ret_cmd->results)));
		complete(&ret_cmd->complete);
		return;
	}

	/* Didn't find the sender, list the waiter */
	dev_err_ratelimited(&gmu->pdev->dev,
		"Unexpectedly got id %d seqnum %d while waiting for id %d seqnum %d\n",
		MSG_HDR_GET_ID(req_hdr), MSG_HDR_GET_SEQNUM(req_hdr),
		MSG_HDR_GET_ID(ret_cmd->sent_hdr),
		MSG_HDR_GET_SEQNUM(ret_cmd->sent_hdr));
}

static void process_msgq_irq(struct adreno_device *adreno_dev)
{
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 rcvd[MAX_RCVD_SIZE];

	if (a6xx_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd)) <= 0)
		return;

	while (a6xx_hfi_queue_read(gmu, HFI_MSG_ID, rcvd, sizeof(rcvd)) > 0) {

		/*
		 * We are assuming that there is only one outstanding ack
		 * because hfi sending thread waits for completion while
		 * holding the device mutex
		 */
		if (MSG_HDR_GET_TYPE(rcvd[0]) == HFI_MSG_ACK)
			a6xx_receive_ack_async(adreno_dev, rcvd,
				&hfi->pending_ack);
	}
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

	if (status & HFI_IRQ_MSGQ_MASK)
		process_msgq_irq(adreno_dev);
	if (status & HFI_IRQ_DBGQ_MASK)
		a6xx_hfi_process_queue(gmu, HFI_DBG_ID, NULL);
	if (status & HFI_IRQ_CM3_FAULT_MASK) {
		atomic_set(&gmu->cm3_fault, 1);

		/* make sure other CPUs see the update */
		smp_wmb();

		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU CM3 fault interrupt received\n");
	}

	/* Ignore OOB bits */
	status &= GENMASK(31, 31 - (oob_max - 1));

	if (status & ~hfi->irq_mask)
		dev_err_ratelimited(&gmu->pdev->dev,
			"Unhandled HFI interrupts 0x%lx\n",
			status & ~hfi->irq_mask);

	return IRQ_HANDLED;
}

#define HFI_IRQ_MSGQ_MASK BIT(0)
#define HFI_RSP_TIMEOUT   100 /* msec */

static int wait_ack_completion(struct adreno_device *adreno_dev, u32 *cmd)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	int rc;

	rc = wait_for_completion_timeout(&hfi->pending_ack.complete,
		HFI_RSP_TIMEOUT);
	if (!rc) {
		dev_err(&gmu->pdev->dev,
			"Ack timeout for id:%d sequence=%d\n",
			MSG_HDR_GET_ID(*cmd),
			MSG_HDR_GET_SEQNUM(*cmd));
		gmu_fault_snapshot(KGSL_DEVICE(adreno_dev));
		return -ETIMEDOUT;
	}

	return 0;
}

static int check_ack_failure(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	struct pending_cmd *cmd = &hfi->pending_ack;
	int rc = cmd->results[2] ? -EINVAL : 0;

	if (cmd->results[2] == 0xffffffff)
		dev_err(&gmu->pdev->dev,
				"HFI ACK failure: Req 0x%8.8x\n",
				cmd->results[1]);

	/* reset the ack */
	memset(cmd->results, 0x0, sizeof(cmd->results));
	reinit_completion(&hfi->pending_ack.complete);
	cmd->sent_hdr = 0;

	return rc;
}

int a6xx_hfi_send_cmd_async(struct adreno_device *adreno_dev, void *data)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	u32 *cmd = data;
	u32 seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	int rc;

	*cmd = MSG_HDR_SET_SEQNUM(*cmd, seqnum);

	hfi->pending_ack.sent_hdr = cmd[0];

	rc = a6xx_hfi_cmdq_write(adreno_dev, cmd);
	if (rc)
		return rc;

	rc = wait_ack_completion(adreno_dev, cmd);
	if (rc)
		return rc;

	return check_ack_failure(adreno_dev);
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

	mapped = iommu_map_sg(a6xx_get_gmu_domain(gmu, desc->gmu_addr,
			desc->size),
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

	entry->gpu_md = kgsl_allocate_global(device, desc->size, flags, priv,
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
	struct a6xx_hwsched_hfi *hfi = to_a6xx_hwsched_hfi(adreno_dev);
	unsigned int seqnum = atomic_inc_return(&gmu->hfi.seqnum);
	int rc = 0;
	struct hfi_start_cmd cmd;
	u32 rcvd[MAX_RCVD_SIZE];

	cmd.hdr = CMD_MSG_HDR(H2F_MSG_START, sizeof(cmd));
	cmd.hdr = MSG_HDR_SET_SEQNUM(cmd.hdr, seqnum);

	hfi->pending_ack.sent_hdr = cmd.hdr;

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
		rc = a6xx_receive_ack_cmd(gmu, rcvd, &hfi->pending_ack);
		if (rc)
			return rc;

		return check_ack_failure(adreno_dev);
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

	init_completion(&hfi->pending_ack.complete);
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

	cmds[0] = CMD_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW,
			(A6XX_CP_INIT_DWORDS + 1) << 2);
	memcpy(&cmds[1], adreno_dev->cp_init_cmds, A6XX_CP_INIT_DWORDS << 2);

	return submit_raw_cmds(adreno_dev, cmds,
			"CP initialization failed to idle\n");
}

static int send_switch_to_unsecure(struct adreno_device *adreno_dev)
{
	u32 cmds[3];

	cmds[0] = CMD_MSG_HDR(H2F_MSG_ISSUE_CMD_RAW, sizeof(cmds));
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

	return 0;
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

static int send_context_register(struct adreno_device *adreno_dev,
	struct kgsl_context *context)
{
	struct hfi_register_ctxt_cmd cmd;
	struct kgsl_pagetable *pt = context->proc_priv->pagetable;

	cmd.hdr = CMD_MSG_HDR(H2F_MSG_REGISTER_CONTEXT, sizeof(cmd));
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

	cmd.hdr = CMD_MSG_HDR(H2F_MSG_CONTEXT_POINTERS, sizeof(cmd));
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
	if (ret)
		dev_err(&gmu->pdev->dev,
			"Unable to register context %d pointers: %d\n",
			context->id, ret);

	if (!ret)
		context->gmu_registered = true;

	return ret;
}

#define HFI_DSP_IRQ_BASE 2

#define DISPQ_IRQ_BIT(_idx) BIT((_idx) + HFI_DSP_IRQ_BASE)

int a6xx_hwsched_submit_cmdobj(struct adreno_device *adreno_dev, u32 flags,
	struct kgsl_drawobj_cmd *cmdobj)
{
	struct a6xx_hfi *hfi = to_a6xx_hfi(adreno_dev);
	struct kgsl_memobj_node *ib;
	int ret = 0;
	u32 numibs = 0, cmd_sizebytes;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct hfi_issue_ib *issue_ib;
	struct hfi_submit_cmd *cmd;

	ret = hfi_context_register(adreno_dev, drawobj->context);
	if (ret)
		return ret;

	/* Get the total IBs in the list */
	list_for_each_entry(ib, &cmdobj->cmdlist, node)
		numibs++;

	/* Add a *issue_ib struct for each IB */
	cmd_sizebytes = sizeof(*cmd) + (sizeof(*issue_ib) * numibs);

	cmd = kvmalloc(cmd_sizebytes, GFP_KERNEL);
	if (cmd == NULL)
		return -ENOMEM;

	cmd->hdr = CMD_MSG_HDR(H2F_MSG_ISSUE_CMD, cmd_sizebytes);
	cmd->hdr = MSG_HDR_SET_SEQNUM(cmd->hdr,
			atomic_inc_return(&hfi->seqnum));

	cmd->ctxt_id = drawobj->context->id;
	cmd->flags = flags;
	cmd->ts = drawobj->timestamp;
	cmd->numibs = numibs;

	issue_ib = (struct hfi_issue_ib *)&cmd[1];

	list_for_each_entry(ib, &cmdobj->cmdlist, node) {
		issue_ib->addr = ib->gpuaddr;
		issue_ib->size = ib->size;
		issue_ib++;
	}

	ret = a6xx_hfi_queue_write(adreno_dev, HFI_DSP_ID_0, (u32 *)cmd);
	if (ret)
		goto free;

	/*
	 * Memory barrier to make sure packet and write index are written before
	 * an interrupt is raised
	 */
	wmb();

	/* Send interrupt to GMU to receive the message */
	gmu_core_regwrite(KGSL_DEVICE(adreno_dev), A6XX_GMU_HOST2GMU_INTR_SET,
		DISPQ_IRQ_BIT(0));

free:
	kvfree(cmd);

	return ret;
}
