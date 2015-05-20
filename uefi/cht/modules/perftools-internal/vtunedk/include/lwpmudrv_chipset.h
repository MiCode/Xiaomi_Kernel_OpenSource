/*COPYRIGHT**
 * -------------------------------------------------------------------------
 *               INTEL CORPORATION PROPRIETARY INFORMATION
 *  This software is supplied under the terms of the accompanying license
 *  agreement or nondisclosure agreement with Intel Corporation and may not
 *  be copied or disclosed except in accordance with the terms of that
 *  agreement.
 *        Copyright (C) 2007-2014 Intel Corporation.  All Rights Reserved.
 * -------------------------------------------------------------------------
**COPYRIGHT*/

#ifndef _LWPMUDRV_CHIPSET_H_
#define _LWPMUDRV_CHIPSET_H_

#if defined(__cplusplus)
extern "C" {
#endif

#define MAX_CHIPSET_EVENT_NAME  64
#define MAX_CHIPSET_COUNTERS    5  // TODO: this covers 1 fixed counter
                                   // plus 4 general counters on GMCH;
                                   // for other chipset devices, this
                                   // can vary from 8 to 32; might consider
                                   // making this per-chipset-type since
                                   // event-multiplexing is currently not
                                   // supported for chipset collections

#if defined(_NTDDK_)
#define CHIPSET_PHYS_ADDRESS PHYSICAL_ADDRESS
#else
#define CHIPSET_PHYS_ADDRESS U64
#endif

// possible values for whether chipset data is valid or not
enum {
    DATA_IS_VALID,
    DATA_IS_INVALID,
    DATA_OUT_OF_RANGE
};

typedef struct CHIPSET_EVENT_NODE_S  CHIPSET_EVENT_NODE;
typedef        CHIPSET_EVENT_NODE   *CHIPSET_EVENT;

//chipset event
struct CHIPSET_EVENT_NODE_S {
   U32     event_id;
   U32     group_id;
   char    name[MAX_CHIPSET_EVENT_NAME];
   U32     pm;
   U32     counter;
};

#define CHIPSET_EVENT_event_id(chipset_event)   (chipset_event)->event_id
#define CHIPSET_EVENT_group_id(chipset_event)   (chipset_event)->group_id
#define CHIPSET_EVENT_name(chipset_event)       (chipset_event)->name
#define CHIPSET_EVENT_pm(chipset_event)         (chipset_event)->pm
#define CHIPSET_EVENT_counter(chipset_event)    (chipset_event)->counter

typedef struct CHIPSET_SEGMENT_NODE_S  CHIPSET_SEGMENT_NODE;
typedef        CHIPSET_SEGMENT_NODE   *CHIPSET_SEGMENT;

//chipset segment data
struct CHIPSET_SEGMENT_NODE_S {
    CHIPSET_PHYS_ADDRESS  physical_address;
    U64                   virtual_address;
    U16                   size;
    U16                   number_of_counters;
    U16                   total_events;
    U16                   start_register; // (see driver for details)
    U32                   read_register;  // read register offset (model dependent)
    U32                   write_register; // write register offset (model dependent)
    CHIPSET_EVENT_NODE    events[MAX_CHIPSET_COUNTERS];
};

#define CHIPSET_SEGMENT_physical_address(chipset_segment)   (chipset_segment)->physical_address
#define CHIPSET_SEGMENT_virtual_address(chipset_segment)    (chipset_segment)->virtual_address
#define CHIPSET_SEGMENT_size(chipset_segment)               (chipset_segment)->size
#define CHIPSET_SEGMENT_num_counters(chipset_segment)       (chipset_segment)->number_of_counters
#define CHIPSET_SEGMENT_total_events(chipset_segment)       (chipset_segment)->total_events
#define CHIPSET_SEGMENT_start_register(chipset_segment)     (chipset_segment)->start_register
#define CHIPSET_SEGMENT_read_register(chipset_segment)      (chipset_segment)->read_register
#define CHIPSET_SEGMENT_write_register(chipset_segment)     (chipset_segment)->write_register
#define CHIPSET_SEGMENT_events(chipset_segment)             (chipset_segment)->events

typedef struct CHIPSET_CONFIG_NODE_S  CHIPSET_CONFIG_NODE;
typedef        CHIPSET_CONFIG_NODE   *CHIPSET_CONFIG;

//chipset struct used for communication between user mode and kernel
struct CHIPSET_CONFIG_NODE_S
{
    U32 length;               // length of this entire area
    U32 major_version;
    U32 minor_version;
    U32 rsvd;
    U64 cpu_counter_mask;
    struct {
        U64 processor             : 1;  // Processor PMU
        U64 mch_chipset           : 1;  // MCH Chipset
        U64 ich_chipset           : 1;  // ICH Chipset
        U64 motherboard_time_flag : 1;  // Motherboard_Time requested.
        U64 host_processor_run    : 1;  // Each processor should manage the MCH counts they see.
                                        // Turn off for Gen 4 (NOA) runs.
        U64 mmio_noa_registers    : 1;  // NOA
        U64 bnb_chipset           : 1;  // BNB Chipset
        U64 gmch_chipset          : 1;  // GMCH Chipset
        U64 rsvd                  : 56;
    } config_flags;
    CHIPSET_SEGMENT_NODE mch;
    CHIPSET_SEGMENT_NODE ich;
    CHIPSET_SEGMENT_NODE mmio;
    CHIPSET_SEGMENT_NODE bnb;
    CHIPSET_SEGMENT_NODE gmch;
};

#define CHIPSET_CONFIG_length(chipset)                 (chipset)->length
#define CHIPSET_CONFIG_major_version(chipset)          (chipset)->major_version
#define CHIPSET_CONFIG_minor_version(chipset)          (chipset)->minor_version
#define CHIPSET_CONFIG_cpu_counter_mask(chipset)       (chipset)->cpu_counter_mask
#define CHIPSET_CONFIG_processor(chipset)              (chipset)->config_flags.processor
#define CHIPSET_CONFIG_mch_chipset(chipset)            (chipset)->config_flags.mch_chipset
#define CHIPSET_CONFIG_ich_chipset(chipset)            (chipset)->config_flags.ich_chipset
#define CHIPSET_CONFIG_motherboard_time(chipset)       (chipset)->config_flags.motherboard_time_flag
#define CHIPSET_CONFIG_host_proc_run(chipset)          (chipset)->config_flags.host_processor_run
#define CHIPSET_CONFIG_noa_chipset(chipset)            (chipset)->config_flags.mmio_noa_registers
#define CHIPSET_CONFIG_bnb_chipset(chipset)            (chipset)->config_flags.bnb_chipset
#define CHIPSET_CONFIG_gmch_chipset(chipset)           (chipset)->config_flags.gmch_chipset
#define CHIPSET_CONFIG_mch(chipset)                    (chipset)->mch
#define CHIPSET_CONFIG_ich(chipset)                    (chipset)->ich
#define CHIPSET_CONFIG_noa(chipset)                    (chipset)->mmio
#define CHIPSET_CONFIG_bnb(chipset)                    (chipset)->bnb
#define CHIPSET_CONFIG_gmch(chipset)                   (chipset)->gmch

typedef struct CHIPSET_PCI_ARG_NODE_S  CHIPSET_PCI_ARG_NODE;
typedef        CHIPSET_PCI_ARG_NODE   *CHIPSET_PCI_ARG;

struct CHIPSET_PCI_ARG_NODE_S {
    U32 address;
    U32 value;
};

#define CHIPSET_PCI_ARG_address(chipset_pci)                    (chipset_pci)->address
#define CHIPSET_PCI_ARG_value(chipset_pci)                      (chipset_pci)->value

typedef struct CHIPSET_PCI_SEARCH_ADDR_NODE_S  CHIPSET_PCI_SEARCH_ADDR_NODE;
typedef        CHIPSET_PCI_SEARCH_ADDR_NODE   *CHIPSET_PCI_SEARCH_ADDR;

struct CHIPSET_PCI_SEARCH_ADDR_NODE_S {
    U32 start;
    U32 stop;
    U32 increment;
    U32 addr;
};

#define CHIPSET_PCI_SEARCH_ADDR_start(pci_search_addr)           (pci_search_addr)->start
#define CHIPSET_PCI_SEARCH_ADDR_stop(pci_search_addr)            (pci_search_addr)->stop
#define CHIPSET_PCI_SEARCH_ADDR_increment(pci_search_addr)       (pci_search_addr)->increment
#define CHIPSET_PCI_SEARCH_ADDR_address(pci_search_addr)         (pci_search_addr)->addr

typedef struct CHIPSET_PCI_CONFIG_NODE_S  CHIPSET_PCI_CONFIG_NODE;
typedef        CHIPSET_PCI_CONFIG_NODE   *CHIPSET_PCI_CONFIG;

struct CHIPSET_PCI_CONFIG_NODE_S
{
    U32 bus;
    U32 device;
    U32 function;
    U32 offset;
    U32 value;
};

#define CHIPSET_PCI_CONFIG_bus(pci_config)                       (pci_config)->bus
#define CHIPSET_PCI_CONFIG_device(pci_config)                    (pci_config)->device
#define CHIPSET_PCI_CONFIG_function(pci_config)                  (pci_config)->function
#define CHIPSET_PCI_CONFIG_offset(pci_config)                    (pci_config)->offset
#define CHIPSET_PCI_CONFIG_value(pci_config)                     (pci_config)->value

typedef struct CHIPSET_MARKER_NODE_S  CHIPSET_MARKER_NODE;
typedef        CHIPSET_MARKER_NODE   *CHIPSET_MARKER;

struct CHIPSET_MARKER_NODE_S {
    U32 processor_number;
    U32 rsvd;
    U64 tsc;
};

#define CHIPSET_MARKER_processor_number(chipset_marker)          (pci_config)->processor_number
#define CHIPSET_MARKER_tsc(chipset_marker)                       (pci_config)->tsc

typedef struct CHAP_INTERFACE_NODE_S  CHAP_INTERFACE_NODE;
typedef        CHAP_INTERFACE_NODE   *CHAP_INTERFACE;

// CHAP chipset registers
// The offsets for registers are command-0x00, event-0x04, status-0x08, data-0x0C
struct CHAP_INTERFACE_NODE_S {
    U32 command_register;
    U32 event_register;
    U32 status_register;
    U32 data_register;
};

#define CHAP_INTERFACE_command_register(chap)                   (chap)->command_register
#define CHAP_INTERFACE_event_register(chap)                     (chap)->event_register
#define CHAP_INTERFACE_status_register(chap)                    (chap)->status_register
#define CHAP_INTERFACE_data_register(chap)                      (chap)->data_register

/**************************************************************************
 * GMCH Registers and Offsets
 **************************************************************************
 */

// Counter registers - each counter has 4 registers
#define GMCH_MSG_CTRL_REG               0xD0        // message control register (MCR) 0xD0-0xD3
#define GMCH_MSG_DATA_REG               0xD4        // message data register (MDR) 0xD4-0xD7

// Counter register offsets
#define GMCH_PMON_CAPABILITIES          0x0005F0F0  // when read, bit 0 enabled means GMCH counters are available
#define GMCH_PMON_GLOBAL_CTRL           0x0005F1F0  // simultaneously enables or disables fixed and general counters

// Fixed counters (32-bit)
#define GMCH_PMON_FIXED_CTR_CTRL        0x0005F4F0  // enables and filters the fixed counters
#define GMCH_PMON_FIXED_CTR0            0x0005E8F0  // 32-bit fixed counter for GMCH_CORE_CLKS event
#define GMCH_PMON_FIXED_CTR_OVF_VAL     0xFFFFFFFFLL  // overflow value for GMCH fixed counters

// General counters (38-bit)
// NOTE: lower order bits on GP counters must be read before the higher bits!
#define GMCH_PMON_GP_CTR0_L             0x0005F8F0  // GMCH GP counter 0, low bits
#define GMCH_PMON_GP_CTR0_H             0x0005FCF0  // GMCH GP counter 0, high bits
#define GMCH_PMON_GP_CTR1_L             0x0005F9F0
#define GMCH_PMON_GP_CTR1_H             0x0005FDF0
#define GMCH_PMON_GP_CTR2_L             0x0005FAF0
#define GMCH_PMON_GP_CTR2_H             0x0005FEF0
#define GMCH_PMON_GP_CTR3_L             0x0005FBF0
#define GMCH_PMON_GP_CTR3_H             0x0005FFF0
#define GMCH_PMON_GP_CTR_OVF_VAL        0x3FFFFFFFFFLL // overflow value for GMCH general counters

/* other counter register offsets ...
#define GMCH_PMON_GLOBAL_STATUS         0x0005F2F0  // bit 16 indicates overflow on fixed counter 0; bits 0-3 indicate overflows on GP counters 0-3
#define GMCH_PMON_GLOBAL_OVF_CTRL       0x0005F3F0  // on CDV, it is write-only psuedo-register that always returns 0 when read
#define GMCH_PMON_PERFEVTSEL0           0x0005E0F0  // this is used for selecting which event in GP counter 0 to count
#define GMCH_PMON_PERFEVTSEL1           0x0005E1F0  // this is used for selecting which event in GP counter 1 to count
#define GMCH_PMON_PERFEVTSEL2           0x0005E2F0  // this is used for selecting which event in GP counter 2 to count
#define GMCH_PMON_PERFEVTSEL3           0x0005E3F0  // this is used for selecting which event in GP counter 3 to count
#define GMCH_PERF_ADDR_LIMIT_H          0x0001E8F0  // used for qualifying upper address limit for DRAM_PAGE_STATUS event
#define GMCH_PERF_ADDR_LIMIT_L          0x0001E9F0  // used for qualifying lower address limit for DRAM_PAGE_STATUS event
#define GMCH_PERF_BANK_SEL              0x0001EAF0  // used for addtional qualification of DRAM_PAGE_STATUS event
*/

// Register offsets for LNC
#define LNC_GMCH_REGISTER_READ          0xD0000000
#define LNC_GMCH_REGISTER_WRITE         0xE0000000

// Register offsets for SLT
#define SLT_GMCH_REGISTER_READ          0x10000000
#define SLT_GMCH_REGISTER_WRITE         0x11000000

// Register offsets for CDV
#define CDV_GMCH_REGISTER_READ          0x10000000
#define CDV_GMCH_REGISTER_WRITE         0x11000000


#if defined(__cplusplus)
}
#endif

#endif

