/*
 * kernel/power/tuxonice_modules.h
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * It contains declarations for modules. Modules are additions to
 * TuxOnIce that provide facilities such as image compression or
 * encryption, backends for storage of the image and user interfaces.
 *
 */

#ifndef TOI_MODULES_H
#define TOI_MODULES_H

/* This is the maximum size we store in the image header for a module name */
#define TOI_MAX_MODULE_NAME_LENGTH 30

struct toi_boot_kernel_data;

/* Per-module metadata */
struct toi_module_header {
	char name[TOI_MAX_MODULE_NAME_LENGTH];
	int enabled;
	int type;
	int index;
	int data_length;
	unsigned long signature;
};

enum {
	FILTER_MODULE,
	WRITER_MODULE,
	BIO_ALLOCATOR_MODULE,
	MISC_MODULE,
	MISC_HIDDEN_MODULE,
};

enum {
	TOI_ASYNC,
	TOI_SYNC
};

enum {
	TOI_VIRT,
	TOI_PAGE,
};

#define TOI_MAP(type, addr) \
 (type == TOI_PAGE ? kmap(addr) : addr)

#define TOI_UNMAP(type, addr) \
 do { \
   if (type == TOI_PAGE) \
     kunmap(addr); \
 } while (0)

struct toi_module_ops {
	/* Functions common to all modules */
	int type;
	char *name;
	char *directory;
	char *shared_directory;
	struct kobject *dir_kobj;
	struct module *module;
	int enabled, early, initialised;
	struct list_head module_list;

	/* List of filters or allocators */
	struct list_head list, type_list;

	/*
	 * Requirements for memory and storage in
	 * the image header..
	 */
	int (*memory_needed) (void);
	int (*storage_needed) (void);

	int header_requested, header_used;

	int (*expected_compression) (void);
#ifdef CONFIG_TOI_ENHANCE
    int (*actual_compression) (void);
#endif

	/*
	 * Debug info
	 */
	int (*print_debug_info) (char *buffer, int size);
	int (*save_config_info) (char *buffer);
	void (*load_config_info) (char *buffer, int len);

	/*
	 * Initialise & cleanup - general routines called
	 * at the start and end of a cycle.
	 */
	int (*initialise) (int starting_cycle);
	void (*cleanup) (int finishing_cycle);

	void (*pre_atomic_restore) (struct toi_boot_kernel_data *bkd);
	void (*post_atomic_restore) (struct toi_boot_kernel_data *bkd);

	/*
	 * Calls for allocating storage (allocators only).
	 *
	 * Header space is requested separately and cannot fail, but the
	 * reservation is only applied when main storage is allocated.
	 * The header space reservation is thus always set prior to
	 * requesting the allocation of storage - and prior to querying
	 * how much storage is available.
	 */

	unsigned long (*storage_available) (void);
	void (*reserve_header_space) (unsigned long space_requested);
	int (*register_storage) (void);
	int (*allocate_storage) (unsigned long space_requested);
	unsigned long (*storage_allocated) (void);

	/*
	 * Routines used in image I/O.
	 */
	int (*rw_init) (int rw, int stream_number);
	int (*rw_cleanup) (int rw);
	int (*write_page) (unsigned long index, int buf_type, void *buf, unsigned int buf_size);
	int (*read_page) (unsigned long *index, int buf_type, void *buf, unsigned int *buf_size);
	int (*io_flusher) (int rw);

	/* Reset module if image exists but reading aborted */
	void (*noresume_reset) (void);

	/* Read and write the metadata */
	int (*write_header_init) (void);
	int (*write_header_cleanup) (void);

	int (*read_header_init) (void);
	int (*read_header_cleanup) (void);

	/* To be called after read_header_init */
	int (*get_header_version) (void);

	int (*rw_header_chunk) (int rw, struct toi_module_ops *owner,
				char *buffer_start, int buffer_size);

	int (*rw_header_chunk_noreadahead) (int rw,
					    struct toi_module_ops *owner, char *buffer_start,
					    int buffer_size);

	/* Attempt to parse an image location */
	int (*parse_sig_location) (char *buffer, int only_writer, int quiet);

	/* Throttle I/O according to throughput */
	void (*update_throughput_throttle) (int jif_index);

	/* Flush outstanding I/O */
	int (*finish_all_io) (void);

	/* Determine whether image exists that we can restore */
	int (*image_exists) (int quiet);

	/* Mark the image as having tried to resume */
	int (*mark_resume_attempted) (int);

	/* Destroy image if one exists */
	int (*remove_image) (void);

	/* Sysfs Data */
	struct toi_sysfs_data *sysfs_data;
	int num_sysfs_entries;

	/* Block I/O allocator */
	struct toi_bio_allocator_ops *bio_allocator_ops;
};

extern int toi_num_modules, toiNumAllocators;

extern struct toi_module_ops *toiActiveAllocator;
extern struct list_head toi_filters, toiAllocators, toi_modules;

extern void toi_prepare_console_modules(void);
extern void toi_cleanup_console_modules(void);

extern struct toi_module_ops *toi_find_module_given_name(char *name);
extern struct toi_module_ops *toi_get_next_filter(struct toi_module_ops *);

extern int toi_register_module(struct toi_module_ops *module);
extern void toi_move_module_tail(struct toi_module_ops *module);

extern long toi_header_storage_for_modules(void);
extern long toi_memory_for_modules(int print_parts);
extern void print_toi_header_storage_for_modules(void);
extern int toi_expected_compression_ratio(void);
#ifdef CONFIG_TOI_ENHANCE
extern int toi_actual_compression_ratio(void);
#endif

extern int toi_print_module_debug_info(char *buffer, int buffer_size);
extern int toi_register_module(struct toi_module_ops *module);
extern void toi_unregister_module(struct toi_module_ops *module);

extern int toi_initialise_modules(int starting_cycle, int early);
#define toi_initialise_modules_early(starting) \
	toi_initialise_modules(starting, 1)
#define toi_initialise_modules_late(starting) \
	toi_initialise_modules(starting, 0)
extern void toi_cleanup_modules(int finishing_cycle);

extern void toi_post_atomic_restore_modules(struct toi_boot_kernel_data *bkd);
extern void toi_pre_atomic_restore_modules(struct toi_boot_kernel_data *bkd);

extern void toi_print_modules(void);

int toi_get_modules(void);
void toi_put_modules(void);
#endif
