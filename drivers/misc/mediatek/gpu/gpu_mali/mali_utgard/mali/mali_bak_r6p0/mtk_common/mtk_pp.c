/*
* Copyright (C) 2011-2014 MediaTek Inc.
* 
* This program is free software: you can redistribute it and/or modify it under the terms of the 
* GNU General Public License version 2 as published by the Free Software Foundation.
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "mtk_pp.h"

#if defined(MTK_DEBUG) && defined(MTK_DEBUG_PROC_PRINT)

static struct proc_dir_entry *g_MTKPP_proc;
static MTK_PROC_PRINT_DATA *g_MTKPPdata[MTKPP_ID_SIZE];

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

//static void MTKPP_PrintQueueBuffer2(MTK_PROC_PRINT_DATA *data, const char *fmt, ...) MTK_PP_FORMAT_PRINTF(2,3);

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
		// out of buffer, ignore input
		MTKPP_UnLock(data);
		return;
	}

	/* Move to next line */
	buf = data->line[data->current_line++] = data->data + data->current_data;
	
	/* Print string */
	va_start(args, fmt);
	len = vsnprintf(buf, (data->data_array_size - data->current_data), fmt, args);
	va_end(args);
	
	data->current_data += len + 1;
	
	MTKPP_UnLock(data);
}

#if 0
static void MTKPP_PrintQueueBuffer2(MTK_PROC_PRINT_DATA *data, const char *fmt, ...)
{
	va_list args;
	char *buf;
	int len;

	MTKPP_Lock(data);
	
	if ((data->current_line >= data->line_array_size)
		|| (data->current_data >= (data->data_array_size - 128)))
	{
		// out of buffer, ignore input
		MTKPP_UnLock(data);
		return;
	}

	/* Move to next line */
	buf = data->line[data->current_line++] = data->data + data->current_data;

	/* Add the current time stamp */
	len = MTKPP_PrintTime(buf, (data->data_array_size - data->current_data));
	buf += len;
	data->current_data += len;

	/* Print string */
	va_start(args, fmt);
	len = vsnprintf(buf, (data->data_array_size - data->current_data), fmt, args);
	va_end(args);
	
	data->current_data += len + 1 ;
	
	MTKPP_UnLock(data);
}
#endif

static void MTKPP_PrintRingBuffer(MTK_PROC_PRINT_DATA *data, const char *fmt, ...)
{
	va_list args;
	char *buf;
	int len, s;

	MTKPP_Lock(data);
	
	if ((data->current_line >= data->line_array_size)
		|| (data->current_data >= (data->data_array_size - 128)))
	{
		data->current_line = 0;
		data->current_data = 0;
	}

	/* Move to next line */
	buf = data->line[data->current_line++] = data->data + data->current_data;

	/* Add the current time stamp */
	len = MTKPP_PrintTime(buf, (data->data_array_size - data->current_data));
	buf += len;
	data->current_data += len;

	/* Print string */
	va_start(args, fmt);
	len = vsnprintf(buf, (data->data_array_size - data->current_data), fmt, args);
	va_end(args);
	
	data->current_data += len + 1 ;

	/* Clear overflow data */
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
			// something wrong
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

static void MTKPP_AllocData(MTK_PROC_PRINT_DATA *data, int data_size, int max_line)
{
	MTKPP_Lock(data);
		
	data->data = (char *)kmalloc(sizeof(char)*data_size, GFP_ATOMIC);
	if (data->data == NULL)
	{
		_MTKPP_DEBUG_LOG("%s, kmalloc data fail, size = %d", __func__, data_size);
		goto err_alloc_struct;
	}
	data->line = (char **)kmalloc(sizeof(char*)*max_line, GFP_ATOMIC);
	if (data->line == NULL)
	{
		_MTKPP_DEBUG_LOG("%s, kmalloc line fail, size = %d", __func__, data_size);
		goto err_alloc_data;
	}

	data->data_array_size = data_size;
	data->line_array_size = max_line;
	
	MTKPP_UnLock(data);

	return;
	
err_alloc_data:
	kfree(data->data);
err_alloc_struct:	
	MTKPP_UnLock(data);
	return;
	
}

static void MTKPP_FreeData(MTK_PROC_PRINT_DATA *data)
{
	MTKPP_Lock(data);
	
	kfree(data->line);
	kfree(data->data);
	
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

	memset(data->line, 0, sizeof(char*)*data->line_array_size);
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
		// MTK: lono@2013/1/7
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
			// FIXME: assert here
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
    .read    = seq_read,    // system
    .llseek  = seq_lseek,   // system
    .release = seq_release  // system
};

void MTKPP_Init(void)
{
	int i;
	struct {
		MTKPP_ID uid;
		MTKPP_BUFFERTYPE type;
		int data_size;
		int max_line;
	} mtk_pp_register_tabls[] =
	{	
		{MTKPP_ID_SYNC,			MTKPP_BUFFERTYPE_RINGBUFFER,	1024 * 1024 * 2,	1024},
	};

	for (i = 0; i < MTKPP_ID_SIZE; ++i)
	{
		if (i != mtk_pp_register_tabls[i].uid)
		{
			_MTKPP_DEBUG_LOG("%s: index(%d) != tabel_uid(%d)", __func__, i, mtk_pp_register_tabls[i].uid);
			goto err_out;
		}
		
		g_MTKPPdata[i] = MTKPP_AllocStruct(mtk_pp_register_tabls[i].type);

		if (g_MTKPPdata[i] == NULL)
		{
			_MTKPP_DEBUG_LOG("%s: alloc struct fail: flags = %d", __func__, mtk_pp_register_tabls[i].type);
			goto err_out;
		}

		if (mtk_pp_register_tabls[i].data_size > 0)
		{
			MTKPP_AllocData(
				g_MTKPPdata[i],
				mtk_pp_register_tabls[i].data_size,
				mtk_pp_register_tabls[i].max_line
				);
			
			MTKPP_CleanData(g_MTKPPdata[i]);
		}
	}
	
	g_MTKPP_proc = proc_create("gpulog", 0, NULL, &g_MTKPP_proc_ops);

	return;
	
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
}

MTK_PROC_PRINT_DATA *MTKPP_GetData(MTKPP_ID id)
{
	return (id >= 0 && id < MTKPP_ID_SIZE) ? 
		g_MTKPPdata[id] : NULL;
}

#endif
