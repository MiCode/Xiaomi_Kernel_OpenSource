#include "spi-xiaomi-tp.h"

static struct ts_spi_info owner;

static ssize_t ts_xsfer_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	mutex_lock(&owner.lock);
	ret = snprintf(buf, PAGE_SIZE, "[%s] used:%d, tmp:%d\n", owner.name, owner.used, owner.tmp);
	mutex_unlock(&owner.lock);
	return ret;
}

static DEVICE_ATTR(ts_xsfer_state, S_IRUGO, ts_xsfer_state_show, NULL);


/*tmporary get spi device's ownership until invoke tmp_drop_ts_xsfer()
 *
 *return -EBUSY others is using, -EPERM if spi device already belong to others.
 */
int32_t tmp_hold_ts_xsfer(struct spi_device **client)
{

	if (!owner.init) {
		PDEBUG("ts_xsfer does not exist");
		return -EINVAL;
	}
	mutex_lock(&owner.lock);
	if (owner.used) {
		PDEBUG("ts_xsfer belong to %s, can't tmporary use it\n", owner.name);
		mutex_unlock(&owner.lock);
		return -EPERM;
	}

	if (owner.tmp) {
		PDEBUG("ts_xsfer is in using, others can't use it now\n");
		mutex_unlock(&owner.lock);
		return -EBUSY;
	}

	owner.tmp = true;
	*client = owner.client;
	mutex_unlock(&owner.lock);
	return 0;
}
EXPORT_SYMBOL_GPL(tmp_hold_ts_xsfer);


/*must balance with tmp_hold_ts_xsfer*/
void tmp_drop_ts_xsfer(void)
{
	if (owner.init) {
		mutex_lock(&owner.lock);
		owner.tmp = false;
		mutex_unlock(&owner.lock);
	}
}
EXPORT_SYMBOL_GPL(tmp_drop_ts_xsfer);

/*make spi device belong to module which invoke this function
 *-EPERM if spi device already belong to others.
 */
int32_t get_ts_xsfer(const char *name)
{
	if (!name) {
		PDEBUG("name can't be empty\n");
		return -EINVAL;
	}
	mutex_lock(&owner.lock);
	if (owner.used) {
		PDEBUG("ts_xsfer belong to %s, others can't get it\n", owner.name);
		mutex_unlock(&owner.lock);
		return -EPERM;
	}
	owner.used = true;
	owner.name = name;
	mutex_unlock(&owner.lock);
	return 0;
}
EXPORT_SYMBOL_GPL(get_ts_xsfer);


void put_ts_xsfer(const char *name)
{
	if (name && !strcmp(name, owner.name)) {
		mutex_lock(&owner.lock);
		owner.used = false;
		owner.name = NULL;
		mutex_unlock(&owner.lock);
	} else {
		PDEBUG("must released by owner\n");
	}
}
EXPORT_SYMBOL_GPL(put_ts_xsfer);



const char *get_owner_name(void)
{
	return owner.name;
}
EXPORT_SYMBOL_GPL(get_owner_name);

/*test weather spi device belong someone, if not get the device's ownership.
 *name@:name of owner
 *return NULL if device already been used.
 */
struct spi_device *test_then_get_spi(const char *name)
{
	struct spi_device *ret;

	mutex_lock(&owner.lock);
	if (owner.used) {
		ret = NULL;
	} else {
		owner.used = true;
		ret = owner.client;
		if (name)
			owner.name = name;
	}
	mutex_unlock(&owner.lock);

	return ret;
}
EXPORT_SYMBOL_GPL(test_then_get_spi);

static int32_t ts_spi_probe(struct spi_device *client)
{
	int32_t ret;
	PDEBUG("Start probe\n");
	mutex_init(&owner.lock);
	owner.name = NULL;
	owner.used = false;
	owner.tmp = false;
	owner.client = client;

	ret = sysfs_create_file(&client->dev.kobj, &dev_attr_ts_xsfer_state.attr);
	if (ret < 0) {
		PDEBUG("create sysfs failed\n");
	}

	owner.init = true;
	PDEBUG("init ts_xsfer device successful\n");

	return 0;
}

static int32_t ts_spi_remove(struct spi_device *client)
{
	PDEBUG("touch_xsfer will be remove, touch must stop spi xsfer\n");
	return 0;
}

static struct of_device_id ts_match_tbl[] = {
	{ .compatible = "xiaomi,spi-for-tp", },
	{},
};

static struct spi_driver touch_spi_drv = {
	.probe = ts_spi_probe,
	.remove = ts_spi_remove,
	.driver = {
		.name = "touch_xsfer",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ts_match_tbl),
	},
};

static int32_t __init touch_spi_init(void)
{
	PDEBUG("Start register spi for tp\n");
	return spi_register_driver(&touch_spi_drv);
}

static void __exit touch_spi_exit(void)
{
	spi_unregister_driver(&touch_spi_drv);
}


module_init(touch_spi_init);
module_exit(touch_spi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("XIAOMI,JIANGHAO");
