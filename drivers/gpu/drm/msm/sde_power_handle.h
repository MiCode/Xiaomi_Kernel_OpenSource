/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_POWER_HANDLE_H_
#define _SDE_POWER_HANDLE_H_

#define MAX_CLIENT_NAME_LEN 128

/**
 * mdss_bus_vote_type: register bus vote type
 * VOTE_INDEX_DISABLE: removes the client vote
 * VOTE_INDEX_LOW: keeps the lowest vote for register bus
 * VOTE_INDEX_MAX: invalid
 */
enum mdss_bus_vote_type {
	VOTE_INDEX_DISABLE,
	VOTE_INDEX_LOW,
	VOTE_INDEX_MAX,
};

/**
 * struct sde_power_client: stores the power client for sde driver
 * @name:	name of the client
 * @usecase_ndx: current regs bus vote type
 * @refcount:	current refcount if multiple modules are using same
 *              same client for enable/disable. Power module will
 *              aggregate the refcount and vote accordingly for this
 *              client.
 * @id:		assigned during create. helps for debugging.
 * @list:	list to attach power handle master list
 */
struct sde_power_client {
	char name[MAX_CLIENT_NAME_LEN];
	short usecase_ndx;
	short refcount;
	u32 id;
	struct list_head list;
};

/**
 * struct sde_power_handle: power handle main struct
 * @mp:		module power for clock and regulator
 * @client_clist: master list to store all clients
 * @phandle_lock: lock to synchronize the enable/disable
 * @usecase_ndx: current usecase index
 * @reg_bus_hdl: current register bus handle
 */
struct sde_power_handle {
	struct dss_module_power mp;
	struct list_head power_client_clist;
	struct mutex phandle_lock;
	u32 current_usecase_ndx;
#ifdef CONFIG_QCOM_BUS_SCALING
	u32 reg_bus_hdl;
#endif
};

/**
 * sde_power_resource_init() - initializes the sde power handle
 * @pdev:   platform device to search the power resources
 * @pdata:  power handle to store the power resources
 *
 * Return: error code.
 */
int sde_power_resource_init(struct platform_device *pdev,
	struct sde_power_handle *pdata);

/**
 * sde_power_resource_deinit() - release the sde power handle
 * @pdev:   platform device for power resources
 * @pdata:  power handle containing the resources
 *
 * Return: error code.
 */
void sde_power_resource_deinit(struct platform_device *pdev,
	struct sde_power_handle *pdata);

/**
 * sde_power_client_create() - create the client on power handle
 * @pdata:  power handle containing the resources
 * @client_name: new client name for registration
 *
 * Return: error code.
 */
struct sde_power_client *sde_power_client_create(struct sde_power_handle *pdata,
	char *client_name);

/**
 * sde_power_client_destroy() - destroy the client on power handle
 * @pdata:  power handle containing the resources
 * @client_name: new client name for registration
 *
 * Return: none
 */
void sde_power_client_destroy(struct sde_power_handle *phandle,
	struct sde_power_client *client);

/**
 * sde_power_resource_enable() - enable/disable the power resources
 * @pdata:  power handle containing the resources
 * @client: client information to enable/disable its vote
 * @enable: boolean request for enable/disable
 *
 * Return: error code.
 */
int sde_power_resource_enable(struct sde_power_handle *pdata,
	struct sde_power_client *pclient, bool enable);

/**
 * sde_power_clk_set_rate() - set the clock rate
 * @pdata:  power handle containing the resources
 * @clock_name: clock name which needs rate update.
 * @rate:       Requested rate.
 *
 * Return: error code.
 */
int sde_power_clk_set_rate(struct sde_power_handle *pdata, char *clock_name,
	u64 rate);

/**
 * sde_power_clk_get_rate() - get the clock rate
 * @pdata:  power handle containing the resources
 * @clock_name: clock name to get the rate
 *
 * Return: current clock rate
 */
u64 sde_power_clk_get_rate(struct sde_power_handle *pdata, char *clock_name);

#endif /* _SDE_POWER_HANDLE_H_ */
