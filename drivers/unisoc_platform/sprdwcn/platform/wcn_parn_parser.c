/*
 * Copyright (c) 2017 Spreadtrum
 *
 * WCN partition parser for different CPU have different path with EMMC and NAND
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/dirent.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fsnotify.h>
#include <linux/fs_struct.h>
#include <linux/module.h>
#include <linux/path.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "mdbg_type.h"
#include "../include/wcn_dbg.h"
#include "wcn_parn_parser.h"

#if 0

#define ROOT_PATH "/"
#define ETC_PATH "/etc"
#define VENDOR_ETC_PATH "/vendor/etc"
#define ETC_FSTAB "/etc/fstab"
#define FSTAB_PATH_NUM 3
#define CONF_COMMENT '#'
#define CONF_LF '\n'
#define CONF_DELIMITERS " =\n\r\t"
#define CONF_VALUES_DELIMITERS "=\n\r\t"
#define CONF_MAX_LINE_LEN 255
#define WCN_MODEM_NAME "wcnmodem"

static const char *prefix = "fstab.s";
static char fstab_name[128];
static char fstab_dir[FSTAB_PATH_NUM][32] = {
			ROOT_PATH, ETC_PATH, VENDOR_ETC_PATH};

static char *fgets(char *buf, int buf_len, struct file *fp)
{
	int ret = 0;
	int i = 0;
#if 0
	ret = kernel_read(fp, buf, buf_len, &fp->f_pos);
#endif
	if (ret <= 0)
		return NULL;

	while (buf[i++] != '\n' && i < ret)
		;

	if (i <= ret)
		fp->f_pos += i;
	else
		return NULL;

	if (i < buf_len)
		buf[i] = 0;

	return buf;
}

static int load_fstab_conf(const char *p_path, char *wcn_path)
{
	struct file *p_file = NULL;
	char *p_name;
	char line[CONF_MAX_LINE_LEN + 1];
	char *p;
	char *temp;
	bool match_flag;
	int off;

	match_flag = false;
	p = line;
	WCN_INFO("Attempt to load conf from %s\n", p_path);
#if 0
	p_file = filp_open(p_path, O_RDONLY, 0);
#endif
	if (IS_ERR(p_file)) {
		WCN_ERR("open file %s error not find\n",
			p_path);
		return PTR_ERR(p_file);
	}

	/* read line by line */
	while (fgets(line, CONF_MAX_LINE_LEN+1, p_file) != NULL) {

		if ((line[0] == CONF_COMMENT) || (line[0] == CONF_LF))
			continue;

		p = line;
		p_name = strsep(&p, CONF_DELIMITERS);
		if (p_name != NULL) {
			temp = strstr(p_name, "userdata");
			if (temp != NULL) {
				off = snprintf(wcn_path, strlen(p_name) + 1,
					       "%s", p_name);
				off -= strlen(temp);
				snprintf(wcn_path + off, strlen(WCN_MODEM_NAME)
					 + 1, "%s", WCN_MODEM_NAME);
				match_flag = true;
				break;
			}
		}
	}
#if 0
	filp_close(p_file, NULL);
#endif
	if (match_flag)
		return 0;
	else
		return -1;
}

static int prefixcmp(const char *str, const char *prefixc)
{
	for (; ; str++, prefixc++)
		if (!*prefixc)
			return 0;
		else if (*str != *prefixc)
			return (unsigned char)*prefixc - (unsigned char)*str;
}

static int find_callback(struct dir_context *ctx, const char *name, int namlen,
		     loff_t offset, u64 ino, unsigned int d_type)
{
	int tmp;

	tmp = prefixcmp(name, prefix);
	if (tmp == 0) {
		if (sizeof(fstab_name) > strlen(fstab_name) + strlen(name) + 2)
			strcat(fstab_name, name);
		WCN_INFO("full fstab name %s\n", fstab_name);
	}

	return 0;
}

static struct dir_context ctx =  {
	.actor = find_callback,
};
#endif

int parse_firmware_path(char *firmware_path)
{
	u32 ret = 0;

	WCN_INFO("%s entry\n", __func__);
	ret = -1;
#if 0
	u32 loop;
	struct file *file1;

	WCN_INFO("%s entry\n", __func__);
	for (loop = 0; loop < FSTAB_PATH_NUM; loop++) {
		file1 = NULL;
		WCN_DEBUG("dir:%s: loop:%d\n", fstab_dir[loop], loop);
#if 0
		file1 = filp_open(fstab_dir[loop], O_DIRECTORY, 0);
#endif
		if (IS_ERR(file1)) {
			WCN_ERR("%s open error:%d\n",
				fstab_dir[loop], IS_ERR(file1));
			continue;
		}
		memset(fstab_name, 0, sizeof(fstab_name));
		strncpy(fstab_name, fstab_dir[loop], strlen(fstab_dir[loop]));
		if (strlen(fstab_name) > 1)
			fstab_name[strlen(fstab_name)] = '/';
#if 0
		ret = iterate_dir(file1, &ctx);
#endif
		if (!ret) {
			WCN_ERR("iterate_dir ret %d\n", ret);
			return 0;
		}
		fput(file1);
		ret = load_fstab_conf(fstab_name, firmware_path);
		WCN_INFO("%s:load conf ret %d\n", fstab_dir[loop], ret);
		if (!ret) {
			WCN_INFO("[%s]:%s\n", fstab_name, firmware_path);
			return 0;
		}
	}
	/* for yunos */
	ret = load_fstab_conf(ETC_FSTAB, firmware_path);
	if (!ret) {
		WCN_INFO(ETC_FSTAB":%s !\n", firmware_path);
		return 0;
	}
#endif
	return ret;
}

