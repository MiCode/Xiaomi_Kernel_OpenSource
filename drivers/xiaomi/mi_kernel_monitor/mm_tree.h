

#ifndef __MM_TREE_H
#define __MM_TREE_H

#include <linux/radix-tree.h>
#include <linux/slab.h>

struct mm_struct;
struct mm_info {
	struct rcu_head rcu_head;
	pid_t pid;
	struct mm_struct *mm;
	char cgroup_buf[256];
	char argv[256];
};

struct mm_tree {
	struct radix_tree_root mm_tree;
	spinlock_t mm_tree_lock;
};

struct xm_proc_chains_detail;
void init_mm_tree(struct mm_tree *mm_tree);
void cleanup_mm_tree(struct mm_tree *mm_tree);
void free_mm_info(struct rcu_head *rcu);
struct mm_info *find_mm_info(struct mm_tree *mm_tree, struct mm_struct *mm);
void putin_mm_info(struct mm_tree *mm_tree, struct mm_info *mm_info);
struct mm_info *takeout_mm_info(struct mm_tree *mm_tree, struct mm_struct *mm);
void __get_argv_processes(struct mm_tree *mm_tree);
void dump_proc_chains_argv(int style, struct mm_tree *mm_tree,
	struct task_struct *tsk,
	struct xm_proc_chains_detail *detail);
void dump_proc_chains_simple(struct task_struct *tsk,
	struct xm_proc_chains_detail *detail);
void printk_process_chains(struct xm_proc_chains_detail *detail);

struct mm_struct;
struct mm_tree;
int get_argv_from_mm(struct mm_struct *mm, char *buf, size_t size);
void xm_hook_exec(struct linux_binprm *bprm, struct mm_tree *mm_tree);
void xm_hook_process_exit_exec(struct task_struct *tsk, struct mm_tree *mm_tree);

#endif /* __MM_TREE_H */
