/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define SDE_POWER_HANDLE_ENABLE_BUS_AB_QUOTA	0
#define SDE_POWER_HANDLE_DISABLE_BUS_AB_QUOTA	0
#define SDE_POWER_HANDLE_ENABLE_BUS_IB_QUOTA		400000000
#define SDE_POWER_HANDLE_ENABLE_NRT_BUS_IB_QUOTA	0
#define SDE_POWER_HANDLE_DISABLE_BUS_IB_QUOTA	0

#define SDE_POWER_HANDLE_CONT_SPLASH_BUS_IB_QUOTA	3000000000
#define SDE_POWER_HANDLE_CONT_SPLASH_BUS_AB_QUOTA	3000000000

#include <linux/sde_io_util.h>
#include <soc/qcom/cx_ipeak.h>

/* event will be triggered before power handler disable */
#define SDE_POWER_EVENT_PRE_DISABLE	0x1

/* event will be triggered after power handler disable */
#define SDE_POWER_EVENT_POST_DISABLE	0x2

/* event will be triggered before power handler enable */
#define SDE_POWER_EVENT_PRE_ENABLE	0x4

/* event will be triggered after power handler enable */
#define SDE_POWER_EVENT_POST_ENABLE	0x8

/**
 * mdss_bus_vote_type: register bus vote type
 * VOTE_INDEX_DISABLE: removes the client vote
 * VOTE_INDEX_LOW: keeps the lowest vote for register bus
 * VOTE_INDEX_MEDIUM: keeps medium vote for register bus
 * VOTE_INDEX_HIGH: keeps the highest vote for register bus
 * VOTE_INDEX_MAX: invalid
 */
enum mdss_bus_vote_type {
	VOTE_INDEX_DISABLE,
	VOTE_INDEX_LOW,
	VOTE_INDEX_MEDIUM,
	VOTE_INDEX_HIGH,
	VOTE_INDEX_MAX,
};

/**
 * enum sde_power_handle_data_bus_client - type of axi bus clients
 * @SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT: core real-time bus client
 * @SDE_POWER_HANDLE_DATA_BUS_CLIENT_NRT: core non-real-time bus client
 * @SDE_POWER_HANDLE_DATA_BUS_CLIENT_MAX: maximum number of bus client type
 */
enum sde_power_handle_data_bus_client {
	SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
	SDE_POWER_HANDLE_DATA_BUS_CLIENT_NRT,
	SDE_POWER_HANDLE_DATA_BUS_CLIENT_MAX
};

/**
 * enum SDE_POWER_HANDLE_DBUS_ID - data bus identifier
 * @SDE_POWER_HANDLE_DBUS_ID_MNOC: DPU/MNOC data bus
 * @SDE_POWER_HANDLE_DBUS_ID_LLCC: MNOC/LLCC data bus
 * @SDE_POWER_HANDLE_DBUS_ID_EBI: LLCC/EBI data bus
 */
enum SDE_POWER_HANDLE_DBUS_ID {
	SDE_POWER_HANDLE_DBUS_ID_MNOC,
	SDE_POWER_HANDLE_DBUS_ID_LLCC,
	SDE_POWER_HANDLE_DBUS_ID_EBI,
	SDE_POWER_HANDLE_DBUS_ID_MAX,
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
 * @ab:         arbitrated bandwidth for each bus client
 * @ib:         instantaneous bandwidth for each bus client
 * @active:	inidcates the state of sde power handle
 */
struct sde_power_client {
	char name[MAX_CLIENT_NAME_LEN];
	short usecase_ndx;
	short refcount;
	u32 id;
	struct list_head list;
	u64 ab[SDE_POWER_HANDLE_DATA_BUS_CLIENT_MAX];
	u64 ib[SDE_POWER_HANDLE_DATA_BUS_CLIENT_MAX];
	bool active;
};

/**
 * struct sde_power_data_handle: power handle struct for data bus
 * @data_bus_scale_table: pointer to bus scaling table
 * @data_bus_hdl: current data bus handle
 * @data_paths_cnt: number of rt data path ports
 * @nrt_data_paths_cnt: number of nrt data path ports
 * @bus_channels: number of memory bus channels
 * @curr_bw_uc_idx: current use case index of data bus
 * @ao_bw_uc_idx: active only use case index of data bus
 * @ab_rt: realtime ab quota
 * @ib_rt: realtime ib quota
 * @ab_nrt: non-realtime ab quota
 * @ib_nrt: non-realtime ib quota
 * @enable: true if bus is enabled
 */
struct sde_power_data_bus_handle {
	struct msm_bus_scale_pdata *data_bus_scale_table;
	u32 data_bus_hdl;
	u32 data_paths_cnt;
	u32 nrt_data_paths_cnt;
	u32 bus_channels;
	u32 curr_bw_uc_idx;
	u32 ao_bw_uc_idx;
	u64 ab_rt;
	u64 ib_rt;
	u64 ab_nrt;
	u64 ib_nrt;
	bool enable;
};

/*
 * struct sde_power_event - local event registration structure
 * @client_name: name of the client registering
 * @cb_fnc: pointer to desired callback function
 * @usr: user pointer to pass to callback event trigger
 * @event: refer to SDE_POWER_HANDLE_EVENT_*
 * @list: list to attach event master list
 * @active: indicates the state of sde power handle
 */
struct sde_power_event {
	char client_name[MAX_CLIENT_NAME_LEN];
	void (*cb_fnc)(u32 event_type, void *usr);
	void *usr;
	u32 event_type;
	struct list_head list;
	bool active;
};

/**
 * struct sde_power_handle: power handle main struct
 * @mp:		module power for clock and regulator
 * @client_clist: master list to store all clients
 * @phandle_lock: lock to synchronize the enable/disable
 * @dev: pointer to device structure
 * @usecase_ndx: current usecase index
 * @reg_bus_hdl: current register bus handle
 * @data_bus_handle: context structure for data bus control
 * @event_list: current power handle event list
 * @rsc_client: sde rsc client pointer
 * @rsc_client_init: boolean to control rsc client create
 * @dss_cx_ipeak: client pointer for cx ipeak driver
 */
struct sde_power_handle {
	struct dss_module_power mp;
	struct list_head power_client_clist;
	struct mutex phandle_lock;
	struct device *dev;
	u32 current_usecase_ndx;
	u32 reg_bus_hdl;
	struct sde_power_data_bus_handle data_bus_handle
		[SDE_POWER_HANDLE_DBUS_ID_MAX];
	struct list_head event_list;
	struct sde_rsc_client *rsc_client;
	bool rsc_client_init;
	struct cx_ipeak_client *dss_cx_ipeak;
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
 * sde_power_scale_reg_bus() - Scale the registers bus for the specified client
 * @phandle:  power handle containing the resources
 * @pclient: client information to scale its vote
 * @usecase_ndx: new use case to scale the reg bus
 * @skip_lock: will skip holding the power rsrc mutex during the call, this is
 *		for internal callers that already hold this required lock.
 *
 * Return: error code.
 */
int sde_power_scale_reg_bus(struct sde_power_handle *phandle,
	struct sde_power_client *pclient, u32 usecase_ndx, bool skip_lock);

/**
 * sde_power_resource_is_enabled() - return true if power resource is enabled
 * @pdata:  power handle containing the resources
 *
 * Return: true if enabled; false otherwise
 */
int sde_power_resource_is_enabled(struct sde_power_handle *pdata);

/**
 * sde_power_data_bus_state_update() - update data bus state
 * @pdata:  power handle containing the resources
 * @enable: take enable vs disable path
 *
 * Return: error code.
 */
int sde_power_data_bus_state_update(struct sde_power_handle *phandle,
							bool enable);

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

/**
 * sde_power_clk_get_max_rate() - get the maximum clock rate
 * @pdata:  power handle containing the resources
 * @clock_name: clock name to get the max rate.
 *
 * Return: maximum clock rate or 0 if not found.
 */
u64 sde_power_clk_get_max_rate(struct sde_power_handle *pdata,
		char *clock_name);

/**
 * sde_power_clk_get_clk() - get the clock
 * @pdata:  power handle containing the resources
 * @clock_name: clock name to get the clk pointer.
 *
 * Return: Pointer to clock
 */
struct clk *sde_power_clk_get_clk(struct sde_power_handle *phandle,
		char *clock_name);

/**
 * sde_power_clk_set_flags() - set the clock flags
 * @pdata:  power handle containing the resources
 * @clock_name: clock name to get the clk pointer.
 * @flags: flags to set
 *
 * Return: error code.
 */
int sde_power_clk_set_flags(struct sde_power_handle *pdata,
		char *clock_name, unsigned long flags);

/**
 * sde_power_data_bus_set_quota() - set data bus quota for power client
 * @phandle:  power handle containing the resources
 * @client: client information to set quota
 * @bus_client: real-time or non-real-time bus client
 * @bus_id: identifier of data bus, see SDE_POWER_HANDLE_DBUS_ID
 * @ab_quota: arbitrated bus bandwidth
 * @ib_quota: instantaneous bus bandwidth
 *
 * Return: zero if success, or error code otherwise
 */
int sde_power_data_bus_set_quota(struct sde_power_handle *phandle,
		struct sde_power_client *pclient,
		int bus_client, u32 bus_id,
		u64 ab_quota, u64 ib_quota);

/**
 * sde_power_data_bus_bandwidth_ctrl() - control data bus bandwidth enable
 * @phandle:  power handle containing the resources
 * @client: client information to bandwidth control
 * @enable: true to enable bandwidth for data base
 *
 * Return: none
 */
void sde_power_data_bus_bandwidth_ctrl(struct sde_power_handle *phandle,
		struct sde_power_client *pclient, int enable);

/**
 * sde_power_handle_register_event - register a callback function for an event.
 *	Clients can register for multiple events with a single register.
 *	Any block with access to phandle can register for the event
 *	notification.
 * @phandle:	power handle containing the resources
 * @event_type:	event type to register; refer SDE_POWER_HANDLE_EVENT_*
 * @cb_fnc:	pointer to desired callback function
 * @usr:	user pointer to pass to callback on event trigger
 *
 * Return:	event pointer if success, or error code otherwise
 */
struct sde_power_event *sde_power_handle_register_event(
		struct sde_power_handle *phandle,
		u32 event_type, void (*cb_fnc)(u32 event_type, void *usr),
		void *usr, char *client_name);
/**
 * sde_power_handle_unregister_event - unregister callback for event(s)
 * @phandle:	power handle containing the resources
 * @event:	event pointer returned after power handle register
 */
void sde_power_handle_unregister_event(struct sde_power_handle *phandle,
		struct sde_power_event *event);

/**
 * sde_power_handle_get_dbus_name - get name of given data bus identifier
 * @bus_id:	data bus identifier
 * Return:	Pointer to name string if success; NULL otherwise
 */
const char *sde_power_handle_get_dbus_name(u32 bus_id);

#endif /* _SDE_POWER_HANDLE_H_ */
