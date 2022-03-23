// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef pr_fmt
#define pr_fmt(fmt) "MKP: " fmt

#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/avc.h>
#include <trace/hooks/creds.h>
#include <trace/hooks/module.h>
#include <trace/hooks/memory.h>
#include <trace/hooks/selinux.h>
#include <trace/hooks/syscall_check.h>
#include <linux/types.h> // for list_head
#include <linux/module.h> // module_layout
#include <linux/init.h> // rodata_enable support
#include <linux/mutex.h>
#include <linux/kernel.h> // round_up
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/tracepoint.h>
#include <linux/of.h>
#include <linux/libfdt.h> // fdt32_ld

#include "selinux/mkp_security.h"
#include "selinux/mkp_policycap.h"

#include "mkp_demo.h"

#include "mkp.h"
#define CREATE_TRACE_POINTS
#include "trace_mtk_mkp.h"

#define mkp_debug 0
DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

struct work_struct *avc_work;

static uint32_t g_ro_avc_handle __ro_after_init;
static uint32_t g_ro_cred_handle __ro_after_init;
static struct page *avc_pages __ro_after_init;
static struct page *cred_pages __ro_after_init;
int avc_array_sz __ro_after_init;
int cred_array_sz __ro_after_init;
int rem;
static bool g_initialized;
static struct selinux_avc *g_avc;
static struct selinux_policy __rcu *g_policy;
const struct selinux_state *g_selinux_state;

#if mkp_debug
static void mkp_trace_event_func(struct timer_list *unused);
static DEFINE_TIMER(mkp_trace_event_timer, mkp_trace_event_func);
#define MKP_TRACE_EVENT_TIME 10
#endif

const char *mkp_trace_print_array(void)
{
	static char str[30] = "mkp test trace point\n";

	return str;
}

#if mkp_debug
static void mkp_trace_event_func(struct timer_list *unused) // do not use sleep
{
	char test[1024];

	memset(test, 0, 1024);
	memcpy(test, "hello world.", 13);
	trace_mkp_trace_event_test(test);
	MKP_DEBUG("timer start\n");
	mod_timer(&mkp_trace_event_timer, jiffies
		+ MKP_TRACE_EVENT_TIME * HZ);

}
#endif

struct rb_root mkp_rbtree = RB_ROOT;
DEFINE_RWLOCK(mkp_rbtree_rwlock);

#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
static void *p_stext;
static void *p_etext;
static void *p__init_begin;
#endif

static void probe_android_vh_set_memory_ro(void *ignore, unsigned long addr,
		int nr_pages)
{
	int ret;
	int region;

	region = is_module_or_bpf_addr((void *)addr);
	if (region == MKP_DEMO_MODULE_CASE) {
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_DRV,
			HELPER_MAPPING_RO);
	} else if (region == MKP_DEMO_BPF_CASE) {
#ifndef CONFIG_DEBUG_VIRTUAL
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_KERNEL_PAGES,
			HELPER_MAPPING_RO);
#endif
	}
}

static void probe_android_vh_set_memory_x(void *ignore, unsigned long addr,
		int nr_pages)
{
	int ret;
	int region;

	region = is_module_or_bpf_addr((void *)addr);
	if (region == MKP_DEMO_MODULE_CASE) {
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_DRV,
			HELPER_MAPPING_X);
	} else if (region == MKP_DEMO_BPF_CASE) {
#ifndef CONFIG_DEBUG_VIRTUAL
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_KERNEL_PAGES,
			HELPER_MAPPING_X);
#endif
	}
}

static void probe_android_vh_set_memory_rw(void *ignore, unsigned long addr,
		int nr_pages)
{
	int ret;
	int region;

	if (((unsigned long)THIS_MODULE->init_layout.base) == addr) {
		return;
	}
	region = is_module_or_bpf_addr((void *)addr);
	if (region == MKP_DEMO_MODULE_CASE) {
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_DRV,
			HELPER_MAPPING_RW);
	} else if (region == MKP_DEMO_BPF_CASE) {
#ifndef CONFIG_DEBUG_VIRTUAL
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_KERNEL_PAGES,
			HELPER_MAPPING_RW);
#endif
	}
}
static void probe_android_vh_set_memory_nx(void *ignore, unsigned long addr,
		int nr_pages)
{
	int ret;
	int region;
	int i = 0;
	unsigned long pfn;
	struct mkp_rb_node *found = NULL;
	phys_addr_t phys_addr;
	uint32_t policy;
	unsigned long flags;

	if (((unsigned long)THIS_MODULE->init_layout.base) == addr) {
		return;
	}
	region = is_module_or_bpf_addr((void *)addr);
	if (region == MKP_DEMO_MODULE_CASE) {
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_DRV,
			HELPER_MAPPING_NX);
		policy = MKP_POLICY_DRV;
	} else if (region == MKP_DEMO_BPF_CASE) {
#ifndef CONFIG_DEBUG_VIRTUAL
		ret = mkp_set_mapping_xxx_helper(addr, nr_pages, MKP_POLICY_KERNEL_PAGES,
			HELPER_MAPPING_NX);
		policy = MKP_POLICY_KERNEL_PAGES;
#else
		return;
#endif
	}

	for (i = 0; i < nr_pages; i++) {
		pfn = vmalloc_to_pfn((void *)(addr+i*PAGE_SIZE));
		phys_addr = pfn << PAGE_SHIFT;
		write_lock_irqsave(&mkp_rbtree_rwlock, flags);
		found = mkp_rbtree_search(&mkp_rbtree, phys_addr);
		if (found != NULL && found->addr != 0 && found->size != 0) {
			ret = mkp_destroy_handle(policy, found->handle);
			ret = mkp_rbtree_erase(&mkp_rbtree, phys_addr);
		}
		write_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
	}
}

#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
static int __init protect_kernel(void)
{
	int ret = 0;
	uint32_t policy = 0;
	uint32_t handle = 0;
	unsigned long addr_start;
	unsigned long addr_end;
	phys_addr_t phys_addr;
	int nr_pages;
	int init = 0;

	MKP_DEBUG("%s start\n", __func__);
	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] &&
		policy_ctrl[MKP_POLICY_KERNEL_RODATA]) {
		mkp_get_krn_info(&p_stext, &p_etext, &p__init_begin);
		init = 1;
	}

	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] != 0) {
		if (!init)
			mkp_get_krn_code(&p_stext, &p_etext);
		// round down addr before minus operation
		addr_start = (unsigned long)p_stext;
		addr_end = (unsigned long)p_etext;
		addr_start = round_up(addr_start, PAGE_SIZE);
		addr_end = round_down(addr_end, PAGE_SIZE);
		nr_pages = (addr_end-addr_start)>>PAGE_SHIFT;
		phys_addr = virt_to_phys((void *)addr_start);
		policy = MKP_POLICY_KERNEL_CODE;
		handle = mkp_create_handle(policy, (unsigned long)phys_addr, nr_pages<<12);
		if (handle == 0) {
			MKP_ERR("%s:%d: Create handle fail\n", __func__, __LINE__);
		} else {
			ret = mkp_set_mapping_x(policy, handle);
			ret = mkp_set_mapping_ro(policy, handle);
		}
	}

	if (policy_ctrl[MKP_POLICY_KERNEL_RODATA] != 0) {
		if (!init)
			mkp_get_krn_rodata(&p_etext, &p__init_begin);
		// round down addr before minus operation
		addr_start = (unsigned long)p_etext;
		addr_end = (unsigned long)p__init_begin;
		addr_start = round_up(addr_start, PAGE_SIZE);
		addr_end = round_down(addr_end, PAGE_SIZE);
		nr_pages = (addr_end-addr_start)>>PAGE_SHIFT;
		phys_addr = virt_to_phys((void *)addr_start);
		policy = MKP_POLICY_KERNEL_RODATA;
		handle = mkp_create_handle(policy, (unsigned long)phys_addr, nr_pages<<12);
		if (handle == 0) {
			MKP_ERR("%s:%d: Create handle fail\n", __func__, __LINE__);
		} else
			ret = mkp_set_mapping_ro(policy, handle);
	}

	MKP_DEBUG("%s done\n", __func__);
	p_stext = NULL;
	p_etext = NULL;
	p__init_begin = NULL;
	return 0;
}
#endif

static void probe_android_vh_set_module_permit_before_init(void *ignore,
	const struct module *mod)
{
	if (mod == THIS_MODULE && policy_ctrl[MKP_POLICY_MKP] != 0) {
		module_enable_ro(mod, false, MKP_POLICY_MKP);
		module_enable_nx(mod, MKP_POLICY_MKP);
		module_enable_x(mod, MKP_POLICY_MKP);
		return;
	}
	if (mod != THIS_MODULE && policy_ctrl[MKP_POLICY_DRV] != 0) {
		module_enable_ro(mod, false, MKP_POLICY_DRV);
		module_enable_nx(mod, MKP_POLICY_DRV);
		module_enable_x(mod, MKP_POLICY_DRV);
	}
}

static void probe_android_vh_set_module_permit_after_init(void *ignore,
	const struct module *mod)
{
	if (mod == THIS_MODULE && policy_ctrl[MKP_POLICY_MKP] != 0) {
		module_enable_ro(mod, true, MKP_POLICY_MKP);
		return;
	}
	if (mod != THIS_MODULE && policy_ctrl[MKP_POLICY_DRV] != 0)
		module_enable_ro(mod, true, MKP_POLICY_DRV);
}

static void probe_android_vh_commit_creds(void *ignore, const struct task_struct *task,
	const struct cred *new)
{
	int ret = -1;
	struct cred_sbuf_content c;

	if (g_ro_cred_handle == 0)
		return;

	c.csc.uid.val = new->uid.val;
	c.csc.gid.val = new->gid.val;
	c.csc.euid.val = new->euid.val;
	c.csc.egid.val = new->egid.val;
	c.csc.fsuid.val = new->fsuid.val;
	c.csc.fsgid.val = new->fsgid.val;
	c.csc.security = new->security;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_TASK_CRED, g_ro_cred_handle,
		(unsigned long)task->pid,
		c.args[0], c.args[1], c.args[2], c.args[3]);
}

static void probe_android_vh_exit_creds(void *ignore, const struct task_struct *task,
	const struct cred *cred)
{
	int ret = -1;

	if (g_ro_cred_handle == 0)
		return;

	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_TASK_CRED, g_ro_cred_handle,
		(unsigned long)task->pid, 0, 0, 0, 0);
}

static void probe_android_vh_override_creds(void *ignore, const struct task_struct *task,
	const struct cred *new)
{
	int ret = -1;
	struct cred_sbuf_content c;

	if (g_ro_cred_handle == 0)
		return;

	c.csc.uid.val = new->uid.val;
	c.csc.gid.val = new->gid.val;
	c.csc.euid.val = new->euid.val;
	c.csc.egid.val = new->egid.val;
	c.csc.fsuid.val = new->fsuid.val;
	c.csc.fsgid.val = new->fsgid.val;
	c.csc.security = new->security;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_TASK_CRED, g_ro_cred_handle,
		(unsigned long)task->pid,
		c.args[0], c.args[1], c.args[2], c.args[3]);
}

static void probe_android_vh_revert_creds(void *ignore, const struct task_struct *task,
	const struct cred *old)
{
	int ret = -1;
	struct cred_sbuf_content c;

	if (g_ro_cred_handle == 0)
		return;

	c.csc.uid.val = old->uid.val;
	c.csc.gid.val = old->gid.val;
	c.csc.euid.val = old->euid.val;
	c.csc.egid.val = old->egid.val;
	c.csc.fsuid.val = old->fsuid.val;
	c.csc.fsgid.val = old->fsgid.val;
	c.csc.security = old->security;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_TASK_CRED, g_ro_cred_handle,
		(unsigned long)task->pid,
		c.args[0], c.args[1], c.args[2], c.args[3]);
}



static void probe_android_vh_selinux_avc_insert(void *ignore, const struct avc_node *node)
{
	struct mkp_avc_node *temp_node = NULL;
	int ret = -1;

	if (g_ro_avc_handle == 0)
		return;
	temp_node = (struct mkp_avc_node *)node;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_SELINUX_AVC, g_ro_avc_handle,
		(unsigned long)temp_node, temp_node->ae.ssid,
		temp_node->ae.tsid, temp_node->ae.tclass, temp_node->ae.avd.allowed);
}

static void probe_android_vh_selinux_avc_node_delete(void *ignore,
	const struct avc_node *node)
{
	int ret = -1;

	if (g_ro_avc_handle == 0)
		return;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_SELINUX_AVC, g_ro_avc_handle,
		(unsigned long)node, 0, 0, 0, 0);
}

static void probe_android_vh_selinux_avc_node_replace(void *ignore,
	const struct avc_node *old, const struct avc_node *new)
{
	struct mkp_avc_node *temp_node = NULL;
	int ret = -1;

	if (g_ro_avc_handle == 0)
		return;
	temp_node = (struct mkp_avc_node *)new;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_SELINUX_AVC, g_ro_avc_handle,
		(unsigned long)old, 0, 0, 0, 0);
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_SELINUX_AVC, g_ro_avc_handle,
		(unsigned long)temp_node, temp_node->ae.ssid,
		temp_node->ae.tsid, temp_node->ae.tclass, temp_node->ae.avd.allowed);
}

static void probe_android_vh_selinux_avc_lookup(void *ignore,
	const struct avc_node *node, u32 ssid, u32 tsid, u16 tclass)
{
	void *va;
	struct avc_sbuf_content *ro_avc_sharebuf_ptr;
	int i;
	struct mkp_avc_node *temp_node = NULL;
	static DEFINE_RATELIMIT_STATE(rs_avc, 1*HZ, 10);

	if (!node || g_ro_avc_handle == 0)
		return;

	ratelimit_set_flags(&rs_avc, RATELIMIT_MSG_ON_RELEASE);
	if (__ratelimit(&rs_avc)) {
		temp_node = (struct mkp_avc_node *)node;
		va = page_address(avc_pages);
		ro_avc_sharebuf_ptr = (struct avc_sbuf_content *)va;

		for (i = 0; i < avc_array_sz; ro_avc_sharebuf_ptr++, i++) {
			if ((unsigned long)ro_avc_sharebuf_ptr->avc_node == (unsigned long)node) {
				if (ro_avc_sharebuf_ptr->ssid != ssid ||
					ro_avc_sharebuf_ptr->tsid != tsid ||
					ro_avc_sharebuf_ptr->tclass != tclass ||
					ro_avc_sharebuf_ptr->ae_allowed !=
						temp_node->ae.avd.allowed) {
					MKP_ERR("avc lookup is not matched\n");
#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
					MKP_ERR("CURRENT-%16lx:%16lx:%16lx:%16lx\n",
					       (unsigned long)ssid,
					       (unsigned long)tsid,
					       (unsigned long)tclass,
					       (unsigned long)temp_node->ae.avd.allowed);
					MKP_ERR("@EXPECT-%16lx:%16lx:%16lx:%16lx\n",
					       (unsigned long)ro_avc_sharebuf_ptr->ssid,
					       (unsigned long)ro_avc_sharebuf_ptr->tsid,
					       (unsigned long)ro_avc_sharebuf_ptr->tclass,
					       (unsigned long)ro_avc_sharebuf_ptr->ae_allowed);
#endif
					handle_mkp_err_action(MKP_POLICY_SELINUX_AVC);
				} else {
					return; // pass
				}
			}
		}
	}
}

static void avc_work_handler(struct work_struct *work)
{
	int ret = 0, ret_erri_line;

	// register avc vendor hook after selinux is initialized
	if (policy_ctrl[MKP_POLICY_SELINUX_AVC] != 0 ||
		g_ro_avc_handle != 0) {
		// register avc vendor hook
		ret = register_trace_android_vh_selinux_avc_insert(
				probe_android_vh_selinux_avc_insert, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto avc_failed;
		}
		ret = register_trace_android_vh_selinux_avc_node_delete(
				probe_android_vh_selinux_avc_node_delete, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto avc_failed;
		}
		ret = register_trace_android_vh_selinux_avc_node_replace(
				probe_android_vh_selinux_avc_node_replace, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto avc_failed;
		}
		ret = register_trace_android_vh_selinux_avc_lookup(
				probe_android_vh_selinux_avc_lookup, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto avc_failed;
		}
	}
avc_failed:
	if (ret)
		MKP_ERR("register avc hooks failed, ret %d line %d\n", ret, ret_erri_line);
}
static void probe_android_vh_selinux_is_initialized(void *ignore,
	const struct selinux_state *state)
{
	g_initialized = state->initialized;
	g_avc = state->avc;
	g_policy = state->policy;
	g_selinux_state = state;

	if (policy_ctrl[MKP_POLICY_SELINUX_AVC]) {
		if (!avc_work) {
			MKP_ERR("avc work create fail\n");
			return;
		}
		INIT_WORK(avc_work, avc_work_handler);
		schedule_work(avc_work);
	}
}

static void check_selinux_state(struct ratelimit_state *rs)
{
	ratelimit_set_flags(rs, RATELIMIT_MSG_ON_RELEASE);
	if (!__ratelimit(rs))
		return;
	if (g_selinux_state &&
		(g_selinux_state->initialized != g_initialized ||
		g_selinux_state->avc != g_avc ||
		g_selinux_state->policy != g_policy)) {
		MKP_ERR("%s:%d: selinux_state is not matched\n", __func__, __LINE__);
#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
		MKP_ERR("CURRENT-%16lx:%16lx:%16lx\n",
				(unsigned long)g_selinux_state->initialized,
				(unsigned long)g_selinux_state->avc,
				(unsigned long)g_selinux_state->policy);
		MKP_ERR("@EXPECT-%16lx:%16lx:%16lx\n",
				(unsigned long)g_initialized,
				(unsigned long)g_avc,
				(unsigned long)g_policy);
#endif
		handle_mkp_err_action(MKP_POLICY_SELINUX_STATE);
	}
}

static bool cred_is_not_matched(const struct cred *curr, pid_t index)
{
	struct cred_sbuf_content *ro_cred_sharebuf_ptr = NULL;
	struct cred_sbuf_content *target = NULL;

	ro_cred_sharebuf_ptr = (struct cred_sbuf_content *)page_address(cred_pages);

	/* pid max */
	if (index > 32767) {
		MKP_ERR("pid is overflow\n");
		return false;
	}

	/* Target for comparison */
	target = ro_cred_sharebuf_ptr + index;

	/* No valid cred or cleared */
	if (target->csc.security == NULL) {
		MKP_WARN("%s:%d: target security point to NULL\n", __func__, __LINE__);
		return false;
	}

	/* Do comparison */
	if (target->csc.uid.val != curr->uid.val ||
		target->csc.gid.val != curr->gid.val ||
		target->csc.euid.val != curr->euid.val ||
		target->csc.egid.val != curr->egid.val ||
		target->csc.fsuid.val != curr->fsuid.val ||
		target->csc.fsgid.val != curr->fsgid.val ||
		target->csc.security != curr->security) {

#if IS_ENABLED(CONFIG_MTK_VM_DEBUG)
		MKP_ERR("CURRENT-(%u)-%16lx:%16lx:%16lx:%16lx:%16lx:%16lx:%16lx\n",
				current->pid,
				(unsigned long)curr->uid.val,
				(unsigned long)curr->gid.val,
				(unsigned long)curr->euid.val,
				(unsigned long)curr->egid.val,
				(unsigned long)curr->fsuid.val,
				(unsigned long)curr->fsgid.val,
				(unsigned long)curr->security);
		MKP_ERR("@EXPECT-(%u)-%16lx:%16lx:%16lx:%16lx:%16lx:%16lx:%16lx\n",
				index,
				(unsigned long)target->csc.uid.val,
				(unsigned long)target->csc.gid.val,
				(unsigned long)target->csc.euid.val,
				(unsigned long)target->csc.egid.val,
				(unsigned long)target->csc.fsuid.val,
				(unsigned long)target->csc.fsgid.val,
				(unsigned long)target->csc.security);
#endif

		return true;
	}

	return false;
}

static void check_cred(struct ratelimit_state *rs)
{
	struct task_struct *cur = NULL, *task;

	ratelimit_set_flags(rs, RATELIMIT_MSG_ON_RELEASE);
	if (!__ratelimit(rs) && (g_ro_cred_handle == 0))
		return;

	cur = get_current();
	task = get_task_struct(cur->group_leader);

	/* If this thread group is exiting, just bypass */
	if (task->flags & PF_EXITING)
		goto exit;

	/* Start matching */
	if (cred_is_not_matched(cur->cred, cur->pid)) {
		MKP_ERR("%s:%d: cred is not matched\n", __func__, __LINE__);
		handle_mkp_err_action(MKP_POLICY_TASK_CRED);
	}

exit:
	put_task_struct(task);
}

static void probe_android_vh_check_mmap_file(void *ignore,
	const struct file *file, unsigned long prot, unsigned long flag, unsigned long ret)
{
	static DEFINE_RATELIMIT_STATE(rs_mmap, 1*HZ, 10);

	check_cred(&rs_mmap);
	check_selinux_state(&rs_mmap);
}

static void probe_android_vh_check_file_open(void *ignore, const struct file *file)
{
	static DEFINE_RATELIMIT_STATE(rs_open, 1*HZ, 10);

	check_cred(&rs_open);
	check_selinux_state(&rs_open);
}

static void probe_android_vh_check_bpf_syscall(void *ignore,
	int cmd, const union bpf_attr *attr, unsigned int size)
{
	static DEFINE_RATELIMIT_STATE(rs_bpf, 1*HZ, 10);

	check_cred(&rs_bpf);
	check_selinux_state(&rs_bpf);
}

static int __init protect_mkp_self(void)
{
	module_enable_ro(THIS_MODULE, false, MKP_POLICY_MKP);
	module_enable_nx(THIS_MODULE, MKP_POLICY_MKP);
	module_enable_x(THIS_MODULE, MKP_POLICY_MKP);

	mkp_start_granting_hvc_call();
	return 0;
}

int mkp_reboot_notifier_event(struct notifier_block *nb, unsigned long event, void *v)
{
	MKP_DEBUG("mkp reboot notifier\n");
	return NOTIFY_DONE;
}
static struct notifier_block mkp_reboot_notifier = {
	.notifier_call = mkp_reboot_notifier_event,
};

/* For probing interesting tracepoints */
struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	int policy;
};

static void mkp_task_newtask(void *ignore, struct task_struct *task, unsigned long clone_flags)
{
	int ret = -1;
	struct cred_sbuf_content c;

	if (g_ro_cred_handle == 0)
		return;

	c.csc.uid.val = task->cred->uid.val;
	c.csc.gid.val = task->cred->gid.val;
	c.csc.euid.val = task->cred->euid.val;
	c.csc.egid.val = task->cred->egid.val;
	c.csc.fsuid.val = task->cred->fsuid.val;
	c.csc.fsgid.val = task->cred->fsgid.val;
	c.csc.security = task->cred->security;
	ret = mkp_update_sharebuf_4_argu(MKP_POLICY_TASK_CRED, g_ro_cred_handle,
			(unsigned long)task->pid,
			c.args[0], c.args[1], c.args[2], c.args[3]);
}

static struct tracepoints_table mkp_tracepoints[] = {
{.name = "task_newtask", .func = mkp_task_newtask, .tp = NULL, .policy = MKP_POLICY_TASK_CRED},
};

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(mkp_tracepoints) / sizeof(struct tracepoints_table); i++)

static void lookup_tracepoints(struct tracepoint *tp, void *ignore)
{
	int i;

	FOR_EACH_INTEREST(i) {
		if (strcmp(mkp_tracepoints[i].name, tp->name) == 0)
			mkp_tracepoints[i].tp = tp;
	}
}

/*
 * Find out interesting tracepoints and try to register them.
 * Update policy_ctrl if needed.
 */
static void __init mkp_hookup_tracepoints(void)
{
	int i;
	int ret;

	/* Find out interesting tracepoints */
	for_each_kernel_tracepoint(lookup_tracepoints, NULL);

	/* Update policy control if needed */
	FOR_EACH_INTEREST(i) {
		if (policy_ctrl[i] != 0 && mkp_tracepoints[i].tp == NULL) {
			MKP_ERR("%s not found for policy %d\n",
				mkp_tracepoints[i].name, mkp_tracepoints[i].policy);
			policy_ctrl[i] = 0;
		}
	}

	/* Probing found tracepoints */
	FOR_EACH_INTEREST(i) {
		if (policy_ctrl[i] != 0 && mkp_tracepoints[i].tp != NULL) {
			ret = tracepoint_probe_register(mkp_tracepoints[0].tp,
							mkp_tracepoints[0].func,  NULL);
			if (ret) {
				MKP_ERR("Failed to register %s for policy %d\n",
					mkp_tracepoints[i].name, mkp_tracepoints[i].policy);
				policy_ctrl[i] = 0;
			}
		}
	}
}

int __init mkp_demo_init(void)
{
	int ret = 0, ret_erri_line;
	unsigned long size = 0x100000;
	struct device_node *node;
	u32 mkp_policy = 0x0001ffff;

	node = of_find_node_by_path("/chosen");
	if (node) {
		if (of_property_read_u32(node, "mkp,policy", &mkp_policy) == 0)
			MKP_DEBUG("mkp_policy: %x\n", mkp_policy);
		else
			MKP_WARN("mkp,policy cannot be found, use default\n");

		if (of_property_read_bool(node, "mkp_panic_on"))
			enable_action_panic();
		else
			pr_info("%s: no mkp_panic_on\n", __func__);
	} else
		MKP_WARN("chosen node cannot be found, use default\n");

	MKP_DEBUG("%s: start\n", __func__);
	if (sizeof(phys_addr_t) != sizeof(unsigned long)) {
		MKP_ERR("init mkp failed, sizeof(phys_addr_t) != sizeof(unsigned long)\n");
		return 0;
	}

	/* Set policy control */
	mkp_set_policy(mkp_policy);

	/* Hook up interesting tracepoints and update corresponding policy_ctrl */
	mkp_hookup_tracepoints();

	/* Protect kernel code & rodata */
	if (policy_ctrl[MKP_POLICY_KERNEL_CODE] != 0 ||
		policy_ctrl[MKP_POLICY_KERNEL_RODATA] != 0) {
#if !IS_ENABLED(CONFIG_KASAN_GENERIC) && !IS_ENABLED(CONFIG_KASAN_SW_TAGS)
		ret = mkp_ka_init();
		if (ret) {
			MKP_ERR("mkp_ka_init failed: %d", ret);
			return ret;
		}
		ret = protect_kernel();
#endif
	}

	/* Protect MKP itself */
	if (policy_ctrl[MKP_POLICY_MKP] != 0)
		ret = protect_mkp_self();

	if (policy_ctrl[MKP_POLICY_SELINUX_AVC] != 0) {
		// Create selinux avc sharebuf
		g_ro_avc_handle = mkp_create_ro_sharebuf(MKP_POLICY_SELINUX_AVC, size, &avc_pages);
		if (g_ro_avc_handle != 0) {
			ret = mkp_configure_sharebuf(MKP_POLICY_SELINUX_AVC, g_ro_avc_handle,
				0, 8192 /* avc_sbuf_content */, sizeof(struct avc_sbuf_content)-8);
			rem = do_div(size, sizeof(struct avc_sbuf_content));
			avc_array_sz = size;
		} else {
			MKP_ERR("Create avc ro sharebuf fail\n");
		}
	}

	if (policy_ctrl[MKP_POLICY_SELINUX_AVC]) {
		avc_work = kmalloc(sizeof(struct work_struct), GFP_KERNEL);
	}

	if (policy_ctrl[MKP_POLICY_TASK_CRED] != 0) {
		// Create task cred sharebuf
		size = 0x100000;
		g_ro_cred_handle = mkp_create_ro_sharebuf(MKP_POLICY_TASK_CRED, size, &cred_pages);
		if (g_ro_cred_handle != 0) {
			ret = mkp_configure_sharebuf(MKP_POLICY_TASK_CRED, g_ro_cred_handle,
				0, 32768/* PID_MAX_DEFAULT */, sizeof(struct cred_sbuf_content));
			rem = do_div(size, sizeof(struct cred_sbuf_content));
			cred_array_sz = size;
		} else {
			MKP_ERR("Create cred sharebuf fail\n");
		}
		// register creds vendor hook
		ret = register_trace_android_vh_commit_creds(
				probe_android_vh_commit_creds, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
		ret = register_trace_android_vh_exit_creds(
				probe_android_vh_exit_creds, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
		ret = register_trace_android_vh_override_creds(
				probe_android_vh_override_creds, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
		ret = register_trace_android_vh_revert_creds(
				probe_android_vh_revert_creds, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
	}

	if (policy_ctrl[MKP_POLICY_DRV] != 0 ||
		policy_ctrl[MKP_POLICY_KERNEL_PAGES] != 0 ||
		policy_ctrl[MKP_POLICY_MKP] != 0) {
		// register x, ro, rw, nx
		ret = register_trace_android_vh_set_memory_x(
				probe_android_vh_set_memory_x, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}

		ret = register_trace_android_vh_set_memory_ro(
				probe_android_vh_set_memory_ro, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}

		ret = register_trace_android_vh_set_memory_rw(
				probe_android_vh_set_memory_rw, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
		ret = register_trace_android_vh_set_memory_nx(
				probe_android_vh_set_memory_nx, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
	}

	if (policy_ctrl[MKP_POLICY_DRV] != 0 ||
		policy_ctrl[MKP_POLICY_MKP] != 0) {
		/* register before/after_init */
		ret = register_trace_android_vh_set_module_permit_before_init(
				probe_android_vh_set_module_permit_before_init, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}

		ret = register_trace_android_vh_set_module_permit_after_init(
				probe_android_vh_set_module_permit_after_init, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
	}

	if (policy_ctrl[MKP_POLICY_SELINUX_STATE] != 0) {
		// register selinux_state
		ret = register_trace_android_vh_selinux_is_initialized(
				probe_android_vh_selinux_is_initialized, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
	}

	if (policy_ctrl[MKP_POLICY_TASK_CRED] ||
		policy_ctrl[MKP_POLICY_SELINUX_STATE]) {
		ret = register_trace_android_vh_check_mmap_file(
				probe_android_vh_check_mmap_file, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
		ret = register_trace_android_vh_check_bpf_syscall(
				probe_android_vh_check_bpf_syscall, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
		ret = register_trace_android_vh_check_file_open(
				probe_android_vh_check_file_open, NULL);
		if (ret) {
			ret_erri_line = __LINE__;
			goto failed;
		}
	}
	register_reboot_notifier(&mkp_reboot_notifier);


#if mkp_debug
	mod_timer(&mkp_trace_event_timer, jiffies
		+ MKP_TRACE_EVENT_TIME * HZ);
#endif

failed:
	if (ret)
		MKP_ERR("register hooks failed, ret %d line %d\n", ret, ret_erri_line);
	MKP_DEBUG("%s: Done\n", __func__);

	return 0;
}
