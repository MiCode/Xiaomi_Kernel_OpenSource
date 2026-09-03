/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Defines for the sdio device driver
 */
#ifndef __SPRD_WCN_H__
#define __SPRD_WCN_H__

#include <linux/platform_device.h>
#include <linux/time.h>
typedef int (*marlin_reset_callback) (void *para);
extern marlin_reset_callback marlin_reset_func;
extern void *marlin_callback_para;

struct wcn_match_data {
	bool unisoc_wcn_integrated;

	bool unisoc_wcn_sipc;
	bool unisoc_wcn_pcie;
	bool unisoc_wcn_usb;

	bool unisoc_wcn_sdio;
	bool unisoc_wcn_slp;

	//bool unisoc_wcn_platform; //macro in wcn_integrated
	//bool unisoc_wcn_boot; //macro in marlin3 series
	//bool unisoc_wcn_utils; //macro in marlin3 series
	bool unisoc_wcn_l3; //SC2342_I
	bool unisoc_wcn_l6; //UMW2631_I
	bool unisoc_wcn_m3lite; //UMW2652
	bool unisoc_wcn_m3; //SC2355
	bool unisoc_wcn_m3e; // UMW2653

	bool unisoc_wcn_swd; // add in dts

	bool unisoc_wcn_marlin_only;
	bool unisoc_wcn_gnss_only;
};

struct wcn_match_data *get_wcn_match_config(void);

int marlin_probe(struct platform_device *pdev);
int marlin_remove(struct platform_device *pdev);
void marlin_shutdown(struct platform_device *pdev);

int wcn_probe(struct platform_device *pdev);
int wcn_remove(struct platform_device *pdev);
void wcn_shutdown(struct platform_device *pdev);

void module_bus_sipc_init(void);
void module_bus_sipc_deinit(void);
void module_bus_sdio_init(void);
void module_bus_sdio_deinit(void);
#ifdef BUILD_WCN_PCIE
void module_bus_pcie_init(void);
void module_bus_pcie_deinit(void);
#else
static inline void module_bus_pcie_init(void)
{
}

static inline void module_bus_pcie_deinit(void)
{
}
#endif

#endif
