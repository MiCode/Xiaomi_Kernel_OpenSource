
#include "linux/battmngr/platform_class.h"

static struct class *battmngr_class;

static ssize_t name_show(struct device *dev,
  				    struct device_attribute *attr, char *buf)
{
  	struct battmngr_device *battmg_dev = to_battmngr_device(dev);

  	return snprintf(buf, 20, "%s\n",
  		       battmg_dev->props.alias_name ?
  		       battmg_dev->props.alias_name : "anonymous");
}

static DEVICE_ATTR_RO(name);

static struct attribute *battmngr_class_attrs[] = {
  	&dev_attr_name.attr,
  	NULL,
};

static const struct attribute_group battmngr_group = {
  	.attrs = battmngr_class_attrs,
};

static const struct attribute_group *battmngr_groups[] = {
  	&battmngr_group,
  	NULL,
};

static void battmngr_device_release(struct device *dev)
{
  	struct battmngr_device *chg_dev = to_battmngr_device(dev);

  	kfree(chg_dev);
}

int battmngr_qtiops_get_fg_soc(struct battmngr_device *battmg_dev)
{
    int soc = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg_soc) {
  		rc = battmg_dev->ops->fg_soc(&soc);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return soc;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg_soc);

int battmngr_qtiops_get_fg_curr(struct battmngr_device *battmg_dev)
{
    int curr = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg_curr) {
  		rc = battmg_dev->ops->fg_curr(&curr);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return curr;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg_curr);


int battmngr_qtiops_get_fg_volt(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg_volt) {
  		rc = battmg_dev->ops->fg_volt(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg_volt);

int battmngr_qtiops_get_fg_temp(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg_temp) {
  		rc = battmg_dev->ops->fg_temp(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg_temp);

int battmngr_qtiops_get_charge_status(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->charge_status) {
  		rc = battmg_dev->ops->charge_status(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_charge_status);

int battmngr_qtiops_get_fg1_ibatt(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg1_ibatt) {
  		rc = battmg_dev->ops->fg1_ibatt(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_ibatt);

int battmngr_qtiops_get_fg2_ibatt(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg2_ibatt) {
  		rc = battmg_dev->ops->fg2_ibatt(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg2_ibatt);

int battmngr_qtiops_get_fg1_volt(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg1_volt) {
  		rc = battmg_dev->ops->fg1_volt(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_volt);

int battmngr_qtiops_get_fg2_volt(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg2_volt) {
  		rc = battmg_dev->ops->fg2_volt(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg2_volt);

int battmngr_qtiops_get_fg1_fcc(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg1_fcc) {
  		rc = battmg_dev->ops->fg1_fcc(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_fcc);

int battmngr_qtiops_get_fg2_fcc(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg2_fcc) {
  		rc = battmg_dev->ops->fg2_fcc(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg2_fcc);

int battmngr_qtiops_get_fg1_rm(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg1_rm) {
  		rc = battmg_dev->ops->fg1_rm(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_rm);

int battmngr_qtiops_get_fg2_rm(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg2_rm) {
  		rc = battmg_dev->ops->fg2_rm(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg2_rm);

int battmngr_qtiops_get_fg1_raw_soc(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg1_raw_soc) {
  		rc = battmg_dev->ops->fg1_raw_soc(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_raw_soc);

int battmngr_qtiops_get_fg2_raw_soc(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg2_raw_soc) {
  		rc = battmg_dev->ops->fg2_raw_soc(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg2_raw_soc);

int battmngr_qtiops_get_fg1_soc(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg1_soc) {
  		rc = battmg_dev->ops->fg1_soc(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_soc);

int battmngr_qtiops_get_fg2_soc(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg2_soc) {
  		rc = battmg_dev->ops->fg2_soc(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg2_soc);

int battmngr_qtiops_set_fg1_fastcharge(struct battmngr_device *battmg_dev, int value)
{
    int rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->set_fg1_fastcharge) {
  		rc = battmg_dev->ops->set_fg1_fastcharge(value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_set_fg1_fastcharge);

int battmngr_qtiops_set_fg2_fastcharge(struct battmngr_device *battmg_dev, int value)
{
    int rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->set_fg2_fastcharge) {
  		rc = battmg_dev->ops->set_fg2_fastcharge(value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_set_fg2_fastcharge);

int battmngr_qtiops_get_fg1_fastcharge(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->get_fg1_fastcharge) {
  		rc = battmg_dev->ops->get_fg1_fastcharge(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_fastcharge);

int battmngr_qtiops_get_fg1_temp(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg1_temp) {
  		rc = battmg_dev->ops->fg1_temp(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg1_temp);

int battmngr_qtiops_get_fg2_temp(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg2_temp) {
  		rc = battmg_dev->ops->fg2_temp(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_fg2_temp);

int battmngr_qtiops_set_fg_suspend(struct battmngr_device *battmg_dev, int value)
{
    int rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->fg_suspend) {
  		rc = battmg_dev->ops->fg_suspend(value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_set_fg_suspend);

int battmngr_qtiops_set_term_cur(struct battmngr_device *battmg_dev, int value)
{
    int rc = 0;

  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->set_term_cur) {
  		rc = battmg_dev->ops->set_term_cur(value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_set_term_cur);

int battmngr_qtiops_get_batt_auth(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->get_batt_auth) {
  		rc = battmg_dev->ops->get_batt_auth(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_batt_auth);

int battmngr_qtiops_get_chip_ok(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->get_chip_ok) {
  		rc = battmg_dev->ops->get_chip_ok(&value);
        if (rc < 0) {
            class_err("%s: get qti ops is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_qtiops_get_chip_ok);

bool check_qti_ops(struct battmngr_device **battmg_dev)
{

	if (*battmg_dev)
		return true;

	*battmg_dev = get_adapter_by_name("qti_ops");
	if (!(*battmg_dev)) {
		class_err("xm charge get battmngr dev is fail");
		return false;
	}

	class_info("xm charge get battmngr dev is success");
	return true;
}
EXPORT_SYMBOL(check_qti_ops);

int battmngr_noops_get_online(struct battmngr_device *battmg_dev)
{
    u8 value = 0;
    int rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->online) {
  		rc = battmg_dev->ops->online(&value);
        if (rc < 0) {
            class_err("%s: get nano ops online is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_noops_get_online);

int battmngr_noops_get_real_type(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->real_type) {
  		rc = battmg_dev->ops->real_type(&value);
        if (rc < 0) {
            class_err("%s: get nano ops real type is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_noops_get_real_type);

int battmngr_noops_get_usb_type(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->usb_type) {
  		rc = battmg_dev->ops->usb_type(&value);
        if (rc < 0) {
            class_err("%s: get nano ops usb type is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_noops_get_usb_type);

int battmngr_noops_get_input_curr_limit(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->input_curr_limit) {
  		rc = battmg_dev->ops->input_curr_limit(&value);
        if (rc < 0) {
            class_err("%s: get nano ops input curr limit is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_noops_get_input_curr_limit);

int battmngr_noops_get_power_max(struct battmngr_device *battmg_dev)
{
    int value = 0, rc = 0;
  	if (battmg_dev != NULL && battmg_dev->ops != NULL &&
  	    battmg_dev->ops->power_max) {
  		rc = battmg_dev->ops->power_max(&value);
        if (rc < 0) {
            class_err("%s: get nano ops input curr limit is fail\n", __func__);
            return rc;
        }
        return value;
    }

  	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(battmngr_noops_get_power_max);

struct battmngr_device* check_nano_ops(void)
{
	struct battmngr_device *battmg_dev = get_adapter_by_name("nano_ops");

	if (!battmg_dev) {
		class_err("nano get battmngr dev is fail");
		return NULL;
	}

	class_info("nano get battmngr dev is success");
	return battmg_dev;
}
EXPORT_SYMBOL(check_nano_ops);

/**
 * battmngr_device_register - create and register a new object of
 *   battmngr_device class.
 * @name: the name of the new object
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use.
 * The methods may retrieve it by using adapter_get_data(adapter_dev).
 * @ops: the charger operations structure.
 *
 * Creates and registers new charger device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct battmngr_device *battmngr_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct battmngr_ops *ops,
		const struct battmngr_properties *props)
{
	struct battmngr_device *battmg_dev = NULL;
	static struct lock_class_key key;
	struct srcu_notifier_head *head = NULL;
	int rc;

	class_info("%s: name=%s\n", __func__, name);
	battmg_dev = kzalloc(sizeof(*battmg_dev), GFP_KERNEL);
	if (!battmg_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&battmg_dev->ops_lock);
	battmg_dev->dev.class = battmngr_class;
	battmg_dev->dev.parent = parent;
	battmg_dev->dev.release = battmngr_device_release;
	dev_set_name(&battmg_dev->dev, "%s", name);
	dev_set_drvdata(&battmg_dev->dev, devdata);
	head = &battmg_dev->evt_nh;
	srcu_init_notifier_head(head);
	/* Rename srcu's lock to avoid LockProve warning */
	lockdep_init_map(&(&head->srcu)->dep_map, name, &key, 0);

	/* Copy properties */
	if (props) {
		memcpy(&battmg_dev->props, props,
		       sizeof(struct battmngr_properties));
	}
	rc = device_register(&battmg_dev->dev);
	if (rc) {
		kfree(battmg_dev);
		return ERR_PTR(rc);
	}
	battmg_dev->ops = ops;
    class_info("%s: is successful\n", __func__);
	return battmg_dev;
}
EXPORT_SYMBOL(battmngr_device_register);

/**
 * battmngr_device_unregister - unregisters a switching charger device
 * object.
 * @adapter_dev: the switching charger device object to be unregistered
 * and freed.
 *
 * Unregisters a previously registered via battmngr_device_register object.
 */
void battmngr_device_unregister(struct battmngr_device *adapter_dev)
{
	if (!adapter_dev)
		return;

	mutex_lock(&adapter_dev->ops_lock);
	adapter_dev->ops = NULL;
	mutex_unlock(&adapter_dev->ops_lock);
	device_unregister(&adapter_dev->dev);
}
EXPORT_SYMBOL(battmngr_device_unregister);

static int battmngr_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct battmngr_device *get_adapter_by_name(const char *name)
{
	struct device *dev = NULL;

	if (!name)
		return (struct battmngr_device *)NULL;
	dev = class_find_device(battmngr_class, NULL, name,
				battmngr_match_device_by_name);

	return dev ? to_battmngr_device(dev) : NULL;

}
EXPORT_SYMBOL(get_adapter_by_name);

void battmngr_class_exit(void)
{
	class_destroy(battmngr_class);
}

int battmngr_class_init(void)
{
	battmngr_class = class_create(THIS_MODULE, "Battmngr_class");
	if (IS_ERR(battmngr_class)) {
		class_err("Unable to create Battmngr_class; errno = %ld\n",
			PTR_ERR(battmngr_class));
		return PTR_ERR(battmngr_class);
	}
	battmngr_class->dev_groups = battmngr_groups;

	return 0;
}

module_init(battmngr_class_init);
module_exit(battmngr_class_exit);
 
MODULE_DESCRIPTION("Battmngr Class Device");
MODULE_AUTHOR("litianpeng6@xiaomi.com");
MODULE_LICENSE("GPL v2");

