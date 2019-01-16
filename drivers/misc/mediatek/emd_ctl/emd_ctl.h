#ifndef _EXT_EMD_CTL_H_
#define _EXT_EMD_CTL_H_
#include "emd_ctl_hal.h"

#define EMD_MAX_NUM 1
#define EMD_CHR_DEV_MAJOR        (167) // Note, may need to change
#define EMD_CHR_CLIENT_NUM       (8)
#define EMD_CFIFO_NUM            (1) //modem&muxd =>1 cfifo
#define EMD_MSG_INF(tag, fmt, args...)  printk(KERN_ERR "[emd/" tag "]" fmt, ##args)

extern int emd_request_reset(void);
extern int emd_md_exception(void);
extern void request_wakeup_md_timeout(unsigned int dev_id, unsigned int dev_sub_id);
extern void emd_spm_suspend(bool md_power_off);
extern void emd_spm_resume(void);

extern int emd_get_md_id_by_dev_major(int dev_major);
extern int emd_get_dev_id_by_md_id(int md_id, char node_name[], int *major, int* minor);

extern int  emd_dev_node_init(int md_id);
extern void emd_dev_node_exit(int md_id);
extern int  emd_cfifo_init(int md_id);
extern void emd_cfifo_exit(int md_id);
extern int  emd_chr_init(int md_id);
extern void emd_chr_exit(int md_id);
extern int  emd_spm_init(int md_id);
extern void emd_spm_exit(int md_id);
extern int emd_reset_register(char *name);
extern int emd_user_ready_to_reset(int handle);
extern void ext_md_wdt_irq_cb(void);
extern void ext_md_wakeup_irq_cb(void);
extern void ext_md_exception_irq_cb(void);
#endif //_EXT_EMD_CTL_H_