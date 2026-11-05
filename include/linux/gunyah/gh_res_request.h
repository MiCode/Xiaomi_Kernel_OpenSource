/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __GH_RESOURCE_H
#define __GH_RESOURCE_H

#include <linux/notifier.h>
#include <linux/gunyah/gh_common.h>
#include <linux/gunyah/gh_rm_drv.h>

#define SUBSYS_NAME_MAX 16
#define RESOURCE_CNT_MAX 8

/* Resource type */
enum gh_res_request_type {
	GH_RESOURCE_IOMEM,
	GH_RESOURCE_GPIO,
	GH_RESOURCE_IRQ,
};

/**
 * struct gh_res_request - Define each resource in request
 * @resource_type: Type of the resource
 * @resource: Detailed resource information
 */
struct gh_res_request {
	enum gh_res_request_type resource_type;
	/**
	 * @irq_num: IRQ number
	 * @gpio_num: GPIO number
	 * @sgl_entry: Memory address and size
	 */
	union {
		int irq_num;
		u32 gpio_num;
		struct gh_sgl_entry sgl_entry;
	} resource;
};

/**
 * struct gh_resource_payload - Message used to communicate between VMs
 * @is_req: Represent resource need request or release
 * @source_vmid: Source VMID of the message
 * @subsys_name: The target subsystem name of resources
 * @resource_cnt: Count of resources
 * @resource: Array for resources in the message
 */
struct gh_resource_payload {
	bool is_req;
	gh_vmid_t source_vmid;
	char subsys_name[SUBSYS_NAME_MAX];
	uint8_t resource_cnt;
	struct gh_res_request resource[];
};

typedef int (*gh_resource_callback)(gh_vmid_t source_vmid, bool is_req,
				    struct gh_res_request *resource,
				    uint8_t resource_cnt);

/**
 * struct gh_resource_client - Client driver who is interested in request
 * @list: List entry to be add in the client list
 * @subsys_name: Subsystem name of resources that client is interested in
 * @cb: Function to be called when request happen
 */
struct gh_resource_client {
	struct list_head list;
	char subsys_name[SUBSYS_NAME_MAX];
	gh_resource_callback cb;
};

#if IS_ENABLED(CONFIG_GH_RES_REQUEST)
int gh_resource_register_req_client(struct gh_resource_client *client);
int gh_resource_unregister_req_client(struct gh_resource_client *client);
int gh_resource_register_release_client(struct gh_resource_client *client);
int gh_resource_unregister_release_client(struct gh_resource_client *client);
int gh_resource_request(gh_vmid_t target_vmid, const char *subsys_name,
			struct gh_res_request *req_resource, uint8_t res_cnt);
int gh_resource_release(gh_vmid_t target_vmid, const char *subsys_name,
			struct gh_res_request *release_resource,
			uint8_t res_cnt);
#else
static inline int
gh_resource_register_req_client(struct gh_resource_client *client)
{
	return -ENODEV;
}
static inline int
gh_resource_unregister_req_client(struct gh_resource_client *client)
{
	return -ENODEV;
}
static inline int
gh_resource_register_release_client(struct gh_resource_client *client)
{
	return -EINVAL;
}
static inline int
gh_resource_unregister_release_client(struct gh_resource_client *client)
{
	return -EINVAL;
}
static inline int gh_resource_request(gh_vmid_t target_vmid,
				      const char *subsys_name,
				      struct gh_res_request *req_resource,
				      uint8_t res_cnt)
{
	return -EINVAL;
}
static inline int gh_resource_release(gh_vmid_t target_vmid,
				      const char *subsys_name,
				      struct gh_res_request *release_resource,
				      uint8_t res_cnt)
{
	return -EINVAL;
}
#endif

#endif
