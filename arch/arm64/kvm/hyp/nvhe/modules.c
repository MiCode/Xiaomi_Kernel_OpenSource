/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Google LLC
 */
#include <asm/kvm_host.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/kvm_hypevents.h>
#include <asm/module.h>

#include <nvhe/alloc.h>
#include <nvhe/iommu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/modules.h>
#include <nvhe/mm.h>
#include <nvhe/serial.h>
#include <nvhe/spinlock.h>
#include <nvhe/trace.h>
#include <nvhe/trap_handler.h>

static void *__pkvm_module_memcpy(void *to, const void *from, size_t count)
{
	return memcpy(to, from, count);
}

static void *__pkvm_module_memset(void *dst, int c, size_t count)
{
	return memset(dst, c, count);
}

static void __kvm_flush_dcache_to_poc(void *addr, size_t size)
{
	kvm_flush_dcache_to_poc((unsigned long)addr, (unsigned long)size);
}

static void __update_hcr_el2(unsigned long set_mask, unsigned long clear_mask)
{
	struct kvm_nvhe_init_params *params = this_cpu_ptr(&kvm_init_params);

	params->hcr_el2 |= set_mask;
	params->hcr_el2 &= ~clear_mask;
	__kvm_flush_dcache_to_poc(params, sizeof(*params));
	write_sysreg(params->hcr_el2, hcr_el2);
}

static void __update_hfgwtr_el2(unsigned long set_mask, unsigned long clear_mask)
{
	struct kvm_nvhe_init_params *params = this_cpu_ptr(&kvm_init_params);

	params->hfgwtr_el2 |= set_mask;
	params->hfgwtr_el2 &= ~clear_mask;
	__kvm_flush_dcache_to_poc(params, sizeof(*params));
	write_sysreg_s(params->hfgwtr_el2, SYS_HFGWTR_EL2);
}

static atomic_t early_lm_pages;
static void *__pkvm_linear_map_early(phys_addr_t phys, size_t size, enum kvm_pgtable_prot prot)
{
	void *addr = NULL;
	int ret;

	if (!PAGE_ALIGNED(phys) || !PAGE_ALIGNED(size))
		return NULL;

	addr = __hyp_va(phys);
	ret = pkvm_create_mappings(addr, addr + size, prot);
	if (ret)
		addr = NULL;
	else
		atomic_add(size, &early_lm_pages);

	return addr;
}

static void __pkvm_linear_unmap_early(void *addr, size_t size)
{
	pkvm_remove_mappings(addr, addr + size);
	atomic_sub(size, &early_lm_pages);
}

void __pkvm_close_module_registration(void)
{
	/*
	 * Page ownership tracking might go out of sync if there are stale
	 * entries in pKVM's linear map range, so they must really be gone by
	 * now.
	 */
	WARN_ON_ONCE(atomic_read(&early_lm_pages));

	/*
	 * Nothing else to do, module loading HVCs are only accessible before
	 * deprivilege
	 */
}

static void tracing_mod_hyp_printk(u8 fmt_id, u64 a, u64 b, u64 c, u64 d)
{
#ifdef CONFIG_TRACING
	struct trace_hyp_format___hyp_printk *entry;
	size_t length = sizeof(*entry);

	if (!atomic_read(&__hyp_printk_enabled))
		return;

	entry = tracing_reserve_entry(length);
	if (!entry)
		return;
	entry->hdr.id = hyp_event_id___hyp_printk.id;
	entry->fmt_id = fmt_id;
	entry->a = a;
	entry->b = b;
	entry->c = c;
	entry->d = d;
	tracing_commit_entry();
#endif
}

static int host_stage2_enable_lazy_pte(u64 pfn, u64 nr_pages)
{
	/*
	 * Deprecating the lazy PTE functionality as now the
	 * host can unmap on FF-A lend.
	 */
	WARN_ON(1);

	return -EPERM;
}

static int host_stage2_disable_lazy_pte(u64 pfn, u64 nr_pages)
{
	WARN_ON(1);

	return -EPERM;
}

static int __hyp_smp_processor_id(void)
{
	return hyp_smp_processor_id();
}

#define MAX_MOD_HANDLERS 16

enum mod_handler_type {
	HOST_FAULT_HANDLER = 0,
	HOST_SMC_HANDLER,
	GUEST_SMC_HANDLER,
	NUM_MOD_HANDLER_TYPES,
};

static void *mod_handlers[NUM_MOD_HANDLER_TYPES][MAX_MOD_HANDLERS];

static int mod_handler_register(enum mod_handler_type type, void *handler)
{
	int i;

	for (i = 0; i < MAX_MOD_HANDLERS; i++) {
		if (!cmpxchg64_release(&mod_handlers[type][i], NULL, handler))
			return 0;
	}

	return -EBUSY;
}

static void *__get_mod_handler(enum mod_handler_type type, int i)
{
	if (WARN_ON(type >= NUM_MOD_HANDLER_TYPES))
		return NULL;

	if (i >= MAX_MOD_HANDLERS)
		return NULL;

	i = array_index_nospec(i, MAX_MOD_HANDLERS);

	return smp_load_acquire(&mod_handlers[type][i]);
}

#define for_each_mod_handler(type, handler, i)					\
	for ((i) = 0, handler = (typeof(handler))__get_mod_handler(type, 0);	\
	     handler;								\
	     handler = (typeof(handler))__get_mod_handler(type, ++(i)))

static int
__register_host_perm_fault_handler(int (*cb)(struct user_pt_regs *regs, u64 esr, u64 addr))
{
	return mod_handler_register(HOST_FAULT_HANDLER, cb);
}

static int __register_host_smc_handler(bool (*cb)(struct user_pt_regs *))
{
	return mod_handler_register(HOST_SMC_HANDLER, cb);
}

static int __register_guest_smc_handler(bool (*cb)(struct arm_smccc_1_2_regs *regs,
						   struct arm_smccc_1_2_regs *res,
						   pkvm_handle_t handle))
{
	return mod_handler_register(GUEST_SMC_HANDLER, cb);
}

bool module_handle_host_perm_fault(struct user_pt_regs *regs, u64 esr, u64 addr)
{
	int (*cb)(struct user_pt_regs *regs, u64 esr, u64 addr);
	int i;

	for_each_mod_handler(HOST_FAULT_HANDLER, cb, i) {
		if (!cb(regs, esr, addr))
			return true;
	}

	return false;
}

bool module_handle_host_smc(struct user_pt_regs *regs)
{
	bool (*cb)(struct user_pt_regs *regs);
	int i;

	for_each_mod_handler(HOST_SMC_HANDLER, cb, i) {
		if (cb(regs))
			return true;
	}

	return false;
}

bool module_handle_guest_smc(struct arm_smccc_1_2_regs *regs, struct arm_smccc_1_2_regs *res,
			     pkvm_handle_t handle)
{
	bool (*cb)(struct arm_smccc_1_2_regs *regs, struct arm_smccc_1_2_regs *res,
		   pkvm_handle_t handle);
	int i;

	for_each_mod_handler(GUEST_SMC_HANDLER, cb, i) {
		if (cb(regs, res, handle))
			return true;
	}

	return false;
}

const struct pkvm_module_ops module_ops = {
	.create_private_mapping = __pkvm_create_private_mapping,
	.alloc_module_va = __pkvm_alloc_module_va,
	.map_module_page = __pkvm_map_module_page,
	.register_serial_driver = __pkvm_register_serial_driver,
	.putc = hyp_putc,
	.puts = hyp_puts,
	.putx64 = hyp_putx64,
	.fixmap_map = hyp_fixmap_map,
	.fixmap_unmap = hyp_fixmap_unmap,
	.fixblock_map = hyp_fixblock_map,
	.fixblock_unmap = hyp_fixblock_unmap,
	.linear_map_early = __pkvm_linear_map_early,
	.linear_unmap_early = __pkvm_linear_unmap_early,
	.flush_dcache_to_poc = __kvm_flush_dcache_to_poc,
	.update_hcr_el2 = __update_hcr_el2,
	.update_hfgwtr_el2 = __update_hfgwtr_el2,
	.register_host_perm_fault_handler = __register_host_perm_fault_handler,
	.host_stage2_mod_prot = module_change_host_page_prot,
	.host_stage2_get_leaf = host_stage2_get_leaf,
	.host_stage2_enable_lazy_pte = host_stage2_enable_lazy_pte,
	.host_stage2_disable_lazy_pte = host_stage2_disable_lazy_pte,
	.register_host_smc_handler = __register_host_smc_handler,
	.register_guest_smc_handler = __register_guest_smc_handler,
	.register_default_trap_handler = __pkvm_register_default_trap_handler,
	.register_illegal_abt_notifier = __pkvm_register_illegal_abt_notifier,
	.register_psci_notifier = __pkvm_register_psci_notifier,
	.register_hyp_panic_notifier = __pkvm_register_hyp_panic_notifier,
	.register_unmask_serror = __pkvm_register_unmask_serror,
	.host_donate_hyp = ___pkvm_host_donate_hyp,
	.host_donate_hyp_prot = ___pkvm_host_donate_hyp_prot,
	.host_donate_sglist_hyp = __pkvm_host_donate_sglist_hyp,
	.hyp_donate_host = __pkvm_hyp_donate_host,
	.host_share_hyp = __pkvm_host_share_hyp,
	.host_unshare_hyp = __pkvm_host_unshare_hyp,
	.pin_shared_mem = hyp_pin_shared_mem,
	.unpin_shared_mem = hyp_unpin_shared_mem,
	.memcpy = __pkvm_module_memcpy,
	.memset = __pkvm_module_memset,
	.hyp_pa = hyp_virt_to_phys,
	.hyp_va = hyp_phys_to_virt,
	.kern_hyp_va = __kern_hyp_va,
	.tracing_reserve_entry = tracing_reserve_entry,
	.tracing_commit_entry = tracing_commit_entry,
	.tracing_mod_hyp_printk = tracing_mod_hyp_printk,
	.hyp_alloc = hyp_alloc,
	.hyp_alloc_errno = hyp_alloc_errno,
	.hyp_free = hyp_free,
	.hyp_alloc_missing_donations = hyp_alloc_missing_donations,
	.iommu_donate_pages = kvm_iommu_donate_pages,
	.iommu_reclaim_pages = kvm_iommu_reclaim_pages,
	.iommu_init_device = kvm_iommu_init_device,
	.udelay = pkvm_udelay,
	.iommu_iotlb_gather_add_page = kvm_iommu_iotlb_gather_add_page,
	.pkvm_unuse_dma = iommu_pkvm_unuse_dma,
#ifdef CONFIG_LIST_HARDENED
	.list_add_valid_or_report = __list_add_valid_or_report,
	.list_del_entry_valid_or_report = __list_del_entry_valid_or_report,
#endif
	.iommu_snapshot_host_stage2 = kvm_iommu_snapshot_host_stage2,
	.iommu_donate_pages_atomic = kvm_iommu_donate_pages_atomic,
	.iommu_reclaim_pages_atomic = kvm_iommu_reclaim_pages_atomic,
	.hyp_smp_processor_id = __hyp_smp_processor_id,
	.device_register_reset = pkvm_device_register_reset,
};

static void *pkvm_module_hyp_va(struct pkvm_el2_module *mod, void *kern_va)
{
	return kern_va - mod->sections.start + mod->hyp_va;
}

int __pkvm_init_module(void *host_mod)
{
	int (*do_module_init)(const struct pkvm_module_ops *ops);
	struct pkvm_el2_module *mod = kern_hyp_va(host_mod);
	void *event_ids, *funcs, *funcs_end, *ftrace_tramp;
	size_t hyp_kern_offset;

	event_ids = pkvm_module_hyp_va(mod, mod->event_ids.start);
	funcs = pkvm_module_hyp_va(mod, mod->patchable_function_entries.start);
	funcs_end = pkvm_module_hyp_va(mod, mod->patchable_function_entries.end);
	/* see module.lds.h */
	ftrace_tramp = pkvm_module_hyp_va(mod, mod->text.end) - 20;

	hyp_kern_offset = mod->sections.start - mod->hyp_va;

	register_hyp_mod_events(event_ids, mod->nr_hyp_events,
				funcs, funcs_end, ftrace_tramp, hyp_kern_offset);

	do_module_init = pkvm_module_hyp_va(mod, (void *)mod->init);

	return do_module_init(&module_ops);
}

#define MAX_DYNAMIC_HCALLS 128

atomic_t num_dynamic_hcalls = ATOMIC_INIT(0);
DEFINE_HYP_SPINLOCK(dyn_hcall_lock);

static dyn_hcall_t host_dynamic_hcalls[MAX_DYNAMIC_HCALLS];

int handle_host_dynamic_hcall(struct user_pt_regs *regs, int id)
{
	dyn_hcall_t hfn;
	int dyn_id;

	/*
	 * TODO: static key to protect when no dynamic hcall is registered?
	 */

	dyn_id = id - __KVM_HOST_SMCCC_FUNC___dynamic_hcalls;
	if (dyn_id < 0)
		return HCALL_UNHANDLED;

	/*
	 * Order access to num_dynamic_hcalls and host_dynamic_hcalls. Paired
	 * with __pkvm_register_hcall().
	 */
	if (dyn_id >= atomic_read_acquire(&num_dynamic_hcalls))
		return HCALL_UNHANDLED;

	hfn = READ_ONCE(host_dynamic_hcalls[dyn_id]);
	if (!hfn)
		return HCALL_UNHANDLED;

	hfn(regs);

	return HCALL_HANDLED;
}

int __pkvm_register_hcall(unsigned long hvn_hyp_va)
{
	dyn_hcall_t hfn = (void *)hvn_hyp_va;
	int reserved_id, ret;

	assert_in_mod_range(hvn_hyp_va);

	hyp_spin_lock(&dyn_hcall_lock);

	reserved_id = atomic_read(&num_dynamic_hcalls);

	if (reserved_id >= MAX_DYNAMIC_HCALLS) {
		ret = -ENOMEM;
		goto err_hcall_unlock;
	}

	WRITE_ONCE(host_dynamic_hcalls[reserved_id], hfn);

	/*
	 * Order access to num_dynamic_hcalls and host_dynamic_hcalls. Paired
	 * with handle_host_dynamic_hcall.
	 */
	atomic_set_release(&num_dynamic_hcalls, reserved_id + 1);

	ret = reserved_id + __KVM_HOST_SMCCC_FUNC___dynamic_hcalls;
err_hcall_unlock:
	hyp_spin_unlock(&dyn_hcall_lock);

	return ret;
};
