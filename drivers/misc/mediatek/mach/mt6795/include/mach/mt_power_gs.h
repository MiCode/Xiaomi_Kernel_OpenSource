#ifndef _MT_POWER_GS_H
#define _MT_POWER_GS_H

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/*****************
* extern variable 
******************/
extern struct proc_dir_entry *mt_power_gs_dir;

/*****************
* extern function 
******************/
extern void mt_power_gs_compare(char *, \
                                unsigned int *, unsigned int, \
                                unsigned int *, unsigned int, \
                                unsigned int *, unsigned int, \
                                unsigned int *, unsigned int, \
                                unsigned int *, unsigned int);

extern void mt_power_gs_dump_suspend(void);
extern void mt_power_gs_dump_dpidle(void);
extern void mt_power_gs_dump_idle(void);
extern void mt_power_gs_dump_audio_playback(void);

#endif
