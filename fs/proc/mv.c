#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <soc/qcom/socinfo.h>
#include <linux/memblock.h>
#include <linux/byteorder/generic.h>
#include <linux/soc/qcom/smem.h>


#define SMEM_ID_VENDOR2                 136

/* Raw data of DDR manufacturer id(MR5) */
#define HWINFO_DDRID_SAMSUNG	0x01
#define HWINFO_DDRID_HYNIX	0x06
#define HWINFO_DDRID_ELPIDA	0x03
#define HWINFO_DDRID_MICRON	0xFF
#define HWINFO_DDRID_NANYA	0x05
#define HWINFO_DDRID_INTEL	0x0E

static unsigned char ddr_info;

#include <linux/mm.h>
#include <linux/swap.h>

struct {
	u16 vendor_id;
	u64 density;
	u8 *inquiry;
} mv_ufs;

static int mv_proc_show(struct seq_file *m, void *v)
{
	u8 ddr_size_in_GB = 0;
	u16 ufs_size_in_GB = 0;
	u8 inquiry_tmp[37] = {};
	pr_info("memblock_phys_mem_size %lx\n", (long unsigned int)memblock_phys_mem_size());
	pr_info("memblock_reserved_size %lx\n", (long unsigned int)memblock_reserved_size());
	pr_info("memblock_start_of_DRAM %lx\n", (long unsigned int)memblock_start_of_DRAM());
	pr_info("memblock_end_of_DRAM %lx\n", (long unsigned int)memblock_end_of_DRAM());
	pr_info("geometry  qTotalRawDeviceCapacity %lx\n", (long unsigned int)be64_to_cpu(mv_ufs.density));

	ddr_size_in_GB = (memblock_phys_mem_size() + memblock_reserved_size()) / 1024 / 1024 / 1024;

	if (ddr_size_in_GB > 10 && ddr_size_in_GB <= 12) {
		ddr_size_in_GB = 12;
	} else if (ddr_size_in_GB > 8) {
		ddr_size_in_GB = 10;
	} else if (ddr_size_in_GB > 6) {
		ddr_size_in_GB = 8;
	} else if (ddr_size_in_GB > 4) {
		ddr_size_in_GB = 6;
	} else if (ddr_size_in_GB > 3) {
		ddr_size_in_GB = 4;
	} else if (ddr_size_in_GB > 2) {
		ddr_size_in_GB = 3;
	} else{
		pr_info("mv unkonwn ddr size %d\n", ddr_size_in_GB);
	}

	ufs_size_in_GB = ((be64_to_cpu(mv_ufs.density)) * 512) / 1024 / 1024 / 1024;

	if (ufs_size_in_GB > 512 && ufs_size_in_GB <= 1024) {
		ufs_size_in_GB = 1024;
	} else if (ufs_size_in_GB > 256) {
		ufs_size_in_GB = 512;
	} else if (ufs_size_in_GB > 128) {
		ufs_size_in_GB = 256;
	} else if (ufs_size_in_GB > 64) {
		ufs_size_in_GB = 128;
	} else if (ufs_size_in_GB > 32) {
		ufs_size_in_GB = 64;
	} else if (ufs_size_in_GB > 16) {
		ufs_size_in_GB = 32;
	} else{
		pr_info("mv unkonwn ufs size %d\n", ufs_size_in_GB);
	}

	if (NULL == mv_ufs.inquiry) {
		pr_err("mv_ufs.inquiry is NULL\n");
		return 0;
	}
	memcpy(inquiry_tmp, mv_ufs.inquiry, 36);

#ifdef EXPORT_NAME
	switch (ddr_info) {
	case HWINFO_DDRID_SAMSUNG:
		seq_printf(m, "DDR: Samsung %dGB\n", ddr_size_in_GB); /* 0000 0001B */
		break;
	case HWINFO_DDRID_HYNIX:
		seq_printf(m, "DDR: Hynix %dGB\n", ddr_size_in_GB); /* 0000 0110B */
		break;
	case HWINFO_DDRID_ELPIDA:
		seq_printf(m, "DDR: Elpida %dGB\n", ddr_size_in_GB); /* 0000 0011B */
		break;
	case HWINFO_DDRID_NANYA:
		seq_printf(m, "DDR: Nanya %dGB\n", ddr_size_in_GB); /* 0000 0101B */
		break;
	case HWINFO_DDRID_INTEL:
		seq_printf(m, "DDR: Inte %dGB\n", ddr_size_in_GB); /* 0000 1110B */
		break;
	default:
		seq_printf(m, "DDR: Unknown %x %dGB\n", ddr_info, ddr_size_in_GB);
		break;
	}

	switch (mv_ufs.vendor_id) {
	case 0x0198:
		seq_printf(m, "UFS: Toshiba");
		break;
	case 0x01ce:
		seq_printf(m, "UFS: Samsung");
		break;
	case 0x01ad:
		seq_printf(m, "UFS: Hynix");
		break;
	case 0x0145:
		seq_printf(m, "UFS: Sandisk");
		break;
	case 0x012c:
		seq_printf(m, "UFS: Micron");
		break;
	default:
		seq_printf(m, "UFS: Unknown %x", mv_ufs.vendor_id);
		break;
	}
	seq_printf(m, " %d %s\n", ufs_size_in_GB, inquiry_tmp + 8);
#else

	switch (mv_ufs.vendor_id) {
	case 0x01ce:/*for samsung*/
		ddr_info = 0x01;
		break;
	case 0x01ad:/*for hynix*/
		ddr_info = 0x06;
		break;
	case 0x012c:/*for micron*/
		ddr_info = 0xff;
		break;
	case  0x0145:/*for sandisk*/
		ddr_info = 0xfe;/*0xfe is not really true, only for distinguish with hynix*/
		break;
	default:
		seq_printf(m, "DDR:Unknown, UFS:%x\n", mv_ufs.vendor_id);
		break;
	}
	seq_printf(m, "D: 0x%02x %d\n", (u32)ddr_info, ddr_size_in_GB); /* 0000 0001B */
	seq_printf(m, "U: 0x%04x %d %s\n", (u32)mv_ufs.vendor_id, ufs_size_in_GB, inquiry_tmp + 16); /* 0000 0001B */
#endif
	return 0;
}

void updata_mv_ufs(u16 w_manufacturer_id, u8 *inquiry, u64 qTotalTawDeviceCapacity)
{
	mv_ufs.vendor_id = w_manufacturer_id;
	mv_ufs.density = qTotalTawDeviceCapacity;
	mv_ufs.inquiry = inquiry;
}

static int mv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mv_proc_show, NULL);
}

static const struct file_operations mv_proc_fops = {
	.open		= mv_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_mv_init(void)
{
	proc_create("mv", 0555, NULL, &mv_proc_fops);
	return 0;
}
late_initcall(proc_mv_init);

