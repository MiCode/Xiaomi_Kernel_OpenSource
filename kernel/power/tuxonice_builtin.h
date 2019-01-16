/*
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 */
#include <asm/setup.h>

extern struct toi_core_fns *toi_core_fns;
extern unsigned long toi_compress_bytes_in, toi_compress_bytes_out;
extern unsigned int nr_hibernates;
extern int toi_in_hibernate;

extern __nosavedata struct pbe *restore_highmem_pblist;

int toi_lowlevel_builtin(void);

#ifdef CONFIG_HIGHMEM
extern __nosavedata struct zone_data *toi_nosave_zone_list;
extern __nosavedata unsigned long toi_nosave_max_pfn;
#endif

extern unsigned long toi_get_nonconflicting_page(void);
extern int toi_post_context_save(void);

extern char toi_wait_for_keypress_dev_console(int timeout);
extern struct block_device *toi_open_by_devnum(dev_t dev);
extern void toi_close_bdev(struct block_device *bdev);
extern int toi_wait;
extern int toi_translate_err_default;
extern int toi_force_no_multithreaded;
extern void toi_read_lock_tasklist(void);
extern void toi_read_unlock_tasklist(void);
extern int toi_in_suspend(void);

#ifdef CONFIG_TOI_ZRAM_SUPPORT
extern int toi_do_flag_zram_disks(void);
#else
#define toi_do_flag_zram_disks() (0)
#endif
