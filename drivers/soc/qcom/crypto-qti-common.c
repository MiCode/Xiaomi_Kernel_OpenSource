// SPDX-License-Identifier: GPL-2.0-only
/*
 * Common crypto library for storage encryption.
 *
 * Copyright (c) 2020-2021, Linux Foundation. All rights reserved.
 */

#include <linux/crypto-qti-common.h>
#include <linux/module.h>
#include "crypto-qti-ice-regs.h"
#include "crypto-qti-platform.h"

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
#include <linux/of.h>
#include <linux/blkdev.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>

#define CRYPTO_ICE_TYPE_NAME_LEN	8
#define CRYPTO_ICE_ENCRYPT		0x1
#define CRYPTO_ICE_DECRYPT		0x2
#define CRYPTO_SECT_LEN_IN_BYTE		512
#define CRYPTO_ICE_CXT_FDE		1
#define CRYPTO_ICE_FDE_KEY_INDEX	31
#define CRYPTO_UD_VOLNAME		"userdata"
#endif //CONFIG_QTI_CRYPTO_FDE

static int ice_check_fuse_setting(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;
	uint32_t major, minor;

	major = (ice_entry->ice_hw_version & ICE_CORE_MAJOR_REV_MASK) >>
			ICE_CORE_MAJOR_REV;
	minor = (ice_entry->ice_hw_version & ICE_CORE_MINOR_REV_MASK) >>
			ICE_CORE_MINOR_REV;

	//Check fuse setting is not supported on ICE 3.2 onwards
	if ((major == 0x03) && (minor >= 0x02))
		return 0;
	regval = ice_readl(ice_entry, ICE_REGS_FUSE_SETTING);
	regval &= (ICE_FUSE_SETTING_MASK |
		ICE_FORCE_HW_KEY0_SETTING_MASK |
		ICE_FORCE_HW_KEY1_SETTING_MASK);

	if (regval) {
		pr_err("%s: error: ICE_ERROR_HW_DISABLE_FUSE_BLOWN\n",
				__func__);
		return -EPERM;
	}
	return 0;
}

static int ice_check_version(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t version, major, minor, step;

	version = ice_readl(ice_entry, ICE_REGS_VERSION);
	major = (version & ICE_CORE_MAJOR_REV_MASK) >> ICE_CORE_MAJOR_REV;
	minor = (version & ICE_CORE_MINOR_REV_MASK) >> ICE_CORE_MINOR_REV;
	step = (version & ICE_CORE_STEP_REV_MASK) >> ICE_CORE_STEP_REV;

	if (major < ICE_CORE_CURRENT_MAJOR_VERSION) {
		pr_err("%s: Unknown ICE device at %lu, rev %d.%d.%d\n",
			__func__, (unsigned long)ice_entry->icemmio_base,
				major, minor, step);
		return -ENODEV;
	}

	ice_entry->ice_hw_version = version;

	return 0;
}

int crypto_qti_init_crypto(struct device *dev, void __iomem *mmio_base,
			   void __iomem *hwkm_slave_mmio_base, void **priv_data)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = devm_kzalloc(dev,
		sizeof(struct crypto_vops_qti_entry),
		GFP_KERNEL);
	if (!ice_entry)
		return -ENOMEM;

	ice_entry->icemmio_base = mmio_base;
	ice_entry->hwkm_slave_mmio_base = hwkm_slave_mmio_base;
	ice_entry->flags = 0;

	err = ice_check_version(ice_entry);
	if (err) {
		pr_err("%s: check version failed, err %d\n", __func__, err);
		return err;
	}

	err = ice_check_fuse_setting(ice_entry);
	if (err)
		return err;

	*priv_data = (void *)ice_entry;

	return err;
}
EXPORT_SYMBOL(crypto_qti_init_crypto);

static void ice_low_power_and_optimization_enable(
		struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;

	regval = ice_readl(ice_entry, ICE_REGS_ADVANCED_CONTROL);
	/* Enable low power mode sequence
	 * [0]-0,[1]-0,[2]-0,[3]-7,[4]-0,[5]-0,[6]-0,[7]-0,
	 * Enable CONFIG_CLK_GATING, STREAM2_CLK_GATING and STREAM1_CLK_GATING
	 */
	regval |= 0x7000;
	/* Optimization enable sequence
	 */
	regval |= 0xD807100;
	ice_writel(ice_entry, regval, ICE_REGS_ADVANCED_CONTROL);
	/*
	 * Memory barrier - to ensure write completion before next transaction
	 */
	wmb();
}

static int ice_wait_bist_status(struct crypto_vops_qti_entry *ice_entry)
{
	int count;
	uint32_t regval;

	for (count = 0; count < QTI_ICE_MAX_BIST_CHECK_COUNT; count++) {
		regval = ice_readl(ice_entry, ICE_REGS_BIST_STATUS);
		if (!(regval & ICE_BIST_STATUS_MASK))
			break;
		udelay(50);
	}

	if (regval) {
		pr_err("%s: wait bist status failed, reg %d\n",
				__func__, regval);
		return -ETIMEDOUT;
	}

	return 0;
}

static void ice_enable_intr(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;

	regval = ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_MASK);
	regval &= ~ICE_REGS_NON_SEC_IRQ_MASK;
	ice_writel(ice_entry, regval, ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Memory barrier - to ensure write completion before next transaction
	 */
	wmb();
}

static void ice_disable_intr(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;

	regval = ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_MASK);
	regval |= ICE_REGS_NON_SEC_IRQ_MASK;
	ice_writel(ice_entry, regval, ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Memory barrier - to ensure write completion before next transaction
	 */
	wmb();
}

int crypto_qti_enable(void *priv_data)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	ice_low_power_and_optimization_enable(ice_entry);
	err = ice_wait_bist_status(ice_entry);
	if (err)
		return err;
	ice_enable_intr(ice_entry);

	return err;
}
EXPORT_SYMBOL(crypto_qti_enable);

void crypto_qti_disable(void *priv_data)
{
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return;
	}

	crypto_qti_disable_platform(ice_entry);
	ice_disable_intr(ice_entry);
}
EXPORT_SYMBOL(crypto_qti_disable);

int crypto_qti_resume(void *priv_data)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	err = ice_wait_bist_status(ice_entry);

	return err;
}
EXPORT_SYMBOL(crypto_qti_resume);

static void ice_dump_test_bus(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval = 0x1;
	uint32_t val;
	uint8_t bus_selector;
	uint8_t stream_selector;

	pr_err("ICE TEST BUS DUMP:\n");

	for (bus_selector = 0; bus_selector <= 0xF;  bus_selector++) {
		regval = 0x1;	/* enable test bus */
		regval |= bus_selector << 28;
		if (bus_selector == 0xD)
			continue;
		ice_writel(ice_entry, regval, ICE_REGS_TEST_BUS_CONTROL);
		/*
		 * make sure test bus selector is written before reading
		 * the test bus register
		 */
		wmb();
		val = ice_readl(ice_entry, ICE_REGS_TEST_BUS_REG);
		pr_err("ICE_TEST_BUS_CONTROL: 0x%08x | ICE_TEST_BUS_REG: 0x%08x\n",
			regval, val);
	}

	pr_err("ICE TEST BUS DUMP (ICE_STREAM1_DATAPATH_TEST_BUS):\n");
	for (stream_selector = 0; stream_selector <= 0xF; stream_selector++) {
		regval = 0xD0000001;	/* enable stream test bus */
		regval |= stream_selector << 16;
		ice_writel(ice_entry, regval, ICE_REGS_TEST_BUS_CONTROL);
		/*
		 * make sure test bus selector is written before reading
		 * the test bus register
		 */
		wmb();
		val = ice_readl(ice_entry, ICE_REGS_TEST_BUS_REG);
		pr_err("ICE_TEST_BUS_CONTROL: 0x%08x | ICE_TEST_BUS_REG: 0x%08x\n",
			regval, val);
	}
}


int crypto_qti_debug(void *priv_data)
{
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	pr_err("%s: ICE Control: 0x%08x | ICE Reset: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_CONTROL),
		ice_readl(ice_entry, ICE_REGS_RESET));

	pr_err("%s: ICE Version: 0x%08x | ICE FUSE:	0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_VERSION),
		ice_readl(ice_entry, ICE_REGS_FUSE_SETTING));

	pr_err("%s: ICE Param1: 0x%08x | ICE Param2:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_1),
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_2));

	pr_err("%s: ICE Param3: 0x%08x | ICE Param4:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_3),
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_4));

	pr_err("%s: ICE Param5: 0x%08x | ICE IRQ STTS:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_5),
		ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_STTS));

	pr_err("%s: ICE IRQ MASK: 0x%08x | ICE IRQ CLR:	0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_MASK),
		ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_CLR));

	pr_err("%s: ICE INVALID CCFG ERR STTS: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_INVALID_CCFG_ERR_STTS));

	pr_err("%s: ICE BIST Sts: 0x%08x | ICE Bypass Sts:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_BIST_STATUS),
		ice_readl(ice_entry, ICE_REGS_BYPASS_STATUS));

	pr_err("%s: ICE ADV CTRL: 0x%08x | ICE ENDIAN SWAP:	0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_ADVANCED_CONTROL),
		ice_readl(ice_entry, ICE_REGS_ENDIAN_SWAP));

	pr_err("%s: ICE_STM1_ERR_SYND1: 0x%08x | ICE_STM1_ERR_SYND2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_ERROR_SYNDROME1),
		ice_readl(ice_entry, ICE_REGS_STREAM1_ERROR_SYNDROME2));

	pr_err("%s: ICE_STM2_ERR_SYND1: 0x%08x | ICE_STM2_ERR_SYND2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_ERROR_SYNDROME1),
		ice_readl(ice_entry, ICE_REGS_STREAM2_ERROR_SYNDROME2));

	pr_err("%s: ICE_STM1_COUNTER1: 0x%08x | ICE_STM1_COUNTER2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS1),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS2));

	pr_err("%s: ICE_STM1_COUNTER3: 0x%08x | ICE_STM1_COUNTER4: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS3),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS4));

	pr_err("%s: ICE_STM2_COUNTER1: 0x%08x | ICE_STM2_COUNTER2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS1),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS2));

	pr_err("%s: ICE_STM2_COUNTER3: 0x%08x | ICE_STM2_COUNTER4: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS3),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS4));

	pr_err("%s: ICE_STM1_CTR5_MSB: 0x%08x | ICE_STM1_CTR5_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS5_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS5_LSB));

	pr_err("%s: ICE_STM1_CTR6_MSB: 0x%08x | ICE_STM1_CTR6_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS6_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS6_LSB));

	pr_err("%s: ICE_STM1_CTR7_MSB: 0x%08x | ICE_STM1_CTR7_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS7_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS7_LSB));

	pr_err("%s: ICE_STM1_CTR8_MSB: 0x%08x | ICE_STM1_CTR8_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS8_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS8_LSB));

	pr_err("%s: ICE_STM1_CTR9_MSB: 0x%08x | ICE_STM1_CTR9_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS9_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS9_LSB));

	pr_err("%s: ICE_STM2_CTR5_MSB: 0x%08x | ICE_STM2_CTR5_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS5_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS5_LSB));

	pr_err("%s: ICE_STM2_CTR6_MSB: 0x%08x | ICE_STM2_CTR6_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS6_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS6_LSB));

	pr_err("%s: ICE_STM2_CTR7_MSB: 0x%08x | ICE_STM2_CTR7_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS7_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS7_LSB));

	pr_err("%s: ICE_STM2_CTR8_MSB: 0x%08x | ICE_STM2_CTR8_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS8_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS8_LSB));

	pr_err("%s: ICE_STM2_CTR9_MSB: 0x%08x | ICE_STM2_CTR9_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS9_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS9_LSB));

	ice_dump_test_bus(ice_entry);

	return 0;
}
EXPORT_SYMBOL(crypto_qti_debug);

int crypto_qti_keyslot_program(void *priv_data,
			       const struct blk_crypto_key *key,
			       unsigned int slot,
			       u8 data_unit_mask, int capid)
{
	int err1 = 0, err2 = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	err1 = crypto_qti_program_key(ice_entry, key, slot,
				data_unit_mask, capid);
	if (err1) {
		pr_err("%s: program key failed with error %d\n",
			__func__, err1);
		err2 = crypto_qti_invalidate_key(ice_entry, slot);
		if (err2) {
			pr_err("%s: invalidate key failed with error %d\n",
				__func__, err2);
		}
	}

	return err1;
}
EXPORT_SYMBOL(crypto_qti_keyslot_program);

int crypto_qti_keyslot_evict(void *priv_data, unsigned int slot)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	err = crypto_qti_invalidate_key(ice_entry, slot);
	if (err) {
		pr_err("%s: invalidate key failed with error %d\n",
			__func__, err);
		return err;
	}

	return err;
}
EXPORT_SYMBOL(crypto_qti_keyslot_evict);

int crypto_qti_derive_raw_secret(void *priv_data,
				 const u8 *wrapped_key,
				 unsigned int wrapped_key_size, u8 *secret,
				 unsigned int secret_size)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	if (wrapped_key_size <= RAW_SECRET_SIZE) {
		pr_err("%s: Invalid wrapped_key_size: %u\n",
				__func__, wrapped_key_size);
		err = -EINVAL;
		return err;
	}
	if (secret_size != RAW_SECRET_SIZE) {
		pr_err("%s: Invalid secret size: %u\n", __func__, secret_size);
		err = -EINVAL;
		return err;
	}

	return crypto_qti_derive_raw_secret_platform(ice_entry,
				wrapped_key, wrapped_key_size,
				secret, secret_size);
}
EXPORT_SYMBOL(crypto_qti_derive_raw_secret);

#if IS_ENABLED(CONFIG_QTI_CRYPTO_FDE)
static int ice_fde_flag;
struct ice_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 max_freq;
	u32 min_freq;
	u32 curr_freq;
	bool enabled;
};

static LIST_HEAD(ice_devices);
/*
 * ICE HW device structure.
 */
struct ice_device {
	struct list_head	list;
	struct device		*pdev;
	dev_t			device_no;
	void __iomem		*mmio;
	int			irq;
	bool			is_ice_enabled;
	ice_error_cb		error_cb;
	void			*host_controller_data; /* UFS/EMMC/other? */
	struct list_head	clk_list_head;
	u32			ice_hw_version;
	bool			is_ice_clk_available;
	char			ice_instance_type[CRYPTO_ICE_TYPE_NAME_LEN];
	struct regulator	*reg;
	bool			is_regulator_available;
};

static int crypto_qti_ice_init(struct ice_device *ice_dev, void *host_controller_data,
							   ice_error_cb error_cb);

static int crypto_qti_ice_get_vreg(struct ice_device *ice_dev)
{
	int ret = 0;

	if (!ice_dev->is_regulator_available)
		return 0;

	if (ice_dev->reg)
		return 0;

	ice_dev->reg = devm_regulator_get(ice_dev->pdev, "vdd-hba");
	if (IS_ERR(ice_dev->reg)) {
		ret = PTR_ERR(ice_dev->reg);
		dev_err(ice_dev->pdev, "%s: %s get failed, err=%d\n",
			__func__, "vdd-hba-supply", ret);
	}
	return ret;
}

static int crypto_qti_ice_setting_config(struct request *req,
				  struct ice_crypto_setting *crypto_data,
				  struct ice_data_setting *setting, uint32_t cxt)
{
	if (!setting)
		return -EINVAL;

	if ((short)(crypto_data->key_index) >= 0) {
		memcpy(&setting->crypto_data, crypto_data,
				sizeof(setting->crypto_data));

		if (rq_data_dir(req) == WRITE) {
			if (((cxt == CRYPTO_ICE_CXT_FDE) &&
				(ice_fde_flag & CRYPTO_ICE_ENCRYPT)))
				setting->encr_bypass = false;
		} else if (rq_data_dir(req) == READ) {
			if (((cxt == CRYPTO_ICE_CXT_FDE) &&
				(ice_fde_flag & CRYPTO_ICE_DECRYPT)))
				setting->decr_bypass = false;
		} else {
			/* Should I say BUG_ON */
			setting->encr_bypass = true;
			setting->decr_bypass = true;
		}
	}

	return 0;
}

static void crypto_qti_ice_disable_intr(struct ice_device *ice_dev)
{
	unsigned int reg;

	reg = crypto_qti_ice_readl(ice_dev, ICE_REGS_NON_SEC_IRQ_MASK);
	reg |= ICE_NON_SEC_IRQ_MASK;
	crypto_qti_ice_writel(ice_dev, reg, ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Ensure previous instructions was completed before issuing next
	 * ICE initialization/optimization instruction
	 */
	mb();
}

static void crypto_qti_ice_parse_ice_instance_type(struct platform_device *pdev,
					     struct ice_device *ice_dev)
{
	int ret = -1;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	const char *type;

	ret = of_property_read_string_index(np, "qcom,instance-type", 0, &type);
	if (ret) {
		pr_err("%s: Could not get ICE instance type\n", __func__);
		goto out;
	}
	strlcpy(ice_dev->ice_instance_type, type, CRYPTO_ICE_TYPE_NAME_LEN);
out:
	return;
}

static int crypto_qti_ice_parse_clock_info(struct platform_device *pdev, struct ice_device *ice_dev)
{
	int ret = -1, cnt, i, len;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	char *name;
	struct ice_clk_info *clki;
	u32 *clkfreq = NULL;

	if (!np)
		goto out;

	cnt = of_property_count_strings(np, "clock-names");
	if (cnt <= 0) {
		dev_info(dev, "%s: Unable to find clocks, assuming enabled\n",
			 __func__);
		ret = cnt;
		goto out;
	}

	if (!of_get_property(np, "qcom,op-freq-hz", &len)) {
		dev_info(dev, "qcom,op-freq-hz property not specified\n");
		goto out;
	}

	len = len/sizeof(*clkfreq);
	if (len != cnt)
		goto out;

	clkfreq = devm_kzalloc(dev, len * sizeof(*clkfreq), GFP_KERNEL);
	if (!clkfreq) {
		ret = -ENOMEM;
		goto out;
	}
	ret = of_property_read_u32_array(np, "qcom,op-freq-hz", clkfreq, len);

	INIT_LIST_HEAD(&ice_dev->clk_list_head);

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(np,
				"clock-names", i, (const char **)&name);
		if (ret)
			goto out;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki) {
			ret = -ENOMEM;
			goto out;
		}
		clki->max_freq = clkfreq[i];
		clki->name = kstrdup(name, GFP_KERNEL);
		list_add_tail(&clki->list, &ice_dev->clk_list_head);
	}
out:
	return ret;
}

static int crypto_qti_ice_get_dts_data(struct platform_device *pdev, struct ice_device *ice_dev)
{
	int rc = -1;

	ice_dev->mmio = NULL;
	if (!of_parse_phandle(pdev->dev.of_node, "vdd-hba-supply", 0)) {
		pr_err("%s: No vdd-hba-supply regulator, assuming not needed\n",
								 __func__);
		ice_dev->is_regulator_available = false;
	} else {
		ice_dev->is_regulator_available = true;
	}
	ice_dev->is_ice_clk_available = of_property_read_bool(
						(&pdev->dev)->of_node,
						"qcom,enable-ice-clk");

	if (ice_dev->is_ice_clk_available) {
		rc = crypto_qti_ice_parse_clock_info(pdev, ice_dev);
		if (rc) {
			pr_err("%s: crypto_qti_ice_parse_clock_info failed (%d)\n",
				__func__, rc);
			goto err_dev;
		}
	}

	crypto_qti_ice_parse_ice_instance_type(pdev, ice_dev);

	return 0;
err_dev:
	return rc;
}

/*
 * ICE HW instance can exist in UFS or eMMC based storage HW
 * Userspace does not know what kind of ICE it is dealing with.
 * Though userspace can find which storage device it is booting
 * from but all kind of storage types dont support ICE from
 * beginning. So ICE device is created for user space to ping
 * if ICE exist for that kind of storage
 */
static const struct file_operations crypto_qti_ice_fops = {
	.owner = THIS_MODULE,
};



static int crypto_qti_ice_probe(struct platform_device *pdev)
{
	struct ice_device *ice_dev;
	int rc = 0;

	if (!pdev) {
		pr_err("%s: Invalid platform_device passed\n",
			__func__);
		return -EINVAL;
	}

	ice_dev = kzalloc(sizeof(struct ice_device), GFP_KERNEL);

	if (!ice_dev) {
		rc = -ENOMEM;
		pr_err("%s: Error %d allocating memory for ICE device:\n",
			__func__, rc);
		goto out;
	}

	ice_dev->pdev = &pdev->dev;
	if (!ice_dev->pdev) {
		rc = -EINVAL;
		pr_err("%s: Invalid device passed in platform_device\n",
								__func__);
		goto err_ice_dev;
	}

	if (pdev->dev.of_node)
		rc = crypto_qti_ice_get_dts_data(pdev, ice_dev);
	else {
		rc = -EINVAL;
		pr_err("%s: ICE device node not found\n", __func__);
	}

	if (rc)
		goto err_ice_dev;

	/*
	 * If ICE is enabled here, it would be waste of power.
	 * We would enable ICE when first request for crypto
	 * operation arrives.
	 */
	rc = crypto_qti_ice_init(ice_dev, NULL, NULL);
	if (rc) {
		pr_err("ice_init failed.\n");
		goto err_ice_dev;
	}
	ice_dev->is_ice_enabled = true;
	platform_set_drvdata(pdev, ice_dev);
	list_add_tail(&ice_dev->list, &ice_devices);

	goto out;

err_ice_dev:
	kfree(ice_dev);
out:
	return rc;
}

static int crypto_qti_ice_remove(struct platform_device *pdev)
{
	struct ice_device *ice_dev;

	ice_dev = (struct ice_device *)platform_get_drvdata(pdev);

	if (!ice_dev)
		return 0;

	crypto_qti_ice_disable_intr(ice_dev);

	device_init_wakeup(&pdev->dev, false);
	if (ice_dev->mmio)
		iounmap(ice_dev->mmio);

	list_del_init(&ice_dev->list);
	kfree(ice_dev);

	return 1;
}


int crypto_qti_ice_config_start(struct request *req, struct ice_data_setting *setting)
{
	struct ice_crypto_setting ice_data = {0};
	unsigned long sec_end = 0;
	sector_t data_size;

	ice_data.key_index = CRYPTO_ICE_FDE_KEY_INDEX;

	if (!req) {
		pr_err("%s: Invalid params passed\n", __func__);
		return -EINVAL;
	}

	/*
	 * It is not an error to have a request with no  bio
	 * Such requests must bypass ICE. So first set bypass and then
	 * return if bio is not available in request
	 */
	if (setting) {
		setting->encr_bypass = true;
		setting->decr_bypass = true;
	}

	if (!req->bio) {
		/* It is not an error to have a request with no  bio */
		return 0;
	}

	if (ice_fde_flag && req->part && req->part->info
				&& req->part->info->volname[0]) {
		if (!strcmp(req->part->info->volname, CRYPTO_UD_VOLNAME)) {
			sec_end = req->part->start_sect + req->part->nr_sects;
			if ((req->__sector >= req->part->start_sect) &&
				(req->__sector < sec_end)) {
				/*
				 * Ugly hack to address non-block-size aligned
				 * userdata end address in eMMC based devices.
				 * for eMMC based devices, since sector and
				 * block sizes are not same i.e. 4K, it is
				 * possible that partition is not a multiple of
				 * block size. For UFS based devices sector
				 * size and block size are same. Hence ensure
				 * that data is within userdata partition using
				 * sector based calculation
				 */
				data_size = req->__data_len /
						CRYPTO_SECT_LEN_IN_BYTE;

				if ((req->__sector + data_size) > sec_end)
					return 0;
				else
					return crypto_qti_ice_setting_config(req,
						&ice_data, setting,
						CRYPTO_ICE_CXT_FDE);
			}
		}
	}

	/*
	 * It is not an error. If target is not req-crypt based, all request
	 * from storage driver would come here to check if there is any ICE
	 * setting required
	 */
	return 0;
}
EXPORT_SYMBOL(crypto_qti_ice_config_start);

void crypto_qti_ice_set_fde_flag(int flag)
{
	ice_fde_flag = flag;
	pr_debug("%s flag = %d\n", __func__, ice_fde_flag);
}
EXPORT_SYMBOL(crypto_qti_ice_set_fde_flag);

/* Following struct is required to match device with driver from dts file */

static const struct of_device_id crypto_qti_ice_match[] = {
	{ .compatible = "qcom,ice" },
	{},
};
MODULE_DEVICE_TABLE(of, crypto_qti_ice_match);

static int crypto_qti_ice_enable_clocks(struct ice_device *ice, bool enable)
{
	int ret = 0;
	struct ice_clk_info *clki = NULL;
	struct device *dev = ice->pdev;
	struct list_head *head = &ice->clk_list_head;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s:ICE Clock list null/empty\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!ice->is_ice_clk_available) {
		dev_err(dev, "%s:ICE Clock not available\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		if (enable)
			ret = clk_prepare_enable(clki->clk);
		else
			clk_disable_unprepare(clki->clk);

		if (ret) {
			dev_err(dev, "Unable to %s ICE core clk\n",
				enable?"enable":"disable");
			goto out;
		}
	}
out:
	return ret;
}

static struct ice_device *crypto_qti_get_ice_device_from_storage_type
					(const char *storage_type)
{
	struct ice_device *ice_dev = NULL;

	if (list_empty(&ice_devices)) {
		pr_err("%s: invalid device list\n", __func__);
		ice_dev = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	list_for_each_entry(ice_dev, &ice_devices, list) {
		if (!strcmp(ice_dev->ice_instance_type, storage_type)) {
			pr_debug("%s: ice device %pK\n", __func__, ice_dev);
			return ice_dev;
		}
	}
out:
	return NULL;
}


static int crypto_qti_ice_enable_setup(struct ice_device *ice_dev)
{
	int ret = -1;

	/* Setup Regulator */
	if (ice_dev->is_regulator_available) {
		if (crypto_qti_ice_get_vreg(ice_dev)) {
			pr_err("%s: Could not get regulator\n", __func__);
			goto out;
		}
		ret = regulator_enable(ice_dev->reg);
		if (ret) {
			pr_err("%s:%pK: Could not enable regulator\n",
					__func__, ice_dev);
			goto out;
		}
	}

	/* Setup Clocks */
	if (crypto_qti_ice_enable_clocks(ice_dev, true)) {
		pr_err("%s:%pK:%s Could not enable clocks\n", __func__,
				ice_dev, ice_dev->ice_instance_type);
		goto out_reg;
	}

	return ret;

out_reg:
	if (ice_dev->is_regulator_available) {
		if (crypto_qti_ice_get_vreg(ice_dev)) {
			pr_err("%s: Could not get regulator\n", __func__);
			goto out;
		}
		ret = regulator_disable(ice_dev->reg);
		if (ret) {
			pr_err("%s:%pK: Could not disable regulator\n",
					__func__, ice_dev);
			goto out;
		}
	}
out:
	return ret;
}

static int crypto_qti_ice_disable_setup(struct ice_device *ice_dev)
{
	int ret = 0;

	/* Setup Clocks */
	if (crypto_qti_ice_enable_clocks(ice_dev, false))
		pr_err("%s:%pK:%s Could not disable clocks\n", __func__,
				ice_dev, ice_dev->ice_instance_type);

	/* Setup Regulator */
	if (ice_dev->is_regulator_available) {
		if (crypto_qti_ice_get_vreg(ice_dev)) {
			pr_err("%s: Could not get regulator\n", __func__);
			goto out;
		}
		ret = regulator_disable(ice_dev->reg);
		if (ret) {
			pr_err("%s:%pK: Could not disable regulator\n",
					__func__, ice_dev);
			goto out;
		}
	}
out:
	return ret;
}


static int crypto_qti_ice_init_clocks(struct ice_device *ice)
{
	int ret = -EINVAL;
	struct ice_clk_info *clki = NULL;
	struct device *dev = ice->pdev;
	struct list_head *head = &ice->clk_list_head;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s:ICE Clock list null/empty\n", __func__);
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		/* Not all clocks would have a rate to be set */
		ret = 0;
		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(dev,
				"%s: %s clk set rate(%dHz) failed, %d\n",
						__func__, clki->name,
				clki->max_freq, ret);
				goto out;
			}
			clki->curr_freq = clki->max_freq;
			dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
		}
	}
out:
	return ret;
}

static int crypto_qti_ice_finish_init(struct ice_device *ice_dev)
{
	int err = 0;

	if (!ice_dev) {
		pr_err("%s: Null data received\n", __func__);
		err = -ENODEV;
		goto out;
	}

	if (ice_dev->is_ice_clk_available) {
		err = crypto_qti_ice_init_clocks(ice_dev);
		if (err)
			goto out;
	}
out:
	return err;
}

static int crypto_qti_ice_init(struct ice_device *ice_dev,
							   void *host_controller_data,
							   ice_error_cb error_cb)
{
	/*
	 * A completion event for host controller would be triggered upon
	 * initialization completion
	 * When ICE is initialized, it would put ICE into Global Bypass mode
	 * When any request for data transfer is received, it would enable
	 * the ICE for that particular request
	 */

	ice_dev->error_cb = error_cb;
	ice_dev->host_controller_data = host_controller_data;

	return crypto_qti_ice_finish_init(ice_dev);
}


int crypto_qti_ice_setup_ice_hw(const char *storage_type, int enable)
{
	int ret = -1;
	struct ice_device *ice_dev = NULL;

	ice_dev = crypto_qti_get_ice_device_from_storage_type(storage_type);
	if (ice_dev == ERR_PTR(-EPROBE_DEFER))
		return -EPROBE_DEFER;

	if (!ice_dev || !ice_dev->is_ice_enabled)
		return ret;
	if (enable)
		return crypto_qti_ice_enable_setup(ice_dev);
	else
		return crypto_qti_ice_disable_setup(ice_dev);
}
EXPORT_SYMBOL(crypto_qti_ice_setup_ice_hw);

static struct platform_driver crypto_qti_ice_driver = {
	.probe          = crypto_qti_ice_probe,
	.remove         = crypto_qti_ice_remove,
	.driver         = {
		.name   = "qcom_ice",
		.of_match_table = crypto_qti_ice_match,
	},
};
module_platform_driver(crypto_qti_ice_driver);
#endif //CONFIG_QTI_CRYPTO_FDE

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Common crypto library for storage encryption");
