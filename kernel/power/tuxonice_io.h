/*
 * kernel/power/tuxonice_io.h
 *
 * Copyright (C) 2005-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * It contains high level IO routines for hibernating.
 *
 */

#include <linux/utsname.h>
#include "tuxonice_pagedir.h"

/* Non-module data saved in our image header */
struct toi_header {
	/*
	 * Mirror struct swsusp_info, but without
	 * the page aligned attribute
	 */
	struct new_utsname uts;
	u32 version_code;
	unsigned long num_physpages;
	int cpus;
	unsigned long image_pages;
	unsigned long pages;
	unsigned long size;

	/* Our own data */
	unsigned long orig_mem_free;
	int page_size;
	int pageset_2_size;
	int param0;
	int param1;
	int param2;
	int param3;
	int progress0;
	int progress1;
	int progress2;
	int progress3;
	int io_time[2][2];
	struct pagedir pagedir;
	dev_t root_fs;
	unsigned long bkd;	/* Boot kernel data locn */
};

extern int write_pageset(struct pagedir *pagedir);
extern int write_image_header(void);
extern int read_pageset1(void);
extern int read_pageset2(int overwrittenpagesonly);

extern int toi_attempt_to_parse_resume_device(int quiet);
extern void attempt_to_parse_resume_device2(void);
extern void attempt_to_parse_alt_resume_param(void);
int image_exists_read(const char *page, int count);
int image_exists_write(const char *buffer, int count);
extern void save_restore_alt_param(int replace, int quiet);
extern atomic_t toi_io_workers;

/* Args to save_restore_alt_param */
#define RESTORE 0
#define SAVE 1

#define NOQUIET 0
#define QUIET 1

extern dev_t name_to_dev_t(char *line);

extern wait_queue_head_t toi_io_queue_flusher;
extern int toi_bio_queue_flusher_should_finish;

int fs_info_space_needed(void);

extern int toi_max_workers;
