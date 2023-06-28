#ifndef _MI_MEMORY_H_
#define _MI_MEMORY_H_
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/mmc/host.h>
#include <linux/printk.h>
#include <linux/mmc/mmc.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/err.h>
#include <linux/mmc/card.h>
#include<linux/init.h>
#define CID_MANFID_SANDISK      0x2
#define CID_MANFID_SANDISK_EMMC 0x45
#define CID_MANFID_ATP          0x9
#define CID_MANFID_TOSHIBA      0x11
#define CID_MANFID_MICRON       0x13
#define CID_MANFID_SAMSUNG      0x15
#define CID_MANFID_APACER       0x27
#define CID_MANFID_KINGSTON     0x70
#define CID_MANFID_HYNIX        0x90
#define CID_MANFID_NUMONYX      0xFE

#define CID_MANFID_WC           0x45
#define CID_MANFID_UNIC         0x8f
#define CID_MANFID_YMTC         0x9b
#define CID_MANFID_HOSIN        0xD6

#define MMC_CMD_RETRIES         3
#define EXT_CSD_STR_LEN 1025
#define mmc_card_blockaddr(c)   ((c)->state & MMC_STATE_BLOCKADDR)
#define MMC_STATE_BLOCKADDR (1<<2)

/*
 * redefine print
 *
 */
#define PRINT_PRE_HR   "[mi-memory-hr]:"
#define pr_mem_err(fmt, ...)        pr_err(PRINT_PRE_HR fmt, ##__VA_ARGS__)
#define pr_mem_info(fmt, ...)       pr_info(PRINT_PRE_HR fmt, ##__VA_ARGS__)

extern const struct attribute_group mmc_sysfs_group;
extern void mmc_put_card(struct mmc_card *card);
extern void mmc_get_card(struct mmc_card *card);
extern int mmc_set_blocklen(struct mmc_card *card, unsigned int blocklen);
extern int mmc_switch(struct mmc_card *card, u8 set, u8 index, u8 value,
        unsigned int timeout_ms);
extern int mmc_send_status(struct mmc_card *card, u32 *status);
extern void mmc_wait_cmdq_empty(struct mmc_host *host);

struct mcb_spec_t {
    struct mmc_card *card;
    unsigned int manfid;
    int (*get_hr)(struct mmc_card *card, char *buf);
};

struct mcb_t {
    int major;
    int minor;
    struct cdev cdev;
    dev_t devid;
    struct class *mem_class;
    struct device *device;
    /*private*/
    char mcb_private[];
};
#endif/*_MI_MEMORY_H_*/
