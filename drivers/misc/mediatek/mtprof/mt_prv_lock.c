#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>

#include <linux/pid.h>
#include <linux/debug_locks.h>
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
static ssize_t mt_##name##_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data);\
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

/*************/
/* sample code */
#if 0
static DEFINE_SPINLOCK(mt_spin_lock);
static DEFINE_SEMAPHORE(mtprof_sem_static);
static struct semaphore *mtprof_sem_dyn;
static void sem_down(void)
{
	mtprof_sem_dyn = mt_sema_init(1);
	printk("down mtprof sem static...\n");
	down(&mtprof_sem_static);
	printk("down mtprof sem dyn..\n");
	down(mtprof_sem_dyn);
}

static void sem_up(void)
{
	printk("up mtprof sem dyn..\n");
	up(mtprof_sem_dyn);
	printk("up mtprof sem static...\n");
	up(&mtprof_sem_static);
}

static DEFINE_SPINLOCK(spin_a);
static DEFINE_SPINLOCK(spin_b);
static DEFINE_SPINLOCK(spin_c);
#endif


static DEFINE_MUTEX(mtx_a);
static DEFINE_MUTEX(mtx_b);
static DEFINE_MUTEX(mtx_c);
MT_DEBUG_ENTRY(pvlk);
static int mt_pvlk_show(struct seq_file *m, void *v)
{
	//pr_err(" debug_locks = %d\n", debug_locks);
    SEQ_printf(m,"debug_locks = %d\n", debug_locks);
	return 0;
}

static ssize_t mt_pvlk_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	if (val == 0) {
		debug_locks_off();
	} else if (val == 2) {
		pr_err("==== circular lock test=====\n");
		mutex_lock(&mtx_a);
		mutex_lock(&mtx_b);
		mutex_lock(&mtx_c);
		mutex_unlock(&mtx_c);
		mutex_unlock(&mtx_b);
		mutex_unlock(&mtx_a);

		mutex_lock(&mtx_c);
		mutex_lock(&mtx_a);
		mutex_lock(&mtx_b);
		mutex_unlock(&mtx_b);
		mutex_unlock(&mtx_a);
		mutex_unlock(&mtx_c);

	}
	pr_err("[MT prove locking] debug_locks = %d\n", debug_locks);
	return cnt;
}

static int __init init_pvlk_prof(void)
{
	struct proc_dir_entry *pe;
	pe = proc_create("mtprof/pvlk", 0664, NULL, &mt_pvlk_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}
late_initcall(init_pvlk_prof);
