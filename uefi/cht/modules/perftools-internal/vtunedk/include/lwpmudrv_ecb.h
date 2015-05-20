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

#ifndef _LWPMUDRV_ECB_H_
#define _LWPMUDRV_ECB_H_

#if defined(DRV_OS_WINDOWS)
#pragma warning (disable:4200)
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// control register types
#define CCCR                1   // counter configuration control register
#define ESCR                2   // event selection control register
#define DATA                4   // collected as snapshot of current value
#define DATA_RO_DELTA       8   // read-only counter collected as current-previous
#define DATA_RO_SS          16  // read-only counter collected as snapshot of current value

// event multiplexing modes
#define EM_DISABLED                -1
#define EM_TIMER_BASED              0
#define EM_EVENT_BASED_PROFILING    1
#define EM_TRIGGER_BASED            2

// ***************************************************************************

/*!\struct EVENT_DESC_NODE
 * \var    sample_size                   - size of buffer in bytes to hold the sample + extras
 * \var    max_gp_events                 - max number of General Purpose events per EM group
 * \var    pebs_offset                   - offset in the sample to locate the pebs capture information
 * \var    lbr_offset                    - offset in the sample to locate the lbr information
 * \var    lbr_num_regs                  - offset in the sample to locate the number of lbr register information
 * \var    latency_offset_in_sample      - offset in the sample to locate the latency information
 * \var    latency_size_in_sample        - size of latency records in the sample
 * \var    latency_size_from_pebs_record - size of the latency data from pebs record in the sample
 * \var    latency_offset_in_pebs_record - offset in the sample to locate the latency information
 *                                         in pebs record
 * \var    power_offset_in_sample        - offset in the sample to locate the power information
 * \var    ebc_offset                    - offset in the sample to locate the ebc count information
 * \var    uncore_ebc_offset             - offset in the sample to locate the uncore ebc count information
 *
 * \var    ro_offset                     - offset of RO data in the sample
 * \var    ro_count                      - total number of RO entries (including all of IEAR/DEAR/BTB/IPEAR)
 * \var    iear_offset                   - offset into RO data at which IEAR entries begin
 * \var    dear_offset                   - offset into RO data at which DEAR entries begin
 * \var    btb_offset                    - offset into RO data at which BTB entries begin (these use the same PMDs)
 * \var    ipear_offset                  - offset into RO data at which IPEAR entries begin (these use the same PMDs)
 * \var    iear_count                    - number of IEAR entries
 * \var    dear_count                    - number of DEAR entries
 * \var    btb_count                     - number of BTB entries
 * \var    ipear_count                   - number of IPEAR entries
 *
 * \var    gfx_offset                    - offset in the sample to locate the gfx count information
 * \var    pwr_offset                    - offset in the sample to locate the pwr count information
 * \var    p_state_offset                - offset in the sample to locate the p_state information (APERF/MPERF)
 *
 * \brief  Data structure to describe the events and the mode
 *
 */

typedef struct EVENT_DESC_NODE_S  EVENT_DESC_NODE;
typedef        EVENT_DESC_NODE   *EVENT_DESC;

struct EVENT_DESC_NODE_S {
    U32     sample_size;
    U32     pebs_offset;
    U32     pebs_size;
    U32     lbr_offset;
    U32     lbr_num_regs;
    U32     latency_offset_in_sample;
    U32     latency_size_in_sample;
    U32     latency_size_from_pebs_record;
    U32     latency_offset_in_pebs_record;
    U32     power_offset_in_sample;
    U32     ebc_offset;
    U32     uncore_ebc_offset;
    U32     eventing_ip_offset;
    U32     hle_offset;
    U32     gfx_offset;
    U32     pwr_offset;
    U32     callstack_offset;
    U32     callstack_size;
    U32     p_state_offset;
    U32     pebs_tsc_offset;
};

//
// Accessor macros for EVENT_DESC node
//
#define EVENT_DESC_sample_size(ec)                        (ec)->sample_size
#define EVENT_DESC_pebs_offset(ec)                        (ec)->pebs_offset
#define EVENT_DESC_pebs_size(ec)                          (ec)->pebs_size
#define EVENT_DESC_lbr_offset(ec)                         (ec)->lbr_offset
#define EVENT_DESC_lbr_num_regs(ec)                       (ec)->lbr_num_regs
#define EVENT_DESC_latency_offset_in_sample(ec)           (ec)->latency_offset_in_sample
#define EVENT_DESC_latency_size_from_pebs_record(ec)      (ec)->latency_size_from_pebs_record
#define EVENT_DESC_latency_offset_in_pebs_record(ec)      (ec)->latency_offset_in_pebs_record
#define EVENT_DESC_latency_size_in_sample(ec)             (ec)->latency_size_in_sample
#define EVENT_DESC_power_offset_in_sample(ec)             (ec)->power_offset_in_sample
#define EVENT_DESC_ebc_offset(ec)                         (ec)->ebc_offset
#define EVENT_DESC_uncore_ebc_offset(ec)                  (ec)->uncore_ebc_offset
#define EVENT_DESC_eventing_ip_offset(ec)                 (ec)->eventing_ip_offset
#define EVENT_DESC_hle_offset(ec)                         (ec)->hle_offset
#define EVENT_DESC_gfx_offset(ec)                         (ec)->gfx_offset
#define EVENT_DESC_pwr_offset(ec)                         (ec)->pwr_offset
#define EVENT_DESC_callstack_offset(ec)                   (ec)->callstack_offset
#define EVENT_DESC_callstack_size(ec)                     (ec)->callstack_size
#define EVENT_DESC_p_state_offset(ec)                     (ec)->p_state_offset
#define EVENT_DESC_pebs_tsc_offset(ec)                    (ec)->pebs_tsc_offset

// ***************************************************************************

/*!\struct EVENT_CONFIG_NODE
 * \var    num_groups      -  The number of groups being programmed
 * \var    em_mode         -  Is EM valid?  If so how?
 * \var    em_time_slice   -  EM valid?  time slice in milliseconds
 * \var    sample_size     -  size of buffer in bytes to hold the sample + extras
 * \var    max_gp_events   -  Max number of General Purpose events per EM group
 * \var    pebs_offset     -  offset in the sample to locate the pebs capture information
 * \var    lbr_offset      -  offset in the sample to locate the lbr information
 * \var    lbr_num_regs    -  offset in the sample to locate the lbr information
 * \var    latency_offset_in_sample      -  offset in the sample to locate the latency information
 * \var    latency_size_in_sample        -  size of latency records in the sample
 * \var    latency_size_from_pebs_record -  offset in the sample to locate the latency
 *                                          size from pebs record
 * \var    latency_offset_in_pebs_record -  offset in the sample to locate the latency information
 *                                          in pebs record
 * \var    power_offset_in_sample        -  offset in the sample to locate the power information
 * \var    ebc_offset                    -  offset in the sample to locate the ebc count information
 *
 * \var    gfx_offset                    -  offset in the sample to locate the gfx count information
 * \var    pwr_offset                    -  offset in the sample to locate the pwr count information
 * \var    p_state_offset                -  offset in the sample to locate the p_state information (APERF/MPERF)
 *
 * \brief  Data structure to describe the events and the mode
 *
 */

typedef struct EVENT_CONFIG_NODE_S  EVENT_CONFIG_NODE;
typedef        EVENT_CONFIG_NODE   *EVENT_CONFIG;

struct EVENT_CONFIG_NODE_S {
    U32     num_groups;
    S32     em_mode;
    S32     em_factor;
    S32     em_event_num;
    U32     sample_size;
    U32     max_gp_events;
    U32     max_fixed_counters;
    U32     max_ro_counters;    // maximum read-only counters
    U32     pebs_offset;
    U32     pebs_size;
    U32     lbr_offset;
    U32     lbr_num_regs;
    U32     latency_offset_in_sample;
    U32     latency_size_in_sample;
    U32     latency_size_from_pebs_record;
    U32     latency_offset_in_pebs_record;
    U32     power_offset_in_sample;
    U32     ebc_offset;
    U32     num_groups_unc;
    U32     ebc_offset_unc;
    U32     sample_size_unc;
    U32     eventing_ip_offset;
    U32     hle_offset;
    U32     gfx_offset;
    U32     pwr_offset;
    U32     callstack_offset;
    U32     callstack_size;
    U32     p_state_offset;
    U32     pebs_tsc_offset;
};

//
// Accessor macros for EVENT_CONFIG node
//
#define EVENT_CONFIG_num_groups(ec)                         (ec)->num_groups
#define EVENT_CONFIG_mode(ec)                               (ec)->em_mode
#define EVENT_CONFIG_em_factor(ec)                          (ec)->em_factor
#define EVENT_CONFIG_em_event_num(ec)                       (ec)->em_event_num
#define EVENT_CONFIG_sample_size(ec)                        (ec)->sample_size
#define EVENT_CONFIG_max_gp_events(ec)                      (ec)->max_gp_events
#define EVENT_CONFIG_max_fixed_counters(ec)                 (ec)->max_fixed_counters
#define EVENT_CONFIG_max_ro_counters(ec)                    (ec)->max_ro_counters
#define EVENT_CONFIG_pebs_offset(ec)                        (ec)->pebs_offset
#define EVENT_CONFIG_pebs_size(ec)                          (ec)->pebs_size
#define EVENT_CONFIG_lbr_offset(ec)                         (ec)->lbr_offset
#define EVENT_CONFIG_lbr_num_regs(ec)                       (ec)->lbr_num_regs
#define EVENT_CONFIG_latency_offset_in_sample(ec)           (ec)->latency_offset_in_sample
#define EVENT_CONFIG_latency_size_from_pebs_record(ec)      (ec)->latency_size_from_pebs_record
#define EVENT_CONFIG_latency_offset_in_pebs_record(ec)      (ec)->latency_offset_in_pebs_record
#define EVENT_CONFIG_latency_size_in_sample(ec)             (ec)->latency_size_in_sample
#define EVENT_CONFIG_power_offset_in_sample(ec)             (ec)->power_offset_in_sample
#define EVENT_CONFIG_ebc_offset(ec)                         (ec)->ebc_offset
#define EVENT_CONFIG_num_groups_unc(ec)                     (ec)->num_groups_unc
#define EVENT_CONFIG_ebc_offset_unc(ec)                     (ec)->ebc_offset_unc
#define EVENT_CONFIG_sample_size_unc(ec)                    (ec)->sample_size_unc
#define EVENT_CONFIG_eventing_ip_offset(ec)                 (ec)->eventing_ip_offset
#define EVENT_CONFIG_hle_offset(ec)                         (ec)->hle_offset
#define EVENT_CONFIG_gfx_offset(ec)                         (ec)->gfx_offset
#define EVENT_CONFIG_pwr_offset(ec)                         (ec)->pwr_offset
#define EVENT_CONFIG_callstack_offset(ec)                   (ec)->callstack_offset
#define EVENT_CONFIG_callstack_size(ec)                     (ec)->callstack_size
#define EVENT_CONFIG_p_state_offset(ec)                     (ec)->p_state_offset
#define EVENT_CONFIG_pebs_tsc_offset(ec)                    (ec)->pebs_tsc_offset

typedef enum {
    UNC_VISA = 1,
    UNC_CHAP
} UNC_SA_PROG_TYPE;

typedef enum {
    UNC_PCICFG = 1,
    UNC_MMIO
} UNC_SA_CONFIG_TYPE;

typedef enum {
    UNC_MCHBAR = 1,
    UNC_DMIBAR,
    UNC_PCIEXBAR,
    UNC_GTTMMADR,
    UNC_GDXCBAR,
    UNC_CHAPADR,
    UNC_SIDEBAND
} UNC_SA_BAR_TYPE;

typedef enum {
    UNC_OP_READ =  1,
    UNC_OP_WRITE,
    UNC_OP_RMW
} UNC_SA_OPERATION;


typedef enum {
    STATIC_COUNTER = 1,
    FREERUN_COUNTER
} COUNTER_TYPES;

typedef enum {
    PACKAGE_EVENT = 1,
    MODULE_EVENT,
    THREAD_EVENT,
    SYSTEM_EVENT
} EVENT_SCOPE_TYPES;

// ***************************************************************************

/*!\struct PCI_ID_NODE
 * \var    offset      -  PCI offset to start the read/write
 * \var    data size      Number of bytes to operate on
 */

typedef struct PCI_ID_NODE_S    PCI_ID_NODE;
typedef        PCI_ID_NODE      *PCI_ID;

struct PCI_ID_NODE_S {
    U32        offset;
    U32        data_size;
};
#define PCI_ID_offset(x)      (x)->offset
#define PCI_ID_data_size(x)   (x)->data_size

// ***************************************************************************

/*!\struct EVENT_REG_ID_NODE
 * \var    reg_id      -  MSR index to r/w
 * \var    pci_id     PCI based register and its details to operate on
 */
typedef union EVENT_REG_ID_NODE_S EVENT_REG_ID_NODE;
typedef       EVENT_REG_ID_NODE  *EVENT_REG_ID;

 union EVENT_REG_ID_NODE_S {
   U16            reg_id;
   PCI_ID_NODE    pci_id;
} ;


// ***************************************************************************

/*!\struct EVENT_REG_NODE
 * \var    reg_type             - register type
 * \var    event_id_index       - event ID index
 * \var    event_id_index_local - event ID index within the device
 * \var    event_reg_id         - register ID/pci register details
 * \var    desc_id              - desc ID
 * \var    flags                - flags
 * \var    reg_value            - register value
 * \var    max_bits             - max bits
 * \var    scheduled            - boolean to specify if this event node has been scheduled already
 * \var    bus_no               - PCI bus number
 * \var    dev_no               - PCI device number
 * \var    func_no              - PCI function number
 * \var    counter_type         - Event counter type - static/freerun
 * \var    event_scope          - Event scope - package/module/thread
 * \var
 * \brief  Data structure to describe the event registers
 *
 */

typedef struct EVENT_REG_NODE_S  EVENT_REG_NODE;
typedef        EVENT_REG_NODE   *EVENT_REG;

struct EVENT_REG_NODE_S {
    U8                   reg_type;
    U8                   event_id_index;       // U8 must be changed if MAX_EVENTS > 256
    U8                   event_id_index_local; // U8 must be changed if MAX_EVENTS > 256
    U8                   emon_event_id_index_local;
    U8                   group_index;
    U8                   reserved0;
    U16                  counter_event_offset;
    EVENT_REG_ID_NODE    event_reg_id;
    U16                  desc_id;
    U16                  flags;
    U32                  reserved1;
    U64                  reg_value;
    U64                  max_bits;
    U8                   scheduled;
    // PCI config-specific fields
    U8                   reserved2;
    U16                  reserved3;
    U32                  bus_no;
    U32                  dev_no;
    U32                  func_no;
    U32                  counter_type;
    U32                  event_scope;
};

//
// Accessor macros for EVENT_REG node
// Note: the flags field is not directly addressible to prevent hackery
//
#define EVENT_REG_reg_type(x,i)                    (x)[(i)].reg_type
#define EVENT_REG_event_id_index(x,i)              (x)[(i)].event_id_index
#define EVENT_REG_event_id_index_local(x,i)        (x)[(i)].event_id_index_local
#define EVENT_REG_emon_event_id_index_local(x,i)   (x)[(i)].emon_event_id_index_local
#define EVENT_REG_counter_event_offset(x,i)        (x)[(i)].counter_event_offset
#define EVENT_REG_group_index(x,i)                 (x)[(i)].group_index
#define EVENT_REG_counter_event_offset(x,i)        (x)[(i)].counter_event_offset
#define EVENT_REG_reg_id(x,i)                      (x)[(i)].event_reg_id.reg_id
#define EVENT_REG_pci_id(x,i)                      (x)[(i)].event_reg_id.pci_id
#define EVENT_REG_pci_id_offset(x,i)               (x)[(i)].event_reg_id.pci_id.offset
#define EVENT_REG_pci_id_size(x,i)                 (x)[(i)].event_reg_id.pci_id.data_size
#define EVENT_REG_desc_id(x,i)                     (x)[(i)].desc_id
#define EVENT_REG_reg_value(x,i)                   (x)[(i)].reg_value
#define EVENT_REG_max_bits(x,i)                    (x)[(i)].max_bits
#define EVENT_REG_scheduled(x,i)                   (x)[(i)].scheduled
// PCI config-specific fields
#define EVENT_REG_bus_no(x,i)                      (x)[(i)].bus_no
#define EVENT_REG_dev_no(x,i)                      (x)[(i)].dev_no
#define EVENT_REG_func_no(x,i)                     (x)[(i)].func_no

#define EVENT_REG_counter_type(x,i)                (x)[(i)].counter_type
#define EVENT_REG_event_scope(x,i)                 (x)[(i)].event_scope
#define EVENT_REG_event_scope(x,i)                 (x)[(i)].event_scope

//
// Config bits
//
#define EVENT_REG_precise_bit               0x00000001
#define EVENT_REG_global_bit                0x00000002
#define EVENT_REG_uncore_bit                0x00000004
#define EVENT_REG_uncore_q_rst_bit          0x00000008
#define EVENT_REG_latency_bit               0x00000010
#define EVENT_REG_is_gp_reg_bit             0x00000020
#define EVENT_REG_clean_up_bit              0x00000040
#define EVENT_REG_em_trigger_bit            0x00000080
#define EVENT_REG_lbr_value_bit             0x00000100
#define EVENT_REG_fixed_reg_bit             0x00000200
#define EVENT_REG_compound_ctr_sub_bit      0x00000400
#define EVENT_REG_compound_ctr_bit          0x00000800
#define EVENT_REG_multi_pkg_evt_bit         0x00001000
#define EVENT_REG_branch_evt_bit            0x00002000


//
// Accessor macros for config bits
//
#define EVENT_REG_precise_get(x,i)          ((x)[(i)].flags &   EVENT_REG_precise_bit)
#define EVENT_REG_precise_set(x,i)          ((x)[(i)].flags |=  EVENT_REG_precise_bit)
#define EVENT_REG_precise_clear(x,i)        ((x)[(i)].flags &= ~EVENT_REG_precise_bit)

#define EVENT_REG_global_get(x,i)           ((x)[(i)].flags &   EVENT_REG_global_bit)
#define EVENT_REG_global_set(x,i)           ((x)[(i)].flags |=  EVENT_REG_global_bit)
#define EVENT_REG_global_clear(x,i)         ((x)[(i)].flags &= ~EVENT_REG_global_bit)

#define EVENT_REG_uncore_get(x,i)           ((x)[(i)].flags &   EVENT_REG_uncore_bit)
#define EVENT_REG_uncore_set(x,i)           ((x)[(i)].flags |=  EVENT_REG_uncore_bit)
#define EVENT_REG_uncore_clear(x,i)         ((x)[(i)].flags &= ~EVENT_REG_uncore_bit)

#define EVENT_REG_uncore_q_rst_get(x,i)     ((x)[(i)].flags &   EVENT_REG_uncore_q_rst_bit)
#define EVENT_REG_uncore_q_rst_set(x,i)     ((x)[(i)].flags |=  EVENT_REG_uncore_q_rst_bit)
#define EVENT_REG_uncore_q_rst_clear(x,i)   ((x)[(i)].flags &= ~EVENT_REG_uncore_q_rst_bit)

#define EVENT_REG_latency_get(x,i)          ((x)[(i)].flags &   EVENT_REG_latency_bit)
#define EVENT_REG_latency_set(x,i)          ((x)[(i)].flags |=  EVENT_REG_latency_bit)
#define EVENT_REG_latency_clear(x,i)        ((x)[(i)].flags &= ~EVENT_REG_latency_bit)

#define EVENT_REG_is_gp_reg_get(x,i)        ((x)[(i)].flags &   EVENT_REG_is_gp_reg_bit)
#define EVENT_REG_is_gp_reg_set(x,i)        ((x)[(i)].flags |=  EVENT_REG_is_gp_reg_bit)
#define EVENT_REG_is_gp_reg_clear(x,i)      ((x)[(i)].flags &= ~EVENT_REG_is_gp_reg_bit)

#define EVENT_REG_lbr_value_get(x,i)        ((x)[(i)].flags &   EVENT_REG_lbr_value_bit)
#define EVENT_REG_lbr_value_set(x,i)        ((x)[(i)].flags |=  EVENT_REG_lbr_value_bit)
#define EVENT_REG_lbr_value_clear(x,i)      ((x)[(i)].flags &= ~EVENT_REG_lbr_value_bit)

#define EVENT_REG_fixed_reg_get(x,i)        ((x)[(i)].flags &   EVENT_REG_fixed_reg_bit)
#define EVENT_REG_fixed_reg_set(x,i)        ((x)[(i)].flags |=  EVENT_REG_fixed_reg_bit)
#define EVENT_REG_fixed_reg_clear(x,i)      ((x)[(i)].flags &= ~EVENT_REG_fixed_reg_bit)

#define EVENT_REG_compound_ctr_bit_get(x,i)   ((x)[(i)].flags &   EVENT_REG_compound_ctr_bit)
#define EVENT_REG_compound_ctr_bit_set(x,i)   ((x)[(i)].flags |=  EVENT_REG_compound_ctr_bit)
#define EVENT_REG_compound_ctr_bit_clear(x,i) ((x)[(i)].flags &= ~EVENT_REG_compound_ctr_bit)

#define EVENT_REG_compound_ctr_sub_bit_get(x,i)   ((x)[(i)].flags &   EVENT_REG_compound_ctr_sub_bit)
#define EVENT_REG_compound_ctr_sub_bit_set(x,i)   ((x)[(i)].flags |=  EVENT_REG_compound_ctr_sub_bit)
#define EVENT_REG_compound_ctr_sub_bit_clear(x,i) ((x)[(i)].flags &= ~EVENT_REG_compound_ctr_sub_bit)

#define EVENT_REG_multi_pkg_evt_bit_get(x,i)   ((x)[(i)].flags &   EVENT_REG_multi_pkg_evt_bit)
#define EVENT_REG_multi_pkg_evt_bit_set(x,i)   ((x)[(i)].flags |=  EVENT_REG_multi_pkg_evt_bit)
#define EVENT_REG_multi_pkg_evt_bit_clear(x,i) ((x)[(i)].flags &= ~EVENT_REG_multi_pkg_evt_bit)

#define EVENT_REG_clean_up_get(x,i)         ((x)[(i)].flags &   EVENT_REG_clean_up_bit)
#define EVENT_REG_clean_up_set(x,i)         ((x)[(i)].flags |=  EVENT_REG_clean_up_bit)
#define EVENT_REG_clean_up_clear(x,i)       ((x)[(i)].flags &= ~EVENT_REG_clean_up_bit)

#define EVENT_REG_em_trigger_get(x,i)       ((x)[(i)].flags &   EVENT_REG_em_trigger_bit)
#define EVENT_REG_em_trigger_set(x,i)       ((x)[(i)].flags |=  EVENT_REG_em_trigger_bit)
#define EVENT_REG_em_trigger_clear(x,i)     ((x)[(i)].flags &= ~EVENT_REG_em_trigger_bit)

#define EVENT_REG_branch_evt_get(x,i)       ((x)[(i)].flags &   EVENT_REG_branch_evt_bit)
#define EVENT_REG_branch_evt_set(x,i)       ((x)[(i)].flags |=  EVENT_REG_branch_evt_bit)
#define EVENT_REG_branch_evt_clear(x,i)     ((x)[(i)].flags &= ~EVENT_REG_branch_evt_bit)


// ***************************************************************************

/*!\struct DRV_PCI_DEVICE_ENTRY_NODE_S
 * \var    bus_no          -  PCI bus no to read
 * \var    dev_no          -  PCI device no to read
 * \var    func_no            PCI device no to read
 * \var    bar_offset         BASE Address Register offset of the PCI based PMU
 * \var    bit_offset         Bit offset of the same
 * \var    size               size of read/write
 * \var    bar_address        the actual BAR present
 * \var    enable_offset      Offset info to enable/disable
 * \var    enabled            Status of enable/disable
 * \brief  Data structure to describe the PCI Device
 *
 */

typedef struct DRV_PCI_DEVICE_ENTRY_NODE_S  DRV_PCI_DEVICE_ENTRY_NODE;
typedef        DRV_PCI_DEVICE_ENTRY_NODE   *DRV_PCI_DEVICE_ENTRY;

struct DRV_PCI_DEVICE_ENTRY_NODE_S {
    U32        bus_no;
    U32        dev_no;
    U32        func_no;
    U32        bar_offset;
    U32        bit_offset;
    U32        size;
    U64        bar_address;
    U32        enable_offset;
    U32        enabled;
    U32        base_offset_for_mmio;
    U32        operation;
    U32        bar_name;
    U32        prog_type;
    U32        config_type;
    U32        reserved0;
    U64        value;
    U64        mask;
    U64        virtual_address;
    U32        port_id;
    U32        op_code;
};

//
// Accessor macros for DRV_PCI_DEVICE_NODE node
//
#define DRV_PCI_DEVICE_ENTRY_bus_no(x)                (x)->bus_no
#define DRV_PCI_DEVICE_ENTRY_dev_no(x)                (x)->dev_no
#define DRV_PCI_DEVICE_ENTRY_func_no(x)               (x)->func_no
#define DRV_PCI_DEVICE_ENTRY_bar_offset(x)            (x)->bar_offset
#define DRV_PCI_DEVICE_ENTRY_bit_offset(x)            (x)->bit_offset
#define DRV_PCI_DEVICE_ENTRY_size(x)                  (x)->size
#define DRV_PCI_DEVICE_ENTRY_bar_address(x)           (x)->bar_address
#define DRV_PCI_DEVICE_ENTRY_enable_offset(x)         (x)->enable_offset
#define DRV_PCI_DEVICE_ENTRY_enable(x)                (x)->enabled
#define DRV_PCI_DEVICE_ENTRY_base_offset_for_mmio(x)  (x)->base_offset_for_mmio
#define DRV_PCI_DEVICE_ENTRY_operation(x)             (x)->operation
#define DRV_PCI_DEVICE_ENTRY_bar_name(x)              (x)->bar_name
#define DRV_PCI_DEVICE_ENTRY_prog_type(x)             (x)->prog_type
#define DRV_PCI_DEVICE_ENTRY_config_type(x)           (x)->config_type
#define DRV_PCI_DEVICE_ENTRY_value(x)                 (x)->value
#define DRV_PCI_DEVICE_ENTRY_mask(x)                  (x)->mask
#define DRV_PCI_DEVICE_ENTRY_virtual_address(x)       (x)->virtual_address
#define DRV_PCI_DEVICE_ENTRY_port_id(x)               (x)->port_id
#define DRV_PCI_DEVICE_ENTRY_op_code(x)               (x)->op_code

// ***************************************************************************

/*!\struct ECB_NODE_S
 * \var    num_entries -       Total number of entries in "entries".
 * \var    group_id    -       Group ID.
 * \var    num_events  -       Number of events in this group.
 * \var    cccr_start  -       Starting index of counter configuration control registers in "entries".
 * \var    cccr_pop    -       Number of counter configuration control registers in "entries".
 * \var    escr_start  -       Starting index of event selection control registers in "entries".
 * \var    escr_pop    -       Number of event selection control registers in "entries".
 * \var    data_start  -       Starting index of data registers in "entries".
 * \var    data_pop    -       Number of data registers in "entries".
 * \var    pcidev_entry_node   PCI device details for one device
 * \var    entries     - .     All the register nodes required for programming
 *
 * \brief
 */

typedef struct ECB_NODE_S  ECB_NODE;
typedef        ECB_NODE   *ECB;

struct ECB_NODE_S {
    U32                          num_entries;
    U32                          group_id;
    U32                          num_events;
    U32                          cccr_start;
    U32                          cccr_pop;
    U32                          escr_start;
    U32                          escr_pop;
    U32                          data_start;
    U32                          data_pop;
    U16                          flags;
    U8                           pmu_timer_interval;
    U8                           reserved0;
    U32                          size_of_allocation;
    U32                          group_offset;
    DRV_PCI_DEVICE_ENTRY_NODE    pcidev_entry_node;
    U32                          num_pci_devices;
    U32                          pcidev_list_offset;
    DRV_PCI_DEVICE_ENTRY         pcidev_entry_list;
#if defined(DRV_IA32)
    U32                          reserved1;
#endif
    EVENT_REG_NODE               entries[];
};

//
// Accessor macros for ECB node
//
#define ECB_num_entries(x)                (x)->num_entries
#define ECB_group_id(x)                   (x)->group_id
#define ECB_num_events(x)                 (x)->num_events
#define ECB_cccr_start(x)                 (x)->cccr_start
#define ECB_cccr_pop(x)                   (x)->cccr_pop
#define ECB_escr_start(x)                 (x)->escr_start
#define ECB_escr_pop(x)                   (x)->escr_pop
#define ECB_data_start(x)                 (x)->data_start
#define ECB_data_pop(x)                   (x)->data_pop
#define ECB_pcidev_entry_node(x)          (x)->pcidev_entry_node
#define ECB_num_pci_devices(x)            (x)->num_pci_devices
#define ECB_pcidev_list_offset(x)         (x)->pcidev_list_offset
#define ECB_pcidev_entry_list(x)          (x)->pcidev_entry_list
#define ECB_flags(x)                      (x)->flags
#define ECB_pmu_timer_interval(x)         (x)->pmu_timer_interval
#define ECB_size_of_allocation(x)         (x)->size_of_allocation
#define ECB_group_offset(x)               (x)->group_offset
#define ECB_entries(x)                    (x)->entries

// for flag bit field
#define ECB_direct2core_bit                0x0001
#define ECB_bl_bypass_bit                  0x0002
#define ECB_pci_id_offset_bit              0x0003
#define ECB_pcu_ccst_debug                 0x0004

#define ECB_CONSTRUCT(x,num_entries,group_id,cccr_start,escr_start,data_start, size_of_allocation)    \
                                           ECB_num_entries((x)) = (num_entries);  \
                                           ECB_group_id((x)) = (group_id);        \
                                           ECB_cccr_start((x)) = (cccr_start);    \
                                           ECB_cccr_pop((x)) = 0;                 \
                                           ECB_escr_start((x)) = (escr_start);    \
                                           ECB_escr_pop((x)) = 0;                 \
                                           ECB_data_start((x)) = (data_start);    \
                                           ECB_data_pop((x)) = 0;                 \
                                           ECB_num_pci_devices((x)) = 0;          \
                                           ECB_size_of_allocation((x)) = (size_of_allocation);

#define ECB_CONSTRUCT1(x,num_entries,group_id,cccr_start,escr_start,data_start,num_pci_devices, size_of_allocation)    \
                                           ECB_num_entries((x)) = (num_entries);  \
                                           ECB_group_id((x)) = (group_id);        \
                                           ECB_cccr_start((x)) = (cccr_start);    \
                                           ECB_cccr_pop((x)) = 0;                 \
                                           ECB_escr_start((x)) = (escr_start);    \
                                           ECB_escr_pop((x)) = 0;                 \
                                           ECB_data_start((x)) = (data_start);    \
                                           ECB_data_pop((x)) = 0;                 \
                                           ECB_num_pci_devices((x)) = (num_pci_devices);  \
                                           ECB_size_of_allocation((x)) = (size_of_allocation);

//
// Accessor macros for ECB node entries
//
#define ECB_entries_reg_type(x,i)                    EVENT_REG_reg_type((ECB_entries(x)),(i))
#define ECB_entries_event_id_index(x,i)              EVENT_REG_event_id_index((ECB_entries(x)),(i))
#define ECB_entries_event_id_index_local(x,i)        EVENT_REG_event_id_index_local((ECB_entries(x)),(i))
#define ECB_entries_emon_event_id_index_local(x,i)   EVENT_REG_emon_event_id_index_local((ECB_entries(x)),(i))
#define ECB_entries_counter_event_offset(x,i)        EVENT_REG_counter_event_offset((ECB_entries(x)),(i))
#define ECB_entries_reg_id(x,i)                      EVENT_REG_reg_id((ECB_entries(x)),(i))
#define ECB_entries_pci_id(x,i)                      EVENT_REG_pci_id((ECB_entries(x)),(i))
#define ECB_entries_pci_id_offset(x,i)               EVENT_REG_pci_id_offset((ECB_entries(x)),(i))
#define ECB_entries_reg_value(x,i)                   EVENT_REG_reg_value((ECB_entries(x)),(i))
#define ECB_entries_max_bits(x,i)                    EVENT_REG_max_bits((ECB_entries(x)),(i))
#define ECB_entries_scheduled(x,i)                   EVENT_REG_scheduled((ECB_entries(x)),(i))
#define ECB_entries_group_index(x,i)                 EVENT_REG_group_index((ECB_entries(x)),(i))
#define ECB_entries_counter_event_offset(x,i)        EVENT_REG_counter_event_offset((ECB_entries(x)),(i))
// PCI config-specific fields
#define ECB_entries_bus_no(x,i)                      EVENT_REG_bus_no((ECB_entries(x)),(i))
#define ECB_entries_dev_no(x,i)                      EVENT_REG_dev_no((ECB_entries(x)),(i))
#define ECB_entries_func_no(x,i)                     EVENT_REG_func_no((ECB_entries(x)),(i))
#define ECB_entries_counter_type(x,i)                EVENT_REG_counter_type((ECB_entries(x)),(i))
#define ECB_entries_event_scope(x,i)                 EVENT_REG_event_scope((ECB_entries(x)),(i))
#define ECB_entries_precise_get(x,i)                 EVENT_REG_precise_get((ECB_entries(x)),(i))
#define ECB_entries_global_get(x,i)                  EVENT_REG_global_get((ECB_entries(x)),(i))
#define ECB_entries_uncore_get(x,i)                  EVENT_REG_uncore_get((ECB_entries(x)),(i))
#define ECB_entries_uncore_q_rst_get(x,i)            EVENT_REG_uncore_q_rst_get((ECB_entries(x)),(i))
#define ECB_entries_is_gp_reg_get(x,i)               EVENT_REG_is_gp_reg_get((ECB_entries(x)),(i))
#define ECB_entries_lbr_value_get(x,i)               EVENT_REG_lbr_value_get((ECB_entries(x)),(i))
#define ECB_entries_fixed_reg_get(x,i)               EVENT_REG_fixed_reg_get((ECB_entries(x)),(i))
#define ECB_entries_is_compound_ctr_bit_set(x,i)     EVENT_REG_compound_ctr_bit_get((ECB_entries(x)),(i))
#define ECB_entries_is_compound_ctr_sub_bit_set(x,i) EVENT_REG_compound_ctr_sub_bit_get((ECB_entries(x)),(i))
#define ECB_entries_is_multi_pkg_bit_set(x,i)        EVENT_REG_multi_pkg_evt_bit_get((ECB_entries(x)),(i))
#define ECB_entries_clean_up_get(x,i)                EVENT_REG_clean_up_get((ECB_entries(x)),(i))
#define ECB_entries_em_trigger_get(x,i)              EVENT_REG_em_trigger_get((ECB_entries(x)),(i))
#define ECB_entries_branch_evt_get(x,i)              EVENT_REG_branch_evt_get((ECB_entries(x)),(i))

// ***************************************************************************

typedef enum {
    LBR_ENTRY_TOS = 0,
    LBR_ENTRY_FROM_IP,
    LBR_ENTRY_TO_IP,
    LBR_ENTRY_INFO
} LBR_ENTRY_TYPE;

/*!\struct  LBR_ENTRY_NODE_S
 * \var     etype       TOS = 0; FROM = 1; TO = 2
 * \var     type_index
 * \var     reg_id
 */

typedef struct LBR_ENTRY_NODE_S  LBR_ENTRY_NODE;
typedef        LBR_ENTRY_NODE   *LBR_ENTRY;

struct LBR_ENTRY_NODE_S {
    U16    etype;
    U16    type_index;
    U32    reg_id;
};

//
// Accessor macros for LBR entries
//
#define LBR_ENTRY_NODE_etype(lentry)          (lentry).etype
#define LBR_ENTRY_NODE_type_index(lentry)     (lentry).type_index
#define LBR_ENTRY_NODE_reg_id(lentry)         (lentry).reg_id

// ***************************************************************************

/*!\struct LBR_NODE_S
 * \var    num_entries     -  The number of entries
 * \var    entries         -  The entries in the list
 *
 * \brief  Data structure to describe the LBR registers that need to be read
 *
 */

typedef struct LBR_NODE_S  LBR_NODE;
typedef        LBR_NODE   *LBR;

struct LBR_NODE_S {
    U32               size;
    U32               num_entries;
    LBR_ENTRY_NODE    entries[];
};

//
// Accessor macros for LBR node
//
#define LBR_size(lbr)                      (lbr)->size
#define LBR_num_entries(lbr)               (lbr)->num_entries
#define LBR_entries_etype(lbr,idx)         (lbr)->entries[idx].etype
#define LBR_entries_type_index(lbr,idx)    (lbr)->entries[idx].type_index
#define LBR_entries_reg_id(lbr,idx)        (lbr)->entries[idx].reg_id

// ***************************************************************************

/*!\struct  PWR_ENTRY_NODE_S
 * \var     etype       none as yet
 * \var     type_index
 * \var     reg_id
 */

typedef struct PWR_ENTRY_NODE_S  PWR_ENTRY_NODE;
typedef        PWR_ENTRY_NODE   *PWR_ENTRY;

struct PWR_ENTRY_NODE_S {
    U16    etype;
    U16    type_index;
    U32    reg_id;
};

//
// Accessor macros for PWR entries
//
#define PWR_ENTRY_NODE_etype(lentry)          (lentry).etype
#define PWR_ENTRY_NODE_type_index(lentry)     (lentry).type_index
#define PWR_ENTRY_NODE_reg_id(lentry)         (lentry).reg_id

// ***************************************************************************

/*!\struct PWR_NODE_S
 * \var    num_entries     -  The number of entries
 * \var    entries         -  The entries in the list
 *
 * \brief  Data structure to describe the PWR registers that need to be read
 *
 */

typedef struct PWR_NODE_S  PWR_NODE;
typedef        PWR_NODE   *PWR;

struct PWR_NODE_S {
    U32               size;
    U32               num_entries;
    PWR_ENTRY_NODE    entries[];
};

//
// Accessor macros for PWR node
//
#define PWR_size(lbr)                      (lbr)->size
#define PWR_num_entries(lbr)               (lbr)->num_entries
#define PWR_entries_etype(lbr,idx)         (lbr)->entries[idx].etype
#define PWR_entries_type_index(lbr,idx)    (lbr)->entries[idx].type_index
#define PWR_entries_reg_id(lbr,idx)        (lbr)->entries[idx].reg_id

// ***************************************************************************

/*!\struct  RO_ENTRY_NODE_S
 * \var     type       - DEAR, IEAR, BTB.
 */

typedef struct RO_ENTRY_NODE_S  RO_ENTRY_NODE;
typedef        RO_ENTRY_NODE   *RO_ENTRY;

struct RO_ENTRY_NODE_S {
    U32    reg_id;
};

//
// Accessor macros for RO entries
//
#define RO_ENTRY_NODE_reg_id(lentry)       (lentry).reg_id

// ***************************************************************************

/*!\struct RO_NODE_S
 * \var    size            - The total size including header and entries.
 * \var    num_entries     - The number of entries.
 * \var    entries         - The entries in the list.
 *
 * \brief  Data structure to describe the RO registers that need to be read.
 *
 */

typedef struct RO_NODE_S  RO_NODE;
typedef        RO_NODE   *RO;

struct RO_NODE_S {
    U32              size;
    U32              num_entries;
    RO_ENTRY_NODE    entries[];
};

//
// Accessor macros for RO node
//
#define RO_size(ro)                      (ro)->size
#define RO_num_entries(ro)               (ro)->num_entries
#define RO_entries_reg_id(ro,idx)        (ro)->entries[idx].reg_id

#if defined(__cplusplus)
}
#endif

#endif

