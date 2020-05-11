// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#define DFT_TAG "[CONN_MD_DMP]"
#include "conn_md_log.h"

#include "conn_md_dbg.h"
#include "conn_md.h"
#include <linux/version.h>


#define USE_NEW_PROC_FS_FLAG 1
#define CONN_MD_DBG_PROCNAME "driver/conn_md_dbg"

static struct proc_dir_entry *gConnMdDbgEntry;

#if USE_NEW_PROC_FS_FLAG
static const struct  file_operations conn_md_dbg_fops = {
	.read = conn_md_dbg_read,
	.write = conn_md_dbg_write,
};
#endif

static int conn_md_test_dbg(int par1, int par2, int par3);
static int conn_md_dbg_set_log_lvl(int par1, int par2, int par3);
static int conn_md_dbg_dmp_msg_log(int par1, int par2, int par3);

static const  conn_md_dev_dbg_func conn_md_dbg_func[] = {
	conn_md_test_dbg,
	conn_md_dbg_set_log_lvl,
	conn_md_dbg_dmp_msg_log,
	NULL,
};

int conn_md_dbg_dmp_msg_log(int par1, int par2, int par3)
{
	return conn_md_dmp_msg_logged(par2, par3);
}

int conn_md_dbg_set_log_lvl(int par1, int par2, int par3)
{
	return conn_md_log_set_lvl(par2);
}

#if USE_NEW_PROC_FS_FLAG
ssize_t conn_md_dbg_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	return 0;
}
#else
static int conn_md_dbg_read(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = 0;

	return len;
}
#endif

#if USE_NEW_PROC_FS_FLAG
ssize_t conn_md_dbg_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *f_pos)
#else
static int conn_md_dbg_write(struct file *file, const char *buffer,
		unsigned long count, void *data)
#endif
{

	char buf[256];
	char *pBuf;
	unsigned long len = count;
	unsigned long x = 0;
	long y = 0;
	long z = 0;
	long i;
	char *pToken = NULL;
	char *pDelimiter = " \t";

	CONN_MD_INFO_FUNC("write parameter len = %d\n\r", (int)len);
	if (len >= sizeof(buf)) {
		CONN_MD_ERR_FUNC("input handling fail!\n");
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;
	buf[len] = '\0';
	CONN_MD_INFO_FUNC("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken != NULL)
		i = kstrtol(pToken, 16, &x);
	else
		x = 0;

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL)
		i = kstrtol(pToken, 16, &y);
	else
		y = 0;

	pToken = strsep(&pBuf, "\t\n ");
	if (pToken != NULL)
		i = kstrtol(pToken, 16, &z);
	else
		z = 0;

	CONN_MD_INFO_FUNC("x(0x%08lx), y(0x%08lx), z(0x%08lx)\n\r", x, y, z);

	if (ARRAY_SIZE(conn_md_dbg_func) > x && NULL != conn_md_dbg_func[x])
		(*conn_md_dbg_func[x]) (x, y, z);
	else
		CONN_MD_WARN_FUNC("no handler for command id(0x%08lx)\n\r", x);
	return len;
}

int conn_md_test_dbg(int par1, int par2, int par3)
{
	return conn_md_test();
}

int conn_md_dbg_init(void)
{
#if USE_NEW_PROC_FS_FLAG
	gConnMdDbgEntry = proc_create(CONN_MD_DBG_PROCNAME, 0664,
				      NULL, &conn_md_dbg_fops);
	if (gConnMdDbgEntry == NULL) {
		CONN_MD_ERR_FUNC("Unable to create /proc entry\n\r");
		return -1;
	}
#else
	gConnMdDbgEntry = create_proc_entry(CONN_MD_DBG_PROCNAME, 0664, NULL);
	if (gConnMdDbgEntry == NULL) {
		CONN_MD_ERR_FUNC("Unable to create /proc entry\n\r");
		return -1;
	}

	gConnMdDbgEntry->read_proc = conn_md_dbg_read;
	gConnMdDbgEntry->write_proc = conn_md_dbg_write;
#endif
	return 0;

}

int conn_md_dbg_deinit(void)
{

#if USE_NEW_PROC_FS_FLAG
	if (gConnMdDbgEntry != NULL)
		proc_remove(gConnMdDbgEntry);
#else

	if (gConnMdDbgEntry != NULL) {
		remove_proc_entry(CONN_MD_DBG_PROCNAME, NULL);
		return -1;
	}
#endif

	return 0;

}
