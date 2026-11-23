

#ifndef __PUB_KPROBE_H
#define __PUB_KPROBE_H

#include <linux/kprobes.h>
#include <linux/version.h>

extern int hook_kprobe(struct kprobe *kp, const char *name,
        kprobe_pre_handler_t pre, kprobe_post_handler_t post);
extern void unhook_kprobe(struct kprobe *kp);
int hook_kretprobe(struct kretprobe *ptr_kretprobe, char *kretprobe_func,
	kretprobe_handler_t kretprobe_entry_handler,
	kretprobe_handler_t kretprobe_ret_handler,
	size_t data_size);
void unhook_kretprobe(struct kretprobe *ptr_kretprobe);

#endif /* __PUB_KPROBE_H */
