

#include <linux/kallsyms.h>
#include <linux/version.h>

#include "pub/kprobe.h"
#include "symbol.h"

unsigned long (*xm_kallsyms_lookup_name)(const char *name);

static int (*xm_kallsyms_on_each_symbol)(int (*fn)(void *, const char *,
						    struct module *,
						    unsigned long),
					  void *data);

#include <linux/kprobes.h>
static struct kprobe kprobe_kallsyms_lookup_name = {
    .symbol_name = "kallsyms_lookup_name"
};

static int noop_pre_handler(struct kprobe* p, struct pt_regs *regs)
{
    return 0;
}

int xiaomi_init_symbol(void)
{
    int ret = 0;
    kprobe_kallsyms_lookup_name.pre_handler = noop_pre_handler;
    register_kprobe(&kprobe_kallsyms_lookup_name);
    if (ret < 0) {
	    printk("mi_kernel-debug, symbol register_kprobe failed!\n");
	    return -EINVAL;
    }
	   
    xm_kallsyms_lookup_name = (void *)kprobe_kallsyms_lookup_name.addr;
    unregister_kprobe(&kprobe_kallsyms_lookup_name);

    ////printk("mi_kernel-debug, xm_kallsyms_lookup_name is %p\n", xm_kallsyms_lookup_name);

    if (!xm_kallsyms_lookup_name) {
        printk("mi_kernel-debug, xm_kallsyms_lookup_name get failed!\n");
        ////return -EINVAL;
    }

	xm_kallsyms_on_each_symbol = (void *)xm_kallsyms_lookup_name("kallsyms_on_each_symbol");
	if (!xm_kallsyms_on_each_symbol) {
	    printk("mi_kernel-debug, xm_kallsyms_on_each_symbol get failed!\n");
        return -EINVAL;
	}

    return 0;
}

struct xm_symbol_info {
    char *symbol;
    int count;
};

static inline int get_symbol_count_callback(void *data, const char *name,
            struct module *mod, unsigned long addr)
{
    struct xm_symbol_info *info = data;

    if (strcmp(name, info->symbol) == 0) {
        info->count++;
        return 0;
    }

    return 0;
}

int xiaomi_get_symbol_count(char *symbol)
{
	struct xm_symbol_info info;

	info.symbol = symbol;
	info.count = 0;  
	xm_kallsyms_on_each_symbol(get_symbol_count_callback, &info);

	return info.count;
}

