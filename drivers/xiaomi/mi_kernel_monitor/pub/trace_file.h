

#include <linux/proc_fs.h>
#include <linux/version.h>

////#include "pub/trace_buffer.h"

#define XM_TRACE_BUF_SIZE 1024

struct xm_trace_buffer {
	struct {
		char *data;
		unsigned int pos;
		int circle;
		unsigned int tail;
		spinlock_t lock;
		struct mutex mutex;
	} buffer;

	struct {
		char *data;
		unsigned int len;
	} product;

	char fmt_buffer[XM_TRACE_BUF_SIZE];
	unsigned int buf_size;
};

struct xm_trace_file;

typedef ssize_t (*xm_trace_file_prepare_read)(struct xm_trace_file *trace_file,
		struct file *file, char __user *buf, size_t size, loff_t *ppos);
typedef ssize_t (*xm_trace_file_write_cb)(struct xm_trace_file *trace_file,
		struct file *file, const char __user *buf, size_t count,
		loff_t *offs);

struct xm_trace_file {
	struct xm_trace_buffer trace_buffer;
	struct proc_ops proc_ops;
	struct proc_dir_entry *pe;
	xm_trace_file_prepare_read prepare_read;
	xm_trace_file_write_cb write;
	unsigned int buf_size;
	char file_name[255];
};

void trace_file_cgroups_tsk(int pre, struct xm_trace_file *file, struct task_struct *tsk);
void trace_file_nolock_cgroups_tsk(int pre, struct xm_trace_file *file, struct task_struct *tsk);
void trace_file_cgroups(int pre, struct xm_trace_file *file);
void trace_file_nolock_cgroups(int pre, struct xm_trace_file *file);
