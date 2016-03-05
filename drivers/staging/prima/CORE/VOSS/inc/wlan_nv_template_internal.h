/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 *WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 *ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#if !defined _WLAN_NV_TEMPLATE_INTERNAL_H
#define  _WLAN_NV_TEMPLATE_INTERNAL_H

/*
  Template constructs
     1. TABLE_: struct
     2. INDEX_ENUM: enums, e.g. {RATE_OFDM_6M,RATE_OFDM_54M}
     3. INDEX_INT: int, e.g.{min, max, increment}
     3. Basic data types: tANI_U8, tANI_S8, tANI_U32, tANI_S32
     4. Storage types:
        4.1 SINGULAR: one element of basic data type
        4.2 ARRAY_1: one dimensional array, x-axis
        4.3 ARRAY_2: two dimensional array, (x, y)
        4.4 ARRAY_3: three dimensional array, (x, y, z)

  Implementation notes
     1. Flow of changing NV data format: (TBD) Either change the template and
        generate the header file, or
        modify header file and auto-generate the template.
     2. Flow of writing NV data: encode the template in the data stream, so the
        NV data is "self-sufficient".
        No separate template, no compability issue, no need of version control.
     3. Flow of reading NV data: parse the binary NV data stream based on the
        template info in the data stream.
     4. The above NV logic is decoupled from the actual data content, a
        generic, content ergonostic parser (reading) and encoder (writing).
        The NV logic is common code shared by tools, s/w
        (both host and firmware), and off-line utilities.
     5. NV data parsing and "acceptanace" into an s/w moduel data structure can
        be "configured" in several ways:
        5.1 only total matching of all fields, otherwise, reject the whole data
        stream (a table).
        5.2 partial matching of fields allowed and the rest fields assume
        reasonal default values,
        The choice can be determined later, but the capability is provided.
     6. We could also design in this selection on an individua table base.
        To design such capability, reserve some header bits in the data stream.
     7. The NV data streams can be modified, replaced, or intact with a new
        data stream of the same table ID added to NV data.
        The choice can be determined later, but the NV scheme provides such
        capability.
     8. The template construct definitions can be common to all tables
        (tbd: in a common section) or table specific, or updated
        in a subsequent format section.
        The use cases are:
        - An index enum (e.g. RF channels) is common to all tables when the NV
          data is created. Later new enums are added (e.g.
        additional channels), one can choose to add the new index enum for new
        tables appended to the NV data, or replace the
        old table with new template info and data.
        The template precedence is table specific then common, and later
        "common" overwrites "earlier" commmon.
        - A new field is added to the table, the user decides to replace the old
        table data, he can simply encode the template
        info in the data stream.
        - In the same scenario (a new field is added), the user decides to
        append a new table, he can encode the template
        in the new data table and append it to NV data, or write a new common
        template section and append the data.

  Key "ingredients", (re-iterate the most important features and capabilities)
     1. How to parse the data is embedded in the NV data itself. It removes the
        dependency on header file matching,
        version checking, compatibility among tools, host and firmware.
     2. Table field ID enables "partial" data acceptance in an s/w module data
        structure. Whether full matching or reject the whole table, or "partial"
        acceptance, the capabiilty is in place and further ensures the robust
        NV data extensibility and compatibility.
     3. The table granularity, data stream based NV data has variable length
        and flexibility of modifying an existing table data, replacing
        the whole data,or leaving the existing data table intact and
        appending a new table.
  Misc notes:
     1. For endianness, support only 4 bytes integer or 4 1-byte
     2. String identifier needs to be shortened to save storage
     3. string_field_name,  field type,  field storage class,  storage size
*/

#include "wlan_nv_types.h"

/*
 * Stream header bitmap
 *                       streamType
 *                        bitmap[7]
 *                      /                    \
 *                1: template               0: data
 *                 bitmap[6]
 *                /     \
 *             0: enum  1: table
 *
 */
/* Stream header type[7], 0:  data, 1:  template */
#define STREAM_HEADER_TYPE_MASK   0x80
#define STREAM_HEADER_TYPE_LSB    7
#define IsStreamTemplate(b)       (((b) & (STREAM_HEADER_TYPE_MASK)) ? 1 : 0)

/* Stream header template type [6],  0: enum; 1:  table */
#define STREAM_HEADER_TEMPLATE_TYPE_MASK   0x40
#define STREAM_HEADER_TEMPLATE_TYPE_LSB    6
#define IsTemplateStreamTable(b)   (((b) & (STREAM_HEADER_TEMPLATE_TYPE_MASK)) ? 1 : 0)

/*
 * Field identifier bitmap
 *
 *                field identifier
 *                bitmap[7]
 *                 /           \
 *            0: table/enum     1: basic data type
 *            bitmap[6:0]         bitmap[6:0]
 *               |                  |
 *            tableIdx/          data types (U8, U32, etc.)
 *            enumIdx
 */
/* Field Identifier type [7]
 *    0:  table
 *    1:  basic data types
 * Note that
 *    - bit[7] table value=0 makes the table ID following data header stream or
 *       template header stream identical to field ID
 *    - tableIdx 0 is the "table of all tables", a.k.a. table content of all
 *      table indexes
 *    - enumIdx 0 is the "enum of all enums", a.k.a. table content of all enum
 *      indexes
 */

#define FIELD_ID_TYPE_MASK                  0x80
#define FIELD_ID_TYPE_LSB                   7
#define IsFieldTypeBasicData(b)             (((b) & (FIELD_ID_TYPE_MASK)) ? 1 : 0)

/* Field Identifier table index [6:0] */
#define FIELD_ID_TABLE_OR_ENUM_IDX_MASK     0x7f
#define FIELD_ID_TABLE_OR_ENUM_IDX_LSB      0
#define _TABLE_IDX(b)                       (((b) & ~FIELD_ID_TYPE_MASK) | ((b) & FIELD_ID_TABLE_OR_ENUM_IDX_MASK))
#define IsIdxTableOfAllTables(b)            (((b) & FIELD_ID_TABLE_OR_ENUM_IDX_MASK) ? 0 : 1)
#define IsIdxEnumOfAllEnums(b)              (((b) & FIELD_ID_TABLE_OR_ENUM_IDX_MASK) ? 0 : 1)

/* Field Identifier basic data types [6:0]
 *    0:  U8
 *    1:  U32
 *    2:  S8
 *    3:  S32
 *    4:  U16
 *    5:  S16
 */

#define FIELD_ID_BASIC_DATA_TYPES_MASK      0x7F
#define FIELD_ID_BASIC_DATA_TYPES_LSB       0

typedef enum {
   _FIELD_ID_DATA_TYPE_U8 = 0,
   _FIELD_ID_DATA_TYPE_U32,
   _FIELD_ID_DATA_TYPE_S8,
   _FIELD_ID_DATA_TYPE_S32,
   _FIELD_ID_DATA_TYPE_U16,
   _FIELD_ID_DATA_TYPE_S16,
   _FIELD_ID_DATA_TYPE_LAST,
} _FIELD_ID_BASIC_DATA_TYPE;

#define TheBasicDataType(b)                 (((b) & (FIELD_ID_BASIC_DATA_TYPES_MASK)) >> FIELD_ID_BASIC_DATA_TYPES_LSB)
#define _ID_U8                              ((FIELD_ID_TYPE_MASK) | (_FIELD_ID_DATA_TYPE_U8))
#define _ID_U32                             ((FIELD_ID_TYPE_MASK) | (_FIELD_ID_DATA_TYPE_U32))
#define _ID_S8                              ((FIELD_ID_TYPE_MASK) | (_FIELD_ID_DATA_TYPE_S8))
#define _ID_S32                             ((FIELD_ID_TYPE_MASK) | (_FIELD_ID_DATA_TYPE_S32))
#define _ID_U16                             ((FIELD_ID_TYPE_MASK) | (_FIELD_ID_DATA_TYPE_U16))
#define _ID_S16                             ((FIELD_ID_TYPE_MASK) | (_FIELD_ID_DATA_TYPE_S16))

/*
 * field storage class
 */
typedef enum {
   SINGULAR = 0,
   ARRAY_1,
   ARRAY_2,
   ARRAY_3,
   STORAGE_TYPE_LAST,
} _FIELD_ID_STORAGE_TYPE;

#define _STORAGE_TYPE(b)   ((b) & 0x3)
#define _STORAGE_SIZE1(byteLow, byteHigh)   (((((byteHigh) >> 2) & 0x3) << 7) | (((byteLow) >> FIELD_SIZE_VALUE_LSB) & FIELD_SIZE_VALUE_MASK))
#define _STORAGE_SIZE2(byteLow, byteHigh)   (((((byteHigh) >> 4) & 0x3) << 7) | (((byteLow) >> FIELD_SIZE_VALUE_LSB) & FIELD_SIZE_VALUE_MASK))
#define _STORAGE_SIZE3(byteLow, byteHigh)   (((((byteHigh) >> 6) & 0x3) << 7) | (((byteLow) >> FIELD_SIZE_VALUE_LSB) & FIELD_SIZE_VALUE_MASK))

#define _ADD_SIZE1(b)  ((((b) >> 7) & 0x3) << 2)
#define _ADD_SIZE2(b)  ((((b) >> 7) & 0x3) << 4)
#define _ADD_SIZE3(b)  ((((b) >> 7) & 0x3) << 6)

/*
 * Field storage size type  [7]
 *         /       \
 *     1: int      0: enum
 *   bitmap[6:0]   bitmap[6:0]
 *       |             |
 *   max int index  enum index into enum tables
 *
 * Note that enum=0 makes the template enum ID following template stream byte
 * identical to enum field storage size type
 *
 * Field storage size value [6:0]
 */
#define FIELD_SIZE_TYPE_MASK              0x80
#define FIELD_SIZE_TYPE_LSB               7
#define FIELD_SIZE_TYPE_BIT(t)            (((t)<< (FIELD_SIZE_TYPE_LSB)) & (FIELD_SIZE_TYPE_MASK))
#define IsFieldSizeInt(b)                 (((b) & (FIELD_SIZE_TYPE_MASK)) ? 1 : 0)

typedef enum {
   FIELD_SIZE_IDX_ENUM = 0,
   FIELD_SIZE_IDX_INT = 1,
} FIELD_SIZE_TYPE;

#define FIELD_SIZE_VALUE_MASK             0x7f
#define FIELD_SIZE_VALUE_LSB              0
#define FIELD_SIZE_VALUE_BITS(val)        (((val) << (FIELD_SIZE_VALUE_LSB)) & (FIELD_SIZE_VALUE_MASK))

/*
 * NV table storage struct in an s/w module
 */
#define _TABLE_NAME_LEN  2
#define _TABLE_FIELD_FULL_NAME_LEN  47

typedef struct _nvTemplateTableStructInternal {
   tANI_U8   fieldName[_TABLE_NAME_LEN + 1];
   tANI_U8   fieldId;
   tANI_U8   fieldStorageType;
   tANI_U8   fieldStorageSize1;
   tANI_U8   fieldStorageSize2;
   tANI_U8   fieldStorageSize3;
   tANI_U32  offset; //void     *offset;
   tANI_U8   fieldFullName[_TABLE_FIELD_FULL_NAME_LEN +1];
} _NV_TEMPLATE_TABLE;

#define _OFFSET_NOT_SET        0xFFFFFFFF
#define TABLE_PREDEFINED_MAX   50
#define TABLE_ENTRIES_MAX      50
#define _LIST_OF_TABLES_IDX     0
#define _TABLE_FIELDS_POS       2
#define _ENUM_START_POS         2
#define _TABLE_FIELD_MIN_LEN    4
#define _ENUM_MIN_LEN           3

#define _ENUM_NAME_LEN _TABLE_NAME_LEN
#define _ENUM_FULL_NAME_LEN    47
typedef struct _nvTemplateEnumStruct {
   tANI_U8   enumName[3];  // 2 char string
   tANI_U8   enumValue;
   tANI_U8   enumValuePeer;
   tANI_U8   enumFullName[_ENUM_FULL_NAME_LEN +1];
} _NV_TEMPLATE_ENUM;
#define INDEX_ENUM_PREDEFINED_MAX    20
#define ENUM_ENTRIES_MAX             200

typedef enum {
   _MIS_MATCH = 0,
   _MATCH,
} _NV_TEMPLATE_PROCESS_RC;

#define _NV_BIN_STREAM_HEADER_BYTE          0
#define _NV_BIN_STREAM_TABLE_ID_BYTE        1
#define _NV_BIN_STREAM_ENUM_ID_BYTE         1
#define _NV_BIN_DATA_STREAM_TABLEID_BYTE    1
#define _NV_BIN_ENUM_TEMPLATE_ENTRY_SIZE    3
#define _NV_LIST_OF_TABLE_ID                0

/*
 * Stream write
 */
#define _STREAM_HEADER_POS            0
#define _ENUM_STREAM_HEADER_POS       _STREAM_HEADER_POS
#define _TABLE_STREAM_HEADER_POS      _STREAM_HEADER_POS
#define _TEMPLATE_INDEX_HEADER_POS    1
#define _ENUM_INDEX_HEADER_POS        _TEMPLATE_INDEX_HEADER_POS
#define _TABLE_INDEX_HEADER_POS       _TEMPLATE_INDEX_HEADER_POS

/*
 * Additional typedef
 */
typedef struct _enumMetaData {
   _NV_TEMPLATE_PROCESS_RC match;
} _ENUM_META_DATA;

#define MAX(a, b)     (((a) > (b)) ? (a) : (b))
#define _NV_STREAM_LEN_MAX           35000

/*
 * Error code should be expanded, this is the beginning set
 */
typedef enum {
   _OK = 0,
   _RESET_STREAM_FAILED,
   _WRITE_STREAM_FAILED,
   _STREAM_NOT_FIT_BUF,
   _SW_BIN_MISMATCH,
   _INSUFFICIENT_FOR_FIELD_PARSER_ERROR,
   _TABLE_NON_EXIST_IN_TABLE_OF_ALL_TABLES,
   _ENUM_NOT_FOUND_IN_BUILT_IN,
} _ErrorCode;

/*
 * Use the stream test stub
 */
//#define _USE_STREAM_STUB
#define RESET_STREAM(b)   resetStream(b)
#define NEXT_STREAM(b,c)    nextStream(b,c)

#endif /*#if !defined(_WLAN_NV_TEMPLATE_INTERNAL_H)*/
