/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef COMMAND_DB_H
#define COMMAND_DB_H


enum cmd_db_hw_type {
	CMD_DB_HW_MIN = 3,
	CMD_DB_HW_ARC = CMD_DB_HW_MIN,
	CMD_DB_HW_VRM = 4,
	CMD_DB_HW_BCM = 5,
	CMD_DB_HW_MAX = CMD_DB_HW_BCM,
	CMD_DB_HW_ALL = 0xff,
};
#ifdef CONFIG_QCOM_COMMAND_DB
/**
 * cmd_db_get_addr() - Query command db for resource id address.
 *
 *  This is used to retrieve resource address based on resource
 *  id.
 *
 *  @resource_id : resource id to query for address
 *
 *  @return address on success or 0 on error otherwise
 */
u32 cmd_db_get_addr(const char *resource_id);

/**
 * cmd_db_get_priority() - Query command db for resource address priority
 * from command DB.
 *
 *  This is used to retrieve a command DB entry based resource address.
 *
 *  @addr : resource addr to query for priority.
 *  @drv_id : DRV ID to query resource for priority on.
 *  @type: HW type of ID being queried for faster lookups. Clients could
 *		pass in CMD_DB_HW_ALL if type field is unknown
 *
 *  @return true if priority bit is set for the DRV ID/address
 */
bool cmd_db_get_priority(u32 addr, u8 drv_id);

/**
 * cmd_db_get_aux_data() - Query command db for aux data. This is used to
 * retrieve a command DB entry based resource address.
 *
 *  @resource_id : Resource to retrieve AUX Data on.
 *  @data : Data buffer to copy returned aux data to. Returns size on NULL
 *  @len : Caller provides size of data buffer passed in.
 *
 *  returns size of data on success, errno on error
 */
int cmd_db_get_aux_data(const char *resource_id, u8 *data, int len);

/**
 * cmd_db_get_aux_data_len - Get the length of the auxllary data stored in DB.
 *
 * @resource_id: Resource to retrieve AUX Data.
 *
 * returns size on success, errno on error
 */
int cmd_db_get_aux_data_len(const char *resource_id);

/**
 * cmd_db_get_version - Get the version of the command DB data
 *
 * @resource_id: Resource id to query the DB for version
 *
 * returns version on success, 0 on error.
 *	Major number in MSB, minor number in LSB
 */
u16 cmd_db_get_version(const char *resource_id);

/**
 * cmd_db_ready - Indicates if command DB is probed
 *
 * returns  0 on success , errno otherwise
 */
int cmd_db_ready(void);

/**
 * cmd_db_get_slave_id - Get the slave ID for a given resource address
 *
 * @resource_id: Resource id to query the DB for version
 *
 * return  cmd_db_hw_type enum  on success, errno on error
 */
int cmd_db_get_slave_id(const char *resource_id);

/**
 * cmd_db_is_standalone - Returns if the command DB is standalone
 *
 * return 1 if command DB is standalone, 0 if not, errno otherwise.
 */
int cmd_db_is_standalone(void);
#else

static inline u32 cmd_db_get_addr(const char *resource_id)
{
	return 0;
}

bool cmd_db_get_priority(u32 addr, u8 drv_id)
{
	return false;
}

int cmd_db_get_aux_data(const char *resource_id, u8 *data, int len)
{
	return -ENODEV;
}

int cmd_db_get_aux_data_len(const char *resource_id)
{
	return -ENODEV;
}

u16 cmd_db_get_version(const char *resource_id)
{
	return 0;
}

int cmd_db_ready(void)
{
	return -ENODEV;
}

int cmd_db_get_slave_id(const char *resource_id)
{
	return -ENODEV;
}

int cmd_db_is_standalone(void)
{
	return -ENODEV;
}
#endif
#endif
