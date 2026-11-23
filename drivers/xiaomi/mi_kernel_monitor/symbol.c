

#include <linux/kallsyms.h>

#include "internal.h"

struct mutex *orig_text_mutex;
rwlock_t *orig_tasklist_lock;

struct mutex *orig_tracepoint_module_list_mutex;
struct list_head *orig_tracepoint_module_list;

void (*orig___flush_dcache_area)(void *addr, size_t len);
int (*orig_aarch64_insn_patch_text)(void *addrs[], u32 insns[], int cnt);

////struct list_head *orig_ptype_all;

void (*orig___show_regs)(struct pt_regs *regs, int all);
void (*orig_save_stack_trace_tsk)(struct task_struct *tsk, struct stack_trace *trace);
void (*orig___do_page_fault)(struct pt_regs *regs, unsigned long error_code,
                unsigned long address);

struct class *orig_block_class;
struct device_type *orig_disk_type;
char *(*orig_disk_name)(struct gendisk *hd, int partno, char *buf);
struct task_struct *(*orig_find_task_by_vpid)(pid_t nr);
struct task_struct *(*orig_find_task_by_pid_ns)(pid_t nr, struct pid_namespace *ns);

int (*orig_access_remote_vm)(struct mm_struct *mm, unsigned long addr,
		void *buf, int len, unsigned int gup_flags);
struct task_struct *(*orig_idle_task)(int cpu);
struct rq *orig_runqueues;
int (*orig_get_task_type)(struct sched_entity *se);
int (*orig_kernfs_name)(struct kernfs_node *kn, char *buf, size_t buflen);

struct cpuacct *orig_root_cpuacct;
struct cgroup_subsys_state *
(*orig_css_next_descendant_pre)(struct cgroup_subsys_state *pos,
			struct cgroup_subsys_state *root);

struct cgroup_subsys *orig_cpuacct_subsys;
struct cgroup_subsys_state *
(*orig_css_get_next)(struct cgroup_subsys *ss, int id,
		 struct cgroup_subsys_state *root, int *foundid);

struct files_struct *(*orig_get_files_struct)(struct task_struct *task);
void (*orig_put_files_struct)(struct files_struct *files);

struct page *(*orig_follow_page_mask)(struct vm_area_struct *vma,
			      unsigned long address, unsigned int foll_flags,
			      unsigned int *page_mask);

unsigned int (*orig_stack_trace_save_tsk)(struct task_struct *task,
				  unsigned long *store, unsigned int size,
				  unsigned int skipnr);
unsigned int (*orig_stack_trace_save_user)(unsigned long *store, unsigned int size);

struct dentry * (*orig_d_find_any_alias)(struct inode *inode);

int (*orig_task_statm)(struct mm_struct *mm,
			 unsigned long *shared, unsigned long *text,
			 unsigned long *data, unsigned long *resident);
struct sched_class *orig_idle_sched_class;


int *orig_kptr_restrict;

static int lookup_syms(void)
{
	printk("mi_kernel lookup_syms begin.\n");
	LOOKUP_SYMS(text_mutex);
	LOOKUP_SYMS(tasklist_lock);
	LOOKUP_SYMS(stack_trace_save_tsk);
	LOOKUP_SYMS(aarch64_insn_patch_text);
	LOOKUP_SYMS(runqueues);
	LOOKUP_SYMS(block_class);
	LOOKUP_SYMS(disk_type);
	LOOKUP_SYMS(access_remote_vm);
	LOOKUP_SYMS(idle_task);
	LOOKUP_SYMS(task_statm);
	LOOKUP_SYMS(kptr_restrict);
	LOOKUP_SYMS(idle_sched_class);
	LOOKUP_SYMS_NORET(d_find_any_alias);

	LOOKUP_SYMS_NORET(find_task_by_vpid);
	LOOKUP_SYMS_NORET(find_task_by_pid_ns);
	LOOKUP_SYMS_NORET(get_task_type);

	LOOKUP_SYMS_NORET(kernfs_name);
	LOOKUP_SYMS_NORET(root_cpuacct);
	LOOKUP_SYMS_NORET(css_next_descendant_pre);
	LOOKUP_SYMS_NORET(cpuacct_subsys);
	LOOKUP_SYMS_NORET(css_get_next);
	printk("mi_kernel lookup_syms end.\n");

	return 0;
}

int xiaomi_symbols_init(void)
{
	int ret;

	ret = lookup_syms();
	if (ret)
		return ret;

	printk("mi_kernel xiaomi_symbols_init begin.\n");
	LOOKUP_SYMS(__show_regs);
	LOOKUP_SYMS(tracepoint_module_list_mutex);
	LOOKUP_SYMS(tracepoint_module_list);
	printk("mi_kernel xiaomi_symbols_init end\n");

	return 0;
}

void xiaomi_symbols_exit(void)
{
	//TO DO
}
