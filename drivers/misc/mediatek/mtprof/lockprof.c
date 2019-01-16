#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>

#define MAX_AEE_KERNEL_BT 16
#define MAX_AEE_KERNEL_SYMBOL 256

struct aee_process_bt {
	pid_t pid;

	int nr_entries;
	struct {
		unsigned long pc;
		char symbol[MAX_AEE_KERNEL_SYMBOL];
	} entries[MAX_AEE_KERNEL_BT];
};
extern int aed_get_process_bt(struct aee_process_bt *bt);
#include <linux/pid.h>
#define SEQ_printf(m, x...)	    \
 do {			    \
    if (m) {		    \
	seq_printf(m, x);	\
	pr_err(x);	    \
    } else		    \
	pr_err(x);	    \
 } while (0)

#define MT_DEBUG_ENTRY(name) \
static int mt_##name##_show(struct seq_file *m, void *v);\
static int mt_##name##_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data);\
static int mt_##name##_open(struct inode *inode, struct file *file) \
{ \
    return single_open(file, mt_##name##_show, inode->i_private); \
} \
\
static const struct file_operations mt_##name##_fops = { \
    .open = mt_##name##_open, \
    .write = mt_##name##_write,\
    .read = seq_read, \
    .llseek = seq_lseek, \
    .release = single_release, \
};\
void mt_##name##_switch(int on);

#include <linux/mt_export.h>

#if 0
/*
 * Ease the printing of nsec fields:
 */
/*
kernel  back trace utility
*/
static void mt_dump_backtrace_entry(struct seq_file *m, unsigned long where, unsigned long from,
				    unsigned long frame)
{
#ifdef CONFIG_KALLSYMS
	SEQ_printf(m, "[<%08lx>] (%pS) from [<%08lx>] (%pS)\n", where, (void *)where, from,
		   (void *)from);
#else
	SEQ_printf(m, "Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif
}

extern int unwind_frame(struct stackframe *frame);
static void proc_stack(struct seq_file *m, struct task_struct *tsk)
{
	struct stackframe frame;
	register unsigned long current_sp asm("sp");

	if (!tsk)
		tsk = current;

	if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)proc_stack;
	} else {
		/* task blocked in __switch_to */
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		/*
		 * The function calling __switch_to cannot be a leaf function
		 * so LR is recovered from the stack.
		 */
		frame.lr = 0;
		frame.pc = thread_saved_pc(tsk);
	}

	while (1) {
		int urc;
		unsigned long where = frame.pc;

		urc = unwind_frame(&frame);
		if (urc < 0)
			break;
		mt_dump_backtrace_entry(m, where, frame.pc, frame.sp - 4);
	}
}
#endif
static void sem_traverse(struct seq_file *m)
{
	struct task_struct *g, *p;
	struct semaphore *sem;
	char state;
	struct aee_process_bt *pbt;
	int i;
	pbt = kmalloc(sizeof(struct aee_process_bt), GFP_KERNEL);
	if (!pbt) {
		pr_err("malloc fail in sem_traverse\n");
		return;
	}
	pr_err("[sem_traverse]\n");
	read_lock(&tasklist_lock);
	SEQ_printf(m, "============= Semaphore list ===============\n");
	do_each_thread(g, p) {
		/*
		 * It's not reliable to print a task's held locks
		 * if it's not sleeping (or if it's not the current
		 * task):
		 */
#if 0
		if (p->hold_mutex == NULL)
			continue;
		printk("%3d[%d:%s]------------------------\n", i, p->pid, p->comm);

		lock = p->hold_mutex;
		do {
			lock = list_entry(lock->mutex_list.next, struct mutex, mutex_list);
			printk("[%s] - 0x%8x\n", lock->mutex_name, lock);
		} while (lock != p->hold_mutex);
#endif

		if (list_empty(&p->sem_head))
			continue;

		state = (p->state == 0) ? 'R' :
		    (p->state < 0) ? 'U' :
		    (p->state & TASK_UNINTERRUPTIBLE) ? 'D' :
		    (p->state & TASK_STOPPED) ? 'T' :
		    (p->state & TASK_TRACED) ? 'C' :
		    (p->exit_state & EXIT_ZOMBIE) ? 'Z' :
		    (p->exit_state & EXIT_DEAD) ? 'E' : (p->state & TASK_INTERRUPTIBLE) ? 'S' : '?';

		SEQ_printf(m, "------------------------------------\n");
		SEQ_printf(m, "[%d:%s] state:%c\n", p->pid, p->comm, state);
		list_for_each_entry(sem, &p->sem_head, sem_list) {
			SEQ_printf(m, "\nSem Name:[%s], Address:[0x%8x]\nCaller:[%pS]\n",
				   sem->sem_name, (unsigned int)sem, sem->caller);
		}
		if (state != 'R') {
			SEQ_printf(m, "Backtrace:\n");
			pbt->pid = p->pid;
			aed_get_process_bt(pbt);
			for (i = 0; i < pbt->nr_entries - 1; i++) {
				SEQ_printf(m, "  [%d]%s\n", i, (char *)&pbt->entries[i].symbol);
			}
			/* proc_stack(m, p); */
		}
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);
	SEQ_printf(m, "[Semaphore owner list End]\n");
	kfree(pbt);
}

static void lock_traverse(struct seq_file *m)
{
	struct task_struct *g, *p;
	struct mutex *lock;
	char state;
	int i;
	struct aee_process_bt *pbt;

	pbt = kmalloc(sizeof(struct aee_process_bt), GFP_KERNEL);
	if (!pbt) {
		pr_err("malloc fail in mutex_traverse\n");
		return;
	}
	pr_err "[mutex_traverse]\n");
	read_lock(&tasklist_lock);
	SEQ_printf(m, "============== Mutex list ==============\n");
	do_each_thread(g, p) {
		/*
		 * It's not reliable to print a task's held locks
		 * if it's not sleeping (or if it's not the current
		 * task):
		 */
#if 0
		if (p->hold_mutex == NULL)
			continue;
		printk("%3d[%d:%s]------------------------\n", i, p->pid, p->comm);

		lock = p->hold_mutex;
		do {
			lock = list_entry(lock->mutex_list.next, struct mutex, mutex_list);
			printk("[%s] - 0x%8x\n", lock->mutex_name, lock);
		} while (lock != p->hold_mutex);
#endif

		if (list_empty(&p->mutex_head))
			continue;

		state = (p->state == 0) ? 'R' :
		    (p->state < 0) ? 'U' :
		    (p->state & TASK_UNINTERRUPTIBLE) ? 'D' :
		    (p->state & TASK_STOPPED) ? 'T' :
		    (p->state & TASK_TRACED) ? 'C' :
		    (p->exit_state & EXIT_ZOMBIE) ? 'Z' :
		    (p->exit_state & EXIT_DEAD) ? 'E' : (p->state & TASK_INTERRUPTIBLE) ? 'S' : '?';

		SEQ_printf(m, "------------------------------------\n");
		SEQ_printf(m, "[%d:%s] state:%c\n", p->pid, p->comm, state);
		list_for_each_entry(lock, &p->mutex_head, mutex_list) {
			SEQ_printf(m, "\nLock Name:[%s], Address:[0x%8x]\nCaller:[%pS]\n",
				   lock->mutex_name, (unsigned int)lock, (void *)lock->caller);
		}
		if (state != 'R') {
			SEQ_printf(m, "Backtrace:\n");
			pbt->pid = p->pid;
			aed_get_process_bt(pbt);
			for (i = 0; i < pbt->nr_entries - 1; i++) {
				SEQ_printf(m, "  [%d]%s\n", i, (char *)&pbt->entries[i].symbol);
			}
			/* proc_stack(m, p); */
		}
	} while_each_thread(g, p);
	read_unlock(&tasklist_lock);
	SEQ_printf(m, "[Mutex owner list End]\n");
	kfree(pbt);
}

/*************/
/* sample code */
#if 0
static DEFINE_SPINLOCK(mt_spin_lock);
static DEFINE_SEMAPHORE(mtprof_sem_static);
static struct semaphore *mtprof_sem_dyn;
static void sem_down(void) {
	mtprof_sem_dyn = mt_sema_init(1);
	printk("down mtprof sem static...\n");
	down(&mtprof_sem_static);
	printk("down mtprof sem dyn..\n");
	down(mtprof_sem_dyn);
}

static void sem_up(void) {
	printk("up mtprof sem dyn..\n");
	up(mtprof_sem_dyn);
	printk("up mtprof sem static...\n");
	up(&mtprof_sem_static);
}
#endif
MT_DEBUG_ENTRY(locktb);
static int mt_locktb_show(struct seq_file *m, void *v) {
	lock_traverse(m);
	pr_err("\n\n");
	sem_traverse(m);
	return 0;
} static ssize_t mt_locktb_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data) {
	return cnt;
} static int __init init_mtlock_prof(void) {
	struct proc_dir_entry *pe;
	 pe = proc_create("mtprof/locktb", 0664, NULL, &mt_locktb_fops);
	if (!pe)
		 return -ENOMEM;
	 return 0;
} late_initcall(init_mtlock_prof);
