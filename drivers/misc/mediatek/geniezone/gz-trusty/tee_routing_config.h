/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _TEE_ROUTING_CONFIG_H_
#define _TEE_ROUTING_CONFIG_H_

#include <gz-trusty/trusty.h>

#define MAX_TEE_ROUTING_SRV_NAME (16)
#define MAX_TEE_ROUTING_NUM (32)

/**
 * struct tee_routing_obj - Record the service(HA) and its coressponding MTEE.
 * @srv_name: The FIRST word of a service name.
 * @tee_id: The tee_id of the coressponding MTEE.
 *	    See include/trusty/trusty.h to get the detail.
 *
 * We use the srv_name and the tee_id to find the correct virtio_device, so
 * that we can route the msg to correct MTEE.
 *
 */

struct tee_routing_obj {
	/**
	 * @srv_name
	 * e.g.  Echo-server: "com.mediatek.geniezone.srv.echo"
	 *	 The first word is "com".
	 *	 New-server: "nebula.com.mediatek.geniezone.srv.new"
	 *	 The first word is "nebula".
	 */
	char srv_name[MAX_TEE_ROUTING_SRV_NAME];
	enum tee_id_t tee_id;
};

/**
 * tee_routing_config - The configing array of struct tee_routing_obj
 *
 * The conent of tee_routing_config will be added into a hash table.
 * You can add new elements like the example below.
 */
static struct tee_routing_obj tee_routing_config[MAX_TEE_ROUTING_NUM] = {
	/* The first element is the default. Do not modify it unless needed.*/
	[0] = { .srv_name = "com",
		.tee_id = TEE_ID_TRUSTY
	},

	{	.srv_name = "nebula",
		.tee_id = TEE_ID_NEBULA
	},

	{	.srv_name = "trusty",
		.tee_id = TEE_ID_TRUSTY
	},
	/* If the service only work for Trusty.
	 * {	.srv_name = "trusty_service",
	 *	.tee_id   = TEE_ID_TRUSTY
	 * },
	 */

	/* Keep the none as the last element. Do not modify it unless needed.*/
	{	.srv_name = "none",
		.tee_id = TEE_ID_END
	}
};

#endif
