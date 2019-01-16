/*
 * kernel/power/tuxonice_storage.h
 *
 * Copyright (C) 2005-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 */

#ifdef CONFIG_NET
int toi_prepare_usm(void);
void toi_cleanup_usm(void);

int toi_activate_storage(int force);
int toi_deactivate_storage(int force);
extern int toi_usm_init(void);
extern void toi_usm_exit(void);
#else
static inline int toi_usm_init(void)
{
	return 0;
}

static inline void toi_usm_exit(void)
{
}

static inline int toi_activate_storage(int force)
{
	return 0;
}

static inline int toi_deactivate_storage(int force)
{
	return 0;
}

static inline int toi_prepare_usm(void)
{
	return 0;
}

static inline void toi_cleanup_usm(void)
{
}
#endif

enum {
	USM_MSG_BASE = 0x10,

	/* Kernel -> Userspace */
	USM_MSG_CONNECT = 0x30,
	USM_MSG_DISCONNECT = 0x31,
	USM_MSG_SUCCESS = 0x40,
	USM_MSG_FAILED = 0x41,

	USM_MSG_MAX,
};
