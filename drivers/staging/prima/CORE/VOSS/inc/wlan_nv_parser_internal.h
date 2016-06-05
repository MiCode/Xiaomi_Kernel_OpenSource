/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#if !defined _WLAN_NV_PARSER_INTERNAL_H
#define  _WLAN_NV_PARSER_INTERNAL_H

#include "wlan_nv_types.h"

/*
 * local prototypes
 */
static _NV_TEMPLATE_PROCESS_RC processNvTemplate(_NV_STREAM_BUF *pStream,
   int len);
static _NV_TEMPLATE_PROCESS_RC processNvTemplateTable(_NV_STREAM_BUF *pStream,
   int len);
static int constructATemplateTable(_NV_STREAM_BUF *pStream, int len,
   char *tableStrName);
static _NV_TEMPLATE_PROCESS_RC compareWithBuiltinTable(int idxFromBin,
   char *tableNameFromBin);
static _NV_TEMPLATE_PROCESS_RC compare2Tables(int idxFromBin, int idxBuiltin);
static _NV_TEMPLATE_PROCESS_RC compare2FieldsAndCopyFromBin(
   _NV_TEMPLATE_TABLE *pTableBuiltin, _NV_TEMPLATE_TABLE *pTableFromBin,
   int idxBuiltin, int idxFromBin);
static _NV_TEMPLATE_PROCESS_RC compare2FieldIDType(
   _NV_TEMPLATE_TABLE *pTableBuiltIn, _NV_TEMPLATE_TABLE *pTableFromBin,
   int idxBuiltin, int idxFromBin);
static _NV_TEMPLATE_PROCESS_RC compare2FieldStorageTypeAndSizes(
   _NV_TEMPLATE_TABLE *pTableBuiltIn, _NV_TEMPLATE_TABLE *pTableFromBin,
   int idxBuiltin, int idxFromBin);
static _NV_TEMPLATE_PROCESS_RC compare2StorageSize(int idxBuiltIn,
   int idxFromBin, int sizeBuiltIn, int sizeFromBin,
   tANI_U8 sizeBuiltInLowByte, tANI_U8 sizeFromBinLowByte);
static _NV_TEMPLATE_PROCESS_RC processNvTemplateEnum(_NV_STREAM_BUF *pStream,
   int len);
static void compareEnumWithBuiltin(char *enumStr, int enumIdxFromBin);
static _NV_TEMPLATE_PROCESS_RC compare2EnumEntriesAndCopy(int idxFromBin,
   int idxBuiltin);
static int constructATemplateEnum(_NV_STREAM_BUF *pStream, int len,
   char *enumStr);
static void processNvData(_NV_STREAM_BUF *pStream, int len);
static int numElemSingular(_NV_TEMPLATE_TABLE *pTableEntry,int);
static int numElemArray1(_NV_TEMPLATE_TABLE *pTableEntry,int);
static int numElemArray2(_NV_TEMPLATE_TABLE *pTableEntry,int);
static int numElemArray3(_NV_TEMPLATE_TABLE *pTableEntry,int);
static void parseSubDataTable4Size(int tableIdx, int numElem);
static void getBasicDataSize(_NV_TEMPLATE_TABLE *pTableEntry);
static int getNumElemOutOfStorageSize(int fieldStorageSize,
   tANI_U8 fieldStorageSizeLowByte,int);

static int getFieldCount(int tableIdx, int fieldIdi, int numElem,
   int nvBin);
static void parseDataTable_new(_NV_STREAM_BUF *pStream, int* pos,
   int tableIdx, int addOffset, int tableBaseOffset);
static int getBuiltInFieldCount (int tblIdBin, char *tableNameFromBin,
   int *tblIdBuiltIn, int *fieldIdBuitIn, int *numElem);
static void parseSubDataTableAndCopy(int tableIdx, int numElem, int numElem2,
   int numElem3, int fieldId, _NV_STREAM_BUF *pStream, int* pos,
   int addOffset, int tableBaseOffset, int localAddOffset);

/*
 * typedef's
 */

typedef int (*pF_NumElemBasedOnStorageType)(_NV_TEMPLATE_TABLE *pTableEntry,
    int nvBin);

#endif


