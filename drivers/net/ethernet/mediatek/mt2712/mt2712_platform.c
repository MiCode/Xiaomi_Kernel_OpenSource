/* Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "mt2712_yheader.h"
#include "mt2712_platform.h"
#include "mt2712_yregacc.h"
#include "mt2712_drv.h"

unsigned long dwc_eth_qos_platform_base_addr;

void init_all_fptrs(struct prv_data *pdata)
{
	init_function_ptrs_dev(&pdata->hw_if);
	init_function_ptrs_desc(&pdata->desc_if);
}

void init_rx_coalesce(struct prv_data *pdata)
{
	struct rx_wrapper_descriptor *rx_desc_data = NULL;
	unsigned int i;

	for (i = 0; i < RX_QUEUE_CNT; i++) {
		rx_desc_data = GET_RX_WRAPPER_DESC(i);

		rx_desc_data->use_riwt = 1;
		rx_desc_data->rx_coal_frames = RX_MAX_FRAMES;
		rx_desc_data->rx_riwt =
			usec2riwt(OPTIMAL_DMA_RIWT_USEC, pdata);
	}
}

static int get_clk(struct platform_device *pdev, struct prv_data *pdata)
{
	int ret = 0;

	pdata->peri_axi = devm_clk_get(&pdev->dev, "axi");
	if (IS_ERR(pdata->peri_axi)) {
		ret = PTR_ERR(pdata->peri_axi);
		dev_err(&pdev->dev, "failed to get peri_axi_clk: %d\n", ret);
	}

	pdata->peri_apb = devm_clk_get(&pdev->dev, "apb");
	if (IS_ERR(pdata->peri_apb)) {
		ret = PTR_ERR(pdata->peri_apb);
		dev_err(&pdev->dev, "failed to get peri_apb_clk: %d\n", ret);
	}

	pdata->ext_125m_clk = devm_clk_get(&pdev->dev, "mac_ext");
	if (IS_ERR(pdata->ext_125m_clk)) {
		ret = PTR_ERR(pdata->ext_125m_clk);
		dev_err(&pdev->dev, "failed to get ext_125m_clk: %d\n", ret);
	}

	pdata->ptp_clk = devm_clk_get(&pdev->dev, "ptp");
	if (IS_ERR(pdata->ptp_clk)) {
		ret = PTR_ERR(pdata->ptp_clk);
		dev_err(&pdev->dev, "failed to get ptp_clk: %d\n", ret);
	}

	pdata->ptp_parent_clk = devm_clk_get(&pdev->dev, "ptp_parent");
	if (IS_ERR(pdata->ptp_parent_clk)) {
		ret = PTR_ERR(pdata->ptp_parent_clk);
		dev_err(&pdev->dev, "failed to get ptp_parent_clk-clk: %d\n", ret);
	}

	return ret;
}

static int close_clk(struct prv_data *pdata)
{
	clk_disable_unprepare(pdata->ptp_parent_clk);

	clk_disable_unprepare(pdata->ptp_clk);

	clk_disable_unprepare(pdata->ext_125m_clk);

	clk_disable_unprepare(pdata->peri_apb);

	clk_disable_unprepare(pdata->peri_axi);

	return 0;
}

static int enable_clk(struct platform_device *pdev, struct prv_data *pdata)
{
	int ret;

	ret = clk_prepare_enable(pdata->peri_axi);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable peri_axi_clk (%d)\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(pdata->peri_apb);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable peri_apb_clk (%d)\n", ret);
		goto err_apb_enable;
	}

	ret = clk_prepare_enable(pdata->ext_125m_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable ext_125m_clk (%d)\n", ret);
		goto err_ext_125m_clk_enable;
	}

	ret = clk_prepare_enable(pdata->ptp_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable ptp_clk (%d)\n", ret);
		goto err_ptp_enable;
	}

	ret = clk_prepare_enable(pdata->ptp_parent_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable ptp_parent_clk (%d)\n", ret);
		goto err_ptp_parent_enable;
	}

	ret = clk_set_parent(pdata->ptp_clk, pdata->ptp_parent_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to set ptp parent clk (%d)\n", ret);
		goto err_ptp_parent_clk_set;
	}

	return 0;

err_ptp_parent_clk_set:
	clk_disable_unprepare(pdata->ptp_parent_clk);

err_ptp_parent_enable:
	clk_disable_unprepare(pdata->ptp_clk);

err_ptp_enable:
	clk_disable_unprepare(pdata->ext_125m_clk);

err_ext_125m_clk_enable:
	clk_disable_unprepare(pdata->peri_apb);

err_apb_enable:
	clk_disable_unprepare(pdata->peri_axi);

	return ret;
}

static int probe(struct platform_device *pdev)
{
	struct prv_data *pdata = NULL;
	struct device *d = &pdev->dev;
	struct resource *res;
	const char *mac_addr = NULL;
	struct net_device *dev = NULL;
	int i = 0, irq, ret = 0;
	struct hw_if_struct *hw_if = NULL;
	struct desc_if_struct *desc_if = NULL;
	unsigned char tx_q_count = 0, rx_q_count = 0;
	struct device_node *np = pdev->dev.of_node;
	unsigned long phy_intf_sel_addr;

	if (!np) {
		dev_err(&pdev->dev, "this driver is required to be instantiated from device tree\n");
		return -EINVAL;
	}

	phy_intf_sel_addr = (unsigned long)ioremap_nocache(0x10003000, 32);
	if (!(void __iomem *)phy_intf_sel_addr) {
		dev_err(&pdev->dev, "cannot map register memory, aborting");
		ret = -EIO;
		goto err_out_map_failed;
	}

	iowrite32(0x1, (void *)ioremap_nocache(0x10001010, 32));

	switch (of_get_phy_mode(np)) {
	case PHY_INTERFACE_MODE_MII:
	case PHY_INTERFACE_MODE_GMII:
		iowrite32(0x0, (void *)(phy_intf_sel_addr + 0x418));
		break;
	case PHY_INTERFACE_MODE_RMII:
		iowrite32(0x14, (void *)(phy_intf_sel_addr + 0x418));	/* bit[5:4] = 1 select rxc */
		break;
	case PHY_INTERFACE_MODE_RGMII:
		iowrite32(0x1, (void *)(phy_intf_sel_addr + 0x418));
		iowrite32(0x24, (void *)(phy_intf_sel_addr + 0x428)); /* TX Timing */
		break;
	default:
		dev_err(&pdev->dev, "phy interface not set\n");
		break;
	}

	/* Get memory resource */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dwc_eth_qos_platform_base_addr = (unsigned long)devm_ioremap_resource(d, res);
	if (IS_ERR((void *)dwc_eth_qos_platform_base_addr)) {
		dev_err(&pdev->dev, "cannot map register memory, aborting");
		ret = PTR_ERR((void *)dwc_eth_qos_platform_base_addr);
		goto err_out_map_failed;
	}

	/* queue count */
	tx_q_count = get_tx_queue_count();
	rx_q_count = get_rx_queue_count();

	dev = alloc_etherdev_mqs(sizeof(struct prv_data), tx_q_count, rx_q_count);
	if (!dev) {
		dev_err(&pdev->dev, "Unable to alloc new net device\n");
		ret = -ENOMEM;
		goto err_out_dev_failed;
	}

	/* Get MAC address if available (DT) */
	mac_addr = of_get_mac_address(np);
	if (mac_addr) {
		dev_info(&pdev->dev, "mac addr is %pM\n", mac_addr);
		ether_addr_copy(dev->dev_addr, mac_addr);
	} else {
		dev_warn(&pdev->dev, "no valid MAC address supplied, using a random one\n");
		eth_hw_addr_random(dev);
	}
	dev->base_addr = dwc_eth_qos_platform_base_addr;

	SET_NETDEV_DEV(dev, &pdev->dev);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(33);/* call dma_set_mask */
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	pdata = netdev_priv(dev);

	init_all_fptrs(pdata);
	hw_if = &pdata->hw_if;
	desc_if = &pdata->desc_if;

	platform_set_drvdata(pdev, dev);
	pdata->pdev = pdev;

	pdata->dev = dev;
	pdata->tx_queue_cnt = tx_q_count;
	pdata->rx_queue_cnt = rx_q_count;

	/* issue software reset to device */
	hw_if->exit();

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "Unable to get irq\n");
		ret = irq;
		goto err_out_q_alloc_failed;
	}

	dev->irq = irq;
	dev_info(&pdev->dev, "irq number is %d\n", irq);

	ret = get_clk(pdev, pdata);
	if (ret < 0) {
		goto err_alloc_queue;
	}

	ret = enable_clk(pdev, pdata);
	if (ret < 0) {
		goto err_alloc_queue;
	}

	get_all_hw_features(pdata);
	print_all_hw_features(pdata);

	ret = desc_if->alloc_queue_struct(pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to alloc Tx/Rx queue\n");
		goto err_alloc_queue;
	}

	dev->netdev_ops = get_netdev_ops();

	pdata->interface = get_phy_interface(pdata);

	ret = mdio_register(dev);
	while (ret == -ENOLINK) {
		ret = mdio_register(dev);
		usleep_range(1000, 2000);
		i++;
		if (i > 10)
			goto err_out_mdio_reg;
	}
	if (ret < 0) {
		dev_err(&pdev->dev, "MDIO bus (id %d) registration failed\n", pdata->bus_id);
		goto err_out_mdio_reg;
	}

	for (i = 0; i < RX_QUEUE_CNT; i++) {
		struct rx_queue *rx_queue = GET_RX_QUEUE_PTR(i);

		netif_napi_add(dev, &rx_queue->napi, poll_mq, (64 * RX_QUEUE_CNT));
	}

	dev->ethtool_ops = get_ethtool_ops();

	init_rx_coalesce(pdata);

	ptp_init(pdata);

	spin_lock_init(&pdata->lock);
	spin_lock_init(&pdata->tx_lock);

	ret = register_netdev(dev);
	if (ret) {
		dev_err(&pdev->dev, "Net device registration failed\n");
		goto err_out_netdev_failed;
	}

	return 0;

 err_out_netdev_failed:
	ptp_remove(pdata);
	mdio_unregister(dev);

 err_out_mdio_reg:
	desc_if->free_queue_struct(pdata);

 err_alloc_queue:
	close_clk(pdata);

 err_out_q_alloc_failed:
	free_netdev(dev);
	platform_set_drvdata(pdev, NULL);

 err_out_dev_failed:
 err_out_map_failed:
	return ret;
}

static int remove(struct platform_device *pdev)
{
	struct net_device *dev = platform_get_drvdata(pdev);
	struct prv_data *pdata = netdev_priv(dev);
	struct desc_if_struct *desc_if = &pdata->desc_if;

	if (pdata->irq_number != 0) {
		free_irq(pdata->irq_number, pdata);
		pdata->irq_number = 0;
	}

	mdio_unregister(dev);

	ptp_remove(pdata);

	unregister_netdev(dev);

	desc_if->free_queue_struct(pdata);

	free_netdev(dev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *net_dev = platform_get_drvdata(pdev);
	struct prv_data *pdata = netdev_priv(net_dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	unsigned long flags;
	unsigned int qinx;

	if (!net_dev || !netif_running(net_dev)) {
		dev_err(&pdev->dev, "suspend fail\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&pdata->pmt_lock, flags);

	if (pdata->phydev)
		phy_stop(pdata->phydev);

	netif_device_detach(net_dev);

	netif_tx_disable(net_dev);
	all_ch_napi_disable(pdata);

	/* stop DMA TX/RX */
	for (qinx = 0; qinx < TX_QUEUE_CNT; qinx++)
		hw_if->stop_dma_tx(qinx);
	for (qinx = 0; qinx < RX_QUEUE_CNT; qinx++)
		hw_if->stop_dma_rx(qinx);

	hw_if->exit();
	close_clk(pdata);

	spin_unlock_irqrestore(&pdata->pmt_lock, flags);

	return 0;
}

static int resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct net_device *net_dev = platform_get_drvdata(pdev);
	struct prv_data *pdata = netdev_priv(net_dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct desc_if_struct *desc_if = &pdata->desc_if;
	unsigned long flags;
	unsigned int qinx;
	int ret;

	if (!net_dev || !netif_running(net_dev)) {
		dev_err(&pdev->dev, "resume fail\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&pdata->pmt_lock, flags);

	ret = enable_clk(pdev, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "resume enable clk fail\n");
		goto clk_fail;
	}

	set_rx_mode(net_dev);
	desc_if->wrapper_tx_desc_init(pdata);
	desc_if->wrapper_rx_desc_init(pdata);
	hw_if->init(pdata);

	if (pdata->phydev)
		phy_start(pdata->phydev);

	/* enable MAC TX/RX */
	hw_if->start_mac_tx_rx();

	/* enable DMA TX/RX */
	for (qinx = 0; qinx < TX_QUEUE_CNT; qinx++)
		hw_if->start_dma_tx(qinx);
	for (qinx = 0; qinx < RX_QUEUE_CNT; qinx++)
		hw_if->start_dma_rx(qinx);

	netif_device_attach(net_dev);

	napi_enable_mq(pdata);

	netif_tx_start_all_queues(net_dev);

	spin_unlock_irqrestore(&pdata->pmt_lock, flags);

	return 0;

clk_fail:
	return ret;
}

static const struct of_device_id dt_ids[] = {
	{ .compatible = "mediatek,mt2712-eth"},
	{}
};

MODULE_DEVICE_TABLE(of, dt_ids);

static const struct dev_pm_ops eth_pm_ops = {
	.suspend = suspend,
	.resume = resume,
};

static struct platform_driver platform_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		   .name = DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(dt_ids),
		   .pm = &eth_pm_ops,
	},
};

static int mtk_init_module(void)
{
	int ret = 0;

	ret = platform_driver_register(&platform_driver);
	if (ret < 0) {
		pr_err("MT2712_ETH:driver registration failed");
		return ret;
	}

	return ret;
}

static void __exit mtk_exit_module(void)
{
	platform_driver_unregister(&platform_driver);
}

module_init(mtk_init_module);

module_exit(mtk_exit_module);

MODULE_AUTHOR("Mediatek");

MODULE_DESCRIPTION("MT2712_ETH Driver");

MODULE_LICENSE("GPL");
