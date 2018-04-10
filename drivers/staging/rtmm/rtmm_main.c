
#define pr_fmt(fmt)  "rtmm : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/rtmm.h>

static int __init rtmm_init(void)
{
	struct dentry *dir;

	if (!debugfs_initialized()) {
		pr_warn("debugfs not available, stat dir not created\n");
		return -ENOENT;
	}
	dir = debugfs_create_dir("rtmm", NULL);
	if (!dir) {
		pr_err("debugfs 'rtmm' stat dir creation failed\n");
		return -ENOMEM ;
	}

	rtmm_pool_init(dir);

	rtmm_reclaim_init(dir);

	pr_info("rtmm init OK\n");

	return 0;
}

fs_initcall(rtmm_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Cai Liu <liucai@xiaomi.com>");
