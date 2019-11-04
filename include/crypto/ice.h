/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _QCOM_INLINE_CRYPTO_ENGINE_H_
#define _QCOM_INLINE_CRYPTO_ENGINE_H_

#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <linux/wait.h>

struct request;

enum ice_cryto_algo_mode {
	ICE_CRYPTO_ALGO_MODE_AES_ECB = 0x0,
	ICE_CRYPTO_ALGO_MODE_AES_XTS = 0x3,
};

enum ice_crpto_key_size {
	ICE_CRYPTO_KEY_SIZE_128 = 0x0,
	ICE_CRYPTO_KEY_SIZE_256 = 0x2,
};

enum ice_crpto_key_mode {
	ICE_CRYPTO_USE_KEY0_HW_KEY = 0x0,
	ICE_CRYPTO_USE_KEY1_HW_KEY = 0x1,
	ICE_CRYPTO_USE_LUT_SW_KEY0 = 0x2,
	ICE_CRYPTO_USE_LUT_SW_KEY  = 0x3
};

#define QCOM_ICE_TYPE_NAME_LEN 8

typedef void (*ice_error_cb)(void *, u32 error);

struct qcom_ice_bus_vote {
	uint32_t client_handle;
	uint32_t curr_vote;
	int min_bw_vote;
	int max_bw_vote;
	int saved_vote;
	bool is_max_bw_needed;
	struct device_attribute max_bus_bw;
};

/*
 * ICE HW device structure.
 */
struct ice_device {
	struct list_head	list;
	struct device		*pdev;
	struct cdev		cdev;
	dev_t			device_no;
	struct class		*driver_class;
	void __iomem		*mmio;
	struct resource		*res;
	int			irq;
	bool			is_ice_enabled;
	bool			is_ice_disable_fuse_blown;
	ice_error_cb		error_cb;
	void			*host_controller_data; /* UFS/EMMC/other? */
	struct list_head	clk_list_head;
	u32			ice_hw_version;
	bool			is_ice_clk_available;
	char			ice_instance_type[QCOM_ICE_TYPE_NAME_LEN];
	struct regulator	*reg;
	bool			is_regulator_available;
	struct qcom_ice_bus_vote bus_vote;
	ktime_t			ice_reset_start_time;
	ktime_t			ice_reset_complete_time;
	void                    *key_table;
	atomic_t		is_ice_suspended;
	atomic_t		is_ice_busy;
	wait_queue_head_t       block_suspend_ice_queue;
};

struct ice_crypto_setting {
	enum ice_crpto_key_size		key_size;
	enum ice_cryto_algo_mode	algo_mode;
	enum ice_crpto_key_mode		key_mode;
	short				key_index;

};

struct ice_data_setting {
	struct ice_crypto_setting	crypto_data;
	bool				sw_forced_context_switch;
	bool				decr_bypass;
	bool				encr_bypass;
};

/* MSM ICE Crypto Data Unit of target DUN of Transfer Request */
enum ice_crypto_data_unit {
	ICE_CRYPTO_DATA_UNIT_512_B          = 0,
	ICE_CRYPTO_DATA_UNIT_1_KB           = 1,
	ICE_CRYPTO_DATA_UNIT_2_KB           = 2,
	ICE_CRYPTO_DATA_UNIT_4_KB           = 3,
	ICE_CRYPTO_DATA_UNIT_8_KB           = 4,
	ICE_CRYPTO_DATA_UNIT_16_KB          = 5,
	ICE_CRYPTO_DATA_UNIT_32_KB          = 6,
	ICE_CRYPTO_DATA_UNIT_64_KB          = 7,
};

struct qcom_ice_variant_ops *qcom_ice_get_variant_ops(struct device_node *node);
struct platform_device *qcom_ice_get_pdevice(struct device_node *node);

#ifdef CONFIG_CRYPTO_DEV_QCOM_ICE
int enable_ice_setup(struct ice_device *ice_dev);
int disable_ice_setup(struct ice_device *ice_dev);
int qcom_ice_setup_ice_hw(const char *storage_type, int enable);
void qcom_ice_set_fde_flag(int flag);
struct list_head *get_ice_dev_list(void);
#else
static inline int enable_ice_setup(struct ice_device *ice_dev)
{
	return 0;
}
static inline int disable_ice_setup(struct ice_device *ice_dev)
{
	return 0;
}
static inline int qcom_ice_setup_ice_hw(const char *storage_type, int enable)
{
	return 0;
}
static inline void qcom_ice_set_fde_flag(int flag) {}
static inline struct list_head *get_ice_dev_list(void)
{
	return NULL;
}
#endif

struct qcom_ice_variant_ops {
	const char *name;
	int	(*init)(struct platform_device *device_init, void *init_data,
				ice_error_cb err);
	int	(*reset)(struct platform_device *device_reset);
	int	(*resume)(struct platform_device *device_resume);
	int	(*suspend)(struct platform_device *device_suspend);
	int	(*config_start)(struct platform_device *device_start,
			struct request *req, struct ice_data_setting *setting,
			bool start);
	int	(*config_end)(struct platform_device *pdev,
			struct request *req);
	int	(*status)(struct platform_device *device_status);
	void	(*debug)(struct platform_device *device_debug);
};

#endif /* _QCOM_INLINE_CRYPTO_ENGINE_H_ */
