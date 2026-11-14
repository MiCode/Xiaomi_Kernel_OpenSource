// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 - Google LLC
 * Author: Quentin Perret <qperret@google.com>
 */

#include <linux/arm_ffa.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/interval_tree_generic.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/kmemleak.h>
#include <linux/kvm_host.h>
#include <asm/kvm_mmu.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/sort.h>

#include <asm/kvm_host.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pkvm.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/patching.h>
#include <asm/setup.h>

#include <kvm/device.h>

#include <linux/init_syscalls.h>
#include <uapi/linux/mount.h>

#include "hyp_constants.h"
#include "hyp_trace.h"

#define PKVM_DEVICE_ASSIGN_COMPAT	"pkvm,device-assignment"

/*
 * Retry the VM creation message for the host for a maximul total
 * amount of times, with sleeps in between. For the first few attempts,
 * do a faster reschedule instead of a full sleep.
 */
#define VM_AVAILABILITY_FAST_RETRIES	5
#define VM_AVAILABILITY_TOTAL_RETRIES	500
#define VM_AVAILABILITY_RETRY_SLEEP_MS	10

DEFINE_STATIC_KEY_FALSE(kvm_protected_mode_initialized);

static phys_addr_t pvmfw_base;
static phys_addr_t pvmfw_size;

static struct pkvm_moveable_reg *moveable_regs = kvm_nvhe_sym(pkvm_moveable_regs);
static struct memblock_region *hyp_memory = kvm_nvhe_sym(hyp_memory);
static unsigned int *hyp_memblock_nr_ptr = &kvm_nvhe_sym(hyp_memblock_nr);

phys_addr_t hyp_mem_base;
phys_addr_t hyp_mem_size;

extern struct pkvm_device *kvm_nvhe_sym(registered_devices);
extern u32 kvm_nvhe_sym(registered_devices_nr);

static int cmp_hyp_memblock(const void *p1, const void *p2)
{
	const struct memblock_region *r1 = p1;
	const struct memblock_region *r2 = p2;

	return r1->base < r2->base ? -1 : (r1->base > r2->base);
}

static void __init sort_memblock_regions(void)
{
	sort(hyp_memory,
	     *hyp_memblock_nr_ptr,
	     sizeof(struct memblock_region),
	     cmp_hyp_memblock,
	     NULL);
}

static int __init register_memblock_regions(void)
{
	struct memblock_region *reg;
	bool pvmfw_in_mem = false;

	for_each_mem_region(reg) {
		if (*hyp_memblock_nr_ptr >= HYP_MEMBLOCK_REGIONS)
			return -ENOMEM;

		hyp_memory[*hyp_memblock_nr_ptr] = *reg;
		(*hyp_memblock_nr_ptr)++;

		if (!pvmfw_size || pvmfw_in_mem ||
			!memblock_addrs_overlap(reg->base, reg->size, pvmfw_base, pvmfw_size))
			continue;
		/* If the pvmfw region overlaps a memblock, it must be a subset */
		if (pvmfw_base < reg->base || (pvmfw_base + pvmfw_size) > (reg->base + reg->size))
			return -EINVAL;
		pvmfw_in_mem = true;
	}

	if (pvmfw_size && !pvmfw_in_mem) {
		if (*hyp_memblock_nr_ptr >= HYP_MEMBLOCK_REGIONS)
			return -ENOMEM;

		hyp_memory[*hyp_memblock_nr_ptr] = (struct memblock_region) {
			.base   = pvmfw_base,
			.size   = pvmfw_size,
			.flags  = MEMBLOCK_NOMAP,
		};
		(*hyp_memblock_nr_ptr)++;
	}
	sort_memblock_regions();

	return 0;
}

static int cmp_moveable_reg(const void *p1, const void *p2)
{
	const struct pkvm_moveable_reg *r1 = p1;
	const struct pkvm_moveable_reg *r2 = p2;

	/*
	 * Moveable regions may overlap, so put the largest one first when start
	 * addresses are equal to allow a simpler walk from e.g.
	 * host_stage2_unmap_unmoveable_regs().
	 */
	if (r1->start < r2->start)
		return -1;
	else if (r1->start > r2->start)
		return 1;
	else if (r1->size > r2->size)
		return -1;
	else if (r1->size < r2->size)
		return 1;
	return 0;
}

static void __init sort_moveable_regs(void)
{
	sort(moveable_regs,
	     kvm_nvhe_sym(pkvm_moveable_regs_nr),
	     sizeof(struct pkvm_moveable_reg),
	     cmp_moveable_reg,
	     NULL);
}

static int __init register_moveable_fdt_resource(struct device_node *np,
						 enum pkvm_moveable_reg_type type)
{
	struct resource res;
	u64 start, size;
	unsigned int j = 0;
	unsigned int i = kvm_nvhe_sym(pkvm_moveable_regs_nr);

	while(!of_address_to_resource(np, j, &res)) {
		if (i >= PKVM_NR_MOVEABLE_REGS)
			return -ENOMEM;

		start = res.start;
		size = resource_size(&res);
		if (!PAGE_ALIGNED(start) || !PAGE_ALIGNED(size))
			return -EINVAL;

		moveable_regs[i].start = start;
		moveable_regs[i].size = size;
		moveable_regs[i].type = type;
		i++;
		j++;
	}

	kvm_nvhe_sym(pkvm_moveable_regs_nr) = i;
	return 0;
}

static int __init register_moveable_regions(void)
{
	struct memblock_region *reg;
	struct device_node *np;
	int i = 0, ret = 0, idx = 0;

	for_each_mem_region(reg) {
		if (i >= PKVM_NR_MOVEABLE_REGS)
			return -ENOMEM;
		moveable_regs[i].start = reg->base;
		moveable_regs[i].size = reg->size;
		moveable_regs[i].type = PKVM_MREG_MEMORY;
		i++;
	}
	kvm_nvhe_sym(pkvm_moveable_regs_nr) = i;

	for_each_compatible_node(np, NULL, "pkvm,protected-region") {
		ret = register_moveable_fdt_resource(np, PKVM_MREG_PROTECTED_RANGE);
		if (ret)
			goto out_fail;
	}

	for_each_compatible_node(np, NULL, PKVM_DEVICE_ASSIGN_COMPAT) {
		struct of_phandle_args args;

		while (!of_parse_phandle_with_fixed_args(np, "devices", 1, idx, &args)) {
			idx++;
			ret = register_moveable_fdt_resource(args.np, PKVM_MREG_ASSIGN_MMIO);
			of_node_put(args.np);
			if (ret)
				goto out_fail;
		}
	}

	sort_moveable_regs();

	return ret;
out_fail:
	of_node_put(np);
	kvm_nvhe_sym(pkvm_moveable_regs_nr) = 0;
	return ret;
}

static int __init early_hyp_lm_size_mb_cfg(char *arg)
{
	return kstrtoull(arg, 10, &kvm_nvhe_sym(hyp_lm_size_mb));
}
early_param("kvm-arm.hyp_lm_size_mb", early_hyp_lm_size_mb_cfg);

DEFINE_STATIC_KEY_FALSE(kvm_ffa_unmap_on_lend);

void __init kvm_hyp_reserve(void)
{
	u64 hyp_mem_pages = 0;
	int ret;

	if (!is_hyp_mode_available() || is_kernel_in_hyp_mode())
		return;

	if (kvm_get_mode() != KVM_MODE_PROTECTED)
		return;

	ret = register_memblock_regions();
	if (ret) {
		*hyp_memblock_nr_ptr = 0;
		kvm_err("Failed to register hyp memblocks: %d\n", ret);
		return;
	}

	ret = register_moveable_regions();
	if (ret) {
		*hyp_memblock_nr_ptr = 0;
		kvm_err("Failed to register pkvm moveable regions: %d\n", ret);
		return;
	}

	hyp_mem_pages += hyp_s1_pgtable_pages();
	hyp_mem_pages += host_s2_pgtable_pages();
	hyp_mem_pages += hyp_vm_table_pages();
	hyp_mem_pages += hyp_vmemmap_pages(STRUCT_HYP_PAGE_SIZE);
	hyp_mem_pages += pkvm_selftest_pages();
	hyp_mem_pages += hyp_ffa_proxy_pages();

	if (static_branch_unlikely(&kvm_ffa_unmap_on_lend))
		hyp_mem_pages += KVM_FFA_SPM_HANDLE_NR_PAGES;

	hyp_mem_pages++; /* hyp_ppages */

	/*
	 * Try to allocate a PMD-aligned region to reduce TLB pressure once
	 * this is unmapped from the host stage-2, and fallback to PAGE_SIZE.
	 */
	hyp_mem_size = hyp_mem_pages << PAGE_SHIFT;
	hyp_mem_base = memblock_phys_alloc(ALIGN(hyp_mem_size, PMD_SIZE),
					   PMD_SIZE);
	if (!hyp_mem_base)
		hyp_mem_base = memblock_phys_alloc(hyp_mem_size, PAGE_SIZE);
	else
		hyp_mem_size = ALIGN(hyp_mem_size, PMD_SIZE);

	if (!hyp_mem_base) {
		kvm_err("Failed to reserve hyp memory\n");
		return;
	}

	kvm_info("Reserved %lld MiB at 0x%llx\n", hyp_mem_size >> 20,
		 hyp_mem_base);
}


static void __pkvm_vcpu_hyp_created(struct kvm_vcpu *vcpu)
{
	if (kvm_vm_is_protected(vcpu->kvm))
		vcpu->arch.sve_state = NULL;

	vcpu_set_flag(vcpu, VCPU_PKVM_FINALIZED);
}

static int __pkvm_create_hyp_vcpu(struct kvm_vcpu *host_vcpu)
{
	pkvm_handle_t handle = host_vcpu->kvm->arch.pkvm.handle;
	struct kvm_hyp_req *hyp_reqs;
	int ret;

	init_hyp_stage2_memcache(&host_vcpu->arch.stage2_mc);

	hyp_reqs = (struct kvm_hyp_req *)__get_free_page(GFP_KERNEL_ACCOUNT);
	if (!hyp_reqs)
		return -ENOMEM;

	ret = kvm_share_hyp(hyp_reqs, hyp_reqs + 1);
	if (ret)
		goto err_free_reqs;
	host_vcpu->arch.hyp_reqs = hyp_reqs;

	ret = kvm_call_refill_hyp_nvhe(__pkvm_init_vcpu, handle, host_vcpu);
	if (!ret) {
		__pkvm_vcpu_hyp_created(host_vcpu);
		return 0;
	}

	kvm_unshare_hyp(hyp_reqs, hyp_reqs + 1);
err_free_reqs:
	free_page((unsigned long)hyp_reqs);
	host_vcpu->arch.hyp_reqs = NULL;

	return ret;
}

/*
 * Handle broken down huge pages which have not been reported to the
 * kvm_pinned_page.
 */
int pkvm_call_hyp_nvhe_ppage(struct kvm_pinned_page *ppage,
			     int (*call_hyp_nvhe)(u64 pfn, u64 gfn, u8 order, void* args),
			     void *args, bool unmap)
{
	size_t page_size, size = PAGE_SIZE << ppage->order;
	u64 pfn = page_to_pfn(ppage->page);
	u8 order = ppage->order;
	u64 gfn = ppage->ipa >> PAGE_SHIFT;

	/* We already know this huge-page has been broken down in the stage-2 */
	if (ppage->pins < (1 << order))
		order = 0;

	while (size) {
		int err = call_hyp_nvhe(pfn, gfn, order, args);

		switch (err) {
		/* The stage-2 huge page has been broken down */
		case -E2BIG:
			if (order)
				order = 0;
			else
				/* Something is really wrong ... */
				return -EINVAL;
			break;
		/* This has been unmapped already */
		case -ENOENT:
			/*
			 * We are not supposed to lose track of PAGE_SIZE pinned
			 * page.
			 */
			if (!ppage->order)
				return -EINVAL;

			fallthrough;
		case 0:
			page_size = PAGE_SIZE << order;
			gfn += 1 << order;
			pfn += 1 << order;

			if (page_size > size)
				return -EINVAL;

			/* If -ENOENT, the pin was already dropped. */
			if (unmap && !err)
				ppage->pins -= 1 << order;

			if (!ppage->pins)
				return 0;

			size -= page_size;
			break;
		default:
			return err;
		}
	}

	return 0;
}

static int __reclaim_dying_guest_page_call(u64 pfn, u64 gfn, u8 order, void *args)
{
	struct kvm *host_kvm = args;

	return kvm_call_hyp_nvhe(__pkvm_reclaim_dying_guest_page,
				 host_kvm->arch.pkvm.handle,
				 pfn, gfn, order);
}

/* __pkvm_notify_guest_vm_avail_retry - notify secure of the VM state change
 * @host_kvm: the kvm structure
 * @availability_msg: the VM state that will be notified
 *
 * Returns: 0 when the notification is sent with success, -EINTR or -EAGAIN if
 * the destruction notification is interrupted and retries exceeded and
 * a positive value indicating the remaining jiffies when the creation
 * notification is sent but interrupted.
 */
static int __pkvm_notify_guest_vm_avail_retry(struct kvm *host_kvm, u32 availability_msg)
{
	int ret, retries;
	long timeout;

	if (!host_kvm->arch.pkvm.ffa_support)
		return 0;

	for (retries = 0; retries < VM_AVAILABILITY_TOTAL_RETRIES; retries++) {
		ret = kvm_call_hyp_nvhe(__pkvm_notify_guest_vm_avail,
					host_kvm->arch.pkvm.handle);
		if (!ret)
			return 0;
		else if (ret != -EINTR && ret != -EAGAIN)
			return ret;

		if (retries < VM_AVAILABILITY_FAST_RETRIES) {
			cond_resched();
		} else if (availability_msg == FFA_VM_DESTRUCTION_MSG) {
			msleep(VM_AVAILABILITY_RETRY_SLEEP_MS);
		} else {
			timeout = msecs_to_jiffies(VM_AVAILABILITY_RETRY_SLEEP_MS);
			timeout = schedule_timeout_killable(timeout);
			if (timeout) {
				/*
				 * The timer did not expire,
				 * most likely because the
				 * process was killed.
				 */
				return ret;
			}
		}
	}

	return ret;
}

static void __pkvm_destroy_hyp_vm(struct kvm *host_kvm)
{
	struct mm_struct *mm = current->mm;
	struct kvm_pinned_page *ppage;
	struct kvm_vcpu *host_vcpu;
	unsigned long nr_busy;
	unsigned long pages;
	unsigned long idx;
	int ret, notify_status;

	if (!pkvm_is_hyp_created(host_kvm))
		goto out_free;

	WARN_ON(kvm_call_hyp_nvhe(__pkvm_start_teardown_vm, host_kvm->arch.pkvm.handle));

retry:
	pages = 0;
	nr_busy = 0;
	ppage = kvm_pinned_pages_iter_first(&host_kvm->arch.pkvm.pinned_pages, 0, ~(0UL));
	while (ppage) {
		struct kvm_pinned_page *next;
		u16 pins = ppage->pins;

		ret = pkvm_call_hyp_nvhe_ppage(ppage,
						 __reclaim_dying_guest_page_call,
						 host_kvm, true);
		cond_resched();
		if (ret == -EBUSY) {
			nr_busy++;
			next = kvm_pinned_pages_iter_next(ppage, 0, ~(0UL));
			ppage = next;
			continue;
		}
		WARN_ON(ret);

		unpin_user_pages_dirty_lock(&ppage->page, 1, true);
		next = kvm_pinned_pages_iter_next(ppage, 0, ~(0UL));
		kvm_pinned_pages_remove(ppage, &host_kvm->arch.pkvm.pinned_pages);
		pages += pins;
		kfree(ppage);
		ppage = next;
	}

	account_locked_vm(mm, pages, false);

	notify_status = __pkvm_notify_guest_vm_avail_retry(host_kvm, FFA_VM_DESTRUCTION_MSG);
	if (nr_busy) {
		do {
			ret = kvm_call_hyp_nvhe(__pkvm_reclaim_dying_guest_ffa_resources,
						host_kvm->arch.pkvm.handle);
			WARN_ON(ret && ret != -EAGAIN);

			if (notify_status == -EINTR || notify_status == -EAGAIN)
				notify_status = __pkvm_notify_guest_vm_avail_retry(
						host_kvm, FFA_VM_DESTRUCTION_MSG);
			cond_resched();
		} while (ret == -EAGAIN);
		goto retry;
	}

	WARN_ON(kvm_call_hyp_nvhe(__pkvm_finalize_teardown_vm, host_kvm->arch.pkvm.handle));

out_free:
	host_kvm->arch.pkvm.handle = 0;

	atomic64_sub(host_kvm->arch.pkvm.stage2_teardown_mc.nr_pages << PAGE_SHIFT,
		     &host_kvm->stat.protected_hyp_mem);
	free_hyp_memcache(&host_kvm->arch.pkvm.stage2_teardown_mc);

	kvm_iommu_guest_free_mc(&host_kvm->arch.pkvm.teardown_iommu_mc);

	kvm_for_each_vcpu(idx, host_vcpu, host_kvm) {
		struct kvm_hyp_req *hyp_reqs = host_vcpu->arch.hyp_reqs;

		if (!hyp_reqs)
			continue;

		kvm_unshare_hyp(hyp_reqs, hyp_reqs + 1);
		host_vcpu->arch.hyp_reqs = NULL;
		free_page((unsigned long)hyp_reqs);

		kvm_iommu_guest_free_mc(&host_vcpu->arch.iommu_mc);
	}
}

/*
 * Allocates and donates memory for hypervisor VM structs at EL2.
 *
 * Allocates space for the VM state, which includes the hyp vm as well as
 * the hyp vcpus.
 *
 * Stores an opaque handler in the kvm struct for future reference.
 *
 * Return 0 on success, negative error code on failure.
 */
static int __pkvm_create_hyp_vm(struct kvm *host_kvm)
{
	size_t pgd_sz;
	void *pgd;
	int ret;

	if (host_kvm->created_vcpus < 1)
		return -EINVAL;

	pgd_sz = kvm_pgtable_stage2_pgd_size(host_kvm->arch.mmu.vtcr);

	/*
	 * The PGD pages will be reclaimed using a hyp_memcache which implies
	 * page granularity. So, use alloc_pages_exact() to get individual
	 * refcounts.
	 */
	pgd = alloc_pages_exact(pgd_sz, GFP_KERNEL_ACCOUNT);
	if (!pgd)
		return -ENOMEM;
	atomic64_add(pgd_sz, &host_kvm->stat.protected_hyp_mem);

	init_hyp_stage2_memcache(&host_kvm->arch.pkvm.stage2_teardown_mc);

	/* Donate the VM memory to hyp and let hyp initialize it. */
	ret = kvm_call_refill_hyp_nvhe(__pkvm_init_vm, host_kvm, pgd);
	if (ret < 0)
		goto free_pgd;

	WRITE_ONCE(host_kvm->arch.pkvm.handle, ret);

	kvm_account_pgtable_pages(pgd, pgd_sz >> PAGE_SHIFT);

	return __pkvm_notify_guest_vm_avail_retry(host_kvm, FFA_VM_CREATION_MSG);
free_pgd:
	free_pages_exact(pgd, pgd_sz);
	atomic64_sub(pgd_sz, &host_kvm->stat.protected_hyp_mem);

	return ret;
}

bool pkvm_is_hyp_created(struct kvm *host_kvm)
{
	return READ_ONCE(host_kvm->arch.pkvm.handle);
}

int pkvm_create_hyp_vm(struct kvm *host_kvm)
{
	int ret = 0;

	mutex_lock(&host_kvm->arch.config_lock);
	if (!pkvm_is_hyp_created(host_kvm))
		ret = __pkvm_create_hyp_vm(host_kvm);
	mutex_unlock(&host_kvm->arch.config_lock);

	return ret;
}

int pkvm_create_hyp_vcpu(struct kvm_vcpu *vcpu)
{
	int ret = 0;

	mutex_lock(&vcpu->kvm->arch.config_lock);
	if (!vcpu_get_flag(vcpu, VCPU_PKVM_FINALIZED))
		ret = __pkvm_create_hyp_vcpu(vcpu);
	mutex_unlock(&vcpu->kvm->arch.config_lock);

	return ret;
}

void pkvm_destroy_hyp_vm(struct kvm *host_kvm)
{
	mutex_lock(&host_kvm->arch.config_lock);
	__pkvm_destroy_hyp_vm(host_kvm);
	mutex_unlock(&host_kvm->arch.config_lock);
}

int pkvm_init_host_vm(struct kvm *host_kvm, unsigned long type)
{
	if (!(type & KVM_VM_TYPE_ARM_PROTECTED))
		return 0;

	if (!is_protected_kvm_enabled())
		return -EINVAL;

	host_kvm->arch.pkvm.pvmfw_load_addr = PVMFW_INVALID_LOAD_ADDR;
	host_kvm->arch.pkvm.enabled = true;
	return 0;
}

static int pkvm_register_device(struct of_phandle_args *args,
				struct pkvm_device *dev)
{
	struct device_node *np = args->np;
	struct of_phandle_args iommu_spec;
	u32 group_id = args->args[0];
	struct resource res;
	u64 base, size, iommu_id;
	unsigned int j = 0;

	/* Parse regs */
	while (!of_address_to_resource(np, j, &res)) {
		if (j >= PKVM_DEVICE_MAX_RESOURCE)
			return -E2BIG;

		base = res.start;
		size = resource_size(&res);
		if (!PAGE_ALIGNED(base) || !PAGE_ALIGNED(size))
			return -EINVAL;

		dev->resources[j].base = base;
		dev->resources[j].size = size;
		j++;
	}
	dev->nr_resources = j;

	/* Parse iommus */
	j = 0;
	while (!of_parse_phandle_with_args(np, "iommus",
					   "#iommu-cells",
					   j, &iommu_spec)) {
		if (iommu_spec.args_count != 1) {
			kvm_err("[Devices] Unsupported binding for %s, expected <&iommu id>",
				np->full_name);
			return -EINVAL;
		}

		if (j >= PKVM_DEVICE_MAX_RESOURCE) {
			of_node_put(iommu_spec.np);
			return -E2BIG;
		}

		iommu_id = kvm_get_iommu_id_by_of(iommu_spec.np);

		dev->iommus[j].id = iommu_id;
		dev->iommus[j].endpoint = iommu_spec.args[0];
		of_node_put(iommu_spec.np);
		j++;
	}

	dev->nr_iommus = j;
	dev->ctxt = NULL;
	dev->group_id = group_id;

	return 0;
}

static int pkvm_init_devices(void)
{
	struct device_node *np;
	int idx = 0, ret = 0, dev_cnt = 0;
	size_t dev_sz;
	struct pkvm_device *dev_base;

	for_each_compatible_node (np, NULL, PKVM_DEVICE_ASSIGN_COMPAT) {
		struct of_phandle_args args;

		while (!of_parse_phandle_with_fixed_args(np, "devices", 1, dev_cnt, &args)) {
			dev_cnt++;
			of_node_put(args.np);
		}
	}
	kvm_info("Found %d assignable devices", dev_cnt);

	if (!dev_cnt)
		return 0;

	dev_sz = PAGE_ALIGN(size_mul(sizeof(struct pkvm_device), dev_cnt));

	dev_base = alloc_pages_exact(dev_sz, GFP_KERNEL_ACCOUNT);

	if (!dev_base)
		return -ENOMEM;

	for_each_compatible_node(np, NULL, PKVM_DEVICE_ASSIGN_COMPAT) {
		struct of_phandle_args args;

		while (!of_parse_phandle_with_fixed_args(np, "devices", 1, idx, &args)) {
			ret = pkvm_register_device(&args, &dev_base[idx]);
			of_node_put(args.np);
			if (ret) {
				of_node_put(np);
				goto out_free;
			}
			idx++;
		}
	}

	kvm_nvhe_sym(registered_devices_nr) = dev_cnt;
	kvm_nvhe_sym(registered_devices) = dev_base;
	return ret;

out_free:
	free_pages_exact(dev_base, dev_sz);
	return ret;
}

static void __init _kvm_host_prot_finalize(void *arg)
{
	int *err = arg;

	if (WARN_ON(kvm_call_hyp_nvhe(__pkvm_prot_finalize)))
		WRITE_ONCE(*err, -EINVAL);
}

static int __init pkvm_drop_host_privileges(void)
{
	int ret = 0;

	/*
	 * Flip the static key upfront as that may no longer be possible
	 * once the host stage 2 is installed.
	 */
	static_branch_enable(&kvm_protected_mode_initialized);
	on_each_cpu(_kvm_host_prot_finalize, &ret, 1);
	return ret;
}

static int __init pkvm_firmware_rmem_clear(void);

static int __init finalize_pkvm(void)
{
	int ret;

	if (!is_protected_kvm_enabled() || !is_kvm_arm_initialised()) {
		pkvm_firmware_rmem_clear();
		return 0;
	}

	/*
	 * Modules can play an essential part in the pKVM protection. All of
	 * them must properly load to enable protected VMs.
	 */
	if (pkvm_load_early_modules())
		pkvm_firmware_rmem_clear();

	ret = kvm_iommu_init_driver();
	if (ret) {
		pr_err("Failed to init KVM IOMMU driver: %d\n", ret);
		pkvm_firmware_rmem_clear();
	}

	ret = pkvm_init_devices();
	if (ret) {
		pr_err("Failed to init kvm devices %d\n", ret);
		pkvm_firmware_rmem_clear();
	}

	ret = kvm_call_hyp_nvhe(__pkvm_devices_init);
	if (ret)
		pr_warn("Assignable devices failed to initialize in the hypervisor %d", ret);

	/*
	 * Exclude HYP sections from kmemleak so that they don't get peeked
	 * at, which would end badly once inaccessible.
	 */
	kmemleak_free_part(__hyp_bss_start, __hyp_bss_end - __hyp_bss_start);
	kmemleak_free_part(__hyp_data_start, __hyp_data_end - __hyp_data_start);
	kmemleak_free_part(__hyp_rodata_start, __hyp_rodata_end - __hyp_rodata_start);
	kmemleak_free_part_phys(hyp_mem_base, hyp_mem_size);

	kvm_s2_ptdump_host_create_debugfs();

	ret = pkvm_drop_host_privileges();
	if (ret) {
		pr_err("Failed to finalize Hyp protection: %d\n", ret);
		kvm_iommu_remove_driver();
	}

	return 0;
}
device_initcall_sync(finalize_pkvm);

void pkvm_host_reclaim_page(struct kvm *host_kvm, phys_addr_t ipa)
{
	struct mm_struct *mm = current->mm;
	struct kvm_pinned_page *ppage;
	u16 pins;

	write_lock(&host_kvm->mmu_lock);
	ppage = kvm_pinned_pages_iter_first(&host_kvm->arch.pkvm.pinned_pages,
					   ipa, ipa + PAGE_SIZE - 1);
	if (ppage) {
		WARN_ON_ONCE(ppage->pins != 1);

		if (ppage->pins)
			ppage->pins--;

		pins = ppage->pins;
		if (!pins)
			kvm_pinned_pages_remove(ppage,
						&host_kvm->arch.pkvm.pinned_pages);
	}
	write_unlock(&host_kvm->mmu_lock);

	if (WARN_ON(!ppage) || pins)
		return;

	account_locked_vm(mm, 1 << ppage->order, false);
	unpin_user_pages_dirty_lock(&ppage->page, 1, true);
	kfree(ppage);
}

int pkvm_enable_smc_forwarding(struct file *kvm_file)
{
	struct kvm *host_kvm;

	if (!file_is_kvm(kvm_file))
		return -EINVAL;

	if (!kvm_get_kvm_safe(kvm_file->private_data))
		return -EINVAL;

	host_kvm = kvm_file->private_data;
	if (!host_kvm)
		return -EINVAL;

	host_kvm->arch.pkvm.smc_forwarded = true;

	return 0;
}

static int __init pkvm_firmware_rmem_err(struct reserved_mem *rmem,
					 const char *reason)
{
	phys_addr_t end = rmem->base + rmem->size;

	kvm_err("Ignoring pkvm guest firmware memory reservation [%pa - %pa]: %s\n",
		&rmem->base, &end, reason);
	return -EINVAL;
}

static int __init pkvm_firmware_rmem_init(struct reserved_mem *rmem)
{
	unsigned long node = rmem->fdt_node;

	if (pvmfw_size)
		return pkvm_firmware_rmem_err(rmem, "duplicate reservation");

	if (!of_get_flat_dt_prop(node, "no-map", NULL))
		return pkvm_firmware_rmem_err(rmem, "missing \"no-map\" property");

	if (of_get_flat_dt_prop(node, "reusable", NULL))
		return pkvm_firmware_rmem_err(rmem, "\"reusable\" property unsupported");

	if (!PAGE_ALIGNED(rmem->base))
		return pkvm_firmware_rmem_err(rmem, "base is not page-aligned");

	if (!PAGE_ALIGNED(rmem->size))
		return pkvm_firmware_rmem_err(rmem, "size is not page-aligned");

	pvmfw_size = kvm_nvhe_sym(pvmfw_size) = rmem->size;
	pvmfw_base = kvm_nvhe_sym(pvmfw_base) = rmem->base;
	return 0;
}
RESERVEDMEM_OF_DECLARE(pkvm_firmware, "linux,pkvm-guest-firmware-memory",
		       pkvm_firmware_rmem_init);

static int __init pkvm_firmware_rmem_clear(void)
{
	void *addr;
	phys_addr_t size;

	if (likely(!pvmfw_size))
		return 0;

	kvm_info("Clearing pKVM firmware memory\n");
	size = pvmfw_size;
	addr = memremap(pvmfw_base, size, MEMREMAP_WB);

	pvmfw_size = kvm_nvhe_sym(pvmfw_size) = 0;
	pvmfw_base = kvm_nvhe_sym(pvmfw_base) = 0;

	if (!addr)
		return -EINVAL;

	memset(addr, 0, size);
	dcache_clean_poc((unsigned long)addr, (unsigned long)addr + size);
	memunmap(addr);
	return 0;
}

static int pkvm_vm_ioctl_set_fw_ipa(struct kvm *kvm, u64 ipa)
{
	int ret = 0;

	if (!pvmfw_size)
		return -EINVAL;

	mutex_lock(&kvm->lock);
	if (kvm->arch.pkvm.handle) {
		ret = -EBUSY;
		goto out_unlock;
	}

	kvm->arch.pkvm.pvmfw_load_addr = ipa;
out_unlock:
	mutex_unlock(&kvm->lock);
	return ret;
}

static u32 pkvm_get_ffa_version(void)
{
	static u32 ffa_version;
	u32 ret;

	ret = READ_ONCE(ffa_version);
	if (ret)
		return ret;

	ret = kvm_call_hyp_nvhe(__pkvm_host_get_ffa_version);
	WRITE_ONCE(ffa_version, ret);
	return ret;

}

static int pkvm_vm_ioctl_info(struct kvm *kvm,
			      struct kvm_protected_vm_info __user *info)
{
	struct kvm_protected_vm_info kinfo = {
		.firmware_size = pvmfw_size,
		.ffa_version = pkvm_get_ffa_version(),
	};

	return copy_to_user(info, &kinfo, sizeof(kinfo)) ? -EFAULT : 0;
}

static int pkvm_vm_ioctl_ffa_support(struct kvm *kvm, u32 enable)
{
	int ret = 0;
	u32 ffa_version;

	/* Restrict userspace from having an IPC channel over FF-A with secure */
	if (!capable(CAP_IPC_OWNER))
		return -EPERM;

	/*
	 * If the host hasn't negotiated a version don't enable the
	 * FF-A capability.
	 */
	ffa_version = pkvm_get_ffa_version();
	if (!ffa_version)
		return -EINVAL;

	mutex_lock(&kvm->arch.config_lock);
	if (kvm->arch.pkvm.handle) {
		ret = -EBUSY;
		goto out_unlock;
	}

	kvm->arch.pkvm.ffa_support = enable;
out_unlock:
	mutex_unlock(&kvm->arch.config_lock);
	return ret;
}

int pkvm_vm_ioctl_enable_cap(struct kvm *kvm, struct kvm_enable_cap *cap)
{
	if (!kvm_vm_is_protected(kvm))
		return -EINVAL;

	if (cap->args[1] || cap->args[2] || cap->args[3])
		return -EINVAL;

	switch (cap->flags) {
	case KVM_CAP_ARM_PROTECTED_VM_FLAGS_SET_FW_IPA:
		return pkvm_vm_ioctl_set_fw_ipa(kvm, cap->args[0]);
	case KVM_CAP_ARM_PROTECTED_VM_FLAGS_INFO:
		return pkvm_vm_ioctl_info(kvm, (void __force __user *)cap->args[0]);
	case KVM_CAP_ARM_PROTECTED_VM_FLAGS_SET_FFA:
		return pkvm_vm_ioctl_ffa_support(kvm, cap->args[0]);
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_MODULES
static char early_pkvm_modules[COMMAND_LINE_SIZE] __initdata;

static int __init early_pkvm_modules_cfg(char *arg)
{
	/*
	 * Loading pKVM modules with kvm-arm.protected_modules is deprecated
	 * Use kvm-arm.protected_modules=<module1>,<module2>
	 */
	if (!arg)
		return -EINVAL;

	strscpy(early_pkvm_modules, arg, COMMAND_LINE_SIZE);

	return 0;
}
early_param("kvm-arm.protected_modules", early_pkvm_modules_cfg);

static void __init free_modprobe_argv(struct subprocess_info *info)
{
	kfree(info->argv);
}

static int __init init_modprobe(struct subprocess_info *info, struct cred *new)
{
	struct file *file = filp_open("/dev/kmsg", O_RDWR, 0);

	if (IS_ERR(file)) {
		pr_warn("Warning: unable to open /dev/kmsg, modprobe will be silent.\n");
		return 0;
	}

	init_dup(file);
	init_dup(file);
	init_dup(file);
	fput(file);

	return 0;
}

/*
 * Heavily inspired by request_module(). The latest couldn't be reused though as
 * the feature can be disabled depending on umh configuration. Here some
 * security is enforced by making sure this can be called only when pKVM is
 * enabled, not yet completely initialized.
 */
static int __init __pkvm_request_early_module(char *module_name,
					      char *module_path)
{
	char *modprobe_path = CONFIG_MODPROBE_PATH;
	struct subprocess_info *info;
	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
		NULL
	};
	static bool proc;
	char **argv;
	int idx = 0;

	if (!is_protected_kvm_enabled())
		return -EACCES;

	if (static_branch_likely(&kvm_protected_mode_initialized))
		return -EACCES;

	argv = kmalloc(sizeof(char *) * 7, GFP_KERNEL);
	if (!argv)
		return -ENOMEM;

	argv[idx++] = modprobe_path;
	argv[idx++] = "-q";
	if (*module_path != '\0') {
		argv[idx++] = "-d";
		argv[idx++] = module_path;
	}
	argv[idx++] = "--";
	argv[idx++] = module_name;
	argv[idx++] = NULL;

	info = call_usermodehelper_setup(modprobe_path, argv, envp, GFP_KERNEL,
					 init_modprobe, free_modprobe_argv, NULL);
	if (!info)
		goto err;

	/* Even with CONFIG_STATIC_USERMODEHELPER we really want this path */
	info->path = modprobe_path;

	if (!proc) {
		wait_for_initramfs();
		if (init_mount("proc", "/proc", "proc",
			       MS_SILENT | MS_NOEXEC | MS_NOSUID, NULL))
			pr_warn("Couldn't mount /proc, pKVM module parameters will be ignored\n");

		proc = true;
	}

	return call_usermodehelper_exec(info, UMH_WAIT_PROC | UMH_KILLABLE);
err:
	kfree(argv);

	return -ENOMEM;
}

static int __init pkvm_request_early_module(char *module_name, char *module_path)
{
	int err = __pkvm_request_early_module(module_name, module_path);

	if (!err)
		return 0;

	/* Already tried the default path */
	if (*module_path == '\0')
		return err;

	pr_info("loading %s from %s failed, fallback to the default path\n",
		module_name, module_path);

	return __pkvm_request_early_module(module_name, "");
}

static void pkvm_el2_mod_free(void);

int __init pkvm_load_early_modules(void)
{
	char *token, *buf = early_pkvm_modules;
	char *module_path = CONFIG_PKVM_MODULE_PATH;
	int err = 0;

	while (true) {
		token = strsep(&buf, ",");

		if (!token)
			break;

		if (*token) {
			err = pkvm_request_early_module(token, module_path);
			if (err) {
				pr_err("Failed to load pkvm module %s: %d\n",
				       token, err);
				goto out;
			}
		}

		if (buf)
			*(buf - 1) = ',';
	}

out:
	pkvm_el2_mod_free();

	return err;
}

static LIST_HEAD(pkvm_modules);

static void pkvm_el2_mod_add(struct pkvm_el2_module *mod)
{
	INIT_LIST_HEAD(&mod->node);
	list_add(&mod->node, &pkvm_modules);
}

static void pkvm_el2_mod_free(void)
{
	struct pkvm_el2_sym *sym, *tmp;
	struct pkvm_el2_module *mod;

	list_for_each_entry(mod, &pkvm_modules, node) {
		list_for_each_entry_safe(sym, tmp, &mod->ext_symbols, node) {
			list_del(&sym->node);
			kfree(sym->name);
			kfree(sym);
		}
	}
}

static struct module *pkvm_el2_mod_to_module(struct pkvm_el2_module *hyp_mod)
{
	struct mod_arch_specific *arch;

	arch = container_of(hyp_mod, struct mod_arch_specific, hyp);
	return container_of(arch, struct module, arch);
}

#ifdef CONFIG_PKVM_STACKTRACE
unsigned long pkvm_el2_mod_kern_va(unsigned long addr)
{
	struct pkvm_el2_module *mod;

	list_for_each_entry(mod, &pkvm_modules, node) {
		unsigned long hyp_va = (unsigned long)mod->hyp_va;
		size_t len = (unsigned long)mod->sections.end -
			     (unsigned long)mod->sections.start;

		if (addr >= hyp_va && addr < (hyp_va + len))
			return (unsigned long)mod->sections.start +
				(addr - hyp_va);
	}

	return 0;
}
#else
unsigned long pkvm_el2_mod_kern_va(unsigned long addr) { return 0; }
#endif

static struct pkvm_el2_module *pkvm_el2_mod_lookup_symbol(const char *name,
							  unsigned long *addr)
{
	struct pkvm_el2_module *hyp_mod;
	unsigned long __addr;

	list_for_each_entry(hyp_mod, &pkvm_modules, node) {
		struct module *mod = pkvm_el2_mod_to_module(hyp_mod);

		__addr = find_kallsyms_symbol_value(mod, name);
		if (!__addr)
			continue;

		*addr = __addr;
		return hyp_mod;
	}

	return NULL;
}

static bool within_pkvm_module_section(struct pkvm_module_section *section,
				       unsigned long addr)
{
	return (addr > (unsigned long)section->start) &&
		(addr < (unsigned long)section->end);
}

static int pkvm_reloc_imported_symbol(struct pkvm_el2_module *importer,
				      struct pkvm_el2_sym *sym,
				      unsigned long hyp_dst)
{
	s64 val, val_max = (s64)(~(BIT(25) - 1)) << 2;
	u32 insn = le32_to_cpu(*sym->rela_pos);
	unsigned long hyp_src;
	u64 imm;

	if (!within_pkvm_module_section(&importer->text,
					(unsigned long)sym->rela_pos))
		return -EINVAL;

	hyp_src = (unsigned long)importer->hyp_va +
		((void *)sym->rela_pos - importer->text.start);

	/*
	 * Module hyp VAs are allocated going upward. Source MUST have a
	 * lower address than the destination
	 */
	if (WARN_ON(hyp_src < hyp_dst))
		return -EINVAL;

	val = hyp_dst - hyp_src;
	if (val < val_max) {
		pr_warn("Exported symbol %s is too far for the relocation in module %s\n",
			sym->name, pkvm_el2_mod_to_module(importer)->name);
		return -ERANGE;
	}

	/* offset encoded as imm26 * 4 */
	imm = (val >> 2) & (BIT(26) - 1);

	insn = aarch64_insn_encode_immediate(AARCH64_INSN_IMM_26, insn, imm);

	return aarch64_insn_patch_text_nosync((void *)sym->rela_pos, insn);
}

static int pkvm_reloc_imported_symbols(struct pkvm_el2_module *importer)
{
	unsigned long addr, offset, hyp_addr;
	struct pkvm_el2_module *exporter;
	struct pkvm_el2_sym *sym;

	list_for_each_entry(sym, &importer->ext_symbols, node) {
		exporter = pkvm_el2_mod_lookup_symbol(sym->name, &addr);
		if (!exporter) {
			pr_warn("pKVM symbol %s not exported by any module\n",
				sym->name);
			return -EINVAL;
		}

		if (!within_pkvm_module_section(&exporter->text, addr)) {
			pr_warn("pKVM symbol %s not part of %s .text section\n",
				sym->name,
				pkvm_el2_mod_to_module(exporter)->name);
			return -EINVAL;
		}

		/* hyp addr in the exporter */
		offset = addr - (unsigned long)exporter->text.start;
		hyp_addr = (unsigned long)exporter->hyp_va + offset;

		pkvm_reloc_imported_symbol(importer, sym, hyp_addr);
	}

	return 0;
}

struct pkvm_mod_sec_mapping {
	struct pkvm_module_section *sec;
	enum kvm_pgtable_prot prot;
};

static void pkvm_unmap_module_pages(void *kern_va, void *hyp_va, size_t size)
{
	size_t offset;
	u64 pfn;

	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		pfn = vmalloc_to_pfn(kern_va + offset);
		kvm_call_hyp_nvhe(__pkvm_unmap_module_page, pfn,
				  hyp_va + offset);
	}
}

static void pkvm_unmap_module_sections(struct pkvm_mod_sec_mapping *secs_map, void *hyp_va_base, int nr_secs)
{
	size_t offset, size;
	void *start;
	int i;

	for (i = 0; i < nr_secs; i++) {
		start = secs_map[i].sec->start;
		size = secs_map[i].sec->end - start;
		offset = start - secs_map[0].sec->start;
		pkvm_unmap_module_pages(start, hyp_va_base + offset, size);
	}
}

static int pkvm_map_module_section(struct pkvm_mod_sec_mapping *sec_map, void *hyp_va)
{
	size_t offset, size = sec_map->sec->end - sec_map->sec->start;
	int ret;
	u64 pfn;

	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		pfn = vmalloc_to_pfn(sec_map->sec->start + offset);
		ret = kvm_call_hyp_nvhe(__pkvm_map_module_page, pfn,
					hyp_va + offset, sec_map->prot);
		if (ret) {
			pkvm_unmap_module_pages(sec_map->sec->start, hyp_va, offset);
			return ret;
		}
	}

	return 0;
}

static int pkvm_map_module_sections(struct pkvm_mod_sec_mapping *secs_map,
				    void *hyp_va_base, int nr_secs)
{
	size_t offset;
	int i, ret;

	for (i = 0; i < nr_secs; i++) {
		offset = secs_map[i].sec->start - secs_map[0].sec->start;
		ret = pkvm_map_module_section(&secs_map[i], hyp_va_base + offset);
		if (ret) {
			pkvm_unmap_module_sections(secs_map, hyp_va_base, i);
			return ret;
		}
	}

	return 0;
}

static int __pkvm_cmp_mod_sec(const void *p1, const void *p2)
{
	struct pkvm_mod_sec_mapping const *s1 = p1;
	struct pkvm_mod_sec_mapping const *s2 = p2;

	return s1->sec->start < s2->sec->start ? -1 : s1->sec->start > s2->sec->start;
}

static void *pkvm_map_module_struct(struct pkvm_el2_module *mod)
{
	void *addr = (void *)__get_free_page(GFP_KERNEL);

	if (!addr)
		return NULL;

	if (kvm_share_hyp(addr, addr + PAGE_SIZE)) {
		free_page((unsigned long)addr);
		return NULL;
	}

	/*
	 * pkvm_el2_module being stored in vmalloc we can't guarantee a
	 * linear map for the hypervisor to rely on. Copy the struct instead.
	 */
	memcpy(addr, mod, sizeof(*mod));

	return addr;
}

static void pkvm_unmap_module_struct(void *addr)
{
	kvm_unshare_hyp(addr, addr + PAGE_SIZE);
	free_page((unsigned long)addr);
}

static void pkvm_module_kmemleak(struct module *this,
				 struct pkvm_mod_sec_mapping *sec_map,
				 int nr_sections)
{
	void *start, *end;
	int i;

	if (!this)
		return;

	/*
	 * The module loader already removes read-only sections from kmemleak
	 * scanned objects. However, few hyp sections are installed into
	 * MOD_DATA. Skip those sections before they are made inaccessible from
	 * the host.
	 */

	start = this->mem[MOD_DATA].base;
	end = start + this->mem[MOD_DATA].size;

	for (i = 0; i < nr_sections; i++, sec_map++) {
		if (sec_map->sec->start < start || sec_map->sec->start >= end)
			continue;

		kmemleak_scan_area(start, sec_map->sec->start - start, GFP_KERNEL);
		start = sec_map->sec->end;
	}

	kmemleak_scan_area(start, end - start, GFP_KERNEL);
}

int __pkvm_load_el2_module(struct module *this, unsigned long *token)
{
	struct pkvm_el2_module *mod = &this->arch.hyp;
	struct pkvm_mod_sec_mapping secs_map[] = {
		{ &mod->text, KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_X },
		{ &mod->bss, KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_W },
		{ &mod->rodata, KVM_PGTABLE_PROT_R },
		{ &mod->event_ids, KVM_PGTABLE_PROT_R },
		{ &mod->patchable_function_entries, KVM_PGTABLE_PROT_R },
		{ &mod->data, KVM_PGTABLE_PROT_R | KVM_PGTABLE_PROT_W },
	};
	void *start, *end, *hyp_va, *mod_remap;
	struct arm_smccc_res res;
	kvm_nvhe_reloc_t *endrel;
	int ret, i, secs_first;
	size_t size;

	/* The pKVM hyp only allows loading before it is fully initialized */
	if (!is_protected_kvm_enabled() || is_pkvm_initialized())
		return -EOPNOTSUPP;

	for (i = 0; i < ARRAY_SIZE(secs_map); i++) {
		if (!PAGE_ALIGNED(secs_map[i].sec->start)) {
			kvm_err("EL2 sections are not page-aligned\n");
			return -EINVAL;
		}
	}

	if (!try_module_get(this)) {
		kvm_err("Kernel module has been unloaded\n");
		return -ENODEV;
	}

	/* Missing or empty module sections are placed first */
	sort(secs_map, ARRAY_SIZE(secs_map), sizeof(secs_map[0]), __pkvm_cmp_mod_sec, NULL);
	for (secs_first = 0; secs_first < ARRAY_SIZE(secs_map); secs_first++) {
		start = secs_map[secs_first].sec->start;
		if (start)
			break;
	}
	end = secs_map[ARRAY_SIZE(secs_map) - 1].sec->end;
	size = end - start;

	mod->sections.start = start;
	mod->sections.end = end;

	arm_smccc_1_1_hvc(KVM_HOST_SMCCC_FUNC(__pkvm_alloc_module_va),
			  size >> PAGE_SHIFT, &res);
	if (res.a0 != SMCCC_RET_SUCCESS || !res.a1) {
		kvm_err("Failed to allocate hypervisor VA space for EL2 module\n");
		module_put(this);
		return res.a0 == SMCCC_RET_SUCCESS ? -ENOMEM : -EPERM;
	}
	hyp_va = (void *)res.a1;
	mod->hyp_va = hyp_va;

	/*
	 * The token can be used for other calls related to this module.
	 * Conveniently the only information needed is this addr so let's use it
	 * as an identifier.
	 */
	if (token)
		*token = (unsigned long)hyp_va;

	mod->sections.start = start;
	mod->sections.end = end;

	endrel = (void *)mod->relocs + mod->nr_relocs * sizeof(*endrel);
	kvm_apply_hyp_module_relocations(mod, mod->relocs, endrel);

	ret = pkvm_reloc_imported_symbols(mod);
	if (ret)
		return ret;

	pkvm_module_kmemleak(this, secs_map, ARRAY_SIZE(secs_map));

	ret = hyp_trace_init_mod_events(mod);
	if (ret)
		kvm_err("Failed to init module events: %d\n", ret);

	mod_remap = pkvm_map_module_struct(mod);
	if (!mod_remap) {
		module_put(this);
		return -ENOMEM;
	}

	ret = pkvm_map_module_sections(secs_map + secs_first, hyp_va,
				       ARRAY_SIZE(secs_map) - secs_first);
	if (ret) {
		kvm_err("Failed to map EL2 module page: %d\n", ret);
		pkvm_unmap_module_struct(mod_remap);
		module_put(this);
		return ret;
	}

	pkvm_el2_mod_add(mod);

	ret = kvm_call_hyp_nvhe(__pkvm_init_module, mod_remap);
	pkvm_unmap_module_struct(mod_remap);
	if (ret) {
		kvm_err("Failed to init EL2 module: %d\n", ret);
		list_del(&mod->node);
		pkvm_unmap_module_sections(secs_map, hyp_va, ARRAY_SIZE(secs_map));
		module_put(this);
		return ret;
	}

	hyp_trace_enable_event_early();

	return 0;
}
EXPORT_SYMBOL(__pkvm_load_el2_module);

int __pkvm_register_el2_call(unsigned long hfn_hyp_va)
{
	return kvm_call_hyp_nvhe(__pkvm_register_hcall, hfn_hyp_va);
}
EXPORT_SYMBOL(__pkvm_register_el2_call);

void pkvm_el2_mod_frob_sections(Elf_Ehdr *ehdr, Elf_Shdr *sechdrs, char *secstrings)
{
#ifdef CONFIG_PKVM_FTRACE
	int i;

	for (i = 0; i < ehdr->e_shnum; i++) {
		if (!strcmp(secstrings + sechdrs[i].sh_name, ".hyp.text")) {
			Elf_Shdr *hyp_text = sechdrs + i;

			/* .hyp.text.ftrace_tramp pollutes .hyp.text flags */
			hyp_text->sh_flags = SHF_EXECINSTR | SHF_ALLOC;
			break;
		}
	}
#endif
}
#endif /* CONFIG_MODULES */

int __pkvm_topup_hyp_alloc(unsigned long nr_pages)
{
	struct kvm_hyp_memcache mc;
	int ret;

	init_hyp_memcache(&mc);

	ret = topup_hyp_memcache(&mc, nr_pages, 0);
	if (ret)
		return ret;

	ret = kvm_call_hyp_nvhe(__pkvm_hyp_alloc_mgt_refill, HYP_ALLOC_MGT_HEAP_ID,
				mc.head, mc.nr_pages);
	if (ret)
		free_hyp_memcache(&mc);

	return ret;
}
EXPORT_SYMBOL(__pkvm_topup_hyp_alloc);

unsigned long __pkvm_reclaim_hyp_alloc_mgt(unsigned long nr_pages)
{
	unsigned long ratelimit, last_reclaim, reclaimed = 0;
	struct kvm_hyp_memcache mc;
	struct arm_smccc_res res;

	init_hyp_memcache(&mc);

	do {
		/* Arbitrary upper bound to limit the time spent at EL2 */
		ratelimit = min(nr_pages, 16UL);

		arm_smccc_1_1_hvc(KVM_HOST_SMCCC_FUNC(__pkvm_hyp_alloc_mgt_reclaim),
				  ratelimit, &res);
		if (WARN_ON(res.a0 != SMCCC_RET_SUCCESS))
			break;

		mc.head = res.a1;
		last_reclaim = mc.nr_pages = res.a2;

		free_hyp_memcache(&mc);
		reclaimed += last_reclaim;

	} while (last_reclaim && (reclaimed < nr_pages));

	return reclaimed;
}

int __pkvm_topup_hyp_alloc_mgt_gfp(unsigned long id, unsigned long nr_pages,
				   unsigned long sz_alloc, gfp_t gfp)
{
	struct kvm_hyp_memcache mc;
	int ret;

	init_hyp_memcache(&mc);

	ret = topup_hyp_memcache_gfp(&mc, nr_pages, get_order(sz_alloc), gfp);
	if (ret)
		return ret;

	ret = kvm_call_hyp_nvhe(__pkvm_hyp_alloc_mgt_refill, id,
				mc.head, mc.nr_pages);
	if (ret)
		free_hyp_memcache(&mc);

	return ret;
}
EXPORT_SYMBOL(__pkvm_topup_hyp_alloc_mgt_gfp);

static int __pkvm_donate_resource(struct resource *r)
{
	if (!PAGE_ALIGNED(resource_size(r)) || !PAGE_ALIGNED(r->start))
		return -EINVAL;

	return kvm_call_hyp_nvhe(__pkvm_host_donate_hyp_mmio,
				 __phys_to_pfn(r->start),
				 resource_size(r) >> PAGE_SHIFT);

}

static int __pkvm_reclaim_resource(struct resource *r)
{
	if (!PAGE_ALIGNED(resource_size(r)) || !PAGE_ALIGNED(r->start))
		return -EINVAL;

	return kvm_call_hyp_nvhe(__pkvm_host_reclaim_hyp_mmio,
				 __phys_to_pfn(r->start),
				 resource_size(r) >> PAGE_SHIFT);
}

static int __pkvm_arch_assign_device(struct device *dev, void *data)
{
	struct platform_device *pdev;
	struct resource *r;
	int index = 0;
	int ret = 0;

	if (!dev_is_platform(dev))
		return -EOPNOTSUPP;

	pdev = to_platform_device(dev);

	while ((r = platform_get_resource(pdev, IORESOURCE_MEM, index++))) {
		ret = __pkvm_donate_resource(r);
		if (ret)
			break;
	}

	if (ret) {
		while (index--) {
			r = platform_get_resource(pdev, IORESOURCE_MEM, index);
			__pkvm_reclaim_resource(r);
		}
	}
	return ret;
}

static int __pkvm_arch_reclaim_device(struct device *dev, void *data)
{
	struct platform_device *pdev;
	struct resource *r;
	int index = 0;

	pdev = to_platform_device(dev);

	while ((r = platform_get_resource(pdev, IORESOURCE_MEM, index++)))
		__pkvm_reclaim_resource(r);

	return 0;
}

int kvm_arch_assign_device(struct device *dev)
{
	if (!is_protected_kvm_enabled())
		return 0;

	return __pkvm_arch_assign_device(dev, NULL);
}

int kvm_arch_assign_group(struct iommu_group *group)
{
	int ret;

	if (!is_protected_kvm_enabled())
		return 0;

	ret = iommu_group_for_each_dev(group, NULL, __pkvm_arch_assign_device);

	if (ret)
		iommu_group_for_each_dev(group, NULL, __pkvm_arch_reclaim_device);

	return ret;
}

void kvm_arch_reclaim_device(struct device *dev)
{
	if (!is_protected_kvm_enabled())
		return;

	__pkvm_arch_reclaim_device(dev, NULL);
}

void kvm_arch_reclaim_group(struct iommu_group *group)
{
	if (!is_protected_kvm_enabled())
		return;

	iommu_group_for_each_dev(group, NULL, __pkvm_arch_reclaim_device);
}

static u64 __pkvm_mapping_start(struct pkvm_mapping *m)
{
	return m->gfn * PAGE_SIZE;
}

static u64 __pkvm_mapping_end(struct pkvm_mapping *m)
{
	return (m->gfn + m->nr_pages) * PAGE_SIZE - 1;
}

INTERVAL_TREE_DEFINE(struct pkvm_mapping, node, u64, __subtree_last,
		__pkvm_mapping_start, __pkvm_mapping_end, static,
		pkvm_mapping);

#define for_each_mapping_in_range_safe(__pgt, __start, __end, __map)				\
	for (struct pkvm_mapping *__tmp = pkvm_mapping_iter_first(&(__pgt)->pkvm_mappings,	\
								  __start, __end - 1);		\
	     __tmp && ({									\
				__map = __tmp;							\
				__tmp = pkvm_mapping_iter_next(__map, __start, __end - 1);	\
				true;								\
		       });									\
	    )

int pkvm_pgtable_stage2_init(struct kvm_pgtable *pgt, struct kvm_s2_mmu *mmu,
			     struct kvm_pgtable_mm_ops *mm_ops, struct kvm_pgtable_pte_ops *pte_ops)
{
	pgt->pkvm_mappings	= RB_ROOT_CACHED;
	pgt->mmu		= mmu;

	return 0;
}

static int __pkvm_pgtable_stage2_unmap(struct kvm_pgtable *pgt, u64 start, u64 end)
{
	struct kvm *kvm = kvm_s2_mmu_to_kvm(pgt->mmu);
	pkvm_handle_t handle = kvm->arch.pkvm.handle;
	struct pkvm_mapping *mapping;
	int ret;

	if (!handle)
		return 0;

	for_each_mapping_in_range_safe(pgt, start, end, mapping) {
		ret = kvm_call_hyp_nvhe(__pkvm_host_unshare_guest, handle, mapping->gfn,
					mapping->nr_pages);
		if (WARN_ON(ret))
			return ret;
		pkvm_mapping_remove(mapping, &pgt->pkvm_mappings);
		kfree(mapping);
	}

	return 0;
}

void pkvm_pgtable_stage2_destroy(struct kvm_pgtable *pgt)
{
	__pkvm_pgtable_stage2_unmap(pgt, 0, ~(0ULL));
}

int pkvm_pgtable_stage2_map(struct kvm_pgtable *pgt, u64 addr, u64 size,
			   u64 phys, enum kvm_pgtable_prot prot,
			   void *mc, enum kvm_pgtable_walk_flags flags)
{
	struct kvm *kvm = kvm_s2_mmu_to_kvm(pgt->mmu);
	struct pkvm_mapping *mapping = NULL;
	struct kvm_hyp_memcache *cache = mc;
	u64 gfn = addr >> PAGE_SHIFT;
	u64 pfn = phys >> PAGE_SHIFT;
	int ret;

	if (size != PAGE_SIZE && size != PMD_SIZE)
		return -EINVAL;

	lockdep_assert_held_write(&kvm->mmu_lock);

	/*
	 * Calling stage2_map() on top of existing mappings is either happening because of a race
	 * with another vCPU, or because we're changing between page and block mappings. As per
	 * user_mem_abort(), same-size permission faults are handled in the relax_perms() path.
	 */
	mapping = pkvm_mapping_iter_first(&pgt->pkvm_mappings, addr, addr + size - 1);
	if (mapping) {
		if (size == (mapping->nr_pages * PAGE_SIZE))
			return -EAGAIN;

		/* Remove _any_ pkvm_mapping overlapping with the range, bigger or smaller. */
		ret = __pkvm_pgtable_stage2_unmap(pgt, addr, addr + size);
		if (ret)
			return ret;
		mapping = NULL;
	}

	ret = kvm_call_hyp_nvhe(__pkvm_host_share_guest, pfn, gfn, prot, size / PAGE_SIZE);
	if (ret) {
		WARN_ON(ret != -ENOMEM);
		return ret;
	}

	swap(mapping, cache->mapping);
	mapping->gfn = gfn;
	mapping->pfn = pfn;
	mapping->nr_pages = size / PAGE_SIZE;
	pkvm_mapping_insert(mapping, &pgt->pkvm_mappings);

	return ret;
}

int pkvm_pgtable_stage2_unmap(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	lockdep_assert_held_write(&kvm_s2_mmu_to_kvm(pgt->mmu)->mmu_lock);

	return __pkvm_pgtable_stage2_unmap(pgt, addr, addr + size);
}

int pkvm_pgtable_stage2_wrprotect(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	struct kvm *kvm = kvm_s2_mmu_to_kvm(pgt->mmu);
	pkvm_handle_t handle = kvm->arch.pkvm.handle;

	return kvm_call_hyp_nvhe(__pkvm_host_wrprotect_guest, handle, addr >> PAGE_SHIFT, size);
}

int pkvm_pgtable_stage2_flush(struct kvm_pgtable *pgt, u64 addr, u64 size)
{
	struct kvm *kvm = kvm_s2_mmu_to_kvm(pgt->mmu);
	struct pkvm_mapping *mapping;

	lockdep_assert_held(&kvm->mmu_lock);
	for_each_mapping_in_range_safe(pgt, addr, addr + size, mapping)
		__clean_dcache_guest_page(pfn_to_kaddr(mapping->pfn), PAGE_SIZE * mapping->nr_pages);

	return 0;
}

bool pkvm_pgtable_stage2_test_clear_young(struct kvm_pgtable *pgt, u64 addr, u64 size, bool mkold)
{
	struct kvm *kvm = kvm_s2_mmu_to_kvm(pgt->mmu);
	pkvm_handle_t handle = kvm->arch.pkvm.handle;

	return kvm_call_hyp_nvhe(__pkvm_host_test_clear_young_guest, handle, addr >> PAGE_SHIFT,
				 size, mkold);
}

int pkvm_pgtable_stage2_relax_perms(struct kvm_pgtable *pgt, u64 addr, enum kvm_pgtable_prot prot,
				    enum kvm_pgtable_walk_flags flags)
{
	return kvm_call_hyp_nvhe(__pkvm_host_relax_perms_guest, addr >> PAGE_SHIFT, prot);
}

kvm_pte_t pkvm_pgtable_stage2_mkyoung(struct kvm_pgtable *pgt, u64 addr,
				 enum kvm_pgtable_walk_flags flags)
{
	return kvm_call_hyp_nvhe(__pkvm_host_mkyoung_guest, addr >> PAGE_SHIFT);
}

void pkvm_pgtable_stage2_free_unlinked(struct kvm_pgtable_mm_ops *mm_ops,
				       struct kvm_pgtable_pte_ops *pte_ops,
				       void *pgtable, s8 level)
{
	WARN_ON_ONCE(1);
}

kvm_pte_t *pkvm_pgtable_stage2_create_unlinked(struct kvm_pgtable *pgt, u64 phys, s8 level,
					enum kvm_pgtable_prot prot, void *mc, bool force_pte)
{
	WARN_ON_ONCE(1);
	return NULL;
}

int pkvm_pgtable_stage2_split(struct kvm_pgtable *pgt, u64 addr, u64 size, void *mc)
{
	WARN_ON_ONCE(1);
	return -EINVAL;
}

static int early_ffa_unmap_on_lend_cfg(char *arg)
{
	static_branch_enable(&kvm_ffa_unmap_on_lend);
	return 0;
}

early_param("kvm-arm.ffa-unmap-on-lend", early_ffa_unmap_on_lend_cfg);
