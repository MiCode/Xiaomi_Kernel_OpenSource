/**
 * @file battery_secret_class.c
 * @brief 电池加密芯片class接口层
 * 
 * 该模块提供外部接口给上层，并提供注册接口给IC驱动层
 * 
 * @author longcheer
 * @date 2025年02月14日
 */
#include <linux/string.h>
#include <linux/init.h>
#include "battery_secret_class.h"
#include "battery_secret_logic.h"

#define MODULE_TAG "secret_manager"
//#include "lc_logfs_class.h"

static struct class *secret_class;


static void secret_device_release(struct device *dev)
{
	struct secret_device *secret_dev = to_secret_device(dev);

	kfree(secret_dev);
}

static int secret_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct secret_device *get_secret_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return (struct secret_device *)NULL;
	dev = class_find_device(secret_class, NULL, name,
				secret_match_device_by_name);

	return dev ? to_secret_device(dev) : NULL;

}
EXPORT_SYMBOL(get_secret_by_name);


void secret_device_unregister(struct secret_device *secret_dev)
{
	if (!secret_dev)
		return;

	mutex_lock(&secret_dev->ops_lock);
	secret_dev->ops = NULL;
	mutex_unlock(&secret_dev->ops_lock);
	debugfs_device_node_deinit(&secret_dev->dev);
	device_unregister(&secret_dev->dev);
}
EXPORT_SYMBOL(secret_device_unregister);

struct secret_device *secret_device_register(const char *role,
		const char *chip_name,
		struct device *parent, void *devdata,
		const struct secret_ops *ops)
{
	struct secret_device *secret_dev;
	//static struct lock_class_key key;
	//struct srcu_notifier_head *head;
	int rc;
	struct secret_device * s_dev = NULL;
	pr_info("%s: role=%s chip=%s \n", __func__, role, chip_name);
	if(IS_ERR_OR_NULL(ops)){
		pr_err("%s ops is null! \n", __func__);
		return ERR_PTR(-EINVAL);
	}
	s_dev = get_secret_by_name(role);
	if(s_dev){
		pr_err("%s %s is already registed by chip:%s \n", __func__, role, s_dev->chip_name);
		return ERR_PTR(-EINVAL);
	}
	secret_dev = kzalloc(sizeof(*secret_dev), GFP_KERNEL);
	if (!secret_dev)
		return ERR_PTR(-ENOMEM);

	//head = &secret_dev->evt_nh;
	//srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	//lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);
	
	mutex_init(&secret_dev->ops_lock);
	secret_dev->dev.class = secret_class;
	secret_dev->dev.parent = parent;
	secret_dev->dev.release = secret_device_release;
	dev_set_name(&secret_dev->dev, "%s",role);
	strlcpy(secret_dev->chip_name, chip_name, sizeof(secret_dev->chip_name));
	dev_set_drvdata(&secret_dev->dev, devdata);

	/* Copy properties */
	//if (props) {
	//	memcpy(&secret_dev->props, props,
	//	       sizeof(struct secret_properties));
	//}
	secret_dev->ops = ops;
	rc = device_register(&secret_dev->dev);
	if (rc) {
		kfree(secret_dev);
		return ERR_PTR(rc);
	}
	debugfs_device_node_init(&secret_dev->dev);

	return secret_dev;
}
EXPORT_SYMBOL(secret_device_register);

int lc_get_uisoh(u8 *data, size_t len)
{
	return ll_get_uisoh(data, len);
}
EXPORT_SYMBOL(lc_get_uisoh);

int lc_set_uisoh(u8 *data, size_t len)
{
	return ll_set_uisoh(data, len);
}
EXPORT_SYMBOL(lc_set_uisoh);

int lc_get_rawsoh(void)
{
	return ll_get_rawsoh();
}
EXPORT_SYMBOL(lc_get_rawsoh);

int lc_set_rawsoh(int rawsoh)
{
	return ll_set_rawsoh(rawsoh);
}
EXPORT_SYMBOL(lc_set_rawsoh);

int lc_get_cycle_count(void)
{
	return ll_get_cycle_count();
}
EXPORT_SYMBOL(lc_get_cycle_count);

int lc_set_cycle_count(int value)
{
	return ll_set_cycle_count(value);
}
EXPORT_SYMBOL(lc_set_cycle_count);

//// fake api

int lc_get_fake_rawsoh(void){
	return ll_get_fake_rawsoh();
}
EXPORT_SYMBOL(lc_get_fake_rawsoh);

int lc_set_fake_rawsoh(int value)
{
	return ll_set_fake_rawsoh(value);
}
EXPORT_SYMBOL(lc_set_fake_rawsoh);

int lc_get_fake_cycle_count(void)
{
	return ll_get_fake_cycle_count();
}
EXPORT_SYMBOL(lc_get_fake_cycle_count);

int lc_set_fake_cycle_count(int value)
{
	return ll_set_fake_cycle_count(value);
}
EXPORT_SYMBOL(lc_set_fake_cycle_count);

const char *lc_get_battery_manufacture_date(void)
{  // 电池出厂时间
	return ll_get_battery_manufacture_date();
}
EXPORT_SYMBOL(lc_get_battery_manufacture_date);

const char *lc_get_battery_first_use_time(void)
{ // 电池首次使用时间
	return ll_get_battery_first_use_time();
}
EXPORT_SYMBOL(lc_get_battery_first_use_time);

int lc_set_battery_first_use_time(char *time)
{ // 电池首次使用时间
	if(!time)
		return -EINVAL;
	return ll_set_battery_first_use_time(time);
}
EXPORT_SYMBOL(lc_set_battery_first_use_time);

int lc_clear_cycle_count(void)
{
	return ll_clear_cycle_count();
}
EXPORT_SYMBOL(lc_clear_cycle_count);

int lc_get_battery_id(void)
{ // 电池ID
	return ll_get_battery_id();
}
EXPORT_SYMBOL(lc_get_battery_id);

const char *lc_get_battery_secret_name(void)
{ // 加密芯片名称
	return ll_get_battery_secret_name();
}
EXPORT_SYMBOL(lc_get_battery_secret_name);

int lc_is_battery_auth_success(void){
	// 鉴权是否成功
	return ll_is_battery_auth_success();
}
EXPORT_SYMBOL(lc_is_battery_auth_success);

const char *lc_get_battery_sn(void)
{	// 电池SN号
	return ll_get_battery_sn();
}
EXPORT_SYMBOL(lc_get_battery_sn);

const char *find_secret_attr_by_name(const char *name)
{
	// 
	return ll_find_secret_attr_by_name(name);
}
EXPORT_SYMBOL(find_secret_attr_by_name);

int ll_check_secret_chip_online(void)
{
	static int first_check = 1;
	static const char *chip_name_cmdline = NULL;

	if (first_check) {
		first_check = 0;
		chip_name_cmdline = find_secret_attr_by_name("chip_name");
	}

	if (!chip_name_cmdline)
		return CHIP_UNSUPRT;
	if (!strcmp(chip_name_cmdline, "unsupported"))
		return CHIP_UNSUPRT;
	if (!strcmp(chip_name_cmdline, "unknown"))
		return CHIP_UNKNOWN;
	return CHIP_ONLINE;
}
EXPORT_SYMBOL(ll_check_secret_chip_online);

static int __init secret_class_init(void)
{
	secret_class = class_create(THIS_MODULE, "battery_secret");
	if (IS_ERR(secret_class)) {
		pr_notice("Unable to create battery_secret class; errno = %ld\n",
			PTR_ERR(secret_class));
		return PTR_ERR(secret_class);
	}
	battery_secret_logic_layer_init();
	//secret_class->dev_groups = secret_groups;
	return 0;
}
static void __exit secret_class_exit(void)
{
	struct secret_device *master = NULL;
	battery_secret_logic_layer_deinit();
	master = get_secret_by_name(MASTER_SECRET);
	if(master) {
		secret_device_unregister(master);
	}
#ifdef DUAL_BATTERY_SECRET
	struct secret_device *slave = get_secret_by_name(SLAVE_SECRET);
	if (slave) {
		secret_device_unregister(slave);
	}
#endif
	class_destroy(secret_class);
}
module_init(secret_class_init);
module_exit(secret_class_exit);

MODULE_DESCRIPTION("battery secret Class Device");
MODULE_AUTHOR("LONGCHEER");
MODULE_LICENSE("GPL");
