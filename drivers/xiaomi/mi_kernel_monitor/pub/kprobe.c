
#include <linux/kallsyms.h>

#include "pub/kprobe.h"
#include "symbol.h"

static DEFINE_MUTEX(kprobe_mutex);
int hook_kprobe(struct kprobe *kp, const char *name,
		kprobe_pre_handler_t pre, kprobe_post_handler_t post)
{
	kprobe_opcode_t *addr;

	if (!name || strlen(name) >= 255)
		return -EINVAL;
	addr = (kprobe_opcode_t *)xm_kallsyms_lookup_name(name);
	if (!addr)
	{
	    printk("huqh_debug in hook_kprobe %s failed!\n", name);
		return -EINVAL;
	}

	printk("huqh_debug in hook_kprobe, %s.\n", name);
	mutex_lock(&kprobe_mutex);
	memset(kp, 0, sizeof(struct kprobe));
	kp->symbol_name = name;
	kp->pre_handler = pre;
	kp->post_handler = post;

	register_kprobe(kp);
	mutex_unlock(&kprobe_mutex);

	return 0;
}

void unhook_kprobe(struct kprobe *kp)
{
	mutex_lock(&kprobe_mutex);
	if (kp->symbol_name != NULL) {
		printk("huqh_debug in hook_probe, %s.\n", kp->symbol_name);
		unregister_kprobe(kp);
	}
	memset(kp, 0, sizeof(struct kprobe));
	mutex_unlock(&kprobe_mutex);
}

int hook_kretprobe(struct kretprobe *ptr_kretprobe, char *kretprobe_func,
	kretprobe_handler_t kretprobe_entry_handler,
	kretprobe_handler_t kretprobe_ret_handler,
	size_t data_size)
{
	int ret;

	memset(ptr_kretprobe, 0, sizeof(struct kretprobe));
	ptr_kretprobe->kp.symbol_name = kretprobe_func;
	ptr_kretprobe->handler = kretprobe_ret_handler;
	ptr_kretprobe->entry_handler = kretprobe_entry_handler;
	ptr_kretprobe->data_size = data_size;
	ptr_kretprobe->maxactive = 200;

	ret = register_kretprobe(ptr_kretprobe);
	if (ret < 0) {
		printk(KERN_INFO "register_kretprobe[%s] failed, returned %d\n",
				kretprobe_func, ret);
		return -1;
	}
	printk(KERN_INFO "Planted return probe at %s: %p\n",
			ptr_kretprobe->kp.symbol_name, ptr_kretprobe->kp.addr);

	return 0;
}

void unhook_kretprobe(struct kretprobe *ptr_kretprobe)
{
	if (!ptr_kretprobe->kp.addr)
		return;

	unregister_kretprobe(ptr_kretprobe);
	printk(KERN_INFO "kretprobe at %p unregistered\n",
			ptr_kretprobe->kp.addr);

	/* nmissed > 0 suggests that maxactive was set too low. */
	printk(KERN_INFO "Missed probing %d instances of %s\n",
			ptr_kretprobe->nmissed, ptr_kretprobe->kp.symbol_name);
	memset(ptr_kretprobe, 0, sizeof(struct kretprobe));
}

