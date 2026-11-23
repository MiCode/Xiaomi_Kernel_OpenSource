#ifndef POWERINSIGHT_MODEM_STATS_H
#define POWERINSIGHT_MODEM_STATS_H
#include <linux/sched.h>
#include <linux/types.h>
#include "powerinsight_ioctl.h"

struct wakeup_event {
	u32 src_node;
	u32 src_port;
	u32 dst_node;
	u32 dst_port;
	u32 payload_low;
	u32 payload_high;
	int service_id;
	u64 timestamp;
} __packed;

/* IOCTL参数结构 */
struct modem_wakeup_events {
	unsigned int max_events;
	unsigned int count;
	u64 events_ptr; /* 用户空间事件缓冲区的地址 */
} __packed;


/* 定义IOCTL命令 */
#define IOC_GET_WAKEUP_EVENTS \
    POWERINSIGHT_MODEM_DIR_IOC(_IOC_READ, 1, struct modem_wakeup_events)

long powerinsight_ioctl_modem(unsigned int cmd, void __user *argp);
void report_wakeup_event_to_pi(u32 src_node, u32 src_port, u32 dst_node, u32 dst_port, u32 payload_low, u32 payload_high, int service_id);
int powerinsight_modem_init(void);
void powerinsight_modem_exit(void);
#endif // POWERINSIGHT_MODEM_STATS_H
