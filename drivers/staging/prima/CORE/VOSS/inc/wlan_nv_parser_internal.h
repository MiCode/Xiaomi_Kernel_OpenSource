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


