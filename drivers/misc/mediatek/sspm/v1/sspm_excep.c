// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <mt-plat/sync_write.h>
#include "sspm_define.h"
#include "sspm_excep.h"
#include "sspm_helper.h"
#include "sspm_sysfs.h"

#if SSPM_COREDUMP_SUPPORT
struct coredump_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int db_ofs;
	unsigned int db_size;
	unsigned int buff_ofs;
	unsigned int buff_size;
};
#endif

#if SSPM_COREDUMP_SUPPORT
struct coredump_ctrl_s *sspm_cd_ctl;
static unsigned int sspm_cd_exists;
#endif

#if SSPM_COREDUMP_SUPPORT
void sspm_log_coredump_recv(unsigned int exists)
{
	sspm_cd_exists = exists;
}

static ssize_t sspm_aee_show(struct device *kobj, struct device_attribute *attr,
	char *buf)
{
	unsigned int ret;

	if (!sspm_cd_exists)
		return 0;

	ret = sspm_cd_ctl->db_size;

	memcpy_fromio(buf, ((unsigned char *) sspm_cd_ctl) +
			sspm_cd_ctl->db_ofs, ret);

	return ret;
}

DEVICE_ATTR_RO(sspm_aee);

static ssize_t sspm_coredump_read(struct file *filep, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t offset, size_t size)
{
	if (!sspm_cd_exists)
		return 0;
	if (offset >= SSPM_COREDUMP_SIZE)
		return 0;
	if ((offset + size) >= SSPM_COREDUMP_SIZE)
		size = SSPM_COREDUMP_SIZE - offset;

	memcpy_fromio(buf, ((unsigned char *) sspm_cd_ctl) +
			sspm_cd_ctl->buff_ofs + offset, size);

	return size;
}

BIN_ATTR_RO(sspm_coredump, 0);
#endif
/*
 * generate an exception according to exception type
 * @param type: exception type
 */
void sspm_aed(enum sspm_excep_id type)
{

}

unsigned int __init sspm_coredump_init(phys_addr_t start, phys_addr_t limit)
{
#if SSPM_COREDUMP_SUPPORT
	unsigned int last_ofs;

	sspm_cd_ctl = (struct coredump_ctrl_s *) start;
	sspm_cd_ctl->base = PLT_COREDUMP_READY; /* magic */

	last_ofs = sizeof(*sspm_cd_ctl);
	sspm_cd_ctl->size = last_ofs;

	sspm_cd_ctl->db_ofs = last_ofs;
	sspm_cd_ctl->db_size = SSPM_DB_SIZE;
	last_ofs += sspm_cd_ctl->db_size;

	sspm_cd_ctl->buff_ofs = last_ofs;
	sspm_cd_ctl->buff_size = SSPM_COREDUMP_SIZE;
	last_ofs += sspm_cd_ctl->buff_size;

	return last_ofs;
#else
	return 0;
#endif
}

int __init sspm_coredump_init_done(void)
{
#if SSPM_COREDUMP_SUPPORT
	int ret;

	if (sspm_cd_ctl) {
		ret = sspm_sysfs_create_file(&dev_attr_sspm_aee);

		if (unlikely(ret != 0))
			return ret;

		ret = sspm_sysfs_create_bin_file(&bin_attr_sspm_coredump);

		if (unlikely(ret != 0))
			return ret;
	}
#endif
	return 0;
}

/*
 * init excep for sspm
 * @return: 0 if success
 */
int __init sspm_excep_init(void)
{
	return 0;
}
