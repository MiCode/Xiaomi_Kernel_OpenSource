// SPDX-License-Identifier: GPL-2.0-only
/*
 * QTI TEE shared memory bridge driver
 *
 * Copyright (c) 2019,2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/genalloc.h>

#include <soc/qcom/scm.h>
#include <soc/qcom/qseecomi.h>
#include <soc/qcom/qtee_shmbridge.h>

#define DEFAULT_BRIDGE_SIZE	SZ_4M	/*4M*/
/*
 * tz_enable_shm_bridge
 * smc_id: 0x02000C1C
 */
#define TZ_SVC_MEMORY_PROTECTION  12

#define TZ_SHM_BRIDGE_ENABLE                   \
	TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, TZ_SVC_MEMORY_PROTECTION, 0x1C)

#define TZ_SHM_BRIDGE_ENABLE_PARAM_ID          \
		TZ_SYSCALL_CREATE_PARAM_ID_0

/*
 * tz_create_shm_bridge
 * smc_id: 0x02000C1E
 */

#define TZ_SHM_BRIDGE_CREATE                   \
	TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, TZ_SVC_MEMORY_PROTECTION, 0x1E)

#define TZ_SHM_BRIDGE_CREATE_PARAM_ID          \
	TZ_SYSCALL_CREATE_PARAM_ID_4( \
	TZ_SYSCALL_PARAM_TYPE_VAL, TZ_SYSCALL_PARAM_TYPE_VAL, \
	TZ_SYSCALL_PARAM_TYPE_VAL, TZ_SYSCALL_PARAM_TYPE_VAL)

/**
 * tz_delete_shm_bridge
 * smc_id: 0x02000C1D
 */
#define TZ_SHM_BRIDGE_DELETE                   \
	TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, TZ_SVC_MEMORY_PROTECTION, 0x1D)

#define TZ_SHM_BRIDGE_DELETE_PARAM_ID          \
	TZ_SYSCALL_CREATE_PARAM_ID_1( \
	TZ_SYSCALL_PARAM_TYPE_VAL)

#define MAXSHMVMS 4
#define PERM_BITS 3
#define VM_BITS 16
#define SELF_OWNER_BIT 1
#define SHM_NUM_VM_SHIFT 9
#define SHM_VM_MASK 0xFFFF
#define SHM_PERM_MASK 0x7

#define VM_PERM_R PERM_READ
#define VM_PERM_W PERM_WRITE

#define SHMBRIDGE_E_NOT_SUPPORTED 4	/* SHMbridge is not implemented */

/* ns_vmids */
#define UPDATE_NS_VMIDS(ns_vmids, id)	\
				(((uint64_t)(ns_vmids) << VM_BITS) \
				| ((uint64_t)(id) & SHM_VM_MASK))

/* ns_perms */
#define UPDATE_NS_PERMS(ns_perms, perm)	\
				(((uint64_t)(ns_perms) << PERM_BITS) \
				| ((uint64_t)(perm) & SHM_PERM_MASK))

/* pfn_and_ns_perm_flags = paddr | ns_perms */
#define UPDATE_PFN_AND_NS_PERM_FLAGS(paddr, ns_perms)	\
				((uint64_t)(paddr) | (ns_perms))


/* ipfn_and_s_perm_flags = ipaddr | tz_perm */
#define UPDATE_IPFN_AND_S_PERM_FLAGS(ipaddr, tz_perm)	\
				((uint64_t)(ipaddr) | (uint64_t)(tz_perm))

/* size_and_flags when dest_vm is not HYP */
#define UPDATE_SIZE_AND_FLAGS(size, destnum)	\
				((size) | (destnum) << SHM_NUM_VM_SHIFT)

struct bridge_info {
	phys_addr_t paddr;
	void *vaddr;
	size_t size;
	uint64_t handle;
	int min_alloc_order;
	struct gen_pool *genpool;
};

struct bridge_list {
	struct list_head head;
	struct mutex lock;
};

struct bridge_list_entry {
	struct list_head list;
	phys_addr_t paddr;
	uint64_t handle;
};

static struct bridge_info default_bridge;
static struct bridge_list bridge_list_head;
static bool qtee_shmbridge_enabled;

/* enable shared memory bridge mechanism in HYP */
static int32_t qtee_shmbridge_enable(bool enable)
{
	int32_t ret = 0;
	struct scm_desc desc = {0};

	qtee_shmbridge_enabled = false;
	if (!enable) {
		pr_warn("shmbridge isn't enabled\n");
		return ret;
	}

	desc.arginfo = TZ_SHM_BRIDGE_ENABLE_PARAM_ID;
	ret = scm_call2(TZ_SHM_BRIDGE_ENABLE, &desc);
	if (ret || desc.ret[0]) {
		pr_err("Failed to enable shmbridge, rsp = %lld, ret = %d\n",
			desc.ret[0], ret);
		if (ret == -EOPNOTSUPP ||
			desc.ret[0] == SHMBRIDGE_E_NOT_SUPPORTED)
			pr_warn("shmbridge is not supported by this target\n");
		return ret | desc.ret[0];
	}
	qtee_shmbridge_enabled = true;
	pr_warn("shmbridge is enabled\n");
	return ret;
}

/* Check whether shmbridge mechanism is enabled in HYP or not */
bool qtee_shmbridge_is_enabled(void)
{
	return qtee_shmbridge_enabled;
}
EXPORT_SYMBOL(qtee_shmbridge_is_enabled);

int32_t qtee_shmbridge_list_add_nolock(phys_addr_t paddr, uint64_t handle)
{
	struct bridge_list_entry *entry;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;
	entry->handle = handle;
	entry->paddr = paddr;
	list_add_tail(&entry->list, &bridge_list_head.head);
	return 0;
}

void qtee_shmbridge_list_del_nolock(uint64_t handle)
{
	struct bridge_list_entry *entry;

	list_for_each_entry(entry, &bridge_list_head.head, list) {
		if (entry->handle == handle) {
			list_del(&entry->list);
			kfree(entry);
			break;
		}
	}
}

int32_t qtee_shmbridge_query_nolock(phys_addr_t paddr)
{
	struct bridge_list_entry *entry;

	list_for_each_entry(entry, &bridge_list_head.head, list)
		if (entry->paddr == paddr) {
			pr_debug("A bridge on %llx exists\n", (uint64_t)paddr);
			return -EEXIST;
		}
	return 0;
}

/* Check whether a bridge starting from paddr exists */
int32_t qtee_shmbridge_query(phys_addr_t paddr)
{
	int32_t ret = 0;

	mutex_lock(&bridge_list_head.lock);
	ret = qtee_shmbridge_query_nolock(paddr);
	mutex_unlock(&bridge_list_head.lock);
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_query);

/* Register paddr & size as a bridge, return bridge handle */
int32_t qtee_shmbridge_register(
		phys_addr_t paddr,
		size_t size,
		uint32_t *ns_vmid_list,
		uint32_t *ns_vm_perm_list,
		uint32_t ns_vmid_num,
		uint32_t tz_perm,
		uint64_t *handle)

{
	int32_t ret = 0;
	uint64_t ns_perms = 0;
	uint64_t ns_vmids = 0;
	struct scm_desc desc = {0};
	int i = 0;

	if (!qtee_shmbridge_enabled)
		return 0;

	if (!handle || !ns_vmid_list || !ns_vm_perm_list ||
				ns_vmid_num > MAXSHMVMS) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	mutex_lock(&bridge_list_head.lock);
	ret = qtee_shmbridge_query_nolock(paddr);
	if (ret)
		goto exit;

	for (i = 0; i < ns_vmid_num; i++) {
		ns_perms = UPDATE_NS_PERMS(ns_perms, ns_vm_perm_list[i]);
		ns_vmids = UPDATE_NS_VMIDS(ns_vmids, ns_vmid_list[i]);
	}

	desc.arginfo = TZ_SHM_BRIDGE_CREATE_PARAM_ID;
	desc.args[0] = UPDATE_PFN_AND_NS_PERM_FLAGS(paddr, ns_perms);
	desc.args[1] = UPDATE_IPFN_AND_S_PERM_FLAGS(paddr, tz_perm);
	desc.args[2] = UPDATE_SIZE_AND_FLAGS(size, ns_vmid_num);
	desc.args[3] = ns_vmids;

	pr_debug("%s: arginfo %x, desc.args[0] %llx, args[1] %llx, args[2] %llx, args[3] %llx\n",
			__func__, desc.arginfo, desc.args[0],
			desc.args[1], desc.args[2], desc.args[3]);
	ret = scm_call2(TZ_SHM_BRIDGE_CREATE, &desc);
	if (ret || desc.ret[0]) {
		pr_err("create shmbridge failed, ret = %d, status = %llx\n",
				ret, desc.ret[0]);
		ret = -EINVAL;
		goto exit;
	}
	*handle = desc.ret[1];

	ret = qtee_shmbridge_list_add_nolock(paddr, *handle);
exit:
	mutex_unlock(&bridge_list_head.lock);
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_register);

/* Deregister bridge */
int32_t qtee_shmbridge_deregister(uint64_t handle)
{
	int32_t ret = 0;
	struct scm_desc desc = {0};

	if (!qtee_shmbridge_enabled)
		return 0;

	mutex_lock(&bridge_list_head.lock);
	desc.arginfo = TZ_SHM_BRIDGE_DELETE_PARAM_ID;
	desc.args[0] = handle;
	ret = scm_call2(TZ_SHM_BRIDGE_DELETE, &desc);
	if (ret) {
		pr_err("Failed to del bridge %lld, ret = %d\n", handle, ret);
		goto exit;
	}
	qtee_shmbridge_list_del_nolock(handle);

exit:
	mutex_unlock(&bridge_list_head.lock);
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_deregister);


/* Sub-allocate from default kernel bridge created by shmb driver */
int32_t qtee_shmbridge_allocate_shm(size_t size, struct qtee_shm *shm)
{
	int32_t ret = 0;
	unsigned long va;

	if (size > DEFAULT_BRIDGE_SIZE) {
		pr_err("requestd size %zu is larger than bridge size %d\n",
			size, DEFAULT_BRIDGE_SIZE);
		ret = -EINVAL;
		goto exit;
	}

	if (IS_ERR_OR_NULL(shm)) {
		pr_err("qtee_shm is NULL\n");
		ret = -EINVAL;
		goto exit;
	}
	size = roundup(size, 1 << default_bridge.min_alloc_order);

	va = gen_pool_alloc(default_bridge.genpool, size);
	if (!va) {
		pr_err("failed to sub-allocate %zu bytes from bridge\n", size);
		ret = -ENOMEM;
		goto exit;
	}

	memset((void *)va, 0, size);
	shm->vaddr = (void *)va;
	shm->paddr = gen_pool_virt_to_phys(default_bridge.genpool, va);
	shm->size = size;

	pr_debug("%s: shm->paddr %llx, size %zu\n",
			__func__, (uint64_t)shm->paddr, shm->size);

exit:
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_allocate_shm);


/* Free buffer that is sub-allocated from default kernel bridge */
void qtee_shmbridge_free_shm(struct qtee_shm *shm)
{
	if (IS_ERR_OR_NULL(shm) || !shm->vaddr)
		return;
	gen_pool_free(default_bridge.genpool, (unsigned long)shm->vaddr,
		      shm->size);
}
EXPORT_SYMBOL(qtee_shmbridge_free_shm);

/*
 * shared memory bridge initialization
 *
 */
static int __init qtee_shmbridge_init(void)
{
	int ret = 0;
	uint32_t ns_vm_ids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {VM_PERM_R|VM_PERM_W};

	if (default_bridge.vaddr) {
		pr_warn("qtee shmbridge is already initialized\n");
		return 0;
	}

	/* allocate a contiguous page aligned buffer */
	default_bridge.size = DEFAULT_BRIDGE_SIZE;
	default_bridge.vaddr = (void *)__get_free_pages(GFP_KERNEL|__GFP_COMP,
				get_order(default_bridge.size));
	if (!default_bridge.vaddr)
		return -ENOMEM;
	default_bridge.paddr = virt_to_phys(default_bridge.vaddr);

	/* create a general mem pool */
	default_bridge.min_alloc_order = PAGE_SHIFT; /* 4K page size aligned */
	default_bridge.genpool = gen_pool_create(
					default_bridge.min_alloc_order, -1);
	if (!default_bridge.genpool) {
		pr_err("gen_pool_add_virt() failed\n");
		ret = -ENOMEM;
		goto exit_freebuf;
	}

	gen_pool_set_algo(default_bridge.genpool, gen_pool_best_fit, NULL);
	ret = gen_pool_add_virt(default_bridge.genpool,
			(uintptr_t)default_bridge.vaddr,
				default_bridge.paddr, default_bridge.size, -1);
	if (ret) {
		pr_err("gen_pool_add_virt() failed, ret = %d\n", ret);
		goto exit_destroy_pool;
	}

	mutex_init(&bridge_list_head.lock);
	INIT_LIST_HEAD(&bridge_list_head.head);

	/* enable shm bridge mechanism */
	ret = qtee_shmbridge_enable(true);
	if (ret) {
		/* keep the mem pool and return if failed to enable bridge */
		ret = 0;
		goto exit;
	}

	/*register default bridge*/
	ret = qtee_shmbridge_register(default_bridge.paddr,
			default_bridge.size, ns_vm_ids,
			ns_vm_perms, 1, VM_PERM_R|VM_PERM_W,
			&default_bridge.handle);
	if (ret) {
		pr_err("Failed to register default bridge, size %zu\n",
			default_bridge.size);
		goto exit;
	}

	pr_debug("qtee shmbridge registered default bridge with size %d bytes\n",
			DEFAULT_BRIDGE_SIZE);

	return 0;

exit_destroy_pool:
	gen_pool_destroy(default_bridge.genpool);
exit_freebuf:
	free_pages((long)default_bridge.vaddr, get_order(default_bridge.size));
exit:
	return ret;
}

early_initcall(qtee_shmbridge_init);
