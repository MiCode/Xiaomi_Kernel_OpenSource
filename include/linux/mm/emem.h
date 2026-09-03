/* SPDX-License-Identifier: GPL-2.0*/

#ifndef __ENHANCE_MEMINFO
#define __ENHANCE_MEMINFO

void unisoc_enhanced_show_mem(void);
void unisoc_emem_notify_workfn(struct work_struct *work);

#if IS_ENABLED(CONFIG_UNISOC_MM_ENHANCE_MEMINFO)
int register_unisoc_show_mem_notifier(struct notifier_block *nb);
int unregister_unisoc_show_mem_notifier(struct notifier_block *nb);
#else
static inline int register_unisoc_show_mem_notifier(struct notifier_block *nb)
{
	return 0;
}
static inline int unregister_unisoc_show_mem_notifier(struct notifier_block *nb)
{
	return 0;
}
#endif

#endif
