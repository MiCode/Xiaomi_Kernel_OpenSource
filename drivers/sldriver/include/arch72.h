/**
 * INTEL CONFIDENTIAL
 * Copyright 2011, 2012, 2013 Intel Corporation All Rights Reserved. 
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced, modified,
 * published, uploaded, posted, transmitted, distributed, or disclosed in any
 * way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#pragma once

//#ifndef _SE_ARCH_H_
//# error "never include arch72.h directly; use arch.h instead."
//#endif

#ifdef ANDROID
# define NUM_ARGS(...) NUM_ARGS_HELP1(__VA_ARGS__, 3, 2, 1, 0)
# define NUM_ARGS_HELP1(...) NUM_ARGS_HELP2(__VA_ARGS__)
# define NUM_ARGS_HELP2(x1, x2, x3, n, ...) n

#define _ASSERT_CONCAT(a, b) a##b
#define ASSERT_CONCAT(a, b) _ASSERT_CONCAT(a, b)

#define static_assert2(e,x) 	typedef char ASSERT_CONCAT(se_static_, __LINE__)[(e)?1:-1]
#define static_assert3(e1,e2,x) typedef char ASSERT_CONCAT(se_static_, __LINE__)[(e1,e2)?1:-1]

#define static_assertx(s1,s2, s3, macro, ...) macro
#define static_assert(e,x,...)  static_assertx(e,x,##__VA_ARGS__, \
                                                static_assert3(e,x,__VA_ARGS__), \
                                                static_assert2(e,x)) 
#endif

#ifndef _SE_ARCH72_H_
#define _SE_ARCH72_H_

#include "se_types.h"
#include "se_attributes.h"
#include "se_key.h"
#include "se_report.h"

#define SE_PAGE_SIZE 0x1000

#pragma pack(push, 1)

#if !defined(__cplusplus) || defined(__INTEL_COMPILER) || (defined(SE_GNU) && !defined(__GXX_EXPERIMENTAL_CXX0X__))
#define _ASSERT_CONCAT(a, b) a##b
#define ASSERT_CONCAT(a, b) _ASSERT_CONCAT(a, b)
#define se_static_assert(e) typedef char ASSERT_CONCAT(assert_line, __LINE__)[(e)?1:-1]
#else
#define se_static_assert(e) static_assert(e,#e)
#endif

#define UUID_SIZE   (36)
typedef struct _per_core_t
{
    uint64_t  tcs;           // 0  tcs
    uint64_t  idtr;          // 8  asserted idtr
    uint64_t  gdtr;          // 16 asserted gdtr
    uint64_t  ldtr;          // 24 asserted ldtr
    uint64_t  tr;            // 32 asserted tr
    uint64_t  r0sp;          // 40 asserted r0 stack
    uint64_t  reserved1;     // 48
    uint64_t  reserved2;     // 56
} se_per_core_data_t;        // size 64 - always keep power of 2

//SECS data structure
typedef struct _secs_t
{
    uint64_t                    size;           // (  0) Size of the enclave in bytes
    PADDED_POINTER(void,        base);          // (  8) Base address of enclave
    uint32_t                    ssa_frame_size; // ( 16) size of 1 SSA frame in pages
#define SECS_RESERVED1_LENGTH 28
    uint8_t                     reserved1[SECS_RESERVED1_LENGTH];  // ( 20) reserved
    se_attributes_t             attributes;     // ( 48) ATTRIBUTES Flags Field
    se_measurement_t            mr_enclave;     // ( 64) Integrity Reg 0 - Enclave measurement
#define SECS_RESERVED2_LENGTH 32
    uint8_t                     reserved2[SECS_RESERVED2_LENGTH];  // ( 96) reserved
    se_measurement_t            mr_signer;      // (128) Integrity Reg 1 - Enclave signing key
#define SECS_RESERVED3_LENGTH 96
    uint8_t                     reserved3[SECS_RESERVED3_LENGTH];  // (160) reserved
    se_prod_id_t                isv_prod_id;    // (256) product ID of enclave
    se_isv_svn_t                isv_svn;        // (258) Security Version of the Enclave
    uint64_t                    eid;            // (260) Enclave id

#define SECS_RESERVED_LENGTH 3292
    uint8_t                     ui_reserved[SECS_RESERVED_LENGTH];//(268) Microcode Specific Reserved Fields
    PADDED_POINTER(void,        acr3);          // (3816) cr3 for view
    uint64_t                    scv;            // (3824) security cookie
    uint64_t                    os_cr3;         // (3832) cached OS cr3
#define SECS_MAX_CORES 8
    se_per_core_data_t          pcd[SECS_MAX_CORES]; //(3840)  per core data
} secs_t;

se_static_assert(sizeof(secs_t) == SE_PAGE_SIZE);

//TCS
// Thread State Definitions
#define SE_STATE_INACTIVE   0            //The TCS is available for a normal EENTER
#define SE_STATE_ACTIVE     1            //A Processor is currently executing in the context of this TCS

#define DBGOPTIN            1           //used by debugger

typedef struct _tcs_t
{
    uint64_t            state;          // (0)Indicates the current state of the thread.  SE_STATE_xxxx
    uint64_t            flags;          // (8)bit 0: DBGOPTION
    PADDED_DWORD(       ossa);          // (16)State Save Area
    uint32_t            cssa;           // (24)Current SSA slot
    uint32_t            nssa;           // (28)Number of SSA slots
    PADDED_DWORD(       oentry);        // (32)Offset in enclave to which control is transferred on EENTER if enclave INACTIVE state
    PADDED_POINTER(void,aep);           // (40)Interrupt return routine. Location reported as exit rip when an asyn exit from the enclave occurs
    PADDED_DWORD(       ofs_base);      // (48)When added to the base address of the enclave, produces the base address FS segment inside the enclave
    PADDED_DWORD(       ogs_base);      // (56)When added to the base address of the enclave, produces the base address GS segment inside the enclave
    uint32_t            ofs_limit;      // (64)Size to become the new FS limit in 32-bit mode
    uint32_t            ogs_limit;      // (68)Size to become the new GS limit in 32-bit mode

    uint16_t            save_fs_selector;        // (72)
    uint32_t            save_fs_desc_low;        // (74)
    uint32_t            save_fs_desc_high;       // (78)

    uint16_t            save_gs_selector;        // (82)
    uint32_t            save_gs_desc_low;        // (84)
    uint32_t            save_gs_desc_high;       // (88)

    uint32_t            ssa;                     // (92)
    uint32_t            eflags;                  // (96)
    PADDED_POINTER(void,os_cr3);                 // (100)
    uint64_t            ur0sp;                   // (108)

#define TCS_RESERVED_LENGTH 3980
    uint8_t             reserved[TCS_RESERVED_LENGTH];  // (116)unused reserved length
}tcs_t;

se_static_assert(sizeof(tcs_t) == SE_PAGE_SIZE);

/****************************************************************************
 * Definitions for SSA
 ****************************************************************************/
typedef struct _exit_info_t
{
    uint32_t    vector:8;               // Exception number of exceptions reported inside enclave
    uint32_t    exit_type:3;            // 3: Hardware exceptions, 6: Software exceptions
    uint32_t    reserved:20;
    uint32_t    valid:1;            // 0: unsupported exceptions, 1: Supported exceptions
} exit_info_t;

#define SE_VECTOR_DE    0
#define SE_VECTOR_DB    1
#define SE_VECTOR_BP    3
#define SE_VECTOR_BR    5
#define SE_VECTOR_UD    6
#define SE_VECTOR_MF    16
#define SE_VECTOR_AC    17
#define SE_VECTOR_XM    19

typedef struct _ssa_gpr_t
{
    REGISTER(   ax);                    // (0)
    REGISTER(   cx);                    // (8)
    REGISTER(   dx);                    // (16)
    REGISTER(   bx);                    // (24)
    REGISTER(   sp);                    // (32)
    REGISTER(   bp);                    // (40)
    REGISTER(   si);                    // (48)
    REGISTER(   di);                    // (56)
    uint64_t    r8;                     // (64)
    uint64_t    r9;                     // (72)
    uint64_t    r10;                    // (80)
    uint64_t    r11;                    // (88)
    uint64_t    r12;                    // (96)
    uint64_t    r13;                    // (104)
    uint64_t    r14;                    // (112)
    uint64_t    r15;                    // (120)
    REGISTER(flags);                    // (128)
    REGISTER(   ip);                    // (136)
    REGISTER( sp_u);                    // (144) untrusted stack pointer. saved by EENTER
    REGISTER( bp_u);                    // (152) untrusted frame pointer. saved by EENTER
    exit_info_t exit_info;              // (160) contain information for exits
    uint32_t    reserved;               // (164) padding to multiple of 8 bytes
} ssa_gpr_t;

/****************************************************************************
 * Definitions for Page Information
 ****************************************************************************/
typedef struct secinfo_flags_t
{
  union {
    struct{
      uint32_t    r:1;
      uint32_t    w:1;
      uint32_t    x:1;
      uint32_t    pt:2;
      uint32_t    reserved: 27;
      uint32_t    reserved1;
    }s1;
    uint64_t bits;
  }u1;
} secinfo_flags_t;

typedef uint64_t si_flags_t;

#define SI_FLAG_R           0x1             // Read Access
#define SI_FLAG_W           0x2             // Write Access
#define SI_FLAG_X           0x4             // Execute Access
#define SI_FLAG_PT_LOW_BIT  0x8                             // PT low bit
#define SI_FLAG_PT_MASK     (0xFF<<SI_FLAG_PT_LOW_BIT)      // Page Type Mask [15:8]
#define SI_FLAG_SECS        (0x00<<SI_FLAG_PT_LOW_BIT)      // SECS
#define SI_FLAG_TCS         (0x01<<SI_FLAG_PT_LOW_BIT)      // TCS
#define SI_FLAG_REG         (0x02<<SI_FLAG_PT_LOW_BIT)      // Regular Page

#define SI_FLAGS_EXTERNAL   (SI_FLAG_PT_MASK | SI_FLAG_R | SI_FLAG_W | SI_FLAG_X)   // Flags visible/usable by instructions
#define SI_FLAGS_R          (SI_FLAG_R|SI_FLAG_REG)
#define SI_FLAGS_RW         (SI_FLAG_R|SI_FLAG_W|SI_FLAG_REG)
#define SI_FLAGS_RX         (SI_FLAG_R|SI_FLAG_X|SI_FLAG_REG)
#define SI_FLAGS_TCS        (SI_FLAG_TCS)
#define SI_FLAGS_SECS       (SI_FLAG_SECS)
#define SI_MASK_TCS         (SI_FLAG_PT_MASK)


typedef struct _sec_info_t
{
   si_flags_t        flags;
   uint64_t          reserved[7];
} sec_info_t;

typedef struct _page_info_t
{
    PADDED_POINTER(void,        lin_addr);      // Enclave linear address
    PADDED_POINTER(void,        src_page);      // Linear address of the page where contents are located
    PADDED_POINTER(sec_info_t,  sec_info);      // Linear address of the SEC_INFO structure for the page
    PADDED_POINTER(void,        secs);          // Linear address of EPC slot that contains SECS for this enclave
} page_info_t;

/****************************************************************************
* Definitions for enclave signature
****************************************************************************/
#define SE_KEY_SIZE         384         // in bytes
#define SE_EXPONENT_SIZE    4           // RSA public key exponent size in bytes

typedef struct _css_header_t {          // 128 bytes
    uint32_t header_type;               // (0)  Module type (Must be 7)
    uint32_t header_len;                // (4)  Header length including crypto fields (dwords) (Must be 00A1h)
    uint32_t header_version;            // (8)  Struct version (Must be 0101h)
    uint32_t type;                      // (12) bit 31: 0 = prod, 1 = debug; Bit 30-0: Must be zero
    uint32_t module_vendor;             // (16) Intel=0x8086, ISV=0x0000
    uint32_t date;                      // (20) build date as yyyymmdd
    uint32_t size;                      // (24) Size of the entire module (dwords) (Must be 0101h)
    uint32_t key_size;                  // (28) RSA public key modulus size (dwords) (Must be 0060h in SE1.5)
    uint32_t modulus_size;              // (32) RSA public key modulus size (dwords) (Must be 0060h in SE1.5)
    uint32_t exponent_size;             // (36) RSA public key exponent size (dword) (Must be 0001h in SE1.5)
    uint32_t hw_version;                // (40) For Architectural Enclaves: refer to pas. For Non-Architectural Enclaves: HWVERSION = 0
    uint8_t  reserved[84];              // (44) Must be 0
} css_header_t;
se_static_assert(sizeof(css_header_t) == 128);

typedef struct _css_key_t {             // 772 bytes
    uint8_t modulus[SE_KEY_SIZE];       // (128) Module Public Key (keylength=3072 bits)
    uint8_t exponent[SE_EXPONENT_SIZE]; // (512) RSA Exponent = 3
    uint8_t signature[SE_KEY_SIZE];     // (516) Signature over Header and Body
} css_key_t;
se_static_assert(sizeof(css_key_t) == 772);

typedef struct _css_body_t {            // 128 bytes
    uint8_t             reserved[28];   // (900) Reserved. Must be 0.
    se_attributes_t     attributes;     // (928) Enclave Attributes that must be set
    se_attributes_t     attribute_mask; // (944) Mask of Attributes to Enforce
    se_measurement_t    enclave_hash;   // (960) MRENCLAVE - (32 bytes)
    uint8_t             reserved2[32];  // (992) Must be 0
    uint16_t            isv_prod_id;    // (1024) ISV assigned Product ID
    uint16_t            isv_svn;        // (1026) ISV assigned SVN
} css_body_t;
se_static_assert(sizeof(css_body_t) == 128);

typedef struct _css_buffer_t {          // 780 bytes
    uint8_t  reserved[12];              // (1028) Must be 0
    uint8_t  q1[SE_KEY_SIZE];           // (1040) Q1 value for RSA Signature Verification
    uint8_t  q2[SE_KEY_SIZE];           // (1424) Q2 value for RSA Signature Verification
} css_buffer_t;
se_static_assert(sizeof(css_buffer_t) == 780);

typedef struct _enclave_css_t {         // 1808 bytes
    css_header_t    header;             // (0)
    css_key_t       key;                // (128)
    css_body_t      body;               // (900)
    css_buffer_t    buffer;             // (1028)
} enclave_css_t;

se_static_assert(sizeof(enclave_css_t) == 1808);

/****************************************************************************
* Definitions for license
****************************************************************************/
typedef struct _license_body_t
{
   uint32_t              valid;            // (  0) 0 = Invalid, 1 = Valid
   uint32_t              reserved1[11];    // (  4) must be zero
   se_attributes_t       attributes;       // ( 48) ATTRIBUTES of Enclave
   se_measurement_t      mr_enclave;       // ( 64) MRENCLAVE of Enclave
   uint8_t               reserved2[32];    // ( 96)
   se_measurement_t      mr_signer;        // (128) MRSIGNER of Enclave
   uint8_t               reserved3[32];    // (160)
} license_body_t;
se_static_assert(sizeof(license_body_t) == 192);

typedef struct _license_t {
  license_body_t         body;
  se_cpu_svn_t           cpu_svn;       // (192) License Enclave's CPUSVN
  uint16_t               isv_prod_id;   // (208) License Enclave's ISVPRODID
  uint16_t               isv_svn;       // (210) License Enclave's ISVSVN
  uint8_t                reserved2[28]; // (212) Must be 0
  se_attributes_t        attributes_le; // (240) ATTRIBUTES of License Enclave
  se_key_id_t            key_id;        // (256) Value for key wear-out protection
  se_mac_t               mac;           // (288) CMAC using License Key
} license_t;
se_static_assert(sizeof(license_t) == 304);

typedef struct _license_blob_body_t {
  uint16_t               blob_rev;      //  (0) License blob revision ID (1 for SKL)
  uint8_t                reserved[10];  //  (2) License Enclave's ISVSVN
  uint32_t               licinfo ;      //  (12) License information
  uint64_t               lic_id[2];     //  (16) License ID
  se_attributes_t        lic_cert_attr; //  (32) ATTRIBUTES of License policy
  se_attributes_t        lic_cert_mask; //  (48) ATTRIBUTES mask of License policy
  uint64_t               lic_plat_type[2];    //  (64) License Platform type
} license_blob_body_t;

se_static_assert(sizeof(license_blob_body_t) == 80);

typedef struct _license_blob_t {
  license_t              lic_token;     //  (0) License token
  license_blob_body_t    lic_blob;      //  (304) License blob
  se_mac_t               mac;           //  (384) CMAC using License Key
} license_blob_t;

se_static_assert(sizeof(license_blob_t) == 400);


typedef struct _sku_blob_t {
  uint8_t                sku_info[16];     // (0)sku information, 16 byte
  se_cpu_svn_t           cpu_svn_le;       // (16)License Enclave’s CPUSVN
  uint16_t               isv_svn_le;       // (32)License Enclave’s ISVSVN
  uint8_t                reserved[6];      // (34)Must be 0
  se_key_id_t            key_id;           // (40) Value for key wear-out
  se_mac_t               mac;              // (72) CMAC using License Key
} sku_blob_t;
se_static_assert(sizeof(sku_blob_t) == 88);


#pragma pack(pop)

#endif//_SE_ARCH72_H_
