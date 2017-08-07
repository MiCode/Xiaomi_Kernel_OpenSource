/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/pm_wakeup.h>
#include <linux/sched.h>
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>
#include <linux/esoc_client.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/log2.h>
#include <linux/etherdevice.h>
#include <linux/msm_pcie.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/ramdump.h>
#include <net/cfg80211.h>
#include <soc/qcom/memory_dump.h>
#include <net/cnss.h>
#include "cnss_common.h"

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
#include <net/cnss_prealloc.h>
#endif

#define subsys_to_drv(d) container_of(d, struct cnss_data, subsys_desc)

#define VREG_ON			1
#define VREG_OFF		0
#define WLAN_EN_HIGH		1
#define WLAN_EN_LOW		0
#define PCIE_LINK_UP		1
#define PCIE_LINK_DOWN		0
#define WLAN_BOOTSTRAP_HIGH	1
#define WLAN_BOOTSTRAP_LOW	0
#define CNSS_DUMP_FORMAT_VER	0x11
#define CNSS_DUMP_MAGIC_VER_V2	0x42445953
#define CNSS_DUMP_NAME		"CNSS_WLAN"

#define QCA6174_VENDOR_ID	(0x168C)
#define QCA6174_DEVICE_ID	(0x003E)
#define BEELINER_DEVICE_ID      (0x0040)
#define QCA6174_REV_ID_OFFSET	(0x08)
#define QCA6174_FW_1_1	(0x11)
#define QCA6174_FW_1_3	(0x13)
#define QCA6174_FW_2_0	(0x20)
#define QCA6174_FW_3_0	(0x30)
#define QCA6174_FW_3_2	(0x32)
#define BEELINER_FW	(0x00)

#define QCA6180_VENDOR_ID	(0x168C)
#define QCA6180_DEVICE_ID	(0x0041)
#define QCA6180_REV_ID_OFFSET	(0x08)

#define WLAN_EN_VREG_NAME	"vdd-wlan-en"
#define WLAN_VREG_NAME		"vdd-wlan"
#define WLAN_VREG_IO_NAME	"vdd-wlan-io"
#define WLAN_VREG_XTAL_NAME	"vdd-wlan-xtal"
#define WLAN_VREG_XTAL_AON_NAME	"vdd-wlan-xtal-aon"
#define WLAN_VREG_CORE_NAME	"vdd-wlan-core"
#define WLAN_VREG_SP2T_NAME	"vdd-wlan-sp2t"
#define WLAN_SWREG_NAME		"wlan-soc-swreg"
#define WLAN_ANT_SWITCH_NAME	"wlan-ant-switch"
#define WLAN_EN_GPIO_NAME	"wlan-en-gpio"
#define WLAN_BOOTSTRAP_GPIO_NAME "wlan-bootstrap-gpio"
#define PM_OPTIONS		0
#define PM_OPTIONS_SUSPEND_LINK_DOWN \
	(MSM_PCIE_CONFIG_NO_CFG_RESTORE | MSM_PCIE_CONFIG_LINKDOWN)
#define PM_OPTIONS_RESUME_LINK_DOWN \
	(MSM_PCIE_CONFIG_NO_CFG_RESTORE)

#define SOC_SWREG_VOLT_MAX	1200000
#define SOC_SWREG_VOLT_MIN	1200000
#define WLAN_ANT_SWITCH_VOLT_MAX	2700000
#define WLAN_ANT_SWITCH_VOLT_MIN	2700000
#define WLAN_ANT_SWITCH_CURR	20000
#define WLAN_VREG_IO_MAX	1800000
#define WLAN_VREG_IO_MIN	1800000
#define WLAN_VREG_XTAL_MAX	1800000
#define WLAN_VREG_XTAL_MIN	1800000
#define WLAN_VREG_CORE_MAX	1300000
#define WLAN_VREG_CORE_MIN	1300000
#define WLAN_VREG_SP2T_MAX	2700000
#define WLAN_VREG_SP2T_MIN	2700000

#define POWER_ON_DELAY		2
#define WLAN_VREG_IO_DELAY_MIN	100
#define WLAN_VREG_IO_DELAY_MAX	1000
#define WLAN_ENABLE_DELAY	10
#define PCIE_SWITCH_DELAY       20
#define WLAN_RECOVERY_DELAY	1
#define PCIE_ENABLE_DELAY	100
#define WLAN_BOOTSTRAP_DELAY	10
#define EVICT_BIN_MAX_SIZE      (512*1024)

static DEFINE_SPINLOCK(pci_link_down_lock);

#define FW_NAME_FIXED_LEN	(6)
#define MAX_NUM_OF_SEGMENTS	(16)
#define MAX_INDEX_FILE_SIZE	(512)
#define FW_FILENAME_LENGTH	(13)
#define TYPE_LENGTH		(4)
#define PER_FILE_DATA		(21)
#define MAX_IMAGE_SIZE		(2*1024*1024)
#define FW_IMAGE_FTM		(0x01)
#define FW_IMAGE_MISSION	(0x02)
#define FW_IMAGE_BDATA		(0x03)
#define FW_IMAGE_PRINT		(0x04)

#define SEG_METADATA		(0x01)
#define SEG_NON_PAGED		(0x02)
#define SEG_LOCKED_PAGE		(0x03)
#define SEG_UNLOCKED_PAGE	(0x04)
#define SEG_NON_SECURE_DATA	(0x05)

#define BMI_TEST_SETUP		(0x09)

struct cnss_wlan_gpio_info {
	char *name;
	u32 num;
	bool state;
	bool init;
	bool prop;
};

struct cnss_wlan_vreg_info {
	struct regulator *wlan_en_reg;
	struct regulator *wlan_reg;
	struct regulator *soc_swreg;
	struct regulator *ant_switch;
	struct regulator *wlan_reg_io;
	struct regulator *wlan_reg_xtal;
	struct regulator *wlan_reg_xtal_aon;
	struct regulator *wlan_reg_core;
	struct regulator *wlan_reg_sp2t;
	bool state;
};

struct segment_memory {
	dma_addr_t dma_region;
	void *cpu_region;
	u32 size;
};

/* FW image descriptor lists */
struct image_desc_hdr {
	u8 image_id;
	u8 reserved[3];
	u32 segments_cnt;
};

struct segment_desc {
	u8 segment_id;
	u8 segment_idx;
	u8 flags[2];
	u32 addr_count;
	u32 addr_low;
	u32 addr_high;
};

struct region_desc {
	u32 addr_low;
	u32 addr_high;
	u32 size;
	u32 reserved;
};

struct index_file {
	u32 type;
	u32 segment_idx;
	u8 file_name[13];
};

struct cnss_dual_wifi {
	bool is_dual_wifi_enabled;
};

/**
 * struct wlan_mac_addr - Structure to hold WLAN MAC Address
 * @mac_addr: MAC address
 */
#define MAX_NO_OF_MAC_ADDR 4
struct cnss_wlan_mac_addr {
	u8 mac_addr[MAX_NO_OF_MAC_ADDR][ETH_ALEN];
	uint32_t no_of_mac_addr_set;
};

/* device_info is expected to be fully populated after cnss_config is invoked.
 * The function pointer callbacks are expected to be non null as well.
 */
static struct cnss_data {
	struct platform_device *pldev;
	struct subsys_device *subsys;
	struct subsys_desc    subsysdesc;
	struct cnss_wlan_mac_addr wlan_mac_addr;
	bool is_wlan_mac_set;
	bool ramdump_dynamic;
	struct ramdump_device *ramdump_dev;
	unsigned long ramdump_size;
	void *ramdump_addr;
	phys_addr_t ramdump_phys;
	struct msm_dump_data dump_data;
	struct cnss_wlan_driver *driver;
	struct pci_dev *pdev;
	const struct pci_device_id *id;
	struct dma_iommu_mapping *smmu_mapping;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	struct cnss_wlan_vreg_info vreg_info;
	bool wlan_en_vreg_support;
	struct cnss_wlan_gpio_info gpio_info;
	bool pcie_link_state;
	bool pcie_link_down_ind;
	bool pci_register_again;
	bool notify_modem_status;
	struct pci_saved_state *saved_state;
	u16 revision_id;
	bool recovery_in_progress;
	atomic_t fw_available;
	struct codeswap_codeseg_info *cnss_seg_info;
	/* Virtual Address of the DMA page */
	void *codeseg_cpuaddr[CODESWAP_MAX_CODESEGS];
	struct cnss_fw_files fw_files;
	struct pm_qos_request qos_request;
	void *modem_notify_handler;
	int modem_current_status;
	struct msm_bus_scale_pdata *bus_scale_table;
	uint32_t bus_client;
	int current_bandwidth_vote;
	void *subsys_handle;
	struct esoc_desc *esoc_desc;
	struct cnss_platform_cap cap;
	struct msm_pcie_register_event event_reg;
	struct wakeup_source ws;
	uint32_t recovery_count;
	enum cnss_driver_status driver_status;
#ifdef CONFIG_CNSS_SECURE_FW
	void *fw_mem;
#endif
	u32 device_id;
	int fw_image_setup;
	uint32_t bmi_test;
	void *fw_cpu;
	dma_addr_t fw_dma;
	u32 fw_dma_size;
	u32 fw_seg_count;
	struct segment_memory fw_seg_mem[MAX_NUM_OF_SEGMENTS];
	/* Firmware setup complete lock */
	struct mutex fw_setup_stat_lock;
	void *bdata_cpu;
	dma_addr_t bdata_dma;
	u32 bdata_dma_size;
	u32 bdata_seg_count;
	struct segment_memory bdata_seg_mem[MAX_NUM_OF_SEGMENTS];
	int wlan_bootstrap_gpio;
	atomic_t auto_suspended;
	bool monitor_wake_intr;
	struct cnss_dual_wifi dual_wifi_info;
	struct cnss_dev_platform_ops platform_ops;
} *penv;

static unsigned int pcie_link_down_panic;
module_param(pcie_link_down_panic, uint, S_IRUSR | S_IWUSR);
MODULE_PARM_DESC(pcie_link_down_panic,
		"Trigger kernel panic when PCIe link down is detected");

static void cnss_put_wlan_enable_gpio(void)
{
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;

	if (penv->wlan_en_vreg_support)
		regulator_put(vreg_info->wlan_en_reg);
	else
		gpio_free(gpio_info->num);
}

static int cnss_wlan_vreg_on(struct cnss_wlan_vreg_info *vreg_info)
{
	int ret;

	if (vreg_info->wlan_reg_core) {
		ret = regulator_enable(vreg_info->wlan_reg_core);
		if (ret) {
			pr_err("%s: regulator enable failed for wlan_reg_core\n",
				__func__);
			goto error_enable_reg_core;
		}
	}

	if (vreg_info->wlan_reg_io) {
		ret = regulator_enable(vreg_info->wlan_reg_io);
		if (ret) {
			pr_err("%s: regulator enable failed for wlan_reg_io\n",
				__func__);
			goto error_enable_reg_io;
		}

		usleep_range(WLAN_VREG_IO_DELAY_MIN, WLAN_VREG_IO_DELAY_MAX);
	}

	if (vreg_info->wlan_reg_xtal_aon) {
		ret = regulator_enable(vreg_info->wlan_reg_xtal_aon);
		if (ret) {
			pr_err("%s: wlan_reg_xtal_aon enable failed\n",
			       __func__);
			goto error_enable_reg_xtal_aon;
		}
	}

	if (vreg_info->wlan_reg_xtal) {
		ret = regulator_enable(vreg_info->wlan_reg_xtal);
		if (ret) {
			pr_err("%s: regulator enable failed for wlan_reg_xtal\n",
				__func__);
			goto error_enable_reg_xtal;
		}
	}

	ret = regulator_enable(vreg_info->wlan_reg);
	if (ret) {
		pr_err("%s: regulator enable failed for WLAN power\n",
		       __func__);
		goto error_enable;
	}

	if (vreg_info->wlan_reg_sp2t) {
		ret = regulator_enable(vreg_info->wlan_reg_sp2t);
		if (ret) {
			pr_err("%s: regulator enable failed for wlan_reg_sp2t\n",
				__func__);
			goto error_enable_reg_sp2t;
		}
	}

	if (vreg_info->ant_switch) {
		ret = regulator_enable(vreg_info->ant_switch);
		if (ret) {
			pr_err("%s: regulator enable failed for ant_switch\n",
			       __func__);
			goto error_enable_ant_switch;
		}
	}

	if (vreg_info->soc_swreg) {
		ret = regulator_enable(vreg_info->soc_swreg);
		if (ret) {
			pr_err("%s: regulator enable failed for external soc-swreg\n",
					__func__);
			goto error_enable_soc_swreg;
		}
	}

	return ret;

error_enable_soc_swreg:
	if (vreg_info->ant_switch)
		regulator_disable(vreg_info->ant_switch);
error_enable_ant_switch:
	if (vreg_info->wlan_reg_sp2t)
		regulator_disable(vreg_info->wlan_reg_sp2t);
error_enable_reg_sp2t:
	regulator_disable(vreg_info->wlan_reg);
error_enable:
	if (vreg_info->wlan_reg_xtal)
		regulator_disable(vreg_info->wlan_reg_xtal);
error_enable_reg_xtal:
	if (vreg_info->wlan_reg_xtal_aon)
		regulator_disable(vreg_info->wlan_reg_xtal_aon);
error_enable_reg_xtal_aon:
	if (vreg_info->wlan_reg_io)
		regulator_disable(vreg_info->wlan_reg_io);
error_enable_reg_io:
	if (vreg_info->wlan_reg_core)
		regulator_disable(vreg_info->wlan_reg_core);
error_enable_reg_core:
	return ret;
}

static int cnss_wlan_vreg_off(struct cnss_wlan_vreg_info *vreg_info)
{
	int ret;

	if (vreg_info->soc_swreg) {
		ret = regulator_disable(vreg_info->soc_swreg);
		if (ret) {
			pr_err("%s: regulator disable failed for external soc-swreg\n",
					__func__);
			goto error_disable;
		}
	}

	if (vreg_info->ant_switch) {
		ret = regulator_disable(vreg_info->ant_switch);
		if (ret) {
			pr_err("%s: regulator disable failed for ant_switch\n",
			       __func__);
			goto error_disable;
		}
	}

	if (vreg_info->wlan_reg_sp2t) {
		ret = regulator_disable(vreg_info->wlan_reg_sp2t);
		if (ret) {
			pr_err("%s: regulator disable failed for wlan_reg_sp2t\n",
				__func__);
			goto error_disable;
		}
	}

	ret = regulator_disable(vreg_info->wlan_reg);
	if (ret) {
		pr_err("%s: regulator disable failed for WLAN power\n",
		       __func__);
		goto error_disable;
	}

	if (vreg_info->wlan_reg_xtal) {
		ret = regulator_disable(vreg_info->wlan_reg_xtal);
		if (ret) {
			pr_err("%s: regulator disable failed for wlan_reg_xtal\n",
				__func__);
			goto error_disable;
		}
	}

	if (vreg_info->wlan_reg_xtal_aon) {
		ret = regulator_disable(vreg_info->wlan_reg_xtal_aon);
		if (ret) {
			pr_err("%s: wlan_reg_xtal_aon disable failed\n",
			       __func__);
			goto error_disable;
		}
	}

	if (vreg_info->wlan_reg_io) {
		ret = regulator_disable(vreg_info->wlan_reg_io);
		if (ret) {
			pr_err("%s: regulator disable failed for wlan_reg_io\n",
				__func__);
			goto error_disable;
		}
	}

	if (vreg_info->wlan_reg_core) {
		ret = regulator_disable(vreg_info->wlan_reg_core);
		if (ret) {
			pr_err("%s: regulator disable failed for wlan_reg_core\n",
				__func__);
			goto error_disable;
		}
	}

error_disable:
	return ret;
}

static int cnss_wlan_vreg_set(struct cnss_wlan_vreg_info *vreg_info, bool state)
{
	int ret = 0;

	if (vreg_info->state == state) {
		pr_debug("Already wlan vreg state is %s\n",
				state ? "enabled" : "disabled");
		goto out;
	}

	if (state)
		ret = cnss_wlan_vreg_on(vreg_info);
	else
		ret = cnss_wlan_vreg_off(vreg_info);

	if (ret)
		goto out;

	pr_debug("%s: wlan vreg is now %s\n", __func__,
			state ? "enabled" : "disabled");
	vreg_info->state = state;

out:
	return ret;
}

static int cnss_wlan_gpio_init(struct cnss_wlan_gpio_info *info)
{
	int ret = 0;

	ret = gpio_request(info->num, info->name);

	if (ret) {
		pr_err("can't get gpio %s ret %d\n", info->name, ret);
		goto err_gpio_req;
	}

	ret = gpio_direction_output(info->num, info->init);

	if (ret) {
		pr_err("can't set gpio direction %s ret %d\n", info->name, ret);
		goto err_gpio_dir;
	}
	info->state = info->init;

	return ret;

err_gpio_dir:
	gpio_free(info->num);

err_gpio_req:

	return ret;
}

static int cnss_wlan_bootstrap_gpio_init(void)
{
	int ret = 0;

	ret = gpio_request(penv->wlan_bootstrap_gpio, WLAN_BOOTSTRAP_GPIO_NAME);
	if (ret) {
		pr_err("%s: Can't get GPIO %s, ret = %d\n",
		       __func__, WLAN_BOOTSTRAP_GPIO_NAME, ret);
		goto out;
	}

	ret = gpio_direction_output(penv->wlan_bootstrap_gpio,
				    WLAN_BOOTSTRAP_HIGH);
	if (ret) {
		pr_err("%s: Can't set GPIO %s direction, ret = %d\n",
		       __func__, WLAN_BOOTSTRAP_GPIO_NAME, ret);
		gpio_free(penv->wlan_bootstrap_gpio);
		goto out;
	}

	msleep(WLAN_BOOTSTRAP_DELAY);
out:
	return ret;
}

static void cnss_wlan_gpio_set(struct cnss_wlan_gpio_info *info, bool state)
{
	if (!info->prop)
		return;

	if (info->state == state) {
		pr_debug("Already %s gpio is %s\n",
			 info->name, state ? "high" : "low");
		return;
	}

	gpio_set_value(info->num, state);
	info->state = state;

	pr_debug("%s: %s gpio is now %s\n", __func__,
		 info->name, info->state ? "enabled" : "disabled");
}

static int cnss_configure_wlan_en_gpio(bool state)
{
	int ret = 0;
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;

	if (penv->wlan_en_vreg_support) {
		if (state)
			ret = regulator_enable(vreg_info->wlan_en_reg);
		else
			ret = regulator_disable(vreg_info->wlan_en_reg);
	} else {
		cnss_wlan_gpio_set(gpio_info, state);
	}

	msleep(WLAN_ENABLE_DELAY);
	return ret;
}

static void cnss_disable_xtal_ldo(struct platform_device *pdev)
{
	struct cnss_wlan_vreg_info *info = &penv->vreg_info;

	if (info->wlan_reg_xtal) {
		regulator_disable(info->wlan_reg_xtal);
		regulator_put(info->wlan_reg_xtal);
	}

	if (info->wlan_reg_xtal_aon) {
		regulator_disable(info->wlan_reg_xtal_aon);
		regulator_put(info->wlan_reg_xtal_aon);
	}
}

static int cnss_enable_xtal_ldo(struct platform_device *pdev)
{
	int ret = 0;
	struct cnss_wlan_vreg_info *info = &penv->vreg_info;

	if (!of_get_property(pdev->dev.of_node,
			     WLAN_VREG_XTAL_AON_NAME "-supply", NULL))
		goto enable_xtal;

	info->wlan_reg_xtal_aon = regulator_get(&pdev->dev,
						WLAN_VREG_XTAL_AON_NAME);
	if (IS_ERR(info->wlan_reg_xtal_aon)) {
		ret = PTR_ERR(info->wlan_reg_xtal_aon);
		pr_err("%s: XTAL AON Regulator get failed err:%d\n", __func__,
		       ret);
		return ret;
	}

	ret = regulator_enable(info->wlan_reg_xtal_aon);
	if (ret) {
		pr_err("%s: VREG_XTAL_ON enable failed\n", __func__);
		goto end;
	}

enable_xtal:

	if (!of_get_property(pdev->dev.of_node,
			     WLAN_VREG_XTAL_NAME "-supply", NULL))
		goto out_disable_xtal_aon;

	info->wlan_reg_xtal = regulator_get(&pdev->dev, WLAN_VREG_XTAL_NAME);

	if (IS_ERR(info->wlan_reg_xtal)) {
		ret = PTR_ERR(info->wlan_reg_xtal);
		pr_err("%s XTAL Regulator get failed err:%d\n", __func__, ret);
		goto out_disable_xtal_aon;
	}

	ret = regulator_set_voltage(info->wlan_reg_xtal, WLAN_VREG_XTAL_MIN,
				    WLAN_VREG_XTAL_MAX);
	if (ret) {
		pr_err("%s: Set wlan_vreg_xtal failed!\n", __func__);
		goto out_put_xtal;
	}

	ret = regulator_enable(info->wlan_reg_xtal);
	if (ret) {
		pr_err("%s: Enable wlan_vreg_xtal failed!\n", __func__);
		goto out_put_xtal;
	}

	return 0;

out_put_xtal:
	if (info->wlan_reg_xtal)
		regulator_put(info->wlan_reg_xtal);

out_disable_xtal_aon:
	if (info->wlan_reg_xtal_aon)
		regulator_disable(info->wlan_reg_xtal_aon);

end:
	if (info->wlan_reg_xtal_aon)
		regulator_put(info->wlan_reg_xtal_aon);

	return ret;
}

static int cnss_get_wlan_enable_gpio(
	struct cnss_wlan_gpio_info *gpio_info,
	struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	if (!of_find_property(dev->of_node, gpio_info->name, NULL)) {
		gpio_info->prop = false;
		return -ENODEV;
	}

	gpio_info->prop = true;
	ret = of_get_named_gpio(dev->of_node, gpio_info->name, 0);
	if (ret >= 0) {
		gpio_info->num = ret;
	} else {
		if (ret == -EPROBE_DEFER)
			pr_debug("get WLAN_EN GPIO probe defer\n");
		else
			pr_err(
			"can't get gpio %s ret %d", gpio_info->name, ret);
	}

	ret = cnss_wlan_gpio_init(gpio_info);
	if (ret)
		pr_err("gpio init failed\n");

	return ret;
}

static int cnss_get_wlan_bootstrap_gpio(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = (&pdev->dev)->of_node;

	if (!of_find_property(node, WLAN_BOOTSTRAP_GPIO_NAME, NULL))
		return ret;

	penv->wlan_bootstrap_gpio =
		of_get_named_gpio(node, WLAN_BOOTSTRAP_GPIO_NAME, 0);
	if (penv->wlan_bootstrap_gpio > 0) {
		ret = cnss_wlan_bootstrap_gpio_init();
	} else {
		ret = penv->wlan_bootstrap_gpio;
		pr_err(
		"%s: Can't get GPIO %s, ret = %d",
		__func__, WLAN_BOOTSTRAP_GPIO_NAME, ret);
	}

	return ret;
}

static int cnss_wlan_get_resources(struct platform_device *pdev)
{
	int ret = 0;
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;
	struct device_node *node = pdev->dev.of_node;

	if (of_get_property(node, WLAN_VREG_CORE_NAME "-supply", NULL)) {
		vreg_info->wlan_reg_core = regulator_get(&pdev->dev,
			WLAN_VREG_CORE_NAME);
		if (IS_ERR(vreg_info->wlan_reg_core)) {
			ret = PTR_ERR(vreg_info->wlan_reg_core);

			if (ret == -EPROBE_DEFER) {
				pr_err("%s: wlan_reg_core probe deferred!\n",
					__func__);
			} else {
				pr_err("%s: Get wlan_reg_core failed!\n",
					__func__);
			}
			goto err_reg_core_get;
		}

		ret = regulator_set_voltage(vreg_info->wlan_reg_core,
			WLAN_VREG_CORE_MIN, WLAN_VREG_CORE_MAX);
		if (ret) {
			pr_err("%s: Set wlan_reg_core failed!\n", __func__);
			goto err_reg_core_set;
		}

		ret = regulator_enable(vreg_info->wlan_reg_core);
		if (ret) {
			pr_err("%s: Enable wlan_reg_core failed!\n", __func__);
			goto err_reg_core_enable;
		}
	}

	if (of_get_property(node, WLAN_VREG_IO_NAME "-supply", NULL)) {
		vreg_info->wlan_reg_io = regulator_get(&pdev->dev,
			WLAN_VREG_IO_NAME);
		if (!IS_ERR(vreg_info->wlan_reg_io)) {
			ret = regulator_set_voltage(vreg_info->wlan_reg_io,
				WLAN_VREG_IO_MIN, WLAN_VREG_IO_MAX);
			if (ret) {
				pr_err("%s: Set wlan_vreg_io failed!\n",
					__func__);
				goto err_reg_io_set;
			}

			ret = regulator_enable(vreg_info->wlan_reg_io);
			if (ret) {
				pr_err("%s: Enable wlan_vreg_io failed!\n",
					__func__);
				goto err_reg_io_enable;
			}

			usleep_range(WLAN_VREG_IO_DELAY_MIN,
				     WLAN_VREG_IO_DELAY_MAX);
		}
	}

	if (cnss_enable_xtal_ldo(pdev))
		goto err_reg_xtal_enable;

	vreg_info->wlan_reg = regulator_get(&pdev->dev, WLAN_VREG_NAME);

	if (IS_ERR(vreg_info->wlan_reg)) {
		if (PTR_ERR(vreg_info->wlan_reg) == -EPROBE_DEFER)
			pr_err("%s: vreg probe defer\n", __func__);
		else
			pr_err("%s: vreg regulator get failed\n", __func__);
		ret = PTR_ERR(vreg_info->wlan_reg);
		goto err_reg_get;
	}

	ret = regulator_enable(vreg_info->wlan_reg);

	if (ret) {
		pr_err("%s: vreg initial vote failed\n", __func__);
		goto err_reg_enable;
	}

	if (of_get_property(node, WLAN_VREG_SP2T_NAME "-supply", NULL)) {
		vreg_info->wlan_reg_sp2t =
			regulator_get(&pdev->dev, WLAN_VREG_SP2T_NAME);
		if (!IS_ERR(vreg_info->wlan_reg_sp2t)) {
			ret = regulator_set_voltage(vreg_info->wlan_reg_sp2t,
				WLAN_VREG_SP2T_MIN, WLAN_VREG_SP2T_MAX);
			if (ret) {
				pr_err("%s: Set wlan_vreg_sp2t failed!\n",
					__func__);
				goto err_reg_sp2t_set;
			}

			ret = regulator_enable(vreg_info->wlan_reg_sp2t);
			if (ret) {
				pr_err("%s: Enable wlan_vreg_sp2t failed!\n",
					__func__);
				goto err_reg_sp2t_enable;
			}
		}
	}

	if (of_get_property(node, WLAN_ANT_SWITCH_NAME "-supply", NULL)) {
		vreg_info->ant_switch =
			regulator_get(&pdev->dev, WLAN_ANT_SWITCH_NAME);
		if (!IS_ERR(vreg_info->ant_switch)) {
			ret = regulator_set_voltage(vreg_info->ant_switch,
						    WLAN_ANT_SWITCH_VOLT_MIN,
						    WLAN_ANT_SWITCH_VOLT_MAX);
			if (ret < 0) {
				pr_err("%s: Set ant_switch voltage failed!\n",
				       __func__);
				goto err_ant_switch_set;
			}

			ret = regulator_set_optimum_mode(vreg_info->ant_switch,
							 WLAN_ANT_SWITCH_CURR);
			if (ret < 0) {
				pr_err("%s: Set ant_switch current failed!\n",
				       __func__);
				goto err_ant_switch_set;
			}

			ret = regulator_enable(vreg_info->ant_switch);
			if (ret < 0) {
				pr_err("%s: Enable ant_switch failed!\n",
				       __func__);
				goto err_ant_switch_enable;
			}
		}
	}

	if (of_find_property(node, "qcom,wlan-uart-access", NULL))
		penv->cap.cap_flag |= CNSS_HAS_UART_ACCESS;

	if (of_get_property(node, WLAN_SWREG_NAME "-supply", NULL)) {
		vreg_info->soc_swreg = regulator_get(&pdev->dev,
			WLAN_SWREG_NAME);
		if (IS_ERR(vreg_info->soc_swreg)) {
			pr_err("%s: soc-swreg node not found\n",
					__func__);
			goto err_reg_get2;
		}
		ret = regulator_set_voltage(vreg_info->soc_swreg,
				SOC_SWREG_VOLT_MIN, SOC_SWREG_VOLT_MAX);
		if (ret) {
			pr_err("%s: vreg initial voltage set failed on soc-swreg\n",
					__func__);
			goto err_reg_set;
		}
		ret = regulator_enable(vreg_info->soc_swreg);
		if (ret) {
			pr_err("%s: vreg initial vote failed\n", __func__);
			goto err_reg_enable2;
		}
		penv->cap.cap_flag |= CNSS_HAS_EXTERNAL_SWREG;
	}

	penv->wlan_en_vreg_support =
		of_property_read_bool(node, "qcom,wlan-en-vreg-support");
	if (penv->wlan_en_vreg_support) {
		vreg_info->wlan_en_reg =
			regulator_get(&pdev->dev, WLAN_EN_VREG_NAME);
		if (IS_ERR(vreg_info->wlan_en_reg)) {
			pr_err("%s:wlan_en vreg get failed\n", __func__);
			ret = PTR_ERR(vreg_info->wlan_en_reg);
			goto err_wlan_en_reg_get;
		}
	}

	if (!penv->wlan_en_vreg_support) {
		ret = cnss_get_wlan_enable_gpio(gpio_info, pdev);
		if (ret) {
			pr_err(
			"%s:Failed to config the WLAN_EN gpio\n", __func__);
			goto err_gpio_wlan_en;
		}
	}
	vreg_info->state = VREG_ON;

	ret = cnss_get_wlan_bootstrap_gpio(pdev);
	if (ret) {
		pr_err("%s: Failed to enable wlan bootstrap gpio\n", __func__);
		goto err_gpio_wlan_bootstrap;
	}

	return ret;

err_gpio_wlan_bootstrap:
	cnss_put_wlan_enable_gpio();
err_gpio_wlan_en:
err_wlan_en_reg_get:
	vreg_info->wlan_en_reg = NULL;
	if (vreg_info->soc_swreg)
		regulator_disable(vreg_info->soc_swreg);
	vreg_info->state = VREG_OFF;

err_reg_enable2:
err_reg_set:
	if (vreg_info->soc_swreg)
		regulator_put(vreg_info->soc_swreg);

err_reg_get2:
	if (vreg_info->ant_switch)
		regulator_disable(vreg_info->ant_switch);

err_ant_switch_enable:
err_ant_switch_set:
	if (vreg_info->ant_switch)
		regulator_put(vreg_info->ant_switch);
	if (vreg_info->wlan_reg_sp2t)
		regulator_disable(vreg_info->wlan_reg_sp2t);

err_reg_sp2t_enable:
err_reg_sp2t_set:
	if (vreg_info->wlan_reg_sp2t)
		regulator_put(vreg_info->wlan_reg_sp2t);
	regulator_disable(vreg_info->wlan_reg);

err_reg_enable:
	regulator_put(vreg_info->wlan_reg);
err_reg_get:
	cnss_disable_xtal_ldo(pdev);

err_reg_xtal_enable:
	if (vreg_info->wlan_reg_io)
		regulator_disable(vreg_info->wlan_reg_io);

err_reg_io_enable:
err_reg_io_set:
	if (vreg_info->wlan_reg_io)
		regulator_put(vreg_info->wlan_reg_io);
	if (vreg_info->wlan_reg_core)
		regulator_disable(vreg_info->wlan_reg_core);

err_reg_core_enable:
err_reg_core_set:
	if (vreg_info->wlan_reg_core)
		regulator_put(vreg_info->wlan_reg_core);

err_reg_core_get:
	return ret;
}

static void cnss_wlan_release_resources(void)
{
	struct cnss_wlan_gpio_info *gpio_info = &penv->gpio_info;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;

	if (penv->wlan_bootstrap_gpio > 0)
		gpio_free(penv->wlan_bootstrap_gpio);
	cnss_put_wlan_enable_gpio();
	gpio_info->state = WLAN_EN_LOW;
	gpio_info->prop = false;
	cnss_wlan_vreg_set(vreg_info, VREG_OFF);
	if (vreg_info->soc_swreg)
		regulator_put(vreg_info->soc_swreg);
	if (vreg_info->ant_switch)
		regulator_put(vreg_info->ant_switch);
	if (vreg_info->wlan_reg_sp2t)
		regulator_put(vreg_info->wlan_reg_sp2t);
	regulator_put(vreg_info->wlan_reg);
	if (vreg_info->wlan_reg_xtal)
		regulator_put(vreg_info->wlan_reg_xtal);
	if (vreg_info->wlan_reg_xtal_aon)
		regulator_put(vreg_info->wlan_reg_xtal_aon);
	if (vreg_info->wlan_reg_io)
		regulator_put(vreg_info->wlan_reg_io);
	if (vreg_info->wlan_reg_core)
		regulator_put(vreg_info->wlan_reg_core);
	vreg_info->state = VREG_OFF;
}

static u8 cnss_get_pci_dev_bus_number(struct pci_dev *pdev)
{
	return pdev->bus->number;
}

void cnss_setup_fw_files(u16 revision)
{
	switch (revision) {

	case QCA6174_FW_1_1:
		strlcpy(penv->fw_files.image_file, "qwlan11.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.board_data, "bdwlan11.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.otp_data, "otp11.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_file, "utf11.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_board_data, "utfbd11.bin",
			CNSS_MAX_FILE_NAME);
		break;

	case QCA6174_FW_1_3:
		strlcpy(penv->fw_files.image_file, "qwlan13.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.board_data, "bdwlan13.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.otp_data, "otp13.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_file, "utf13.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_board_data, "utfbd13.bin",
			CNSS_MAX_FILE_NAME);
		break;

	case QCA6174_FW_2_0:
		strlcpy(penv->fw_files.image_file, "qwlan20.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.board_data, "bdwlan20.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.otp_data, "otp20.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_file, "utf20.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_board_data, "utfbd20.bin",
			CNSS_MAX_FILE_NAME);
		break;

	case QCA6174_FW_3_0:
	case QCA6174_FW_3_2:
		strlcpy(penv->fw_files.image_file, "qwlan30.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.board_data, "bdwlan30.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.otp_data, "otp30.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_file, "utf30.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_board_data, "utfbd30.bin",
			CNSS_MAX_FILE_NAME);
		break;

	default:
		strlcpy(penv->fw_files.image_file, "qwlan.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.board_data, "bdwlan.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.otp_data, "otp.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_file, "utf.bin",
			CNSS_MAX_FILE_NAME);
		strlcpy(penv->fw_files.utf_board_data, "utfbd.bin",
			CNSS_MAX_FILE_NAME);
		break;
	}
}

int cnss_get_fw_files(struct cnss_fw_files *pfw_files)
{
	if (!penv || !pfw_files)
		return -ENODEV;

	*pfw_files = penv->fw_files;

	return 0;
}
EXPORT_SYMBOL(cnss_get_fw_files);

#ifdef CONFIG_CNSS_SECURE_FW
static void cnss_wlan_fw_mem_alloc(struct pci_dev *pdev)
{
	penv->fw_mem = devm_kzalloc(&pdev->dev, MAX_FIRMWARE_SIZE, GFP_KERNEL);

	if (!penv->fw_mem)
		pr_debug("Memory not available for Secure FW\n");
}
#else
static void cnss_wlan_fw_mem_alloc(struct pci_dev *pdev)
{
}
#endif

static int get_image_file(const u8 *index_info, u8 *file_name,
	u32 *type, u32 *segment_idx)
{

	if (!file_name || !index_info || !type)
		return -EINVAL;

	memcpy(type, index_info, TYPE_LENGTH);
	memcpy(segment_idx, index_info + TYPE_LENGTH, TYPE_LENGTH);
	memcpy(file_name, index_info + TYPE_LENGTH + TYPE_LENGTH,
		FW_FILENAME_LENGTH);

	pr_debug("%u: %u: %s", *type, *segment_idx, file_name);

	return PER_FILE_DATA;
}

static void print_allocated_image_table(void)
{
	u32 seg = 0, count = 0;
	u8 *dump_addr;
	struct segment_memory *pseg_mem = penv->fw_seg_mem;
	struct segment_memory *p_bdata_seg_mem = penv->bdata_seg_mem;

	pr_debug("%s: Dumping FW IMAGE\n", __func__);
	while (seg++ < penv->fw_seg_count) {
		dump_addr = (u8 *)pseg_mem->cpu_region +
			sizeof(struct region_desc);
		for (count = 0; count < pseg_mem->size -
				sizeof(struct region_desc); count++)
			pr_debug("%02x", dump_addr[count]);

		pseg_mem++;
	}

	seg = 0;
	pr_debug("%s: Dumping BOARD DATA\n", __func__);
	while (seg++ < penv->bdata_seg_count) {
		dump_addr = (u8 *)p_bdata_seg_mem->cpu_region +
			sizeof(struct region_desc);
		for (count = 0; count < p_bdata_seg_mem->size -
			     sizeof(struct region_desc); count++)
			pr_debug("%02x ", dump_addr[count]);

		p_bdata_seg_mem++;
	}
}

static void free_allocated_image_table(void)
{
	struct device *dev = &penv->pdev->dev;
	struct segment_memory *pseg_mem;
	u32 seg = 0;

	/* free fw memroy */
	pseg_mem = penv->fw_seg_mem;
	while (seg++ < penv->fw_seg_count) {
		dma_free_coherent(dev, pseg_mem->size,
			pseg_mem->cpu_region, pseg_mem->dma_region);
		pseg_mem++;
	}
	if (penv->fw_cpu)
		dma_free_coherent(dev,
			sizeof(struct segment_desc) * MAX_NUM_OF_SEGMENTS,
			penv->fw_cpu, penv->fw_dma);
	penv->fw_seg_count = 0;
	penv->fw_dma = 0;
	penv->fw_cpu = NULL;
	penv->fw_dma_size = 0;

	/* free bdata memory */
	seg = 0;
	pseg_mem = penv->bdata_seg_mem;
	while (seg++ < penv->bdata_seg_count) {
		dma_free_coherent(dev, pseg_mem->size,
			pseg_mem->cpu_region, pseg_mem->dma_region);
		pseg_mem++;
	}
	if (penv->bdata_cpu)
		dma_free_coherent(dev,
			sizeof(struct segment_desc) * MAX_NUM_OF_SEGMENTS,
			penv->bdata_cpu, penv->bdata_dma);
	penv->bdata_seg_count = 0;
	penv->bdata_dma = 0;
	penv->bdata_cpu = NULL;
	penv->bdata_dma_size = 0;
}

static int cnss_setup_fw_image_table(int mode)
{
	struct image_desc_hdr *image_hdr;
	struct segment_desc *pseg = NULL;
	const struct firmware *fw_index, *fw_image;
	struct device *dev = NULL;
	char reserved[3] = "";
	u8 image_file[FW_FILENAME_LENGTH] = "";
	u8 index_file[FW_FILENAME_LENGTH] = "";
	u8 index_info[MAX_INDEX_FILE_SIZE] = "";
	size_t image_desc_size = 0, file_size = 0;
	size_t index_pos = 0, image_pos = 0;
	struct region_desc *reg_desc = NULL;
	u32 type = 0;
	u32 segment_idx = 0;
	uintptr_t address;
	int ret = 0;
	dma_addr_t dma_addr;
	void *vaddr = NULL;
	dma_addr_t paddr;
	struct segment_memory *pseg_mem;
	u32 *pseg_count;

	if (!penv || !penv->pdev) {
		pr_err("cnss: invalid penv or pdev or dev\n");
		ret = -EINVAL;
		goto err;
	}
	dev = &penv->pdev->dev;

	/*  meta data file has image details */
	switch (mode) {
	case FW_IMAGE_FTM:
		ret = scnprintf(index_file, FW_FILENAME_LENGTH, "qftm.bin");
		pseg_mem = penv->fw_seg_mem;
		pseg_count = &penv->fw_seg_count;
		break;
	case FW_IMAGE_MISSION:
		ret = scnprintf(index_file, FW_FILENAME_LENGTH, "qwlan.bin");
		pseg_mem = penv->fw_seg_mem;
		pseg_count = &penv->fw_seg_count;
		break;
	case FW_IMAGE_BDATA:
		ret = scnprintf(index_file, FW_FILENAME_LENGTH, "bdwlan.bin");
		pseg_mem = penv->bdata_seg_mem;
		pseg_count = &penv->bdata_seg_count;
		break;
	default:
		pr_err("%s: Unknown meta data file type 0x%x\n",
		       __func__, mode);
		ret = -EINVAL;
	}
	if (ret < 0)
		goto err;

	image_desc_size = sizeof(struct image_desc_hdr) +
		sizeof(struct segment_desc) * MAX_NUM_OF_SEGMENTS;

	vaddr = dma_alloc_coherent(dev, image_desc_size,
			&paddr, GFP_KERNEL);

	if (!vaddr) {
		pr_err("cnss: image desc allocation failure\n");
		ret = -ENOMEM;
		goto err;
	}

	memset(vaddr, 0, image_desc_size);

	image_hdr = (struct image_desc_hdr *)vaddr;
	image_hdr->image_id = mode;
	memcpy(image_hdr->reserved, reserved, 3);

	pr_err("cnss: request meta data file %s\n", index_file);
	ret = request_firmware(&fw_index, index_file, dev);
	if (ret || !fw_index || !fw_index->data || !fw_index->size) {
		pr_err("cnss: meta data file open failure %s\n", index_file);
		goto err_free;
	}

	if (fw_index->size > MAX_INDEX_FILE_SIZE) {
		pr_err("cnss: meta data file has invalid size %s: %zu\n",
				index_file, fw_index->size);
		release_firmware(fw_index);
		goto err_free;
	}

	memcpy(index_info, fw_index->data, fw_index->size);
	file_size = fw_index->size;
	release_firmware(fw_index);

	while (file_size >= PER_FILE_DATA  && image_pos < image_desc_size &&
			image_hdr->segments_cnt < MAX_NUM_OF_SEGMENTS) {

		ret = get_image_file(index_info + index_pos,
			image_file, &type, &segment_idx);
		if (ret == -EINVAL)
			goto err_free;

		file_size -= ret;
		index_pos += ret;
		pseg = vaddr + image_pos +
				sizeof(struct image_desc_hdr);

		switch (type) {
		case SEG_METADATA:
		case SEG_NON_PAGED:
		case SEG_LOCKED_PAGE:
		case SEG_UNLOCKED_PAGE:
		case SEG_NON_SECURE_DATA:

			image_hdr->segments_cnt++;
			pseg->segment_id = type;
			pseg->segment_idx = (u8)(segment_idx & 0xff);
			memcpy(pseg->flags, reserved, 2);

			ret = request_firmware(&fw_image, image_file, dev);
			if (ret || !fw_image || !fw_image->data ||
				!fw_image->size) {
				pr_err("cnss: image file read failed %s",
						image_file);
				goto err_free;
			}
			if (fw_image->size > MAX_IMAGE_SIZE) {
				pr_err("cnss: %s: image file invalid size %zu\n",
					image_file, fw_image->size);
				release_firmware(fw_image);
				ret = -EINVAL;
				goto err_free;
			}
			reg_desc = dma_alloc_coherent(dev,
				sizeof(struct region_desc) + fw_image->size,
				    &dma_addr, GFP_KERNEL);
			if (!reg_desc) {
				pr_err("cnss: region allocation failure\n");
				ret = -ENOMEM;
				release_firmware(fw_image);
				goto err_free;
			}
			address = (uintptr_t) dma_addr;
			pseg->addr_low = address & 0xFFFFFFFF;
			pseg->addr_high = 0x00;
			/* one region for one image file */
			pseg->addr_count = 1;
			memcpy((u8 *)reg_desc + sizeof(struct region_desc),
					fw_image->data, fw_image->size);
			address += sizeof(struct region_desc);
			reg_desc->addr_low = address & 0xFFFFFFFF;
			reg_desc->addr_high = 0x00;
			reg_desc->reserved = 0;
			reg_desc->size = fw_image->size;

			pseg_mem[*pseg_count].dma_region = dma_addr;
			pseg_mem[*pseg_count].cpu_region = reg_desc;
			pseg_mem[*pseg_count].size =
				sizeof(struct region_desc) + fw_image->size;

			release_firmware(fw_image);
			(*pseg_count)++;
			break;

		default:
			pr_err("cnss: Unknown segment %d", type);
			ret = -EINVAL;
			goto err_free;
	    }
	    image_pos += sizeof(struct segment_desc);
	}
	if (mode != FW_IMAGE_BDATA) {
		penv->fw_cpu = vaddr;
		penv->fw_dma = paddr;
		penv->fw_dma_size = sizeof(struct image_desc_hdr) +
			sizeof(struct segment_desc) * image_hdr->segments_cnt;
	} else {
		penv->bdata_cpu = vaddr;
		penv->bdata_dma = paddr;
		penv->bdata_dma_size = sizeof(struct image_desc_hdr) +
			sizeof(struct segment_desc) * image_hdr->segments_cnt;
	}
	pr_info("%s: Mode %d: Image setup table built on host", __func__, mode);

	return file_size;


err_free:
	free_allocated_image_table();
err:
	pr_err("cnss: image file setup failed %d\n", ret);
	return ret;
}

int cnss_get_fw_image(struct image_desc_info *image_desc_info)
{
	if (!image_desc_info || !penv ||
	    !penv->fw_seg_count || !penv->bdata_seg_count)
		return -EINVAL;

	mutex_lock(&penv->fw_setup_stat_lock);
	image_desc_info->fw_addr = penv->fw_dma;
	image_desc_info->fw_size = penv->fw_dma_size;
	image_desc_info->bdata_addr = penv->bdata_dma;
	image_desc_info->bdata_size = penv->bdata_dma_size;
	mutex_unlock(&penv->fw_setup_stat_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_get_fw_image);

static ssize_t wlan_setup_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", penv->revision_id);
}

static DEVICE_ATTR(wlan_setup, S_IRUSR,
			wlan_setup_show, NULL);

static int cnss_wlan_is_codeswap_supported(u16 revision)
{
	switch (revision) {
	case QCA6174_FW_3_0:
	case QCA6174_FW_3_2:
		return 0;
	default:
		return 1;
	}
}

static int cnss_smmu_init(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
	int disable_htw = 1;
	int atomic_ctx = 1;
	int ret;

	mapping = arm_iommu_create_mapping(&platform_bus_type,
					   penv->smmu_iova_start,
					   penv->smmu_iova_len);
	if (IS_ERR(mapping)) {
		pr_err("%s: create mapping failed, err = %d\n", __func__, ret);
		ret = PTR_ERR(mapping);
		goto map_fail;
	}

	ret = iommu_domain_set_attr(mapping->domain,
			      DOMAIN_ATTR_COHERENT_HTW_DISABLE,
			      &disable_htw);
	if (ret) {
		pr_err("%s: set disable_htw attribute failed, err = %d\n",
			__func__, ret);
		goto set_attr_fail;
	}

	ret = iommu_domain_set_attr(mapping->domain,
				    DOMAIN_ATTR_ATOMIC,
				    &atomic_ctx);
	if (ret) {
		pr_err("%s: set atomic_ctx attribute failed, err = %d\n",
			__func__, ret);
		goto set_attr_fail;
	}

	ret = arm_iommu_attach_device(dev, mapping);
	if (ret) {
		pr_err("%s: attach device failed, err = %d\n", __func__, ret);
		goto attach_fail;
	}

	penv->smmu_mapping = mapping;

	return ret;

attach_fail:
set_attr_fail:
	arm_iommu_release_mapping(mapping);
map_fail:
	return ret;
}

static void cnss_smmu_remove(struct device *dev)
{
	arm_iommu_detach_device(dev);
	arm_iommu_release_mapping(penv->smmu_mapping);

	penv->smmu_mapping = NULL;
}

#ifdef CONFIG_PCI_MSM
struct pci_saved_state *cnss_pci_store_saved_state(struct pci_dev *dev)
{
	return pci_store_saved_state(dev);
}

int cnss_msm_pcie_pm_control(
		enum msm_pcie_pm_opt pm_opt, u32 bus_num,
		struct pci_dev *pdev, u32 options)
{
	return msm_pcie_pm_control(pm_opt, bus_num, pdev, NULL, options);
}

int cnss_pci_load_and_free_saved_state(
	struct pci_dev *dev, struct pci_saved_state **state)
{
	return pci_load_and_free_saved_state(dev, state);
}

int cnss_msm_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	return msm_pcie_shadow_control(dev, enable);
}

int cnss_msm_pcie_deregister_event(struct msm_pcie_register_event *reg)
{
	return msm_pcie_deregister_event(reg);
}

int cnss_msm_pcie_recover_config(struct pci_dev *dev)
{
	return msm_pcie_recover_config(dev);
}

int cnss_msm_pcie_register_event(struct msm_pcie_register_event *reg)
{
	return msm_pcie_register_event(reg);
}

int cnss_msm_pcie_enumerate(u32 rc_idx)
{
	return msm_pcie_enumerate(rc_idx);
}
#else /* !defined CONFIG_PCI_MSM */

struct pci_saved_state *cnss_pci_store_saved_state(struct pci_dev *dev)
{
	return NULL;
}

int cnss_msm_pcie_pm_control(
		enum msm_pcie_pm_opt pm_opt, u32 bus_num,
		struct pci_dev *pdev, u32 options)
{
	return -ENODEV;
}

int cnss_pci_load_and_free_saved_state(
	struct pci_dev *dev, struct pci_saved_state **state)
{
	return 0;
}

int cnss_msm_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	return -ENODEV;
}

int cnss_msm_pcie_deregister_event(struct msm_pcie_register_event *reg)
{
	return -ENODEV;
}

int cnss_msm_pcie_recover_config(struct pci_dev *dev)
{
	return -ENODEV;
}

int cnss_msm_pcie_register_event(struct msm_pcie_register_event *reg)
{
	return -ENODEV;
}

int cnss_msm_pcie_enumerate(u32 rc_idx)
{
	return -EPROBE_DEFER;
}
#endif

static void cnss_pcie_set_platform_ops(struct device *dev)
{
	struct cnss_dev_platform_ops *pf_ops = &penv->platform_ops;

	pf_ops->request_bus_bandwidth = cnss_pci_request_bus_bandwidth;
	pf_ops->get_virt_ramdump_mem = cnss_pci_get_virt_ramdump_mem;
	pf_ops->device_self_recovery = cnss_pci_device_self_recovery;
	pf_ops->schedule_recovery_work = cnss_pci_schedule_recovery_work;
	pf_ops->device_crashed = cnss_pci_device_crashed;
	pf_ops->get_wlan_mac_address = cnss_pci_get_wlan_mac_address;
	pf_ops->set_wlan_mac_address = cnss_pcie_set_wlan_mac_address;
	pf_ops->power_up = cnss_pcie_power_up;
	pf_ops->power_down = cnss_pcie_power_down;

	dev->platform_data = pf_ops;
}

static void cnss_pcie_reset_platform_ops(struct device *dev)
{
	struct cnss_dev_platform_ops *pf_ops = &penv->platform_ops;

	memset(pf_ops, 0, sizeof(struct cnss_dev_platform_ops));
	dev->platform_data = NULL;
}

static int cnss_wlan_pci_probe(struct pci_dev *pdev,
			       const struct pci_device_id *id)
{
	int ret = 0;
	struct cnss_wlan_vreg_info *vreg_info = &penv->vreg_info;
	void *cpu_addr;
	dma_addr_t dma_handle;
	struct codeswap_codeseg_info *cnss_seg_info = NULL;
	struct device *dev = &pdev->dev;

	cnss_pcie_set_platform_ops(dev);
	penv->pdev = pdev;
	penv->id = id;
	atomic_set(&penv->fw_available, 0);
	penv->device_id = pdev->device;

	if (penv->smmu_iova_len) {
		ret = cnss_smmu_init(&pdev->dev);
		if (ret) {
			pr_err("%s: SMMU init failed, err = %d\n",
				__func__, ret);
			goto smmu_init_fail;
		}
	}

	if (penv->pci_register_again) {
		pr_debug("%s: PCI re-registration complete\n", __func__);
		penv->pci_register_again = false;
		return 0;
	}

	switch (pdev->device) {
	case QCA6180_DEVICE_ID:
		pci_read_config_word(pdev, QCA6180_REV_ID_OFFSET,
				&penv->revision_id);
		break;

	case QCA6174_DEVICE_ID:
		pci_read_config_word(pdev, QCA6174_REV_ID_OFFSET,
				&penv->revision_id);
		cnss_setup_fw_files(penv->revision_id);
		break;

	default:
		pr_err("cnss: unknown device found %d\n", pdev->device);
		ret = -EPROBE_DEFER;
		goto err_unknown;
	}


	if (penv->pcie_link_state) {
		pci_save_state(pdev);
		penv->saved_state = cnss_pci_store_saved_state(pdev);

		ret = cnss_msm_pcie_pm_control(
			MSM_PCIE_SUSPEND, cnss_get_pci_dev_bus_number(pdev),
			pdev, PM_OPTIONS);
		if (ret) {
			pr_err("Failed to shutdown PCIe link\n");
			goto err_pcie_suspend;
		}
		penv->pcie_link_state = PCIE_LINK_DOWN;
	}

	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	ret = cnss_wlan_vreg_set(vreg_info, VREG_OFF);

	if (ret) {
		pr_err("can't turn off wlan vreg\n");
		goto err_pcie_suspend;
	}

	mutex_lock(&penv->fw_setup_stat_lock);
	cnss_wlan_fw_mem_alloc(pdev);
	mutex_unlock(&penv->fw_setup_stat_lock);

	ret = device_create_file(&penv->pldev->dev, &dev_attr_wlan_setup);

	if (ret) {
		pr_err("Can't Create Device file\n");
		goto err_pcie_suspend;
	}

	if (cnss_wlan_is_codeswap_supported(penv->revision_id)) {
		pr_debug("Code-swap not enabled: %d\n", penv->revision_id);
		goto err_pcie_suspend;
	}

	cpu_addr = dma_alloc_coherent(dev, EVICT_BIN_MAX_SIZE,
					&dma_handle, GFP_KERNEL);
	if (!cpu_addr || !dma_handle) {
		pr_err("cnss: Memory Alloc failed for codeswap feature\n");
		goto err_pcie_suspend;
	}

	memset(cpu_addr, 0, EVICT_BIN_MAX_SIZE);
	cnss_seg_info = devm_kzalloc(dev, sizeof(*cnss_seg_info),
							GFP_KERNEL);
	if (!cnss_seg_info) {
		pr_err("Fail to allocate memory for cnss_seg_info\n");
		goto end_dma_alloc;
	}

	memset(cnss_seg_info, 0, sizeof(*cnss_seg_info));
	cnss_seg_info->codeseg_busaddr[0]   = (void *)dma_handle;
	penv->codeseg_cpuaddr[0]            = cpu_addr;
	cnss_seg_info->codeseg_size         = EVICT_BIN_MAX_SIZE;
	cnss_seg_info->codeseg_total_bytes  = EVICT_BIN_MAX_SIZE;
	cnss_seg_info->num_codesegs         = 1;
	cnss_seg_info->codeseg_size_log2    = ilog2(EVICT_BIN_MAX_SIZE);

	penv->cnss_seg_info = cnss_seg_info;
	pr_debug("%s: Successfully allocated memory for CODESWAP\n", __func__);

	return ret;

end_dma_alloc:
	dma_free_coherent(dev, EVICT_BIN_MAX_SIZE, cpu_addr, dma_handle);
err_unknown:
err_pcie_suspend:
smmu_init_fail:
	cnss_pcie_reset_platform_ops(dev);
	return ret;
}

static void cnss_wlan_pci_remove(struct pci_dev *pdev)
{
	struct device *dev;

	if (!penv)
		return;

	dev = &penv->pldev->dev;
	cnss_pcie_reset_platform_ops(dev);
	device_remove_file(dev, &dev_attr_wlan_setup);

	if (penv->smmu_mapping)
		cnss_smmu_remove(&pdev->dev);
}

static int cnss_wlan_pci_suspend(struct device *dev)
{
	int ret = 0;
	struct cnss_wlan_driver *wdriver;
	struct pci_dev *pdev = to_pci_dev(dev);

	pm_message_t state = { .event = PM_EVENT_SUSPEND };

	if (!penv)
		goto out;

	if (!penv->pcie_link_state)
		goto out;

	wdriver = penv->driver;
	if (!wdriver)
		goto out;

	if (wdriver->suspend) {
		ret = wdriver->suspend(pdev, state);

		if (penv->pcie_link_state) {
			pci_save_state(pdev);
			penv->saved_state = cnss_pci_store_saved_state(pdev);
		}
	}
	penv->monitor_wake_intr = false;

out:
	return ret;
}

static int cnss_wlan_pci_resume(struct device *dev)
{
	int ret = 0;
	struct cnss_wlan_driver *wdriver;
	struct pci_dev *pdev = to_pci_dev(dev);

	if (!penv)
		goto out;

	if (!penv->pcie_link_state)
		goto out;

	wdriver = penv->driver;
	if (!wdriver)
		goto out;

	if (wdriver->resume && !penv->pcie_link_down_ind) {
		if (penv->saved_state)
			cnss_pci_load_and_free_saved_state(
				pdev, &penv->saved_state);
		pci_restore_state(pdev);

		ret = wdriver->resume(pdev);
	}

out:
	return ret;
}

static int cnss_wlan_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct cnss_wlan_driver *wdrv;

	if (!penv)
		return -EAGAIN;

	if (penv->pcie_link_down_ind) {
		pr_debug("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	pr_debug("cnss: runtime suspend start\n");

	wdrv = penv->driver;

	if (wdrv && wdrv->runtime_ops && wdrv->runtime_ops->runtime_suspend)
		ret = wdrv->runtime_ops->runtime_suspend(to_pci_dev(dev));

	pr_info("cnss: runtime suspend status: %d\n", ret);

	return ret;

}

static int cnss_wlan_runtime_resume(struct device *dev)
{
	struct cnss_wlan_driver *wdrv;
	int ret = 0;

	if (!penv)
		return -EAGAIN;

	if (penv->pcie_link_down_ind) {
		pr_debug("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	pr_debug("cnss: runtime resume start\n");

	wdrv = penv->driver;

	if (wdrv && wdrv->runtime_ops && wdrv->runtime_ops->runtime_resume)
		ret = wdrv->runtime_ops->runtime_resume(to_pci_dev(dev));

	pr_info("cnss: runtime resume status: %d\n", ret);

	return ret;
}

static int cnss_wlan_runtime_idle(struct device *dev)
{
	pr_debug("cnss: runtime idle\n");

	pm_request_autosuspend(dev);

	return -EBUSY;
}

static DECLARE_RWSEM(cnss_pm_sem);

static int cnss_pm_notify(struct notifier_block *b,
			unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&cnss_pm_sem);
		break;

	case PM_POST_SUSPEND:
		up_write(&cnss_pm_sem);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block cnss_pm_notifier = {
	.notifier_call = cnss_pm_notify,
};

static DEFINE_PCI_DEVICE_TABLE(cnss_wlan_pci_id_table) = {
	{ QCA6174_VENDOR_ID, QCA6174_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6174_VENDOR_ID, BEELINER_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6180_VENDOR_ID, QCA6180_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, cnss_wlan_pci_id_table);

#ifdef CONFIG_PM
static const struct dev_pm_ops cnss_wlan_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cnss_wlan_pci_suspend, cnss_wlan_pci_resume)
	SET_RUNTIME_PM_OPS(cnss_wlan_runtime_suspend, cnss_wlan_runtime_resume,
			cnss_wlan_runtime_idle)
};
#endif

struct pci_driver cnss_wlan_pci_driver = {
	.name     = "cnss_wlan_pci",
	.id_table = cnss_wlan_pci_id_table,
	.probe    = cnss_wlan_pci_probe,
	.remove   = cnss_wlan_pci_remove,
#ifdef CONFIG_PM
	.driver = {
		.pm = &cnss_wlan_pm_ops,
	},
#endif
};

static ssize_t fw_image_setup_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (!penv)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", penv->fw_image_setup);
}

static ssize_t fw_image_setup_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int ret;

	if (!penv)
		return -ENODEV;

	mutex_lock(&penv->fw_setup_stat_lock);
	pr_info("%s: Firmware setup in progress\n", __func__);

	if (kstrtoint(buf, 0, &val)) {
		mutex_unlock(&penv->fw_setup_stat_lock);
		return -EINVAL;
	}

	if (val == FW_IMAGE_FTM || val == FW_IMAGE_MISSION
	    || val == FW_IMAGE_BDATA) {
		pr_info("%s: fw image setup triggered %d\n", __func__, val);
		ret = cnss_setup_fw_image_table(val);
		if (ret != 0) {
			pr_err("%s: Invalid parsing of FW image files %d",
			       __func__, ret);
			mutex_unlock(&penv->fw_setup_stat_lock);
			return -EINVAL;
		}
		penv->fw_image_setup = val;
	} else if (val == FW_IMAGE_PRINT) {
		print_allocated_image_table();
	} else if (val == BMI_TEST_SETUP) {
		penv->bmi_test = val;
	}

	pr_info("%s: Firmware setup completed\n", __func__);
	mutex_unlock(&penv->fw_setup_stat_lock);
	return count;
}

static DEVICE_ATTR(fw_image_setup, S_IRUSR | S_IWUSR,
	fw_image_setup_show, fw_image_setup_store);

void cnss_pci_recovery_work_handler(struct work_struct *recovery)
{
	cnss_pci_device_self_recovery();
}

DECLARE_WORK(cnss_pci_recovery_work, cnss_pci_recovery_work_handler);

void cnss_schedule_recovery_work(void)
{
	schedule_work(&cnss_pci_recovery_work);
}
EXPORT_SYMBOL(cnss_schedule_recovery_work);

static inline void __cnss_disable_irq(void *data)
{
	struct pci_dev *pdev = data;

	disable_irq(pdev->irq);
}

void cnss_pci_events_cb(struct msm_pcie_notify *notify)
{
	unsigned long flags;

	if (notify == NULL)
		return;

	switch (notify->event) {

	case MSM_PCIE_EVENT_LINKDOWN:
		if (pcie_link_down_panic)
			panic("PCIe link is down!\n");

		spin_lock_irqsave(&pci_link_down_lock, flags);
		if (penv->pcie_link_down_ind) {
			pr_debug("PCI link down recovery is in progress, ignore!\n");
			spin_unlock_irqrestore(&pci_link_down_lock, flags);
			return;
		}
		penv->pcie_link_down_ind = true;
		spin_unlock_irqrestore(&pci_link_down_lock, flags);

		pr_err("PCI link down, schedule recovery\n");
		__cnss_disable_irq(notify->user);
		schedule_work(&cnss_pci_recovery_work);
		break;

	case MSM_PCIE_EVENT_WAKEUP:
		if (penv->monitor_wake_intr &&
			atomic_read(&penv->auto_suspended)) {
			penv->monitor_wake_intr = false;
			pm_request_resume(&penv->pdev->dev);
		}
		break;

	default:
		pr_err("cnss: invalid event from PCIe callback %d\n",
			notify->event);
	}
}

void cnss_wlan_pci_link_down(void)
{
	unsigned long flags;

	if (pcie_link_down_panic)
		panic("PCIe link is down!\n");

	spin_lock_irqsave(&pci_link_down_lock, flags);
	if (penv->pcie_link_down_ind) {
		pr_debug("PCI link down recovery is in progress, ignore!\n");
		spin_unlock_irqrestore(&pci_link_down_lock, flags);
		return;
	}
	penv->pcie_link_down_ind = true;
	spin_unlock_irqrestore(&pci_link_down_lock, flags);

	pr_err("PCI link down detected by host driver, schedule recovery!\n");
	schedule_work(&cnss_pci_recovery_work);
}
EXPORT_SYMBOL(cnss_wlan_pci_link_down);

int cnss_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	return cnss_msm_pcie_shadow_control(dev, enable);
}
EXPORT_SYMBOL(cnss_pcie_shadow_control);

int cnss_get_codeswap_struct(struct codeswap_codeseg_info *swap_seg)
{
	struct codeswap_codeseg_info *cnss_seg_info = penv->cnss_seg_info;

	mutex_lock(&penv->fw_setup_stat_lock);
	if (!cnss_seg_info) {
		swap_seg = NULL;
		mutex_unlock(&penv->fw_setup_stat_lock);
		return -ENOENT;
	}

	if (!atomic_read(&penv->fw_available)) {
		pr_debug("%s: fw is not available\n", __func__);
		mutex_unlock(&penv->fw_setup_stat_lock);
		return -ENOENT;
	}

	*swap_seg = *cnss_seg_info;
	mutex_unlock(&penv->fw_setup_stat_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_get_codeswap_struct);

static void cnss_wlan_memory_expansion(void)
{
	struct device *dev;
	const struct firmware *fw_entry;
	const char *filename;
	u_int32_t fw_entry_size, size_left, dma_size_left, length;
	char *fw_temp;
	char *fw_data;
	char *dma_virt_addr;
	struct codeswap_codeseg_info *cnss_seg_info;
	u_int32_t total_length = 0;
	struct pci_dev *pdev;

	mutex_lock(&penv->fw_setup_stat_lock);
	filename = cnss_wlan_get_evicted_data_file();
	pdev = penv->pdev;
	dev = &pdev->dev;
	cnss_seg_info = penv->cnss_seg_info;

	if (!cnss_seg_info) {
		pr_debug("cnss: cnss_seg_info is NULL\n");
		mutex_unlock(&penv->fw_setup_stat_lock);
		goto end;
	}

	if (atomic_read(&penv->fw_available)) {
		pr_debug("cnss: fw code already copied to host memory\n");
		mutex_unlock(&penv->fw_setup_stat_lock);
		goto end;
	}

	if (request_firmware(&fw_entry, filename, dev) != 0) {
		pr_debug("cnss: failed to get fw: %s\n", filename);
		mutex_unlock(&penv->fw_setup_stat_lock);
		goto end;
	}

	if (!fw_entry || !fw_entry->data) {
		pr_err("%s: INVALID FW entries\n", __func__);
		mutex_unlock(&penv->fw_setup_stat_lock);
		goto release_fw;
	}

	dma_virt_addr = (char *)penv->codeseg_cpuaddr[0];
	fw_data = (u8 *) fw_entry->data;
	fw_temp = fw_data;
	fw_entry_size = fw_entry->size;
	if (fw_entry_size > EVICT_BIN_MAX_SIZE)
		fw_entry_size = EVICT_BIN_MAX_SIZE;
	size_left = fw_entry_size;
	dma_size_left = EVICT_BIN_MAX_SIZE;
	while ((size_left && fw_temp) && (dma_size_left > 0)) {
		fw_temp = fw_temp + 4;
		size_left = size_left - 4;
		length = *(int *)fw_temp;
		if ((length > size_left || length <= 0) ||
			(dma_size_left <= 0 || length > dma_size_left)) {
			pr_err("cnss: wrong length read:%d\n",
							length);
			break;
		}
		fw_temp = fw_temp + 4;
		size_left = size_left - 4;
		memcpy(dma_virt_addr, fw_temp, length);
		dma_size_left = dma_size_left - length;
		size_left = size_left - length;
		fw_temp = fw_temp + length;
		dma_virt_addr = dma_virt_addr + length;
		total_length += length;
		pr_debug("cnss: bytes_left to copy: fw:%d; dma_page:%d\n",
						size_left, dma_size_left);
	}
	pr_debug("cnss: total_bytes copied: %d\n", total_length);
	cnss_seg_info->codeseg_total_bytes = total_length;

	atomic_set(&penv->fw_available, 1);
	mutex_unlock(&penv->fw_setup_stat_lock);

release_fw:
	release_firmware(fw_entry);
end:
	return;
}

/**
 * cnss_get_wlan_mac_address() - API to return MAC addresses buffer
 * @dev: struct device pointer
 * @num: buffer for number of mac addresses supported
 *
 * API returns the pointer to the buffer filled with mac addresses and
 * updates num with the number of mac addresses the buffer contains.
 *
 * Return: pointer to mac address buffer.
 */
u8 *cnss_pci_get_wlan_mac_address(uint32_t *num)
{
	struct cnss_wlan_mac_addr *addr = NULL;

	if (!penv) {
		pr_err("%s: Invalid Platform Driver Context\n", __func__);
		goto end;
	}

	if (!penv->is_wlan_mac_set) {
		pr_info("%s: Platform Driver doesn't have any mac address\n",
			__func__);
		goto end;
	}

	addr = &penv->wlan_mac_addr;
	*num = addr->no_of_mac_addr_set;
	return &addr->mac_addr[0][0];

end:
	*num = 0;
	return NULL;
}

/**
 * cnss_get_wlan_mac_address() - API to return MAC addresses buffer
 * @dev: struct device pointer
 * @num: buffer for number of mac addresses supported
 *
 * API returns the pointer to the buffer filled with mac addresses and
 * updates num with the number of mac addresses the buffer contains.
 *
 * Return: pointer to mac address buffer.
 */
u8 *cnss_get_wlan_mac_address(struct device *dev, uint32_t *num)
{
	struct cnss_wlan_mac_addr *addr = NULL;

	if (!penv) {
		pr_err("%s: Invalid Platform Driver Context\n", __func__);
		goto end;
	}

	if (!penv->is_wlan_mac_set) {
		pr_info("%s: Platform Driver doesn't have any mac address\n",
			__func__);
		goto end;
	}

	addr = &penv->wlan_mac_addr;
	*num = addr->no_of_mac_addr_set;
	return &addr->mac_addr[0][0];
end:
	*num = 0;
	return NULL;
}
EXPORT_SYMBOL(cnss_get_wlan_mac_address);

/**
* cnss_pcie_set_wlan_mac_address() - API to get two wlan mac address
* @in: Input buffer with wlan mac addresses
* @len: Size of the buffer passed
*
* API to store wlan mac address passed by the caller. The stored mac
* addresses are used by the wlan functional driver to program wlan HW.
*
* Return: kernel error code.
*/
int cnss_pcie_set_wlan_mac_address(const u8 *in, uint32_t len)
{
	uint32_t no_of_mac_addr;
	struct cnss_wlan_mac_addr *addr = NULL;
	int iter = 0;
	u8 *temp = NULL;

	if (len == 0 || (len % ETH_ALEN) != 0) {
		pr_err("%s: Invalid Length:%d\n", __func__, len);
		return -EINVAL;
	}

	no_of_mac_addr = len / ETH_ALEN;

	if (no_of_mac_addr > MAX_NO_OF_MAC_ADDR) {
		pr_err("%s: Num of supported MAC  addresses are:%d given:%d\n",
		       __func__, MAX_NO_OF_MAC_ADDR, no_of_mac_addr);
		return -EINVAL;
	}

	if (!penv) {
		pr_err("%s: Invalid CNSS Platform Context\n", __func__);
		return -ENOENT;
	}

	if (penv->is_wlan_mac_set) {
		pr_info("%s: Already MAC address are configured\n", __func__);
		return 0;
	}

	penv->is_wlan_mac_set = true;
	addr = &penv->wlan_mac_addr;
	addr->no_of_mac_addr_set = no_of_mac_addr;
	temp = &addr->mac_addr[0][0];

	for (; iter < no_of_mac_addr; ++iter, temp += ETH_ALEN, in +=
	     ETH_ALEN) {
		ether_addr_copy(temp, in);
		pr_debug("%s MAC_ADDR:%02x:%02x:%02x:%02x:%02x:%02x\n",
			 __func__, temp[0], temp[1], temp[2], temp[3], temp[4],
			 temp[5]);
	}
	return 0;
}

int cnss_wlan_register_driver(struct cnss_wlan_driver *driver)
{
	int ret = 0;
	int probe_again = 0;
	struct cnss_wlan_driver *wdrv;
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	struct pci_dev *pdev;

	if (!penv)
		return -ENODEV;

	wdrv = penv->driver;
	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;
	pdev = penv->pdev;

	if (!wdrv) {
		penv->driver = wdrv = driver;
	} else {
		pr_err("driver already registered\n");
		return -EEXIST;
	}

again:
	ret = cnss_wlan_vreg_set(vreg_info, VREG_ON);
	if (ret) {
		pr_err("wlan vreg ON failed\n");
		goto err_wlan_vreg_on;
	}

	msleep(POWER_ON_DELAY);

	if (penv->wlan_bootstrap_gpio > 0) {
		gpio_set_value(penv->wlan_bootstrap_gpio, WLAN_BOOTSTRAP_HIGH);
		msleep(WLAN_BOOTSTRAP_DELAY);
	}

	cnss_configure_wlan_en_gpio(WLAN_EN_HIGH);

	if (!pdev) {
		pr_debug("%s: invalid pdev. register pci device\n", __func__);
		ret = pci_register_driver(&cnss_wlan_pci_driver);

		if (ret) {
			pr_err("%s: pci registration failed\n", __func__);
			goto err_pcie_reg;
		}
		pdev = penv->pdev;
		if (!pdev) {
			pr_err("%s: pdev is still invalid\n", __func__);
			goto err_pcie_reg;
		}
	}

	penv->event_reg.events = MSM_PCIE_EVENT_LINKDOWN |
			MSM_PCIE_EVENT_WAKEUP;
	penv->event_reg.user = pdev;
	penv->event_reg.mode = MSM_PCIE_TRIGGER_CALLBACK;
	penv->event_reg.callback = cnss_pci_events_cb;
	penv->event_reg.options = MSM_PCIE_CONFIG_NO_RECOVERY;
	ret = cnss_msm_pcie_register_event(&penv->event_reg);
	if (ret)
		pr_err("%s: PCIe event register failed! %d\n", __func__, ret);

	if (!penv->pcie_link_state && !penv->pcie_link_down_ind) {
		ret = cnss_msm_pcie_pm_control(
			MSM_PCIE_RESUME, cnss_get_pci_dev_bus_number(pdev),
			pdev, PM_OPTIONS);
		if (ret) {
			pr_err("PCIe link bring-up failed\n");
			goto err_pcie_link_up;
		}
		penv->pcie_link_state = PCIE_LINK_UP;
	} else if (!penv->pcie_link_state && penv->pcie_link_down_ind) {

		ret = cnss_msm_pcie_pm_control(
			MSM_PCIE_RESUME, cnss_get_pci_dev_bus_number(pdev),
			pdev, PM_OPTIONS_RESUME_LINK_DOWN);

		if (ret) {
			pr_err("PCIe link bring-up failed (link down option)\n");
			goto err_pcie_link_up;
		}
		penv->pcie_link_state = PCIE_LINK_UP;

		ret = cnss_msm_pcie_recover_config(pdev);
		if (ret) {
			pr_err("cnss: PCI link failed to recover\n");
			goto err_pcie_link_up;
		}
		penv->pcie_link_down_ind = false;
	}

	if (!cnss_wlan_is_codeswap_supported(penv->revision_id))
		cnss_wlan_memory_expansion();

	if (wdrv->probe) {
		if (penv->saved_state)
			cnss_pci_load_and_free_saved_state(
				pdev, &penv->saved_state);

		pci_restore_state(pdev);

		ret = wdrv->probe(pdev, penv->id);
		if (ret) {
			wcnss_prealloc_check_memory_leak();
			wcnss_pre_alloc_reset();

			if (probe_again > 3) {
				pr_err("Failed to probe WLAN\n");
				goto err_wlan_probe;
			}
			pci_save_state(pdev);
			penv->saved_state = cnss_pci_store_saved_state(pdev);
			cnss_msm_pcie_deregister_event(&penv->event_reg);
			cnss_msm_pcie_pm_control(
				MSM_PCIE_SUSPEND,
				cnss_get_pci_dev_bus_number(pdev),
				pdev, PM_OPTIONS);
			penv->pcie_link_state = PCIE_LINK_DOWN;
			cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
			cnss_wlan_vreg_set(vreg_info, VREG_OFF);
			msleep(POWER_ON_DELAY);
			probe_again++;
			goto again;
		}
	}

	if (penv->notify_modem_status && wdrv->modem_status)
		wdrv->modem_status(pdev, penv->modem_current_status);

	return ret;

err_wlan_probe:
	pci_save_state(pdev);
	penv->saved_state = cnss_pci_store_saved_state(pdev);

err_pcie_link_up:
	cnss_msm_pcie_deregister_event(&penv->event_reg);
	if (penv->pcie_link_state) {
		cnss_msm_pcie_pm_control(
			MSM_PCIE_SUSPEND, cnss_get_pci_dev_bus_number(pdev),
			pdev, PM_OPTIONS);
		penv->pcie_link_state = PCIE_LINK_DOWN;
	}

err_pcie_reg:
	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	cnss_wlan_vreg_set(vreg_info, VREG_OFF);
	if (penv->pdev) {
		pr_err("%d: Unregistering PCI device\n", __LINE__);
		pci_unregister_driver(&cnss_wlan_pci_driver);
		penv->pdev = NULL;
		penv->pci_register_again = true;
	}

err_wlan_vreg_on:
	penv->driver = NULL;

	return ret;
}
EXPORT_SYMBOL(cnss_wlan_register_driver);

void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver)
{
	struct cnss_wlan_driver *wdrv;
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	struct pci_dev *pdev;

	if (!penv)
		return;

	wdrv = penv->driver;
	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;
	pdev = penv->pdev;

	if (!wdrv) {
		pr_err("driver not registered\n");
		return;
	}

	if (penv->bus_client)
		msm_bus_scale_client_update_request(penv->bus_client,
						    CNSS_BUS_WIDTH_NONE);

	if (!pdev) {
		pr_err("%d: invalid pdev\n", __LINE__);
		goto cut_power;
	}

	if (wdrv->remove)
		wdrv->remove(pdev);

	wcnss_prealloc_check_memory_leak();
	wcnss_pre_alloc_reset();

	cnss_msm_pcie_deregister_event(&penv->event_reg);

	if (penv->pcie_link_state && !penv->pcie_link_down_ind) {
		pci_save_state(pdev);
		penv->saved_state = cnss_pci_store_saved_state(pdev);

		if (cnss_msm_pcie_pm_control(
			MSM_PCIE_SUSPEND, cnss_get_pci_dev_bus_number(pdev),
			pdev, PM_OPTIONS)) {
			pr_err("Failed to shutdown PCIe link\n");
			return;
		}
	} else if (penv->pcie_link_state && penv->pcie_link_down_ind) {
		penv->saved_state = NULL;

		if (cnss_msm_pcie_pm_control(
			MSM_PCIE_SUSPEND, cnss_get_pci_dev_bus_number(pdev),
				pdev, PM_OPTIONS_SUSPEND_LINK_DOWN)) {
			pr_err("Failed to shutdown PCIe link (with linkdown option)\n");
			return;
		}
	}
	penv->pcie_link_state = PCIE_LINK_DOWN;
	penv->driver_status = CNSS_UNINITIALIZED;
	penv->monitor_wake_intr = false;
	atomic_set(&penv->auto_suspended, 0);

cut_power:
	penv->driver = NULL;

	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	if (cnss_wlan_vreg_set(vreg_info, VREG_OFF))
		pr_err("wlan vreg OFF failed\n");
}
EXPORT_SYMBOL(cnss_wlan_unregister_driver);

#ifdef CONFIG_PCI_MSM
int cnss_wlan_pm_control(bool vote)
{
	if (!penv || !penv->pdev)
		return -ENODEV;

	return cnss_msm_pcie_pm_control(
		vote ? MSM_PCIE_DISABLE_PC : MSM_PCIE_ENABLE_PC,
		cnss_get_pci_dev_bus_number(penv->pdev),
		penv->pdev, PM_OPTIONS);
}
EXPORT_SYMBOL(cnss_wlan_pm_control);
#endif

void cnss_lock_pm_sem(void)
{
	down_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_lock_pm_sem);

void cnss_release_pm_sem(void)
{
	up_read(&cnss_pm_sem);
}
EXPORT_SYMBOL(cnss_release_pm_sem);

void cnss_pci_schedule_recovery_work(void)
{
	schedule_work(&cnss_pci_recovery_work);
}

void *cnss_pci_get_virt_ramdump_mem(unsigned long *size)
{
	if (!penv || !penv->pldev)
		return NULL;

	*size = penv->ramdump_size;

	return penv->ramdump_addr;
}

void cnss_pci_device_crashed(void)
{
	if (penv && penv->subsys) {
		subsys_set_crash_status(penv->subsys, true);
		subsystem_restart_dev(penv->subsys);
	}
}

void *cnss_get_virt_ramdump_mem(unsigned long *size)
{
	if (!penv || !penv->pldev)
		return NULL;

	*size = penv->ramdump_size;

	return penv->ramdump_addr;
}
EXPORT_SYMBOL(cnss_get_virt_ramdump_mem);

void cnss_device_crashed(void)
{
	if (penv && penv->subsys) {
		subsys_set_crash_status(penv->subsys, true);
		subsystem_restart_dev(penv->subsys);
	}
}
EXPORT_SYMBOL(cnss_device_crashed);

static int cnss_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct cnss_wlan_driver *wdrv;
	struct pci_dev *pdev;
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	int ret = 0;

	if (!penv)
		return -ENODEV;

	penv->recovery_in_progress = true;
	wdrv = penv->driver;
	pdev = penv->pdev;
	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;

	if (!pdev) {
		ret = -EINVAL;
		goto cut_power;
	}

	if (wdrv && wdrv->shutdown)
		wdrv->shutdown(pdev);

	if (penv->pcie_link_state) {
		if (cnss_msm_pcie_pm_control(
			MSM_PCIE_SUSPEND, cnss_get_pci_dev_bus_number(pdev),
				pdev, PM_OPTIONS_SUSPEND_LINK_DOWN)) {
			pr_debug("cnss: Failed to shutdown PCIe link!\n");
			ret = -EFAULT;
		}
		penv->saved_state = NULL;
		penv->pcie_link_state = PCIE_LINK_DOWN;
	}

cut_power:
	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	if (cnss_wlan_vreg_set(vreg_info, VREG_OFF))
		pr_err("cnss: Failed to set WLAN VREG_OFF!\n");

	return ret;
}

static int cnss_powerup(const struct subsys_desc *subsys)
{
	struct cnss_wlan_driver *wdrv;
	struct pci_dev *pdev;
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	int ret = 0;

	if (!penv)
		return -ENODEV;

	if (!penv->driver)
		goto out;

	wdrv = penv->driver;
	pdev = penv->pdev;
	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;

	ret = cnss_wlan_vreg_set(vreg_info, VREG_ON);
	if (ret) {
		pr_err("cnss: Failed to set WLAN VREG_ON!\n");
		goto err_wlan_vreg_on;
	}

	msleep(POWER_ON_DELAY);
	cnss_configure_wlan_en_gpio(WLAN_EN_HIGH);
	/**
	 *  Some platforms have wifi and other PCIE card attached with PCIE
	 *  switch on the same RC like P5459 board(ROME 3.2 PCIE card + Ethernet
	 *  PCI), it will need extra time to stable the signals when do SSR,
	 *  otherwise fail to create the PCIE link, so add PCIE_SWITCH_DELAY.
	 */
	msleep(PCIE_SWITCH_DELAY);

	if (!pdev) {
		pr_err("%d: invalid pdev\n", __LINE__);
		goto err_pcie_link_up;
	}

	if (!penv->pcie_link_state) {
		ret = cnss_msm_pcie_pm_control(
			MSM_PCIE_RESUME,
			cnss_get_pci_dev_bus_number(pdev),
			pdev, PM_OPTIONS_RESUME_LINK_DOWN);

		if (ret) {
			pr_err("cnss: Failed to bring-up PCIe link!\n");
			goto err_pcie_link_up;
		}
		penv->pcie_link_state = PCIE_LINK_UP;
		ret = cnss_msm_pcie_recover_config(penv->pdev);
		if (ret) {
			pr_err("cnss: PCI link failed to recover\n");
			goto err_pcie_link_up;
		}
		penv->pcie_link_down_ind = false;
	}

	if (wdrv && wdrv->reinit) {
		if (penv->saved_state)
			cnss_pci_load_and_free_saved_state(
				pdev, &penv->saved_state);

		pci_restore_state(pdev);

		ret = wdrv->reinit(pdev, penv->id);
		if (ret) {
			pr_err("%d: Failed to do reinit\n", __LINE__);
			goto err_wlan_reinit;
		}
	} else {
		pr_err("%d: wdrv->reinit is invalid\n", __LINE__);
			goto err_pcie_link_up;
	}

	if (penv->notify_modem_status && wdrv->modem_status)
		wdrv->modem_status(pdev, penv->modem_current_status);

out:
	penv->recovery_in_progress = false;
	return ret;

err_wlan_reinit:
	pci_save_state(pdev);
	penv->saved_state = cnss_pci_store_saved_state(pdev);
	cnss_msm_pcie_pm_control(
			MSM_PCIE_SUSPEND,
			cnss_get_pci_dev_bus_number(pdev),
			pdev, PM_OPTIONS);
	penv->pcie_link_state = PCIE_LINK_DOWN;

err_pcie_link_up:
	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	cnss_wlan_vreg_set(vreg_info, VREG_OFF);
	if (penv->pdev) {
		pr_err("%d: Unregistering pci device\n", __LINE__);
		pci_unregister_driver(&cnss_wlan_pci_driver);
		penv->pdev = NULL;
		penv->pci_register_again = true;
	}

err_wlan_vreg_on:
	return ret;
}

void cnss_pci_device_self_recovery(void)
{
	if (!penv)
		return;

	if (penv->recovery_in_progress) {
		pr_err("cnss: Recovery already in progress\n");
		return;
	}

	if (penv->driver_status == CNSS_LOAD_UNLOAD) {
		pr_err("cnss: load unload in progress\n");
		return;
	}

	penv->recovery_count++;
	penv->recovery_in_progress = true;
	cnss_pm_wake_lock(&penv->ws);
	cnss_shutdown(NULL, false);
	msleep(WLAN_RECOVERY_DELAY);
	cnss_powerup(NULL);
	cnss_pm_wake_lock_release(&penv->ws);
	penv->recovery_in_progress = false;
}

static int cnss_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct ramdump_segment segment;

	if (!penv)
		return -ENODEV;

	if (!penv->ramdump_size)
		return -ENOENT;

	if (!enable)
		return 0;

	memset(&segment, 0, sizeof(segment));
	segment.v_address = penv->ramdump_addr;
	segment.size = penv->ramdump_size;

	return do_ramdump(penv->ramdump_dev, &segment, 1);
}

static void cnss_crash_shutdown(const struct subsys_desc *subsys)
{
	struct cnss_wlan_driver *wdrv;
	struct pci_dev *pdev;

	if (!penv)
		return;

	wdrv = penv->driver;
	pdev = penv->pdev;

	if (pdev && wdrv && wdrv->crash_shutdown)
		wdrv->crash_shutdown(pdev);
}

void cnss_device_self_recovery(void)
{
	if (!penv)
		return;

	if (penv->recovery_in_progress) {
		pr_err("cnss: Recovery already in progress\n");
		return;
	}
	if (penv->driver_status == CNSS_LOAD_UNLOAD) {
		pr_err("cnss: load unload in progress\n");
		return;
	}
	penv->recovery_count++;
	penv->recovery_in_progress = true;
	cnss_pm_wake_lock(&penv->ws);
	cnss_shutdown(NULL, false);
	msleep(WLAN_RECOVERY_DELAY);
	cnss_powerup(NULL);
	cnss_pm_wake_lock_release(&penv->ws);
	penv->recovery_in_progress = false;
}
EXPORT_SYMBOL(cnss_device_self_recovery);

static int cnss_modem_notifier_nb(struct notifier_block *this,
				  unsigned long code,
				  void *ss_handle)
{
	struct cnss_wlan_driver *wdrv;
	struct pci_dev *pdev;

	pr_debug("%s: Modem-Notify: event %lu\n", __func__, code);

	if (!penv)
		return NOTIFY_DONE;

	if (SUBSYS_AFTER_POWERUP == code)
		penv->modem_current_status = 1;
	else if (SUBSYS_BEFORE_SHUTDOWN == code)
		penv->modem_current_status = 0;
	else
		return NOTIFY_DONE;

	wdrv = penv->driver;
	pdev = penv->pdev;

	if (!wdrv || !pdev || !wdrv->modem_status)
		return NOTIFY_DONE;

	wdrv->modem_status(pdev, penv->modem_current_status);

	return NOTIFY_OK;
}

static struct notifier_block mnb = {
	.notifier_call = cnss_modem_notifier_nb,
};

static int cnss_init_dump_entry(void)
{
	struct msm_dump_entry dump_entry;

	if (!penv)
		return -ENODEV;

	if (!penv->ramdump_dynamic)
		return 0;

	penv->dump_data.addr = penv->ramdump_phys;
	penv->dump_data.len = penv->ramdump_size;
	penv->dump_data.version = CNSS_DUMP_FORMAT_VER;
	penv->dump_data.magic = CNSS_DUMP_MAGIC_VER_V2;
	strlcpy(penv->dump_data.name, CNSS_DUMP_NAME,
		sizeof(penv->dump_data.name));
	dump_entry.id = MSM_DUMP_DATA_CNSS_WLAN;
	dump_entry.addr = virt_to_phys(&penv->dump_data);

	return msm_dump_data_register(MSM_DUMP_TABLE_APPS, &dump_entry);
}

static int cnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct esoc_desc *desc;
	const char *client_desc;
	struct device *dev = &pdev->dev;
	u32 rc_num;
	struct resource *res;
	u32 ramdump_size = 0;
	u32 smmu_iova_address[2];

	if (penv)
		return -ENODEV;

	penv = devm_kzalloc(&pdev->dev, sizeof(*penv), GFP_KERNEL);
	if (!penv)
		return -ENOMEM;

	penv->pldev = pdev;
	penv->esoc_desc = NULL;

	penv->gpio_info.name = WLAN_EN_GPIO_NAME;
	penv->gpio_info.num = 0;
	penv->gpio_info.state = WLAN_EN_LOW;
	penv->gpio_info.init = WLAN_EN_LOW;
	penv->gpio_info.prop = false;
	penv->vreg_info.wlan_reg = NULL;
	penv->vreg_info.state = VREG_OFF;
	penv->pci_register_again = false;
	mutex_init(&penv->fw_setup_stat_lock);

	ret = cnss_wlan_get_resources(pdev);
	if (ret)
		goto err_get_wlan_res;

	ret = cnss_configure_wlan_en_gpio(WLAN_EN_HIGH);
	if (ret) {
		pr_err("%s: Failed to enable WLAN enable gpio\n", __func__);
		goto err_get_rc;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,wlan-rc-num", &rc_num);
	if (ret) {
		pr_err("%s: Failed to find PCIe RC number!\n", __func__);
		goto err_get_rc;
	}

	ret = cnss_msm_pcie_enumerate(rc_num);
	if (ret) {
		pr_err("%s: Failed to enable PCIe RC%x!\n", __func__, rc_num);
		goto err_pcie_enumerate;
	}

	penv->pcie_link_state = PCIE_LINK_UP;

	penv->notify_modem_status =
		of_property_read_bool(dev->of_node,
				      "qcom,notify-modem-status");

	if (penv->notify_modem_status) {
		ret = of_property_read_string_index(dev->of_node, "esoc-names",
						    0, &client_desc);
		if (ret) {
			pr_debug("%s: esoc-names is not defined in DT, SKIP\n",
				__func__);
		} else {
			desc = devm_register_esoc_client(dev, client_desc);
			if (IS_ERR_OR_NULL(desc)) {
				ret = PTR_RET(desc);
				pr_err("%s: can't find esoc desc\n", __func__);
				goto err_esoc_reg;
			}
			penv->esoc_desc = desc;
		}
	}

	penv->subsysdesc.name = "AR6320";
	penv->subsysdesc.owner = THIS_MODULE;
	penv->subsysdesc.shutdown = cnss_shutdown;
	penv->subsysdesc.powerup = cnss_powerup;
	penv->subsysdesc.ramdump = cnss_ramdump;
	penv->subsysdesc.crash_shutdown = cnss_crash_shutdown;
	penv->subsysdesc.dev = &pdev->dev;
	penv->subsys = subsys_register(&penv->subsysdesc);
	if (IS_ERR(penv->subsys)) {
		ret = PTR_ERR(penv->subsys);
		goto err_subsys_reg;
	}

	penv->subsys_handle = subsystem_get(penv->subsysdesc.name);

	if (of_property_read_bool(dev->of_node, "qcom,is-dual-wifi-enabled"))
		penv->dual_wifi_info.is_dual_wifi_enabled = true;

	if (of_property_read_u32(dev->of_node, "qcom,wlan-ramdump-dynamic",
				&ramdump_size) == 0) {
		penv->ramdump_addr = dma_alloc_coherent(&pdev->dev,
				ramdump_size, &penv->ramdump_phys, GFP_KERNEL);

		if (penv->ramdump_addr)
			penv->ramdump_size = ramdump_size;
		penv->ramdump_dynamic = true;
	} else {
		res = platform_get_resource_byname(penv->pldev,
				IORESOURCE_MEM, "ramdump");
		if (res) {
			penv->ramdump_phys = res->start;
			ramdump_size = resource_size(res);
			penv->ramdump_addr = ioremap(penv->ramdump_phys,
					ramdump_size);

			if (penv->ramdump_addr)
				penv->ramdump_size = ramdump_size;

			penv->ramdump_dynamic = false;
		}
	}

	pr_debug("%s: ramdump addr: %p, phys: %pa\n", __func__,
			penv->ramdump_addr, &penv->ramdump_phys);

	if (penv->ramdump_size == 0) {
		pr_info("%s: CNSS ramdump will not be collected", __func__);
		goto skip_ramdump;
	}

	ret = cnss_init_dump_entry();
	if (ret) {
		pr_err("%s: Dump table setup failed: %d\n", __func__, ret);
		goto err_ramdump_create;
	}

	penv->ramdump_dev = create_ramdump_device(penv->subsysdesc.name,
				penv->subsysdesc.dev);
	if (!penv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump_create;
	}

skip_ramdump:
	penv->modem_current_status = 0;

	if (penv->notify_modem_status) {
		penv->modem_notify_handler =
			subsys_notif_register_notifier(penv->esoc_desc ?
						       penv->esoc_desc->name :
						       "modem", &mnb);
		if (IS_ERR(penv->modem_notify_handler)) {
			ret = PTR_ERR(penv->modem_notify_handler);
			pr_err("%s: Register notifier Failed\n", __func__);
			goto err_notif_modem;
		}
	}

	if (of_property_read_u32_array(dev->of_node,
				       "qcom,wlan-smmu-iova-address",
				       smmu_iova_address, 2) == 0) {
		penv->smmu_iova_start = smmu_iova_address[0];
		penv->smmu_iova_len = smmu_iova_address[1];
	}

	ret = pci_register_driver(&cnss_wlan_pci_driver);
	if (ret)
		goto err_pci_reg;

	penv->bus_scale_table = 0;
	penv->bus_scale_table = msm_bus_cl_get_pdata(pdev);

	if (penv->bus_scale_table)  {
		penv->bus_client =
			msm_bus_scale_register_client(penv->bus_scale_table);

		if (!penv->bus_client) {
			pr_err("Failed to register with bus_scale client\n");
			goto err_bus_reg;
		}
	}
	cnss_pm_wake_lock_init(&penv->ws, "cnss_wlock");

	register_pm_notifier(&cnss_pm_notifier);

#ifdef CONFIG_CNSS_MAC_BUG
	/* 0-4K memory is reserved for QCA6174 to address a MAC HW bug.
	 * MAC would do an invalid pointer fetch based on the data
	 * that was read from 0 to 4K. So fill it with zero's (to an
	 * address for which PCIe RC honored the read without any errors).
	 */
	memset(phys_to_virt(0), 0, SZ_4K);
#endif

	ret = device_create_file(dev, &dev_attr_fw_image_setup);
	if (ret) {
		pr_err("cnss: fw_image_setup sys file creation failed\n");
		goto err_bus_reg;
	}
	pr_info("cnss: Platform driver probed successfully.\n");
	return ret;

err_bus_reg:
	if (penv->bus_scale_table)
		msm_bus_cl_clear_pdata(penv->bus_scale_table);
	pci_unregister_driver(&cnss_wlan_pci_driver);

err_pci_reg:
	if (penv->notify_modem_status)
		subsys_notif_unregister_notifier
			(penv->modem_notify_handler, &mnb);

err_notif_modem:
	if (penv->ramdump_dev)
		destroy_ramdump_device(penv->ramdump_dev);

err_ramdump_create:
	if (penv->ramdump_addr) {
		if (penv->ramdump_dynamic) {
			dma_free_coherent(&pdev->dev, penv->ramdump_size,
					penv->ramdump_addr, penv->ramdump_phys);
		} else {
			iounmap(penv->ramdump_addr);
		}
	}

	if (penv->subsys_handle)
		subsystem_put(penv->subsys_handle);

	subsys_unregister(penv->subsys);

err_subsys_reg:
	if (penv->esoc_desc)
		devm_unregister_esoc_client(&pdev->dev, penv->esoc_desc);

err_esoc_reg:
err_pcie_enumerate:
err_get_rc:
	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	cnss_wlan_release_resources();

err_get_wlan_res:
	penv = NULL;

	return ret;
}

static int cnss_remove(struct platform_device *pdev)
{
	unregister_pm_notifier(&cnss_pm_notifier);
	device_remove_file(&pdev->dev, &dev_attr_fw_image_setup);

	cnss_pm_wake_lock_destroy(&penv->ws);

	if (penv->bus_client)
		msm_bus_scale_unregister_client(penv->bus_client);

	if (penv->bus_scale_table)
		msm_bus_cl_clear_pdata(penv->bus_scale_table);

	if (penv->ramdump_addr) {
		if (penv->ramdump_dynamic) {
			dma_free_coherent(&pdev->dev, penv->ramdump_size,
					penv->ramdump_addr, penv->ramdump_phys);
		} else {
			iounmap(penv->ramdump_addr);
		}
	}

	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	if (penv->wlan_bootstrap_gpio > 0)
		gpio_set_value(penv->wlan_bootstrap_gpio, WLAN_BOOTSTRAP_LOW);
	cnss_wlan_release_resources();

	return 0;
}

static const struct of_device_id cnss_dt_match[] = {
	{.compatible = "qcom,cnss"},
	{}
};

MODULE_DEVICE_TABLE(of, cnss_dt_match);

static struct platform_driver cnss_driver = {
	.probe  = cnss_probe,
	.remove = cnss_remove,
	.driver = {
		.name = "cnss",
		.owner = THIS_MODULE,
		.of_match_table = cnss_dt_match,
#ifdef CONFIG_CNSS_ASYNC
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
	},
};

static int __init cnss_initialize(void)
{
	return platform_driver_register(&cnss_driver);
}

static void __exit cnss_exit(void)
{
	struct platform_device *pdev = penv->pldev;

	if (penv->ramdump_dev)
		destroy_ramdump_device(penv->ramdump_dev);
	if (penv->notify_modem_status)
		subsys_notif_unregister_notifier(penv->modem_notify_handler,
						&mnb);
	subsys_unregister(penv->subsys);
	if (penv->esoc_desc)
		devm_unregister_esoc_client(&pdev->dev, penv->esoc_desc);
	platform_driver_unregister(&cnss_driver);
}

void cnss_request_pm_qos_type(int latency_type, u32 qos_val)
{
	if (!penv) {
		pr_err("%s: penv is NULL!\n", __func__);
		return;
	}

	pm_qos_add_request(&penv->qos_request, latency_type, qos_val);
}
EXPORT_SYMBOL(cnss_request_pm_qos_type);

void cnss_request_pm_qos(u32 qos_val)
{
	if (!penv) {
		pr_err("%s: penv is NULL!\n", __func__);
		return;
	}

	pm_qos_add_request(&penv->qos_request, PM_QOS_CPU_DMA_LATENCY, qos_val);
}
EXPORT_SYMBOL(cnss_request_pm_qos);

void cnss_remove_pm_qos(void)
{
	if (!penv) {
		pr_err("%s: penv is NULL!\n", __func__);
		return;
	}

	pm_qos_remove_request(&penv->qos_request);
}
EXPORT_SYMBOL(cnss_remove_pm_qos);

void cnss_pci_request_pm_qos_type(int latency_type, u32 qos_val)
{
	if (!penv) {
		pr_err("%s: penv is NULL\n", __func__);
		return;
	}

	pm_qos_add_request(&penv->qos_request, latency_type, qos_val);
}
EXPORT_SYMBOL(cnss_pci_request_pm_qos_type);

void cnss_pci_request_pm_qos(u32 qos_val)
{
	if (!penv) {
		pr_err("%s: penv is NULL\n", __func__);
		return;
	}

	pm_qos_add_request(&penv->qos_request, PM_QOS_CPU_DMA_LATENCY, qos_val);
}
EXPORT_SYMBOL(cnss_pci_request_pm_qos);

void cnss_pci_remove_pm_qos(void)
{
	if (!penv) {
		pr_err("%s: penv is NULL\n", __func__);
		return;
	}

	pm_qos_remove_request(&penv->qos_request);
}
EXPORT_SYMBOL(cnss_pci_remove_pm_qos);

int cnss_pci_request_bus_bandwidth(int bandwidth)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	if (!penv->bus_client)
		return -ENOSYS;

	switch (bandwidth) {
	case CNSS_BUS_WIDTH_NONE:
	case CNSS_BUS_WIDTH_LOW:
	case CNSS_BUS_WIDTH_MEDIUM:
	case CNSS_BUS_WIDTH_HIGH:
		ret = msm_bus_scale_client_update_request(
				penv->bus_client, bandwidth);
		if (!ret) {
			penv->current_bandwidth_vote = bandwidth;
		} else {
			pr_err("%s: could not set bus bandwidth %d, ret = %d\n",
			       __func__, bandwidth, ret);
		}
		break;

	default:
		pr_err("%s: Invalid request %d", __func__, bandwidth);
		ret = -EINVAL;
	}
	return ret;
}

int cnss_request_bus_bandwidth(int bandwidth)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	if (!penv->bus_client)
		return -ENOSYS;

	switch (bandwidth) {
	case CNSS_BUS_WIDTH_NONE:
	case CNSS_BUS_WIDTH_LOW:
	case CNSS_BUS_WIDTH_MEDIUM:
	case CNSS_BUS_WIDTH_HIGH:
		ret = msm_bus_scale_client_update_request(
				penv->bus_client, bandwidth);
		if (!ret) {
			penv->current_bandwidth_vote = bandwidth;
		} else {
			pr_err("%s: could not set bus bandwidth %d, ret = %d\n",
			       __func__, bandwidth, ret);
		}
		break;

	default:
		pr_err("%s: Invalid request %d", __func__, bandwidth);
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(cnss_request_bus_bandwidth);

int cnss_get_platform_cap(struct cnss_platform_cap *cap)
{
	if (!penv)
		return -ENODEV;

	if (cap)
		*cap = penv->cap;

	return 0;
}
EXPORT_SYMBOL(cnss_get_platform_cap);

void cnss_set_driver_status(enum cnss_driver_status driver_status)
{
	penv->driver_status = driver_status;
}
EXPORT_SYMBOL(cnss_set_driver_status);

int cnss_get_bmi_setup(void)
{
	if (!penv)
		return -ENODEV;

	return penv->bmi_test;
}
EXPORT_SYMBOL(cnss_get_bmi_setup);

#ifdef CONFIG_CNSS_SECURE_FW
int cnss_get_sha_hash(const u8 *data, u32 data_len, u8 *hash_idx, u8 *out)
{
	struct scatterlist sg;
	struct hash_desc desc;
	int ret = 0;

	if (!out) {
		pr_err("memory for output buffer is not allocated\n");
		ret = -EINVAL;
		goto end;
	}

	desc.flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	desc.tfm   = crypto_alloc_hash(hash_idx, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(desc.tfm)) {
		pr_err("crypto_alloc_hash failed:%ld\n", PTR_ERR(desc.tfm));
		ret = PTR_ERR(desc.tfm);
		goto end;
	}

	sg_init_one(&sg, data, data_len);
	ret = crypto_hash_digest(&desc, &sg, sg.length, out);
	crypto_free_hash(desc.tfm);
end:
	return ret;
}
EXPORT_SYMBOL(cnss_get_sha_hash);

void *cnss_get_fw_ptr(void)
{
	if (!penv)
		return NULL;

	return penv->fw_mem;
}
EXPORT_SYMBOL(cnss_get_fw_ptr);
#endif

int cnss_auto_suspend(void)
{
	int ret = 0;
	struct pci_dev *pdev;

	if (!penv || !penv->driver)
		return -ENODEV;

	pdev = penv->pdev;

	if (penv->pcie_link_state) {
		pci_save_state(pdev);
		penv->saved_state = cnss_pci_store_saved_state(pdev);
		pci_disable_device(pdev);
		ret = pci_set_power_state(pdev, PCI_D3hot);
		if (ret)
			pr_err("%s: Set D3Hot failed: %d\n", __func__, ret);
		if (cnss_msm_pcie_pm_control(
				MSM_PCIE_SUSPEND,
				cnss_get_pci_dev_bus_number(pdev),
				pdev, PM_OPTIONS)) {
			pr_err("%s: Failed to shutdown PCIe link\n", __func__);
			ret = -EAGAIN;
			goto out;
		}
	}
	atomic_set(&penv->auto_suspended, 1);
	penv->monitor_wake_intr = true;
	penv->pcie_link_state = PCIE_LINK_DOWN;

	msm_bus_scale_client_update_request(penv->bus_client,
					    CNSS_BUS_WIDTH_NONE);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_suspend);

int cnss_auto_resume(void)
{
	int ret = 0;
	struct pci_dev *pdev;

	if (!penv || !penv->driver)
		return -ENODEV;

	pdev = penv->pdev;
	if (!penv->pcie_link_state) {
		if (cnss_msm_pcie_pm_control(
			MSM_PCIE_RESUME, cnss_get_pci_dev_bus_number(pdev),
				pdev, PM_OPTIONS)) {
			pr_err("%s: Failed to resume PCIe link\n", __func__);
			ret = -EAGAIN;
			goto out;
		}
		ret = pci_enable_device(pdev);
		if (ret)
			pr_err("%s: enable device failed: %d\n", __func__, ret);
		penv->pcie_link_state = PCIE_LINK_UP;
	}

	if (penv->saved_state)
		cnss_pci_load_and_free_saved_state(pdev, &penv->saved_state);

	pci_restore_state(pdev);
	pci_set_master(pdev);

	atomic_set(&penv->auto_suspended, 0);

	msm_bus_scale_client_update_request(penv->bus_client,
					    penv->current_bandwidth_vote);
out:
	return ret;
}
EXPORT_SYMBOL(cnss_auto_resume);

int cnss_pm_runtime_request(struct device *dev,
		enum cnss_runtime_request request)
{
	int ret = 0;

	switch (request) {
	case CNSS_PM_RUNTIME_GET:
		ret = pm_runtime_get(dev);
		break;
	case CNSS_PM_RUNTIME_PUT:
		ret = pm_runtime_put(dev);
		break;
	case CNSS_PM_RUNTIME_MARK_LAST_BUSY:
		pm_runtime_mark_last_busy(dev);
		break;
	case CNSS_PM_RUNTIME_RESUME:
		ret = pm_runtime_resume(dev);
		break;
	case CNSS_PM_RUNTIME_PUT_AUTO:
		ret = pm_runtime_put_autosuspend(dev);
		break;
	case CNSS_PM_RUNTIME_PUT_NOIDLE:
		pm_runtime_put_noidle(dev);
		break;
	case CNSS_PM_REQUEST_RESUME:
		ret = pm_request_resume(dev);
		break;
	case CNSS_PM_GET_NORESUME:
		pm_runtime_get_noresume(dev);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(cnss_pm_runtime_request);

void cnss_runtime_init(struct device *dev, int auto_delay)
{
	pm_runtime_set_autosuspend_delay(dev, auto_delay);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_allow(dev);
	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_noidle(dev);
	pm_suspend_ignore_children(dev, true);
}
EXPORT_SYMBOL(cnss_runtime_init);

void cnss_runtime_exit(struct device *dev)
{
	pm_runtime_get_noresume(dev);
	pm_runtime_set_active(dev);
}
EXPORT_SYMBOL(cnss_runtime_exit);

static void __cnss_set_pcie_monitor_intr(struct device *dev, bool val)
{
	penv->monitor_wake_intr = val;
}

static void __cnss_set_auto_suspend(struct device *dev, int val)
{
	atomic_set(&penv->auto_suspended, val);
}

static int __cnss_resume_link(struct device *dev, u32 flags)
{
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	u8 bus_num = cnss_get_pci_dev_bus_number(pdev);

	ret = cnss_msm_pcie_pm_control(MSM_PCIE_RESUME, bus_num, pdev, flags);
	if (ret)
		pr_err("%s: PCIe link resume failed with flags:%d bus_num:%d\n",
		       __func__, flags, bus_num);

	penv->pcie_link_state = PCIE_LINK_UP;

	return ret;
}

static int __cnss_suspend_link(struct device *dev, u32 flags)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	u8 bus_num = cnss_get_pci_dev_bus_number(pdev);
	int ret;

	if (!penv->pcie_link_state)
		return 0;

	ret = cnss_msm_pcie_pm_control(MSM_PCIE_SUSPEND, bus_num, pdev, flags);
	if (ret) {
		pr_err("%s: Failed to suspend link\n", __func__);
		return ret;
	}

	penv->pcie_link_state = PCIE_LINK_DOWN;

	return ret;
}

static int __cnss_pcie_recover_config(struct device *dev)
{
	int ret;

	ret = cnss_msm_pcie_recover_config(to_pci_dev(dev));
	if (ret)
		pr_err("%s: PCIe Recover config failed\n", __func__);

	return ret;
}

static int __cnss_event_reg(struct device *dev)
{
	int ret;
	struct msm_pcie_register_event *event_reg;

	event_reg = &penv->event_reg;

	event_reg->events = MSM_PCIE_EVENT_LINKDOWN |
		MSM_PCIE_EVENT_WAKEUP;
	event_reg->user = to_pci_dev(dev);
	event_reg->mode = MSM_PCIE_TRIGGER_CALLBACK;
	event_reg->callback = cnss_pci_events_cb;
	event_reg->options = MSM_PCIE_CONFIG_NO_RECOVERY;

	ret = cnss_msm_pcie_register_event(event_reg);
	if (ret)
		pr_err("%s: PCIe event register failed! %d\n", __func__, ret);

	return ret;
}

static void __cnss_event_dereg(struct device *dev)
{
	cnss_msm_pcie_deregister_event(&penv->event_reg);
}

static struct pci_dev *__cnss_get_pcie_dev(struct device *dev)
{
	int ret;
	struct pci_dev *pdev = penv->pdev;

	if (pdev)
		return pdev;

	ret = pci_register_driver(&cnss_wlan_pci_driver);
	if (ret) {
		pr_err("%s: pci re-registration failed\n", __func__);
		return NULL;
	}

	pdev = penv->pdev;

	return pdev;
}

static int __cnss_pcie_power_up(struct device *dev)
{
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	int ret;

	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;

	ret = cnss_wlan_vreg_set(vreg_info, VREG_ON);
	if (ret) {
		pr_err("%s: WLAN VREG ON Failed\n", __func__);
		return ret;
	}

	msleep(POWER_ON_DELAY);

	if (penv->wlan_bootstrap_gpio > 0) {
		gpio_set_value(penv->wlan_bootstrap_gpio, WLAN_BOOTSTRAP_HIGH);
		msleep(WLAN_BOOTSTRAP_DELAY);
	}

	cnss_configure_wlan_en_gpio(WLAN_EN_HIGH);
	return 0;
}

static int __cnss_pcie_power_down(struct device *dev)
{
	struct cnss_wlan_vreg_info *vreg_info;
	struct cnss_wlan_gpio_info *gpio_info;
	int ret;

	vreg_info = &penv->vreg_info;
	gpio_info = &penv->gpio_info;

	cnss_configure_wlan_en_gpio(WLAN_EN_LOW);
	if (penv->wlan_bootstrap_gpio > 0)
		gpio_set_value(penv->wlan_bootstrap_gpio, WLAN_BOOTSTRAP_LOW);

	ret = cnss_wlan_vreg_set(vreg_info, VREG_OFF);
	if (ret)
		pr_err("%s: Failed to turn off 3.3V regulator\n", __func__);

	return ret;
}

static int __cnss_suspend_link_state(struct device *dev)
{
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);
	int link_ind;

	if (!penv->pcie_link_state) {
		pr_debug("%s: Link is already suspended\n", __func__);
		return 0;
	}

	link_ind = penv->pcie_link_down_ind;

	if (!link_ind)
		pci_save_state(pdev);

	penv->saved_state = link_ind ? NULL : cnss_pci_store_saved_state(pdev);

	ret = link_ind ? __cnss_suspend_link(dev, PM_OPTIONS_SUSPEND_LINK_DOWN)
		: __cnss_suspend_link(dev, PM_OPTIONS);
	if (ret) {
		pr_err("%s: Link Suspend failed in state:%s\n", __func__,
		       link_ind ? "LINK_DOWN" : "LINK_ACTIVE");
		return ret;
	}

	penv->pcie_link_state = PCIE_LINK_DOWN;

	return 0;
}

static int __cnss_restore_pci_config_space(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	int ret = 0;

	if (penv->saved_state)
		ret = cnss_pci_load_and_free_saved_state(pdev,
							 &penv->saved_state);
	pci_restore_state(pdev);

	return ret;
}

static int __cnss_resume_link_state(struct device *dev)
{
	int ret;
	int link_ind;

	if (penv->pcie_link_state) {
		pr_debug("%s: Link is already in active state\n", __func__);
		return 0;
	}

	link_ind = penv->pcie_link_down_ind;

	ret = link_ind ? __cnss_resume_link(dev, PM_OPTIONS_RESUME_LINK_DOWN) :
		__cnss_resume_link(dev, PM_OPTIONS);

	if (ret) {
		pr_err("%s: Resume Link failed in link state:%s\n", __func__,
		       link_ind ? "LINK_DOWN" : "LINK_ACTIVE");
		return ret;
	}

	penv->pcie_link_state = PCIE_LINK_UP;

	ret = link_ind ?  __cnss_pcie_recover_config(dev) :
		__cnss_restore_pci_config_space(dev);

	if (ret) {
		pr_err("%s: Link Recovery Config Failed link_state:%s\n",
		       __func__, link_ind ? "LINK_DOWN" : "LINK_ACTIVE");
		penv->pcie_link_state = PCIE_LINK_DOWN;
		return ret;
	}

	penv->pcie_link_down_ind  = false;
	return ret;
}

int cnss_pcie_power_up(struct device *dev)
{
	int ret;
	struct pci_dev *pdev;

	if (!penv) {
		pr_err("%s: platform data is NULL\n", __func__);
		return -ENODEV;
	}

	ret = __cnss_pcie_power_up(dev);
	if (ret) {
		pr_err("%s: Power UP Failed\n", __func__);
		return ret;
	}

	pdev = __cnss_get_pcie_dev(dev);
	if (!pdev) {
		pr_err("%s: PCIe Dev is NULL\n", __func__);
		goto power_down;
	}

	ret = __cnss_event_reg(dev);

	if (ret)
		pr_err("%s: PCIe event registration failed\n", __func__);

	ret = __cnss_resume_link_state(dev);

	if (ret) {
		pr_err("%s: Link Bring Up Failed\n", __func__);
		goto event_dereg;
	}

	__cnss_set_pcie_monitor_intr(dev, true);

	return ret;

event_dereg:
	__cnss_event_dereg(dev);
power_down:
	__cnss_pcie_power_down(dev);
	pr_err("%s: Device Power Up Failed Fatal Error!\n", __func__);
	return ret;
}

static void __cnss_vote_bus_width(struct device *dev, uint32_t option)
{
	if (penv->bus_client)
		msm_bus_scale_client_update_request(penv->bus_client, option);
}

int cnss_pcie_power_down(struct device *dev)
{
	int ret;
	struct pci_dev *pdev = to_pci_dev(dev);

	if (!penv) {
		pr_err("%s: Invalid Platform data\n", __func__);
		return -ENODEV;
	}

	if (!pdev) {
		pr_err("%s: Invalid Pdev, Cut Power to device\n", __func__);
		__cnss_pcie_power_down(dev);
		return -ENODEV;
	}

	__cnss_vote_bus_width(dev, CNSS_BUS_WIDTH_NONE);
	__cnss_event_dereg(dev);

	ret = __cnss_suspend_link_state(dev);

	if (ret) {
		pr_err("%s: Suspend Link failed\n", __func__);
		return ret;
	}

	__cnss_set_pcie_monitor_intr(dev, false);
	__cnss_set_auto_suspend(dev, 0);

	ret = __cnss_pcie_power_down(dev);
	if (ret)
		pr_err("%s: Power Down Failed\n", __func__);

	return ret;
}

module_init(cnss_initialize);
module_exit(cnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "CNSS Driver");
