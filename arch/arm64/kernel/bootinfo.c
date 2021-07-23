#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <asm/setup.h>
#define  HWVERSION_CMDLINE "hwversion="

static int cpumaxfreq_show(struct seq_file *m, void *v)
{

#if defined(CONFIG_MACH_MT6785)
	seq_printf(m, "2.05\n");
#elif defined(CONFIG_MACH_MT6853)
	seq_printf(m, "2.4\n");
#else
	int main_hwid = 0;
	//get hwversion value in cmdline and pick the first number as main_hwid.
	char* hwid_ptr = strstr(saved_command_line, HWVERSION_CMDLINE);
	if (hwid_ptr) {
		sscanf(hwid_ptr, HWVERSION_CMDLINE"%d", &main_hwid);
		pr_info("CPU_MAXFREQ: main hwid = %d", main_hwid);
	}

	//judge the type by main_hwid: 1-J7A, 2-J7B
	if (main_hwid == 1) {
		seq_printf(m, "2.6\n");
	} else if (main_hwid == 2) {
		seq_printf(m, "2.6\n");
	} else {
		seq_printf(m, "unknown\n");
		pr_info("CPU_MAXFREQ: main hwid get error!");
	}
#endif
	return 0;
}

static int cpumaxfreq_open(struct inode *inode, struct file *file)
{
	return single_open(file, &cpumaxfreq_show, NULL);
}

static const struct file_operations proc_cpumaxfreq_operations = {
	.open		= cpumaxfreq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init bootinfo_init(void)
{
	int res = 0;
	struct proc_dir_entry *file;

	file = proc_create("cpumaxfreq", S_IRUGO, NULL, &proc_cpumaxfreq_operations);
	if (!file)
		res = -ENOMEM;
	return res;
}

static void __exit bootinfo_exit(void)
{
	remove_proc_entry("cpumaxfreq", NULL);
	return;
}

core_initcall(bootinfo_init);
module_exit(bootinfo_exit);
