/*
 * kernel/power/tuxonice_bio_internal.h
 *
 * Copyright (C) 2009-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * Distributed under GPLv2.
 *
 * This file contains declarations for functions exported from
 * tuxonice_bio.c, which contains low level io functions.
 */

/* Extent chains */
void toi_extent_state_goto_start(void);
void toi_extent_state_save(int slot);
int go_next_page(int writing, int section_barrier);
void toi_extent_state_restore(int slot);
void free_all_bdev_info(void);
int devices_of_same_priority(struct toi_bdev_info *this);
int toi_register_storage_chain(struct toi_bdev_info *new);
int toi_serialise_extent_chains(void);
int toi_load_extent_chains(void);
int toi_bio_rw_page(int writing, struct page *page, int is_readahead, int free_group);
int toi_bio_restore_original_signature(void);
int toi_bio_devinfo_storage_needed(void);
unsigned long get_headerblock(void);
dev_t get_header_dev_t(void);
struct block_device *get_header_bdev(void);
int toi_bio_allocate_storage(unsigned long request);

/* Signature functions */
#define HaveImage "HaveImage"
#define NoImage "TuxOnIce"
#define sig_size (sizeof(HaveImage))

struct sig_data {
	char sig[sig_size];
	int have_image;
	int resumed_before;

	char have_uuid;
	char header_uuid[17];
	dev_t header_dev_t;
	unsigned long first_header_block;

	/* Repeat the signature to be sure we have a header version */
	char sig2[sig_size];
	int header_version;
};

void forget_signature_page(void);
int toi_check_for_signature(void);
int toi_bio_image_exists(int quiet);
int get_signature_page(void);
int toi_bio_mark_resume_attempted(int);
extern char *toi_cur_sig_page;
extern char *toi_orig_sig_page;
int toi_bio_mark_have_image(void);
extern struct sig_data *toi_sig_data;
extern dev_t resume_dev_t;
extern struct block_device *resume_block_device;
extern struct block_device *header_block_device;
extern unsigned long resume_firstblock;

struct block_device *open_bdev(dev_t device, int display_errs);
extern int current_stream;
extern int more_readahead;
int toi_do_io(int writing, struct block_device *bdev, long block0,
	      struct page *page, int is_readahead, int syncio, int free_group);
int get_main_pool_phys_params(void);

void toi_close_bdev(struct block_device *bdev);
struct block_device *toi_open_bdev(char *uuid, dev_t default_device, int display_errs);

extern struct toi_module_ops toi_blockwriter_ops;
void dump_block_chains(void);
void debug_broken_header(void);
extern unsigned long raw_pages_allocd, header_pages_reserved;
int toi_bio_chains_debug_info(char *buffer, int size);
void toi_bio_chains_post_atomic(struct toi_boot_kernel_data *bkd);
int toi_bio_scan_for_image(int quiet);
int toi_bio_get_header_version(void);

void close_resume_dev_t(int force);
int open_resume_dev_t(int force, int quiet);
