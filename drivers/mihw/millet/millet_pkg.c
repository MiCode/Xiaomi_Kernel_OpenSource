#define pr_fmt(fmt) "millet-millet_pkg: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/file.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/types.h>
#include <net/sock.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/inet_hashtables.h>
#include <net/inet6_hashtables.h>

static int __init millet_pkg_init(void)
{
	pr_err("enter millet_pkg_init\n");
	return 0;
}

static void __exit millet_pkg_exit(void)
{
	return;
}


module_init(millet_pkg_init);
module_exit(millet_pkg_exit);

MODULE_LICENSE("GPL");
