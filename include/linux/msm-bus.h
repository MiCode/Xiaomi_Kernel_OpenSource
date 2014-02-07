/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_BUS_H
#define _ARCH_ARM_MACH_MSM_BUS_H

#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>

/*
 * Macros for clients to convert their data to ib and ab
 * Ws : Time window over which to transfer the data in SECONDS
 * Bs : Size of the data block in bytes
 * Per : Recurrence period
 * Tb : Throughput bandwidth to prevent stalling
 * R  : Ratio of actual bandwidth used to Tb
 * Ib : Instantaneous bandwidth
 * Ab : Arbitrated bandwidth
 *
 * IB_RECURRBLOCK and AB_RECURRBLOCK:
 * These are used if the requirement is to transfer a
 * recurring block of data over a known time window.
 *
 * IB_THROUGHPUTBW and AB_THROUGHPUTBW:
 * These are used for CPU style masters. Here the requirement
 * is to have minimum throughput bandwidth available to avoid
 * stalling.
 */
#define IB_RECURRBLOCK(Ws, Bs) ((Ws) == 0 ? 0 : ((Bs)/(Ws)))
#define AB_RECURRBLOCK(Ws, Per) ((Ws) == 0 ? 0 : ((Bs)/(Per)))
#define IB_THROUGHPUTBW(Tb) (Tb)
#define AB_THROUGHPUTBW(Tb, R) ((Tb) * (R))

struct msm_bus_vectors {
	int src; /* Master */
	int dst; /* Slave */
	uint64_t ab; /* Arbitrated bandwidth */
	uint64_t ib; /* Instantaneous bandwidth */
};

struct msm_bus_paths {
	int num_paths;
	struct msm_bus_vectors *vectors;
};

struct msm_bus_scale_pdata {
	struct msm_bus_paths *usecase;
	int num_usecases;
	const char *name;
	/*
	 * If the active_only flag is set to 1, the BW request is applied
	 * only when at least one CPU is active (powered on). If the flag
	 * is set to 0, then the BW request is always applied irrespective
	 * of the CPU state.
	 */
	unsigned int active_only;
};

/* Scaling APIs */

/*
 * This function returns a handle to the client. This should be used to
 * call msm_bus_scale_client_update_request.
 * The function returns 0 if bus driver is unable to register a client
 */

#ifdef CONFIG_MSM_BUS_SCALING
uint32_t msm_bus_scale_register_client(struct msm_bus_scale_pdata *pdata);
int msm_bus_scale_client_update_request(uint32_t cl, unsigned int index);
void msm_bus_scale_unregister_client(uint32_t cl);
/* AXI Port configuration APIs */
int msm_bus_axi_porthalt(int master_port);
int msm_bus_axi_portunhalt(int master_port);

#else
static inline uint32_t
msm_bus_scale_register_client(struct msm_bus_scale_pdata *pdata)
{
	return 1;
}

static inline int
msm_bus_scale_client_update_request(uint32_t cl, unsigned int index)
{
	return 0;
}

static inline void
msm_bus_scale_unregister_client(uint32_t cl)
{
}

static inline int msm_bus_axi_porthalt(int master_port)
{
	return 0;
}

static inline int msm_bus_axi_portunhalt(int master_port)
{
	return 0;
}
#endif

#if defined(CONFIG_OF) && defined(CONFIG_MSM_BUS_SCALING)
struct msm_bus_scale_pdata *msm_bus_pdata_from_node(
		struct platform_device *pdev, struct device_node *of_node);
struct msm_bus_scale_pdata *msm_bus_cl_get_pdata(struct platform_device *pdev);
void msm_bus_cl_clear_pdata(struct msm_bus_scale_pdata *pdata);
#else
static inline struct msm_bus_scale_pdata
*msm_bus_cl_get_pdata(struct platform_device *pdev)
{
	return NULL;
}

static inline struct msm_bus_scale_pdata *msm_bus_pdata_from_node(
		struct platform_device *pdev, struct device_node *of_node)
{
	return NULL;
}

static inline void msm_bus_cl_clear_pdata(struct msm_bus_scale_pdata *pdata)
{
}
#endif
#endif /*_ARCH_ARM_MACH_MSM_BUS_H*/
