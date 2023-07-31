#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <misc/xiaomi_panel_notifier.h>

static BLOCKING_NOTIFIER_HEAD(xiaomi_panel_notifier_list);

/*
 *	xiaomi_panel_notifier_register_client - register a client notifier
 *	@nb:notifier block to callback when event happen
 */
int xiaomi_panel_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&xiaomi_panel_notifier_list, nb);
}
EXPORT_SYMBOL(xiaomi_panel_notifier_register_client);

/*
 *	xiaomi_panel_notifier_unregister_client - unregister a client notifier
 *	@nb:notifier block to callback when event happen
 */
int xiaomi_panel_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&xiaomi_panel_notifier_list, nb);
}
EXPORT_SYMBOL(xiaomi_panel_notifier_unregister_client);

/*
 *	xiaomi_panel_notifier_notifier_call_chain - notify clients of xiaomi_panel_notifier_event
 *
 */

int xiaomi_panel_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&xiaomi_panel_notifier_list, val, v);
}
EXPORT_SYMBOL(xiaomi_panel_notifier_call_chain);


static int __init xiaomi_panel_notifier_init(void)
{
	pr_info("%s Entry\n", __func__);

	return 0;
}

static void __exit xiaomi_panel_notifier_exit(void)
{
	pr_info("%s Entry\n", __func__);
}

module_init(xiaomi_panel_notifier_init);
module_exit(xiaomi_panel_notifier_exit);

MODULE_AUTHOR("Xiaomi display driver team");
MODULE_DESCRIPTION("Xiaomi Panel notifier driver");
MODULE_LICENSE("GPL v2");
