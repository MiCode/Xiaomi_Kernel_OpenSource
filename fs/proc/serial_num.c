#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

static char * serialno_get(void)
{
	char * s1= "";
	char  dest[64]="";
	char * s2="not found";

	s1 = strstr(saved_command_line,"uniqueno=");
	if(!s1){
		printk("uniqueno not found in cmdline\n");
		return s2;
	}
	s1 += strlen("uniqueno="); //skip uniqueno=
	strncpy(dest,s1,40); //serialno length=40
	dest[40]='\0';
	s1 = dest;

	return s1;
}

static int serialno_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "0x%s\n", serialno_get());
	return 0;
}

static int serialno_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, serialno_proc_show, NULL);
}

static const struct file_operations serialno_proc_fops = {
	.open		= serialno_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_serialno_init(void)
{
	proc_create("serial_num", 0, NULL, &serialno_proc_fops);
	return 0;
}
fs_initcall(proc_serialno_init);
