/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _PMIC_GLINK_H
#define _PMIC_GLINK_H

#include <linux/types.h>

struct pmic_glink_client;
struct device;

/**
 * struct pmic_glink_client_data - pmic_glink client data
 * @name:	Client name
 * @id:		Unique id for client for communication
 * @priv:	private data for client
 * @callback:	callback function for client
 */
struct pmic_glink_client_data {
	const char	*name;
	u32		id;
	void		*priv;
	int		(*callback)(void *priv, void *data, size_t len);
};

/**
 * struct pmic_glink_hdr - PMIC Glink message header
 * @owner:	message owner for a client
 * @type:	message type
 * @opcode:	message opcode
 */
struct pmic_glink_hdr {
	u32 owner;
	u32 type;
	u32 opcode;
};

#if IS_ENABLED(CONFIG_QTI_PMIC_GLINK)
struct pmic_glink_client *pmic_glink_register_client(struct device *dev,
			const struct pmic_glink_client_data *client_data);
int pmic_glink_unregister_client(struct pmic_glink_client *client);
int pmic_glink_write(struct pmic_glink_client *client, void *data,
			size_t len);
#else
static inline struct pmic_glink_client *pmic_glink_register_client(
			struct device *dev,
			const struct pmic_glink_client_data *client_data)
{
	return ERR_PTR(-ENODEV);
}

static inline int pmic_glink_unregister_client(struct pmic_glink_client *client)
{
	return -ENODEV;
}

static inline int pmic_glink_write(struct pmic_glink_client *client, void *data,
				size_t len)
{
	return -ENODEV;
}
#endif

#endif
