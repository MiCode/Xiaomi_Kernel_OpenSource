#include <linux/version.h>
#include <linux/module.h>
#include "ipanic.h"
#include "ipanic_version.h"

u8 *log_temp;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
/*
 * The function is used to dump kernel log after *Linux 3.5*
 */
size_t log_rest;
u8 *log_idx;
int ipanic_kmsg_dump3(struct kmsg_dumper *dumper, char *buf, size_t len)
{
	size_t rc = 0;
	size_t sum = 0;
	/* if log_rest is no 0, it means that there some bytes left by previous log entry need to write to buf */
	if (log_rest != 0) {
		memcpy(buf, log_idx, log_rest);
		sum += log_rest;
		log_rest = 0;
	}
	while (kmsg_dump_get_line_nolock(dumper, false, log_temp, len, &rc)
	       && dumper->cur_seq <= dumper->next_seq) {
		if (sum + rc >= len) {
			memcpy(buf + sum, log_temp, len - sum);
			log_rest = rc - (len - sum);
			log_idx = log_temp + (len - sum);
			sum += (len - sum);
			return len;
		}
		memcpy(buf + sum, log_temp, rc);
		sum += rc;
	}
	return sum;
}
EXPORT_SYMBOL(ipanic_kmsg_dump3);
#else
extern int log_buf_copy2(char *dest, int dest_len, int log_copy_start, int log_copy_end);
#endif

void ipanic_klog_region(struct kmsg_dumper *dumper)
{
	static struct ipanic_log_index next = { 0 };
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
#define __LOG_BUF_LEN (1 << CONFIG_LOG_BUF_SHIFT)
	/* tricky usage of dump->list for log_start/log_end index */
	dumper->list.prev =
	    next.value ? (struct list_head *)next.value : (struct list_head *)(log_end -
									       __LOG_BUF_LEN);
	dumper->list.next = (struct list_head *)log_end;
	next.value = log_end;
	LOGD("kernel log region: [%x,%x]", (int)dumper->list.prev, (int)dumper->list.next);
#else
	dumper->cur_idx = next.seq ? next.idx : log_first_idx;
	dumper->cur_seq = next.seq ? next.seq : log_first_seq;
	dumper->next_idx = log_next_idx;
	dumper->next_seq = log_next_seq;
	next.idx = log_next_idx;
	next.seq = log_next_seq;
	LOGD("kernel log region: [%x:%llx,%x:%llx]", dumper->cur_idx, dumper->cur_seq,
	     dumper->next_idx, dumper->next_seq);
#endif
}

int ipanic_klog_buffer(void *data, unsigned char *buffer, size_t sz_buf)
{
	int rc = 0;
	struct kmsg_dumper *dumper = (struct kmsg_dumper *)data;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)
	rc = log_buf_copy2(buffer, sz_buf, (int)dumper->list.prev, (int)dumper->list.next);
	if (rc < 0)
		rc = -1;
	else
		dumper->list.prev = (struct list_head *)(rc + (int)dumper->list.prev);
#else
	dumper->active = true;
	rc = ipanic_kmsg_dump3(dumper, buffer, sz_buf);
	if (rc < 0)
		rc = -1;
#endif
	return rc;
}

void ipanic_log_temp_init(void)
{
	log_temp = (u8 *) __get_free_page(GFP_KERNEL);
}
