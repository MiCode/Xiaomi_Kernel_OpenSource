// SPDX-License-Identifier: GPL-2.0-only
/*
 * QTI TEE shared memory bridge driver
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/genalloc.h>

#include <soc/qcom/scm.h>
#include <soc/qcom/qseecomi.h>
#include <soc/qcom/qtee_shmbridge.h>
#include <soc/qcom/secure_buffer.h>

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

#define PERM_BITS 3
#define VM_BITS 16
#define SELF_OWNER_BIT 1
#define SHM_NUM_VM_SHIFT 9

#define VM_PERM_R PERM_READ
#define VM_PERM_W PERM_WRITE

/* ns_vmids = ns_vmid as destination number is only 1 */
#define UPDATE_NS_VMIDS(ns_vmid)	((uint64_t)(ns_vmid))

/* ns_perms = ns_vm_perm as destination number is only 1 */
#define UPDATE_NS_PERMS(ns_vm_perm)	((uint64_t)(ns_vm_perm))

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
static struct bridge_info default_bridge;
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
	if (ret) {
		pr_err("Failed to enable shmbridge, rsp = %d, ret = %d\n",
			desc.ret[0], ret);
		return -EINVAL;
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

/* Register paddr & size as a bridge, return bridge handle */
int32_t qtee_shmbridge_register(
		phys_addr_t paddr,
		size_t size,
		uint32_t ns_vmid,
		uint32_t ns_vm_perm,
		uint32_t tz_perm,
		uint64_t *handle)

{
	int32_t ret = 0;
	uint64_t ns_perms = 0;
	uint64_t destnum = 1;
	struct scm_desc desc = {0};

	if (!handle) {
		pr_err("shmb handle pointer is NULL\n");
		return -EINVAL;
	}
	pr_debug("%s: paddr %lx, size %zu, ns_vmid %x, ns_vm_perm %x, ns_perms %s, tz_perm %x\n",
			__func__, (uint64_t)paddr, size, ns_vmid,
			ns_vm_perm, ns_perms, tz_perm);

	ns_perms = UPDATE_NS_PERMS(ns_vm_perm);
	desc.arginfo = TZ_SHM_BRIDGE_CREATE_PARAM_ID;
	desc.args[0] = UPDATE_PFN_AND_NS_PERM_FLAGS(paddr, ns_perms);
	desc.args[1] = UPDATE_IPFN_AND_S_PERM_FLAGS(paddr, tz_perm);
	desc.args[2] = UPDATE_SIZE_AND_FLAGS(size, destnum);
	desc.args[3] = UPDATE_NS_VMIDS(ns_vmid);

	pr_debug("%s: arginfo %lx, desc.args[0] %lx, args[1] %lx, args[2] %lx, args[3] %lx\n",
			__func__, desc.arginfo, desc.args[0],
			desc.args[1], desc.args[2], desc.args[3]);
	ret = scm_call2(TZ_SHM_BRIDGE_CREATE, &desc);
	if (ret || desc.ret[0]) {
		pr_err("create shmbridge failed, ret = %d, status = %x\n",
				ret, desc.ret[0]);
		return ret;
	}
	*handle = desc.ret[1];
	return 0;
}
EXPORT_SYMBOL(qtee_shmbridge_register);

/* Deregister bridge */
int32_t qtee_shmbridge_deregister(uint64_t handle)
{
	int32_t ret = 0;
	struct scm_desc desc = {0};

	desc.arginfo = TZ_SHM_BRIDGE_DELETE_PARAM_ID;
	desc.args[0] = handle;
	ret = scm_call2(TZ_SHM_BRIDGE_DELETE, &desc);
	if (ret) {
		pr_err("scm_call to delete shmbridge failed, ret = %d\n", ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(qtee_shmbridge_deregister);


/* Sub-allocate from default kernel bridge created by shmb driver */
int32_t qtee_shmbridge_allocate_shm(size_t size, struct qtee_shm *shm)
{
	int32_t ret = 0;
	unsigned long va;

	if (size > DEFAULT_BRIDGE_SIZE) {
		pr_err("requestd size %zu is larger than bridge size %zu\n",
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

	pr_debug("%s: shm->paddr %lx, size %zu\n",
			__func__, (uint64_t)shm->paddr, shm->size);

exit:
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_allocate_shm);


/* Free buffer that is sub-allocated from default kernel bridge */
void qtee_shmbridge_free_shm(struct qtee_shm *shm)
{
	if (IS_ERR_OR_NULL(shm))
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

	if (default_bridge.vaddr) {
		pr_warn("qtee shmbridge is already initialized\n");
		goto exit;
	}

	/* do not enable shm bridge mechanism for now*/
	ret = qtee_shmbridge_enable(false);
	if (ret)
		goto exit;

	/* allocate a contiguous buffer */
	default_bridge.size = DEFAULT_BRIDGE_SIZE;
	default_bridge.vaddr = kzalloc(default_bridge.size, GFP_KERNEL);
	if (!default_bridge.vaddr) {
		ret = -ENOMEM;
		goto exit;
	}
	default_bridge.paddr = virt_to_phys(default_bridge.vaddr);

	/*register default bridge*/
	ret = qtee_shmbridge_register(default_bridge.paddr,
			default_bridge.size, VMID_HLOS,
			VM_PERM_R|VM_PERM_W, VM_PERM_R|VM_PERM_W,
			&default_bridge.handle);
	if (ret) {
		pr_err("Failed to register default bridge, size %zu\n",
			default_bridge.size);
		goto exit_freebuf;
	}

	/* create a general mem pool */
	default_bridge.min_alloc_order = 3; /* 8 byte aligned */
	default_bridge.genpool = gen_pool_create(
					default_bridge.min_alloc_order, -1);
	if (!default_bridge.genpool) {
		pr_err("gen_pool_add_virt() failed\n");
		ret = -ENOMEM;
		goto exit_dereg;
	}

	gen_pool_set_algo(default_bridge.genpool, gen_pool_best_fit, NULL);
	ret = gen_pool_add_virt(default_bridge.genpool,
			(uintptr_t)default_bridge.vaddr,
				default_bridge.paddr, default_bridge.size, -1);
	if (ret) {
		pr_err("gen_pool_add_virt() failed\n");
		goto exit_destroy_pool;
	}

	pr_warn("qtee shmbridge registered default bridge with size %d bytes\n",
			DEFAULT_BRIDGE_SIZE);

	return 0;

exit_destroy_pool:
	gen_pool_destroy(default_bridge.genpool);
exit_dereg:
	qtee_shmbridge_deregister(default_bridge.handle);
exit_freebuf:
	kfree(default_bridge.vaddr);
exit:
	return ret;
}

early_initcall(qtee_shmbridge_init);
