#include <linux/types.h>
#include <linux/genhd.h> 
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include <linux/mmc/sd_misc.h>

#if 0
#include <linux/kernel.h>	/* printk() */
#include <linux/module.h>
#include <linux/types.h>	/* size_t */
#include <linux/slab.h>		/* kmalloc() */

#include <linux/mmc/sd_misc.h>
#endif

#define TAG     "[PART_KL]"

#define part_err(fmt, args...)       \
    pr_err(TAG);           \
    pr_cont(fmt, ##args) 
#define part_info(fmt, args...)      \
    pr_notice(TAG);        \
    pr_cont(fmt, ##args)


struct part_t {
    u64 start;
    u64 size;
    u32 part_id;
    u8 name[64];
};


struct hd_struct *get_part(char *name)
{
    dev_t devt;
    int partno;
    struct disk_part_iter piter;
    struct gendisk *disk;
    struct hd_struct *part = NULL; 
    
    if (!name)
        return part;

    devt = blk_lookup_devt("mmcblk0", 0);
    disk = get_gendisk(devt, &partno);

    if (!disk || get_capacity(disk) == 0)
        return 0;

	disk_part_iter_init(&piter, disk, 0);
	while ((part = disk_part_iter_next(&piter))) {
        if (part->info && !strcmp(part->info->volname, name)) {
            get_device(part_to_dev(part));
            break;
        }
	}
	disk_part_iter_exit(&piter);
    
    return part;
}
EXPORT_SYMBOL(get_part);


void put_part(struct hd_struct *part)
{
    disk_put_part(part);
}
EXPORT_SYMBOL(put_part);


static int partinfo_show_proc(struct seq_file *m, void *v)
{
    dev_t devt;
    int partno;
    struct disk_part_iter piter;
    struct gendisk *disk;
    struct hd_struct *part; 
    u64 last = 0;

    devt = blk_lookup_devt("mmcblk0", 0);
    disk = get_gendisk(devt, &partno), 

    seq_printf(m, "%-16s %-16s\t%-16s\n", "Name", "Start", "Size");

    if (!disk || get_capacity(disk) == 0)
        return 0;

	disk_part_iter_init(&piter, disk, 0);
    seq_printf(m, "%-16s 0x%016llx\t0x%016llx\n", "pgpt", 0ULL, 512 * 1024ULL);

    while ((part = disk_part_iter_next(&piter))) {
        seq_printf(m, "%-16s 0x%016llx\t0x%016llx\n", 
            part->info ? (char *)(part->info->volname) : "unknown",
            (u64)part->start_sect * 512,
            (u64)part->nr_sects * 512);
        last = (part->start_sect + part->nr_sects) * 512;
	}

    seq_printf(m, "%-16s 0x%016llx\t0x%016llx\n", "sgpt", last, 512 * 1024ULL);
	disk_part_iter_exit(&piter);

	return 0;
}

static int partinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, partinfo_show_proc, inode);
}

static const struct file_operations partinfo_proc_fops = { 
    .open       = partinfo_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};


static int __init partition_init(void)
{
    struct proc_dir_entry *partinfo_proc;

    partinfo_proc = proc_create("partinfo", 0444, NULL, &partinfo_proc_fops);
    if (!partinfo_proc) {
        part_err("[%s]fail to register /proc/partinfo\n", __func__);
    }

    return 0;
}

static void __exit partition_exit(void)
{
    remove_proc_entry("partinfo", NULL);
}

module_init(partition_init);
module_exit(partition_exit);
