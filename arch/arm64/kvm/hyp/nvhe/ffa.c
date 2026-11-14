// SPDX-License-Identifier: GPL-2.0-only
/*
 * FF-A v1.0 proxy to filter out invalid memory-sharing SMC calls issued by
 * the host. FF-A is a slightly more palatable abbreviation of "Arm Firmware
 * Framework for Arm A-profile", which is specified by Arm in document
 * number DEN0077.
 *
 * Copyright (C) 2022 - Google LLC
 * Author: Andrew Walbran <qwandor@google.com>
 *
 * This driver hooks into the SMC trapping logic for the host and intercepts
 * all calls falling within the FF-A range. Each call is either:
 *
 *	- Forwarded on unmodified to the SPMD at EL3
 *	- Rejected as "unsupported"
 *	- Accompanied by a host stage-2 page-table check/update and reissued
 *
 * Consequently, any attempts by the host to make guest memory pages
 * accessible to the secure world using FF-A will be detected either here
 * (in the case that the memory is already owned by the guest) or during
 * donation to the guest (in the case that the memory was previously shared
 * with the secure world).
 *
 * To allow the rolling-back of page-table updates and FF-A calls in the
 * event of failure, operations involving the RXTX buffers are locked for
 * the duration and are therefore serialised.
 */

#include <linux/arm_ffa.h>
#include <asm/kvm_hypevents.h>
#include <asm/kvm_pkvm.h>
#include <kvm/arm_hypercalls.h>
#include <asm/virt.h>

#include <nvhe/arm-smccc.h>
#include <nvhe/alloc.h>
#include <nvhe/ffa.h>
#include <nvhe/mem_protect.h>
#include <nvhe/memory.h>
#include <nvhe/pkvm.h>
#include <nvhe/trap_handler.h>
#include <nvhe/spinlock.h>

#define VM_FFA_SUPPORTED(vcpu)		((vcpu)->kvm->arch.pkvm.ffa_support)
#define FFA_INVALID_SPM_HANDLE		(BIT(63) - 1)

/* The maximum number of secure partitions that can register for VM availability */
#define FFA_MAX_VM_AVAIL_SPS	(8)
#define FFA_VM_AVAIL_SPS_OOM	(-2)

#define FFA_PART_VM_AVAIL_MASK (FFA_PARTITION_DIRECT_RECV |\
				FFA_PARTITION_HYP_CREATE_VM |\
				FFA_PARTITION_HYP_DESTROY_VM)
#define FFA_PART_SUPPORTS_VM_AVAIL (FFA_PART_VM_AVAIL_MASK)

/*
 * A buffer to hold the maximum descriptor size we can see from the host,
 * which is required when the SPMD returns a fragmented FFA_MEM_RETRIEVE_RESP
 * when resolving the handle on the reclaim path.
 */
struct kvm_ffa_descriptor_buffer {
	void	*buf;
	size_t	len;
};

static struct kvm_ffa_descriptor_buffer ffa_desc_buf;

struct ffa_translation {
	struct list_head node;
	u64 ipa;
	phys_addr_t pa;
};

struct ffa_handle {
	u64	handle: 63;
	u64	is_lend: 1;
};

/*
 * Note that we don't currently lock these buffers explicitly, instead
 * relying on the locking of the hyp FFA buffers.
 */
static struct kvm_ffa_buffers hyp_buffers;
static struct kvm_ffa_buffers host_buffers;
static u32 hyp_ffa_version;
static bool has_version_negotiated;
static bool has_hyp_ffa_buffer_mapped;
static bool has_host_signalled;

static struct ffa_handle *spm_handles, *spm_free_handle;
static u32 num_spm_handles;

static DEFINE_HYP_SPINLOCK(version_lock);
static DEFINE_HYP_SPINLOCK(kvm_ffa_hyp_lock);

/* Secure partitions that can receive VM availability messages */
struct kvm_ffa_vm_avail_sp {
	u16 sp_id;
	bool wants_create;
	bool wants_destroy;
};

static struct kvm_ffa_vm_avail_sp vm_avail_sps[FFA_MAX_VM_AVAIL_SPS];
static int num_vm_avail_sps = -1;

static struct kvm_ffa_buffers *ffa_get_buffers(struct pkvm_hyp_vcpu *hyp_vcpu)
{
	if (!hyp_vcpu)
		return &host_buffers;

	return &pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu)->ffa_buf;
}

DECLARE_STATIC_KEY_FALSE(kvm_ffa_unmap_on_lend);

static int ffa_host_store_handle(u64 ffa_handle, bool is_lend)
{
	u32 i;
	struct ffa_handle *free_handle = NULL;

	if (!static_branch_unlikely(&kvm_ffa_unmap_on_lend))
		return 0;

	if (spm_free_handle >= spm_handles &&
	    spm_free_handle < (spm_handles + num_spm_handles)) {
		free_handle = spm_free_handle;
	} else {
		for (i = 0; i < num_spm_handles; i++)
			if (spm_handles[i].handle == FFA_INVALID_SPM_HANDLE)
				break;

		if (i == num_spm_handles)
			return -ENOSPC;

		free_handle = &spm_handles[i];
	}

	free_handle->handle = ffa_handle;
	free_handle->is_lend = is_lend;
	return 0;
}

static struct ffa_handle *ffa_host_get_handle(u64 ffa_handle)
{
	u32 i;

	for (i = 0; i < num_spm_handles; i++)
		if (spm_handles[i].handle == ffa_handle)
			return &spm_handles[i];
	return NULL;
}

static int ffa_host_clear_handle(u64 ffa_handle)
{
	struct ffa_handle *entry = ffa_host_get_handle(ffa_handle);

	if (!entry)
		return -EINVAL;

	entry->handle = FFA_INVALID_SPM_HANDLE;
	spm_free_handle = entry;
	return 0;
}

static void ffa_to_smccc_error(struct arm_smccc_res *res, u64 ffa_errno)
{
	*res = (struct arm_smccc_res) {
		.a0	= FFA_ERROR,
		.a2	= ffa_errno,
	};
}

static void ffa_to_smccc_res_prop(struct arm_smccc_res *res, int ret, u64 prop)
{
	if (ret == FFA_RET_SUCCESS) {
		*res = (struct arm_smccc_res) { .a0 = FFA_SUCCESS,
						.a2 = prop };
	} else {
		ffa_to_smccc_error(res, ret);
	}
}

static void ffa_to_smccc_res(struct arm_smccc_res *res, int ret)
{
	ffa_to_smccc_res_prop(res, ret, 0);
}

static void ffa_set_retval(struct kvm_cpu_context *ctxt,
			   struct arm_smccc_res *res)
{
	cpu_reg(ctxt, 0) = res->a0;
	cpu_reg(ctxt, 1) = res->a1;
	cpu_reg(ctxt, 2) = res->a2;
	cpu_reg(ctxt, 3) = res->a3;
}

static int ffa_map_hyp_buffers(u64 ffa_page_count)
{
	struct arm_smccc_res res;

	/*
	 * Ensure that the read of `has_hyp_ffa_buffer_mapped` is visible
	 * to other CPUs before proceeding.
	 */
	if (smp_load_acquire(&has_hyp_ffa_buffer_mapped))
		return 0;

	arm_smccc_1_1_smc(FFA_FN64_RXTX_MAP,
			  hyp_virt_to_phys(hyp_buffers.tx),
			  hyp_virt_to_phys(hyp_buffers.rx),
			  ffa_page_count,
			  0, 0, 0, 0,
			  &res);
	if (res.a0 != FFA_SUCCESS)
		return res.a2;

	/*
	 * Ensure that the write to `has_hyp_ffa_buffer_mapped` is visible
	 * to other CPUs after the previous operations.
	 */
	smp_store_release(&has_hyp_ffa_buffer_mapped, true);

	return res.a0 == FFA_SUCCESS ? FFA_RET_SUCCESS : res.a2;
}

static int ffa_unmap_hyp_buffers(void)
{
	struct arm_smccc_res res;

	/*
	 * Ensure that the read of `has_hyp_ffa_buffer_mapped` is visible
	 * to other CPUs before proceeding.
	 */
	if (!smp_load_acquire(&has_hyp_ffa_buffer_mapped))
		return 0;

	arm_smccc_1_1_smc(FFA_RXTX_UNMAP,
			  HOST_FFA_ID,
			  0, 0, 0, 0, 0, 0,
			  &res);
	if (res.a0 != FFA_SUCCESS)
		return res.a2;

	/*
	 * Ensure that the write to `has_hyp_ffa_buffer_mapped` is visible
	 * to other CPUs after the previous operations.
	 */
	smp_store_release(&has_hyp_ffa_buffer_mapped, false);

	return FFA_RET_SUCCESS;
}

static void ffa_mem_frag_tx(struct arm_smccc_res *res, u32 handle_lo,
			     u32 handle_hi, u32 fraglen, u32 endpoint_id)
{
	arm_smccc_1_1_smc(FFA_MEM_FRAG_TX,
			  handle_lo, handle_hi, fraglen, endpoint_id,
			  0, 0, 0,
			  res);
}

static void ffa_mem_frag_rx(struct arm_smccc_res *res, u32 handle_lo,
			     u32 handle_hi, u32 fragoff)
{
	arm_smccc_1_1_smc(FFA_MEM_FRAG_RX,
			  handle_lo, handle_hi, fragoff, HOST_FFA_ID,
			  0, 0, 0,
			  res);
}

static void ffa_mem_xfer(struct arm_smccc_res *res, u64 func_id, u32 len,
			  u32 fraglen)
{
	arm_smccc_1_1_smc(func_id, len, fraglen,
			  0, 0, 0, 0, 0,
			  res);
}

static void ffa_mem_reclaim(struct arm_smccc_res *res, u32 handle_lo,
			     u32 handle_hi, u32 flags)
{
	arm_smccc_1_1_smc(FFA_MEM_RECLAIM,
			  handle_lo, handle_hi, flags,
			  0, 0, 0, 0,
			  res);
}

static void ffa_retrieve_req(struct arm_smccc_res *res, u32 len)
{
	arm_smccc_1_1_smc(FFA_FN64_MEM_RETRIEVE_REQ,
			  len, len,
			  0, 0, 0, 0, 0,
			  res);
}

static void ffa_rx_release(struct arm_smccc_res *res)
{
	arm_smccc_1_1_smc(FFA_RX_RELEASE,
			  0, 0,
			  0, 0, 0, 0, 0,
			  res);
}

static int parse_vm_availability_resp(u32 partition_sz, u32 count)
{
	struct ffa_partition_info *part;
	u32 i, j, off;
	bool supports_direct_recv, wants_create, wants_destroy;

	if (num_vm_avail_sps >= 0)
		return FFA_RET_SUCCESS;
	if (num_vm_avail_sps == FFA_VM_AVAIL_SPS_OOM)
		return FFA_RET_NO_MEMORY;

	num_vm_avail_sps = 0;
	for (i = 0; i < count; i++) {
		if (check_mul_overflow(i, partition_sz, &off))
			return FFA_RET_INVALID_PARAMETERS;

		if (off >= KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE)
			return FFA_RET_INVALID_PARAMETERS;

		part = hyp_buffers.rx + off;
		supports_direct_recv = part->properties & FFA_PARTITION_DIRECT_RECV;
		wants_create = part->properties & FFA_PARTITION_HYP_CREATE_VM;
		wants_destroy = part->properties & FFA_PARTITION_HYP_DESTROY_VM;

		if (supports_direct_recv && (wants_create || wants_destroy)) {
			/* Check for duplicate SP IDs */
			for (j = 0; j < num_vm_avail_sps; j++)
				if (vm_avail_sps[j].sp_id == part->id)
					break;

			if (j == num_vm_avail_sps) {
				if (num_vm_avail_sps >= FFA_MAX_VM_AVAIL_SPS) {
					/* We ran out of space in the array */
					num_vm_avail_sps = FFA_VM_AVAIL_SPS_OOM;
					return FFA_RET_NO_MEMORY;
				}

				vm_avail_sps[num_vm_avail_sps].sp_id = part->id;
				vm_avail_sps[num_vm_avail_sps].wants_create = wants_create;
				vm_avail_sps[num_vm_avail_sps].wants_destroy = wants_destroy;
				num_vm_avail_sps++;
			}
		}
	}

	return FFA_RET_SUCCESS;
}

static int kvm_notify_vm_availability(uint16_t vm_handle, struct kvm_ffa_buffers *ffa_buf,
				      u32 availability_msg)
{
	int i;
	struct arm_smccc_res res;
	u64 avail_bit = availability_msg != FFA_VM_DESTRUCTION_MSG;

	for (i = 0; i < num_vm_avail_sps; i++) {
		u64 sp_mask = 1UL << i;
		u64 avail_value = avail_bit << i;
		uint32_t dest = ((uint32_t)vm_avail_sps[i].sp_id << 16) | hyp_smp_processor_id();

		if ((ffa_buf->vm_avail_bitmap & sp_mask) == avail_value &&
		    !(ffa_buf->vm_creating_bitmap & sp_mask))
			continue;

		if (avail_bit && !vm_avail_sps[i].wants_create) {
			/*
			 * The SP did not ask for creation messages,
			 * so just mark this VM as available and
			 * continue
			 */
			ffa_buf->vm_avail_bitmap |= avail_value;
			continue;
		} else if (!avail_bit && !vm_avail_sps[i].wants_destroy) {
			/*
			 * The SP did not ask for destruction messages,
			 * so just mark this VM as not available and
			 * continue
			 */
			ffa_buf->vm_avail_bitmap &= ~sp_mask;
			continue;
		}

		/*
		 * Give the SP some cycles in advance,
		 * in case it got interrupted the last time.
		 *
		 * Some TEEs return NOT_SUPPORTED instead.
		 * If that happens, ignore the error and continue.
		 */
		arm_smccc_1_1_smc(FFA_RUN, dest, 0, 0, 0, 0, 0, 0, &res);
		if (res.a0 == FFA_ERROR && (int)res.a2 != FFA_RET_NOT_SUPPORTED)
			return ffa_to_linux_errno(res.a2);
		else if (res.a0 == FFA_INTERRUPT)
			return -EINTR;

		if (availability_msg == FFA_VM_DESTRUCTION_MSG &&
		    (ffa_buf->vm_creating_bitmap & sp_mask)) {
			/*
			 * If we sent the initial creation message for this VM
			 * but never got the success response from the TEE, we
			 * need to keep trying to create it until it works.
			 * Otherwise we cannot destroy it.
			 *
			 * TODO: this is not triggered for SPs that requested only
			 * creation messages (but not destruction). In that case,
			 * we will never retry the creation message, and the SP
			 * will probably leak its state for the pending VM.
			 */
			arm_smccc_1_1_smc(FFA_MSG_SEND_DIRECT_REQ, vm_avail_sps[i].sp_id,
					  FFA_VM_CREATION_MSG, HANDLE_LOW(FFA_INVALID_HANDLE),
					  HANDLE_HIGH(FFA_INVALID_HANDLE), vm_handle, 0, 0,
					  &res);

			if (res.a0 != FFA_MSG_SEND_DIRECT_RESP)
				return -EINVAL;
			if (res.a3 != FFA_RET_SUCCESS)
				return ffa_to_linux_errno(res.a3);

			/* Creation completed successfully, clear the flag */
			ffa_buf->vm_creating_bitmap &= ~sp_mask;
		}

		arm_smccc_1_1_smc(FFA_MSG_SEND_DIRECT_REQ, vm_avail_sps[i].sp_id,
				  availability_msg, HANDLE_LOW(FFA_INVALID_HANDLE),
				  HANDLE_HIGH(FFA_INVALID_HANDLE), vm_handle, 0, 0,
				  &res);
		if (res.a0 != FFA_MSG_SEND_DIRECT_RESP)
			return -EINVAL;

		switch ((int)res.a3) {
		case FFA_RET_SUCCESS:
			ffa_buf->vm_avail_bitmap &= ~sp_mask;
			ffa_buf->vm_avail_bitmap |= avail_value;
			ffa_buf->vm_creating_bitmap &= ~sp_mask;
			break;

		case FFA_RET_INTERRUPTED:
		case FFA_RET_RETRY:
			if (availability_msg == FFA_VM_CREATION_MSG)
				ffa_buf->vm_creating_bitmap |= sp_mask;

			fallthrough;
		default:
			return ffa_to_linux_errno(res.a3);
		}
	}

	return 0;
}

static void do_ffa_rxtx_map(struct arm_smccc_res *res,
			    struct kvm_cpu_context *ctxt,
			    struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(phys_addr_t, tx, ctxt, 1);
	DECLARE_REG(phys_addr_t, rx, ctxt, 2);
	DECLARE_REG(u32, npages, ctxt, 3);
	int ret = 0;
	void *rx_virt, *tx_virt;
	struct kvm_ffa_buffers *ffa_buf;

	if (npages != (KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE) / FFA_PAGE_SIZE) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto out;
	}

	if (!PAGE_ALIGNED(tx) || !PAGE_ALIGNED(rx)) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto out;
	}

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ffa_buf = ffa_get_buffers(hyp_vcpu);
	if (ffa_buf->tx) {
		ret = FFA_RET_DENIED;
		goto out_unlock;
	}

	/*
	 * Map our hypervisor buffers into the SPMD before mapping and
	 * pinning the host buffers in our own address space.
	 */
	ret = ffa_map_hyp_buffers(npages);
	if (ret)
		goto out_unlock;

	ret = __pkvm_host_share_hyp(hyp_phys_to_pfn(tx));
	if (ret) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto err_unmap;
	}

	ret = __pkvm_host_share_hyp(hyp_phys_to_pfn(rx));
	if (ret) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto err_unshare_tx;
	}

	tx_virt = hyp_phys_to_virt(tx);
	ret = hyp_pin_shared_mem(tx_virt, tx_virt + 1);
	if (ret) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto err_unshare_rx;
	}

	rx_virt = hyp_phys_to_virt(rx);
	ret = hyp_pin_shared_mem(rx_virt, rx_virt + 1);
	if (ret) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto err_unpin_tx;
	}

	ffa_buf->tx = tx_virt;
	ffa_buf->rx = rx_virt;

out_unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);
out:
	ffa_to_smccc_res(res, ret);
	return;

err_unpin_tx:
	hyp_unpin_shared_mem(tx_virt, tx_virt + 1);
err_unshare_rx:
	__pkvm_host_unshare_hyp(hyp_phys_to_pfn(rx));
err_unshare_tx:
	__pkvm_host_unshare_hyp(hyp_phys_to_pfn(tx));
err_unmap:
	ffa_unmap_hyp_buffers();
	goto out_unlock;
}

static int do_ffa_rxtx_guest_map(struct kvm_cpu_context *ctxt, struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(phys_addr_t, tx, ctxt, 1);
	DECLARE_REG(phys_addr_t, rx, ctxt, 2);
	DECLARE_REG(u32, npages, ctxt, 3);
	int ret = 0;
	u64 rx_va, tx_va;
	struct kvm_ffa_buffers *ffa_buf;
	struct kvm_hyp_req *req;

	if (npages != (KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE) / FFA_PAGE_SIZE)
		return -EINVAL;

	if (!PAGE_ALIGNED(tx) || !PAGE_ALIGNED(rx))
		return -EINVAL;

	ret = __pkvm_guest_share_hyp_page(hyp_vcpu, tx, &tx_va);
	if (ret)
		goto out_err;

	ret = __pkvm_guest_share_hyp_page(hyp_vcpu, rx, &rx_va);
	if (ret)
		goto out_err_with_tx;

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ffa_buf = ffa_get_buffers(hyp_vcpu);
	if (ffa_buf->tx) {
		ret = -EACCES;
		goto out_unlock;
	}

	ffa_buf->tx = (void *)tx_va;
	ffa_buf->rx = (void *)rx_va;
	ffa_buf->tx_ipa = tx;
	ffa_buf->rx_ipa = rx;
out_unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);
	return ret;
out_err_with_tx:
	WARN_ON(__pkvm_guest_unshare_hyp_page(hyp_vcpu, tx));
out_err:
	if (ret == -EFAULT) {
		req = pkvm_hyp_req_reserve(hyp_vcpu, KVM_HYP_REQ_TYPE_MAP);
		if (!req || !pkvm_hyp_req_reserve(hyp_vcpu, KVM_HYP_REQ_TYPE_MAP))
			return -ENOSPC;

		req->map.guest_ipa = tx;
		req->map.size = PAGE_SIZE;

		req++;

		req->map.guest_ipa = rx;
		req->map.size = PAGE_SIZE;
	}

	return ret;
}

static void do_ffa_rxtx_unmap(struct arm_smccc_res *res,
			      struct kvm_cpu_context *ctxt,
			      struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(u32, id, ctxt, 1);
	int ret = 0;
	struct kvm_ffa_buffers *ffa_buf;

	if (hyp_vcpu_to_ffa_handle(hyp_vcpu) != id) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto out;
	}

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ffa_buf = ffa_get_buffers(hyp_vcpu);
	if (!ffa_buf->tx) {
		ret = FFA_RET_INVALID_PARAMETERS;
		goto out_unlock;
	}

	if (!hyp_vcpu) {
		hyp_unpin_shared_mem(ffa_buf->tx, ffa_buf->tx + 1);
		WARN_ON(__pkvm_host_unshare_hyp(hyp_virt_to_pfn(ffa_buf->tx)));

		hyp_unpin_shared_mem(ffa_buf->rx, ffa_buf->rx + 1);
		WARN_ON(__pkvm_host_unshare_hyp(hyp_virt_to_pfn(ffa_buf->rx)));

		ffa_unmap_hyp_buffers();
	} else {
		WARN_ON(__pkvm_guest_unshare_hyp_page(hyp_vcpu, ffa_buf->tx_ipa));
		WARN_ON(__pkvm_guest_unshare_hyp_page(hyp_vcpu, ffa_buf->rx_ipa));
	}

	ffa_buf->rx = NULL;
	ffa_buf->tx = NULL;

out_unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);
out:
	ffa_to_smccc_res(res, ret);
}

static u32 __ffa_host_share_ranges(struct ffa_mem_region_addr_range *ranges,
				   u32 nranges, bool is_lend)
{
	u32 i;
	int ret;

	for (i = 0; i < nranges; ++i) {
		struct ffa_mem_region_addr_range *range = &ranges[i];
		u64 sz = (u64)range->pg_cnt * FFA_PAGE_SIZE;
		u64 pfn = hyp_phys_to_pfn(range->address);

		if (!PAGE_ALIGNED(sz))
			break;

		if (static_branch_unlikely(&kvm_ffa_unmap_on_lend) && is_lend)
			ret = __pkvm_host_donate_ffa(pfn, sz / PAGE_SIZE);
		else
			ret = __pkvm_host_share_ffa(pfn, sz / PAGE_SIZE);
		if (ret)
			break;
	}

	return i;
}

/*
 * Verify if the page is lent on shared and unshare it with FF-A.
 * On success, return the number of *unshared* pages and store in the
 * is_lend argument whether the range was shared or lent.
 */
static u32 __ffa_host_unshare_ranges(struct ffa_mem_region_addr_range *ranges,
				     u32 nranges, bool is_lend)
{
	u32 i;
	int ret;

	for (i = 0; i < nranges; ++i) {
		struct ffa_mem_region_addr_range *range = &ranges[i];
		u64 sz = (u64)range->pg_cnt * FFA_PAGE_SIZE;
		u64 pfn = hyp_phys_to_pfn(range->address);

		if (!PAGE_ALIGNED(sz))
			break;

		if (static_branch_unlikely(&kvm_ffa_unmap_on_lend) && is_lend)
			ret = __pkvm_host_reclaim_ffa(pfn, sz / PAGE_SIZE);
		else

			ret = __pkvm_host_unshare_ffa(pfn, sz / PAGE_SIZE);
		if (ret)
			break;
	}

	return i;
}

static int ffa_store_translation(struct ffa_mem_transfer *transfer, u64 ipa, phys_addr_t pa)
{
	struct ffa_translation *tr;

	tr = hyp_alloc(sizeof(struct ffa_translation));
	if (!tr)
		return -ENOMEM;

	tr->ipa = ipa;
	tr->pa = pa;
	list_add(&tr->node, &transfer->translations);

	return 0;
}

static void ffa_guest_unshare_ranges(struct pkvm_hyp_vcpu *vcpu,
				     struct ffa_mem_transfer *transfer)
{
	struct ffa_translation *translation, *tmp;

	list_for_each_entry_safe(translation, tmp, &transfer->translations, node) {
		WARN_ON(__pkvm_guest_unshare_ffa_page(vcpu, translation->ipa));
		list_del(&translation->node);
		hyp_free(translation);
	}
}

static int ffa_guest_share_ranges(struct ffa_mem_region_addr_range *ranges,
				  u32 nranges, struct pkvm_hyp_vcpu *vcpu,
				  struct ffa_composite_mem_region *out_region,
				  size_t reg_len,
				  struct ffa_mem_transfer *transfer)
{
	struct ffa_mem_region_addr_range *range;
	struct ffa_mem_region_addr_range *buf = out_region->constituents;
	int i, j, ret;
	u32 mem_region_idx = 0;
	u64 ipa, pa;

	for (i = 0; i < nranges; i++) {
		range = &ranges[i];
		for (j = 0; j < range->pg_cnt; j++) {
			if (mem_region_idx * sizeof(struct ffa_mem_region_addr_range) >= reg_len) {
				ret = -EINVAL;
				goto unshare;
			}

			ipa = range->address + PAGE_SIZE * j;
			ret = __pkvm_guest_share_ffa_page(vcpu, ipa, &pa);
			if (ret)
				goto unshare;

			ret = ffa_store_translation(transfer, ipa, pa);
			if (ret) {
				WARN_ON(__pkvm_guest_unshare_ffa_page(vcpu, ipa));
				goto unshare;
			}

			buf[mem_region_idx].address = pa;
			buf[mem_region_idx].pg_cnt = 1;

			if (mem_region_idx + 1 < mem_region_idx) {
				ret = -EINVAL;
				goto unshare;
			}

			mem_region_idx++;
		}
	}

	out_region->addr_range_cnt = mem_region_idx;
	return 0;
unshare:
	ffa_guest_unshare_ranges(vcpu, transfer);
	return ret;
}

static int ffa_host_share_ranges(struct ffa_mem_region_addr_range *ranges,
				 u32 nranges, bool is_lend)
{
	u32 nshared = __ffa_host_share_ranges(ranges, nranges, is_lend);
	int ret = 0;

	if (nshared != nranges) {
		WARN_ON(__ffa_host_unshare_ranges(ranges, nshared, is_lend) != nshared);
		ret = -EACCES;
	}

	return ret;
}

static int ffa_host_unshare_ranges(struct ffa_mem_region_addr_range *ranges,
				   u32 nranges, bool is_lend)
{
	int ret = 0;
	u32 nunshared = __ffa_host_unshare_ranges(ranges, nranges, is_lend);

	if (nunshared != nranges) {
		WARN_ON(__ffa_host_share_ranges(ranges, nunshared, is_lend) != nunshared);
		ret = -EACCES;
	}

	return ret;
}

static void do_ffa_mem_frag_tx(struct arm_smccc_res *res,
			       struct kvm_cpu_context *ctxt,
			       struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(u32, handle_lo, ctxt, 1);
	DECLARE_REG(u32, handle_hi, ctxt, 2);
	DECLARE_REG(u32, fraglen, ctxt, 3);
	DECLARE_REG(u32, endpoint_id, ctxt, 4);
	struct ffa_mem_region_addr_range *buf;
	int ret = FFA_RET_INVALID_PARAMETERS;
	u32 nr_ranges;
	struct kvm_ffa_buffers *ffa_buf;
	bool is_lend = false;
	u64 host_handle = PACK_HANDLE(handle_lo, handle_hi);
	struct ffa_handle *entry;

	if (fraglen > KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE)
		goto out;

	if (fraglen % sizeof(*buf))
		goto out;

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ffa_buf = ffa_get_buffers(hyp_vcpu);
	if (!ffa_buf->tx)
		goto out_unlock;

	buf = hyp_buffers.tx;
	memcpy(buf, ffa_buf->tx, fraglen);
	nr_ranges = fraglen / sizeof(*buf);

	if (static_branch_unlikely(&kvm_ffa_unmap_on_lend)) {
		entry = ffa_host_get_handle(host_handle);
		if (!entry) {
			ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
			goto out_unlock;
		}

		is_lend = entry->is_lend;
	}

	ret = ffa_host_share_ranges(buf, nr_ranges, is_lend);
	if (ret) {
		/*
		 * We're effectively aborting the transaction, so we need
		 * to restore the global state back to what it was prior to
		 * transmission of the first fragment.
		 */
		ffa_mem_reclaim(res, handle_lo, handle_hi, 0);
		WARN_ON(res->a0 != FFA_SUCCESS);
		goto out_unlock;
	}

	ffa_mem_frag_tx(res, handle_lo, handle_hi, fraglen, endpoint_id);
	if (res->a0 != FFA_SUCCESS && res->a0 != FFA_MEM_FRAG_RX)
		WARN_ON(ffa_host_unshare_ranges(buf, nr_ranges, is_lend));

out_unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);
out:
	if (ret)
		ffa_to_smccc_res(res, ret);

	/*
	 * If for any reason this did not succeed, we're in trouble as we have
	 * now lost the content of the previous fragments and we can't rollback
	 * the host stage-2 changes. The pages previously marked as shared will
	 * remain stuck in that state forever, hence preventing the host from
	 * sharing/donating them again and may possibly lead to subsequent
	 * failures, but this will not compromise confidentiality.
	 */
	return;
}

static bool is_page_count_valid(struct ffa_composite_mem_region *reg,
				u32 nranges)
{
	u32 i, pg_cnt = 0, new_pg_cnt;

	for (i = 0; i < nranges; i++) {
		new_pg_cnt = pg_cnt + reg->constituents[i].pg_cnt;
		if (new_pg_cnt < pg_cnt)
			return false;

		pg_cnt = new_pg_cnt;
	}

	return pg_cnt == reg->total_pg_cnt;
}

static int __do_ffa_mem_xfer(const u64 func_id,
			     struct arm_smccc_res *res,
			     struct kvm_cpu_context *ctxt,
			     struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(u32, len, ctxt, 1);
	DECLARE_REG(u32, fraglen, ctxt, 2);
	DECLARE_REG(u64, addr_mbz, ctxt, 3);
	DECLARE_REG(u32, npages_mbz, ctxt, 4);
	struct ffa_mem_region_attributes *ep_mem_access;
	struct ffa_composite_mem_region *reg, *temp_reg;
	struct ffa_mem_region *buf;
	struct kvm_ffa_buffers *ffa_buf;
	u32 offset, nr_ranges;
	int ret = 0;
	struct ffa_mem_transfer *transfer = NULL;
	u64 ffa_handle;
	bool is_lend = func_id == FFA_FN64_MEM_LEND;

	if (addr_mbz || npages_mbz || fraglen > len ||
	    fraglen > KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE) {
		ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
		return 0;
	}

	if (fraglen < sizeof(struct ffa_mem_region) +
		      sizeof(struct ffa_mem_region_attributes)) {
		ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
		return 0;
	}

	if (hyp_vcpu) {
		/* Reject the fragmentation API for the guest */
		if (len != fraglen) {
			ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
			return 0;
		}

		transfer = hyp_alloc(sizeof(struct ffa_mem_transfer));
		if (!transfer)
			return -ENOMEM;

		INIT_LIST_HEAD(&transfer->translations);
	}

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ffa_buf = ffa_get_buffers(hyp_vcpu);
	if (!ffa_buf->tx) {
		ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
		goto out_unlock;
	}

	if (len > ffa_desc_buf.len) {
		ffa_to_smccc_error(res, FFA_RET_NO_MEMORY);
		goto out_unlock;
	}

	buf = hyp_buffers.tx;
	memcpy(buf, ffa_buf->tx, fraglen);

	ep_mem_access = (void *)buf +
			ffa_mem_desc_offset(buf, 0, hyp_ffa_version);
	offset = ep_mem_access->composite_off;
	if (!offset || buf->ep_count != 1 || buf->sender_id != hyp_vcpu_to_ffa_handle(hyp_vcpu)) {
		ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
		goto out_unlock;
	}

	if (fraglen < offset + sizeof(struct ffa_composite_mem_region)) {
		ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
		goto out_unlock;
	}

	reg = (void *)buf + offset;
	nr_ranges = ((void *)buf + fraglen) - (void *)reg->constituents;
	if (nr_ranges % sizeof(reg->constituents[0])) {
		ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
		goto out_unlock;
	}

	nr_ranges /= sizeof(reg->constituents[0]);
	if (hyp_vcpu) {
		if (!is_page_count_valid(reg, nr_ranges)) {
			ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
			goto out_unlock;
		}

		size_t translated_sz = reg->total_pg_cnt * sizeof(struct ffa_mem_region_addr_range)
			+ offset;
		if (translated_sz > PAGE_SIZE) {
			ffa_to_smccc_error(res, FFA_RET_INVALID_PARAMETERS);
			goto out_unlock;
		}

		memcpy(ffa_desc_buf.buf, buf, offset);
		temp_reg = ffa_desc_buf.buf + offset;
		ret = ffa_guest_share_ranges(reg->constituents, nr_ranges, hyp_vcpu,
					     temp_reg, ffa_desc_buf.len - offset,
					     transfer);
		if (!ret) {
			/* Re-adjust the size of the transfer after painting with PAs */
			if (temp_reg->addr_range_cnt > reg->addr_range_cnt) {
				u32 extra_sz = (temp_reg->addr_range_cnt - reg->addr_range_cnt) *
					sizeof(struct ffa_mem_region_addr_range);
				fraglen += extra_sz;
				len += extra_sz;

				nr_ranges = reg->addr_range_cnt = temp_reg->addr_range_cnt;
			}

			memcpy(reg->constituents, temp_reg->constituents,
			       temp_reg->addr_range_cnt * sizeof(struct ffa_mem_region_addr_range));
		}
	} else
		ret = ffa_host_share_ranges(reg->constituents, nr_ranges, is_lend);
	if (ret)
		goto out_unlock;

	ffa_mem_xfer(res, func_id, len, fraglen);
	if (fraglen != len) {
		if (res->a0 != FFA_MEM_FRAG_RX)
			goto err_unshare;

		if (res->a3 != fraglen)
			goto err_unshare;

		ffa_handle = PACK_HANDLE(res->a1, res->a2);
	} else if (res->a0 == FFA_SUCCESS) {
		ffa_handle = PACK_HANDLE(res->a2, res->a3);
	} else {
		goto err_unshare;
	}

	if (hyp_vcpu && transfer) {
		transfer->ffa_handle = ffa_handle;
		list_add(&transfer->node, &ffa_buf->xfer_list);
	} else if (!hyp_vcpu) {
		ret = ffa_host_store_handle(ffa_handle, is_lend);
		if (ret)
			goto err_unshare;
	}
	hyp_spin_unlock(&kvm_ffa_hyp_lock);
	return 0;
out_unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);
	if (transfer)
		hyp_free(transfer);
	if (ret)
		ffa_to_smccc_res(res, linux_errno_to_ffa(ret));
	return ret;
err_unshare:
	if (hyp_vcpu)
		ffa_guest_unshare_ranges(hyp_vcpu, transfer);
	else
		WARN_ON(ffa_host_unshare_ranges(reg->constituents, nr_ranges, is_lend));
	goto out_unlock;
}

#define do_ffa_mem_xfer(fid, res, ctxt, hyp_vcpu)	({\
		BUILD_BUG_ON((fid) != FFA_FN64_MEM_SHARE &&	\
			     (fid) != FFA_FN64_MEM_LEND);	\
		__do_ffa_mem_xfer((fid), (res), (ctxt), (hyp_vcpu));\
	})

struct ffa_mem_transfer *find_transfer_by_handle(u64 ffa_handle, struct kvm_ffa_buffers *buf)
{
	struct ffa_mem_transfer *transfer;

	list_for_each_entry(transfer, &buf->xfer_list, node)
		if (transfer->ffa_handle == ffa_handle)
			return transfer;
	return NULL;
}

static void do_ffa_mem_reclaim(struct arm_smccc_res *res,
			       struct kvm_cpu_context *ctxt,
			       struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(u32, handle_lo, ctxt, 1);
	DECLARE_REG(u32, handle_hi, ctxt, 2);
	DECLARE_REG(u32, flags, ctxt, 3);
	struct ffa_mem_region_attributes *ep_mem_access;
	struct ffa_composite_mem_region *reg;
	u32 offset, len, fraglen, fragoff;
	struct ffa_mem_region *buf;
	int ret = 0;
	u64 handle;
	struct ffa_mem_transfer *transfer = NULL;
	struct kvm_ffa_buffers *ffa_buf;
	struct ffa_handle *entry;
	bool is_lend = false;

	handle = PACK_HANDLE(handle_lo, handle_hi);

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ffa_buf = ffa_get_buffers(hyp_vcpu);
	if (hyp_vcpu) {
		transfer = find_transfer_by_handle(handle, ffa_buf);
		if (!transfer) {
			ret = FFA_RET_INVALID_PARAMETERS;
			goto out_unlock;
		}

		goto out_reclaim;
	} else {
		transfer = __pkvm_get_vm_ffa_transfer(handle);

		/* Prevent the host from replicating a transfer handle used by the guest */
		WARN_ON(transfer);

		if (static_branch_unlikely(&kvm_ffa_unmap_on_lend)) {
			entry = ffa_host_get_handle(handle);
			if (!entry) {
				ret = FFA_RET_INVALID_PARAMETERS;
				goto out_unlock;
			}

			is_lend = entry->is_lend;
		}
	}

	buf = hyp_buffers.tx;
	*buf = (struct ffa_mem_region) {
		.handle		= handle,
	};

	ffa_retrieve_req(res, sizeof(*buf));
	buf = hyp_buffers.rx;
	if (res->a0 != FFA_MEM_RETRIEVE_RESP)
		goto out_unlock;

	len = res->a1;
	fraglen = res->a2;

	ep_mem_access = (void *)buf +
			ffa_mem_desc_offset(buf, 0, hyp_ffa_version);
	offset = ep_mem_access->composite_off;
	/*
	 * We can trust the SPMD to get this right, but let's at least
	 * check that we end up with something that doesn't look _completely_
	 * bogus.
	 */
	if (WARN_ON(offset > len ||
		    fraglen > KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE)) {
		ret = FFA_RET_ABORTED;
		ffa_rx_release(res);
		goto out_unlock;
	}

	if (len > ffa_desc_buf.len) {
		ret = FFA_RET_NO_MEMORY;
		ffa_rx_release(res);
		goto out_unlock;
	}

	buf = ffa_desc_buf.buf;
	memcpy(buf, hyp_buffers.rx, fraglen);
	ffa_rx_release(res);

	for (fragoff = fraglen; fragoff < len; fragoff += fraglen) {
		ffa_mem_frag_rx(res, handle_lo, handle_hi, fragoff);
		if (res->a0 != FFA_MEM_FRAG_TX) {
			ret = FFA_RET_INVALID_PARAMETERS;
			goto out_unlock;
		}

		fraglen = res->a3;
		memcpy((void *)buf + fragoff, hyp_buffers.rx, fraglen);
		ffa_rx_release(res);
	}

out_reclaim:
	ffa_mem_reclaim(res, handle_lo, handle_hi, flags);
	if (res->a0 != FFA_SUCCESS)
		goto out_unlock;

	/* If the SPMD was happy, then we should be too. */
	if (hyp_vcpu)
		ffa_guest_unshare_ranges(hyp_vcpu, transfer);
	else {
		reg = (void *)buf + offset;
		WARN_ON(ffa_host_unshare_ranges(reg->constituents,
						reg->addr_range_cnt, is_lend));
		if (static_branch_unlikely(&kvm_ffa_unmap_on_lend))
			ffa_host_clear_handle(handle);
	}

	if (transfer) {
		list_del(&transfer->node);
		hyp_free(transfer);
	}
out_unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);

	if (ret)
		ffa_to_smccc_res(res, ret);
}

/*
 * Is a given FFA function supported, either by forwarding on directly
 * or by handling at EL2?
 */
static bool ffa_call_supported(u64 func_id)
{
	switch (func_id) {
	/* Unsupported memory management calls */
	case FFA_FN64_MEM_RETRIEVE_REQ:
	case FFA_MEM_RETRIEVE_RESP:
	case FFA_MEM_RELINQUISH:
	case FFA_MEM_OP_PAUSE:
	case FFA_MEM_OP_RESUME:
	case FFA_MEM_FRAG_RX:
	case FFA_FN64_MEM_DONATE:
	/* Indirect message passing via RX/TX buffers */
	case FFA_MSG_SEND:
	case FFA_MSG_POLL:
	case FFA_MSG_WAIT:
	/* 32-bit variants of 64-bit calls */
	case FFA_MSG_SEND_DIRECT_RESP:
	case FFA_RXTX_MAP:
	case FFA_MEM_DONATE:
	case FFA_MEM_RETRIEVE_REQ:
		return false;
	}

	return true;
}

static bool do_ffa_features(struct arm_smccc_res *res,
			    struct kvm_cpu_context *ctxt)
{
	DECLARE_REG(u32, id, ctxt, 1);
	u64 prop = 0;
	int ret = 0;

	if (!ffa_call_supported(id)) {
		ret = FFA_RET_NOT_SUPPORTED;
		goto out_handled;
	}

	switch (id) {
	case FFA_MEM_SHARE:
	case FFA_FN64_MEM_SHARE:
	case FFA_MEM_LEND:
	case FFA_FN64_MEM_LEND:
		ret = FFA_RET_SUCCESS;
		prop = 0; /* No support for dynamic buffers */
		goto out_handled;
	default:
		return false;
	}

out_handled:
	ffa_to_smccc_res_prop(res, ret, prop);
	return true;
}

static void do_ffa_guest_features(struct arm_smccc_res *res, struct kvm_cpu_context *ctxt)
{
	DECLARE_REG(u32, id, ctxt, 1);
	u64 prop = 0;
	int ret;

	switch (id) {
	case FFA_MEM_SHARE:
	case FFA_FN64_MEM_SHARE:
	case FFA_MEM_LEND:
	case FFA_FN64_MEM_LEND:
	case FFA_RX_RELEASE:
		ret = FFA_RET_SUCCESS;
		goto out_handled;
	case FFA_RXTX_MAP:
	case FFA_FN64_RXTX_MAP:
		ret = FFA_RET_SUCCESS;
		if (PAGE_SIZE == SZ_4K)
			prop = FFA_FEAT_RXTX_MIN_SZ_4K;
		else if (PAGE_SIZE == SZ_64K)
			prop = FFA_FEAT_RXTX_MIN_SZ_64K;
		else if (PAGE_SIZE == SZ_16K)
			prop = FFA_FEAT_RXTX_MIN_SZ_16K;
		else	/* prop == b'11 is reserved per DEN0077A v1.3 ALP1 */
			ret = FFA_RET_NOT_SUPPORTED;
		goto out_handled;
	default:
		ret = FFA_RET_NOT_SUPPORTED;
		goto out_handled;
	}

out_handled:
	ffa_to_smccc_res_prop(res, ret, prop);
}

static void do_ffa_part_get_response(struct arm_smccc_res *res,
				     u32 uuid0, u32 uuid1, u32 uuid2,
				     u32 uuid3, u32 flags, struct kvm_ffa_buffers *ffa_buf)
{
	int ret;
	u32 count, partition_sz, copy_sz;

	arm_smccc_1_1_smc(FFA_PARTITION_INFO_GET, uuid0, uuid1,
			  uuid2, uuid3, flags, 0, 0,
			  res);

	if (res->a0 != FFA_SUCCESS)
		return;

	count = res->a2;
	if (!count)
		return;

	if (hyp_ffa_version > FFA_VERSION_1_0) {
		/* Get the number of partitions deployed in the system */
		if (flags & 0x1)
			return;

		partition_sz  = res->a3;
	} else
		/* FFA_VERSION_1_0 lacks the size in the response */
		partition_sz = FFA_1_0_PARTITON_INFO_SZ;

	copy_sz = partition_sz * count;
	if (copy_sz > KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE) {
		ffa_to_smccc_res(res, FFA_RET_ABORTED);
		return;
	}

	ret = parse_vm_availability_resp(partition_sz, count);
	if (ret)
		ffa_to_smccc_res(res, ret);

	if (ffa_buf)
		memcpy(ffa_buf->rx, hyp_buffers.rx, copy_sz);
}

static int hyp_ffa_post_init(void)
{
	size_t min_rxtx_sz;
	struct arm_smccc_res res;

	arm_smccc_1_1_smc(FFA_ID_GET, 0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 != FFA_SUCCESS)
		return -EOPNOTSUPP;

	if (res.a2 != HOST_FFA_ID)
		return -EINVAL;

	arm_smccc_1_1_smc(FFA_FEATURES, FFA_FN64_RXTX_MAP,
			  0, 0, 0, 0, 0, 0, &res);
	if (res.a0 != FFA_SUCCESS)
		return -EOPNOTSUPP;

	switch (res.a2) {
	case FFA_FEAT_RXTX_MIN_SZ_4K:
		min_rxtx_sz = SZ_4K;
		break;
	case FFA_FEAT_RXTX_MIN_SZ_16K:
		min_rxtx_sz = SZ_16K;
		break;
	case FFA_FEAT_RXTX_MIN_SZ_64K:
		min_rxtx_sz = SZ_64K;
		break;
	default:
		return -EINVAL;
	}

	if (min_rxtx_sz > PAGE_SIZE)
		return -EOPNOTSUPP;

	return 0;
}

static void do_ffa_version(struct arm_smccc_res *res,
			   struct kvm_cpu_context *ctxt)
{
	DECLARE_REG(u32, ffa_req_version, ctxt, 1);

	if (FFA_MAJOR_VERSION(ffa_req_version) != 1) {
		res->a0 = FFA_RET_NOT_SUPPORTED;
		return;
	}

	hyp_spin_lock(&version_lock);
	if (has_version_negotiated) {
		if (FFA_MINOR_VERSION(ffa_req_version) < FFA_MINOR_VERSION(hyp_ffa_version))
			res->a0 = FFA_RET_NOT_SUPPORTED;
		else
			res->a0 = hyp_ffa_version;
		goto unlock;
	}

	/*
	 * If the client driver tries to downgrade the version, we need to ask
	 * first if TEE supports it.
	 */
	if (FFA_MINOR_VERSION(ffa_req_version) < FFA_MINOR_VERSION(hyp_ffa_version)) {
		arm_smccc_1_1_smc(FFA_VERSION, ffa_req_version, 0,
				  0, 0, 0, 0, 0,
				  res);
		if (res->a0 == FFA_RET_NOT_SUPPORTED)
			goto unlock;

		hyp_ffa_version = ffa_req_version;
	}

	if (hyp_ffa_post_init()) {
		res->a0 = FFA_RET_NOT_SUPPORTED;
	} else {
		/*
		 * Ensure that the write to `has_version_negotiated` is visible
		 * to other CPUs after the previous operations.
		 */
		smp_store_release(&has_version_negotiated, true);
		res->a0 = hyp_ffa_version;
	}
unlock:
	hyp_spin_unlock(&version_lock);
}

static void do_ffa_guest_version(struct arm_smccc_res *res, struct kvm_cpu_context *ctxt,
				 struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(u32, ffa_req_version, ctxt, 1);

	if (FFA_MAJOR_VERSION(ffa_req_version) != 1) {
		res->a0 = FFA_RET_NOT_SUPPORTED;
		return;
	}

	hyp_spin_lock(&version_lock);
	if (has_version_negotiated)
		res->a0 = hyp_ffa_version;
	else
		res->a0 = FFA_RET_NOT_SUPPORTED;
	hyp_spin_unlock(&version_lock);
}

static void do_ffa_part_get(struct arm_smccc_res *res,
			    struct kvm_cpu_context *ctxt,
			    struct pkvm_hyp_vcpu *hyp_vcpu)
{
	DECLARE_REG(u32, uuid0, ctxt, 1);
	DECLARE_REG(u32, uuid1, ctxt, 2);
	DECLARE_REG(u32, uuid2, ctxt, 3);
	DECLARE_REG(u32, uuid3, ctxt, 4);
	DECLARE_REG(u32, flags, ctxt, 5);
	struct kvm_ffa_buffers *ffa_buf;

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ffa_buf = ffa_get_buffers(hyp_vcpu);
	if (!ffa_buf->rx) {
		ffa_to_smccc_res(res, FFA_RET_BUSY);
		goto out_unlock;
	}

	do_ffa_part_get_response(res, uuid0, uuid1, uuid2, uuid3, flags, ffa_buf);
out_unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);
}

static void do_ffa_direct_msg(struct kvm_cpu_context *ctxt,
			      u64 vm_handle)
{
	DECLARE_REG(u32, endp, ctxt, 1);

	struct arm_smccc_1_2_regs *reg = (void *)&ctxt->regs.regs[0];

	if (FIELD_GET(FFA_SRC_ENDPOINT_MASK, endp) != vm_handle) {
		struct arm_smccc_res res;

		ffa_to_smccc_error(&res, FFA_RET_INVALID_PARAMETERS);
		ffa_set_retval(ctxt, &res);
		return;
	}

	__hyp_exit();
	arm_smccc_1_2_smc(reg, reg);
	__hyp_enter();
}

static int kvm_host_ffa_signal_availability(void)
{
	int ret;
	struct arm_smccc_res res;

	/*
	 * Map our hypervisor buffers into the SPMD before mapping and
	 * pinning the host buffers in our own address space.
	 */
	ret = ffa_map_hyp_buffers((KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE) / FFA_PAGE_SIZE);
	if (ret)
		return ffa_to_linux_errno(ret);

	do_ffa_part_get_response(&res, 0, 0, 0, 0, 0, NULL);
	if (res.a0 != FFA_SUCCESS)
		return ffa_to_linux_errno(ret);

	ffa_rx_release(&res);

	return kvm_notify_vm_availability(HOST_FFA_ID, &host_buffers, FFA_VM_CREATION_MSG);
}

bool kvm_host_ffa_handler(struct kvm_cpu_context *host_ctxt, u32 func_id)
{
	struct arm_smccc_res res;
	int ret;

	/*
	 * There's no way we can tell what a non-standard SMC call might
	 * be up to. Ideally, we would terminate these here and return
	 * an error to the host, but sadly devices make use of custom
	 * firmware calls for things like power management, debugging,
	 * RNG access and crash reporting.
	 *
	 * Given that the architecture requires us to trust EL3 anyway,
	 * we forward unrecognised calls on under the assumption that
	 * the firmware doesn't expose a mechanism to access arbitrary
	 * non-secure memory. Short of a per-device table of SMCs, this
	 * is the best we can do.
	 */
	if (!is_ffa_call(func_id))
		return false;

	if (func_id != FFA_VERSION &&
	    !smp_load_acquire(&has_version_negotiated)) {
		ffa_to_smccc_error(&res, FFA_RET_INVALID_PARAMETERS);
		goto out_handled;
	}

	/*
	 * Notify TZ of host VM creation immediately
	 * before handling the first non-version SMC/HVC
	 */
	if (func_id != FFA_VERSION && !has_host_signalled) {
		ret = kvm_host_ffa_signal_availability();
		if (!ret)
			/*
			 * Ensure that the write to `has_host_signalled` is visible
			 * to other CPUs after the previous operations.
			 */
			has_host_signalled = true;
		else if (ret == -EAGAIN || ret == -EINTR) {
			/*
			 * Don't retry with interrupts masked as we will spin
			 * forever.
			 */
			if (host_ctxt->regs.pstate & PSR_I_BIT) {
				ffa_to_smccc_error(&res, FFA_RET_DENIED);
				goto out_handled;
			}

			/* Go back to the host and replay the last instruction */
			write_sysreg_el2(read_sysreg_el2(SYS_ELR) - 4, SYS_ELR);
			return true;
		}
	}

	switch (func_id) {
	case FFA_FEATURES:
		if (!do_ffa_features(&res, host_ctxt))
			return false;
		goto out_handled;
	/* Memory management */
	case FFA_FN64_RXTX_MAP:
		do_ffa_rxtx_map(&res, host_ctxt, NULL);
		goto out_handled;
	case FFA_RXTX_UNMAP:
		do_ffa_rxtx_unmap(&res, host_ctxt, NULL);
		goto out_handled;
	case FFA_MEM_SHARE:
	case FFA_FN64_MEM_SHARE:
		do_ffa_mem_xfer(FFA_FN64_MEM_SHARE, &res, host_ctxt, NULL);
		goto out_handled;
	case FFA_MEM_RECLAIM:
		do_ffa_mem_reclaim(&res, host_ctxt, NULL);
		goto out_handled;
	case FFA_MEM_LEND:
	case FFA_FN64_MEM_LEND:
		do_ffa_mem_xfer(FFA_FN64_MEM_LEND, &res, host_ctxt, NULL);
		goto out_handled;
	case FFA_MEM_FRAG_TX:
		do_ffa_mem_frag_tx(&res, host_ctxt, NULL);
		goto out_handled;
	case FFA_VERSION:
		do_ffa_version(&res, host_ctxt);
		goto out_handled;
	case FFA_PARTITION_INFO_GET:
		do_ffa_part_get(&res, host_ctxt, NULL);
		goto out_handled;
	case FFA_RX_RELEASE:
		hyp_spin_lock(&kvm_ffa_hyp_lock);
		ffa_rx_release(&res);
		hyp_spin_unlock(&kvm_ffa_hyp_lock);
		goto out_handled;
	case FFA_ID_GET:
		ffa_to_smccc_res_prop(&res, FFA_RET_SUCCESS, HOST_FFA_ID);
		goto out_handled;
	case FFA_MSG_SEND_DIRECT_REQ:
	case FFA_FN64_MSG_SEND_DIRECT_REQ:
		do_ffa_direct_msg(host_ctxt, HOST_FFA_ID);
		return true;
	}

	if (ffa_call_supported(func_id))
		return false; /* Pass through */

	ffa_to_smccc_error(&res, FFA_RET_NOT_SUPPORTED);
out_handled:
	ffa_set_retval(host_ctxt, &res);
	return true;
}

bool kvm_guest_ffa_handler(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code)
{
	struct kvm_vcpu *vcpu = &hyp_vcpu->vcpu;
	struct kvm_cpu_context *ctxt = &vcpu->arch.ctxt;
	struct arm_smccc_res res;
	int ret, hyp_alloc_ret;
	struct kvm_hyp_req *req;

	DECLARE_REG(u64, func_id, ctxt, 0);

	if (!is_ffa_call(func_id)) {
		smccc_set_retval(vcpu, SMCCC_RET_NOT_SUPPORTED, 0, 0, 0);
		return true;
	}

	if (!VM_FFA_SUPPORTED(vcpu)) {
		ffa_to_smccc_error(&res, FFA_RET_NOT_SUPPORTED);
		ffa_set_retval(ctxt, &res);
		return true;
	}

	switch (func_id) {
	case FFA_FEATURES:
		do_ffa_guest_features(&res, ctxt);
		goto out_guest;
	case FFA_VERSION:
		do_ffa_guest_version(&res, ctxt, hyp_vcpu);
		goto out_guest;
	case FFA_FN64_RXTX_MAP:
		ret = do_ffa_rxtx_guest_map(ctxt, hyp_vcpu);
		break;
	case FFA_RXTX_UNMAP:
		do_ffa_rxtx_unmap(&res, ctxt, hyp_vcpu);
		goto out_guest;
	case FFA_MEM_RECLAIM:
		do_ffa_mem_reclaim(&res, ctxt, hyp_vcpu);
		goto out_guest;
	case FFA_MEM_SHARE:
	case FFA_FN64_MEM_SHARE:
		ret = do_ffa_mem_xfer(FFA_FN64_MEM_SHARE, &res, ctxt, hyp_vcpu);
		if (!ret)
			goto out_guest;
		break;
	case FFA_MEM_LEND:
	case FFA_FN64_MEM_LEND:
		ret = do_ffa_mem_xfer(FFA_FN64_MEM_LEND, &res, ctxt, hyp_vcpu);
		if (!ret)
			goto out_guest;
		break;
	case FFA_ID_GET:
		ffa_to_smccc_res_prop(&res, FFA_RET_SUCCESS, hyp_vcpu_to_ffa_handle(hyp_vcpu));
		goto out_guest;
	case FFA_PARTITION_INFO_GET:
		do_ffa_part_get(&res, ctxt, hyp_vcpu);
		goto out_guest;
	case FFA_RX_RELEASE:
		hyp_spin_lock(&kvm_ffa_hyp_lock);
		ffa_rx_release(&res);
		hyp_spin_unlock(&kvm_ffa_hyp_lock);
		goto out_guest;
	case FFA_MSG_SEND_DIRECT_REQ:
	case FFA_FN64_MSG_SEND_DIRECT_REQ:
		do_ffa_direct_msg(ctxt, hyp_vcpu_to_ffa_handle(hyp_vcpu));
		return true;
	default:
		ret = -EOPNOTSUPP;
		break;
	}

	if (ret == -EFAULT || ret == -ENOMEM) {
		hyp_alloc_ret = hyp_alloc_errno();
		if (hyp_alloc_ret == -ENOMEM) {
			req = pkvm_hyp_req_reserve(hyp_vcpu, KVM_HYP_REQ_TYPE_MEM);
			if (!req)
				goto out_guest_with_ret;

			req->mem.dest = REQ_MEM_DEST_HYP_ALLOC;
			req->mem.nr_pages = hyp_alloc_missing_donations();
		} else if (hyp_alloc_ret) {
			/* Nothing the host can do for us, let the HVC fail */
			ret = hyp_alloc_ret;
			goto out_guest_with_ret;
		}

		req = pkvm_hyp_req_reserve(hyp_vcpu, KVM_HYP_REQ_TYPE_MEM);
		if (!req)
			goto out_guest_with_ret;

		req->mem.dest = REQ_MEM_DEST_VCPU_MEMCACHE;
		req->mem.nr_pages = kvm_mmu_cache_min_pages(&hyp_vcpu->vcpu.kvm->arch.mmu);

		/* Go back to the host and replay the last guest instruction */
		write_sysreg_el2(read_sysreg_el2(SYS_ELR) - 4, SYS_ELR);
		*exit_code = ARM_EXCEPTION_HYP_REQ;
		return false;
	}

out_guest_with_ret:
	ffa_to_smccc_res(&res, linux_errno_to_ffa(ret));
out_guest:
	ffa_set_retval(ctxt, &res);
	return true;
}

static void kvm_guest_try_reclaim_transfer(struct ffa_mem_transfer *transfer,
					   struct pkvm_hyp_vm *vm)
{
	struct ffa_translation *translation, *tmp;
	struct arm_smccc_res res;

	ffa_mem_reclaim(&res, HANDLE_LOW(transfer->ffa_handle),
			HANDLE_HIGH(transfer->ffa_handle), 0);
	if (res.a0 != FFA_SUCCESS)
		return;

	list_for_each_entry_safe(translation, tmp, &transfer->translations, node) {
		WARN_ON(__pkvm_guest_unshare_ffa_page(vm->vcpus[0], translation->ipa));
		list_del(&translation->node);
		hyp_free(translation);
	}

	list_del(&transfer->node);
	hyp_free(transfer);
}

int kvm_dying_guest_reclaim_ffa_resources(struct pkvm_hyp_vm *vm)
{
	struct kvm_ffa_buffers *ffa_buf = &vm->ffa_buf;
	struct ffa_mem_transfer *transfer;
	int ret = 0;

	if (!vm->kvm.arch.pkvm.ffa_support)
		return 0;

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	if (!ffa_buf->tx && !ffa_buf->rx)
		goto unlock;

	if (list_empty(&ffa_buf->xfer_list)) {
		/* XXX - needs an explicit rxtx unmap call ? */
		if (ffa_buf->tx) {
			WARN_ON(__pkvm_guest_unshare_hyp_page(vm->vcpus[0], ffa_buf->tx_ipa));
			ffa_buf->tx = NULL;
		}
		if (ffa_buf->rx) {
			WARN_ON(__pkvm_guest_unshare_hyp_page(vm->vcpus[0], ffa_buf->rx_ipa));
			ffa_buf->rx = NULL;
		}
		goto unlock;
	}

	transfer = list_first_entry(&ffa_buf->xfer_list, typeof(*transfer), node);
	kvm_guest_try_reclaim_transfer(transfer, vm);
	ret = -EAGAIN;

unlock:
	hyp_spin_unlock(&kvm_ffa_hyp_lock);

	return ret;
}

int kvm_guest_notify_availability(u32 ffa_handle, struct kvm_ffa_buffers *ffa_buf, bool is_dying)
{
	int ret;

	hyp_spin_lock(&kvm_ffa_hyp_lock);
	ret = kvm_notify_vm_availability(ffa_handle, ffa_buf,
					 is_dying ? FFA_VM_DESTRUCTION_MSG : FFA_VM_CREATION_MSG);
	hyp_spin_unlock(&kvm_ffa_hyp_lock);

	return ret;
}

u32 ffa_get_hypervisor_version(void)
{
	u32 version = 0;

	hyp_spin_lock(&version_lock);
	if (has_version_negotiated)
		version = hyp_ffa_version;
	hyp_spin_unlock(&version_lock);

	return version;
}

int hyp_ffa_init(void *pages)
{
	struct arm_smccc_res res;
	void *tx, *rx;

	if (kvm_host_psci_config.smccc_version < ARM_SMCCC_VERSION_1_1)
		return 0;

	arm_smccc_1_1_smc(FFA_VERSION, FFA_VERSION_1_1, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == FFA_RET_NOT_SUPPORTED)
		return 0;

	/*
	 * Firmware returns the maximum supported version of the FF-A
	 * implementation. Check that the returned version is
	 * backwards-compatible with the hyp according to the rules in DEN0077A
	 * v1.1 REL0 13.2.1.
	 *
	 * Of course, things are never simple when dealing with firmware. v1.1
	 * broke ABI with v1.0 on several structures, which is itself
	 * incompatible with the aforementioned versioning scheme. The
	 * expectation is that v1.x implementations that do not support the v1.0
	 * ABI return NOT_SUPPORTED rather than a version number, according to
	 * DEN0077A v1.1 REL0 18.6.4.
	 */
	if (FFA_MAJOR_VERSION(res.a0) != 1)
		return -EOPNOTSUPP;

	if (FFA_MINOR_VERSION(res.a0) < FFA_MINOR_VERSION(FFA_VERSION_1_1))
		hyp_ffa_version = res.a0;
	else
		hyp_ffa_version = FFA_VERSION_1_1;

	tx = pages;
	pages += KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE;
	rx = pages;
	pages += KVM_FFA_MBOX_NR_PAGES * PAGE_SIZE;

	if (static_branch_unlikely(&kvm_ffa_unmap_on_lend)) {
		spm_handles = pages;
		pages += KVM_FFA_SPM_HANDLE_NR_PAGES * PAGE_SIZE;
		num_spm_handles = KVM_FFA_SPM_HANDLE_NR_PAGES * PAGE_SIZE /
			sizeof(struct ffa_handle);
		memset(spm_handles, -1, KVM_FFA_SPM_HANDLE_NR_PAGES * PAGE_SIZE);
	}

	ffa_desc_buf = (struct kvm_ffa_descriptor_buffer) {
		.buf	= pages,
		.len	= PAGE_SIZE *
			  (hyp_ffa_proxy_pages() - (2 * KVM_FFA_MBOX_NR_PAGES)),
	};

	hyp_buffers = (struct kvm_ffa_buffers) {
		.tx	= tx,
		.rx	= rx,
	};

	version_lock = __HYP_SPIN_LOCK_UNLOCKED;
	INIT_LIST_HEAD(&host_buffers.xfer_list);

	return 0;
}
