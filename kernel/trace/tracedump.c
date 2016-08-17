/*
 * kernel/trace/tracedump.c
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/console.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/ring_buffer.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/threads.h>
#include <linux/tracedump.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>

#include "trace.h"
#include "trace_output.h"

#define CPU_MAX (NR_CPUS-1)

#define TRYM(fn, ...) do {		\
	int try_error = (fn);		\
	if (try_error < 0) {		\
		printk(__VA_ARGS__);	\
		return try_error;	\
	}				\
} while (0)

#define TRY(fn) TRYM(fn, TAG "Caught error from %s in %s\n", #fn, __func__)

/* Stolen from printk.c */
#define for_each_console(con) \
	for (con = console_drivers; con != NULL; con = con->next)

#define TAG KERN_ERR "tracedump: "

#define TD_MIN_CONSUME 2000
#define TD_COMPRESS_CHUNK 0x8000

static DEFINE_MUTEX(tracedump_proc_lock);

static const char MAGIC_NUMBER[9] = "TRACEDUMP";
static const char CPU_DELIM[7] = "CPU_END";
#define CMDLINE_DELIM "|"

/* Type of output */
static bool current_format;
static bool format_ascii;
module_param(format_ascii, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(format_ascii, "Dump ascii or raw data");

/* Max size of output */
static uint panic_size = 0x80000;
module_param(panic_size, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(panic_size, "Max dump size during kernel panic (bytes)");

static uint compress_level = 9;
module_param(compress_level, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(compress_level, "Level of compression to use. [0-9]");

static char out_buf[TD_COMPRESS_CHUNK];
static z_stream stream;
static int compress_done;
static int flush;

static int old_trace_flags;

static struct trace_iterator iter;
static struct pager_s {
	struct trace_array	*tr;
	void			*spare;
	int			cpu;
	int			len;
	char __user		*ubuf;
} pager;

static char cmdline_buf[16+TASK_COMM_LEN];

static int print_to_console(const char *buf, size_t len)
{
	struct console *con;

	/* Stolen from printk.c */
	for_each_console(con) {
		if ((con->flags & CON_ENABLED) && con->write &&
		   (cpu_online(smp_processor_id()) ||
		   (con->flags & CON_ANYTIME)))
			con->write(con, buf, len);
	}
	return 0;
}

static int print_to_user(const char *buf, size_t len)
{
	int size;
	size = copy_to_user(pager.ubuf, buf, len);
	if (size > 0) {
		printk(TAG "Failed to copy to user %d bytes\n", size);
		return -EINVAL;
	}
	return 0;
}

static int print(const char *buf, size_t len, int print_to)
{
	if (print_to == TD_PRINT_CONSOLE)
		TRY(print_to_console(buf, len));
	else if (print_to == TD_PRINT_USER)
		TRY(print_to_user(buf, len));
	return 0;
}

/* print_magic will print MAGIC_NUMBER using the
 * print function selected by print_to.
 */
static inline ssize_t print_magic(int print_to)
{
	print(MAGIC_NUMBER, sizeof(MAGIC_NUMBER), print_to);
	return sizeof(MAGIC_NUMBER);
}

static int iter_init(void)
{
	int cpu;

	/* Make iter point to global ring buffer used in trace. */
	trace_init_global_iter(&iter);

	/* Disable tracing */
	for_each_tracing_cpu(cpu) {
		atomic_inc(&iter.tr->data[cpu]->disabled);
	}

	/* Save flags */
	old_trace_flags = trace_flags;

	/* Dont look at memory in panic mode. */
	trace_flags &= ~TRACE_ITER_SYM_USEROBJ;

	/* Prepare ring buffer iter */
	for_each_tracing_cpu(cpu) {
		iter.buffer_iter[cpu] =
		 ring_buffer_read_prepare(iter.tr->buffer, cpu);
	}
	ring_buffer_read_prepare_sync();
	for_each_tracing_cpu(cpu) {
		ring_buffer_read_start(iter.buffer_iter[cpu]);
		tracing_iter_reset(&iter, cpu);
	}
	return 0;
}

/* iter_next gets the next entry in the ring buffer, ordered by time.
 * If there are no more entries, returns 0.
 */
static ssize_t iter_next(void)
{
	/* Zero out the iterator's seq */
	memset(&iter.seq, 0,
		sizeof(struct trace_iterator) -
		offsetof(struct trace_iterator, seq));

	while (!trace_empty(&iter)) {
		if (trace_find_next_entry_inc(&iter) == NULL) {
			printk(TAG "trace_find_next_entry failed!\n");
			return -EINVAL;
		}

		/* Copy the ring buffer data to iterator's seq */
		print_trace_line(&iter);
		if (iter.seq.len != 0)
			return iter.seq.len;
	}
	return 0;
}

static int iter_deinit(void)
{
	int cpu;
	/* Enable tracing */
	for_each_tracing_cpu(cpu) {
		ring_buffer_read_finish(iter.buffer_iter[cpu]);
	}
	for_each_tracing_cpu(cpu) {
		atomic_dec(&iter.tr->data[cpu]->disabled);
	}

	/* Restore flags */
	trace_flags = old_trace_flags;
	return 0;
}

static int pager_init(void)
{
	int cpu;

	/* Need to do this to get a pointer to global_trace (iter.tr).
	   Lame, I know. */
	trace_init_global_iter(&iter);

	/* Turn off tracing */
	for_each_tracing_cpu(cpu) {
		atomic_inc(&iter.tr->data[cpu]->disabled);
	}

	memset(&pager, 0, sizeof(pager));
	pager.tr = iter.tr;
	pager.len = TD_COMPRESS_CHUNK;

	return 0;
}

/* pager_next_cpu moves the pager to the next cpu.
 * Returns 0 if pager is done, else 1.
 */
static ssize_t pager_next_cpu(void)
{
	if (pager.cpu <= CPU_MAX) {
		pager.cpu += 1;
		return 1;
	}

	return 0;
}

/* pager_next gets the next page of data from the ring buffer
 * of the current cpu. Returns page size or 0 if no more data.
 */
static ssize_t pager_next(void)
{
	int ret;

	if (pager.cpu > CPU_MAX)
		return 0;

	if (!pager.spare)
		pager.spare = ring_buffer_alloc_read_page(pager.tr->buffer, pager.cpu);
	if (!pager.spare) {
		printk(TAG "ring_buffer_alloc_read_page failed!");
		return -ENOMEM;
	}

	ret = ring_buffer_read_page(pager.tr->buffer,
				    &pager.spare,
				    pager.len,
				    pager.cpu, 0);
	if (ret < 0)
		return 0;

	return PAGE_SIZE;
}

static int pager_deinit(void)
{
	int cpu;
	if (pager.spare != NULL)
		ring_buffer_free_read_page(pager.tr->buffer, pager.spare);

	for_each_tracing_cpu(cpu) {
		atomic_dec(&iter.tr->data[cpu]->disabled);
	}
	return 0;
}

/* cmdline_next gets the next saved cmdline from the trace and
 * puts it in cmdline_buf. Returns the size of the cmdline, or 0 if empty.
 * but will reset itself on a subsequent call.
 */
static ssize_t cmdline_next(void)
{
	static int pid;
	ssize_t size = 0;

	if (pid >= PID_MAX_DEFAULT)
		pid = -1;

	while (size == 0 && pid < PID_MAX_DEFAULT) {
		pid++;
		trace_find_cmdline(pid, cmdline_buf);
		if (!strncmp(cmdline_buf, "<...>", 5))
			continue;

		sprintf(&cmdline_buf[strlen(cmdline_buf)], " %d"
				     CMDLINE_DELIM, pid);
		size = strlen(cmdline_buf);
	}
	return size;
}

/* comsume_events removes the first 'num' entries from the ring buffer. */
static int consume_events(size_t num)
{
	TRY(iter_init());
	for (; num > 0 && !trace_empty(&iter); num--) {
		trace_find_next_entry_inc(&iter);
		ring_buffer_consume(iter.tr->buffer, iter.cpu, &iter.ts,
				    &iter.lost_events);
	}
	TRY(iter_deinit());
	return 0;
}

static int data_init(void)
{
	if (current_format)
		TRY(iter_init());
	else
		TRY(pager_init());
	return 0;
}

/* data_next will figure out the right 'next' function to
 * call and will select the right buffer to pass back
 * to compress_next.
 *
 * iter_next should be used to get data entry-by-entry, ordered
 * by time, which is what we need in order to convert it to ascii.
 *
 * pager_next will return a full page of raw data at a time, one
 * CPU at a time. pager_next_cpu must be called to get the next CPU.
 * cmdline_next will get the next saved cmdline
 */
static ssize_t data_next(const char **buf)
{
	ssize_t size;

	if (current_format) {
		TRY(size = iter_next());
		*buf = iter.seq.buffer;
	} else {
		TRY(size = pager_next());
		*buf = pager.spare;
		if (size == 0) {
			if (pager_next_cpu()) {
				size = sizeof(CPU_DELIM);
				*buf = CPU_DELIM;
			} else {
				TRY(size = cmdline_next());
				*buf = cmdline_buf;
			}
		}
	}
	return size;
}

static int data_deinit(void)
{
	if (current_format)
		TRY(iter_deinit());
	else
		TRY(pager_deinit());
	return 0;
}

static int compress_init(void)
{
	int workspacesize, ret;

	compress_done = 0;
	flush = Z_NO_FLUSH;
	stream.data_type = current_format ? Z_ASCII : Z_BINARY;
	workspacesize = zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL);
	stream.workspace = vmalloc(workspacesize);
	if (!stream.workspace) {
		printk(TAG "Could not allocate "
			   "enough memory for zlib!\n");
		return -ENOMEM;
	}
	memset(stream.workspace, 0, workspacesize);

	ret = zlib_deflateInit(&stream, compress_level);
	if (ret != Z_OK) {
		printk(TAG "%s\n", stream.msg);
		return ret;
	}
	stream.avail_in = 0;
	stream.avail_out = 0;
	TRY(data_init());
	return 0;
}

/* compress_next will compress up to min(max_out, TD_COMPRESS_CHUNK) bytes
 * of data into the output buffer. It gets the data by calling data_next.
 * It will return the most data it possibly can. If it returns 0, then
 * there is no more data.
 *
 * By the way that zlib works, each call to zlib_deflate will possibly
 * consume up to avail_in bytes from next_in, and will fill up to
 * avail_out bytes in next_out. Once flush == Z_FINISH, it can not take
 * any more input. It will output until it is finished, and will return
 * Z_STREAM_END.
 */
static ssize_t compress_next(size_t max_out)
{
	ssize_t ret;
	max_out = min(max_out, (size_t)TD_COMPRESS_CHUNK);
	stream.next_out = out_buf;
	stream.avail_out = max_out;
	while (stream.avail_out > 0 && !compress_done) {
		if (stream.avail_in == 0 && flush != Z_FINISH) {
			TRY(stream.avail_in =
			    data_next((const char **)&stream.next_in));
			flush = (stream.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH;
		}
		if (stream.next_in != NULL) {
			TRYM((ret = zlib_deflate(&stream, flush)),
			     "zlib: %s\n", stream.msg);
			compress_done = (ret == Z_STREAM_END);
		}
	}
	ret = max_out - stream.avail_out;
	return ret;
}

static int compress_deinit(void)
{
	TRY(data_deinit());

	zlib_deflateEnd(&stream);
	vfree(stream.workspace);

	/* TODO: remove */
	printk(TAG "Total in: %ld\n", stream.total_in);
	printk(TAG "Total out: %ld\n", stream.total_out);
	return stream.total_out;
}

static int compress_reset(void)
{
	TRY(compress_deinit());
	TRY(compress_init());
	return 0;
}

/* tracedump_init initializes all tracedump components.
 * Call this before tracedump_next
 */
int tracedump_init(void)
{
	TRY(compress_init());
	return 0;
}

/* tracedump_next will print up to max_out data from the tracing ring
 * buffers using the print function selected by print_to. The data is
 * compressed using zlib.
 *
 * The output type of the data is specified by the format_ascii module
 * parameter. If format_ascii == 1, human-readable data will be output.
 * Otherwise, it will output raw data from the ring buffer in cpu order,
 * followed by the saved_cmdlines data.
 */
ssize_t tracedump_next(size_t max_out, int print_to)
{
	ssize_t size;
	TRY(size = compress_next(max_out));
	print(out_buf, size, print_to);
	return size;
}

/* tracedump_all will print all data in the tracing ring buffers using
 * the print function selected by print_to. The data is compressed using
 * zlib, and is surrounded by MAGIC_NUMBER.
 *
 * The output type of the data is specified by the format_ascii module
 * parameter. If format_ascii == 1, human-readable data will be output.
 * Otherwise, it will output raw data from the ring buffer in cpu order,
 * followed by the saved_cmdlines data.
 */
ssize_t tracedump_all(int print_to)
{
	ssize_t ret, size = 0;
	TRY(size += print_magic(print_to));

	do {
		/* Here the size used doesn't really matter,
		 * since we're dumping everything. */
		TRY(ret = tracedump_next(0xFFFFFFFF, print_to));
		size += ret;
	} while (ret > 0);

	TRY(size += print_magic(print_to));

	return size;
}

/* tracedump_deinit deinitializes all tracedump components.
 * This must be called, even on error.
 */
int tracedump_deinit(void)
{
	TRY(compress_deinit());
	return 0;
}

/* tracedump_reset reinitializes all tracedump components. */
int tracedump_reset(void)
{
	TRY(compress_reset());
	return 0;
}



/* tracedump_open opens the tracedump file for reading. */
static int tracedump_open(struct inode *inode, struct file *file)
{
	int ret;
	mutex_lock(&tracedump_proc_lock);
	current_format = format_ascii;
	ret = tracedump_init();
	if (ret < 0)
		goto err;

	ret = nonseekable_open(inode, file);
	if (ret < 0)
		goto err;
	return ret;

err:
	mutex_unlock(&tracedump_proc_lock);
	return ret;
}

/* tracedump_read will reads data from tracedump_next and prints
 * it to userspace. It will surround the data with MAGIC_NUMBER.
 */
static ssize_t tracedump_read(struct file *file, char __user *buf,
			      size_t len, loff_t *offset)
{
	static int done;
	ssize_t size = 0;

	pager.ubuf = buf;

	if (*offset == 0) {
		done = 0;
		TRY(size = print_magic(TD_PRINT_USER));
	} else if (!done) {
		TRY(size = tracedump_next(len, TD_PRINT_USER));
		if (size == 0) {
			TRY(size = print_magic(TD_PRINT_USER));
			done = 1;
		}
	}

	*offset += size;

	return size;
}

static int tracedump_release(struct inode *inode, struct file *file)
{
	int ret;
	ret = tracedump_deinit();
	mutex_unlock(&tracedump_proc_lock);
	return ret;
}

/* tracedump_dump dumps all tracing data from the tracing ring buffers
 * to all consoles. For details about the output format, see
 * tracedump_all.

 * At most max_out bytes are dumped. To accomplish this,
 * tracedump_dump calls tracedump_all several times without writing the data,
 * each time tossing out old data until it reaches its goal.
 *
 * Note: dumping raw pages currently does NOT follow the size limit.
 */

int tracedump_dump(size_t max_out)
{
	ssize_t size;
	size_t consume;

	printk(TAG "\n");

	tracedump_init();

	if (format_ascii) {
		size = tracedump_all(TD_NO_PRINT);
		if (size < 0) {
			printk(TAG "failed to dump\n");
			goto out;
		}
		while (size > max_out) {
			TRY(tracedump_deinit());
			/* Events take more or less 60 ascii bytes each,
			   not counting compression */
			consume = TD_MIN_CONSUME + (size - max_out) /
					(60 / (compress_level + 1));
			TRY(consume_events(consume));
			TRY(tracedump_init());
			size = tracedump_all(TD_NO_PRINT);
			if (size < 0) {
				printk(TAG "failed to dump\n");
				goto out;
			}
		}

		TRY(tracedump_reset());
	}
	size = tracedump_all(TD_PRINT_CONSOLE);
	if (size < 0) {
		printk(TAG "failed to dump\n");
		goto out;
	}

out:
	tracedump_deinit();
	printk(KERN_INFO "\n" TAG " end\n");
	return size;
}

static const struct file_operations tracedump_fops = {
	.owner = THIS_MODULE,
	.open = tracedump_open,
	.read = tracedump_read,
	.release = tracedump_release,
};

#ifdef CONFIG_TRACEDUMP_PANIC
static int tracedump_panic_handler(struct notifier_block *this,
				   unsigned long event, void *unused)
{
	tracedump_dump(panic_size);
	return 0;
}

static struct notifier_block tracedump_panic_notifier = {
	.notifier_call	= tracedump_panic_handler,
	.next		= NULL,
	.priority	= 150   /* priority: INT_MAX >= x >= 0 */
};
#endif

static int __init tracedump_initcall(void)
{
#ifdef CONFIG_TRACEDUMP_PROCFS
	struct proc_dir_entry *entry;

	/* Create a procfs file for easy dumping */
	entry = create_proc_entry("tracedump", S_IFREG | S_IRUGO, NULL);
	if (!entry)
		printk(TAG "failed to create proc entry\n");
	else
		entry->proc_fops = &tracedump_fops;
#endif

#ifdef CONFIG_TRACEDUMP_PANIC
	/* Automatically dump to console on a kernel panic */
	atomic_notifier_chain_register(&panic_notifier_list,
				       &tracedump_panic_notifier);
#endif
	return 0;
}

early_initcall(tracedump_initcall);
