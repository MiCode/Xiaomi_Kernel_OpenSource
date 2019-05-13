/*
 * Copyright 2014-2017 NXP Semiconductors
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dbgprint.h"
#include "tfa_service.h"
#include "tfa98xx_tfafieldnames.h"

/* support for error code translation into text */
static char latest_errorstr[64];

const char* tfa98xx_get_error_string(enum Tfa98xx_Error error)
{
  const char* pErrStr;

  switch (error)
  {
  case Tfa98xx_Error_Ok:
    pErrStr = "Ok";
    break;
  case Tfa98xx_Error_DSP_not_running:
    pErrStr = "DSP_not_running";
    break;
  case Tfa98xx_Error_Bad_Parameter:
    pErrStr = "Bad_Parameter";
    break;
  case Tfa98xx_Error_NotOpen:
    pErrStr = "NotOpen";
    break;
  case Tfa98xx_Error_InUse:
    pErrStr = "InUse";
    break;
  case Tfa98xx_Error_RpcBusy:
    pErrStr = "RpcBusy";
    break;
  case Tfa98xx_Error_RpcModId:
    pErrStr = "RpcModId";
    break;
  case Tfa98xx_Error_RpcParamId:
    pErrStr = "RpcParamId";
    break;
  case Tfa98xx_Error_RpcInvalidCC:
    pErrStr = "RpcInvalidCC";
    break;
  case Tfa98xx_Error_RpcInvalidSeq:
    pErrStr = "RpcInvalidSeq";
    break;
  case Tfa98xx_Error_RpcInvalidParam:
    pErrStr = "RpcInvalidParam";
    break;
  case Tfa98xx_Error_RpcBufferOverflow:
    pErrStr = "RpcBufferOverflow";
    break;
  case Tfa98xx_Error_RpcCalibBusy:
    pErrStr = "RpcCalibBusy";
    break;
  case Tfa98xx_Error_RpcCalibFailed:
    pErrStr = "RpcCalibFailed";
    break;
  case Tfa98xx_Error_Not_Supported:
    pErrStr = "Not_Supported";
    break;
  case Tfa98xx_Error_I2C_Fatal:
    pErrStr = "I2C_Fatal";
    break;
  case Tfa98xx_Error_I2C_NonFatal:
    pErrStr = "I2C_NonFatal";
    break;
  case Tfa98xx_Error_StateTimedOut:
	pErrStr = "WaitForState_TimedOut";
	break;
  default:
    sprintf(latest_errorstr, "Unspecified error (%d)", (int)error);
    pErrStr = latest_errorstr;
  }
  return pErrStr;
}
/*****************************************************************************/
/*      bitfield lookups */
/*
 * generic table lookup functions
 */
/**
 * lookup bf in table
 *   return 'unkown' if not found
 */
static char *tfa_bf2name(tfaBfName_t *table, uint16_t bf) {
	int n=0;

	do {
		if ((table[n].bfEnum & 0xfff0 ) == (bf & 0xfff0 )) {
			return table[n].bfName;
		}
	}
	while( table[n++].bfEnum != 0xffff);

	return table[n-1].bfName; /* last name says unkown */
}
/**
 * lookup name in table
 *   return 0xffff if not found
 */
static uint16_t tfa_name2bf(tfaBfName_t *table,const  char *name) {
	int n = 0;

	do {
		if (strcasecmp(name, table[n].bfName)==0)
			return table[n].bfEnum;
	} while (table[n++].bfEnum != 0xffff);

	return 0xffff;
}

/*
 * tfa2 bitfield name table
 */
TFA2_NAMETABLE
TFA2_BITNAMETABLE

/*
 * tfa1 bitfield name tables
 */
TFA1_NAMETABLE
TFA9896_NAMETABLE
TFA9872_NAMETABLE
TFA9874_NAMETABLE
TFA9890_NAMETABLE
TFA9891_NAMETABLE
TFA9887_NAMETABLE
TFA1_BITNAMETABLE
TFA9912_NAMETABLE
TFA9894_NAMETABLE
TFA9896_BITNAMETABLE
TFA9872_BITNAMETABLE
TFA9874_BITNAMETABLE
TFA9912_BITNAMETABLE
TFA9890_BITNAMETABLE
TFA9891_BITNAMETABLE
TFA9887_BITNAMETABLE
TFA9894_BITNAMETABLE

char *tfaContBitName(uint16_t num, unsigned short rev)
{
	char *name;
	 /* end of list for the unknown string */
	int tableLength = sizeof(Tfa1DatasheetNames)/sizeof(tfaBfName_t);
	const char *unknown=Tfa1DatasheetNames[tableLength-1].bfName;

	switch (rev & 0xff) {
	case 0x88:
		name =  tfa_bf2name(Tfa2BitNames, num);
		break;
	case 0x97:
		name =  tfa_bf2name(Tfa1BitNames, num);
		break;
	case 0x96:
		name =  tfa_bf2name(Tfa9896BitNames, num);
		break;
	case 0x72:
		name =  tfa_bf2name(Tfa9872BitNames, num);
		break;
	case 0x74:
		name =  tfa_bf2name(Tfa9874BitNames, num);
		break;
	case 0x92:
		name =  tfa_bf2name(Tfa9891BitNames, num);
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		name = tfa_bf2name(Tfa9890BitNames, num); /* my tabel 1st */
		if (strcmp(unknown, name)==0)
			name = tfa_bf2name(Tfa1BitNames, num); /* try generic table */
		break;
	case 0x12:
		name = tfa_bf2name(Tfa9887BitNames, num); /* my tabel 1st */
		if (strcmp(unknown, name)==0)
			name = tfa_bf2name(Tfa1BitNames, num); /* try generic table */
		break;
	case 0x13:
		name =  tfa_bf2name(Tfa9912BitNames, num);
		break;
	case 0x94:
		name = tfa_bf2name(Tfa9894BitNames, num);
		break;
	default:
		PRINT_ERROR("unknown REVID:0x%0x\n", rev);
		tableLength = sizeof(Tfa1BitNames)/sizeof(tfaBfName_t); /* end of list */
		name = (char *)unknown;
		break;
	}
	return name;
}

char *tfaContDsName(uint16_t num, unsigned short rev)
{
	char *name;
	 /* end of list for the unknown string */
	int tableLength = sizeof(Tfa1DatasheetNames)/sizeof(tfaBfName_t);
	const char *unknown=Tfa1DatasheetNames[tableLength-1].bfName;

	switch (rev & 0xff) {
	case 0x88:
		name =  tfa_bf2name(Tfa2DatasheetNames, num);
		break;
	case 0x97:
		name =  tfa_bf2name(Tfa1DatasheetNames, num);
		break;
	case 0x96:
		name =  tfa_bf2name(Tfa9896DatasheetNames, num);
		break;
	case 0x72:
		name =  tfa_bf2name(Tfa9872DatasheetNames, num);
		break;
	case 0x74:
		name =  tfa_bf2name(Tfa9874DatasheetNames, num);
		break;        
	case 0x92:
		name =  tfa_bf2name(Tfa9891DatasheetNames, num);
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		name = tfa_bf2name(Tfa9890DatasheetNames, num); /* my tabel 1st */
		if (strcmp(unknown, name)==0)
			name = tfa_bf2name(Tfa1DatasheetNames, num); /* try generic table */
		break;
	case 0x12:
		name = tfa_bf2name(Tfa9887DatasheetNames, num); /* my tabel 1st */
		if (strcmp(unknown, name)==0)
			name = tfa_bf2name(Tfa1DatasheetNames, num); /* try generic table */
		break;
	case 0x13:
		name =  tfa_bf2name(Tfa9912DatasheetNames, num);
		break;
	case 0x94:
		name =  tfa_bf2name(Tfa9894DatasheetNames, num);
		break;
	default:
		PRINT_ERROR("unknown REVID:0x%0x\n", rev);
		tableLength = sizeof(Tfa1DatasheetNames)/sizeof(tfaBfName_t); /* end of list */
		name = (char *)unknown;
		break;
	}
	return name;
}

char *tfaContBfName(uint16_t num, unsigned short rev)
{
	char *name;
	/* end of list for the unknown string */
	int tableLength = sizeof(Tfa1DatasheetNames)/sizeof(tfaBfName_t);
	const char *unknown=Tfa1DatasheetNames[tableLength-1].bfName;

	/* if datasheet name does not exist look for bitfieldname */
	name = tfaContDsName(num, rev);
	if (strcmp(unknown, name)==0)
		name = tfaContBitName(num, rev);

	return name;
}

uint16_t tfaContBfEnum(const char *name, unsigned short rev)
{
	uint16_t bfnum;

	switch (rev & 0xff) {
	case 0x88:
		bfnum =  tfa_name2bf(Tfa2DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa2BitNames, name);/* try long bitname table */
		break;
	case 0x97:
		bfnum =  tfa_name2bf(Tfa1DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa1BitNames, name);/* try generic table */
		break;
	case 0x96:
		bfnum =  tfa_name2bf(Tfa9896DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa9896BitNames, name);/* try generic table */
		break;
	case 0x72:
		bfnum =  tfa_name2bf(Tfa9872DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa9872BitNames, name);/* try long bitname table */
		break;
	case 0x74:
		bfnum =  tfa_name2bf(Tfa9874DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa9874BitNames, name);/* try long bitname table */
		break;        
	case 0x92:
		bfnum =  tfa_name2bf(Tfa9891DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa9891BitNames, name);/* try long bitname table */
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		bfnum = tfa_name2bf(Tfa9890DatasheetNames, name); /* my tabel 1st */
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa1DatasheetNames, name);/* try generic table */
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa1BitNames, name); /* try 2nd generic table */
		break;
	case 0x12:
		bfnum = tfa_name2bf(Tfa9887DatasheetNames, name); /* my tabel 1st */
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa1DatasheetNames, name);/* try generic table */
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa1BitNames, name);/* try 2nd generic table */
		break;
	case 0x13:
		bfnum =  tfa_name2bf(Tfa9912DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa9912BitNames, name);/* try long bitname table */
		break;
	case 0x94:
		bfnum =  tfa_name2bf(Tfa9894DatasheetNames, name);
		if (bfnum==0xffff)
			bfnum = tfa_name2bf(Tfa9894BitNames, name);/* try long bitname table */
		break;

	default:
		PRINT_ERROR("unknown REVID:0x%0x\n", rev);
		bfnum=0xffff;
		break;
	}

	return bfnum;
}

/*
 * check all lists for a hit
 *  this is for the parser to know if it's  an existing bitname
 */
uint16_t tfaContBfEnumAny(const char *name)
{
	uint16_t bfnum;

	/* datasheet names first */
	bfnum =  tfa_name2bf(Tfa2DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa1DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9891DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9890DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9887DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9872DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
   	bfnum =  tfa_name2bf(Tfa9874DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9896DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9912DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9894DatasheetNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	/* and then bitfield names */
	bfnum =  tfa_name2bf(Tfa2BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa1BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9891BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9890BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9887BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9872BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
    bfnum =  tfa_name2bf(Tfa9874BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum =  tfa_name2bf(Tfa9896BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum = tfa_name2bf(Tfa9912BitNames, name);
	if (bfnum!=0xffff)
		return bfnum;
	bfnum = tfa_name2bf(Tfa9894BitNames, name);

	return bfnum;
}
