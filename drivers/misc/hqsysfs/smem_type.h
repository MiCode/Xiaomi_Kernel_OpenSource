#ifndef SMEM_TYPE_H
#define SMEM_TYPE_H

/**
 * @file smem_type.h
 *
 * Public data types used by SMEM
 */

/*==============================================================================
  Copyright (c) 2011-2017 Qualcomm Technologies, Inc.
		All Rights Reserved.
  Qualcomm Technologies, Inc. Confidential and Proprietary.
==============================================================================*/

/*===========================================================================

		EDIT HISTORY FOR FILE

$Header: //components/rel/boot.xf/4.2/QcomPkg/Include/Library/smem_type.h#1 $

when       who     what, where, why
--------   ---     ----------------------------------------------------------
02/14/16   vr      SMEM Version 0x000C0000 changes.
09/20/16   vr      Adding SMEM_WLAN_CONFIG by duplicating
			SMEM_RESERVED_SMEM_SLOW_CLOCK_SYNC
08/09/16   vr      Using reserved smem host ID for CDSP processor
			Added smem item for SMP2P in CDSP
06/22/16   vr      Added SMEM_CDSP as a new smem_host_type
03/30/16   na      Adding SMEM_XBL_LOADER_CORE_INFO by duplicating
			SMEM_RESERVED_SMEM_STATIC_LOG_IDX
07/07/15   bc      Add smem host id for secure processor and hypervisor
06/18/15   rv      Renamed SMEM_RESERVED_WM_UUID to SMEM_BOOT_BOARD_INFO
05/06/15   db      Adding SMEM_INVALID_HOST (-1) to make sure that
			invalid host value remains constant
04/07/15   db      Added smem_host_type for TZ
			Increased SMEM_NUM_HOSTS
			Added smem items for SMP2P in TZ
03/04/15   rv      Renamed SMEM_RESERVE_BAD_BLOCKS to SMEM_ERR_CRASH_LOG_ADSP.
11/13/14   an      Add SMEM_SSC.
09/30/14   bc      Renamed SMEM_RESERVED_CLKREGIM_BSP to SMEM_VSENSE_DATA
08/01/14   an      Add 3 items for GLINK native SMEM transport.
01/22/14   bt      Add SMEM_COEX_MDM_WCN for Mdm<->Wcn Coex information.
12/16/13   bt      Add SMEM_RF_EEPROM_DATA for 50B of EEPROM for I2C.
12/06/13   an      change SMEM_RESERVED_SMDLITE_TABLE to SMEM_SMD_FEATURE_SET
10/18/13   bt      Add SMEM_IPC_INFO for Femto DANIPC descriptors.
09/10/13   bt      Add SMEM_CLOCK_INFO for SBL and RPM during bootup (SysDr).
09/10/13   bt      Add SMEM_CPR_CONFIG for SBL and RPM during bootup.
09/10/13   rs      Add SMEM_A2_BAM_DESCRIPTOR_FIFO for unmount feature.
05/21/13   hwu     Added SMEM_FLASH_NAND_DEV_INFO for Flash/Boot team.
01/24/13   pa      Add SMEM_LC_DEBUGGER item for low-cost/software debugger.
01/21/13   bt      Add SMEM_IMAGE_VERSION_TABLE for B-family image versioning.
12/05/12   bt      Add SMEM_BAM_PIPE_MEMORY for A2/USB Apps<->Modem BAM.
11/30/12   pa      Added SMP2P SMEM items.
10/29/12   bt      Add SMEM_FLASH_DEVICE_INFO for TZ image.
09/14/12   bt      Move SMEM_VERSION_ID to smem_type.h, increment to 0x000B2002
			-(minor version change): indicates SMEM_VOICE present.
			-Also make version mask names unambiguous.
06/20/12   bt      Add SMEM_VOICE to replace previous PMEM usage, ADSP + MPSS.
05/04/12   bm      Add RPM smem_host_type.
01/23/12   hwu     Moved smem_host_type to public header.
12/08/11   bt      Add SMEM_SSR_REASON smem items for each peripheral proc.
10/31/11   bt      Reserve SMEM_SMD_SMSM_INTR_MUX, all interrupts separated.
10/05/11   bt      Added SMEM_SMD_LOOPBACK_REGISTER for N-way SMD loopback.
08/03/11   bt      Remove/reserve all unused smem_mem_types.
04/22/11   bt      Replace SMEM_NUM_SMD_STREAM/BLOCK_CHANNELS with
			SMEM_NUM_SMD_CHANNELS, since Stream is only protocol now.
04/15/11   tl      Created separate header for smem_mem_type
===========================================================================*/

/** @addtogroup smem
@{ */

/*
=============================================================================
			TYPE DEFINITIONS
=============================================================================
*/
/*
*****************************************************************************
   The most significant two bytes of this number are the smem major version and
 the least significant two bytes are the smem minor version.  The major version
 number should be updated whenever a change which causes an incompatibility is
 introduced.
   The minor version number can track API changes and deprecations that will
 not affect remote processors.  This may include changes to the smem_mem_type
 enum, if dependencies have already been satisfied on the relevant processors.
   Inconsistencies in minor version, between processors, will not prevent smem
 from booting, but major version inconsistencies will.
*****************************************************************************
*/
#define SMEM_VERSION_ID                     0x000C0000

#define SMEM_MAJOR_VERSION_MASK             0xFFFF0000
#define SMEM_MINOR_VERSION_MASK             0x0000FFFF
#define SMEM_FULL_VERSION_MASK              0xFFFFFFFF

#define SMEM_NUM_SMD_CHANNELS               64
#define SMEM_NUM_SMP2P_EDGES                8

/** Types of memory that can be requested via smem_alloc.

  All of these types of memory have corresponding buffers allocated in
  smem_data_decl. If a buffer is added to smem_data_decl, add a corresponding
  entry to this list.

  SMEM_VERSION_FIRST and SMEM_VERSION_LAST are the first and last
  boundaries for external version checking via the smem_version_set routine.
  To set up versioning for a shared item, add an entry between
  SMEM_VERSION_FIRST and SMEM_VERSION_LAST and update the SMEM version in
  smem_version.h.

  SMEM_VERSION_LAST need not be the last item in the enum.
*/
typedef enum {
  SMEM_MEM_FIRST,
  SMEM_RESERVED_PROC_COMM = SMEM_MEM_FIRST,
  SMEM_FIRST_FIXED_BUFFER = SMEM_RESERVED_PROC_COMM,
  SMEM_HEAP_INFO,
  SMEM_ALLOCATION_TABLE,
  SMEM_VERSION_INFO,
  SMEM_HW_RESET_DETECT,
  SMEM_RESERVED_AARM_WARM_BOOT,
  SMEM_DIAG_ERR_MESSAGE,
  SMEM_SPINLOCK_ARRAY,
  SMEM_MEMORY_BARRIER_LOCATION,
  SMEM_LAST_FIXED_BUFFER = SMEM_MEMORY_BARRIER_LOCATION,
  SMEM_AARM_PARTITION_TABLE,
  SMEM_AARM_BAD_BLOCK_TABLE,
  SMEM_ERR_CRASH_LOG_ADSP,
  SMEM_BOOT_BOARD_INFO,
  SMEM_CHANNEL_ALLOC_TBL,
  SMEM_SMD_BASE_ID,
  SMEM_SMEM_LOG_IDX = SMEM_SMD_BASE_ID + SMEM_NUM_SMD_CHANNELS,
  SMEM_SMEM_LOG_EVENTS,
  SMEM_RESERVED_SMEM_STATIC_LOG_IDX,
  SMEM_XBL_LOADER_CORE_INFO =
  SMEM_RESERVED_SMEM_STATIC_LOG_IDX,
  SMEM_RESERVED_SMEM_STATIC_LOG_EVENTS,
  SMEM_CHARGER_BATTERY_INFO =
  SMEM_RESERVED_SMEM_STATIC_LOG_EVENTS,
  SMEM_RESERVED_SMEM_SLOW_CLOCK_SYNC,
  SMEM_WLAN_CONFIG =
  SMEM_RESERVED_SMEM_SLOW_CLOCK_SYNC,
  SMEM_RESERVED_SMEM_SLOW_CLOCK_VALUE,
  SMEM_RESERVED_BIO_LED_BUF,
  SMEM_SMSM_SHARED_STATE,
  SMEM_RESERVED_SMSM_INT_INFO,
  SMEM_RESERVED_SMSM_SLEEP_DELAY,
  SMEM_RESERVED_SMSM_LIMIT_SLEEP,
  SMEM_RESERVED_SLEEP_POWER_COLLAPSE_DISABLED,
  SMEM_RESERVED_KEYPAD_KEYS_PRESSED,
  SMEM_RESERVED_KEYPAD_STATE_UPDATED,
  SMEM_RESERVED_KEYPAD_STATE_IDX,
  SMEM_RESERVED_GPIO_INT,
  SMEM_ID_SMP2P_BASE_CDSP,
  SMEM_RESERVED_SMD_PROFILES = SMEM_ID_SMP2P_BASE_CDSP +
		SMEM_NUM_SMP2P_EDGES,
  SMEM_RESERVED_TSSC_BUSY,
  SMEM_RESERVED_HS_SUSPEND_FILTER_INFO,
  SMEM_RESERVED_BATT_INFO,
  SMEM_RESERVED_APPS_BOOT_MODE,
  SMEM_VERSION_FIRST,
  SMEM_VERSION_SMD = SMEM_VERSION_FIRST,
  SMEM_VERSION_SMD_BRIDGE,
  SMEM_VERSION_SMSM,
  SMEM_VERSION_SMD_NWAY_LOOP,
  SMEM_VERSION_LAST = SMEM_VERSION_FIRST + 24,
  SMEM_RESERVED_OSS_RRCASN1_BUF1,
  SMEM_RESERVED_OSS_RRCASN1_BUF2,
  SMEM_ID_VENDOR0,
  SMEM_ID_VENDOR1,
  SMEM_ID_VENDOR2,
  SMEM_HW_SW_BUILD_ID,
  SMEM_RESERVED_SMD_BLOCK_PORT_BASE_ID,
  SMEM_RESERVED_SMD_BLOCK_PORT_PROC0_HEAP =
		SMEM_RESERVED_SMD_BLOCK_PORT_BASE_ID +
		SMEM_NUM_SMD_CHANNELS,
  SMEM_RESERVED_SMD_BLOCK_PORT_PROC1_HEAP =
		SMEM_RESERVED_SMD_BLOCK_PORT_PROC0_HEAP +
		SMEM_NUM_SMD_CHANNELS,
  SMEM_I2C_MUTEX = SMEM_RESERVED_SMD_BLOCK_PORT_PROC1_HEAP +
		SMEM_NUM_SMD_CHANNELS,
  SMEM_SCLK_CONVERSION,
  SMEM_RESERVED_SMD_SMSM_INTR_MUX,
  SMEM_SMSM_CPU_INTR_MASK,
  SMEM_RESERVED_APPS_DEM_SLAVE_DATA,
  SMEM_RESERVED_QDSP6_DEM_SLAVE_DATA,
  SMEM_VSENSE_DATA,
  SMEM_RESERVED_CLKREGIM_SOURCES,
  SMEM_SMD_FIFO_BASE_ID,
  SMEM_USABLE_RAM_PARTITION_TABLE = SMEM_SMD_FIFO_BASE_ID +
		SMEM_NUM_SMD_CHANNELS,
  SMEM_POWER_ON_STATUS_INFO,
  SMEM_DAL_AREA,
  SMEM_SMEM_LOG_POWER_IDX,
  SMEM_SMEM_LOG_POWER_WRAP,
  SMEM_SMEM_LOG_POWER_EVENTS,
  SMEM_ERR_CRASH_LOG,
  SMEM_ERR_F3_TRACE_LOG,
  SMEM_SMD_BRIDGE_ALLOC_TABLE,
  SMEM_SMD_FEATURE_SET,
  SMEM_RESERVED_SD_IMG_UPGRADE_STATUS,
  SMEM_SEFS_INFO,
  SMEM_RESERVED_RESET_LOG,
  SMEM_RESERVED_RESET_LOG_SYMBOLS,
  SMEM_MODEM_SW_BUILD_ID,
  SMEM_SMEM_LOG_MPROC_WRAP,
  SMEM_RESERVED_BOOT_INFO_FOR_APPS,
  SMEM_SMSM_SIZE_INFO,
  SMEM_SMD_LOOPBACK_REGISTER,
  SMEM_SSR_REASON_MSS0,
  SMEM_SSR_REASON_WCNSS0,
  SMEM_SSR_REASON_LPASS0,
  SMEM_SSR_REASON_DSPS0,
  SMEM_SSR_REASON_VCODEC0,
  SMEM_VOICE,
  SMEM_ID_SMP2P_BASE_APPS, /* = 427 */
  SMEM_ID_SMP2P_BASE_MODEM = SMEM_ID_SMP2P_BASE_APPS +
		SMEM_NUM_SMP2P_EDGES, /* = 435 */
  SMEM_ID_SMP2P_BASE_ADSP = SMEM_ID_SMP2P_BASE_MODEM +
		SMEM_NUM_SMP2P_EDGES, /* = 443 */
  SMEM_ID_SMP2P_BASE_WCN = SMEM_ID_SMP2P_BASE_ADSP +
		SMEM_NUM_SMP2P_EDGES, /* = 451 */
  SMEM_ID_SMP2P_BASE_RPM = SMEM_ID_SMP2P_BASE_WCN +
		SMEM_NUM_SMP2P_EDGES, /* = 459 */
  SMEM_FLASH_DEVICE_INFO = SMEM_ID_SMP2P_BASE_RPM +
		SMEM_NUM_SMP2P_EDGES, /* = 467 */
  SMEM_BAM_PIPE_MEMORY, /* = 468 */
  SMEM_IMAGE_VERSION_TABLE, /* = 469 */
  SMEM_LC_DEBUGGER, /* = 470 */
  SMEM_FLASH_NAND_DEV_INFO, /* =471 */
  SMEM_A2_BAM_DESCRIPTOR_FIFO,          /* = 472 */
  SMEM_CPR_CONFIG,                      /* = 473 */
  SMEM_CLOCK_INFO,                      /* = 474 */
  SMEM_IPC_INFO,                        /* = 475 */
  SMEM_RF_EEPROM_DATA,                  /* = 476 */
  SMEM_COEX_MDM_WCN,                    /* = 477 */
  SMEM_GLINK_NATIVE_XPORT_DESCRIPTOR,   /* = 478 */
  SMEM_GLINK_NATIVE_XPORT_FIFO_0,       /* = 479 */
  SMEM_GLINK_NATIVE_XPORT_FIFO_1,       /* = 480 */
  SMEM_ID_SMP2P_BASE_SSC = 481,         /* = 481 */
  SMEM_ID_SMP2P_BASE_TZ  = SMEM_ID_SMP2P_BASE_SSC + /** = 489 */
  SMEM_NUM_SMP2P_EDGES,
  SMEM_IPA_FILTER_TABLE  = SMEM_ID_SMP2P_BASE_TZ +  /* = 497 */
  SMEM_NUM_SMP2P_EDGES,
  SMEM_MEM_LAST          = SMEM_IPA_FILTER_TABLE,
  SMEM_INVALID                          /* = 498 */
} smem_mem_type;

/*
 * A list of hosts supported in SMEM
*/

typedef enum {
  SMEM_APPS         =  0,                     /**< Apps Processor */
  SMEM_MODEM        =  1,                     /**< Modem processor */
  SMEM_ADSP         =  2,                     /**< ADSP processor */
  SMEM_SSC          =  3,                     /**< Sensor processor */
  SMEM_WCN          =  4,                     /**< WCN processor */
  SMEM_CDSP         =  5,                     /**< Reserved */
  SMEM_RPM          =  6,                     /**< RPM processor */
  SMEM_TZ           =  7,                     /**< TZ processor */
  SMEM_SPSS         =  8,                     /**< Secure processor */
  SMEM_HYP          =  9,                     /**< Hypervisor */
  SMEM_NUM_HOSTS    = 10,                     /**< Max number of host in target */

  SMEM_Q6        = SMEM_ADSP,             /**< Kept for legacy purposes.**  New code should use SMEM_ADSP */
  SMEM_RIVA      = SMEM_WCN,              /**< Kept for legacy purposes.**  New code should use SMEM_WCN */
  SMEM_DSPS      = SMEM_SSC,              /**< Kept for legacy purposes.**  New code should use SMEM_SSC */

  SMEM_CMDDB        = 0xFFFD,             /**< Reserverd partition for command DB usecase */
  SMEM_COMMON_HOST  = 0xFFFE,             /**< Common host */
  SMEM_INVALID_HOST = 0xFFFF,             /**< Invalid processor */
} smem_host_type;

/** @} */ /* end_addtogroup smem */

#endif /* SMEM_TYPE_H */
