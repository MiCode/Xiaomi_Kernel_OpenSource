#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "xmc_ops.h"

static DEFINE_SPINLOCK(xmc_ops_list_slock);
static LIST_HEAD(xmc_ops_list);

int xmc_ops_charge_enable(struct xmc_device *xmc_dev, bool enable)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->charge_enable)
		return xmc_dev->ops->charge_enable(xmc_dev, enable);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_charge_enable);

int xmc_ops_get_charge_enable(struct xmc_device *xmc_dev, bool *enable)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->get_charge_enable)
		return xmc_dev->ops->get_charge_enable(xmc_dev, enable);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_charge_enable);

int xmc_ops_powerpath_enable(struct xmc_device *xmc_dev, bool enable)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->powerpath_enable)
		return xmc_dev->ops->powerpath_enable(xmc_dev, enable);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_powerpath_enable);

int xmc_ops_get_powerpath_enable(struct xmc_device *xmc_dev, bool *enable)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->get_powerpath_enable)
		return xmc_dev->ops->get_powerpath_enable(xmc_dev, enable);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_powerpath_enable);

int xmc_ops_set_div_mode(struct xmc_device *xmc_dev, enum xmc_cp_div_mode mode)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->set_div_mode)
		return xmc_dev->ops->set_div_mode(xmc_dev, mode);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_set_div_mode);

int xmc_ops_get_div_mode(struct xmc_device *xmc_dev, enum xmc_cp_div_mode *mode)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->get_div_mode)
		return xmc_dev->ops->get_div_mode(xmc_dev, mode);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_div_mode);

int xmc_ops_set_fcc(struct xmc_device *xmc_dev, int value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->set_fcc)
		return xmc_dev->ops->set_fcc(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_set_fcc);

int xmc_ops_set_icl(struct xmc_device *xmc_dev, int value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->set_icl)
		return xmc_dev->ops->set_icl(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_set_icl);

int xmc_ops_set_fv(struct xmc_device *xmc_dev, int value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->set_fv)
		return xmc_dev->ops->set_fv(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_set_fv);

int xmc_ops_set_iterm(struct xmc_device *xmc_dev, int value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->set_iterm)
		return xmc_dev->ops->set_iterm(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_set_iterm);

int xmc_ops_set_vinmin(struct xmc_device *xmc_dev, int value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->set_vinmin)
		return xmc_dev->ops->set_vinmin(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_set_vinmin);

int xmc_ops_get_vbus(struct xmc_device *xmc_dev, int *value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->get_vbus)
		return xmc_dev->ops->get_vbus(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_vbus);

int xmc_ops_get_ibus(struct xmc_device *xmc_dev, int *value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->get_ibus)
		return xmc_dev->ops->get_ibus(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_ibus);

int xmc_ops_get_ts(struct xmc_device *xmc_dev, int *value)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->get_ts)
		return xmc_dev->ops->get_ts(xmc_dev, value);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_ts);

int xmc_ops_adc_enable(struct xmc_device *xmc_dev, bool enable)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->adc_enable)
		return xmc_dev->ops->adc_enable(xmc_dev, enable);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_adc_enable);

int xmc_ops_device_init(struct xmc_device *xmc_dev, enum xmc_cp_div_mode mode)
{
	if (xmc_dev && xmc_dev->ops && xmc_dev->ops->device_init)
		return xmc_dev->ops->device_init(xmc_dev, mode);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_device_init);

int xmc_ops_request_vdm_cmd(struct xmc_device *xmc_dev, enum uvdm_state cmd, unsigned char *data, unsigned int data_len)
{
	if (xmc_dev != NULL && xmc_dev->ops != NULL && xmc_dev->ops->request_vdm_cmd)
		return xmc_dev->ops->request_vdm_cmd(xmc_dev, cmd, data, data_len);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_request_vdm_cmd);

int xmc_ops_set_cap(struct xmc_device *xmc_dev, enum xmc_pdo_type type, int mV, int mA)
{
	if (xmc_dev != NULL && xmc_dev->ops != NULL && xmc_dev->ops->set_cap)
		return xmc_dev->ops->set_cap(xmc_dev, type, mV, mA);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_set_cap);

int xmc_ops_get_cap(struct xmc_device *xmc_dev, enum xmc_pdo_type type, struct xmc_pd_cap *cap)
{
	if (xmc_dev != NULL && xmc_dev->ops != NULL && xmc_dev->ops->get_cap)
		return xmc_dev->ops->get_cap(xmc_dev, type, cap);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_cap);

int xmc_ops_get_pd_id(struct xmc_device *xmc_dev)
{
	if (xmc_dev != NULL && xmc_dev->ops != NULL && xmc_dev->ops->get_pd_id)
		return xmc_dev->ops->get_pd_id(xmc_dev);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(xmc_ops_get_pd_id);

void *xmc_ops_get_data(const struct xmc_device *xmc_dev)
{
	return xmc_dev->data;
}
EXPORT_SYMBOL(xmc_ops_get_data);

struct xmc_device *xmc_ops_find_device(const char *name)
{
	unsigned long flags = 0;
	struct xmc_device *xmc_dev = NULL;
	bool found = false;

	if (!name)
		return NULL;

	spin_lock_irqsave(&xmc_ops_list_slock, flags);
	if (list_empty(&xmc_ops_list))
		goto out;

	list_for_each_entry(xmc_dev, &xmc_ops_list, list) {
		if (!strcmp(xmc_dev->name, name)) {
			found = true;
			break;
		}
	}
out:
	spin_unlock_irqrestore(&xmc_ops_list_slock, flags);

	return found ? xmc_dev : NULL;
}
EXPORT_SYMBOL(xmc_ops_find_device);

struct xmc_device *xmc_device_register(const char *name, const struct xmc_ops *ops, void *data)
{
	struct xmc_device *xmc_dev = NULL;
	unsigned long flags = 0;

	xmc_dev = kzalloc(sizeof(struct xmc_device), GFP_KERNEL);
	if (!xmc_dev)
		return ERR_PTR(-ENOMEM);

	xmc_dev->name = kstrdup(name, GFP_KERNEL);
	if (!xmc_dev->name) {
		kfree(xmc_dev);
		return ERR_PTR(-ENOMEM);
	}

	xmc_dev->data = data;
	xmc_dev->ops = ops;

	spin_lock_irqsave(&xmc_ops_list_slock, flags);
	list_add(&xmc_dev->list, &xmc_ops_list);
	spin_unlock_irqrestore(&xmc_ops_list_slock, flags);

	return xmc_dev;
}
EXPORT_SYMBOL(xmc_device_register);

void xmc_device_unregister(struct xmc_device *xmc_dev)
{
	unsigned long flags = 0;

	if (!xmc_dev)
		return;

	spin_lock_irqsave(&xmc_ops_list_slock, flags);
	list_del(&xmc_dev->list);
	spin_unlock_irqrestore(&xmc_ops_list_slock, flags);

	kfree(xmc_dev->name);
	kfree(xmc_dev);

	return;
}
EXPORT_SYMBOL(xmc_device_unregister);

MODULE_LICENSE("GPL");
