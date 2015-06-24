/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

/** Trustlet Blob length info */
#define MC_TLBLOBLEN_MAGIC	0x7672746C	/* Magic for SWd: vrtl */
#define MAX_SO_CONT_SIZE	512		/* Max size for a container */

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
	SERVICE_TYPE_SP_TRUSTLET	= 2,
	SERVICE_TYPE_SYSTEM_TRUSTLET	= 3,
	SERVICE_TYPE_MIDDLEWARE		= 4,
	SERVICE_TYPE_LAST_ENTRY		= 5,
};

/**
 * Descriptor for a memory segment.
 */
struct segment_descriptor {
	uint32_t	start;	/**< Virtual start address */
	uint32_t	len;	/**< Segment length in bytes */
};

/**
 * MCLF intro for data structure identification.
 * Must be the first element of a valid MCLF file.
 */
struct mclf_intro {
	uint32_t	magic;		/**< Header magic value ASCII "MCLF" */
	uint32_t	version;	/**< Version the MCLF header struct */
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
	uint32_t		login_type;
	/**< GP TA login data */
	uint8_t			login_data[16];
};

/**
 * Version 2.1/2.2 MCLF header.
 */
struct mclf_header_v2 {
	/**< MCLF header start with the mandatory intro */
	struct mclf_intro	intro;
	/**< Service flags */
	uint32_t		flags;
	/**< Type of memory the service must be executed from */
	uint32_t		mem_type;
	/**< Type of service */
	enum service_type	service_type;
	/**< Number of instances which can be run simultaneously */
	uint32_t		num_instances;
	/**< Loadable service unique identifier (UUID) */
	struct mc_uuid_t	uuid;
	/**< If the service_type is SERVICE_TYPE_DRIVER the Driver ID is used */
	uint32_t		driver_id;
	/**<
	 * Number of threads (N) in a service:
	 *   SERVICE_TYPE_SP_TRUSTLET: N = 1
	 *   SERVICE_TYPE_SYSTEM_TRUSTLET: N = 1
	 *   SERVICE_TYPE_DRIVER: N >= 1
	 */
	uint32_t		num_threads;
	/**< Virtual text segment */
	struct segment_descriptor text;
	/**< Virtual data segment */
	struct segment_descriptor data;
	/**< Length of the BSS segment in bytes. MUST be at least 8 byte */
	uint32_t		bss_len;
	/**< Virtual start address of service code */
	uint32_t		entry;
	/**< Version of the interface the driver exports */
	uint32_t		service_version;
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
	uint32_t	magic;		/**< New blob format magic number */
	uint32_t	root_size;	/**< Root container size */
	uint32_t	sp_size;	/**< SP container size */
	uint32_t	ta_size;	/**< TA container size */
	uint32_t	reserved[4];	/**< Reserved for further Use */
};

#endif /* MCLOADFORMAT_H_ */
