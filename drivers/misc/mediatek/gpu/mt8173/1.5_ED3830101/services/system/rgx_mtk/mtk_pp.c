#if defined(MTK_DEBUG_PROC_PRINT)

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include "mtk_pp.h"

//#define ENABLE_AEE_WHEN_LOCKUP

#if defined(ENABLE_AEE_WHEN_LOCKUP)
#include <linux/workqueue.h>
#include <linux/aee.h>
#endif

static struct proc_dir_entry *g_MTKPP_proc;
static MTK_PROC_PRINT_DATA *g_MTKPPdata[MTKPP_ID_SIZE];

static int g_init_done = 0;

#if defined(ENABLE_AEE_WHEN_LOCKUP)

typedef struct MTKPP_WORKQUEUE_t
{
	int cycle;
	struct workqueue_struct     *psWorkQueue;
} MTKPP_WORKQUEUE;
MTKPP_WORKQUEUE g_MTKPP_workqueue;

typedef struct MTKPP_WORKQUEUE_WORKER_t
{
	struct work_struct          sWork;
} MTKPP_WORKQUEUE_WORKER;
MTKPP_WORKQUEUE_WORKER g_MTKPP_worker;

#endif

static void MTKPP_InitLock(MTK_PROC_PRINT_DATA *data)
{
	spin_lock_init(&data->lock);
}

static void MTKPP_Lock(MTK_PROC_PRINT_DATA *data)
{
	spin_lock_irqsave(&data->lock, data->irqflags);
}
static void MTKPP_UnLock(MTK_PROC_PRINT_DATA *data)
{
	spin_unlock_irqrestore(&data->lock, data->irqflags);
}

static void MTKPP_PrintQueueBuffer(MTK_PROC_PRINT_DATA *data, const char *fmt, ...) MTK_PP_FORMAT_PRINTF(2,3);

static void MTKPP_PrintQueueBuffer2(MTK_PROC_PRINT_DATA *data, const char *fmt, ...) MTK_PP_FORMAT_PRINTF(2,3);

static void MTKPP_PrintRingBuffer(MTK_PROC_PRINT_DATA *data, const char *fmt, ...) MTK_PP_FORMAT_PRINTF(2,3);

static int MTKPP_PrintTime(char *buf, int n)
{
	/* copy & modify from ./kernel/printk.c */
	unsigned long long t;
	unsigned long nanosec_rem;

	t = cpu_clock(smp_processor_id());
	nanosec_rem = do_div(t, 1000000000);

	return snprintf(buf, n, "[%5lu.%06lu] ", (unsigned long) t, nanosec_rem / 1000);
}

static void MTKPP_PrintQueueBuffer(MTK_PROC_PRINT_DATA *data, const char *fmt, ...)
{
	va_list args;
	char *buf;
	int len;

	MTKPP_Lock(data);

	if ((data->current_line >= data->line_array_size)
		|| (data->current_data >= (data->data_array_size - 128)))
	{
		MTKPP_UnLock(data);
		return;
	}

	/* move to next line */
	buf = data->line[data->current_line++] = data->data + data->current_data;

	/* print string */
	va_start(args, fmt);
	len = vsnprintf(buf, (data->data_array_size - data->current_data), fmt, args);
	va_end(args);

	data->current_data += len + 1;

	MTKPP_UnLock(data);
}

static void MTKPP_PrintQueueBuffer2(MTK_PROC_PRINT_DATA *data, const char *fmt, ...)
{
	va_list args;
	char *buf;
	int len;

	MTKPP_Lock(data);

	if ((data->current_line >= data->line_array_size)
		|| (data->current_data >= (data->data_array_size - 128)))
	{
		/* buffer full, ignore the coming input */
		MTKPP_UnLock(data);
		return;
	}

	/* move to next line */
	buf = data->line[data->current_line++] = data->data + data->current_data;

	/* add the current time stamp */
	len = MTKPP_PrintTime(buf, (data->data_array_size - data->current_data));
	buf += len;
	data->current_data += len;

	/* print string */
	va_start(args, fmt);
	len = vsnprintf(buf, (data->data_array_size - data->current_data), fmt, args);
	va_end(args);

	data->current_data += len + 1 ;

	MTKPP_UnLock(data);
}

static void MTKPP_PrintRingBuffer(MTK_PROC_PRINT_DATA *data, const char *fmt, ...)
{
	va_list args;
	char *buf;
	int len, s;

	MTKPP_Lock(data);

	if ((data->current_line >= data->line_array_size)
		|| (data->current_data >= (data->data_array_size - 128)))
	{
		/* buffer full, move the pointer to the head */
		data->current_line = 0;
		data->current_data = 0;
	}

	/* move to next line */
	buf = data->line[data->current_line++] = data->data + data->current_data;

	/* add the current time stamp */
	len = MTKPP_PrintTime(buf, (data->data_array_size - data->current_data));
	buf += len;
	data->current_data += len;

	/* print string */
	va_start(args, fmt);
	len = vsnprintf(buf, (data->data_array_size - data->current_data), fmt, args);
	va_end(args);

	data->current_data += len + 1 ;

	/* clear data which are overlaid by the new log */
	buf += len; s = data->current_line;
	while (s < data->line_array_size
		&& data->line[s] != NULL
		&& data->line[s] <= buf)
	{
		data->line[s++] = NULL;
	}

	MTKPP_UnLock(data);
}

static MTK_PROC_PRINT_DATA *MTKPP_AllocStruct(int type)
{
	MTK_PROC_PRINT_DATA *data;

	data = vmalloc(sizeof(MTK_PROC_PRINT_DATA));
	if (data == NULL)
	{
		_MTKPP_DEBUG_LOG("%s: vmalloc fail", __func__);
		goto err_out;
	}

	MTKPP_InitLock(data);

	switch (type)
	{
		case MTKPP_BUFFERTYPE_QUEUEBUFFER:
			data->pfn_print = MTKPP_PrintQueueBuffer;
			break;
		case MTKPP_BUFFERTYPE_RINGBUFFER:
			data->pfn_print = MTKPP_PrintRingBuffer;
			break;
		default:
			_MTKPP_DEBUG_LOG("%s: unknow flags: %d", __func__, type);
			goto err_out2;
			break;
	}

	data->data = NULL;
	data->line = NULL;
	data->data_array_size = 0;
	data->line_array_size = 0;
	data->current_data = 0;
	data->current_line = 0;
	data->type = type;

	return data;

err_out2:
	vfree(data);
err_out:
	return NULL;

}

static void MTKPP_FreeStruct(MTK_PROC_PRINT_DATA **data)
{
	vfree(*data);
	*data = NULL;
}

static void MTKPP_AllocData(MTK_PROC_PRINT_DATA *data, int data_size, int line_size)
{
 	void * buf;

	buf = vmalloc( sizeof(char) * data_size + sizeof(char*) * line_size );
	if (buf == NULL)
	{
		return;
	}

	MTKPP_Lock(data);

	data->data_array_size = data_size;
	data->line_array_size = line_size;

	data->data = (char *)buf;
	data->line = buf + sizeof(char) * data_size;

	MTKPP_UnLock(data);
}

static void MTKPP_FreeData(MTK_PROC_PRINT_DATA *data)
{
	void * buf;

	MTKPP_Lock(data);

	buf = data->data;

	vfree(buf);

	data->line = NULL;
	data->data = NULL;
	data->data_array_size = 0;
	data->line_array_size = 0;
	data->current_data = 0;
	data->current_line = 0;

	MTKPP_UnLock(data);
}

static void MTKPP_CleanData(MTK_PROC_PRINT_DATA *data)
{
	MTKPP_Lock(data);

	if (data->line)
	{
		memset(data->line, 0, sizeof(char*) * data->line_array_size);
	}
	data->current_data = 0;
	data->current_line = 0;

	MTKPP_UnLock(data);
}

static void* MTKPP_SeqStart(struct seq_file *s, loff_t *pos)
{
	loff_t *spos;

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);

	if (*pos >= MTKPP_ID_SIZE)
	{
		kfree(spos);
		return NULL;
	}

	if (spos == NULL)
	{
		return NULL;
	}

	*spos = *pos;
	return spos;
}

static void* MTKPP_SeqNext(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *spos = (loff_t *) v;
	*pos = ++(*spos);

	return (*pos < MTKPP_ID_SIZE) ? spos : NULL;
}

static void MTKPP_SeqStop(struct seq_file *s, void *v)
{
	kfree(v);
}

static int MTKPP_SeqShow(struct seq_file *sfile, void *v)
{
	MTK_PROC_PRINT_DATA *data;
	int off, i;
	loff_t *spos = (loff_t *) v;

	off = *spos;
	data = g_MTKPPdata[off];

	seq_printf(sfile, "\n" "===== buffer_id = %d =====\n", off);

	MTKPP_Lock(data);

	switch (data->type)
	{
		case MTKPP_BUFFERTYPE_QUEUEBUFFER:
			seq_printf(sfile, "data_size = %d/%d\n", data->current_data, data->data_array_size);
			seq_printf(sfile, "data_line = %d/%d\n", data->current_line, data->line_array_size);
			for (i = 0; i < data->current_line; ++i)
			{
				seq_printf(sfile, "%s\n", data->line[i]);
			}
			break;
		case MTKPP_BUFFERTYPE_RINGBUFFER:
			seq_printf(sfile, "data_size = %d\n", data->data_array_size);
			seq_printf(sfile, "data_line = %d\n", data->line_array_size);
			for (i = data->current_line; i < data->line_array_size; ++i)
			{
				if (data->line[i] != NULL)
				{
					seq_printf(sfile, "%s\n", data->line[i]);
				}
			}
			for (i = 0; i < data->current_line; ++i)
			{
				if (data->line[i] != NULL)
				{
					seq_printf(sfile, "%s\n", data->line[i]);
				}
			}
			break;
		default:
			break;
	}

	MTKPP_UnLock(data);

	return 0;
}

static struct seq_operations g_MTKPP_seq_ops = {
    .start = MTKPP_SeqStart,
    .next  = MTKPP_SeqNext,
    .stop  = MTKPP_SeqStop,
    .show  = MTKPP_SeqShow
};

static int MTKPP_ProcOpen(struct inode *inode, struct file *file)
{
    return seq_open(file, &g_MTKPP_seq_ops);
}

static struct file_operations g_MTKPP_proc_ops = {
    .open    = MTKPP_ProcOpen,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = seq_release
};

#if defined(ENABLE_AEE_WHEN_LOCKUP)

static IMG_VOID MTKPP_WORKR_Handle(struct work_struct *_psWork)
{
	struct MTKPP_WORKQUEUE_WORKER_t* psWork =
		container_of(_psWork, MTKPP_WORKQUEUE_WORKER, sWork);

	/* avoid the build warnning */
	psWork = psWork;

	aee_kernel_exception("gpulog", "aee dump gpulog");
}

#endif

void MTKPP_Init(void)
{
	int i;
	struct {
		MTKPP_ID uid;
		MTKPP_BUFFERTYPE type;
		int data_size;
		int line_size;
	} MTKPP_TABLE[] =
	{
		{MTKPP_ID_FW,       MTKPP_BUFFERTYPE_QUEUEBUFFER,  248 * 1024,  1 * 1024}, /* 256 KB */
		{MTKPP_ID_SYNC,     MTKPP_BUFFERTYPE_RINGBUFFER,    56 * 1024,  1 * 1024}, /*  64 KB */
	};

	for (i = 0; i < MTKPP_ID_SIZE; ++i)
	{
		if (i != MTKPP_TABLE[i].uid)
		{
			_MTKPP_DEBUG_LOG("%s: index(%d) != tabel_uid(%d)", __func__, i, MTKPP_TABLE[i].uid);
			goto err_out;
		}

		g_MTKPPdata[i] = MTKPP_AllocStruct(MTKPP_TABLE[i].type);

		if (g_MTKPPdata[i] == NULL)
		{
			_MTKPP_DEBUG_LOG("%s: alloc struct fail: flags = %d", __func__, MTKPP_TABLE[i].type);
			goto err_out;
		}

		if (MTKPP_TABLE[i].data_size > 0)
		{
			MTKPP_AllocData(g_MTKPPdata[i], MTKPP_TABLE[i].data_size, MTKPP_TABLE[i].line_size);
			MTKPP_CleanData(g_MTKPPdata[i]);
		}
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0))
	g_MTKPP_proc = proc_create("gpulog", 0, NULL, &g_MTKPP_proc_ops);
#else
	g_MTKPP_proc = create_proc_entry("gpulog", 0, NULL);
	g_MTKPP_proc->proc_fops = &g_MTKPP_proc_ops;
#endif

#if defined(ENABLE_AEE_WHEN_LOCKUP)
	g_MTKPP_workqueue.psWorkQueue = alloc_ordered_workqueue("mwp", WQ_FREEZABLE | WQ_MEM_RECLAIM);
	INIT_WORK(&g_MTKPP_worker.sWork, MTKPP_WORKR_Handle);
#endif

	g_init_done = 1;

err_out:
	return;
}

void MTKPP_Deinit(void)
{
	int i;

	remove_proc_entry("gpulog", NULL);

	for (i = (MTKPP_ID_SIZE - 1); i >= 0; --i)
	{
		MTKPP_FreeData(g_MTKPPdata[i]);
		MTKPP_FreeStruct(&g_MTKPPdata[i]);
	}

	g_init_done = 0;
}

MTK_PROC_PRINT_DATA *MTKPP_GetData(MTKPP_ID id)
{
	return g_init_done ? g_MTKPPdata[id] : NULL;
}

void MTKPP_LOGTIME(MTKPP_ID id, const char * str)
{
	if (g_init_done)
	{
		_MTKPP_DEBUG_LOG("%s", str);
		MTKPP_PrintQueueBuffer2(g_MTKPPdata[id], "%s", str);
	}
	else
	{
		_MTKPP_DEBUG_LOG("[not init] %s", str);
	}
}

void MTKPP_TriggerAEE(void)
{
#if defined(ENABLE_AEE_WHEN_LOCKUP)
	if (g_init_done)
	{
		queue_work(g_MTKPP_workqueue.psWorkQueue, &g_MTKPP_worker.sWork);
	}
#endif
}

#endif
