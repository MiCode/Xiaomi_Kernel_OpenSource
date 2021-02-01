#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/mdss_io_util.h>
#include <sound/fm_lan.h>

//#define wfj_debug printk("fm-inside-lan  %s:%d\n",__func__,__LINE__)
struct vreg_config {
	char *name;
	unsigned long vmin;
	unsigned long vmax;
	int ua_load;
};

struct fm_data {
	struct device *dev;
	struct mutex lock;
	struct regulator *pwr_vdd;  /* ldio21 2.7V */
};
static struct fm_data *fm;

static int32_t fm_get_regulator(bool get)
{
	int32_t ret = 0;

	pr_err("%s: get/put regulator : %d \n", __func__, get);
	if (!get) {
		goto put_regulator;
	}

	fm->pwr_vdd = regulator_get(fm->dev, "vdd_io");
	if (IS_ERR_OR_NULL(fm->pwr_vdd)) {
		ret = PTR_ERR(fm->pwr_vdd);
		pr_err("%s: Failed to get vdd regulator",__func__);
		goto put_regulator;
	} else {
        if (regulator_count_voltages(fm->pwr_vdd) > 0) {
            ret = regulator_set_voltage(fm->pwr_vdd, 2700000, 2700000);
            if (ret) {
                pr_err("%s: vddio regulator set_vtg failed,ret=%d", __func__, ret);
                goto put_regulator;
            }
        }
	}

	return 0;

put_regulator:
	if (fm->pwr_vdd) {
		regulator_put(fm->pwr_vdd);
		fm->pwr_vdd = NULL;
	}
	return ret;
}

static int32_t fm_enable_regulator(bool en)
{
	static bool status = false;
	int32_t ret = 0;

	if (status == en) {
		pr_err("%s: Already %s fm regulator", __func__, en?"enable":"disable");
		return 0;
	}
	status = en;
	pr_err("%s: %s fm regulator", __func__, en?"enable":"disable");

	if (!en) {
		goto disable_vdd_regulator;
	}

	if (fm->pwr_vdd) {
		ret = regulator_enable(fm->pwr_vdd);
		if (ret < 0) {
			pr_err("%s: Failed to enable vdd regulator",__func__);
			goto exit;
		}
	}

	return 0;

disable_vdd_regulator:
	if (fm->pwr_vdd)
		regulator_disable(fm->pwr_vdd);

exit:
	return ret;
}

void fm_lan_power_set(bool status)
{
	if(status){
   		fm_enable_regulator(true);
		pr_err("%s: fm lan power enable\n", __func__);
   	}else{
   		fm_enable_regulator(false);
		pr_err("%s: fm lan power disable\n", __func__);
	}

   return;
}EXPORT_SYMBOL(fm_lan_power_set);

static int fm_power_probe(struct platform_device *pdev)
{
	int ret = 0;

    dev_err(&pdev->dev,"probe fm_inside_lan driver");
	fm = kzalloc(sizeof(struct fm_data),GFP_KERNEL);
    if (!fm) {
		dev_err(&pdev->dev,"failed to allocate memory for struct fm_data\n");
		ret = -ENOMEM;
		goto free_pdata;
	}

   fm->dev = &pdev->dev;
   platform_set_drvdata(pdev, fm);

   ret = fm_get_regulator(true);
	if (ret < 0) {
		pr_err("%s: Failed to get register \n",__func__);
		goto err_get_regulator;
	}

	ret = fm_enable_regulator(false);
	if(ret < 0){
		pr_err("%s: Failed to enable regulator \n",__func__);
		goto err_enable_regulator;
	}

	return 0;

err_enable_regulator:
	fm_enable_regulator(false);

err_get_regulator:
	fm_get_regulator(false);

free_pdata:
	kfree(fm);
    return ret;
}

static int fm_power_remove(struct platform_device *pdev)
{
	fm_enable_regulator(false);
	fm_get_regulator(false);
	platform_set_drvdata(pdev, NULL);

	if (fm) {
		kfree(fm);
		fm = NULL;
	}

	dev_info(&pdev->dev, "%s\n", __func__);
	return 0;
}

static struct of_device_id fm_of_match[] = {
	{ .compatible = "qcom,fm_inside_lan", },
	{}
};
MODULE_DEVICE_TABLE(of, fm_of_match);

static struct platform_driver fm_driver = {
	.driver = {
		.name	= "fm_inside_lan",
		.owner	= THIS_MODULE,
		.of_match_table = fm_of_match,
	},
	.probe	= fm_power_probe,
	.remove	= fm_power_remove,
};

static int __init fm_init(void)
{
	int rc = platform_driver_register(&fm_driver);

	if (!rc)
		pr_info("%s: fm lan init OK\n", __func__);
	else
		pr_err("%s: fm lan init fail :%d\n", __func__, rc);

	return rc;
}

static void __exit fm_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&fm_driver);
}

module_init(fm_init);
module_exit(fm_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("wangfajie <wangfajie@longcheer.com>");
MODULE_DESCRIPTION("fm-inside-lan driver.");
