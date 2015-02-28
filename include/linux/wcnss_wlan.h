/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _WCNSS_WLAN_H_
#define _WCNSS_WLAN_H_

#include <linux/device.h>

enum wcnss_opcode {
	WCNSS_WLAN_SWITCH_OFF = 0,
	WCNSS_WLAN_SWITCH_ON,
};

enum wcnss_hw_type {
	WCNSS_RIVA_HW = 0,
	WCNSS_PRONTO_HW,
};

struct wcnss_wlan_config {
	int	use_48mhz_xo;
	int	is_pronto_vt;
	int	is_pronto_v3;
	void __iomem	*msm_wcnss_base;
	int	vbatt;
};

enum {
	WCNSS_XO_48MHZ = 1,
	WCNSS_XO_19MHZ,
	WCNSS_XO_INVALID,
};

enum {
	WCNSS_WLAN_DATA2,
	WCNSS_WLAN_DATA1,
	WCNSS_WLAN_DATA0,
	WCNSS_WLAN_SET,
	WCNSS_WLAN_CLK,
	WCNSS_WLAN_MAX_GPIO,
};

#define WCNSS_VBATT_THRESHOLD           3500000
#define WCNSS_VBATT_GUARD               20000
#define WCNSS_VBATT_HIGH                3700000
#define WCNSS_VBATT_LOW                 3300000
#define WCNSS_VBATT_INITIAL             3000000
#define WCNSS_WLAN_IRQ_INVALID -1
#define HAVE_WCNSS_SUSPEND_RESUME_NOTIFY 1
#define HAVE_WCNSS_RESET_INTR 1
#define HAVE_WCNSS_CAL_DOWNLOAD 1
#define HAVE_CBC_DONE 1
#define HAVE_WCNSS_RX_BUFF_COUNT 1
#define WLAN_MAC_ADDR_SIZE (6)
#define WLAN_RF_REG_ADDR_START_OFFSET	0x3
#define WLAN_RF_REG_DATA_START_OFFSET	0xf
#define WLAN_RF_READ_REG_CMD		0x3
#define WLAN_RF_WRITE_REG_CMD		0x2
#define WLAN_RF_READ_CMD_MASK		0x3fff
#define WLAN_RF_CLK_WAIT_CYCLE		2
#define WLAN_RF_PREPARE_CMD_DATA	5
#define WLAN_RF_READ_DATA		6
#define WLAN_RF_DATA_LEN		3
#define WLAN_RF_DATA0_SHIFT		0
#define WLAN_RF_DATA1_SHIFT		1
#define WLAN_RF_DATA2_SHIFT		2
#define PRONTO_PMU_OFFSET       0x1004
#define WCNSS_PMU_CFG_GC_BUS_MUX_SEL_TOP   BIT(5)

struct device *wcnss_wlan_get_device(void);
void wcnss_get_monotonic_boottime(struct timespec *ts);
struct resource *wcnss_wlan_get_memory_map(struct device *dev);
int wcnss_wlan_get_dxe_tx_irq(struct device *dev);
int wcnss_wlan_get_dxe_rx_irq(struct device *dev);
void wcnss_wlan_register_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops);
void wcnss_wlan_unregister_pm_ops(struct device *dev,
				const struct dev_pm_ops *pm_ops);
void wcnss_register_thermal_mitigation(struct device *dev,
				void (*tm_notify)(struct device *dev, int));
void wcnss_unregister_thermal_mitigation(
				void (*tm_notify)(struct device *dev, int));
struct platform_device *wcnss_get_platform_device(void);
struct wcnss_wlan_config *wcnss_get_wlan_config(void);
void wcnss_set_iris_xo_mode(int iris_xo_mode_set);
int wcnss_wlan_power(struct device *dev,
				struct wcnss_wlan_config *cfg,
				enum wcnss_opcode opcode,
				int *iris_xo_mode_set);
int wcnss_req_power_on_lock(char *driver_name);
int wcnss_free_power_on_lock(char *driver_name);
unsigned int wcnss_get_serial_number(void);
int wcnss_get_wlan_mac_address(char mac_addr[WLAN_MAC_ADDR_SIZE]);
void wcnss_allow_suspend(void);
void wcnss_prevent_suspend(void);
int wcnss_hardware_type(void);
void *wcnss_prealloc_get(unsigned int size);
int wcnss_prealloc_put(void *ptr);
void wcnss_reset_intr(void);
void wcnss_suspend_notify(void);
void wcnss_resume_notify(void);
void wcnss_riva_log_debug_regs(void);
void wcnss_pronto_log_debug_regs(void);
int wcnss_is_hw_pronto_ver3(void);
int wcnss_device_ready(void);
bool wcnss_cbc_complete(void);
int wcnss_device_is_shutdown(void);
void wcnss_riva_dump_pmic_regs(void);
int wcnss_xo_auto_detect_enabled(void);
u32 wcnss_get_wlan_rx_buff_count(void);
int wcnss_wlan_iris_xo_mode(void);
#ifdef CONFIG_WCNSS_REGISTER_DUMP_ON_BITE
void wcnss_log_debug_regs_on_bite(void);
#else
static inline void wcnss_log_debug_regs_on_bite(void)
{
}
#endif
int wcnss_set_wlan_unsafe_channel(
				u16 *unsafe_ch_list, u16 ch_count);
int wcnss_get_wlan_unsafe_channel(
				u16 *unsafe_ch_list, u16 buffer_size,
				u16 *ch_count);
#define wcnss_wlan_get_drvdata(dev) dev_get_drvdata(dev)
#define wcnss_wlan_set_drvdata(dev, data) dev_set_drvdata((dev), (data))
/* WLAN driver uses these names */
#define req_riva_power_on_lock(name) wcnss_req_power_on_lock(name)
#define free_riva_power_on_lock(name) wcnss_free_power_on_lock(name)

#endif /* _WCNSS_WLAN_H_ */
