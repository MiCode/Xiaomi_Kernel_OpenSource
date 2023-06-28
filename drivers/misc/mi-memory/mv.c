#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include "mv.h"
struct mv_emmc *mvemmc = NULL;
struct mmc_card *mv_card = NULL;
extern unsigned int get_dram_mr(unsigned int index);
static u8 memblock_mem_size_in_gb(void)
{
    return (u8)((memblock_phys_mem_size() + memblock_reserved_size()) / 1024 / 1024 / 1024);
}
static u8 get_ddr_size(void)
{
    u8 ddr_size_in_GB = 0;
    ddr_size_in_GB = memblock_mem_size_in_gb();
    pr_info("memblock_mem_size mv %lx\n", ddr_size_in_GB);
    if (ddr_size_in_GB > 16 && ddr_size_in_GB <= 18) {
        ddr_size_in_GB = 18;
    } else if (ddr_size_in_GB > 12) {
        ddr_size_in_GB = 16;
    } else if (ddr_size_in_GB > 10) {
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
    } else if (ddr_size_in_GB > 1) {
        ddr_size_in_GB = 2;
    }else {
        ddr_size_in_GB = 0;
    }
    return ddr_size_in_GB;
}
static unsigned int get_ddr_id(void)
{
    return get_dram_mr(5);
}
static void mv_emmc_init(void)
{
    u64 raw_device_capacity = 0;
    mvemmc = kzalloc(sizeof(struct mv_emmc), GFP_ATOMIC);
    mvemmc->vendor_id = (u16)mv_card->cid.manfid;
    memcpy(mvemmc->product_name, mv_card->cid.prod_name, sizeof(mvemmc->product_name));
    if (mv_card->ext_csd.rev < 7) {
        snprintf(mvemmc->product_revision, 3, "0x%x", mv_card->cid.fwrev);
    } else {
        memcpy(mvemmc->product_revision, mv_card->ext_csd.fwrev, sizeof(mvemmc->product_revision));
    }
    raw_device_capacity = mv_card->ext_csd.sectors;
    raw_device_capacity = (raw_device_capacity * 512) / 1024 / 1024 / 1024;
    if (raw_device_capacity > 512 && raw_device_capacity <= 1024) {
        mvemmc->density = 1024;
    } else if (raw_device_capacity > 256) {
        mvemmc->density = 512;
    } else if (raw_device_capacity > 128) {
        mvemmc->density = 256;
    } else if (raw_device_capacity > 64) {
        mvemmc->density = 128;
    } else if (raw_device_capacity > 32) {
        mvemmc->density = 64;
    } else if (raw_device_capacity > 16) {
        mvemmc->density = 32;
    } else if (raw_device_capacity > 8) {
        mvemmc->density = 8;
    } else {
        mvemmc->density = 0;
        pr_info("mv unkonwn emmc size %d\n", raw_device_capacity);
    }
}
static int mv_proc_show(struct seq_file *m, void *v)
{
    if (NULL == mvemmc) {
        mv_emmc_init();
        pr_info("mv emmc init done\n");
    }
    seq_printf(m, "D: 0x%02x %d\n", get_ddr_id(), get_ddr_size());
    seq_printf(m, "E: 0x%04x %llu %s 0x%*phN\n", mvemmc->vendor_id, mvemmc->density, mvemmc->product_name, MMC_FIRMWARE_LEN, mvemmc->product_revision);
    return 0;
}
static int mv_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, mv_proc_show, NULL);
}
static const struct file_operations mv_proc_fops = {
    .open       = mv_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
static int add_proc_mv_node(void)
{
    proc_create(MV_NAME, 0444, NULL, &mv_proc_fops);
    return 0;
}
/*
static int remove_proc_mv_node(void)
{
    remove_proc_entry(MV_NAME, NULL);
    return 0;
}*/
static int __init proc_mv_init(void)
{
    add_proc_mv_node();
    return 0;
}
late_initcall(proc_mv_init);
