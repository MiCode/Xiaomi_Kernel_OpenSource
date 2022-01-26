#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/reboot.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/memory.h>


static char *fac_lk_check_get(void)
{
	char fac_mode[32];
	char *br_ptr;
	char *br_ptr_e;
	char *match = "1";
	char *mismatch = "0";
	char *get = "G";
#ifdef WT_COMPILE_FACTORY_VERSION
	char *check = "1";
#else
	char *check = "0";
#endif

	memset(fac_mode, 0x0, 32);
	br_ptr = strstr(saved_command_line,"factory_mode=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");

		if (br_ptr_e != 0) {
			strncpy(fac_mode, br_ptr + 13, br_ptr_e - br_ptr - 13);
			fac_mode[br_ptr_e - br_ptr - 13] = '\0';
		}

		if ((memcmp(fac_mode,check,1)) != 0){
			printk("factory_mode mismatch!");
			return mismatch;
		}else
			return match;
	}
	return get;
}

static int fac_lk_check_show(struct seq_file *m, void *v)
{
	seq_printf(m,"%s\n",fac_lk_check_get());
	return 0;

}

static int factory_lk_check_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, fac_lk_check_show, NULL);
}

static const struct file_operations factory_lk_check_proc_fops = {
	.owner      = THIS_MODULE,
	.open		= factory_lk_check_proc_open,
	.read		= seq_read,
};

static int __init proc_factory_lk_check_init(void)
{
	char *flag;
	flag = fac_lk_check_get();
	if((strcmp(flag,"0")) == 0){
		printk("Fatal ERROR:boot failed!ODM Flash protecting!");
		panic("ODM:lk or boot mismatch\n");
	}
	else
		printk("ATO lk check pass!");
	proc_create("factory_mode_lk_check", 0644, NULL, &factory_lk_check_proc_fops);
	return 0;
}
fs_initcall(proc_factory_lk_check_init);
