/*
 * kernel/power/tuxonice_checksum.h
 *
 * Copyright (C) 2006-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * This file contains data checksum routines for TuxOnIce,
 * using cryptoapi. They are used to locate any modifications
 * made to pageset 2 while we're saving it.
 */

#if defined(CONFIG_TOI_CHECKSUM)
extern int toi_checksum_init(void);
extern void toi_checksum_exit(void);
void check_checksums(void);
int allocate_checksum_pages(void);
void free_checksum_pages(void);
char *tuxonice_get_next_checksum(void);
int tuxonice_calc_checksum(struct page *page, char *checksum_locn);
#else
static inline int toi_checksum_init(void)
{
	return 0;
}

static inline void toi_checksum_exit(void)
{
}

static inline void check_checksums(void)
{
};

static inline int allocate_checksum_pages(void)
{
	return 0;
};

static inline void free_checksum_pages(void)
{
};

static inline char *tuxonice_get_next_checksum(void)
{
	return NULL;
};

static inline int tuxonice_calc_checksum(struct page *page, char *checksum_locn)
{
	return 0;
}
#endif
