/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#if !defined(__MRDUMP_MINI_H__)
struct mrdump_mini_extra_misc {
	void (*dump_func)(unsigned long *vaddr, unsigned long *size);
	const char *dump_name;
	unsigned long max_size;
};

#if IS_ENABLED(CONFIG_HAVE_MTK_GZ_LOG)
extern void get_gz_log_buffer(unsigned long *addr, unsigned long *paddr,
			unsigned long *size, unsigned long *start);
#endif
extern void get_mbootlog_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start);
extern struct mboot_params_buffer *mboot_params_buffer;
extern void aee_rr_get_desc_info(unsigned long *addr, unsigned long *size,
		unsigned long *start);
#ifdef CONFIG_MODULES
void load_ko_addr_list(struct module *module);
void unload_ko_addr_list(struct module *module);
#endif

#endif
