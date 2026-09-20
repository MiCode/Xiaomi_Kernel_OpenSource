#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
/* Add 0508*/
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
/* -Add 0508*/

struct rfcable_det_data {
	struct device *dev;
	struct pinctrl *dev_pinctrl;
	int status_gpio;
};
static ssize_t rfcable_det_status(struct file *f_det, char __user *buf, size_t count, loff_t *boff) {
	char out_buf[PAGE_SIZE];
	int ret = 0;
	ssize_t out_length = 0, real_len = 0;
	struct rfcable_det_data *data = pde_data(file_inode(f_det));
	if(!data) {
		pr_err("[RFCableDet]CANNOT GET CABLE DATA!\n");
		return -EFAULT;
	}
	pr_info("[RFCableDet]detected GPIO is %d\n", data->status_gpio);
	out_length = sprintf(out_buf, "%d\n", gpio_get_value(data->status_gpio));
	if (out_length > *boff)
		out_length -= *boff;
	else
		out_length = 0;
	real_len = out_length > count ? count : out_length;
	ret = copy_to_user(buf, out_buf, real_len);
	if (ret) {
		pr_err("[RFCableDet]CANNOT OUTPUT CABLE STATUS: error code %d\n", ret);
		return -EFAULT;
	}
	*boff += real_len;
	return real_len;
}
struct file_operations fo_swtp_status_value = {
    .read = rfcable_det_status,
};
static int rfcable_det_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct rfcable_det_data *data;
	pr_info("[RFCableDet]detection %s entered\n", __func__);
	data = devm_kzalloc(dev, sizeof(struct rfcable_det_data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "[RFCableDet]NO MEMORY!\n");
		return -ENOMEM;
	}
	data->status_gpio = of_get_named_gpio(np, "target-gpio", 0);
	if (!gpio_is_valid(data->status_gpio)) {
		dev_err(dev, "[RFCableDet]CANNOT FIND GPIO!\n");
		return -EINVAL;
	} else {
		printk("[RFCableDet]Init gpio is %d\n", data->status_gpio);
	}
	ret = devm_gpio_request(dev, data->status_gpio, "lx_rfcable");
	if (ret) {
		dev_err(dev, "[RFCableDet]CANNOT REQUEST GPIO! (ERR %d)\n", ret);
		return -EINVAL;
	}
	data->dev_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR_OR_NULL(data->dev_pinctrl)) {
		struct pinctrl_state *pstate = pinctrl_lookup_state(data->dev_pinctrl, "rf_luxcable_det");
		if (!IS_ERR_OR_NULL(pstate)) {
			ret = pinctrl_select_state(data->dev_pinctrl, pstate);
			if (ret < 0) {
				dev_err(dev, "[RFCableDet]CANNOT SELECT PIN STATE! (ERR %d)\n", ret);
			}
		} else {
			dev_err(dev, "[RFCableDet]CANNOT LOOKUP PINCTRL STATE!\n");
			ret = -EFAULT;
		}
	} else {
		dev_err(dev, "[RFCableDet]CANNOT GET PINCTRL INSTANCE!\n");
		return -EINVAL;
	}
	data->dev = dev;
	if (!proc_create_data("swtp_status_value", S_IRUGO, NULL, (const struct proc_ops *)&fo_swtp_status_value, data)) {
		dev_err(dev, "[RFCableDet]CANNOT CREATE NODE!\n");
		ret = -ENOSPC;
	}
	if (ret < 0) {
		devm_pinctrl_put(data->dev_pinctrl);
		return -EINVAL;
	}
	platform_set_drvdata(pdev, data);
	pr_info("[RFCableDet]initialization succeed.");
	return ret;
}
static int rfcable_det_remove(struct platform_device *pdev)
{
	struct rfcable_det_data *data = dev_get_drvdata(&pdev->dev);
	if (data && !IS_ERR_OR_NULL(data->dev_pinctrl)) {
		remove_proc_subtree("swtp_status_value", NULL);
		devm_pinctrl_put(data->dev_pinctrl);
	}
	return 0;
}
static const struct of_device_id rfcable_det_of_match[] = {
	{ .compatible = "lxcom,rfcable-detect", },
	{},
};
static struct platform_driver radio_cable_det_driver = {
	.driver = {
		.name = "rfcable-detect",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rfcable_det_of_match),
	},
	.probe = rfcable_det_probe,
	.remove = rfcable_det_remove,
};
module_platform_driver(radio_cable_det_driver);
MODULE_AUTHOR("p-yanghaopeng<Haopeng.Yang@luxshare-ict.com>");
MODULE_DESCRIPTION("LXCOM radio cable detection");
MODULE_LICENSE("GPL");
