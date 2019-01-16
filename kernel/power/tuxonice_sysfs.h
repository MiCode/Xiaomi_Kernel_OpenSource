/*
 * kernel/power/tuxonice_sysfs.h
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 */

#include <linux/sysfs.h>

struct toi_sysfs_data {
	struct attribute attr;
	int type;
	int flags;
	union {
		struct {
			unsigned long *bit_vector;
			int bit;
		} bit;
		struct {
			int *variable;
			int minimum;
			int maximum;
		} integer;
		struct {
			long *variable;
			long minimum;
			long maximum;
		} a_long;
		struct {
			unsigned long *variable;
			unsigned long minimum;
			unsigned long maximum;
		} ul;
		struct {
			char *variable;
			int max_length;
		} string;
		struct {
			int (*read_sysfs) (const char *buffer, int count);
			int (*write_sysfs) (const char *buffer, int count);
			void *data;
		} special;
	} data;

	/* Side effects routine. Used, eg, for reparsing the
	 * resume= entry when it changes */
	void (*write_side_effect) (void);
	struct list_head sysfs_data_list;
};

enum {
	TOI_SYSFS_DATA_NONE = 1,
	TOI_SYSFS_DATA_CUSTOM,
	TOI_SYSFS_DATA_BIT,
	TOI_SYSFS_DATA_INTEGER,
	TOI_SYSFS_DATA_UL,
	TOI_SYSFS_DATA_LONG,
	TOI_SYSFS_DATA_STRING
};

#define SYSFS_WRITEONLY 0200
#define SYSFS_READONLY 0444
#define SYSFS_RW 0644

#define SYSFS_BIT(_name, _mode, _ul, _bit, _flags) { \
	.attr = {.name  = _name , .mode   = _mode }, \
	.type = TOI_SYSFS_DATA_BIT, \
	.flags = _flags, \
	.data = { .bit = { .bit_vector = _ul, .bit = _bit } } }

#define SYSFS_INT(_name, _mode, _int, _min, _max, _flags, _wse) { \
	.attr = {.name  = _name , .mode   = _mode }, \
	.type = TOI_SYSFS_DATA_INTEGER, \
	.flags = _flags, \
	.data = { .integer = { .variable = _int, .minimum = _min, \
			.maximum = _max } }, \
	.write_side_effect = _wse }

#define SYSFS_UL(_name, _mode, _ul, _min, _max, _flags) { \
	.attr = {.name  = _name , .mode   = _mode }, \
	.type = TOI_SYSFS_DATA_UL, \
	.flags = _flags, \
	.data = { .ul = { .variable = _ul, .minimum = _min, \
			.maximum = _max } } }

#define SYSFS_LONG(_name, _mode, _long, _min, _max, _flags) { \
	.attr = {.name  = _name , .mode   = _mode }, \
	.type = TOI_SYSFS_DATA_LONG, \
	.flags = _flags, \
	.data = { .a_long = { .variable = _long, .minimum = _min, \
			.maximum = _max } } }

#define SYSFS_STRING(_name, _mode, _string, _max_len, _flags, _wse) { \
	.attr = {.name  = _name , .mode   = _mode }, \
	.type = TOI_SYSFS_DATA_STRING, \
	.flags = _flags, \
	.data = { .string = { .variable = _string, .max_length = _max_len } }, \
	.write_side_effect = _wse }

#define SYSFS_CUSTOM(_name, _mode, _read, _write, _flags, _wse) { \
	.attr = {.name  = _name , .mode   = _mode }, \
	.type = TOI_SYSFS_DATA_CUSTOM, \
	.flags = _flags, \
	.data = { .special = { .read_sysfs = _read, .write_sysfs = _write } }, \
	.write_side_effect = _wse }

#define SYSFS_NONE(_name, _wse) { \
	.attr = {.name  = _name , .mode   = SYSFS_WRITEONLY }, \
	.type = TOI_SYSFS_DATA_NONE, \
	.write_side_effect = _wse, \
}

/* Flags */
#define SYSFS_NEEDS_SM_FOR_READ 1
#define SYSFS_NEEDS_SM_FOR_WRITE 2
#define SYSFS_HIBERNATE 4
#define SYSFS_RESUME 8
#define SYSFS_HIBERNATE_OR_RESUME (SYSFS_HIBERNATE | SYSFS_RESUME)
#define SYSFS_HIBERNATING (SYSFS_HIBERNATE | SYSFS_NEEDS_SM_FOR_WRITE)
#define SYSFS_RESUMING (SYSFS_RESUME | SYSFS_NEEDS_SM_FOR_WRITE)
#define SYSFS_NEEDS_SM_FOR_BOTH \
 (SYSFS_NEEDS_SM_FOR_READ | SYSFS_NEEDS_SM_FOR_WRITE)

int toi_register_sysfs_file(struct kobject *kobj, struct toi_sysfs_data *toi_sysfs_data);
void toi_unregister_sysfs_file(struct kobject *kobj, struct toi_sysfs_data *toi_sysfs_data);

extern struct kobject *tuxonice_kobj;

struct kobject *make_toi_sysdir(char *name);
void remove_toi_sysdir(struct kobject *obj);
extern void toi_cleanup_sysfs(void);

extern int toi_sysfs_init(void);
extern void toi_sysfs_exit(void);
