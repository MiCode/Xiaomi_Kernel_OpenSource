/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __RECLAIM_H__
#define __RECLAIM_H__

#if IS_ENABLED(CONFIG_UNISOC_MM_SHRINKLRU)
int kshrink_lruvec_init(void);
void kshrink_lruvec_exit(void);
#else
static inline int kshrink_lruvec_init(void)
{
	return 0;
}
static inline int kshrink_lruvec_exit(void)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_UNISOC_MM_SHRINKSLAB)
int kshrink_slabd_async_init(void);
void kshrink_slabd_async_exit(void);
#else
static inline int kshrink_slabd_async_init(void)
{
        return 0;
}
static inline int kshrink_slabd_async_exit(void)
{
        return 0;
}
#endif

#if IS_ENABLED(CONFIG_UNISOC_MM_DIRECT_SWAPPINESS)
int unisoc_enhance_reclaim_init(void);
void unisoc_enhance_reclaim_exit(void);
#else
static inline int unisoc_enhance_reclaim_init(void)
{
	return 0;
}
static inline void unisoc_enhance_reclaim_exit(void) { }
#endif

#if IS_ENABLED(CONFIG_UNISOC_MM_SHRINK_ANON)
void shrink_anon_init(void);
void shrink_anon_exit(void);
#else
static inline void shrink_anon_init(void) { }
static inline void shrink_anon_exit(void) { }
#endif

#endif
