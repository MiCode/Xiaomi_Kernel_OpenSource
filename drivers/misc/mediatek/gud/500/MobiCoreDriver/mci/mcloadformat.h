/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MCLOADFORMAT_H_
#define MCLOADFORMAT_H_

#include <linux/uaccess.h>	/* u32 and friends */
#include "public/mc_user.h"	/* struct mc_uuid_t */

#define MAX_SO_CONT_SIZE	512		/* Max size for a container */

/** MCLF magic */
/**< "MCLF" in big endian integer representation */
#define MC_SERVICE_HEADER_MAGIC_BE \
	((uint32_t)('M' | ('C' << 8) | ('L' << 16) | ('F' << 24)))
/**< "MCLF" in little endian integer representation */
#define MC_SERVICE_HEADER_MAGIC_LE \
	((uint32_t)(('M' << 24) | ('C' << 16) | ('L' << 8) | 'F'))

/** MCLF flags */
/**< Loaded service cannot be unloaded from MobiCore. */
#define MC_SERVICE_HEADER_FLAGS_PERMANENT		BIT(0)
/**< Service has no WSM control interface. */
#define MC_SERVICE_HEADER_FLAGS_NO_CONTROL_INTERFACE	BIT(1)
/**< Service can be debugged. */
#define MC_SERVICE_HEADER_FLAGS_DEBUGGABLE		BIT(2)
/**< New-layout trusted application or trusted driver. */
#define MC_SERVICE_HEADER_FLAGS_EXTENDED_LAYOUT		BIT(3)

/** Service type.
 * The service type defines the type of executable.
 */
enum service_type {
	SERVICE_TYPE_ILLEGAL		= 0,
	SERVICE_TYPE_DRIVER		= 1,
	SERVICE_TYPE_FLAG_DEPRECATED	= 2,
	SERVICE_TYPE_SYSTEM_TRUSTLET	= 3,
	SERVICE_TYPE_MIDDLEWARE		= 4,
	SERVICE_TYPE_LAST_ENTRY		= 5,
};

/**
 * Descriptor for a memory segment.
 */
struct segment_descriptor {
	u32	start;	/**< Virtual start address */
	u32	len;	/**< Segment length in bytes */
};

/**
 * MCLF intro for data structure identification.
 * Must be the first element of a valid MCLF file.
 */
struct mclf_intro {
	u32	magic;		/**< Header magic value ASCII "MCLF" */
	u32	version;	/**< Version the MCLF header struct */
};

/**
 * @defgroup MCLF_VER_V2   MCLF Version 32
 * @ingroup MCLF_VER
 *
 * @addtogroup MCLF_VER_V2
 */

/*
 * GP TA identity.
 */
struct identity {
	/**< GP TA login type */
	u32	login_type;
	/**< GP TA login data */
	u8	login_data[16];
};

/**
 * Version 2.1/2.2 MCLF header.
 */
struct mclf_header_v2 {
	/**< MCLF header start with the mandatory intro */
	struct mclf_intro	intro;
	/**< Service flags */
	u32	flags;
	/**< Type of memory the service must be executed from */
	u32	mem_type;
	/**< Type of service */
	enum service_type	service_type;
	/**< Number of instances which can be run simultaneously */
	u32	num_instances;
	/**< Loadable service unique identifier (UUID) */
	struct mc_uuid_t	uuid;
	/**< If the service_type is SERVICE_TYPE_DRIVER the Driver ID is used */
	u32	driver_id;
	/**<
	 * Number of threads (N) in a service:
	 *   SERVICE_TYPE_SYSTEM_TRUSTLET: N = 1
	 *   SERVICE_TYPE_DRIVER: N >= 1
	 */
	u32	num_threads;
	/**< Virtual text segment */
	struct segment_descriptor text;
	/**< Virtual data segment */
	struct segment_descriptor data;
	/**< Length of the BSS segment in bytes. MUST be at least 8 byte */
	u32	bss_len;
	/**< Virtual start address of service code */
	u32	entry;
	/**< Version of the interface the driver exports */
	u32	service_version;
};

/**
 * @addtogroup MCLF
 */

/** MCLF header */
union mclf_header {
	/**< Intro for data identification */
	struct mclf_intro	intro;
	/**< Version 2 header */
	struct mclf_header_v2	mclf_header_v2;
};

struct mc_blob_len_info {
	u32	magic;		/**< New blob format magic number */
	u32	root_size;	/**< Root container size */
	u32	sp_size;	/**< SP container size */
	u32	ta_size;	/**< TA container size */
	u32	reserved[4];	/**< Reserved for further Use */
};

#endif /* MCLOADFORMAT_H_ */
