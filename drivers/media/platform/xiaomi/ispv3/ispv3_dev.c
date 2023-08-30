/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/aer.h>
#include <linux/prefetch.h>
#include <linux/msi.h>
#include <linux/mfd/core.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include "ispv3_cam_dev.h"

#define DRIVER_NAME "xiaomi_ispv3"
static struct ispv3_data *ispv3_dev_info;

static struct resource ispv3_rpmsg_res[RPMSG_RES_NUM];
static struct resource ispv3_cam_res[CAM_RES_NUM];

static struct mfd_cell ispv3_v4l2_cell = {
	.name = "ispv3-v4l2",
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv3_rpmsg_pci_cell = {
	.name = "ispv3-rpmsg_pci",
	.num_resources = ARRAY_SIZE(ispv3_rpmsg_res),
	.resources = ispv3_rpmsg_res,
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv3_rpmsg_spi_cell = {
	.name = "ispv3-rpmsg_spi",
	.num_resources = ARRAY_SIZE(ispv3_rpmsg_res),
	.resources = ispv3_rpmsg_res,
	.ignore_resource_conflicts = true,
};

static struct mfd_cell ispv3_cam_cell = {
	.name = "ispv3-cam",
	.num_resources = ARRAY_SIZE(ispv3_cam_res),
	.resources = ispv3_cam_res,
	.ignore_resource_conflicts = true,
};

static struct ispv3_data *ispv3_get_plat_priv(void);
static void ispv3_set_plat_priv(struct ispv3_data *plat_priv);

static pci_ers_result_t ispv3_io_error_detected(struct pci_dev *pdev,
						pci_channel_state_t state)
{
	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	pci_disable_device(pdev);

	/* Request a slot reset. */
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t ispv3_io_slot_reset(struct pci_dev *pdev)
{
	pci_ers_result_t result;
	int err;

	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev,
			"Cannot re-enable PCI device after reset.\n");
		result = PCI_ERS_RESULT_DISCONNECT;
	} else {
		pdev->state_saved = true;
		pci_restore_state(pdev);
		pci_set_master(pdev);

		result = PCI_ERS_RESULT_RECOVERED;
	}

	return result;
}

static const struct pci_error_handlers ispv3_err_handler = {
	.error_detected = ispv3_io_error_detected,
	.slot_reset = ispv3_io_slot_reset,
};

static int ispv3_init_rpmsg_callback(struct ispv3_data *pdata)
{
	int ret = 0;
	struct device *pdev = pdata->dev;

	ret = mfd_add_devices(pdev, PLATFORM_DEVID_NONE,
			&ispv3_rpmsg_spi_cell, 1, NULL, 0, NULL);
	if (ret) {
		dev_err(pdev, "MFD add rpmsg devices failed: %d\n", ret);
		return ret;
	}
	dev_info(pdev, "ispv3 rpmsg device registered.\n");

	return 0;
}

static int ispv3_init_v4l2(struct pci_dev *pdev)
{
	int ret = 0;

	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			&ispv3_v4l2_cell, 1, NULL, 0, NULL);
	if (ret) {
		dev_err(&pdev->dev, "MFD add v4l2 devices failed: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv3 v4l2 device registered.\n");

	return 0;
}

static int ispv3_init_rpmsg(struct ispv3_data *pdata)
{
	int ret = 0, idx = 0;
        struct pci_dev *pdev = pdata->pci;

	for (idx = 0; idx < ARRAY_SIZE(ispv3_rpmsg_res); idx++) {
		switch (idx) {
		case RPMSG_RAM:
			ispv3_rpmsg_res[idx].flags = IORESOURCE_MEM;
			ispv3_rpmsg_res[idx].start = pci_resource_start(pdev, BAR_2) +
						     OCRAM_OFFSET;
			ispv3_rpmsg_res[idx].end = pci_resource_start(pdev, BAR_2) +
						   OCRAM_OFFSET + RPMSG_SIZE - 1;
			break;
		case RPMSG_IRQ:
			ispv3_rpmsg_res[idx].flags = IORESOURCE_IRQ;
			ispv3_rpmsg_res[idx].start = pdev->irq;
			ispv3_rpmsg_res[idx].end = pdev->irq;
			break;

		default:
			dev_err(&pdev->dev, "Invalid parameter!\n");
			break;
		}
	}

	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			&ispv3_rpmsg_pci_cell, 1, NULL, 0, NULL);
	if (ret) {
		dev_err(&pdev->dev, "MFD add rpmsg devices failed: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv3 rpmsg device registered.\n");

	return 0;
}

static int ispv3_init_camera(struct pci_dev *pdev, struct ispv3_data *priv)
{
	int ret = 0, idx = 0;

	for (idx = 0; idx < ARRAY_SIZE(ispv3_cam_res); idx++) {
		switch (idx) {
		case ISP_RAM:
			ispv3_cam_res[idx].flags = IORESOURCE_MEM;
			ispv3_cam_res[idx].start = pci_resource_start(pdev, BAR_2) +
						   OCRAM_OFFSET + RPMSG_SIZE;
			ispv3_cam_res[idx].end = pci_resource_end(pdev, BAR_2);
			break;
		case ISP_DDR:
			ispv3_cam_res[idx].flags = IORESOURCE_MEM;
			ispv3_cam_res[idx].start = pci_resource_start(pdev, BAR_4);
			ispv3_cam_res[idx].end = pci_resource_end(pdev, BAR_4);
			break;
		default:
			dev_err(&pdev->dev, "Invalid parameter!\n");
			break;
		}
	}

	ispv3_cam_cell.platform_data = (void *)priv;
	ret = mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
			&ispv3_cam_cell, 1, NULL, 0, NULL);
	if (ret) {
		dev_err(&pdev->dev, "MFD add camera device failed: %d\n", ret);
		return ret;
	}
	dev_info(&pdev->dev, "ispv3 cam device registered.\n");

	return 0;
}

static void ispv3_init_bar_resource(struct pci_dev *pdev, struct ispv3_data *priv)
{
	int idx = 0;
	int bar_idx = 0;
	struct resource *res = priv->bar_res;

	for (idx = 0; idx < ISPV3_BAR_NUM; idx++) {

		res->flags = IORESOURCE_MEM;
		res->start = pci_resource_start(pdev, bar_idx);
		res->end = pci_resource_end(pdev, bar_idx);

		bar_idx += 2;
		res++;
	}
}

static bool ispv3_alloc_irq_vectors(struct pci_dev *pdev, struct ispv3_data *priv)
{
	struct device *dev = &pdev->dev;
	bool res = true;
	int irq = -1;

	switch (priv->pci_irq_type) {
	case IRQ_TYPE_LEGACY:
		irq = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_LEGACY);
		if (irq < 0)
			dev_err(dev, "Failed to get Legacy interrupt!\n");
		break;
	case IRQ_TYPE_MSI:
		irq = pci_alloc_irq_vectors(pdev, 1, 3, PCI_IRQ_MSI);
		if (irq < 0)
			dev_err(dev, "Failed to get MSI interrupts!\n");
		break;
	default:
		dev_err(dev, "Invalid IRQ type selected.\n");
		break;
	}

	if (irq < 0) {
		irq = 0;
		res = false;
	}

	return res;
}

static void ispv3_free_irq_vectors(struct pci_dev *pdev)
{
	pci_free_irq_vectors(pdev);
}

static int ispv3_pci_probe(struct pci_dev *pdev,
	const struct pci_device_id *id)
{
	int idx, ret;
	struct ispv3_data *priv  = ispv3_get_plat_priv();

	if (!priv)
		return -ENOMEM;

	for (idx = PCI_STD_RESOURCES; idx < (PCI_STD_RESOURCE_END + 1); idx++) {
		ret = pci_assign_resource(pdev, idx);
		if (ret)
			dev_err(&pdev->dev, "assign pci resource failed!\n");

		if (pci_resource_flags(pdev, idx) & IORESOURCE_MEM_64)
			idx = idx + 1;
	}

	ret = pci_enable_device(pdev);
	if (ret) {
		dev_err(&pdev->dev, "enable pci device failed: %d\n", ret);
		return ret;
	}

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev,
				"DMA configuration failed: 0x%x\n", ret);
		goto err_disable;
	}

	/* set up pci connections */
	ret = pci_request_selected_regions(pdev, ((1 << 1) - 1), DRIVER_NAME);
	if (ret) {
		dev_err(&pdev->dev,
			 "pci_request_selected_regions failed %d\n", ret);
		goto err_disable;
	}

	if (pci_resource_flags(pdev, BAR_0) & IORESOURCE_MEM) {
		priv->base = pci_ioremap_bar(pdev, BAR_0);

		if (!priv->base) {
			dev_err(&pdev->dev, "Failed to map the register of BAR0!\n");
			goto err_release_region;
		}
	}

	pci_enable_pcie_error_reporting(pdev);
	pci_set_master(pdev);

	priv->pci = pdev;
	pci_save_state(pdev);
	priv->default_state = pci_store_saved_state(pdev);
	pci_set_drvdata(pdev, priv);

	atomic_set(&priv->pci_link_state, ISPV3_PCI_LINK_UP);

	priv->pci_irq_type = IRQ_TYPE_MSI;
	if (!ispv3_alloc_irq_vectors(pdev, priv))
		goto err_disable;

	ispv3_init_bar_resource(pdev, priv);

	priv->remote_callback = ispv3_init_rpmsg;

	ret = ispv3_init_camera(pdev, priv);
	if (ret) {
		dev_err(&pdev->dev, "init cam mfd failed: %d\n", ret);
		goto err_disable_irq;
	}

	ret = ispv3_init_v4l2(pdev);
	if (ret) {
		dev_err(&pdev->dev, "init v4l2 mfd failed: %d\n", ret);
		goto err_disable_irq;
	}
	dev_info(&pdev->dev, "ispv3 all ispv3 mfd devices registered.\n");

	return 0;

err_disable_irq:
	ispv3_free_irq_vectors(pdev);

err_release_region:
	pci_release_selected_regions(pdev, ((1 << 1) - 1));

err_disable:
	pci_disable_device(pdev);

	return ret;

}

static void ispv3_pci_remove(struct pci_dev *pdev)
{
	struct ispv3_data *priv  = ispv3_get_plat_priv();

	if (priv->saved_state)
		pci_load_and_free_saved_state(pdev, &priv->saved_state);
	pci_load_and_free_saved_state(pdev, &priv->default_state);
	mfd_remove_devices(&pdev->dev);
	pci_disable_pcie_error_reporting(pdev);
	pci_release_selected_regions(pdev, ((1 << 1) - 1));
	pci_disable_device(pdev);
}

static void ispv3_shutdown(struct pci_dev *pdev)
{
	pci_disable_device(pdev);
}

static const struct pci_device_id ispv3_pci_tbl[] = {
	{ PCI_DEVICE(ISPV3_PCI_VENDOR_ID, ISPV3_PCI_DEVICE_ID) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, ispv3_pci_tbl);

static struct pci_driver ispv3_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = ispv3_pci_tbl,
	.probe = ispv3_pci_probe,
	.remove = ispv3_pci_remove,
	.shutdown = ispv3_shutdown,
	.err_handler = &ispv3_err_handler,

};

int ispv3_pci_init(struct ispv3_data *data)
{
	int ret = 0;

	/* enumerate it on PCIE */
	ret = msm_pcie_enumerate(data->rc_index);
	if (ret < 0) {
		dev_err(data->dev, "ispv3 PCIE enumeration failed! Not PCIE mode\n");
		return ret;
	}

	data->interface_type = ISPV3_PCIE;

	ret = pci_register_driver(&ispv3_pci_driver);
	if (ret) {
		dev_err(data->dev, "Failed to register PCIe driver!\n");
		goto out;
	}
out:
	return ret;
}

void ispv3_pci_deinit(u32 rc_index)
{
	pci_unregister_driver(&ispv3_pci_driver);
}

static uint32_t RGLTR_COUNT[2] = {
	ISPV3_SOC_8450_RGLTR_COUNT,
	ISPV3_SOC_8475_RGLTR_COUNT,
};

static int ispv3_get_dt_regulator_info(struct ispv3_data *data)
{
	int ret = 0, count = 0, i = 0;
	struct device_node *of_node = data->dev->of_node;

	if (!data || !data->dev) {
		dev_err(data->dev, "Invalid parameters!\n");
		return -EINVAL;
	}

	data->num_rgltr = 0;
	count = of_property_count_strings(of_node, "regulator-names");
	if (count != RGLTR_COUNT[data->soc_id]) {
		dev_err(data->dev, "regulators num error!\n");
		return -EINVAL;
	}
	data->num_rgltr = count;

	for (i = 0; i < data->num_rgltr; i++) {
		ret = of_property_read_string_index(of_node,
			"regulator-names", i, &data->rgltr_name[i]);
		dev_dbg(data->dev, "rgltr_name[%d] = %s\n",
			i, data->rgltr_name[i]);
		if (ret) {
			dev_err(data->dev, "no regulator resource at cnt=%d\n",
				i);
			return -ENODEV;
		}
	}

	ret = of_property_read_u32_array(of_node, "rgltr-min-voltage",
					 data->rgltr_min_volt, data->num_rgltr);
	if (ret) {
		dev_err(data->dev, "No min volatage value found, ret=%d\n", ret);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(of_node, "rgltr-max-voltage",
					 data->rgltr_max_volt, data->num_rgltr);
	if (ret) {
		dev_err(data->dev, "No max volatage value found, ret=%d\n", ret);
		return -EINVAL;
	}

	for (i = 0; i < data->num_rgltr; i++) {
		data->rgltr[i] = regulator_get(data->dev, data->rgltr_name[i]);
		if (IS_ERR_OR_NULL(data->rgltr[i])) {
			dev_err(data->dev, "Regulator %s get failed!\n",
				data->rgltr_name[i]);
			return -EINVAL;
		}
	}

	return ret;
}

static int ispv3_get_dt_clk_info(struct ispv3_data *data)
{
	int ret = 0;
	int count;
	int num_clk_rates;
	struct device_node *of_node = data->dev->of_node;

	if (!data || !data->dev) {
		dev_err(data->dev, "Invalid parameters!\n");
		return -EINVAL;
	}

	count = of_property_count_strings(of_node, "clock-names");
	if (count != ISPV3_CLK_NUM) {
		dev_err(data->dev, "invalid count of clocks, count=%d\n",
			count);
		ret = -EINVAL;
		return ret;
	}

	ret = of_property_read_string(of_node, "clock-names", &data->clk_name);
	if (ret) {
		dev_err(data->dev, "reading clock-names failed!\n");
		return ret;
	}

	num_clk_rates = of_property_count_u32_elems(of_node, "clock-rates");
	if (num_clk_rates <= 0) {
		dev_err(data->dev, "reading clock-rates count failed!\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(of_node, "clock-rates",
		0, &data->clk_rate);
	if (ret) {
		dev_err(data->dev, "Error reading clock-rates, ret=%d\n", ret);
		return ret;
	}

	data->clk_rate = (data->clk_rate == 0) ? (int32_t)ISPV3_NO_SET_RATE : data->clk_rate;
	dev_dbg(data->dev, "mclk_rate = %d", data->clk_rate);

	data->clk = devm_clk_get(data->dev, data->clk_name);
	if (!data->clk) {
		dev_err(data->dev, "get clk failed for %s\n", data->clk_name);
		ret = -ENOENT;
		return ret;
	}

	return ret;
}

static int ispv3_pinctrl_init(struct ispv3_pinctrl_info *sensor_pctrl,
			      struct device *dev)
{
	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		dev_err(dev, "Getting pinctrl handle failed!\n");
		return -EINVAL;
	}

	sensor_pctrl->gpio_state_active =
			pinctrl_lookup_state(sensor_pctrl->pinctrl,
			ISPV3_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_active)) {
		dev_err(dev, "Failed to get the active state pinctrl handle!\n");
		return -EINVAL;
	}

	sensor_pctrl->gpio_state_suspend =
			pinctrl_lookup_state(sensor_pctrl->pinctrl,
			ISPV3_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_suspend)) {
		dev_err(dev, "Failed to get the suspend state pinctrl handle!\n");
		return -EINVAL;
	}

	return 0;
}

static int ispv3_conf_gpio(struct ispv3_data *data)
{
	int ret = 0;
	struct device_node *np = data->dev->of_node;

	data->gpio_sys_reset = of_get_named_gpio(np, "ispv3_gpio_reset", 0);
	if (data->gpio_sys_reset < 0) {
		dev_err(data->dev, "get reset gpio failed!\n");
		return -EINVAL;
	}

	ret = devm_gpio_request(data->dev, data->gpio_sys_reset, "reset_gpio0");
	if (ret) {
		dev_err(data->dev, "reset gpio request failed!\n");
		return ret;
	}

	ret = gpio_direction_output(data->gpio_sys_reset, 0);
	if (ret) {
		dev_err(data->dev, "cannot set direction for reset gpio[%d]\n",
			data->gpio_sys_reset);
		return ret;
	}

	data->gpio_isolation = of_get_named_gpio(np, "ispv3_gpio_15", 0);
	if (data->gpio_isolation < 0) {
		dev_err(data->dev, "get isolation gpio failed\n");
		return -EINVAL;
	}

	ret = devm_gpio_request(data->dev, data->gpio_isolation, "isolation_gpio");
	if (ret) {
		dev_err(data->dev, "request isolation gpio failed\n");
		return ret;
	}

	ret = gpio_direction_output(data->gpio_isolation, 0);
	if (ret) {
		dev_err(data->dev, "cannot set direction for isolation gpio[%d]\n",
			data->gpio_isolation);
		return ret;
	}

	data->gpio_swcr_reset = of_get_named_gpio(np, "ispv3_gpio_14", 0);
	if (data->gpio_swcr_reset < 0) {
		dev_err(data->dev, "get reset1 gpio failed\n");
		return -EINVAL;
	}

	ret = devm_gpio_request(data->dev, data->gpio_swcr_reset, "reset_gpio1");
	if (ret) {
		dev_err(data->dev, "request reset1 gpio failed\n");
		return ret;
	}

	ret = gpio_direction_output(data->gpio_swcr_reset, 0);
	if (ret) {
		dev_err(data->dev, "cannot set direction for reset1 gpio[%d]\n",
			data->gpio_swcr_reset);
		return ret;
	}

	data->gpio_fan_en = of_get_named_gpio(np, "ispv3_gpio_fan_en", 0);
	if (data->gpio_fan_en < 0) {
		dev_err(data->dev, "get fan53526 enable gpio failed\n");
		return -EINVAL;
	}

	ret = devm_gpio_request(data->dev, data->gpio_fan_en,
				"fan_en_gpio");
	if (ret) {
		dev_err(data->dev, "request fan53526 enable gpio failed\n");
		return ret;
	}

	ret = gpio_direction_output(data->gpio_fan_en, 0);
	if (ret) {
		dev_err(data->dev, "cannot set direction for fan53526 enable gpio[%d]\n",
			data->gpio_fan_en);
		return ret;
	}

	data->gpio_int0 = of_get_named_gpio(np, "ispv3_gpio_int0", 0);
	if (data->gpio_int0 < 0) {
		dev_err(data->dev, "get interrupt0 gpio failed\n");
		return -EINVAL;
	}

	data->gpio_irq_cam = gpio_to_irq(data->gpio_int0);

	ret = devm_gpio_request(data->dev, data->gpio_int0, "interrupt_gpio_cam");
	if (ret) {
		dev_err(data->dev, "request interrupt0 gpio failed\n");
		return ret;
	}

	ret = gpio_direction_input(data->gpio_int0);
	if (ret) {
		dev_err(data->dev, "cannot set direction for interrupt0 gpio[%d]\n",
			data->gpio_int0);
		return ret;
	}

	data->gpio_int1 = of_get_named_gpio(np, "ispv3_gpio_int1", 0);
	if (data->gpio_int1 < 0) {
		dev_err(data->dev, "get interrupt1 gpio failed\n");
		return -EINVAL;
	}

	data->gpio_irq_power = gpio_to_irq(data->gpio_int1);

	ret = devm_gpio_request(data->dev, data->gpio_int1, "interrupt_gpio_power");
	if (ret) {
		dev_err(data->dev, "request interrupt1 gpio failed\n");
		return ret;
	}

	ret = gpio_direction_input(data->gpio_int1);
	if (ret) {
		dev_err(data->dev, "cannot set direction for interrupt1 gpio[%d]\n",
			data->gpio_int1);
		return ret;
	}

	return 0;
}

static void ispv3_free_gpio(struct ispv3_data *data)
{
	if (data->gpio_sys_reset)
		devm_gpio_free(data->dev, data->gpio_sys_reset);
	data->gpio_sys_reset = 0;

	if (data->gpio_isolation)
		devm_gpio_free(data->dev, data->gpio_isolation);
	data->gpio_isolation = 0;

	if (data->gpio_swcr_reset)
		devm_gpio_free(data->dev, data->gpio_swcr_reset);
	data->gpio_swcr_reset = 0;

	if (data->gpio_int0)
		devm_gpio_free(data->dev, data->gpio_int0);
	data->gpio_int0 = 0;

	if (data->gpio_int1)
		devm_gpio_free(data->dev, data->gpio_int1);
	data->gpio_int1 = 0;

	if (data->gpio_fan_en)
		devm_gpio_free(data->dev, data->gpio_fan_en);
	data->gpio_fan_en = 0;
}

static int ispv3_get_dt_data(struct ispv3_data *data)
{
	int ret = 0;
	u32 rc_index;
	struct device_node *np = data->dev->of_node;

	if (of_get_property(np, "sm8475", NULL) != NULL)
		data->soc_id = ISPV3_SOC_ID_SM8475;
	else
		data->soc_id = ISPV3_SOC_ID_SM8450;

	dev_info(data->dev, "soc id is %d", data->soc_id);


	ret = of_property_read_u32(np, "qcom,ispv3-rc-index", &rc_index);
	if (ret) {
		dev_err(data->dev, "Failed to find PCIe RC number!\n");
		return -EINVAL;
	}
	data->rc_index = rc_index;

	ret = ispv3_conf_gpio(data);
	if (ret) {
		dev_err(data->dev, "config pin function failed!\n");
		return ret;
	}

	ret = ispv3_get_dt_regulator_info(data);
	if (ret) {
		dev_err(data->dev, "get dt regulator failed!\n");
		return ret;
	}

	ret = ispv3_get_dt_clk_info(data);
	if (ret) {
		dev_err(data->dev, "get dt clk failed!\n");
		return ret;
	}

	memset(&(data->pinctrl_info), 0x0, sizeof(data->pinctrl_info));
	ret = ispv3_pinctrl_init(&(data->pinctrl_info), data->dev);
	if (ret < 0) {
		dev_err(data->dev, "Initialization of pinctrl failed!\n");
		data->pinctrl_info.pinctrl_status = 0;
		return ret;
	}

	data->pinctrl_info.pinctrl_status = 1;

	return ret;
}

static void ispv3_set_plat_priv(struct ispv3_data *plat_priv)
{
	ispv3_dev_info = plat_priv;
}

static struct ispv3_data *ispv3_get_plat_priv(void)
{
	return ispv3_dev_info;
}

static int ispv3_spi_probe(struct spi_device *spi)
{
	struct ispv3_data *data = NULL;
	struct device *dev = &spi->dev;
	int ret = 0;

	spi->max_speed_hz = ISPV3_SPI_SPEED_HZ;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;

	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(dev, "spi_setup failed (%d)!\n", ret);
		goto out;
	}

	data = devm_kzalloc(dev, sizeof(struct ispv3_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto out;
	}

	spi_set_drvdata(spi, data);
	ispv3_set_plat_priv(data);
	data->spi = spi;
	data->dev = dev;

	ret = ispv3_get_dt_data(data);
	if (ret < 0) {
		dev_err(dev, "ispv3_get_dt_data failed (%d)!\n", ret);
		goto free_data;
	}

#ifdef BUG_SOF
	atomic_set(&data->power_state, 1);
#else
	data->power_state = ISPV3_POWER_OFF;
#endif
	data->interface_type = ISPV3_SPI;
	mutex_init(&data->ispv3_interf_mutex);

	ispv3_gpio_reset_clear(data);

	ret = ispv3_power_on(data);
	if (ret < 0) {
		dev_err(dev, "ispv3 power on failed (%d)!\n", ret);
		goto remove_pin;
	}

	ret = ispv3_pci_init(data);
	if (ret) {
		dev_err(dev, "ispv3 pci init failed (%d)! Not PCIE mode\n", ret);
		data->interface_type = ISPV3_SPI;
	}

	if (data->interface_type == ISPV3_SPI) {
		data->remote_callback = ispv3_init_rpmsg_callback;

		ispv3_cam_cell.platform_data = (void *)data;
		ret = mfd_add_devices(dev, PLATFORM_DEVID_NONE,
				&ispv3_cam_cell, 1, NULL, 0, NULL);
		if (ret) {
			dev_err(dev, "MFD add camera device failed: %d\n", ret);
			goto power_off;
		}
		dev_info(dev, "ispv3 cam device registered.\n");
	}

	dev_info(dev, "ispv3 all ispv3 mfd devices registered.\n");

	return 0;

power_off:
	ispv3_power_off(data);
remove_pin:
	ispv3_free_gpio(data);
free_data:
	devm_kfree(dev, data);
	ispv3_set_plat_priv(NULL);
out:
	return ret;
}

static int ispv3_spi_remove(struct spi_device *spi)
{
	struct ispv3_data *data = spi_get_drvdata(spi);
	int ret = 0;

	ispv3_free_gpio(data);
#ifdef CONFIG_ZISP_OCRAM_AON
	ret = ispv3_power_off(data);
#endif
	dev_info(&spi->dev, "ispv3 remove success !\n");

	return ret;
}

static const struct of_device_id ispv3_spi_of_match[] = {
	{.compatible = "xiaomi,ispv3_spi",},
	{},
};

static const struct spi_device_id ispv3_spi_device_id[] = {
	{"ispv3_spi", 0},
	{}
};

static struct spi_driver ispv3_spi_drv = {
	.driver = {
		.name = "ispv3_spi",
		.owner = THIS_MODULE,
		.of_match_table = ispv3_spi_of_match,
	},
	.probe = ispv3_spi_probe,
	.remove = ispv3_spi_remove,
	.id_table = ispv3_spi_device_id,
};

static int __init ispv3_module_init(void)
{
	int ret;

	ret = spi_register_driver(&ispv3_spi_drv);

	return ret;
}

static void __exit ispv3_module_exit(void)
{
	spi_unregister_driver(&ispv3_spi_drv);
}

module_init(ispv3_module_init);
module_exit(ispv3_module_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for Xiaomi, Inc. ZISP V3");
