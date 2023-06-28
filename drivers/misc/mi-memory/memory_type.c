#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <linux/memblock.h>
#include <linux/byteorder/generic.h>
#include <linux/soc/qcom/smem.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/string.h>
#define SMEM_ID_VENDOR2                 136
/* Raw data of DDR manufacturer id(MR5) */
#define HWINFO_DDRID_SAMSUNG    0x01
#define HWINFO_DDRID_HYNIX  0x06
#define HWINFO_DDRID_ELPIDA 0x03
#define HWINFO_DDRID_MICRON 0xFF
#define HWINFO_DDRID_NANYA  0x05
#define HWINFO_DDRID_INTEL  0x0E
#define HWINFO_DDRID_CXMT   0x13
#include <linux/mm.h>
#include <linux/swap.h>
static int memory_type_proc_show(struct seq_file *mem, void *vd)
{
    seq_printf(mem, "EMMC\n");
    return 0;
}
static int memory_type_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, memory_type_proc_show, NULL);
}
static const struct file_operations memory_type_proc_fops = {
    .open       = memory_type_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
static int __init proc_memory_type_init(void)
{
    proc_create("memory_type", 0555, NULL, &memory_type_proc_fops);
    return 0;
}
late_initcall(proc_memory_type_init);
