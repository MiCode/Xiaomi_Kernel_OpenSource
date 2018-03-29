/*
 * These are exported solely for the purpose of mtd_blkdevs.c and mtdchar.c.
 * You should not use them for _anything_ else.
 */

extern struct mutex mtd_table_mutex;

struct mtd_info *__mtd_next_device(int i);
int add_mtd_device(struct mtd_info *mtd);
int del_mtd_device(struct mtd_info *mtd);
int add_mtd_partitions(struct mtd_info *, const struct mtd_partition *, int);
int del_mtd_partitions(struct mtd_info *);
int parse_mtd_partitions(struct mtd_info *master, const char * const *types,
			 struct mtd_partition **pparts,
			 struct mtd_part_parser_data *data);

int __init init_mtdchar(void);
void __exit cleanup_mtdchar(void);

#define DYNAMIC_CHANGE_MTD_WRITEABLE
#ifdef DYNAMIC_CHANGE_MTD_WRITEABLE	/* tonykuo 2013-11-05 */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

extern ssize_t mtd_writeable_proc_write(struct file *file, const char *buffer, size_t count,
				    loff_t *data);
extern ssize_t mtd_change_proc_write(struct file *file, const char *buffer, size_t count,
				 loff_t *data);
ssize_t mtd_writeable_proc_write(struct file *file, const char *buffer, size_t count, loff_t *data);
ssize_t mtd_change_proc_write(struct file *file, const char *buffer, size_t count, loff_t *data);

struct mtd_change {
	uint64_t size;
	uint64_t offset;
};
#endif
#define mtd_for_each_device(mtd)			\
	for ((mtd) = __mtd_next_device(0);		\
	     (mtd) != NULL;				\
	     (mtd) = __mtd_next_device(mtd->index + 1))
