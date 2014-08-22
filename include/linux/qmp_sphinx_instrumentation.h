/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#ifndef __QMP_SPHINX_LOGK_STUB__
#define __QMP_SPHINX_LOGK_STUB__

#ifdef CONFIG_QMP_CORE
#include <linux/kernel.h>
/*(blksize(256) - hdrsize(60))*/
#define MAX_BUF_SIZE 196

extern void *(*qmp_sphinx_logk_kernel_begin) (char **buf);
extern void (*qmp_sphinx_logk_kernel_end) (void *blck);

static inline void *qmp_sphinx_setup_buf(char **buf)
{
	void *blck;

	if (qmp_sphinx_logk_kernel_begin && qmp_sphinx_logk_kernel_end) {
		blck = qmp_sphinx_logk_kernel_begin(buf);
		if (!*buf) {
			qmp_sphinx_logk_kernel_end(blck);
			return NULL;
		}
	} else {
		return NULL;
	}
	return blck;
}
/*
 * NOTE: only sendto is going to be instrumented
 * since send sys call internally calls sendto
 * with 2 extra parameters
 */
static inline void qmp_sphinx_logk_sendto(int fd, void __user *buff, size_t len,
		unsigned flags, struct sockaddr __user *addr, int addr_len)
{
	char *buf = NULL;
	void *blck = NULL;

	/*sets up buf and blck correctly*/
	blck = qmp_sphinx_setup_buf(&buf);
	if (!blck)
		return;

	/*fill the buf*/
	snprintf(buf, MAX_BUF_SIZE, "-1|kernel|sendto|len=%u,fd=%d|--end",
			(unsigned int)len, fd);

	qmp_sphinx_logk_kernel_end(blck);
}

/*
 * NOTE: only recvfrom is going to be instrumented
 * since recv sys call internally calls recvfrom
 * with 2 extra parameters
 */
static inline void qmp_sphinx_logk_recvfrom(int fd, void __user *ubuf,
		size_t size, unsigned flags, struct sockaddr __user *addr,
		int __user *addr_len)
{
	char *buf = NULL;
	void *blck = NULL;

	/*sets up buf and blck correctly*/
	blck = qmp_sphinx_setup_buf(&buf);
	if (!blck)
		return;

	/*fill the buf*/
	snprintf(buf, MAX_BUF_SIZE, "-1|kernel|recvfrom|size=%u,fd=%d|--end",
			(unsigned int)size, fd);

	qmp_sphinx_logk_kernel_end(blck);
}

static inline void qmp_sphinx_logk_oom_adjust_write(pid_t pid,
					uid_t uid, int oom_adj)
{
	char *buf = NULL;
	void *blck = NULL;

	/*sets up buf and blck correctly*/
	blck = qmp_sphinx_setup_buf(&buf);
	if (!blck)
		return;

	/*fill the buf*/
	snprintf(buf, MAX_BUF_SIZE,
			"-1|kernel|oom_adjust_write|app_uid=%d,app_pid=%d,oom_adj=%d|--end",
			uid, pid, oom_adj);

	qmp_sphinx_logk_kernel_end(blck);
}

static inline void qmp_sphinx_logk_oom_score_adj_write(pid_t pid, uid_t uid,
					int oom_adj_score)
{
	char *buf = NULL;
	void *blck = NULL;

	/*sets up buf and blck correctly*/
	blck = qmp_sphinx_setup_buf(&buf);
	if (!blck)
		return;

	/*fill the buf*/
	snprintf(buf, MAX_BUF_SIZE,
		"-1|kernel|oom_score_adj_write|app_uid=%d,app_pid=%d,oom_adj=%d|--end",
		uid, pid, oom_adj_score);

	qmp_sphinx_logk_kernel_end(blck);
}

#else
static inline void qmp_sphinx_logk_sendto(int fd, void __user *buff,
		size_t len, unsigned flags, struct sockaddr __user *addr,
		int addr_len)
{
}

static inline void qmp_sphinx_logk_recvfrom
		(int fd, void __user *ubuf, size_t size,
		unsigned flags, struct sockaddr __user *addr,
		int __user *addr_len)
{
}

static inline void qmp_sphinx_logk_oom_adjust_write
		(pid_t pid, uid_t uid, int oom_adj)
{
}

static inline void qmp_sphinx_logk_oom_score_adj_write
		(pid_t pid, uid_t uid, int oom_adj_score)
{
}
#endif
#endif
