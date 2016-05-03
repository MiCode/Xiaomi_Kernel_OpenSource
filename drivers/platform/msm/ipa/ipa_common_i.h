/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_COMMON_I_H_
#define _IPA_COMMON_I_H_
#include <linux/ipc_logging.h>

#define __FILENAME__ \
	(strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = EP; \
		log_info.id_string = ipa_clients_strings[client]

#define IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = SIMPLE; \
		log_info.id_string = __func__

#define IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = RESOURCE; \
		log_info.id_string = resource_name

#define IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str) \
		log_info.file = __FILENAME__; \
		log_info.line = __LINE__; \
		log_info.type = SPECIAL; \
		log_info.id_string = id_str

#define IPA_ACTIVE_CLIENTS_INC_EP(client) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_EP(client) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_EP(log_info, client); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_SIMPLE() \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_SIMPLE() \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SIMPLE(log_info); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_RESOURCE(resource_name) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_RESOURCE(resource_name) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_RESOURCE(log_info, resource_name); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_INC_SPECIAL(id_str) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str); \
		ipa_inc_client_enable_clks(&log_info); \
	} while (0)

#define IPA_ACTIVE_CLIENTS_DEC_SPECIAL(id_str) \
	do { \
		struct ipa_active_client_logging_info log_info; \
		IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, id_str); \
		ipa_dec_client_disable_clks(&log_info); \
	} while (0)

#define ipa_assert_on(condition)\
do {\
	if (unlikely(condition))\
		ipa_assert();\
} while (0)

enum ipa_active_client_log_type {
	EP,
	SIMPLE,
	RESOURCE,
	SPECIAL,
	INVALID
};

struct ipa_active_client_logging_info {
	const char *id_string;
	char *file;
	int line;
	enum ipa_active_client_log_type type;
};

extern const char *ipa_clients_strings[];

#define IPA_IPC_LOGGING(buf, fmt, args...) \
	do { \
		if (buf) \
			ipc_log_string((buf), fmt, __func__, __LINE__, \
				## args); \
	} while (0)

void ipa_inc_client_enable_clks(struct ipa_active_client_logging_info *id);
void ipa_dec_client_disable_clks(struct ipa_active_client_logging_info *id);
int ipa_inc_client_enable_clks_no_block(
	struct ipa_active_client_logging_info *id);
int ipa_suspend_resource_no_block(enum ipa_rm_resource_name resource);
int ipa_resume_resource(enum ipa_rm_resource_name name);
int ipa_suspend_resource_sync(enum ipa_rm_resource_name resource);
int ipa_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
	u32 bandwidth_mbps);
void *ipa_get_ipc_logbuf(void);
void *ipa_get_ipc_logbuf_low(void);
void ipa_assert(void);


#endif /* _IPA_COMMON_I_H_ */
