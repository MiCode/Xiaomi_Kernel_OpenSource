

#ifndef __SYMBOL_H
#define __SYMBOL_H

#include "pub/symbol.h"

struct mutex;
struct stack_trace;
struct pid_namespace;
extern struct mutex *orig_text_mutex;
extern rwlock_t *orig_tasklist_lock;

extern unsigned long (*xm_kallsyms_lookup_name)(const char *name);

extern void (*orig___flush_dcache_area)(void *addr, size_t len);
extern int (*orig_aarch64_insn_patch_text)(void *addrs[], u32 insns[], int cnt);

extern void (*orig___show_regs)(struct pt_regs *regs, int all);
////extern struct list_head *orig_ptype_all;

extern unsigned int (*orig_stack_trace_save_tsk)(struct task_struct *task,
				  unsigned long *store, unsigned int size,
				  unsigned int skipnr);
#ifdef CONFIG_USER_STACKTRACE_SUPPORT
extern unsigned int (*orig_stack_trace_save_user)(unsigned long *store, unsigned int size);
#endif


extern void
(*orig___do_page_fault)(struct pt_regs *regs, unsigned long error_code,
		unsigned long address);
extern struct task_struct *(*orig_find_task_by_vpid)(pid_t nr);
extern struct task_struct *(*orig_find_task_by_pid_ns)(pid_t nr, struct pid_namespace *ns);
extern struct task_struct *(*orig_idle_task)(int cpu);
struct class;
struct device_type;
extern struct class *orig_block_class;
extern struct device_type *orig_disk_type;
struct gendisk;
extern char *(*orig_disk_name)(struct gendisk *hd, int partno, char *buf);
extern int (*orig_access_remote_vm)(struct mm_struct *mm, unsigned long addr,
		void *buf, int len, unsigned int gup_flags);
struct rq;
extern struct rq *orig_runqueues;
struct sched_entity;
extern int (*orig_get_task_type)(struct sched_entity *se);
struct kernfs_node;
extern int (*orig_kernfs_name)(struct kernfs_node *kn, char *buf, size_t buflen);
////extern struct page *(*orig_follow_page_mask)(struct vm_area_struct *vma,
////			      unsigned long address, unsigned int foll_flags,
////			      unsigned int *page_mask);

struct cpuacct;
extern struct cpuacct *orig_root_cpuacct;
struct cgroup_subsys_state;
extern struct cgroup_subsys_state *
(*orig_css_next_descendant_pre)(struct cgroup_subsys_state *pos,
			struct cgroup_subsys_state *root);

struct cgroup_subsys;
extern struct cgroup_subsys *orig_cpuacct_subsys;
extern struct cgroup_subsys_state *
(*orig_css_get_next)(struct cgroup_subsys *ss, int id,
		 struct cgroup_subsys_state *root, int *foundid);

struct files_struct;
extern struct files_struct *(*orig_get_files_struct)(struct task_struct *task);
extern void (*orig_put_files_struct)(struct files_struct *files);

struct dentry;
struct inode;
extern struct dentry * (*orig_d_find_any_alias)(struct inode *inode);
extern int (*orig_task_statm)(struct mm_struct *mm,
			 unsigned long *shared, unsigned long *text,
			 unsigned long *data, unsigned long *resident);

extern unsigned int (*orig_stack_trace_save_tsk)(struct task_struct *task,
				  unsigned long *store, unsigned int size,
				  unsigned int skipnr);
extern unsigned int (*orig_stack_trace_save_user)(unsigned long *store, unsigned int size);

extern int *orig_kptr_restrict;
struct sched_class;
extern struct sched_class *orig_idle_sched_class;
int xiaomi_symbols_init(void);
void xiaomi_symbols_exit(void);

#endif /* __SYMBOL_H */

