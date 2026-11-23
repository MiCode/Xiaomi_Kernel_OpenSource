#define pr_fmt(fmt) "powerinsight_modem: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include "utils/powerinsight_utils.h"
#include "powerinsight_modem_stats.h"

#define MAX_EVENTS 1000
#define REMOVE_RATIO 3

static struct {
	struct wakeup_event events[MAX_EVENTS];
	unsigned int count;
	spinlock_t lock;
	bool initialized;
} event_buffer = {
	.count = 0,
	.lock = __SPIN_LOCK_UNLOCKED(event_buffer.lock),
	.initialized = false,
};

void __attribute__((used)) report_wakeup_event_to_pi(
	u32 src_node, u32 src_port, u32 dst_node, u32 dst_port,
	u32 payload_low, u32 payload_high, int service_id)
{
	struct wakeup_event event;
	unsigned long flags;

	// 检查是否已初始化，防止卸载后访问
	if (!READ_ONCE(event_buffer.initialized))
		return;

	event.src_node = src_node;
	event.src_port = src_port;
	event.dst_node = dst_node;
	event.dst_port = dst_port;
	event.payload_low = payload_low;
	event.payload_high = payload_high;
	event.service_id = service_id;
	event.timestamp = ktime_get_boottime_ns();

	spin_lock_irqsave(&event_buffer.lock, flags);

	if (event_buffer.count >= MAX_EVENTS) {
		unsigned int remove_count = MAX_EVENTS / REMOVE_RATIO;
		if (remove_count < 1) remove_count = 1;
		if (remove_count >= MAX_EVENTS) remove_count = MAX_EVENTS - 1;
		unsigned int keep_count = MAX_EVENTS - remove_count;
		memmove(event_buffer.events,
			event_buffer.events + remove_count,
			keep_count * sizeof(struct wakeup_event));
		event_buffer.events[keep_count] = event;
		event_buffer.count = keep_count + 1;
	} else {
		event_buffer.events[event_buffer.count++] = event;
	}

	spin_unlock_irqrestore(&event_buffer.lock, flags);
}
EXPORT_SYMBOL(report_wakeup_event_to_pi);

static int get_all_wakeup_events(struct wakeup_event __user *user_buf)
{
	unsigned long flags;
	int event_count;
	struct wakeup_event *local_buf = NULL;

	spin_lock_irqsave(&event_buffer.lock, flags);
	event_count = event_buffer.count;

	if (event_count > 0) {
		local_buf = kmalloc(event_count * sizeof(struct wakeup_event),
				    GFP_ATOMIC);
		if (!local_buf) {
			spin_unlock_irqrestore(&event_buffer.lock, flags);
			return -ENOMEM;
		}
		memcpy(local_buf, event_buffer.events,
		       event_count * sizeof(struct wakeup_event));
		event_buffer.count = 0;
	}

	spin_unlock_irqrestore(&event_buffer.lock, flags);

	if (local_buf) {
		if (copy_to_user(user_buf, local_buf,
				 event_count * sizeof(struct wakeup_event))) {
			kfree(local_buf);
			return -EFAULT;
		}
		kfree(local_buf);
	}

	return event_count;
}

long powerinsight_ioctl_modem(unsigned int cmd, void __user *argp)
{
	// 检查是否已初始化
	if (!READ_ONCE(event_buffer.initialized))
		return -ENODEV;

	switch (cmd) {
	case IOC_GET_WAKEUP_EVENTS: {
		struct modem_wakeup_events k_args;
		if (copy_from_user(&k_args, argp, sizeof(k_args))) {
			pr_err("copy_from_user failed for args!\n");
			return -EFAULT;
		}
		pr_debug("User max_events: %u, events_ptr: 0x%llx\n",
			k_args.max_events, k_args.events_ptr);
		// 验证用户缓冲区大小
		if (k_args.max_events < MAX_EVENTS) {
			pr_err("Invalid max_events: %u (min %d)\n",
				k_args.max_events, MAX_EVENTS);
			return -EINVAL;
		}
		if (!k_args.events_ptr) {
			pr_err("Invalid events_ptr: 0x%llx\n", k_args.events_ptr);
			return -EINVAL;
		}

		int count = get_all_wakeup_events(
			(struct wakeup_event __user *)(uintptr_t)k_args.events_ptr);
		pr_debug("Returning %d events to user\n", count);
		if (count < 0) return count;
		k_args.count = count;
		if (copy_to_user(argp, &k_args, sizeof(k_args)))
			return -EFAULT;

		return 0;
	}
	default:
		return -ENOTTY;
	}
}

// 模块初始化函数
int powerinsight_modem_init(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&event_buffer.lock, flags);
	event_buffer.count = 0;
	event_buffer.initialized = true;
	spin_unlock_irqrestore(&event_buffer.lock, flags);

	pr_info("subsystem initialized\n");
	return 0;
}

// 模块退出函数
void powerinsight_modem_exit(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&event_buffer.lock, flags);
	event_buffer.initialized = false;
	spin_unlock_irqrestore(&event_buffer.lock, flags);

	pr_info("subsystem exited\n");
}
