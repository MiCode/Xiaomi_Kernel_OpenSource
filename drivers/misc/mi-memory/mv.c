#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <linux/memblock.h>
#include <linux/byteorder/generic.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include "mv.h"
#include "mi_memory_sysfs.h"
#include "mem_interface.h"

struct MV_UFS *mv_ufs = NULL;

static void mv_ufs_init(void)
{
	u64 raw_device_capacity = 0;
	u16 ufs_id = 0;

	struct ufs_hba *hba = get_ufs_hba_data();

	mv_ufs = kzalloc(sizeof(struct MV_UFS), GFP_KERNEL);

	ufs_get_string_desc(hba, mv_ufs->product_name, (sizeof(mv_ufs->product_name) - 1), DEVICE_DESC_PARAM_PRDCT_NAME, SD_ASCII_STD);
	ufs_get_string_desc(hba, mv_ufs->product_revision, (sizeof(mv_ufs->product_revision) - 1), DEVICE_DESC_PARAM_PRDCT_REV, SD_ASCII_STD);

	ufs_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_MANF_ID, &ufs_id, 2);

	mv_ufs->vendor_id = ufs_id;

	ufs_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, GEOMETRY_DESC_PARAM_DEV_CAP, &raw_device_capacity, 8);
	raw_device_capacity = (raw_device_capacity * 512) / 1024 / 1024 / 1024;
	if (raw_device_capacity > 512 && raw_device_capacity <= 1024) {
		mv_ufs->density = 1024;
	} else if (raw_device_capacity > 256) {
		mv_ufs->density = 512;
	} else if (raw_device_capacity > 128) {
		mv_ufs->density = 256;
	} else if (raw_device_capacity > 64) {
		mv_ufs->density = 128;
	} else if (raw_device_capacity > 32) {
		mv_ufs->density = 64;
	} else if (raw_device_capacity > 16) {
		mv_ufs->density = 32;
	} else if (raw_device_capacity > 8) {
		mv_ufs->density = 8;
	} else {
		mv_ufs->density = 0;
		pr_info("mv unkonwn ufs size %d\n", raw_device_capacity);
	}
}

static int mv_proc_show(struct seq_file *m, void *v)
{
	if (NULL == mv_ufs) {
		mv_ufs_init();
	}

	seq_printf(m, "D: 0x%02x %d\n", get_ddr_id(), get_ddr_size());
	seq_printf(m, "U: 0x%04x %d %s %s\n", mv_ufs->vendor_id, mv_ufs->density, mv_ufs->product_name, mv_ufs->product_revision);

	return 0;
}

static int mv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mv_proc_show, NULL);
}

static const struct proc_ops mv_proc_fops = {
	.proc_open	= mv_proc_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

int add_proc_mv_node(void)
{
	proc_create(MV_NAME, 0555, NULL, &mv_proc_fops);
	return 0;
}

int remove_proc_mv_node(void)
{
	remove_proc_entry(MV_NAME, NULL);
	return 0;
}