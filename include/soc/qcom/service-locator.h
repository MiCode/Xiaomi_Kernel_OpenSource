/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
 * Process Domain Service Locator API header
 */

#ifndef _SERVICE_LOCATOR_H
#define _SERVICE_LOCATOR_H

#define QMI_SERVREG_LOC_NAME_LENGTH_V01 64
#define QMI_SERVREG_LOC_LIST_LENGTH_V01 32

/*
 * @name: The full process domain path for a process domain which provides
 *	  a particular service
 * @instance_id: The QMI instance id corresponding to the root process
 *		 domain which is responsible for notifications for this
 *		 process domain
 * @service_data_valid: Indicates if service_data field has valid data
 * @service_data: Optional service data provided by the service locator
 */
struct servreg_loc_entry_v01 {
	char name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
	uint32_t instance_id;
	uint8_t service_data_valid;
	uint32_t service_data;
};

/*
 * @client_name:   Name of the client calling the api
 * @service_name:  Name of the service for which the list of process domains
 *		   is requested
 * @total_domains: Length of the process domain list
 * @db_rev_count:  Process domain list database revision number
 * @domain_list:   List of process domains providing the service
 */
struct pd_qmi_client_data {
	char client_name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
	char service_name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
	int total_domains;
	int db_rev_count;
	struct servreg_loc_entry_v01 *domain_list;
};

#if defined(CONFIG_MSM_SERVICE_LOCATOR)
/*
 * Use this api to request information regarding the process domains on which
 * a particular service runs. The client name and the service name inside the
 * pd_qmi_client_data structure need to be filled in by the client calling the
 * api. The total domains, db revision and the domain list will be filled in
 * by the service locator.
 * Returns 0 on success; otherwise a value < 0 if no valid subsystem is found.
 */
int get_service_location(struct pd_qmi_client_data *data);

/*
 * Use this api to request information regarding the subsystem the process
 * domain runs on.
 * @pd_path: The name field from inside the servreg_loc_entry that one
 *	     gets back using the get_processdomains api.
 * Returns 0 on success; otherwise a value < 0 if no valid subsystem is found.
 */
int find_subsys(const char *pd_path, char *subsys);

#else

static inline int get_service_location(struct pd_qmi_client_data *data)
{
	return 0;
}

static inline int find_subsys(const char *pd_path, const char *subsys)
{
	return 0;
}

#endif /* CONFIG_MSM_SERVICE_LOCATOR */

#endif
