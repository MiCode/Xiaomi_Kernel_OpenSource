/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MMSRAM_H
#define MMSRAM_H

struct mmsram_data {
	void __iomem *paddr;
	void __iomem *vaddr;
	ssize_t size;
};

#if IS_ENABLED(CONFIG_MTK_SLBC)
extern int enable_mmsram(void);
extern void disable_mmsram(void);
extern void mmsram_get_info(struct mmsram_data *data);
extern int mmsram_power_on(void);
extern void mmsram_power_off(void);
extern void mmsram_set_secure(bool secure_on);
#else
static inline int enable_mmsram(void) { return 0; }
static inline void disable_mmsram(void) {}
static inline void mmsram_get_info(struct mmsram_data *data) {}
static inline int mmsram_power_on(void) { return 0; }
static inline void mmsram_power_off(void) {}
static inline void mmsram_set_secure(bool secure_on) {}
#endif /* CONFIG_MTK_SLBC */

#endif

