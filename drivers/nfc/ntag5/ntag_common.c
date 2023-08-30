
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/ioctl.h>

#include "ntag_common.h"

int nfc_parse_dt(struct device *dev, struct platform_configs *nfc_configs)
{
	int ret=0;
	struct device_node *np = dev->of_node;
	struct platform_gpio *nfc_gpio = &nfc_configs->gpio;

	if (!np) {
		pr_err("nfc of_node NULL\n");
		return -EINVAL;
	}

	nfc_gpio->pu = -EINVAL;
	nfc_gpio->hpd = -EINVAL;

	nfc_gpio->pu = of_get_named_gpio(np, DTS_IRQ_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->pu))) {
		pr_err("nfc irq gpio invalid %d\n", nfc_gpio->pu);
		return -EINVAL;
	}
	pr_info("%s: irq %d\n", __func__, nfc_gpio->pu);

	nfc_gpio->hpd = of_get_named_gpio(np, DTS_HPD_GPIO_STR, 0);
	if ((!gpio_is_valid(nfc_gpio->hpd))) {
		pr_err("nfc ven gpio invalid %d\n", nfc_gpio->hpd);
		return -EINVAL;
	}

	pr_info("%s: hpd %d\n", __func__, nfc_gpio->hpd);

	return ret;
}


int configure_gpio(unsigned int gpio, int flag)
{
	int ret = 0;

	pr_info("%s: nfc gpio [%d] flag [%01x]\n", __func__, gpio, flag);

	if (gpio_is_valid(gpio)) {
		ret = gpio_request(gpio, "nfc_gpio");
		if (ret) {
			pr_err("%s: unable to request nfc gpio [%d]\n",  __func__, gpio);
			return ret;
		}

		if (flag == GPIO_HPD) {
			ret = gpio_direction_output(gpio, 0);
			if (ret) {
				pr_err("%s: unable to set direction for nfc gpio [%d]\n", __func__, gpio);
				gpio_free(gpio);
				return ret;
			}

			mdelay(5);
			gpio_set_value(gpio, 1);
			pr_err("%s: unable to set direction for nfc gpio [%d]\n", __func__, gpio_get_value(gpio));


			mdelay(5);
			gpio_set_value(gpio, 0);
			pr_err("%s: unable to set direction for nfc gpio [%d]\n", __func__, gpio_get_value(gpio));
		} else if (flag == GPIO_IRQ) {
			ret = gpio_direction_input(gpio);
			if (ret) {
				pr_err("%s: unable to set direction for nfc gpio [%d]\n", __func__, gpio);
				gpio_free(gpio);
				return ret;
			}

			ret = gpio_to_irq(gpio);
		}
	} else {
		pr_err("%s: invalid gpio\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}

int ntag5_gpio_init(struct ntag_dev *ntag5_dev)
{
	int ret=0;

	ntag5_dev->ntag5_pinctrl = devm_pinctrl_get(ntag5_dev->nfc_device);
	if(IS_ERR_OR_NULL(ntag5_dev->ntag5_pinctrl)) {
		pr_err("%s: No pinctrl config specified\n", __func__);
		ret = PTR_ERR(ntag5_dev->nfc_device);
		return ret;
	}

	// the default ED pinctrl state check
	ntag5_dev->ntag5_pu_default =
		pinctrl_lookup_state(ntag5_dev->ntag5_pinctrl, "ntag5_pu_default");
	if(IS_ERR_OR_NULL(ntag5_dev->ntag5_pu_default)) {
		pr_err("%s: ntag5_pu_default  \n", __func__);
		ret = PTR_ERR(ntag5_dev->ntag5_pu_default);
		return ret;
	}

	// the default HPD pinctrl state check
	ntag5_dev->ntag5_hpd_default =
		pinctrl_lookup_state(ntag5_dev->ntag5_pinctrl, "ntag5_hpd_default");
	if (IS_ERR_OR_NULL(ntag5_dev->ntag5_hpd_default)) {
		pr_err("%s: ntag5_hpd_default  \n", __func__);
		ret = PTR_ERR(ntag5_dev->ntag5_hpd_default);
		return ret;
	}

	// set the ED and HPD to the default state
	ret = pinctrl_select_state(ntag5_dev->ntag5_pinctrl, ntag5_dev->ntag5_pu_default);
	if (ret < 0) {
		pr_err("%s: fail to select pinctrl pu default rc=%d\n", __func__, ret);
		return ret;
	}

	ret = pinctrl_select_state(ntag5_dev->ntag5_pinctrl, ntag5_dev->ntag5_hpd_default);
	if (ret < 0) {
		pr_err("%s: fail to select pinctrl hpd default rc=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

void nfc_misc_unregister(struct ntag_dev *nfc_dev, int count)
{
	pr_debug("%s: entry\n", __func__);
	device_destroy(nfc_dev->nfc_class, nfc_dev->devno);
	cdev_del(&nfc_dev->c_dev);
	class_destroy(nfc_dev->nfc_class);
	unregister_chrdev_region(nfc_dev->devno, count);
}

int nfc_misc_register(struct ntag_dev *nfc_dev,
		      const struct file_operations *nfc_fops, int count,
		      char *devname, char *classname)
{
	int ret = 0;

	ret = alloc_chrdev_region(&nfc_dev->devno, 0, count, devname);
	if (ret < 0) {
		pr_err("%s: failed to alloc chrdev region ret %d\n",
			__func__, ret);
		return ret;
	}

	pr_info("%s: enter\n", "device starting registered!");

	nfc_dev->nfc_class = class_create(THIS_MODULE, classname);
	if (IS_ERR(nfc_dev->nfc_class)) {
		ret = PTR_ERR(nfc_dev->nfc_class);
		pr_err("%s: failed to register device class ret %d\n",
			__func__, ret);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}

	cdev_init(&nfc_dev->c_dev, nfc_fops);

	ret = cdev_add(&nfc_dev->c_dev, nfc_dev->devno, count);
	if (ret < 0) {
		pr_err("%s: failed to add cdev ret %d\n", __func__, ret);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}

	nfc_dev->nfc_device = device_create(nfc_dev->nfc_class, NULL,
					    nfc_dev->devno, nfc_dev, devname);
	if (IS_ERR(nfc_dev->nfc_device)) {
		ret = PTR_ERR(nfc_dev->nfc_device);
		pr_err("%s: failed to create the device ret %d\n",
			__func__, ret);
		cdev_del(&nfc_dev->c_dev);
		class_destroy(nfc_dev->nfc_class);
		unregister_chrdev_region(nfc_dev->devno, count);
		return ret;
	}

	return 0;
}










