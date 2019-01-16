/*
 * kernel/power/tuxonice_cluster.h
 *
 * Copyright (C) 2006-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 */

#ifdef CONFIG_TOI_CLUSTER
extern int toi_cluster_init(void);
extern void toi_cluster_exit(void);
extern void toi_initiate_cluster_hibernate(void);
#else
static inline int toi_cluster_init(void)
{
	return 0;
}

static inline void toi_cluster_exit(void)
{
}

static inline void toi_initiate_cluster_hibernate(void)
{
}
#endif
