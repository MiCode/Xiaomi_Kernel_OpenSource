// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/rtc.h>

#include <linux/module.h>

#include <linux/trace_events.h>

#include "ged_base.h"
#include "ged_log.h"
#include "ged_debugFS.h"
#include "ged_hashtable.h"

enum {
	/* 0x00 - 0xff reserved for internal buffer type */

	/* rewrite the oldest log when buffer is full */
	GED_LOG_ATTR_RINGBUFFER     = 0x1,
	/* stop logging when buffer is full */
	GED_LOG_ATTR_QUEUEBUFFER    = 0x2,
	/* increase buffersize when buffer is full */
	GED_LOG_ATTR_AUTO_INCREASE  = 0x4,
};

struct GED_LOG_BUF_LINE {
	int         offset;
	int         tattrs;
	long long   time;
	long	    time_usec;
	int         pid;
	int         tid;
};

struct GED_LOG_BUF {
	GED_LOG_BUF_TYPE    eType;
	int                 attrs;

	void                *pMemory;
	int                 i32MemorySize;

	struct GED_LOG_BUF_LINE    *psLine;
	char                *pcBuffer;
	int                 i32LineCount;
	int                 i32BufferSize;
	int                 i32LineCurrent;
	int                 i32BufferCurrent;

	spinlock_t          sSpinLock;
	unsigned long       ulIRQFlags;

	char                acName[GED_LOG_BUF_NAME_LENGTH];
	char                acNodeName[GED_LOG_BUF_NODE_NAME_LENGTH];

	struct dentry      *psEntry;

	struct list_head    sList;

	unsigned long       ulHashNodeID;

};

struct GED_LOG_LISTEN {
	GED_LOG_BUF_HANDLE  *pCBHnd;
	char                acName[GED_LOG_BUF_NAME_LENGTH];
	struct list_head    sList;
};

struct GED_LOG_BUF_LIST {
	rwlock_t sLock;
	struct list_head sList_buf;
	struct list_head sList_listen;
};

static struct GED_LOG_BUF_LIST gsGEDLogBufList = {
	.sLock          = __RW_LOCK_UNLOCKED(gsGEDLogBufList.sLock),
	.sList_buf      = LIST_HEAD_INIT(gsGEDLogBufList.sList_buf),
	.sList_listen   = LIST_HEAD_INIT(gsGEDLogBufList.sList_listen),
};

static struct dentry *gpsGEDLogEntry;
static struct dentry *gpsGEDLogBufsDir;

static GED_HASHTABLE_HANDLE ghHashTable;

unsigned int ged_log_trace_enable;
unsigned int ged_log_perf_trace_enable;

//-----------------------------------------------------------------------------
//
//  GED Log Buf
//
//-----------------------------------------------------------------------------
static struct GED_LOG_BUF *ged_log_buf_from_handle(GED_LOG_BUF_HANDLE hLogBuf)
{
	return ged_hashtable_find(ghHashTable, (unsigned long)hLogBuf);
}

static
GED_ERROR __ged_log_buf_vprint(struct GED_LOG_BUF *psGEDLogBuf,
	const char *fmt, va_list args, int attrs)
{
	struct GED_LOG_BUF_LINE *curline;
	int buf_n;
	int len;

	if (!psGEDLogBuf)
		return GED_OK;

	spin_lock_irqsave(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ulIRQFlags);

	/* if OOM */
	if (psGEDLogBuf->i32LineCurrent >= psGEDLogBuf->i32LineCount ||
			psGEDLogBuf->i32BufferCurrent + 256
			> psGEDLogBuf->i32BufferSize) {
		if (attrs & GED_LOG_ATTR_RINGBUFFER) {
			/* for ring buffer, we start over. */
			psGEDLogBuf->i32LineCurrent = 0;
			psGEDLogBuf->i32BufferCurrent = 0;
		} else if (attrs & GED_LOG_ATTR_QUEUEBUFFER) {
			if (attrs & GED_LOG_ATTR_AUTO_INCREASE) {
				int newLineCount, newBufferSize;

				/* incease min(25%, 1MB) */
				if ((psGEDLogBuf->i32LineCount >> 2)
					<= 1024 * 1024) {
					newLineCount = psGEDLogBuf->i32LineCount
						+ (psGEDLogBuf->i32LineCount
							>> 2);
					newBufferSize =
						psGEDLogBuf->i32BufferSize
						+ (psGEDLogBuf->i32BufferSize
							>> 2);
				} else {
					newLineCount =
						psGEDLogBuf->i32LineCount
						+ 4096;
					newBufferSize =
						psGEDLogBuf->i32BufferSize
						+ 1024 * 1024;
				}

				spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
					psGEDLogBuf->ulIRQFlags);
				if (ged_log_buf_resize(
					psGEDLogBuf->ulHashNodeID,
					newLineCount, newBufferSize) != GED_OK)
					return GED_ERROR_OOM;
				spin_lock_irqsave(&psGEDLogBuf->sSpinLock,
					psGEDLogBuf->ulIRQFlags);
			} else {
				/* for queuebuffer only, we skip the log. */
				spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
					psGEDLogBuf->ulIRQFlags);
				return GED_ERROR_OOM;
			}
		}
	}

	curline = &psGEDLogBuf->psLine[psGEDLogBuf->i32LineCurrent];

	curline->offset = psGEDLogBuf->i32BufferCurrent;
	curline->tattrs = 0;
	curline->time = 0;

	/* record the kernel time */
	if (attrs & GED_LOG_ATTR_TIME) {
		curline->tattrs = GED_LOG_ATTR_TIME;
		curline->time = ged_get_time();
	}

	/* record the user time */
	if (attrs & GED_LOG_ATTR_TIME_TPT) {
		struct timeval time;
		unsigned long local_time;

		do_gettimeofday(&time);
		local_time = (u32)(time.tv_sec - (sys_tz.tz_minuteswest * 60));

		curline->tattrs = GED_LOG_ATTR_TIME_TPT;
		curline->time = local_time;
		curline->time_usec = time.tv_usec;
		curline->pid = current->tgid;
		curline->tid = current->pid;
	}

	buf_n = psGEDLogBuf->i32BufferSize - psGEDLogBuf->i32BufferCurrent;
	len = vsnprintf(psGEDLogBuf->pcBuffer + psGEDLogBuf->i32BufferCurrent,
		buf_n, fmt, args);

	/* if 'len' >= 'buf_n', the resulting string is truncated.
	 * let 'len' be a safe number
	 */
	if (len > buf_n)
		len = buf_n;

	if (psGEDLogBuf->pcBuffer[psGEDLogBuf->i32BufferCurrent + len - 1]
		== '\n') {
		/* remove tailing newline */
		psGEDLogBuf->pcBuffer[psGEDLogBuf->i32BufferCurrent + len - 1] =
			0;
		len -= 1;
	}

	buf_n -= len;

	if (attrs & GED_LOG_ATTR_RINGBUFFER) {
		int i;
		int check = 10 + 1; /* we check the following 10 items. */
		int a = psGEDLogBuf->i32BufferCurrent;
		int b = psGEDLogBuf->i32BufferCurrent + len + 2;

		for (i = psGEDLogBuf->i32LineCurrent+1;
			--check && i < psGEDLogBuf->i32LineCount; ++i) {
			int pos = psGEDLogBuf->psLine[i].offset;

			if (pos >= a && pos < b)
				psGEDLogBuf->psLine[i].offset = -1;
		}

		if (check && i == psGEDLogBuf->i32LineCount) {
			for (i = 0; --check && i < psGEDLogBuf->i32LineCurrent;
				++i) {
				int pos = psGEDLogBuf->psLine[i].offset;

				if (pos >= a && pos < b)
					psGEDLogBuf->psLine[i].offset = -1;
			}
		}
	}

	/* update current */
	psGEDLogBuf->i32BufferCurrent += len + 2;
	psGEDLogBuf->i32LineCurrent += 1;

	spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
		psGEDLogBuf->ulIRQFlags);

	return GED_OK;
}

static
GED_ERROR __ged_log_buf_print(struct GED_LOG_BUF *psGEDLogBuf,
	const char *fmt, ...)
{
	va_list args;
	GED_ERROR err;

	va_start(args, fmt);
	err = __ged_log_buf_vprint(psGEDLogBuf, fmt, args,
		psGEDLogBuf->attrs | GED_LOG_ATTR_TIME);
	va_end(args);

	return err;
}

static int __ged_log_buf_write(struct GED_LOG_BUF *psGEDLogBuf,
	const char __user *pszBuffer, int i32Count)
{
	int cnt;
	char buf[256];

	if (!psGEDLogBuf)
		return 0;


	cnt = (i32Count >= 256) ? 255 : i32Count;

	ged_copy_from_user(buf, pszBuffer, cnt);

	buf[cnt] = 0;

	__ged_log_buf_print(psGEDLogBuf, "%s", buf);

	return cnt;
}

static int __ged_log_buf_check_get_early_list(GED_LOG_BUF_HANDLE hLogBuf,
	const char *pszName)
{
	struct list_head *psListEntry, *psListEntryTemp, *psList;
	struct GED_LOG_LISTEN *psFound = NULL, *psLogListen = NULL;

	read_lock_bh(&gsGEDLogBufList.sLock);

	psList = &gsGEDLogBufList.sList_listen;
	list_for_each_safe(psListEntry, psListEntryTemp, psList) {
		psLogListen = list_entry(psListEntry,
			struct GED_LOG_LISTEN, sList);
		if ((pszName != NULL)
			&& (psLogListen != NULL)
			&& strcmp(psLogListen->acName, pszName) == 0) {
			psFound = psLogListen;
			break;
		}
	}

	read_unlock_bh(&gsGEDLogBufList.sLock);

	if (psFound) {
		write_lock_bh(&gsGEDLogBufList.sLock);
		*psFound->pCBHnd = hLogBuf;
		list_del(&psFound->sList);
		write_unlock_bh(&gsGEDLogBufList.sLock);
	}

	return !!psFound;
}

static ssize_t ged_log_buf_write_entry(const char __user *pszBuffer,
	size_t uiCount, loff_t uiPosition, void *pvData)
{
	return (ssize_t)__ged_log_buf_write((struct GED_LOG_BUF *)pvData,
		pszBuffer, (int)uiCount);
}
//-----------------------------------------------------------------------------
static void *ged_log_buf_seq_start(struct seq_file *psSeqFile,
	loff_t *puiPosition)
{
	struct GED_LOG_BUF *psGEDLogBuf =
		(struct GED_LOG_BUF *)psSeqFile->private;

	if (*puiPosition == 0)
		return psGEDLogBuf;

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_log_buf_seq_stop(struct seq_file *psSeqFile, void *pvData)
{

}
//-----------------------------------------------------------------------------
static void *ged_log_buf_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	(*puiPosition)++;

	return NULL;
}
//-----------------------------------------------------------------------------
static int ged_log_buf_seq_show_print(struct seq_file *psSeqFile,
	struct GED_LOG_BUF *psGEDLogBuf, int i)
{
	int err = 0;
	struct GED_LOG_BUF_LINE *line;

	line = &psGEDLogBuf->psLine[i];

	if (line->offset >= 0) {
		if (line->tattrs & GED_LOG_ATTR_TIME) {
			unsigned long long t;
			unsigned long nanosec_rem;

			t = line->time;
			nanosec_rem = do_div(t, 1000000000);

			seq_printf(psSeqFile, "[%5llu.%06lu] ", t,
				nanosec_rem / 1000);
		}

#if defined(CONFIG_RTC_LIB)
		if (line->tattrs & GED_LOG_ATTR_TIME_TPT) {
			unsigned long local_time;
			struct rtc_time tm;

			local_time = line->time;
			rtc_time_to_tm(local_time, &tm);

			seq_printf(psSeqFile,
				"%02d-%02d %02d:%02d:%02d.%06lu %5d %5d ",
				tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				line->time_usec, line->pid, line->tid);
		}
#endif

		seq_printf(psSeqFile, "%s\n",
			psGEDLogBuf->pcBuffer + line->offset);
	}

	return err;
}

static int ged_log_buf_seq_show(struct seq_file *psSeqFile, void *pvData)
{
	struct GED_LOG_BUF *psGEDLogBuf = (struct GED_LOG_BUF *)pvData;

	if (psGEDLogBuf != NULL) {
		int i;

#if defined(CONFIG_MACH_MT8167) || defined(CONFIG_MACH_MT8173)\
|| defined(CONFIG_MACH_MT6739) || defined(CONFIG_MACH_MT6761)\
|| defined(CONFIG_MACH_MT6765)
		if (strncmp(psGEDLogBuf->acName, "fw_trace", 8) == 0)
			ged_dump_fw();
#endif

		spin_lock_irqsave(&psGEDLogBuf->sSpinLock,
			psGEDLogBuf->ulIRQFlags);

		if (psGEDLogBuf->acName[0] != '\0') {
			seq_printf(psSeqFile,
				"---------- %s (%d/%d) ----------\n",
				psGEDLogBuf->acName,
				psGEDLogBuf->i32BufferCurrent,
				psGEDLogBuf->i32BufferSize);
		}

		if (psGEDLogBuf->attrs & GED_LOG_ATTR_RINGBUFFER) {
			for (i = psGEDLogBuf->i32LineCurrent;
				i < psGEDLogBuf->i32LineCount; ++i) {
				if (ged_log_buf_seq_show_print(psSeqFile,
					psGEDLogBuf, i) != 0)
					break;
			}

			for (i = 0; i < psGEDLogBuf->i32LineCurrent; ++i) {
				if (ged_log_buf_seq_show_print(
					psSeqFile, psGEDLogBuf, i) != 0)
					break;
			}
		} else if (psGEDLogBuf->attrs & GED_LOG_ATTR_QUEUEBUFFER) {
			for (i = 0; i < psGEDLogBuf->i32LineCount; ++i) {
				if (ged_log_buf_seq_show_print(psSeqFile,
					psGEDLogBuf, i) != 0)
					break;
			}
		}

		spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
			psGEDLogBuf->ulIRQFlags);
	}

	return 0;
}
//-----------------------------------------------------------------------------
static const struct seq_operations gsGEDLogBufReadOps = {
	.start = ged_log_buf_seq_start,
	.stop = ged_log_buf_seq_stop,
	.next = ged_log_buf_seq_next,
	.show = ged_log_buf_seq_show,
};
//-----------------------------------------------------------------------------
GED_LOG_BUF_HANDLE ged_log_buf_alloc(
		int i32MaxLineCount,
		int i32MaxBufferSizeByte,
		GED_LOG_BUF_TYPE eType,
		const char *pszName,
		const char *pszNodeName)
{
	struct GED_LOG_BUF *psGEDLogBuf;
	GED_ERROR error;

	if (((!pszName) && (!pszNodeName))
		|| (i32MaxLineCount <= 0) || (i32MaxBufferSizeByte <= 0)) {
		return (GED_LOG_BUF_HANDLE)0;
	}

	psGEDLogBuf = (struct GED_LOG_BUF *)
		ged_alloc(sizeof(struct GED_LOG_BUF));
	if (psGEDLogBuf == NULL) {
		GED_LOGE("ged: failed to allocate log buf!\n");
		return (GED_LOG_BUF_HANDLE)0;
	}

	psGEDLogBuf->eType = eType;

	switch (eType) {
	case GED_LOG_BUF_TYPE_RINGBUFFER:
		psGEDLogBuf->attrs = GED_LOG_ATTR_RINGBUFFER;
		break;
	case GED_LOG_BUF_TYPE_QUEUEBUFFER:
		psGEDLogBuf->attrs = GED_LOG_ATTR_QUEUEBUFFER;
		break;
	case GED_LOG_BUF_TYPE_QUEUEBUFFER_AUTO_INCREASE:
		psGEDLogBuf->attrs =
			GED_LOG_ATTR_QUEUEBUFFER | GED_LOG_ATTR_AUTO_INCREASE;
		break;
	}

	psGEDLogBuf->i32MemorySize = i32MaxBufferSizeByte
		+ sizeof(struct GED_LOG_BUF_LINE) * i32MaxLineCount;
	psGEDLogBuf->pMemory = ged_alloc(psGEDLogBuf->i32MemorySize);
	if (psGEDLogBuf->pMemory == NULL) {
		ged_free(psGEDLogBuf, sizeof(struct GED_LOG_BUF));
		GED_LOGE("ged: failed to allocate log buf!\n");
		return (GED_LOG_BUF_HANDLE)0;
	}

	psGEDLogBuf->psLine = (struct GED_LOG_BUF_LINE *)psGEDLogBuf->pMemory;
	psGEDLogBuf->pcBuffer = (char *)&psGEDLogBuf->psLine[i32MaxLineCount];
	psGEDLogBuf->i32LineCount = i32MaxLineCount;
	psGEDLogBuf->i32BufferSize = i32MaxBufferSizeByte;
	psGEDLogBuf->i32LineCurrent = 0;
	psGEDLogBuf->i32BufferCurrent = 0;

	psGEDLogBuf->psEntry = NULL;
	spin_lock_init(&psGEDLogBuf->sSpinLock);
	psGEDLogBuf->acName[0] = '\0';
	psGEDLogBuf->acNodeName[0] = '\0';

	/* Init Line */
	{
		int i = 0;
		for (i = 0; i < psGEDLogBuf->i32LineCount; ++i)
			psGEDLogBuf->psLine[i].offset = -1;
	}

	if (pszName)
		snprintf(psGEDLogBuf->acName,
			GED_LOG_BUF_NAME_LENGTH, "%s", pszName);


	// Add into the global list
	INIT_LIST_HEAD(&psGEDLogBuf->sList);
	write_lock_bh(&gsGEDLogBufList.sLock);
	list_add(&psGEDLogBuf->sList, &gsGEDLogBufList.sList_buf);
	write_unlock_bh(&gsGEDLogBufList.sLock);

	if (pszNodeName) {
		int err;
		snprintf(psGEDLogBuf->acNodeName,
			GED_LOG_BUF_NODE_NAME_LENGTH, "%s", pszNodeName);
		err = ged_debugFS_create_entry(
				psGEDLogBuf->acNodeName,
				gpsGEDLogBufsDir,
				&gsGEDLogBufReadOps,
				ged_log_buf_write_entry,
				psGEDLogBuf,
				&psGEDLogBuf->psEntry);

		if (unlikely(err)) {
			GED_LOGE("ged: failed to create %s entry, err(%d)!\n",
				pszNodeName, err);
			ged_log_buf_free(psGEDLogBuf->ulHashNodeID);
			return (GED_LOG_BUF_HANDLE)0;
		}
	}

	error = ged_hashtable_insert(ghHashTable, psGEDLogBuf,
		&psGEDLogBuf->ulHashNodeID);
	if (error != GED_OK) {
		GED_LOGE("ged: failed to insert into a hash table, err(%d)!\n",
			error);
		ged_log_buf_free(psGEDLogBuf->ulHashNodeID);
		return (GED_LOG_BUF_HANDLE)0;
	}

	GED_LOGI("%s OK\n", __func__);

	while (__ged_log_buf_check_get_early_list(
		psGEDLogBuf->ulHashNodeID, pszName)) {
		continue;
	};

	return (GED_LOG_BUF_HANDLE)psGEDLogBuf->ulHashNodeID;
}
EXPORT_SYMBOL(ged_log_buf_alloc);

GED_ERROR ged_log_buf_resize(
		GED_LOG_BUF_HANDLE hLogBuf,
		int i32NewMaxLineCount,
		int i32NewMaxBufferSizeByte)
{
	int i;
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);
	int i32NewMemorySize, i32OldMemorySize;
	void *pNewMemory, *pOldMemory;
	struct GED_LOG_BUF_LINE *pi32NewLine;
	char *pcNewBuffer;

	if ((psGEDLogBuf == NULL)
		|| (i32NewMaxLineCount <= 0)
		|| (i32NewMaxBufferSizeByte <= 0))
		return GED_ERROR_INVALID_PARAMS;


	i32NewMemorySize =
		i32NewMaxBufferSizeByte +
		sizeof(struct GED_LOG_BUF_LINE) * i32NewMaxLineCount;
	pNewMemory = ged_alloc(i32NewMemorySize);
	if (pNewMemory == NULL)
		return GED_ERROR_OOM;


	spin_lock_irqsave(&psGEDLogBuf->sSpinLock, psGEDLogBuf->ulIRQFlags);

	pi32NewLine = (struct GED_LOG_BUF_LINE *)pNewMemory;
	pcNewBuffer = (char *)&pi32NewLine[i32NewMaxLineCount];

	memcpy(pi32NewLine, psGEDLogBuf->psLine,
		sizeof(struct GED_LOG_BUF_LINE) * min(i32NewMaxLineCount,
		psGEDLogBuf->i32LineCount));
	memcpy(pcNewBuffer, psGEDLogBuf->pcBuffer,
		min(i32NewMaxBufferSizeByte, psGEDLogBuf->i32BufferSize));

	for (i = psGEDLogBuf->i32LineCount; i < i32NewMaxLineCount; ++i)
		pi32NewLine[i].offset = -1;

	i32OldMemorySize = psGEDLogBuf->i32MemorySize;
	pOldMemory = psGEDLogBuf->pMemory;

	psGEDLogBuf->i32MemorySize = i32NewMemorySize;
	psGEDLogBuf->pMemory = pNewMemory;
	psGEDLogBuf->psLine = pi32NewLine;
	psGEDLogBuf->pcBuffer = pcNewBuffer;
	psGEDLogBuf->i32LineCount = i32NewMaxLineCount;
	psGEDLogBuf->i32BufferSize = i32NewMaxBufferSizeByte;

	if (psGEDLogBuf->i32BufferCurrent >= i32NewMaxBufferSizeByte)
		psGEDLogBuf->i32BufferCurrent = i32NewMaxBufferSizeByte - 1;
	pcNewBuffer[psGEDLogBuf->i32BufferCurrent] = 0;

	spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
		psGEDLogBuf->ulIRQFlags);
	ged_free(pOldMemory, i32OldMemorySize);

	return GED_OK;
}

GED_ERROR ged_log_buf_ignore_lines(GED_LOG_BUF_HANDLE hLogBuf, int n)
{
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

	if (psGEDLogBuf && n > 0) {
		if (psGEDLogBuf->attrs & GED_LOG_ATTR_QUEUEBUFFER) {
			if (n >= psGEDLogBuf->i32LineCurrent) {
				/* reset all buffer */
				ged_log_buf_reset(hLogBuf);
			} else {
				int i;
				int buf_offset;
				int buf_size;

				spin_lock_irqsave(&psGEDLogBuf->sSpinLock,
					psGEDLogBuf->ulIRQFlags);

				buf_offset = psGEDLogBuf->psLine[n].offset;
				buf_size = psGEDLogBuf->i32BufferCurrent
					- buf_offset;

				/* Move lines, update offset and current */
				for (i = 0; n + i < psGEDLogBuf->i32LineCount;
					++i) {
					psGEDLogBuf->psLine[i] =
						psGEDLogBuf->psLine[n + i];
					psGEDLogBuf->psLine[i].offset -=
						buf_offset;
				}
				psGEDLogBuf->i32LineCurrent -= n;

				/* Move buffers and update current */
				for (i = 0; i < buf_size; ++i)
					psGEDLogBuf->pcBuffer[i] =
					psGEDLogBuf->pcBuffer[buf_offset + i];
				psGEDLogBuf->i32BufferCurrent = buf_size;

				spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
					psGEDLogBuf->ulIRQFlags);
			}
		}
	}

	return GED_OK;
}

GED_LOG_BUF_HANDLE ged_log_buf_get(const char *pszName)
{
	struct list_head *psListEntry, *psListEntryTemp, *psList;
	struct GED_LOG_BUF *psFound = NULL, *psLogBuf;

	if (!pszName)
		return (GED_LOG_BUF_HANDLE)0;

	read_lock_bh(&gsGEDLogBufList.sLock);

	psList = &gsGEDLogBufList.sList_buf;
	list_for_each_safe(psListEntry, psListEntryTemp, psList) {
		psLogBuf = list_entry(psListEntry, struct GED_LOG_BUF, sList);
		if (!strcmp(psLogBuf->acName, pszName)) {
			psFound = psLogBuf;
			break;
		}
	}

	read_unlock_bh(&gsGEDLogBufList.sLock);

	if (!psFound)
		return (GED_LOG_BUF_HANDLE)0;

	return (GED_LOG_BUF_HANDLE)psFound->ulHashNodeID;
}
EXPORT_SYMBOL(ged_log_buf_get);

int ged_log_buf_get_early(const char *pszName,
	GED_LOG_BUF_HANDLE *callback_set_handle)
{
	int err = 0;

	if (pszName == NULL)
		return GED_ERROR_INVALID_PARAMS;

	*callback_set_handle = ged_log_buf_get(pszName);

	if (*callback_set_handle == 0) {
		struct GED_LOG_LISTEN *psGEDLogListen;

		write_lock_bh(&gsGEDLogBufList.sLock);

		/* search again */
		{
			struct list_head *psListEntry;
			struct list_head *psListEntryTemp, *psList;
			struct GED_LOG_BUF *psFound = NULL, *psLogBuf;

			psList = &gsGEDLogBufList.sList_buf;
			list_for_each_safe(psListEntry,
				psListEntryTemp, psList) {
				psLogBuf = list_entry(psListEntry,
					struct GED_LOG_BUF, sList);
				if (!strcmp(psLogBuf->acName, pszName)) {
					psFound = psLogBuf;
					break;
				}
			}

			if (psFound) {
				*callback_set_handle =
					(GED_LOG_BUF_HANDLE)
					psFound->ulHashNodeID;
				goto exit_unlock;
			}
		}

		/* add to listen list */
		psGEDLogListen =
			(struct GED_LOG_LISTEN *)ged_alloc(
				sizeof(struct GED_LOG_LISTEN));
		if (psGEDLogListen == NULL) {
			err = GED_ERROR_OOM;
			goto exit_unlock;
		}
		psGEDLogListen->pCBHnd = callback_set_handle;
		snprintf(psGEDLogListen->acName,
			GED_LOG_BUF_NAME_LENGTH, "%s", pszName);
		INIT_LIST_HEAD(&psGEDLogListen->sList);
		list_add(&psGEDLogListen->sList, &gsGEDLogBufList.sList_listen);

exit_unlock:
		write_unlock_bh(&gsGEDLogBufList.sLock);
	}

	return err;
}
EXPORT_SYMBOL(ged_log_buf_get_early);

//-----------------------------------------------------------------------------
void ged_log_buf_free(GED_LOG_BUF_HANDLE hLogBuf)
{
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

	if (psGEDLogBuf) {
		ged_hashtable_remove(ghHashTable, psGEDLogBuf->ulHashNodeID);

		write_lock_bh(&gsGEDLogBufList.sLock);
		list_del(&psGEDLogBuf->sList);
		write_unlock_bh(&gsGEDLogBufList.sLock);

		if (psGEDLogBuf->psEntry)
			ged_debugFS_remove_entry(psGEDLogBuf->psEntry);

		ged_free(psGEDLogBuf->pMemory, psGEDLogBuf->i32MemorySize);
		ged_free(psGEDLogBuf, sizeof(struct GED_LOG_BUF));

		GED_LOGI("%s OK\n", __func__);
	}
}
EXPORT_SYMBOL(ged_log_buf_free);
//-----------------------------------------------------------------------------
GED_ERROR ged_log_buf_print(GED_LOG_BUF_HANDLE hLogBuf, const char *fmt, ...)
{
	va_list args;
	GED_ERROR err;
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

	if (psGEDLogBuf) {
		va_start(args, fmt);
		err = __ged_log_buf_vprint(psGEDLogBuf,
			fmt, args, psGEDLogBuf->attrs);
		va_end(args);
	}

	return GED_OK;
}
EXPORT_SYMBOL(ged_log_buf_print);

GED_ERROR ged_log_buf_print2(GED_LOG_BUF_HANDLE hLogBuf,
	int i32LogAttrs, const char *fmt, ...)
{
	va_list args;
	GED_ERROR err;
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

	if (psGEDLogBuf) {
		/* clear reserved attrs */
		i32LogAttrs &= ~0xff;

		va_start(args, fmt);
		err = __ged_log_buf_vprint(psGEDLogBuf, fmt,
			args, psGEDLogBuf->attrs | i32LogAttrs);
		va_end(args);
	}

	return GED_OK;
}
EXPORT_SYMBOL(ged_log_buf_print2);
//-----------------------------------------------------------------------------
GED_ERROR ged_log_buf_reset(GED_LOG_BUF_HANDLE hLogBuf)
{
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

	if (psGEDLogBuf) {
		int i;
		spin_lock_irqsave(&psGEDLogBuf->sSpinLock,
			psGEDLogBuf->ulIRQFlags);

		psGEDLogBuf->i32LineCurrent = 0;
		psGEDLogBuf->i32BufferCurrent = 0;
		for (i = 0; i < psGEDLogBuf->i32LineCount; ++i)
			psGEDLogBuf->psLine[i].offset = -1;

		spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
			psGEDLogBuf->ulIRQFlags);
	}

	return GED_OK;
}
EXPORT_SYMBOL(ged_log_buf_reset);

//-----------------------------------------------------------------------------
//
//  GED Log System
//
//-----------------------------------------------------------------------------
static ssize_t ged_log_write_entry(const char __user *pszBuffer, size_t uiCount,
	loff_t uiPosition, void *pvData)
{
#define GED_LOG_CMD_SIZE 64
	char acBuffer[GED_LOG_CMD_SIZE];

	if ((uiCount > 0) && (uiCount < GED_LOG_CMD_SIZE)) {
		if (ged_copy_from_user(acBuffer, pszBuffer, uiCount) == 0) {
			acBuffer[uiCount - 1] = '\0';
			if (strcmp(acBuffer, "reset") == 0) {
				struct list_head *psListEntry;
				struct list_head *psListEntryTemp, *psList;

				write_lock_bh(&gsGEDLogBufList.sLock);
				psList = &gsGEDLogBufList.sList_buf;
				list_for_each_safe(psListEntry,
					psListEntryTemp,
					psList) {
					struct GED_LOG_BUF *psGEDLogBuf =
						(struct GED_LOG_BUF *)
						list_entry(
							psListEntry,
							struct GED_LOG_BUF,
							sList);
					ged_log_buf_reset(
						psGEDLogBuf->ulHashNodeID);
				}
				write_unlock_bh(&gsGEDLogBufList.sLock);
			}
		}
	}

	return uiCount;
}
//-----------------------------------------------------------------------------
static void *ged_log_seq_start(struct seq_file *psSeqFile, loff_t *puiPosition)
{
	struct list_head *psListEntry, *psListEntryTemp, *psList;
	loff_t uiCurrentPosition = 0;

	read_lock_bh(&gsGEDLogBufList.sLock);

	psList = &gsGEDLogBufList.sList_buf;
	list_for_each_safe(psListEntry, psListEntryTemp, psList) {
		struct GED_LOG_BUF *psGEDLogBuf =
		(struct GED_LOG_BUF *)list_entry(psListEntry,
			struct GED_LOG_BUF, sList);
		if (psGEDLogBuf->acName[0] != '\0') {
			if (uiCurrentPosition == *puiPosition)
				return psGEDLogBuf;
			uiCurrentPosition++;
		}
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static void ged_log_seq_stop(struct seq_file *psSeqFile, void *pvData)
{
	read_unlock_bh(&gsGEDLogBufList.sLock);
}
//-----------------------------------------------------------------------------
static void *ged_log_seq_next(struct seq_file *psSeqFile,
	void *pvData, loff_t *puiPosition)
{
	struct list_head *psListEntry, *psListEntryTemp, *psList;
	loff_t uiCurrentPosition = 0;

	(*puiPosition)++;

	psList = &gsGEDLogBufList.sList_buf;
	list_for_each_safe(psListEntry, psListEntryTemp, psList) {
		struct GED_LOG_BUF *psGEDLogBuf =
			(struct GED_LOG_BUF *)list_entry(psListEntry,
				struct GED_LOG_BUF, sList);
		if (psGEDLogBuf->acName[0] != '\0') {
			if (uiCurrentPosition == *puiPosition)
				return psGEDLogBuf;
			uiCurrentPosition++;
		}
	}

	return NULL;
}
//-----------------------------------------------------------------------------
static const struct seq_operations gsGEDLogReadOps = {
	.start = ged_log_seq_start,
	.stop = ged_log_seq_stop,
	.next = ged_log_seq_next,
	.show = ged_log_buf_seq_show,
};
//-----------------------------------------------------------------------------
GED_ERROR ged_log_system_init(void)
{
	GED_ERROR err = GED_OK;

	err = ged_debugFS_create_entry(
			"gedlog",
			NULL,
			&gsGEDLogReadOps,
			ged_log_write_entry,
			NULL,
			&gpsGEDLogEntry);

	if (unlikely(err != GED_OK)) {
		GED_LOGE("ged: failed to create gedlog entry!\n");
		goto ERROR;
	}

	err = ged_debugFS_create_entry_dir(
			"logbufs",
			NULL,
			&gpsGEDLogBufsDir);

	if (unlikely(err != GED_OK)) {
		err = GED_ERROR_FAIL;
		GED_LOGE("ged: failed to create logbufs dir!\n");
		goto ERROR;
	}

	ghHashTable = ged_hashtable_create(5);
	if (!ghHashTable) {
		err = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create a hash table!\n");
		goto ERROR;
	}

	ged_log_trace_enable = 0;
	ged_log_perf_trace_enable = 0;

	return err;

ERROR:

	ged_log_system_exit();

	return err;
}
//-----------------------------------------------------------------------------
void ged_log_system_exit(void)
{
	ged_hashtable_destroy(ghHashTable);

	ged_debugFS_remove_entry(gpsGEDLogEntry);
}
//-----------------------------------------------------------------------------
int ged_log_buf_write(GED_LOG_BUF_HANDLE hLogBuf,
	const char __user *pszBuffer, int i32Count)
{
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

	return __ged_log_buf_write(psGEDLogBuf, pszBuffer, i32Count);
}

static int ged_log_buf_dump(struct GED_LOG_BUF *psGEDLogBuf, int i)
{
	int err = 0;
	struct GED_LOG_BUF_LINE *line;

	line = &psGEDLogBuf->psLine[i];

	if (line->offset >= 0) {
		if (line->tattrs & GED_LOG_ATTR_TIME) {
			unsigned long long t;
			unsigned long nanosec_rem;

			t = line->time;
			nanosec_rem = do_div(t, 1000000000);

			pr_debug("[%5llu.%06lu] ", t, nanosec_rem / 1000);
		}

#if defined(CONFIG_RTC_LIB)
		if (line->tattrs & GED_LOG_ATTR_TIME_TPT) {
			unsigned long local_time;
			struct rtc_time tm;

			local_time = line->time;
			rtc_time_to_tm(local_time, &tm);

			pr_debug("%02d-%02d %02d:%02d:%02d.%06lu %5d %5d ",
					tm.tm_mon + 1, tm.tm_mday,
					tm.tm_hour, tm.tm_min, tm.tm_sec,
					line->time_usec, line->pid, line->tid);
		}
#endif

		pr_debug("%s\n", psGEDLogBuf->pcBuffer + line->offset);
	}

	return err;
}

void ged_log_dump(GED_LOG_BUF_HANDLE hLogBuf)
{
	struct GED_LOG_BUF *psGEDLogBuf = ged_log_buf_from_handle(hLogBuf);

	if (psGEDLogBuf != NULL) {
		int i;

		spin_lock_irqsave(&psGEDLogBuf->sSpinLock,
			psGEDLogBuf->ulIRQFlags);

		if (psGEDLogBuf->acName[0] != '\0')
			pr_debug("---------- %s (%d/%d) ----------\n",
				psGEDLogBuf->acName,
				psGEDLogBuf->i32BufferCurrent,
				psGEDLogBuf->i32BufferSize);

		if (psGEDLogBuf->attrs & GED_LOG_ATTR_RINGBUFFER) {
			for (i = psGEDLogBuf->i32LineCurrent;
				i < psGEDLogBuf->i32LineCount; ++i)
				if (ged_log_buf_dump(psGEDLogBuf, i) != 0)
					break;

			for (i = 0; i < psGEDLogBuf->i32LineCurrent; ++i)
				if (ged_log_buf_dump(psGEDLogBuf, i) != 0)
					break;
		} else if (psGEDLogBuf->attrs & GED_LOG_ATTR_QUEUEBUFFER)
			for (i = 0; i < psGEDLogBuf->i32LineCount; ++i)
				if (ged_log_buf_dump(psGEDLogBuf, i) != 0)
					break;

		spin_unlock_irqrestore(&psGEDLogBuf->sSpinLock,
			psGEDLogBuf->ulIRQFlags);
	}
}

static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}
void ged_log_trace_begin(char *name)
{
	if (ged_log_trace_enable) {
		__mt_update_tracing_mark_write_addr();
#ifdef ENABLE_GED_SYSTRACE_UTIL
		preempt_disable();
		event_trace_printk(tracing_mark_write_addr,
			"B|%d|%s\n", current->tgid, name);
		preempt_enable();
#endif
	}
}
EXPORT_SYMBOL(ged_log_trace_begin);
void ged_log_trace_end(void)
{
	if (ged_log_trace_enable) {
		__mt_update_tracing_mark_write_addr();
#ifdef ENABLE_GED_SYSTRACE_UTIL
		preempt_disable();
		event_trace_printk(tracing_mark_write_addr, "E\n");
		preempt_enable();
#endif
	}
}
EXPORT_SYMBOL(ged_log_trace_end);
void ged_log_trace_counter(char *name, int count)
{
	if (ged_log_trace_enable) {
		__mt_update_tracing_mark_write_addr();
#ifdef ENABLE_GED_SYSTRACE_UTIL
		preempt_disable();
		event_trace_printk(tracing_mark_write_addr,
			"C|5566|%s|%d\n", name, count);
		preempt_enable();
#endif
	}
}
EXPORT_SYMBOL(ged_log_trace_counter);
void ged_log_perf_trace_counter(char *name, long long count, int pid,
	unsigned long frameID, u64 BQID)
{
	if (ged_log_perf_trace_enable) {
		__mt_update_tracing_mark_write_addr();
#ifdef ENABLE_GED_SYSTRACE_UTIL
		preempt_disable();
		event_trace_printk(tracing_mark_write_addr,
			"C|%d|%s|%lld|%llu|%lu\n", pid,
			name, count, (unsigned long long)BQID, frameID);
		preempt_enable();
#endif
	}
}
EXPORT_SYMBOL(ged_log_perf_trace_counter);

module_param(ged_log_trace_enable, uint, 0644);
module_param(ged_log_perf_trace_enable, uint, 0644);
