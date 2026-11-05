// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm ICE (Inline Crypto Engine) support.
 *
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2019, Google LLC
 * Copyright (c) 2023, Linaro Limited
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/firmware/qcom/qcom_scm.h>

#include <soc/qcom/ice.h>
#include <linux/qtee_shmbridge.h>

#define AES_256_XTS_KEY_SIZE			64

/*
 * Wrapped key sizes from HWKm is different for different versions of
 * HW. It is not expected to change again in the future.
 */
#define QCOM_ICE_HWKM_WRAPPED_KEY_SIZE(v)	\
	((v) == 1 ? 68 : 100)

/* QCOM ICE registers */
#define QCOM_ICE_REG_VERSION			0x0008
#define QCOM_ICE_REG_FUSE_SETTING		0x0010
#define QCOM_ICE_REG_BIST_STATUS		0x0070
#define QCOM_ICE_REG_ADVANCED_CONTROL		0x1000
#define QCOM_ICE_REG_CONTROL			0x0
#define QCOM_ICE_LUT_KEYS_CRYPTOCFG_R16		0x4040

/* QCOM ICE HWKM registers */
#define QTI_HWKM_ICE_RG_IPCAT_VERSION			0x0000
#define QCOM_ICE_REG_HWKM_TZ_KM_CTL			0x1000
#define QCOM_ICE_REG_HWKM_TZ_KM_STATUS			0x1004
#define QCOM_ICE_REG_HWKM_BANK0_BANKN_IRQ_STATUS	0x2008
#define QCOM_ICE_REG_HWKM_BANK0_BBAC_0			0x5000
#define QCOM_ICE_REG_HWKM_BANK0_BBAC_1			0x5004
#define QCOM_ICE_REG_HWKM_BANK0_BBAC_2			0x5008
#define QCOM_ICE_REG_HWKM_BANK0_BBAC_3			0x500C
#define QCOM_ICE_REG_HWKM_BANK0_BBAC_4			0x5010

/* QCOM ICE HWKM BIST vals */
#define QCOM_ICE_HWKM_BIST_DONE_V1_VAL		0x14007
#define QCOM_ICE_HWKM_BIST_DONE_V2_VAL		0x287

/* QCOM ICE HWKM version*/
#define QCOM_ICE_HWKM_V2_0_0			0x02000000
#define QCOM_ICE_HWKM_V2_1_0			0x02010000
/* BIST ("built-in self-test") status flags */
#define QCOM_ICE_BIST_STATUS_MASK		GENMASK(31, 28)

#define QCOM_ICE_FUSE_SETTING_MASK		0x1
#define QCOM_ICE_FORCE_HW_KEY0_SETTING_MASK	0x2
#define QCOM_ICE_FORCE_HW_KEY1_SETTING_MASK	0x4

#define QCOM_ICE_LUT_KEYS_CRYPTOCFG_OFFSET	0x80

#define QCOM_ICE_HWKM_REG_OFFSET	0x8000
#define HWKM_OFFSET(reg)		((reg) + QCOM_ICE_HWKM_REG_OFFSET)

#define qcom_ice_writel(engine, val, reg)	\
	writel((val), (engine)->base + (reg))

#define qcom_ice_readl(engine, reg)	\
	readl((engine)->base + (reg))

struct qcom_ice {
	struct device *dev;
	void __iomem *base;
	struct device_link *link;

	struct clk *core_clk;
	u8 hwkm_version;
	bool use_hwkm;
	bool hwkm_init_complete;
	bool handle_clks;
};

union crypto_cfg {
	__le32 regval;
	struct {
		u8 dusize;
		u8 capidx;
		u8 reserved;
		u8 cfge;
	};
};

static bool qcom_ice_check_supported(struct qcom_ice *ice)
{
	u32 regval = qcom_ice_readl(ice, QCOM_ICE_REG_VERSION);
	struct device *dev = ice->dev;
	int major = FIELD_GET(GENMASK(31, 24), regval);
	int minor = FIELD_GET(GENMASK(23, 16), regval);
	int step = FIELD_GET(GENMASK(15, 0), regval);

	/* For now this driver only supports ICE version 3 and 4. */
	if (major != 3 && major != 4) {
		dev_warn(dev, "Unsupported ICE version: v%d.%d.%d\n",
			 major, minor, step);
		return false;
	}

	if (major >= 4 || (major == 3 && minor == 2 && step >= 1))
		ice->hwkm_version = 2;
	else if (major == 3 && minor == 2)
		ice->hwkm_version = 1;
	else
		ice->hwkm_version = 0;

	if (ice->hwkm_version == 0)
		ice->use_hwkm = false;

	dev_info(dev, "Found QC Inline Crypto Engine (ICE) v%d.%d.%d\n",
		 major, minor, step);
	if (!ice->hwkm_version)
		dev_dbg(dev, "QC ICE HWKM (Hardware Key Manager) not supported\n");
	else
		dev_dbg(dev, "QC ICE HWKM (Hardware Key Manager) version = %d\n",
			 ice->hwkm_version);

	if (!ice->use_hwkm)
		dev_dbg(dev, "QC ICE HWKM (Hardware Key Manager) not used");

	/* If fuses are blown, ICE might not work in the standard way. */
	regval = qcom_ice_readl(ice, QCOM_ICE_REG_FUSE_SETTING);
	if (regval & (QCOM_ICE_FUSE_SETTING_MASK |
		      QCOM_ICE_FORCE_HW_KEY0_SETTING_MASK |
		      QCOM_ICE_FORCE_HW_KEY1_SETTING_MASK)) {
		dev_warn(dev, "Fuses are blown; ICE is unusable!\n");
		return false;
	}

	return true;
}

static void qcom_ice_low_power_mode_enable(struct qcom_ice *ice)
{
	u32 regval;

	regval = qcom_ice_readl(ice, QCOM_ICE_REG_ADVANCED_CONTROL);

	/* Enable low power mode sequence */
	regval |= 0x7000;
	qcom_ice_writel(ice, regval, QCOM_ICE_REG_ADVANCED_CONTROL);
}

static void qcom_ice_optimization_enable(struct qcom_ice *ice)
{
	u32 regval;

	/* ICE Optimizations Enable Sequence */
	regval = qcom_ice_readl(ice, QCOM_ICE_REG_ADVANCED_CONTROL);
	regval |= 0xd807100;
	/* ICE HPG requires delay before writing */
	udelay(5);
	qcom_ice_writel(ice, regval, QCOM_ICE_REG_ADVANCED_CONTROL);
	udelay(5);
}

/*
 * Wait until the ICE BIST (built-in self-test) has completed.
 *
 * This may be necessary before ICE can be used.
 * Note that we don't really care whether the BIST passed or failed;
 * we really just want to make sure that it isn't still running. This is
 * because (a) the BIST is a FIPS compliance thing that never fails in
 * practice, (b) ICE is documented to reject crypto requests if the BIST
 * fails, so we needn't do it in software too, and (c) properly testing
 * storage encryption requires testing the full storage stack anyway,
 * and not relying on hardware-level self-tests.
 *
 * However, we still care about if HWKM BIST failed (when supported) as
 * important functionality would fail later, so disable hwkm on failure.
 */
static int qcom_ice_wait_bist_status(struct qcom_ice *ice)
{
	u32 regval;
	u32 bist_done_val;
	int err;

	err = readl_poll_timeout(ice->base + QCOM_ICE_REG_BIST_STATUS,
				 regval, !(regval & QCOM_ICE_BIST_STATUS_MASK),
				 50, 5000);
	if (err)
		dev_err(ice->dev, "Timed out waiting for ICE self-test to complete\n");

	if (ice->use_hwkm) {
		bist_done_val = (ice->hwkm_version == 1) ?
				 QCOM_ICE_HWKM_BIST_DONE_V1_VAL :
				 QCOM_ICE_HWKM_BIST_DONE_V2_VAL;
		if (qcom_ice_readl(ice,
				   HWKM_OFFSET(QCOM_ICE_REG_HWKM_TZ_KM_STATUS)) !=
				   bist_done_val) {
			dev_warn(ice->dev, "HWKM BIST error\n");
			ice->use_hwkm = false;
		}
	}

	return err;
}

static void qcom_ice_enable_standard_mode(struct qcom_ice *ice)
{
	u32 val = 0;

	if (!ice->use_hwkm)
		return;

	/*
	 * When ICE is in standard (hwkm) mode, it supports HW wrapped
	 * keys, and when it is in legacy mode, it only supports standard
	 * (non HW wrapped) keys.
	 *
	 * Put ICE in standard mode, ICE defaults to legacy mode.
	 * Legacy mode - ICE HWKM slave not supported.
	 * Standard mode - ICE HWKM slave supported.
	 *
	 * Depending on the version of HWKM, it is controlled by different
	 * registers in ICE.
	 */
	if (ice->hwkm_version >= 2) {
		val = qcom_ice_readl(ice, QCOM_ICE_REG_CONTROL);
		val = val & 0xFFFFFFFE;
		qcom_ice_writel(ice, val, QCOM_ICE_REG_CONTROL);
	} else {
		qcom_ice_writel(ice, 0x7,
				HWKM_OFFSET(QCOM_ICE_REG_HWKM_TZ_KM_CTL));
	}
}

static void qcom_ice_hwkm_init(struct qcom_ice *ice)
{
	if (!ice->use_hwkm)
		return;

	/* Disable CRC checks. This HWKM feature is not used. */
	qcom_ice_writel(ice, 0x6,
			HWKM_OFFSET(QCOM_ICE_REG_HWKM_TZ_KM_CTL));

	/*
	 * Give register bank of the HWKM slave access to read and modify
	 * the keyslots in ICE HWKM slave. Without this, trustzone will not
	 * be able to program keys into ICE.
	 */
	qcom_ice_writel(ice, 0xFFFFFFFF,
			HWKM_OFFSET(QCOM_ICE_REG_HWKM_BANK0_BBAC_0));
	qcom_ice_writel(ice, 0xFFFFFFFF,
			HWKM_OFFSET(QCOM_ICE_REG_HWKM_BANK0_BBAC_1));
	qcom_ice_writel(ice, 0xFFFFFFFF,
			HWKM_OFFSET(QCOM_ICE_REG_HWKM_BANK0_BBAC_2));
	qcom_ice_writel(ice, 0xFFFFFFFF,
			HWKM_OFFSET(QCOM_ICE_REG_HWKM_BANK0_BBAC_3));
	qcom_ice_writel(ice, 0xFFFFFFFF,
			HWKM_OFFSET(QCOM_ICE_REG_HWKM_BANK0_BBAC_4));

	/* Clear HWKM response FIFO before doing anything */
	qcom_ice_writel(ice, 0x8,
			HWKM_OFFSET(QCOM_ICE_REG_HWKM_BANK0_BANKN_IRQ_STATUS));

	ice->hwkm_init_complete = true;
}

int qcom_ice_enable(struct qcom_ice *ice)
{
	int err;

	qcom_ice_low_power_mode_enable(ice);
	qcom_ice_optimization_enable(ice);

	qcom_ice_enable_standard_mode(ice);

	err = qcom_ice_wait_bist_status(ice);
	if (err)
		return err;

	qcom_ice_hwkm_init(ice);

	return err;
}
EXPORT_SYMBOL_GPL(qcom_ice_enable);

int qcom_ice_resume(struct qcom_ice *ice)
{
	struct device *dev = ice->dev;
	int err;

	if (ice->handle_clks) {
		err = clk_prepare_enable(ice->core_clk);
		if (err) {
			dev_err(dev, "failed to enable core clock (%d)\n",
				err);
			return err;
		}
	}

	qcom_ice_enable_standard_mode(ice);
	qcom_ice_hwkm_init(ice);
	return qcom_ice_wait_bist_status(ice);
}
EXPORT_SYMBOL_GPL(qcom_ice_resume);

int qcom_ice_suspend(struct qcom_ice *ice)
{
	if (ice->handle_clks)
		clk_disable_unprepare(ice->core_clk);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_ice_suspend);

/*
 * HW dictates the internal mapping between the ICE and HWKM slots,
 * which are different for different versions, make the translation
 * here. For v1 however, the translation is done in trustzone.
 */
static int translate_hwkm_slot(struct qcom_ice *ice, int slot)
{
	int offset = 0;
	u32 hwkm_version = 0;

	if (ice->hwkm_init_complete) {
		hwkm_version = qcom_ice_readl(ice, HWKM_OFFSET(QTI_HWKM_ICE_RG_IPCAT_VERSION));
		if (hwkm_version >= QCOM_ICE_HWKM_V2_0_0 && hwkm_version < QCOM_ICE_HWKM_V2_1_0)
			offset = 10;
	}
	return (ice->hwkm_version == 1) ? slot : ((slot * 2) + offset);
}

#if IS_ENABLED(CONFIG_SCSI_UFS_CRYPTO_QTI) || IS_ENABLED(CONFIG_MMC_CRYPTO_QTI)
static int qcom_ice_program_wrapped_key(struct qcom_ice *ice,
					const struct blk_crypto_key *key,
					u8 data_unit_size, int slot)
{
	int hwkm_slot;
	int err;
	union crypto_cfg cfg;
	struct qtee_shm shm;

	hwkm_slot = translate_hwkm_slot(ice, slot);

	memset(&cfg, 0, sizeof(cfg));
	cfg.dusize = data_unit_size;
	cfg.capidx = QCOM_SCM_ICE_CIPHER_AES_256_XTS;
	cfg.cfge = 0x80;

	/* Clear CFGE */
	qcom_ice_writel(ice, 0x0, QCOM_ICE_LUT_KEYS_CRYPTOCFG_R16 +
				  QCOM_ICE_LUT_KEYS_CRYPTOCFG_OFFSET * slot);

	/*
	 * The following logic for shmbridge will be taken care in SCM driver
	 * in upstream. For now, handle it in the ICE driver downstream until
	 * wrapped key upstream effort is complete.
	 */
	err = qtee_shmbridge_allocate_shm(key->size, &shm);
	if (err)
		return -ENOMEM;

	memcpy(shm.vaddr, key->raw, key->size);
	qtee_shmbridge_flush_shm_buf(&shm);

	/* Call trustzone to program the wrapped key using hwkm */
	err = qcom_scm_config_set_ice_key(hwkm_slot, shm.paddr, key->size,
					  0, 0, 0);
	if (err) {
		pr_err("%s:SCM call Error: 0x%x slot %d\n", __func__, err,
		       slot);
		return err;
	}

	/* Enable CFGE after programming key */
	qcom_ice_writel(ice, cfg.regval, QCOM_ICE_LUT_KEYS_CRYPTOCFG_R16 +
					 QCOM_ICE_LUT_KEYS_CRYPTOCFG_OFFSET * slot);

	qtee_shmbridge_inv_shm_buf(&shm);
	qtee_shmbridge_free_shm(&shm);
	return err;
}

int qcom_ice_program_key_hwkm(struct qcom_ice *ice,
			      u8 algorithm_id, u8 key_size,
			      const struct blk_crypto_key *bkey,
			      u8 data_unit_size, int slot)
{
	struct device *dev = ice->dev;
	int err = 0;

	/* Only AES-256-XTS has been tested so far. */
	if (algorithm_id != QCOM_ICE_CRYPTO_ALG_AES_XTS ||
	    (key_size != QCOM_ICE_CRYPTO_KEY_SIZE_256 &&
	    key_size != QCOM_ICE_CRYPTO_KEY_SIZE_WRAPPED)) {
		dev_err_ratelimited(dev,
				    "Unhandled crypto capability; algorithm_id=%d, key_size=%d\n",
				    algorithm_id, key_size);
		return -EINVAL;
	}

	if (bkey->crypto_cfg.key_type == BLK_CRYPTO_KEY_TYPE_HW_WRAPPED) {
		if (!ice->use_hwkm)
			return -EINVAL;
		err = qcom_ice_program_wrapped_key(ice, bkey, data_unit_size,
						   slot);
	}

	return err;
}
EXPORT_SYMBOL_GPL(qcom_ice_program_key_hwkm);

#endif

int qcom_ice_program_key(struct qcom_ice *ice,
			 u8 algorithm_id, u8 key_size,
			 const u8 crypto_key[], u8 data_unit_size,
			 int slot)
{
	struct device *dev = ice->dev;
	union {
		u8 bytes[AES_256_XTS_KEY_SIZE];
		u32 words[AES_256_XTS_KEY_SIZE / sizeof(u32)];
	} key;
	int i;
	int err;

	/* Only AES-256-XTS has been tested so far. */
	if (algorithm_id != QCOM_ICE_CRYPTO_ALG_AES_XTS ||
	    key_size != QCOM_ICE_CRYPTO_KEY_SIZE_256) {
		dev_err_ratelimited(dev,
				    "Unhandled crypto capability; algorithm_id=%d, key_size=%d\n",
				    algorithm_id, key_size);
		return -EINVAL;
	}

	memcpy(key.bytes, crypto_key, AES_256_XTS_KEY_SIZE);

	/* The SCM call requires that the key words are encoded in big endian */
	for (i = 0; i < ARRAY_SIZE(key.words); i++)
		__cpu_to_be32s(&key.words[i]);

	err = qcom_scm_ice_set_key(slot, key.bytes, AES_256_XTS_KEY_SIZE,
				   QCOM_SCM_ICE_CIPHER_AES_256_XTS,
				   data_unit_size);

	memzero_explicit(&key, sizeof(key));

	return err;
}
EXPORT_SYMBOL_GPL(qcom_ice_program_key);

int qcom_ice_evict_key(struct qcom_ice *ice, int slot)
{
	int hwkm_slot = slot;

	if (ice->use_hwkm) {
		hwkm_slot = translate_hwkm_slot(ice, slot);
	/*
	 * Ignore calls to evict key when HWKM is supported and hwkm init
	 * is not yet done. This is to avoid the clearing all slots call
	 * during a storage reset when ICE is still in legacy mode. HWKM slave
	 * in ICE takes care of zeroing out the keytable on reset.
	 */
		if (!ice->hwkm_init_complete)
			return 0;
	}

	return qcom_scm_clear_ice_key(hwkm_slot, 0);
}
EXPORT_SYMBOL_GPL(qcom_ice_evict_key);

bool qcom_ice_hwkm_supported(struct qcom_ice *ice)
{
	return ice->use_hwkm;
}
EXPORT_SYMBOL_GPL(qcom_ice_hwkm_supported);

int qcom_ice_derive_sw_secret(struct qcom_ice *ice, const u8 wkey[],
			      unsigned int wkey_size,
			      u8 sw_secret[BLK_CRYPTO_SW_SECRET_SIZE])
{
	int err = 0;
	struct qtee_shm shm_key, shm_secret;

	/*
	 * The following logic for shmbridge will be taken care in SCM driver
	 * in upstream. For now, handle it in the ICE driver downstream until
	 * wrapped key upstream effort is complete.
	 */
	err = qtee_shmbridge_allocate_shm(wkey_size, &shm_key);
	if (err)
		return -ENOMEM;

	err = qtee_shmbridge_allocate_shm(BLK_CRYPTO_SW_SECRET_SIZE, &shm_secret);
	if (err)
		goto free_key;

	memcpy(shm_key.vaddr, wkey, wkey_size);
	qtee_shmbridge_flush_shm_buf(&shm_key);

	memset(shm_secret.vaddr, 0, BLK_CRYPTO_SW_SECRET_SIZE);
	qtee_shmbridge_flush_shm_buf(&shm_secret);

	err = qcom_scm_derive_sw_secret(shm_key.paddr, wkey_size,
					shm_secret.paddr, BLK_CRYPTO_SW_SECRET_SIZE);
	if (err) {
		pr_err("%s:SCM call error for raw secret: 0x%x\n", __func__, err);
		goto free_secret;
	}

	qtee_shmbridge_inv_shm_buf(&shm_secret);
	memcpy(sw_secret, shm_secret.vaddr, BLK_CRYPTO_SW_SECRET_SIZE);
	qtee_shmbridge_inv_shm_buf(&shm_key);

free_secret:
	qtee_shmbridge_free_shm(&shm_secret);
free_key:
	qtee_shmbridge_free_shm(&shm_key);
	return err;
}
EXPORT_SYMBOL_GPL(qcom_ice_derive_sw_secret);

static struct qcom_ice *qcom_ice_create(struct device *dev,
					void __iomem *base)
{
	struct qcom_ice *engine;

	if (!qcom_scm_is_available())
		return ERR_PTR(-EPROBE_DEFER);

	if (!qcom_scm_ice_available()) {
		dev_warn(dev, "ICE SCM interface not found\n");
		return NULL;
	}

	engine = devm_kzalloc(dev, sizeof(*engine), GFP_KERNEL);
	if (!engine)
		return ERR_PTR(-ENOMEM);

	engine->dev = dev;
	engine->base = base;

	engine->handle_clks = false;
	engine->handle_clks = of_property_read_bool(dev->of_node,
						 "qcom,ice-handle-clks");
	/*
	 * Legacy DT binding uses different clk names for each consumer,
	 * so lets try those first. If none of those are a match, it means
	 * the we only have one clock and it is part of the dedicated DT node.
	 * Also, enable the clock before we check what HW version the driver
	 * supports.
	 */
	if (engine->handle_clks) {
		engine->core_clk = devm_clk_get_optional_enabled(dev, "core_clk_ice");
		if (!engine->core_clk)
			engine->core_clk = devm_clk_get_optional_enabled(dev, "ice");
		if (!engine->core_clk)
			engine->core_clk = devm_clk_get_enabled(dev, NULL);
		if (IS_ERR(engine->core_clk))
			return ERR_CAST(engine->core_clk);
	}

	engine->use_hwkm = of_property_read_bool(dev->of_node,
						 "qcom,ice-use-hwkm");

	if (!qcom_ice_check_supported(engine))
		return ERR_PTR(-EOPNOTSUPP);

	dev_dbg(dev, "Registered Qualcomm Inline Crypto Engine\n");

	return engine;
}

/**
 * of_qcom_ice_get() - get an ICE instance from a DT node
 * @dev: device pointer for the consumer device
 *
 * This function will provide an ICE instance either by creating one for the
 * consumer device if its DT node provides the 'ice' reg range and the 'ice'
 * clock (for legacy DT style). On the other hand, if consumer provides a
 * phandle via 'qcom,ice' property to an ICE DT, the ICE instance will already
 * be created and so this function will return that instead.
 *
 * Return: ICE pointer on success, NULL if there is no ICE data provided by the
 * consumer or ERR_PTR() on error.
 */
struct qcom_ice *of_qcom_ice_get(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct qcom_ice *ice;
	struct device_node *node;
	struct resource *res;
	void __iomem *base;

	if (!dev || !dev->of_node)
		return ERR_PTR(-ENODEV);

	/*
	 * In order to support legacy style devicetree bindings, we need
	 * to create the ICE instance using the consumer device and the reg
	 * range called 'ice' it provides.
	 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ice");
	if (res) {
		base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(base))
			return ERR_CAST(base);

		/* create ICE instance using consumer dev */
		return qcom_ice_create(&pdev->dev, base);
	}

	/*
	 * If the consumer node does not provider an 'ice' reg range
	 * (legacy DT binding), then it must at least provide a phandle
	 * to the ICE devicetree node, otherwise ICE is not supported.
	 */
	node = of_parse_phandle(dev->of_node, "qcom,ice", 0);
	if (!node)
		return NULL;

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		dev_err(dev, "Cannot find device node %s\n", node->name);
		ice = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	ice = platform_get_drvdata(pdev);
	if (!ice) {
		dev_err(dev, "Cannot get ice instance from %s\n",
			dev_name(&pdev->dev));
		platform_device_put(pdev);
		ice = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	ice->link = device_link_add(dev, &pdev->dev, DL_FLAG_AUTOREMOVE_SUPPLIER);
	if (!ice->link) {
		dev_err(&pdev->dev,
			"Failed to create device link to consumer %s\n",
			dev_name(dev));
		platform_device_put(pdev);
		ice = ERR_PTR(-EINVAL);
	}

out:
	of_node_put(node);

	return ice;
}
EXPORT_SYMBOL_GPL(of_qcom_ice_get);

static int qcom_ice_probe(struct platform_device *pdev)
{
	struct qcom_ice *engine;
	void __iomem *base;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base)) {
		dev_warn(&pdev->dev, "ICE registers not found\n");
		return PTR_ERR(base);
	}

	engine = qcom_ice_create(&pdev->dev, base);
	if (IS_ERR(engine))
		return PTR_ERR(engine);

	platform_set_drvdata(pdev, engine);

	return 0;
}

static const struct of_device_id qcom_ice_of_match_table[] = {
	{ .compatible = "qcom,inline-crypto-engine" },
	{ },
};
MODULE_DEVICE_TABLE(of, qcom_ice_of_match_table);

static struct platform_driver qcom_ice_driver = {
	.probe	= qcom_ice_probe,
	.driver = {
		.name = "qcom-ice",
		.of_match_table = qcom_ice_of_match_table,
	},
};

module_platform_driver(qcom_ice_driver);

MODULE_DESCRIPTION("Qualcomm Inline Crypto Engine driver");
MODULE_LICENSE("GPL");
