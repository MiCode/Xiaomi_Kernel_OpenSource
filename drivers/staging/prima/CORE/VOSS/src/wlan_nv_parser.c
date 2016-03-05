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
/*===========================================================================
                       EDIT HISTORY FOR FILE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header:$ $DateTime: $ $Author: $

  when        who        what, where, why
  --------    ---        -----------------------------------------------------
  04/10/13    kumarpra   nv parser creation
===========================================================================*/

#include <linux/string.h>
#include "wlan_nv.h"

/*
 * NV stream layer service
 */
#include "wlan_nv_stream.h"
#include "wlan_nv_template_internal.h"
#include "wlan_nv_parser_internal.h"
#include "wlan_nv_template_api.h"
#include "wlan_nv_template_builtin.h"

#define _RECURSIVE_DATA_TABLE_PARSING
// Recursive/iterative switch !! Default iterative
#define _RECURSIVE_VERSION

/*
 * Build process should have created the built-in templates in
 *     "wlan_nv_templateBuiltIn.c"
 *     Include its auto-generated companion header file
 *     "wlan_nv_templateBuiltIn.h"
 *
 * The main definitions are
 *     _NV_TEMPLATE_TABLE NvTablesBuiltIn[];
 *
 */

/*
 * Parsing control bitmap
 */
tANI_U32 gNVParsingControlLo;
static int subTableSize;
static int fieldSize;
static sHalNv *gpnvData_t;
/* store enum comparison results*/
static _ENUM_META_DATA enumMetaDataFromBin[INDEX_ENUM_MAX];


/*
 * This data copy logic ignores the enum or int data types,
 * but simply copy the whole chunk to the NV data structure
 */
typedef struct {
   int idxSubFromBin;
   int idxSubBuiltin;
} _SUBTABLES_QUEUE;

pF_NumElemBasedOnStorageType numElemBasedOnStorageType[] = {
   numElemSingular,     // SINGULAR=0
   numElemArray1,       // ARRAY_1=1
   numElemArray2,       // ARRAY_2=2
   numElemArray3,       // ARRAY_3=3
};

static int sizeOneElemBasedOnFieldIdBasicDataType[] = {
   1,  // _FIELD_ID_DATA_TYPE_U8 =0
   4,  // _FIELD_ID_DATA_TYPE_U32
   1,  // _FIELD_ID_DATA_TYPE_S8
   4,  // _FIELD_ID_DATA_TYPE_S32
   2,  // _FIELD_ID_DATA_TYPE_U16
   2,  // _FIELD_ID_DATA_TYPE_S16
};

static _NV_STREAM_BUF nvStream[_NV_STREAM_LEN_MAX];
static int subTableRd, subTableWr;

#if !defined(_RECURSIVE_VERSION)
#define _SUBTABLES_MAX 32
static _SUBTABLES_QUEUE subTablesQueue[_SUBTABLES_MAX];
#endif

/*==============================================================================
*
* Storage for NvTablesFromBin
*
*===============================================================================
*/

/*
 * Init NvTablesFromBin
 * All entries are initialized to 0, pointers to NULL
*/

_NV_TEMPLATE_TABLE NvTablesFromBin[TABLES_MAX][TABLE_ENTRIES_MAX] = {
   { /* TABLE_LAST*/
       {{nul}, 0, 0, 0, 0, 0, 0, {nul}},
   },
};

static void initNvTablesFromBin(void)
{
   int i, j;

   for (i = 0; i < TABLES_MAX; i++) {
      for (j = 0; j < TABLE_ENTRIES_MAX; j++) {
         NvTablesFromBin[i][j].fieldName[0] = nul;
         NvTablesFromBin[i][j].fieldName[1] = nul;
         NvTablesFromBin[i][j].fieldName[2] = nul;
         NvTablesFromBin[i][j].fieldId = 0;
         NvTablesFromBin[i][j].fieldStorageType = 0;
         NvTablesFromBin[i][j].fieldStorageSize1 = 0;
         NvTablesFromBin[i][j].fieldStorageSize2 = 0;
         NvTablesFromBin[i][j].fieldStorageSize3 = 0;
         NvTablesFromBin[i][j].offset = 0;
         NvTablesFromBin[i][j].fieldFullName[0] = nul;
         NvTablesFromBin[i][j].fieldFullName[1] = nul;
         NvTablesFromBin[i][j].fieldFullName[2] = nul;
      }
   }

   return;
}

/*==============================================================================
*
* Storage for NvEnumsFromBin
*
* ==============================================================================
*/

/*
 * Prepare the NV enum templates storage parsed from nv.bin
 * They are used later for parsing the nv.bin data
 * All entries are initialized to 0, pointers to NULL
 */

_NV_TEMPLATE_ENUM NvEnumsFromBin[INDEX_ENUM_MAX][ENUM_ENTRIES_MAX] = {
   { /* INDEX_ENUM_LAST */
      {{nul}, 0, 0, {nul}},
   },
};

static void initNvEnumsFromBin(void)
{
   int i, j;

   for(i = 0; i < INDEX_ENUM_MAX; i++) {
      for(j = 0; j < ENUM_ENTRIES_MAX; j++) {
         NvEnumsFromBin[i][j].enumName[0] = nul;
         NvEnumsFromBin[i][j].enumName[1] = nul;
         NvEnumsFromBin[i][j].enumName[2] = nul;
         NvEnumsFromBin[i][j].enumValue = 0;
         NvEnumsFromBin[i][j].enumValuePeer = 0xFF;
         NvEnumsFromBin[i][j].enumFullName[0] = nul;
      }
   }
   return;
}


// =============================================================================
//
// Parse template streams
//
// =============================================================================

/*
 * Read nv.bin to extract the template info
 *     _NV_TEMPLATE_TABLE NvTablesFromBin[];
 */

/*
 * Parse nv.bin data and extract to the build-in data storage
 *
 * There are two outcomes from earlier templates comparison operation.
 *     different or identical
 * If identical, this operation will most likely take place.
 * If different,
 *     One is to simply indicate to the user and abort reading the nv.bin data
 *     The other is to continue this operation, and extract the matching entries
 *      in nv.bin
 */

/*
 * The template based NV logic:
 *    - the s/w module has the built-in templates
 *    - nv.bin is read one stream at a time, sequentially from beginning to end
 *    - if the stream is an enum stream,
 *    -     add to nv.bin template data structure
 *    -     compare with the built in template, by the string ID
 *    -     if two match, move on
 *    -     if not match, indicate mismatch, act based on the global logic
 *    _     selection
 *    -         if abort, exit here
 *    -         if extract-matching-ones,
 *    -             copy the enum from the built-in template over to a separate
 *    _             column
 *    -             when the enum comparison is done, all correlated enums have
 *    -             a built-in enum value
 *    -                 all mismtached ones have 0xff
 *    - else if the stream is a table
 *    -     add to nv.bin template data structure
 *    -     compare with the built-in template, by the field string ID
 *    -     if two tables match, move on
 *    -     if not match, indicate mismatch and proceed based on the global
 *    -     logic selection
 *    -         if abort, exit here
 *    -         if extract-matching-ones,
 *    -             copy the built-in template offset to a separate column
 *    - eles if the stream is a data stream
 *    -     parse the data stream based on the accumulated NV templates so far,
 *    -     note at this point,
 *    -         1. the accumulated templates may be incomplete, to make up the
 *    -            whole NV structure
 *    -         2. some will be "overwritten" by later templates definitions
 *    -            that change the earlier templates)
 *    -     how to parse?
 *    -         based on the nv.bin accumulated templates so far,
 *    -         select the table definition from the data stream,
 *    -         find the corresponding table template,
 *    -         start parsing data based on the template's field string IDs,
 *    -         field by field sequentially.
 *    -         if the field is a nested table,
 *    -             go inside to the next level
 *    -         if the field is a basic type,
 *    -             copy data of the given size to the offset which is the
 *    -             offset in the built-in nv data storage
 *    - end of the logic
 */

/*----------------------------------------------------------------------------
  \brief nvParser() - parse nv data provided in input buffer and store
  \ output in sHalNv
  \param inputEncodedbuffer, length, sHalNv - ptr to input stream,
  \param length, sHalNv
  \return success when successfully decode and copy to sHalNv structure
  \sa
-----------------------------------------------------------------------------*/

VOS_STATUS nvParser(tANI_U8 *pnvEncodedBuf, tANI_U32 nvReadBufSize,
   sHalNv *hal_nv)
{
   _STREAM_RC streamRc;
   _NV_STREAM_BUF *pStream = &nvStream[0];
   tANI_U32 len;
   _ErrorCode errCode = _OK;
   VOS_STATUS ret = VOS_STATUS_SUCCESS;
   gpnvData_t = hal_nv;

    // prepare storages for parsing nv.bin
   initNvTablesFromBin();
   initNvEnumsFromBin();

    // init stream read pointer
   initReadStream(pnvEncodedBuf, nvReadBufSize);

    // get and process streams one by one
   while (RC_FAIL != (streamRc = NEXT_STREAM(&len, &nvStream[0])) ) {
        // need to copy, stream layer is freeing it
      if (len > _NV_STREAM_LEN_MAX) {
         errCode = _STREAM_NOT_FIT_BUF;
         goto _error;
      }
        // template or data
      if (IsStreamTemplate(pStream[_NV_BIN_STREAM_HEADER_BYTE])) {
          if (_MIS_MATCH == processNvTemplate(pStream, len)) {
              if (_FLAG_AND_ABORT(gNVParsingControlLo) ) {
                  errCode = _SW_BIN_MISMATCH;
                  break;
              }
          }
      }
      else {
          processNvData(pStream, len);
      }
   }

_error:
   if (_OK != errCode) {
      ret = VOS_STATUS_E_INVAL;
   }

    // all done
   return ret;
}

static _NV_TEMPLATE_PROCESS_RC processNvTemplate(_NV_STREAM_BUF *pStream,
   int len)
{
    // Table or enum
    if (IsTemplateStreamTable(pStream[_NV_BIN_STREAM_HEADER_BYTE])) {
        return processNvTemplateTable(pStream, len);
    }
    else {
        return processNvTemplateEnum(pStream, len);
    }
}

/* -----------------------------------------------------------------------------
 *
 * Parse one table template stream in nv.bin
 * The length of table templates varies, based on the field ID class,
 * field size type
 */

static _NV_TEMPLATE_PROCESS_RC processNvTemplateTable(_NV_STREAM_BUF *pStream,
   int len)
{
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;
   char tableNameFromBin[_TABLE_NAME_LEN +1];
   int tableIdxFromBin;

    // construct the template table in the NvTablesFromBin
   memset((void*)tableNameFromBin, '\0', (size_t) (_TABLE_NAME_LEN +1));
   tableIdxFromBin = constructATemplateTable(pStream, len, tableNameFromBin);

    // fetch the table name from the first entry, the Table of all tables
    // search for the corresponding table in NvDataBuiltIn
   if (tableIdxFromBin) {
       rc = compareWithBuiltinTable(tableIdxFromBin, tableNameFromBin);
   }
    // done
   return rc;
}

static int getOffsetFromBuiltIn(char *tableNameFromBin)
{
   int offset = _OFFSET_NOT_SET;
   int i;

   _NV_TEMPLATE_TABLE (*pTableBuiltin)[TABLE_ENTRIES_MAX] = NvTablesBuiltIn;
    // search NvTablesBuiltIn for the same string named table, and its idx
   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
       if (nul == pTableBuiltin[0][i].fieldName[0]) {
          break;
       }
       if (!strcmp(tableNameFromBin, pTableBuiltin[0][i].fieldName)) {
          offset = pTableBuiltin[0][i].offset;
          break;
       }
   }
   return offset;
}

/*
 * Construct a table template in the NvTablesFromBin
 * it returns the newly constructed table, for comparison with NvTablesBuiltIn
 */
static int constructATemplateTable(_NV_STREAM_BUF *pStream, int len,
   char *tableStrName)
{
   int pos = 0;
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX] = NvTablesFromBin;
   int tableIdx, entryIdx;
   int i;
   _ErrorCode errCode = _OK;

   tableIdx = (pStream[_NV_BIN_STREAM_TABLE_ID_BYTE] &
                        FIELD_ID_TABLE_OR_ENUM_IDX_MASK);

   if (IsIdxTableOfAllTables(tableIdx)) {
   }
   else {
        // find the string name of the table
      for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
         if (nul == pTable[0][i].fieldName[0]) {
            break;
         }
         if ((pTable[0][i].fieldId & FIELD_ID_TABLE_OR_ENUM_IDX_MASK) ==
           tableIdx) {
            strlcpy(tableStrName, pTable[0][i].fieldName, (_TABLE_NAME_LEN +1));
            break;
         }
     }
     if (TABLE_ENTRIES_MAX == i) {
            // if string name not found, don't know what to do
        errCode = _TABLE_NON_EXIST_IN_TABLE_OF_ALL_TABLES;
        goto _error;
     }
   }

    // check if the table is already populated
   if (nul != pTable[tableIdx][0].fieldName[0]) {  // there is data in that enty
       // tbd: decision logic based on Parsing Control (bitmap)
   }

    // overwrite table entry, tableIdx
   pos = _TABLE_FIELDS_POS;
   entryIdx = 0;
   while (pos < len) {
      if (!(pos <= (len - _TABLE_FIELD_MIN_LEN))) {
           // error condition
         errCode = _INSUFFICIENT_FOR_FIELD_PARSER_ERROR;
         break;
      }

        //  populate the entry
      memset(pTable[tableIdx][entryIdx].fieldName, '\0',
         (size_t) (_TABLE_NAME_LEN + 1));
      memset(pTable[tableIdx][entryIdx].fieldFullName, '\0',
        (size_t) (_TABLE_FIELD_FULL_NAME_LEN + 1));
      pTable[tableIdx][entryIdx].fieldName[0] = pStream[pos++];
      pTable[tableIdx][entryIdx].fieldName[1] = pStream[pos++];
      pTable[tableIdx][entryIdx].fieldId = pStream[pos++];
      pTable[tableIdx][entryIdx].fieldStorageType = pStream[pos++];
      pTable[tableIdx][entryIdx].fieldStorageSize1 = 0;
      pTable[tableIdx][entryIdx].fieldStorageSize2 = 0;
      pTable[tableIdx][entryIdx].fieldStorageSize3 = 0;
      pTable[tableIdx][entryIdx].offset =
         getOffsetFromBuiltIn(pTable[tableIdx][entryIdx].fieldName);

      if (SINGULAR ==
         _STORAGE_TYPE(pTable[tableIdx][entryIdx].fieldStorageType)) {
      }
      else if (ARRAY_1 ==
         _STORAGE_TYPE(pTable[tableIdx][entryIdx].fieldStorageType)) {
         pTable[tableIdx][entryIdx].fieldStorageSize1 = pStream[pos++];
      }
      else if (ARRAY_2 ==
         _STORAGE_TYPE(pTable[tableIdx][entryIdx].fieldStorageType)) {
         pTable[tableIdx][entryIdx].fieldStorageSize1 = pStream[pos++];
         pTable[tableIdx][entryIdx].fieldStorageSize2 = pStream[pos++];
      }
      else if (ARRAY_3 ==
         _STORAGE_TYPE(pTable[tableIdx][entryIdx].fieldStorageType)) {
         pTable[tableIdx][entryIdx].fieldStorageSize1 = pStream[pos++];
         pTable[tableIdx][entryIdx].fieldStorageSize2 = pStream[pos++];
         pTable[tableIdx][entryIdx].fieldStorageSize3 = pStream[pos++];
      }
        //
      entryIdx++;
   }

_error:
   if (_OK != errCode) {
   }

    // all done
   return tableIdx;
}

/* -----------------------------------------------------------------------------
 *
 * Table Compare logic:
 *
 * 1. the fields need to be in the same order. Looping through fields doesn't
 *    guarantee order.
 * 2. whenever mismatch occurs in this "same-order" comparison, flag.
 * 3. If extract matching entries' option is selected, proceed to nv.bin table
 *    and extract data.
 *
 * Note
 *   "compareWithBuiltinTable" is the initiating point.
 *   "compare2Tables" is the top level compare logic.
 *       it is naturally implemented as a recursive call, but out of stack
 *       overflow concern,
 *       it is also implemented as an iterative loop.
 *
 */

static _NV_TEMPLATE_PROCESS_RC compareWithBuiltinTable(int idxFromBin,
   char *tableNameFromBin)
{
   int i;
   _NV_TEMPLATE_TABLE (*pTableBuiltin)[TABLE_ENTRIES_MAX] = NvTablesBuiltIn;
   int tableIdxBuiltin = 0;
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;
   _ErrorCode errCode = _OK;

    // search NvTablesBuiltIn for the same string named table, and its idx
   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
      if (nul == pTableBuiltin[0][i].fieldName[0]) {
         break;
      }
      if (!strcmp(tableNameFromBin, pTableBuiltin[0][i].fieldName)) {
          tableIdxBuiltin = (pTableBuiltin[0][i].fieldId &
             FIELD_ID_TABLE_OR_ENUM_IDX_MASK);
          break;
      }
   }

   // compare and copy values
   if (!tableIdxBuiltin) {
      errCode = _TABLE_NON_EXIST_IN_TABLE_OF_ALL_TABLES;
      rc = _MIS_MATCH;
   }
   else {
      subTableRd = 0;
      subTableWr = 0;

      // fire the comparison logic
      if (_MIS_MATCH == compare2Tables(idxFromBin, tableIdxBuiltin)) {
          rc = _MIS_MATCH;
      }

      // for iterative version
      // return code (rc) should only be set to _MIS_MATCH when it happens at
      // least once
#if !defined(_RECURSIVE_VERSION)
      {
      int idxSubFromBin, idxSubBuiltin;
      while (subTableRd != subTableWr) {
          idxSubFromBin = subTablesQueue[subTableRd].idxSubFromBin;
          idxSubBuiltin = subTablesQueue[subTableRd].idxSubBuiltin;
          if (_MIS_MATCH == compare2Tables(idxSubFromBin, idxSubBuiltin)) {
              rc = _MIS_MATCH;
          }
          // increment read pointer
          subTableRd = (subTableRd+1) % _SUBTABLES_MAX;
     }
     }
#endif //#if !defined(_RECURSIVE_VERSION)
   }

//_error:
   if (_OK != errCode) {
        //printf("Error %d \n", errCode);
   }

   //
   return rc;
}

static _NV_TEMPLATE_PROCESS_RC compare2Tables(int idxFromBin, int idxBuiltin)
{
   int i, j;
   _NV_TEMPLATE_TABLE (*pTableBuiltIn)[TABLE_ENTRIES_MAX];
   _NV_TEMPLATE_TABLE (*pTableFromBin)[TABLE_ENTRIES_MAX];
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;

   pTableBuiltIn = NvTablesBuiltIn;
   pTableFromBin = NvTablesFromBin;

   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
      if ((nul == pTableBuiltIn[idxBuiltin][i].fieldName[0]) ||
      // end of table occurs in either table
          (nul == pTableFromBin[idxFromBin][i].fieldName[0])) {
          // end of table occurs in either table
         if ((nul == pTableBuiltIn[idxBuiltin][i].fieldName[0]) &&
             (nul == pTableFromBin[idxFromBin][i].fieldName[0])) {
            rc = _MATCH;
         }
         else {
            rc = _MIS_MATCH;

            for (j=0; j<TABLE_ENTRIES_MAX; j++) {
               if (nul == pTableBuiltIn[idxBuiltin][j].fieldName[0]) {
                       // end of bin table
                  break;
               }
               if (!strcmp(
                  (const char*) pTableBuiltIn[idxBuiltin][j].fieldName,
                  (const char*) pTableFromBin[idxFromBin][i].fieldName)) {
                        // found matching field in bin table
                        // DO NOT check return code, it's already a mismatch
                  compare2FieldsAndCopyFromBin(
                       &(pTableBuiltIn[idxBuiltin][j]),
                       &(pTableFromBin[idxFromBin][i]), idxBuiltin,
                       idxFromBin);
                  break;
               }
            }
         }
         break; // end of table, either table, condition
      }
      else {
         if (!strcmp(pTableBuiltIn[idxBuiltin][i].fieldName,
                 pTableFromBin[idxFromBin][i].fieldName)) {
         // two field names match
            if (_MATCH == compare2FieldsAndCopyFromBin(
                             &(pTableBuiltIn[idxBuiltin][i]),
                             &(pTableFromBin[idxFromBin][i]),
                             idxBuiltin, idxFromBin)) {
               rc = _MATCH;
            }
            else {
               rc = _MIS_MATCH;
               if ( _FLAG_AND_ABORT(gNVParsingControlLo) ) {
                  break;
               }
            }
         }
         else {
            rc = _MIS_MATCH;
            if ( _FLAG_AND_ABORT(gNVParsingControlLo) ) {
               break;
            }
            // else
            // loop through the WHOLE bin tables, looking for a matching field.
            // this would take care of both cases where bin table field is
            // either "ahead" or "behind"
            for (j = 0; j < TABLE_ENTRIES_MAX; j++) {
            // end of bin table
               if (nul == pTableFromBin[idxBuiltin][j].fieldName[0]) {
                   break;
               }
               if (!strcmp(pTableBuiltIn[idxBuiltin][j].fieldName,
                     pTableFromBin[idxFromBin][i].fieldName)) {
                // found matching field in bin table
                // DO NOT check return code, it's already a mismatch
                  compare2FieldsAndCopyFromBin(&(pTableBuiltIn[idxBuiltin][j]),
                      &(pTableFromBin[idxFromBin][i]), idxBuiltin, idxFromBin);
                  break;
               }
            }
         }
      }
   }

   return rc;
}

static _NV_TEMPLATE_PROCESS_RC compare2FieldsAndCopyFromBin(
   _NV_TEMPLATE_TABLE*pTableBuiltIn,
   _NV_TEMPLATE_TABLE *pTableFromBin, int idxBuiltin, int idxFromBin)
{
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;

   // Verified already the fieldNames match
   // fieldId type matching?
   /*
    * If it's a simple type, simple comparison, ==,after mask out the data type
    * else if it's a table, need to compare 2 tables, don't call recursively
    */
   if (_MIS_MATCH == compare2FieldIDType(pTableBuiltIn, pTableFromBin,
         idxBuiltin, idxFromBin)) {
      rc = _MIS_MATCH;
      goto _end;
   }
    // storage type matching?
    // If it's SINGULAR, simple == comparison
    // else if it's ARRAY_n, besides check ARRAY_n equal, check sizes
    //     if sizes are Index_int, simple comparison
    //     else if sizes are INDEX_ENUM, need to loop through enums
    //
   if (_MIS_MATCH == compare2FieldStorageTypeAndSizes(pTableBuiltIn,
            pTableFromBin, idxBuiltin, idxFromBin)) {
      rc = _MIS_MATCH;
      goto _end;
   }

   // all matched, copy offset over
   rc = _MATCH;
   pTableFromBin->offset = pTableBuiltIn->offset;

_end:
   return rc;
}

static _NV_TEMPLATE_PROCESS_RC compare2FieldIDType(
   _NV_TEMPLATE_TABLE *pTableBuiltIn,
   _NV_TEMPLATE_TABLE *pTableFromBin, int idxBuiltin, int idxFromBin)
{
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;

   if (IsFieldTypeBasicData(pTableBuiltIn->fieldId)) {
      if (pTableBuiltIn->fieldId == pTableFromBin->fieldId) {
         rc = _MATCH;
      }
      else {
         rc = _MIS_MATCH;
      }
   }
   else { // field is a table
      int idxSubBuiltin = pTableBuiltIn->fieldId &
        FIELD_ID_TABLE_OR_ENUM_IDX_MASK;
      int idxSubFromBin = pTableFromBin->fieldId &
        FIELD_ID_TABLE_OR_ENUM_IDX_MASK;
#if defined(_RECURSIVE_VERSION)
      rc = compare2Tables(idxSubFromBin, idxSubBuiltin);
#else
      {
      subTablesQueue[subTableWr].idxSubFromBin = idxSubFromBin;
      subTablesQueue[subTableWr].idxSubBuiltin = idxSubBuiltin;
      subTableWr = (subTableWr +1) % _SUBTABLES_MAX;
      }
#endif //#if defined(_RECURSIVE_VERSION)
   }

   return rc;
}


static _NV_TEMPLATE_PROCESS_RC compare2FieldStorageTypeAndSizes(
   _NV_TEMPLATE_TABLE *pTableBuiltIn,
   _NV_TEMPLATE_TABLE *pTableFromBin, int idxBuiltIn, int idxFromBin)
{
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;

   if (_STORAGE_TYPE(pTableBuiltIn->fieldStorageType) ==
      _STORAGE_TYPE(pTableFromBin->fieldStorageType)) {
      if (SINGULAR == _STORAGE_TYPE(pTableBuiltIn->fieldStorageType)) {
         rc = _MATCH;
      }
      else if (ARRAY_1 == _STORAGE_TYPE(pTableBuiltIn->fieldStorageType)) {
         if ((_MATCH == compare2StorageSize(idxBuiltIn, idxFromBin,
            _STORAGE_SIZE1(pTableBuiltIn->fieldStorageSize1,
               pTableBuiltIn->fieldStorageType),
            _STORAGE_SIZE1(pTableFromBin->fieldStorageSize1,
               pTableFromBin->fieldStorageType),
            pTableBuiltIn->fieldStorageSize1,
            pTableFromBin->fieldStorageSize1)) ) {

            rc = _MATCH;
         }
      }
      else if (ARRAY_2 == _STORAGE_TYPE(pTableBuiltIn->fieldStorageType)) {
         if ((_MATCH == compare2StorageSize(idxBuiltIn, idxFromBin,
             _STORAGE_SIZE1(pTableBuiltIn->fieldStorageSize1,
                pTableBuiltIn->fieldStorageType),
             _STORAGE_SIZE1(pTableFromBin->fieldStorageSize1,
                pTableFromBin->fieldStorageType),
             pTableBuiltIn->fieldStorageSize1,
             pTableFromBin->fieldStorageSize1)) &&
            (_MATCH == compare2StorageSize(idxBuiltIn, idxFromBin,
             _STORAGE_SIZE2(pTableBuiltIn->fieldStorageSize2,
                pTableBuiltIn->fieldStorageType),
                _STORAGE_SIZE2(pTableFromBin->fieldStorageSize2,
                pTableFromBin->fieldStorageType),
                pTableBuiltIn->fieldStorageSize2,
                pTableFromBin->fieldStorageSize2)) ) {
                rc = _MATCH;
         }
      }
      else if (ARRAY_3 == _STORAGE_TYPE(pTableBuiltIn->fieldStorageType)) {
         if ((_MATCH == compare2StorageSize(idxBuiltIn, idxFromBin,
             _STORAGE_SIZE1(pTableBuiltIn->fieldStorageSize1,
                pTableBuiltIn->fieldStorageType),
             _STORAGE_SIZE1(pTableFromBin->fieldStorageSize1,
                pTableFromBin->fieldStorageType),
             pTableBuiltIn->fieldStorageSize1,
             pTableFromBin->fieldStorageSize1)) &&
             (_MATCH == compare2StorageSize(idxBuiltIn, idxFromBin,
                _STORAGE_SIZE2(pTableBuiltIn->fieldStorageSize2,
                   pTableBuiltIn->fieldStorageType),
                _STORAGE_SIZE2(pTableFromBin->fieldStorageSize2,
                   pTableFromBin->fieldStorageType),
                pTableBuiltIn->fieldStorageSize2,
                pTableFromBin->fieldStorageSize2)) &&
             (_MATCH == compare2StorageSize(idxBuiltIn, idxFromBin,
                _STORAGE_SIZE3(pTableBuiltIn->fieldStorageSize3,
                   pTableBuiltIn->fieldStorageType),
                _STORAGE_SIZE3(pTableFromBin->fieldStorageSize3,
                   pTableFromBin->fieldStorageType),
                pTableBuiltIn->fieldStorageSize3,
                pTableFromBin->fieldStorageSize3)) ) {
                rc = _MATCH;
         }
      }
   }
   else {
      rc = _MIS_MATCH;
   }
   return rc;
}

static _NV_TEMPLATE_PROCESS_RC compare2StorageSize(int idxBuiltIn,
   int idxFromBin, int sizeBuiltIn,
   int sizeFromBin, tANI_U8 sizeBuiltInLowByte, tANI_U8 sizeFromBinLowByte)
{
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;

   if (IsFieldSizeInt(sizeBuiltInLowByte) &&
         IsFieldSizeInt(sizeFromBinLowByte)) {
      if (sizeBuiltIn == sizeFromBin) {
         rc = _MATCH;
      }
      else {
         rc = _MIS_MATCH;
      }
   }
   else if (!IsFieldSizeInt(sizeBuiltInLowByte) &&
              !IsFieldSizeInt(sizeFromBinLowByte)) {
        // enums should have been compared when enum streams are parsed
        // The implication is that the enum streams should go before tables'
      rc = enumMetaDataFromBin[idxFromBin].match;
   }
   else {
      rc = _MIS_MATCH;
   }

   return rc;
}

/*
 * ----------------------------------------------------------------------------
 *
 * Parse one enum template stream in nv.bin
 */
static _NV_TEMPLATE_PROCESS_RC processNvTemplateEnum(_NV_STREAM_BUF *pStream,
   int len)
{
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;
   char enumStr[_ENUM_NAME_LEN + 1];
    //_NV_TEMPLATE_ENUM *pEnum;
   int enumIdx;

    // construct the enum template in the NvEnumsFromBin
   memset((void*)enumStr, '\0', (size_t) (_ENUM_NAME_LEN + 1));
   enumIdx = constructATemplateEnum(pStream, len, enumStr);

    // Compare the enum template
    // also record compare results for later use in the table
    // templates parsing where
    // the fields may be indexed by enums
    // if enumIdx ==0, didn't construct any entry,
    // (or only the first entry)
   if (enumIdx) {
      compareEnumWithBuiltin(enumStr, enumIdx);
   }

   return rc;
}

static void compareEnumWithBuiltin(char *enumStr, int enumIdxFromBin)
{
   int i;
   int enumIdxBuiltin = 0;
   _NV_TEMPLATE_ENUM (*pEnumBuiltin)[ENUM_ENTRIES_MAX] = NvEnumsBuiltIn;
   _ErrorCode errCode = _OK;

   for (i = 0; i < ENUM_ENTRIES_MAX; i++) {
      if (nul == pEnumBuiltin[0][i].enumName[0]) {
          break;
      }
      if (!strcmp(enumStr, pEnumBuiltin[0][i].enumName)) {
          enumIdxBuiltin = pEnumBuiltin[0][i].enumValue;
          break;
      }
   }
   if (!enumIdxBuiltin) {
      errCode = _ENUM_NOT_FOUND_IN_BUILT_IN;
      return;
   }
   else {
      compare2EnumEntriesAndCopy(enumIdxFromBin, enumIdxBuiltin);
   }

//_error:
   if (_OK != errCode) {
      //printf("Error %d\n", errCode);
   }

   return;
}

static _NV_TEMPLATE_PROCESS_RC compare2EnumEntriesAndCopy(int idxFromBin,
   int idxBuiltin)
{
   int i,j;
   _NV_TEMPLATE_PROCESS_RC rc = _MATCH;
   _NV_TEMPLATE_ENUM (*enumsFromBin)[ENUM_ENTRIES_MAX] = NvEnumsFromBin;
   _NV_TEMPLATE_ENUM (*enumsBuiltin)[ENUM_ENTRIES_MAX] = NvEnumsBuiltIn;

    // need to go through all enums
   for (i = 0; i < ENUM_ENTRIES_MAX; i++) {
       // end conditions: either both reach the end (match),
       // or one of them reaching the end (mismatch)
      if ((nul == enumsBuiltin[idxBuiltin][i].enumName[0]) ||
          (nul == enumsFromBin[idxFromBin][i].enumName[0])) {
          if ((nul == enumsBuiltin[idxBuiltin][i].enumName[0]) &&
              (nul == enumsFromBin[idxFromBin][i].enumName[0])) {
                // fully matched
              rc = _MATCH;
              break;
          }
          else {
              rc = _MIS_MATCH;
              for (j = 0; j < ENUM_ENTRIES_MAX; j++) {
                  if (nul == enumsBuiltin[idxBuiltin][j].enumName[0]) {
                      break;
                  }
                  if (!strcmp((const char*)enumsFromBin[idxFromBin][i].enumName,
                       (const char*)enumsBuiltin[idxBuiltin][j].enumName)) {
                      enumsFromBin[idxFromBin][i].enumValuePeer =
                             enumsBuiltin[idxBuiltin][j].enumValue;
                      break;
                  }
              }
              break;
          }
      }
      else {
          if (!strcmp(enumsBuiltin[idxBuiltin][i].enumName,
                enumsFromBin[idxFromBin][i].enumName)) {
                // copy builtIn enum value to fromBin
              enumsFromBin[idxFromBin][i].enumValuePeer =
                 enumsBuiltin[idxBuiltin][i].enumValue;
          }
          else {
              // mismatch, but still loop through the whole enum list
              // for the "ahead" and "behind" scenarios
              rc = _MIS_MATCH;
              for (j = 0; j < ENUM_ENTRIES_MAX; j++) {
                 if (nul == enumsBuiltin[idxBuiltin][j].enumName[0]) {
                     break;
                 }
                 if (!strcmp(enumsFromBin[idxFromBin][i].enumName,
                        enumsBuiltin[idxBuiltin][j].enumName)) {
                     enumsFromBin[idxFromBin][i].enumValuePeer =
                        enumsBuiltin[idxBuiltin][j].enumValue;
                     break;
                 }
              }
         }
      }
   }

   // record match or mismatch for later data parsing use
   enumMetaDataFromBin[idxFromBin].match = rc;

   // all done
   return rc;
}

static int constructATemplateEnum(_NV_STREAM_BUF *pStream, int len,
   char *enumStr)
{
    int pos = 0;
    _NV_TEMPLATE_ENUM (*pEnum)[ENUM_ENTRIES_MAX];
    int enumIdx = 0;
    int i;
    int entryIdx;
    _ErrorCode errCode = _OK;

    enumIdx = (pStream[_NV_BIN_STREAM_ENUM_ID_BYTE] &
       FIELD_ID_TABLE_OR_ENUM_IDX_MASK);
    pEnum = NvEnumsFromBin;

    // find its string name
    // the logic:
    // since the nv.bin doesn't encode the enum string name in the actual enum
    // stream, only the first "enum of all enums"
    //    has the names of all enums.
    if (IsIdxEnumOfAllEnums(enumIdx)) {
    }
    else {
        for (i = 0; i < ENUM_ENTRIES_MAX; i++) {
            if (nul == pEnum[0][i].enumName[0]) {
                break;
            }
            if (pEnum[0][i].enumValue == enumIdx) {
                strlcpy(enumStr, pEnum[0][i].enumName,(_ENUM_NAME_LEN + 1));
                break;
            }
        }
        if (ENUM_ENTRIES_MAX == i) {
            // without a string name, don't know what to do with the enum indexed
            errCode = _ENUM_NOT_FOUND_IN_BUILT_IN;
            goto _error;
        }
    }

    // Found the enum string name, now parsing decision time ...
    // Is the entry already populated?
    if (nul != pEnum[enumIdx][0].enumName[0]) {  // there is data in that entry
        // TBD:
        // the logic here depends on how we support "parsing data based on the
        // latest template".
        // one way is to overwrite the template, so the subsequent parsing will
        // be based on the "latest".
        // the second way is to "append" the template, and the parsing should
        // always be based on the "last" of the same name
        // for simplicity, support the first approach for now.
        //
        // the logic:
        // based on the parsing control (bitmap), we may proceed on overwriting
        // enums with blind faith that the writing logic is correct,
        // or ignore the appended.
        //
    }

    // overwrite entry, enumIdx
    pos = _ENUM_START_POS;
    entryIdx = 0;
    while (pos < len) {
        if (!(pos <= (len - _ENUM_MIN_LEN))) {
            // error condition
            errCode = _INSUFFICIENT_FOR_FIELD_PARSER_ERROR;
            break;
        }

        // populate the entry
        memset(pEnum[enumIdx][entryIdx].enumName, '\0',
           (size_t) (_ENUM_NAME_LEN +1));
        memset(pEnum[enumIdx][entryIdx].enumFullName, '\0',
           (size_t) (_ENUM_FULL_NAME_LEN +1));
        pEnum[enumIdx][entryIdx].enumName[0] = pStream[pos++];
        pEnum[enumIdx][entryIdx].enumName[1] = pStream[pos++];
        pEnum[enumIdx][entryIdx].enumValue   = pStream[pos++];
        entryIdx++;
    }

_error:
    if (_OK != errCode) {
        //printf("Error %d\n", errCode);
    }

    // all done
    return enumIdx;
}

/* -----------------------------------------------------------------------------
 *
 * Process data stream
 * The purpose is to copy nv.bin data into built in NV data structure.
 * This is the parser function.
 *
 * Next phase:
 * With NV data in the s/w module data structure, nv.bin conforming
 * to the new format can be generated. That is the nv.bin generation logic.)
 *
 * Data stream has the following format
 *
 * delimiter|streamHeader|tableID|data....|CRC|delimiter
 * Note
 *    1. delimiters are not present in the stream data, pStream.
 *    2. nested tables do NOT have table IDs with them.
 *
 */
// NV data, per built in templates
// Recursive table parsing, which is naturally depth-first
// If iterative, a bit hard to implement

static void parseSubDataTable4Size(int tableIdx, int numElem)
{
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX] = NvTablesFromBin;
   int idxSubTable;
   int i;
   int numSubElem = 0, idx=0;

   // "apply" template to data -- parsing the actual NV data,
   //     so far we have been parsing and building templates
   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
      if (nul == pTable[tableIdx][i].fieldName[0]) {
          // data parsing done for this stream
          break;
      }
      if (IsFieldTypeBasicData(pTable[tableIdx][i].fieldId)) {
          getBasicDataSize(&(pTable[tableIdx][i]));
      }
      else {
         // number of element
         idx =
            _STORAGE_TYPE(pTable[tableIdx][i].fieldStorageType);
         numSubElem = numElemBasedOnStorageType[idx](
                         &(pTable[tableIdx][i]), 1);
         // get size of the sub-table
         idxSubTable = (pTable[tableIdx][i].fieldId &
            FIELD_ID_TABLE_OR_ENUM_IDX_MASK);
         // recursive calls for the total size of the subtable
         parseSubDataTable4Size(idxSubTable, numSubElem);
      }
   }
   // update subTableSize for the number of elements
   subTableSize *= numElem;

   return;
}

static void copyDataToBuiltInFromBin(int tableIdx,int fieldId,
    _NV_STREAM_BUF *pStream, int *pos, int addOffset, int tableBaseOffset)
{
   int i,j,k,storageType;
   int idx=0, size1=0, size2=0, size3=0;
   int enumIdx1=0, enumIdx2=0, enumIdx3=0, sizeOneElem=0;
   int isFirstFieldEnum=0,isSecondFieldEnum=0,isThirdFieldEnum=0;
   int index,index1,index2;
   int dindex,dindex1,dindex2,totalSize;
   int offset=0,sizeBuiltIn,tableIdxBuiltIn,fieldIdBuiltIn;
   int idxBuiltIn=0, size1BuiltIn=0, size2BuiltIn=0, size3BuiltIn=0;
   int size1Bin=0, size2Bin=0, size3Bin=0, numElemBuiltIn;
   int sizeOneElemBuiltIn=0, field;
   unsigned char *ptr, *dptr;
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX] = NvTablesFromBin;
   _NV_TEMPLATE_TABLE (*pTableBuiltIn)[TABLE_ENTRIES_MAX] = NvTablesBuiltIn;

   storageType = _STORAGE_TYPE(pTable[tableIdx][fieldId].fieldStorageType);
   field = pTable[tableIdx][fieldId].fieldId;
   sizeOneElem = sizeOneElemBasedOnFieldIdBasicDataType[field &
                    FIELD_ID_TABLE_OR_ENUM_IDX_MASK];
   sizeBuiltIn = getBuiltInFieldCount(tableIdx,
                    pTable[tableIdx][fieldId].fieldName,
                    &tableIdxBuiltIn,&fieldIdBuiltIn,&numElemBuiltIn);

   field = pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldId;
   sizeOneElemBuiltIn = sizeOneElemBasedOnFieldIdBasicDataType[field &
                          FIELD_ID_TABLE_OR_ENUM_IDX_MASK];

   if (storageType == SINGULAR ) {
      ptr = (unsigned char*)((int)gpnvData_t + tableBaseOffset + addOffset);
      dptr = (unsigned char *)&pStream[*pos];

      if (IsFieldTypeBasicData(pTable[tableIdx][fieldId].fieldId)) {
         idx = _STORAGE_TYPE(pTable[tableIdx][fieldId].fieldStorageType);
         size1Bin = numElemBasedOnStorageType[idx](
                       &(pTable[tableIdx][fieldId]), 1);
         field = pTable[tableIdx][fieldId].fieldId;
         sizeOneElem = sizeOneElemBasedOnFieldIdBasicDataType[field &
                         FIELD_ID_TABLE_OR_ENUM_IDX_MASK];

         idxBuiltIn = _STORAGE_TYPE(
           pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);
         size1BuiltIn = numElemBasedOnStorageType[idx](
                      &(pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn]), 0);

         size1 = size1Bin;
         if (size1 > size1BuiltIn) {
            size1 = size1BuiltIn;
         }
      }
      totalSize = size1 * sizeOneElem;

      offset = 0;
      for (i = 0; i < size1; i++) {
         memcpy(&ptr[offset], &dptr[offset], sizeOneElem);
         offset = offset + sizeOneElem;
      }

      *pos = *pos + (size1Bin * sizeOneElem);
   }
   else {
      if (ARRAY_1 == storageType) {
         ptr = (unsigned char*)((int)gpnvData_t + tableBaseOffset + addOffset);
         dptr = (unsigned char *)&pStream[*pos];

         idx = _STORAGE_SIZE1(pTable[tableIdx][fieldId].fieldStorageSize1,
                 pTable[tableIdx][fieldId].fieldStorageType);
         size1Bin = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize1, 1);

         idx = _STORAGE_SIZE1(
               pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1,
               pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

         size1BuiltIn = getNumElemOutOfStorageSize(idx,
            pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1, 0);

         size1 = size1Bin;

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize1)) {
            enumIdx1 = ((pTable[tableIdx][fieldId].fieldStorageSize1 &
               FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
            isFirstFieldEnum = 1;
         }
         else {
            isFirstFieldEnum = 0;
            if (size1 > size1BuiltIn) {
               size1 = size1BuiltIn;
            }
         }

         offset = 0;
         for (i = 0; i < size1; i++) {
            if (isFirstFieldEnum) {
               if (NvEnumsFromBin[enumIdx1][i].enumValuePeer != 0xFF) {
                  index = NvEnumsFromBin[enumIdx1][i].enumValuePeer;
                  dindex = NvEnumsFromBin[enumIdx1][i].enumValue;

                  index = index * sizeOneElem;
                  dindex = dindex * sizeOneElem;

                  memcpy(&ptr[index], &dptr[dindex], sizeOneElem);
               }
            }
            else {
               memcpy(&ptr[offset], &dptr[offset], sizeOneElem);
               offset = offset + sizeOneElem;
            }
         }

         *pos = *pos + (size1Bin * sizeOneElem);
      }
      else if (ARRAY_2 == storageType) {
         ptr = (unsigned char*)((int)gpnvData_t + tableBaseOffset + addOffset);
         dptr = (unsigned char *)&pStream[*pos];

         idx = _STORAGE_SIZE1(pTable[tableIdx][fieldId].fieldStorageSize1,
                  pTable[tableIdx][fieldId].fieldStorageType);
         size1Bin = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize1, 1);

         idx = _STORAGE_SIZE2(pTable[tableIdx][fieldId].fieldStorageSize2,
                  pTable[tableIdx][fieldId].fieldStorageType);

         size2Bin = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize2, 1);

         idx = _STORAGE_SIZE1(
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1,
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

         size1BuiltIn = getNumElemOutOfStorageSize(idx,
            pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1, 0);

         idx = _STORAGE_SIZE2(
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize2,
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

         size2BuiltIn = getNumElemOutOfStorageSize(idx,
            pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize2, 0);

         size1 = size1Bin;
         size2 = size2Bin;

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize1)) {
            enumIdx1 = ((pTable[tableIdx][fieldId].fieldStorageSize1 &
               FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
            isFirstFieldEnum = 1;
         }
         else {
            isFirstFieldEnum = 0;
            if (size1 > size1BuiltIn) {
               size1 = size1BuiltIn;
            }
         }

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize2)) {
            enumIdx2 = ((pTable[tableIdx][fieldId].fieldStorageSize2 &
               FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
            isSecondFieldEnum = 1;
         }
         else {
            isSecondFieldEnum = 0;
            if (size2 > size2BuiltIn) {
               size2 = size2BuiltIn;
            }
         }

         offset = 0;

         for (i = 0; i < size1; i++) {
            if (isFirstFieldEnum) {
              if (NvEnumsFromBin[enumIdx1][i].enumValuePeer == 0xFF) {
                 continue;
              }

              index = NvEnumsFromBin[enumIdx1][i].enumValuePeer;
              dindex = NvEnumsFromBin[enumIdx1][i].enumValue;
            }
            else {
               index = dindex = i;
            }

            for (j = 0; j < size2; j++) {
              if (isSecondFieldEnum) {
                 if (NvEnumsFromBin[enumIdx2][j].enumValuePeer == 0xFF) {
                    continue;
                 }

                 index1 = NvEnumsFromBin[enumIdx2][j].enumValuePeer;
                 dindex1 = NvEnumsFromBin[enumIdx2][j].enumValue;
              }
              else {
                 index1 = dindex1 = j;
              }

              memcpy(&ptr[(index1 + index * size2BuiltIn)*sizeOneElem],
                   &dptr[(dindex1+dindex*size2Bin)*sizeOneElem], sizeOneElem);
              offset = offset + sizeOneElem;
            }
         }

         *pos = *pos + size2Bin * size1Bin * sizeOneElem;
      }
      else if (ARRAY_3 == storageType) {
         ptr = (unsigned char*)((int)gpnvData_t + tableBaseOffset + addOffset);
         dptr = (unsigned char *)&pStream[*pos];

         idx = _STORAGE_SIZE1(pTable[tableIdx][fieldId].fieldStorageSize1,
                  pTable[tableIdx][fieldId].fieldStorageType);
         size1Bin = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize1, 1);

         idx = _STORAGE_SIZE2(pTable[tableIdx][fieldId].fieldStorageSize2,
                  pTable[tableIdx][fieldId].fieldStorageType);
         size2Bin = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize2, 1);

         idx = _STORAGE_SIZE3(pTable[tableIdx][fieldId].fieldStorageSize3,
                  pTable[tableIdx][fieldId].fieldStorageType);
         size3Bin = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize3, 1);

         idx = _STORAGE_SIZE1(
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1,
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

         size1BuiltIn = getNumElemOutOfStorageSize(idx,
            pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1,
            0);

         idx = _STORAGE_SIZE2(
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize2,
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

         size2BuiltIn = getNumElemOutOfStorageSize(idx,
            pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize2,
            0);

         idx = _STORAGE_SIZE3(
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize3,
             pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

         size3BuiltIn = getNumElemOutOfStorageSize(idx,
            pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize3,
            0);

         size1 = size1Bin;
         size2 = size2Bin;
         size3 = size3Bin;

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize1)) {
            enumIdx1 = ((pTable[tableIdx][fieldId].fieldStorageSize1 &
               FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
            isFirstFieldEnum = 1;
         }
         else {
            isFirstFieldEnum = 0;
            if (size1 > size1BuiltIn) {
               size1 = size1BuiltIn;
            }
         }

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize2)) {
            enumIdx2 = ((pTable[tableIdx][fieldId].fieldStorageSize2 &
               FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
            isSecondFieldEnum = 1;
         }
         else {
            isSecondFieldEnum = 0;
            if (size2 > size2BuiltIn) {
               size2 = size2BuiltIn;
            }
         }

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize3)) {
            enumIdx3 = ((pTable[tableIdx][fieldId].fieldStorageSize3 &
               FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
            isThirdFieldEnum = 1;
         }
         else {
            isThirdFieldEnum = 0;
            if (size3 > size3BuiltIn) {
               size3 = size3BuiltIn;
            }
         }

         offset = 0;
         for (i = 0; i < size1; i++) {
            if (isFirstFieldEnum) {
              if (NvEnumsFromBin[enumIdx1][i].enumValuePeer == 0xFF) {
                 continue;
              }

              index = NvEnumsFromBin[enumIdx1][i].enumValuePeer;
              dindex = NvEnumsFromBin[enumIdx1][i].enumValue;
            }
            else {
              index = dindex = i;
            }

            for (j = 0; j < size2; j++) {
              if (isSecondFieldEnum) {
                 if (NvEnumsFromBin[enumIdx2][j].enumValuePeer == 0xFF) {
                    continue;
                 }

                 index1 = NvEnumsFromBin[enumIdx2][j].enumValuePeer;
                 dindex1 = NvEnumsFromBin[enumIdx2][j].enumValue;
              }
              else {
                 index1 = dindex1 = j;
              }

              for (k = 0; k < size3; k++) {
                 if (isThirdFieldEnum) {
                    if (NvEnumsFromBin[enumIdx2][j].enumValuePeer == 0xFF) {
                       continue;
                    }

                    index2 = NvEnumsFromBin[enumIdx3][k].enumValuePeer;
                    dindex2 = NvEnumsFromBin[enumIdx3][k].enumValue;
                 }
                 else {
                    index2 = dindex2 = k;
                 }

                 memcpy(&ptr[(index2 + (index1 * size2BuiltIn) +
                          (index * size3BuiltIn * size2BuiltIn)) * sizeOneElem],
                        &dptr[(dindex2 + (dindex1 * size2Bin) +
                          (dindex * size3Bin * size2Bin))*sizeOneElem],
                        sizeOneElem);

                 offset = offset + sizeOneElem;
              }
            }
         }

         *pos = *pos + size1Bin * size2Bin * size3Bin * sizeOneElem;
      }
      else {
      }
   }
}

// search NvTablesBuiltIn for the same string named table, and its idx
static int getBuiltInFieldCount (int tableIdxBin, char *tableNameFromBin,
   int *tblIdBuiltIn, int *fieldIdBuitIn, int *numElements)
{
   int i,idx,numElem,tableIdxBuiltin=0,fieldCnt;
   _NV_TEMPLATE_TABLE (*pTableBin)[TABLE_ENTRIES_MAX] = NvTablesFromBin;
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX] = NvTablesBuiltIn;
   int found=0, fieldIndex = 0;

   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
       if (nul == pTableBin[0][i].fieldName[0]) {
           break;
       }

       if ((pTableBin[0][i].fieldId &
            FIELD_ID_TABLE_OR_ENUM_IDX_MASK) == tableIdxBin) {
           found = 1;
           break;
       }
   }

   if (!found) {
      return -1;
   }

   //fieldName index got from tableId from Bin
   fieldIndex = i;
   found = 0;

   for (i=0;i<TABLE_ENTRIES_MAX;i++) {
      if (nul == pTable[0][i].fieldName[0]) {
           break;
       }

       if (!strcmp((const char*)pTableBin[0][fieldIndex].fieldName,
          (const char*)pTable[0][i].fieldName)) {
          found = 1;
          break;
       }
   }

   if (!found) {
      return -1;
   }

   //found tableId of builtIn
   tableIdxBuiltin = *tblIdBuiltIn =
                 (pTable[0][i].fieldId & FIELD_ID_TABLE_OR_ENUM_IDX_MASK);
   found = 0;

   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
      if (nul == pTable[tableIdxBuiltin][i].fieldName[0]) {
         break;
      }

      if (!strcmp((const char*)tableNameFromBin,
         (const char*)pTable[tableIdxBuiltin][i].fieldName)) {
         found = 1;
         break;
      }
   }

   if (!found) {
      return -1;
   }

   *fieldIdBuitIn = i;

   idx = _STORAGE_TYPE(pTable[tableIdxBuiltin][i].fieldStorageType);
   numElem = numElemBasedOnStorageType[idx](&(pTable[tableIdxBuiltin][i]), 0);

   fieldSize = 0;
   fieldCnt = getFieldCount (tableIdxBuiltin, i, numElem, 0);

   *numElements = numElem;

   return fieldCnt;
}

static int getFieldCount(int tableIdx, int fieldId, int numElem, int nvBin)
{
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX];
   int idxSubTable;
   int i, j, storageType, field;
   int numSubElem=0, idx =0, size1=0, size2=0, size3=0, sizeOneElem=0;
   int enumIdx1=0, enumIdx2=0, enumIdx3=0;

   if ( nvBin ) {
      pTable = NvTablesFromBin;
   }
   else {
      pTable = NvTablesBuiltIn;
   }

   storageType = _STORAGE_TYPE(pTable[tableIdx][fieldId].fieldStorageType);

   if (SINGULAR == storageType) {
      if (IsFieldTypeBasicData(pTable[tableIdx][fieldId].fieldId)) {
         idx = _STORAGE_TYPE(pTable[tableIdx][fieldId].fieldStorageType);
         size1 = numElemBasedOnStorageType[idx](&(pTable[tableIdx][fieldId]),
                   nvBin);
      }
      else {
      }
   }
   else {
      if (ARRAY_1 == storageType) {
         idx = _STORAGE_SIZE1(pTable[tableIdx][fieldId].fieldStorageSize1,
               pTable[tableIdx][fieldId].fieldStorageType);
         size1 = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize1,nvBin);

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize1)) {
            enumIdx1 = ((pTable[tableIdx][fieldId].fieldStorageSize1 &
                         FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
         }
      }
      else if (ARRAY_2 == storageType) {
         idx = _STORAGE_SIZE1(pTable[tableIdx][fieldId].fieldStorageSize1,
               pTable[tableIdx][fieldId].fieldStorageType);
         size1 = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize1,nvBin);

         idx = _STORAGE_SIZE2(pTable[tableIdx][fieldId].fieldStorageSize2,
               pTable[tableIdx][fieldId].fieldStorageType);

         size2 = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize2,nvBin);

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize1)) {
            enumIdx1 = ((pTable[tableIdx][fieldId].fieldStorageSize1 &
                         FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
         }

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize2)) {
            enumIdx2 = ((pTable[tableIdx][fieldId].fieldStorageSize2 &
                         FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
         }
      }
      else if (ARRAY_3 == storageType) {
         idx = _STORAGE_SIZE1(pTable[tableIdx][fieldId].fieldStorageSize1,
               pTable[tableIdx][fieldId].fieldStorageType);
         size1 = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize1,nvBin);

         idx = _STORAGE_SIZE2(pTable[tableIdx][fieldId].fieldStorageSize2,
               pTable[tableIdx][fieldId].fieldStorageType);
         size2 = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize2,nvBin);

         idx = _STORAGE_SIZE3(pTable[tableIdx][fieldId].fieldStorageSize3,
               pTable[tableIdx][fieldId].fieldStorageType);
         size3 = getNumElemOutOfStorageSize(idx,
                 pTable[tableIdx][fieldId].fieldStorageSize3,nvBin);

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize1)) {
            enumIdx1 = ((pTable[tableIdx][fieldId].fieldStorageSize1 &
                         FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
         }

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize2)) {
            enumIdx2 = ((pTable[tableIdx][fieldId].fieldStorageSize2 &
                         FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
         }

         if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize3)) {
            enumIdx3 = ((pTable[tableIdx][fieldId].fieldStorageSize3 &
                         FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
         }
      }
      else {
      }
   }

   if (IsFieldTypeBasicData(pTable[tableIdx][fieldId].fieldId)) {
       field = pTable[tableIdx][fieldId].fieldId;
       sizeOneElem = sizeOneElemBasedOnFieldIdBasicDataType[field &
                       FIELD_ID_TABLE_OR_ENUM_IDX_MASK];
       if ( size3 ) {
          fieldSize = fieldSize + size3  * size2 * size1 * sizeOneElem;
       }
       else if ( size2 ) {
          fieldSize = fieldSize + size2 * size1 * sizeOneElem;
       }
       else if ( size1 ) {
          fieldSize = fieldSize + size1 * sizeOneElem;
       }
       else {
       }
   }
   else {
      idxSubTable = (pTable[tableIdx][fieldId].fieldId &
            FIELD_ID_TABLE_OR_ENUM_IDX_MASK);

      for (j=0; j<numElem; j++) {
         for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
            if (nul == pTable[idxSubTable][i].fieldName[0]) {
             // data parsing done for this stream
             break;
            }

            idx = _STORAGE_TYPE(pTable[idxSubTable][i].fieldStorageType);
            numSubElem = numElemBasedOnStorageType[idx](
                            &(pTable[idxSubTable][i]),nvBin);

            getFieldCount(idxSubTable, i, numSubElem, nvBin);
         }
      }
   }

   return fieldSize;
}

static void parseSubDataTableAndCopy(int tableIdx, int numElem, int numElem2,
   int numElem3, int fieldId, _NV_STREAM_BUF *pStream, int *pos, int addOffset,
   int tableBaseOffset, int localAddOffset)
{
   int idxSubTable, i, j, l, m, idx1, storageType, fieldCount=0;
   int size1BuiltIn, size2BuiltIn, size3BuiltIn;
   int tableIdxBuiltIn, fieldIdBuiltIn, numElemBuiltIn, incAddOffset=0;
   int totalOffset=0, enumIdx, size1Bin, size2Bin, size3Bin;
   int numSubElem, numSubElem2, numSubElem3, sizeBuiltIn=0, idx=0;
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX] = NvTablesFromBin;
   _NV_TEMPLATE_TABLE (*pTableBuiltIn)[TABLE_ENTRIES_MAX] = NvTablesBuiltIn;

   // "apply" template to data -- parsing the actual NV data,
   //     so far we have been parsing and building templates
   if (IsFieldTypeBasicData(pTable[tableIdx][fieldId].fieldId)) {
      // First Entry, same as off addOffset just increment localOffset
      if (pTable[tableIdx][fieldId].offset == addOffset) {
         totalOffset = localAddOffset + pTable[tableIdx][fieldId].offset;
      }
      else {
         // Multiple  Entry next index array, addOffset and localOffset
         totalOffset = localAddOffset + pTable[tableIdx][fieldId].offset +
                       addOffset;
      }
      copyDataToBuiltInFromBin(tableIdx, fieldId, pStream, pos, totalOffset,
         tableBaseOffset);
   }
   else {
      // number of element
      // get size of the sub-table
      idxSubTable = (pTable[tableIdx][fieldId].fieldId &
                      FIELD_ID_TABLE_OR_ENUM_IDX_MASK);

      fieldSize = 0;
      sizeBuiltIn = getBuiltInFieldCount(tableIdx,
                      pTable[tableIdx][fieldId].fieldName,
                      &tableIdxBuiltIn, &fieldIdBuiltIn, &numElemBuiltIn);
      incAddOffset = 0;

      if (numElemBuiltIn) {
         incAddOffset = sizeBuiltIn/numElemBuiltIn;
      }

      storageType = _STORAGE_TYPE(pTable[tableIdx][fieldId].fieldStorageType);

      fieldSize = 0;
      fieldCount = getFieldCount(tableIdx, fieldId, numElem, 1);

      idx1 = _STORAGE_SIZE1(pTable[tableIdx][fieldId].fieldStorageSize1,
               pTable[tableIdx][fieldId].fieldStorageType);
      size1Bin = getNumElemOutOfStorageSize(idx1,
                 pTable[tableIdx][fieldId].fieldStorageSize1, 1);

      for (l=0; l < numElem3; l++) {
         if (storageType == ARRAY_3) {
            idx1 = _STORAGE_SIZE3(pTable[tableIdx][fieldId].fieldStorageSize3,
                      pTable[tableIdx][fieldId].fieldStorageType);

            size3Bin = getNumElemOutOfStorageSize(idx1,
                    pTable[tableIdx][fieldId].fieldStorageSize3, 1);

            fieldSize = 0;
            getBuiltInFieldCount(tableIdx,
              pTable[tableIdx][fieldId].fieldName, &tableIdxBuiltIn,
              &fieldIdBuiltIn, &numElemBuiltIn);

            idx1 = _STORAGE_SIZE3(
              pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize3,
              pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);
            size3BuiltIn = getNumElemOutOfStorageSize(idx1,
              pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize3,
              0);

            if (!IsFieldSizeInt(pTable[tableIdx][fieldId].fieldStorageSize3)) {
               enumIdx = ((pTable[tableIdx][fieldId].fieldStorageSize3 &
                  FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
               if (NvEnumsFromBin[enumIdx][l].enumValuePeer == 0xFF) {
                  *pos = *pos + (fieldCount/size1Bin) * numElem * numElem2;
                  continue;
               }
            }
            else {
               if ((l+1) > size3BuiltIn) {
                  *pos = *pos + (fieldCount/size1Bin) * numElem * numElem2;
                  continue;
               }
            }
         }
         for (m=0; m < numElem2; m++) {
            if (storageType == ARRAY_2) {
               idx1 = _STORAGE_SIZE2(
                         pTable[tableIdx][fieldId].fieldStorageSize2,
                         pTable[tableIdx][fieldId].fieldStorageType);
               size2Bin = getNumElemOutOfStorageSize(idx1,
                    pTable[tableIdx][fieldId].fieldStorageSize2, 1);

               fieldSize = 0;
               getBuiltInFieldCount(tableIdx,
                 pTable[tableIdx][fieldId].fieldName, &tableIdxBuiltIn,
                 &fieldIdBuiltIn, &numElemBuiltIn);

               idx1 = _STORAGE_SIZE2(
               pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize2,
               pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

               size2BuiltIn = getNumElemOutOfStorageSize(idx1,
               pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize2,
               0);

               if (!IsFieldSizeInt(
                  pTable[tableIdx][fieldId].fieldStorageSize2)) {
                  enumIdx = ((pTable[tableIdx][fieldId].fieldStorageSize2 &
                     FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
                  if (NvEnumsFromBin[enumIdx][m].enumValuePeer == 0xFF) {
                     *pos = *pos + (fieldCount/size1Bin) * numElem;
                     continue;
                  }
               }
               else {
                  if ((m+1) > size2BuiltIn) {
                     *pos = *pos + (fieldCount/size1Bin) * numElem;
                     continue;
                  }
               }
            }
            for (j=0; j < numElem; j++) {
               if (storageType == ARRAY_1) {
                  fieldSize = 0;
                  getBuiltInFieldCount(tableIdx,
                  pTable[tableIdx][fieldId].fieldName, &tableIdxBuiltIn,
                  &fieldIdBuiltIn, &numElemBuiltIn);

                  idx1 = _STORAGE_SIZE1(
                  pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1,
                  pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageType);

                  size1BuiltIn = getNumElemOutOfStorageSize(idx1,
                  pTableBuiltIn[tableIdxBuiltIn][fieldIdBuiltIn].fieldStorageSize1,
                  0);

                  if (!IsFieldSizeInt(
                        pTable[tableIdx][fieldId].fieldStorageSize1)) {
                     enumIdx = ((pTable[tableIdx][fieldId].fieldStorageSize1 &
                        FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);
                     if (NvEnumsFromBin[enumIdx][j].enumValuePeer == 0xFF) {
                        *pos = *pos + (fieldCount/size1Bin);
                        continue;
                     }
                  }
                  else {
                     if ((j+1) > size1BuiltIn) {
                        *pos = *pos + (fieldCount/size1Bin);
                        continue;
                     }
                  }
               }

               for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
                  if (nul == pTable[idxSubTable][i].fieldName[0]) {
                   // data parsing done for this stream
                      break;
                  }

                  idx = _STORAGE_TYPE(pTable[idxSubTable][i].fieldStorageType);
                  numSubElem = numElemBasedOnStorageType[idx](
                     &(pTable[idxSubTable][i]),1);
                  numSubElem2 = numSubElem3 = 1;

                  if (idx == ARRAY_1) {
                     idx1 = _STORAGE_SIZE1(
                        pTable[idxSubTable][i].fieldStorageSize1,
                        pTable[idxSubTable][i].fieldStorageType);
                     numSubElem = getNumElemOutOfStorageSize(idx1,
                       pTable[idxSubTable][i].fieldStorageSize1, 1);
                  }
                  else if (idx == ARRAY_2) {
                     idx1 = _STORAGE_SIZE1(
                        pTable[idxSubTable][i].fieldStorageSize1,
                        pTable[idxSubTable][i].fieldStorageType);
                     numSubElem = getNumElemOutOfStorageSize(idx1,
                       pTable[idxSubTable][i].fieldStorageSize1, 1);

                     idx1 = _STORAGE_SIZE2(
                        pTable[idxSubTable][i].fieldStorageSize2,
                        pTable[idxSubTable][i].fieldStorageType);
                     numSubElem2 = getNumElemOutOfStorageSize(idx1,
                       pTable[idxSubTable][i].fieldStorageSize2, 1);
                  }
                  else if (idx == ARRAY_3) {
                     idx1 = _STORAGE_SIZE1(
                        pTable[idxSubTable][i].fieldStorageSize1,
                        pTable[idxSubTable][i].fieldStorageType);
                     numSubElem = getNumElemOutOfStorageSize(idx1,
                       pTable[idxSubTable][i].fieldStorageSize1, 1);

                     idx1 = _STORAGE_SIZE2(
                        pTable[idxSubTable][i].fieldStorageSize2,
                        pTable[idxSubTable][i].fieldStorageType);
                     numSubElem2 = getNumElemOutOfStorageSize(idx1,
                       pTable[idxSubTable][i].fieldStorageSize2, 1);

                     idx1 = _STORAGE_SIZE3(
                        pTable[idxSubTable][i].fieldStorageSize3,
                        pTable[idxSubTable][i].fieldStorageType);
                     numSubElem3 = getNumElemOutOfStorageSize(idx1,
                       pTable[idxSubTable][i].fieldStorageSize3, 1);
                  }

                  if (_OFFSET_NOT_SET != pTable[idxSubTable][i].offset) {
                     if ( pTable[tableIdx][fieldId].offset == addOffset ) {
                        parseSubDataTableAndCopy(idxSubTable, numSubElem,
                           numSubElem2, numSubElem3, i, pStream, pos,
                           addOffset, tableBaseOffset, localAddOffset);
                     }
                     else {
                  // NOT the first Entry in the table..
                        if ( !pTable[tableIdx][fieldId].offset ) {
                           parseSubDataTableAndCopy(idxSubTable, numSubElem,
                             numSubElem2, numSubElem3, i, pStream, pos,
                             addOffset, tableBaseOffset, localAddOffset);
                        }
                        else {
                           //First Entry in the the table..
                           //(Sending parent offset..)
                           parseSubDataTableAndCopy(idxSubTable, numSubElem,
                              numSubElem2, numSubElem3, i, pStream, pos,
                              addOffset, tableBaseOffset,
                              pTable[tableIdx][fieldId].offset);
                        }
                     }
                  }
                  else {
                     fieldSize = 0;
                     fieldCount = getFieldCount(idxSubTable, i, numSubElem, 1);
                     *pos += fieldCount;
                  }
              }

              localAddOffset = localAddOffset + incAddOffset;
           }
        }
     }
  }

  return;
}

static void processNvData(_NV_STREAM_BUF *pStream, int len)
{
   int tableIdx, pos, idx = 0, addOffset = 0, i;
   int numElem = 0, additionalOffset = 0, tableBaseOffset = 0;
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX] = NvTablesFromBin;

    // fetch the table template
   pos = 0; // stream header byte is already checked, that's why we are here
   pos += _NV_BIN_DATA_STREAM_TABLEID_BYTE;
   tableIdx = (pStream[_NV_BIN_DATA_STREAM_TABLEID_BYTE] &
       FIELD_ID_TABLE_OR_ENUM_IDX_MASK);
   pos++;

   // call the table parsing
   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
      if (nul == pTable[0][i].fieldName[0]) {
          break;
      }
      if (tableIdx == _TABLE_IDX(pTable[0][i].fieldId)) {
         // Table base offset, stored in the "table of all tables" (index 0),
         // will be added to
         // fields relative offset in all tables.
         tableBaseOffset = pTable[0][i].offset;

         idx = _STORAGE_TYPE(pTable[0][i].fieldStorageType);

         // number of element
         numElem = numElemBasedOnStorageType[idx](&(pTable[0][i]),1);

         // recursive calls for the total size of the subtable, which may
         // contain nested tables
         subTableSize = 0;
         parseSubDataTable4Size(tableIdx, numElem);

         // additional offset for EACH subsequent table element
         additionalOffset = subTableSize/numElem;

         break;
      }
   }

   if (numElem) {
      for (i = 0; i < numElem; i++) {
          addOffset = (i * additionalOffset);
          parseDataTable_new(pStream, &pos, tableIdx, addOffset,
             tableBaseOffset);
      }
   }

   // the above recursive data table parser takes care of the nested tables
   // all done
   return;
}

static void parseDataTable_new(_NV_STREAM_BUF *pStream, int* pos, int tableIdx,
   int addOffset, int tableBaseOffset)
{
   _NV_TEMPLATE_TABLE (*pTable)[TABLE_ENTRIES_MAX] = NvTablesFromBin;
   int i, idx, fieldCount;
   int numElem, numElem2, numElem3, storageType, idxSubTable;

   // "apply" template to data -- parsing the actual NV data,
   //     so far we have been parsing and building templates
   for (i = 0; i < TABLE_ENTRIES_MAX; i++) {
      if (nul == pTable[tableIdx][i].fieldName[0]) {
          // data parsing done for this stream
          break;
      }

      // get size of the sub-table
      idxSubTable = (pTable[tableIdx][i].fieldId &
                             FIELD_ID_TABLE_OR_ENUM_IDX_MASK);

      idx = _STORAGE_TYPE(pTable[tableIdx][i].fieldStorageType);

      numElem = numElemBasedOnStorageType[idx](&(pTable[tableIdx][i]),1);

      addOffset = pTable[tableIdx][i].offset;

      fieldSize = 0;
      fieldCount = getFieldCount(tableIdx, i, numElem, 1);

      numElem2 = numElem3 = 1;

      if (idx == ARRAY_1 ) {
         storageType = _STORAGE_SIZE1(pTable[tableIdx][i].fieldStorageSize1,
                       pTable[tableIdx][i].fieldStorageType);
         numElem = getNumElemOutOfStorageSize(storageType,
                 pTable[tableIdx][i].fieldStorageSize1, 1);
      }
      else if (idx == ARRAY_2) {
         storageType = _STORAGE_SIZE1(pTable[tableIdx][i].fieldStorageSize1,
                       pTable[tableIdx][i].fieldStorageType);

         numElem = getNumElemOutOfStorageSize(storageType,
                 pTable[tableIdx][i].fieldStorageSize1, 1);

         storageType = _STORAGE_SIZE2(pTable[tableIdx][i].fieldStorageSize2,
                       pTable[tableIdx][i].fieldStorageType);

         numElem2 = getNumElemOutOfStorageSize(storageType,
                 pTable[tableIdx][i].fieldStorageSize2, 1);
      }
      else if (idx == ARRAY_3) {
         storageType = _STORAGE_SIZE1(pTable[tableIdx][i].fieldStorageSize1,
                       pTable[tableIdx][i].fieldStorageType);

         numElem = getNumElemOutOfStorageSize(storageType,
                 pTable[tableIdx][i].fieldStorageSize1, 1);

         storageType = _STORAGE_SIZE2(pTable[tableIdx][i].fieldStorageSize2,
                       pTable[tableIdx][i].fieldStorageType);

         numElem2 = getNumElemOutOfStorageSize(storageType,
                 pTable[tableIdx][i].fieldStorageSize2, 1);

         storageType = _STORAGE_SIZE3(pTable[tableIdx][i].fieldStorageSize3,
                       pTable[tableIdx][i].fieldStorageType);

         numElem3 = getNumElemOutOfStorageSize(storageType,
                 pTable[tableIdx][i].fieldStorageSize3, 1);
      }

      if (_OFFSET_NOT_SET != pTable[tableIdx][i].offset) {
         parseSubDataTableAndCopy(tableIdx, numElem, numElem2, numElem3, i,
                 pStream, pos, addOffset, tableBaseOffset, 0);
      }
      else {
         *pos += fieldCount;
      }
   }
}

static void getBasicDataSize(_NV_TEMPLATE_TABLE *pTableEntry)
{
   int numElem, sizeOneElem, totalSize;
   int idx, idx1;

   // number of element
   idx = _STORAGE_TYPE(pTableEntry->fieldStorageType);
   numElem = numElemBasedOnStorageType[idx](pTableEntry, 1);

   // size of each element
   idx1 = pTableEntry->fieldId & FIELD_ID_TABLE_OR_ENUM_IDX_MASK;
   sizeOneElem = sizeOneElemBasedOnFieldIdBasicDataType[idx1];

   // total size in bytes
   totalSize = numElem * sizeOneElem;

   // all done, update global
   subTableSize += totalSize;

   return;
}

static int numElemSingular(_NV_TEMPLATE_TABLE *pTableEntry, int unused)
{
    return 1;
}

static int getNumElemOutOfStorageSize(int fieldStorageSize,
   uint8 fieldStorageSizeLowByte, int nvBin)
{
   int ret = 0;
   if (IsFieldSizeInt(fieldStorageSizeLowByte)) {
       return fieldStorageSize;
   }
   else {
      int maxEnumVal=0, i;
      _NV_TEMPLATE_ENUM (*pEnum)[ENUM_ENTRIES_MAX];
      int enumIdx = ((fieldStorageSizeLowByte &
         FIELD_SIZE_VALUE_MASK) >> FIELD_SIZE_VALUE_LSB);

      if (nvBin) {
         pEnum = NvEnumsFromBin;
      }
      else {
         pEnum = NvEnumsBuiltIn;
      }

      for (i = 0; i < ENUM_ENTRIES_MAX; i++) {
          if (nul == pEnum[enumIdx][i].enumName[0]) {
              if ( i == 0 ) {
                 maxEnumVal = 0;
              }
              else {
                 maxEnumVal = pEnum[enumIdx][i-1].enumValue;
              }
              break;
          }
      }
      ret = (maxEnumVal + 1);
      return ret; // +1 to count for 0 to maxEnumVal
   }
}

static int numElemArray1(_NV_TEMPLATE_TABLE *pTableEntry, int nvBin)
{
   int fieldStorageSize = 0;

   fieldStorageSize = getNumElemOutOfStorageSize(_STORAGE_SIZE1(
             pTableEntry->fieldStorageSize1, pTableEntry->fieldStorageType),
             pTableEntry->fieldStorageSize1, nvBin);

   return fieldStorageSize;
}

static int numElemArray2(_NV_TEMPLATE_TABLE *pTableEntry, int nvBin)
{
   int fieldStorageSize1,fieldStorageSize2,fieldStorageSize;

   fieldStorageSize1 = getNumElemOutOfStorageSize(_STORAGE_SIZE1(
                         pTableEntry->fieldStorageSize1,
                         pTableEntry->fieldStorageType),
                         pTableEntry->fieldStorageSize1, nvBin);

   fieldStorageSize2 = getNumElemOutOfStorageSize(_STORAGE_SIZE2(
                        pTableEntry->fieldStorageSize2,
                        pTableEntry->fieldStorageType),
                        pTableEntry->fieldStorageSize2, nvBin);

   fieldStorageSize = fieldStorageSize1 * fieldStorageSize2;

   return fieldStorageSize;
}

static int numElemArray3(_NV_TEMPLATE_TABLE *pTableEntry, int nvBin)
{
   int fieldStorageSize1,fieldStorageSize2,fieldStorageSize3,fieldStorageSize;

   fieldStorageSize1 = getNumElemOutOfStorageSize(_STORAGE_SIZE1(
                         pTableEntry->fieldStorageSize1,
                         pTableEntry->fieldStorageType),
                         pTableEntry->fieldStorageSize1, nvBin);

   fieldStorageSize2 = getNumElemOutOfStorageSize(_STORAGE_SIZE2(
                        pTableEntry->fieldStorageSize2,
                        pTableEntry->fieldStorageType),
                        pTableEntry->fieldStorageSize2, nvBin);

   fieldStorageSize3 = getNumElemOutOfStorageSize(_STORAGE_SIZE3(
                        pTableEntry->fieldStorageSize3,
                        pTableEntry->fieldStorageType),
                        pTableEntry->fieldStorageSize3, nvBin);

   fieldStorageSize = fieldStorageSize1 * fieldStorageSize2 * fieldStorageSize3;

   return fieldStorageSize;
}
