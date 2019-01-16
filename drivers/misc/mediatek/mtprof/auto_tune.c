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

#include <linux/delay.h>
#include <linux/slab.h>

//#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt


#define SEQ_printf(m, x...)	    \
 do {			    \
    if (m) {		    \
	seq_printf(m, x);	\
	pr_debug(x);	    \
    } else		    \
	pr_debug(x);	    \
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

static void enable_RA_mode(void)
{
#if 0
	int temp;
	asm volatile ("MRC p15, 0, %0, c1, c0, 1\n" "BIC %0, %0, #1 << 11\n"	/* enable */
		      "BIC %0, %0, #1 << 12\n"	/* enable */
		      "MCR p15, 0, %0, c1, c0, 1\n":"+r" (temp)
 :  : "cc");
	/* printk(KERN_EMERG"temp = 0x%x\n", temp); */
	pr_debug("temp = 0x%x\n", temp);
#endif
}

static void disable_RA_mode(void)
{
#if 0
	int temp;
	asm volatile ("MRC p15, 0, %0, c1, c0, 1\n" "ORR %0, %0, #1 << 11\n"	/* disable */
		      "ORR %0, %0, #1 << 12\n"	/* disable */
		      "MCR p15, 0, %0, c1, c0, 1\n":"+r" (temp)
 :  : "cc");
#endif
}

extern void inner_dcache_flush_all(void);
static char buffer_src[4 * 1024 * 1024 + 16];
static char buffer_dst[4 * 1024 * 1024 + 16];
static int flush_cache;

#include "attu64_test_instr.h"
static void test_instr(int printlog)
{
}

static void test_instr2(int printlog)
{
}

extern bool printk_disable_uart;
int hander_debug = 0;
static void (*test_callback) (int log);
static atomic_t thread_exec_count;


static set_affinity(int cpu)
{
	int mask_len = DIV_ROUND_UP(NR_CPUS, 32) * 9;
	char *mask_str = kmalloc(mask_len, GFP_KERNEL);
	cpumask_t mask = { CPU_BITS_NONE };
	cpumask_set_cpu(cpu, &mask);
	cpumask_scnprintf(mask_str, mask_len, &mask);

	sched_setaffinity(current->pid, &mask);
	/* printk(KERN_EMERG"set cpu %d affinity, mask [%s]\n",cpu, mask_str); */

	kfree(mask_str);
}

static void temp_thread(int cpu)
{
	pr_err("[%d] >>>> Start at CPU:%d\n", cpu, cpu);
	set_affinity(cpu);
	pr_err("[%d] SET affinity done\n", cpu);
	/* Sync exec */

	atomic_inc(&thread_exec_count);
	pr_err("[%d] wait for exec...\n", cpu);
	while (atomic_read(&thread_exec_count) < 8);
	/* Start teh Evaluation. Running the test */
	if (cpu == 0) {
		pr_err("[%d]do the test\n", cpu);
		test_callback(1);
	} else {
		pr_err("[%d]do the test\n", cpu);
		test_callback(0);
	}
	/* End */
	pr_err("[%d] End\n", cpu);
}

MT_DEBUG_ENTRY(attu);
static int mt_attu_show(struct seq_file *m, void *v)
{
	pr_debug(" debug_locks = %d\n", debug_locks);

	return 0;
}

static ssize_t mt_attu_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data)
{
	char buf[64];
	unsigned long val;
	int ret;
	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	printk_disable_uart = 0;
	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;
	if (val == 0) {
		/* debug_locks_off(); */
		flush_cache ^= 0x1;
		printk(KERN_EMERG "switch flusch_cache = %d\n", flush_cache);
	} else if (val == 1) {
		atomic_set(&thread_exec_count, 0);
		kernel_thread(temp_thread, 0, CLONE_FS | CLONE_FILES | SIGCHLD);
		kernel_thread(temp_thread, 1, CLONE_FS | CLONE_FILES | SIGCHLD);
		kernel_thread(temp_thread, 2, CLONE_FS | CLONE_FILES | SIGCHLD);
		kernel_thread(temp_thread, 3, CLONE_FS | CLONE_FILES | SIGCHLD);
		kernel_thread(temp_thread, 4, CLONE_FS | CLONE_FILES | SIGCHLD);
		kernel_thread(temp_thread, 5, CLONE_FS | CLONE_FILES | SIGCHLD);
		kernel_thread(temp_thread, 6, CLONE_FS | CLONE_FILES | SIGCHLD);
		kernel_thread(temp_thread, 7, CLONE_FS | CLONE_FILES | SIGCHLD);
	} else if (val == 2) {
		test_instr(1);
		test_callback = test_instr;
	} else if (val == 3) {
		test_instr_NEON(1);
		test_callback = test_instr_NEON;
	} else if (val == 4) {
	} else if (val == 5) {
	} else if (val == 6) {
		enable_RA_mode();
		pr_err("enable RA mode\n");
	} else if (val == 7) {
		disable_RA_mode();
		pr_err("disable RA mode\n");
	} else if (val == 8) {
		hander_debug = 1;
	} else if (val == 9) {
		hander_debug = 0;
	}
	return cnt;
}

static int __init init_auto_tune(void)
{
	struct proc_dir_entry *pe;
	flush_cache = 1;
	test_callback = test_instr;
	pe = proc_create("mtprof/attu", 0664, NULL, &mt_attu_fops);
	if (!pe)
		return -ENOMEM;
	return 0;
}
late_initcall(init_auto_tune);
