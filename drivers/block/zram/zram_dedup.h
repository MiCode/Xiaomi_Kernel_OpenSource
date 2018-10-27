#ifndef _ZRAM_DEDUP_H_
#define _ZRAM_DEDUP_H_

struct zram;
struct zram_entry;

#ifdef CONFIG_ZRAM_DEDUP

u64 zram_dedup_dup_size(struct zram *zram);
u64 zram_dedup_meta_size(struct zram *zram);

void zram_dedup_insert(struct zram *zram, struct zram_entry *new,
				u32 checksum);
struct zram_entry *zram_dedup_find(struct zram *zram, struct page *page,
				u32 *checksum);

void zram_dedup_init_entry(struct zram *zram, struct zram_entry *entry,
				unsigned long handle, unsigned int len);
bool zram_dedup_put_entry(struct zram *zram, struct zram_entry *entry);

int zram_dedup_init(struct zram *zram, size_t num_pages);
void zram_dedup_fini(struct zram *zram);
#else

static inline u64 zram_dedup_dup_size(struct zram *zram) { return 0; }
static inline u64 zram_dedup_meta_size(struct zram *zram) { return 0; }

static inline void zram_dedup_insert(struct zram *zram, struct zram_entry *new,
			u32 checksum) { }
static inline struct zram_entry *zram_dedup_find(struct zram *zram,
			struct page *page, u32 *checksum) { return NULL; }

static inline void zram_dedup_init_entry(struct zram *zram,
			struct zram_entry *entry, unsigned long handle,
			unsigned int len) { }
static inline bool zram_dedup_put_entry(struct zram *zram,
			struct zram_entry *entry) { return true; }

static inline int zram_dedup_init(struct zram *zram,
			size_t num_pages) { return 0; }
static inline void zram_dedup_fini(struct zram *zram) { }

#endif

#endif /* _ZRAM_DEDUP_H_ */
