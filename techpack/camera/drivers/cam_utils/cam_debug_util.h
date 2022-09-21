/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_DEBUG_UTIL_H_
#define _CAM_DEBUG_UTIL_H_

#include <linux/platform_device.h>

/* Module IDs used for debug logging */
#define CAM_CDM        (1 << 0)
#define CAM_CORE       (1 << 1)
#define CAM_CPAS       (1 << 2)
#define CAM_ISP        (1 << 3)
#define CAM_CRM        (1 << 4)
#define CAM_SENSOR     (1 << 5)
#define CAM_SMMU       (1 << 6)
#define CAM_SYNC       (1 << 7)
#define CAM_ICP        (1 << 8)
#define CAM_JPEG       (1 << 9)
#define CAM_FD         (1 << 10)
#define CAM_LRME       (1 << 11)
#define CAM_FLASH      (1 << 12)
#define CAM_ACTUATOR   (1 << 13)
#define CAM_CCI        (1 << 14)
#define CAM_CSIPHY     (1 << 15)
#define CAM_EEPROM     (1 << 16)
#define CAM_UTIL       (1 << 17)
#define CAM_HFI        (1 << 18)
#define CAM_CTXT       (1 << 19)
#define CAM_OIS        (1 << 20)
#define CAM_RES        (1 << 21)
#define CAM_MEM        (1 << 22)
#define CAM_IRQ_CTRL   (1 << 23)
#define CAM_REQ        (1 << 24)
#define CAM_PERF       (1 << 25)
#define CAM_CUSTOM     (1 << 26)
#define CAM_PRESIL     (1 << 27)
#define CAM_OPE        (1 << 28)
#define CAM_IO_ACCESS  (1 << 29)
#define CAM_SFE        (1 << 30)

/* Log level types */
#define CAM_TYPE_TRACE      (1 << 0)
#define CAM_TYPE_ERR        (1 << 1)
#define CAM_TYPE_WARN       (1 << 2)
#define CAM_TYPE_INFO       (1 << 3)
#define CAM_TYPE_DBG        (1 << 4)

#define STR_BUFFER_MAX_LENGTH  512

/**
 * struct cam_cpas_debug_settings - Sysfs debug settings for cpas driver
 */
struct cam_cpas_debug_settings {
	uint64_t mnoc_hf_0_ab_bw;
	uint64_t mnoc_hf_0_ib_bw;
	uint64_t mnoc_hf_1_ab_bw;
	uint64_t mnoc_hf_1_ib_bw;
	uint64_t mnoc_sf_0_ab_bw;
	uint64_t mnoc_sf_0_ib_bw;
	uint64_t mnoc_sf_1_ab_bw;
	uint64_t mnoc_sf_1_ib_bw;
	uint64_t mnoc_sf_icp_ab_bw;
	uint64_t mnoc_sf_icp_ib_bw;
	uint64_t camnoc_bw;
};

/**
 * struct camera_debug_settings - Sysfs debug settings for camera
 *
 * @cpas_settings: Debug settings for cpas driver.
 */
struct camera_debug_settings {
	struct cam_cpas_debug_settings cpas_settings;
};

/*
 *  cam_debug_log()
 *
 * @brief     :  Get the Module name from module ID and print
 *               respective debug logs
 *
 * @module_id :  Respective Module ID which is calling this function
 * @func      :  Function which is calling to print logs
 * @line      :  Line number associated with the function which is calling
 *               to print log
 * @fmt       :  Formatted string which needs to be print in the log
 *
 */
void cam_debug_log(unsigned int module_id, const char *func, const int line,
	const char *fmt, ...);

/*
 *  cam_debug_trace()
 *
 * @brief     :  Get the Module name from module ID and print
 *               respective debug logs in ftrace
 *
 * @tag       :  Tag indicating whether TRACE, ERR, WARN, INFO, DBG
 * @module_id :  Respective Module ID which is calling this function
 * @func      :  Function which is calling to print logs
 * @line      :  Line number associated with the function which is calling
 *               to print log
 * @fmt       :  Formatted string which needs to be print in the log
 *
 */
void cam_debug_trace(unsigned int tag, unsigned int module_id,
	const char *func, const int line, const char *fmt, ...);

/*
 * cam_get_module_name()
 *
 * @brief     :  Get the module name from module ID
 *
 * @module_id :  Module ID which is using this function
 */
const char *cam_get_module_name(unsigned int module_id);

/*
 * CAM_TRACE
 * @brief    :  This Macro will print logs in ftrace
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_TRACE(__module, fmt, args...)                                      \
	({                                                                     \
		cam_debug_trace(CAM_TYPE_TRACE, __module, __func__, __LINE__,  \
			fmt, ##args);                                          \
	})

/*
 * CAM_ERR
 * @brief    :  This Macro will print error logs
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_ERR(__module, fmt, args...)                                        \
	({                                                                     \
		pr_info("CAM_ERR: %s: %s: %d " fmt "\n",                       \
			cam_get_module_name(__module), __func__,               \
			__LINE__, ##args);                                     \
		cam_debug_trace(CAM_TYPE_ERR, __module, __func__, __LINE__,    \
			fmt, ##args);                                          \
	})

/*
 * CAM_WARN
 * @brief    :  This Macro will print warning logs
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_WARN(__module, fmt, args...)                                       \
	({                                                                     \
		pr_info("CAM_WARN: %s: %s: %d " fmt "\n",                      \
			cam_get_module_name(__module), __func__,               \
			__LINE__, ##args);                                     \
		cam_debug_trace(CAM_TYPE_ERR, __module, __func__, __LINE__,    \
			fmt, ##args);                                          \
	})

/*
 * CAM_INFO
 * @brief    :  This Macro will print Information logs
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_INFO(__module, fmt, args...)                                       \
	({                                                                     \
		pr_info("CAM_INFO: %s: %s: %d " fmt "\n",                      \
			cam_get_module_name(__module), __func__,               \
			__LINE__, ##args);                                     \
		cam_debug_trace(CAM_TYPE_INFO, __module, __func__, __LINE__,   \
			fmt, ##args);                                          \
	})

/*
 * CAM_INFO_RATE_LIMIT
 * @brief    :  This Macro will print info logs with ratelimit
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_INFO_RATE_LIMIT(__module, fmt, args...)                            \
	({                                                                     \
		pr_info_ratelimited("CAM_INFO: %s: %s: %d " fmt "\n",          \
			cam_get_module_name(__module), __func__,               \
			__LINE__, ##args);                                     \
		cam_debug_trace(CAM_TYPE_INFO, __module, __func__, __LINE__,   \
			fmt, ##args);                                          \
	})

/*
 * CAM_DBG
 * @brief    :  This Macro will print debug logs when enabled using GROUP
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_DBG(__module, fmt, args...)                            \
	cam_debug_log(__module, __func__, __LINE__, fmt, ##args)

/*
 * CAM_ERR_RATE_LIMIT
 * @brief    :  This Macro will print error print logs with ratelimit
 */
#define CAM_ERR_RATE_LIMIT(__module, fmt, args...)                             \
	({                                                                     \
		pr_info_ratelimited("CAM_ERR: %s: %s: %d " fmt "\n",           \
			cam_get_module_name(__module), __func__,               \
			__LINE__, ##args);                                     \
		cam_debug_trace(CAM_TYPE_INFO, __module, __func__, __LINE__,   \
			fmt, ##args);                                          \
	})
/*
 * CAM_WARN_RATE_LIMIT
 * @brief    :  This Macro will print warning logs with ratelimit
 *
 * @__module :  Respective module id which is been calling this Macro
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_WARN_RATE_LIMIT(__module, fmt, args...)                            \
	({                                                                     \
		pr_info_ratelimited("CAM_WARN: %s: %s: %d " fmt "\n",          \
			cam_get_module_name(__module), __func__,               \
			__LINE__, ##args);                                     \
		cam_debug_trace(CAM_TYPE_WARN, __module, __func__, __LINE__,   \
			fmt, ##args);                                          \
	})

/*
 * CAM_WARN_RATE_LIMIT_CUSTOM
 * @brief    :  This Macro will print warn logs with custom ratelimit
 *
 * @__module :  Respective module id which is been calling this Macro
 * @interval :  Time interval in seconds
 * @burst    :  No of logs to print in interval time
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_WARN_RATE_LIMIT_CUSTOM(__module, interval, burst, fmt, args...)    \
	({                                                                     \
		static DEFINE_RATELIMIT_STATE(_rs,                             \
			(interval * HZ),                                       \
			burst);                                                \
		if (__ratelimit(&_rs))                                         \
			pr_info(                                               \
				"CAM_WARN: %s: %s: %d " fmt "\n",              \
				cam_get_module_name(__module), __func__,       \
				__LINE__, ##args);                             \
		cam_debug_trace(CAM_TYPE_WARN, __module, __func__, __LINE__,   \
			fmt, ##args);                                          \
	})

/*
 * CAM_INFO_RATE_LIMIT_CUSTOM
 * @brief    :  This Macro will print info logs with custom ratelimit
 *
 * @__module :  Respective module id which is been calling this Macro
 * @interval :  Time interval in seconds
 * @burst    :  No of logs to print in interval time
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_INFO_RATE_LIMIT_CUSTOM(__module, interval, burst, fmt, args...)    \
	({                                                                     \
		static DEFINE_RATELIMIT_STATE(_rs,                             \
			(interval * HZ),                                       \
			burst);                                                \
		if (__ratelimit(&_rs))                                         \
			pr_info(                                               \
				"CAM_INFO: %s: %s: %d " fmt "\n",              \
				cam_get_module_name(__module), __func__,       \
				__LINE__, ##args);                             \
		cam_debug_trace(CAM_TYPE_INFO, __module, __func__, __LINE__,   \
			fmt, ##args);                                          \
	})

/*
 * CAM_ERR_RATE_LIMIT_CUSTOM
 * @brief    :  This Macro will print error logs with custom ratelimit
 *
 * @__module :  Respective module id which is been calling this Macro
 * @interval :  Time interval in seconds
 * @burst    :  No of logs to print in interval time
 * @fmt      :  Formatted string which needs to be print in log
 * @args     :  Arguments which needs to be print in log
 */
#define CAM_ERR_RATE_LIMIT_CUSTOM(__module, interval, burst, fmt, args...)    \
	({                                                                    \
		static DEFINE_RATELIMIT_STATE(_rs,                            \
			(interval * HZ),                                      \
			burst);                                               \
		if (__ratelimit(&_rs))                                        \
			pr_info(                                              \
				"CAM_ERR: %s: %s: %d " fmt "\n",              \
				cam_get_module_name(__module), __func__,      \
				__LINE__, ##args);                            \
		cam_debug_trace(CAM_TYPE_ERR, __module, __func__, __LINE__,   \
			fmt, ##args);                                         \
	})

/**
 * @brief : API to get camera debug settings
 * @return const struct camera_debug_settings pointer.
 */
const struct camera_debug_settings *cam_debug_get_settings(void);

/**
 * @brief : API to parse and store input from sysfs debug node
 * @return Number of bytes read from buffer on success, or -EPERM on error.
 */
ssize_t cam_debug_sysfs_node_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#endif /* _CAM_DEBUG_UTIL_H_ */
