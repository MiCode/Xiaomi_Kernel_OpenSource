
#include "tfa_service.h"
#include "tfa98xx_tfafieldnames.h"
#include "config.h"

/* support for error code translation into text */
static char latest_errorstr[64];

const char *tfa98xx_get_error_string(enum Tfa98xx_Error error)
{
	const char *pErrStr;

	switch (error) {
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
	case Tfa98xx_Error_RpcInfoId:
		pErrStr = "RpcInfoId";
		break;
	case Tfa98xx_Error_RpcNotAllowedSpeaker:
		pErrStr = "RpcNotAllowedSpeaker";
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
static char *tfa_bf2name(tfaBfName_t *table, uint16_t bf)
{
	int n = 0;

	do {
		if (table[n].bfEnum == bf)
			return table[n].bfName;
	} while (table[n++].bfEnum != 0xffff);

	return table[n-1].bfName; /* last name says unkown */
}
/**
 * lookup name in table
 *   return 0xffff if not found
 */
static uint16_t tfa_name2bf(tfaBfName_t *table, const char *name)
{
	int n = 0;

	do {
#if defined(WIN32) || defined(_X64)
		if (_stricmp(name, table[n].bfName) == 0)
			return table[n].bfEnum;
#else
		if (strcasecmp(name, table[n].bfName) == 0)
			return table[n].bfEnum;
#endif
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
TFA9890_NAMETABLE
TFA9891_NAMETABLE
TFA9887_NAMETABLE
TFA1_BITNAMETABLE
TFA9890_BITNAMETABLE
TFA9891_BITNAMETABLE
TFA9887_BITNAMETABLE
char *tfaContBitName(uint16_t num, unsigned short rev)
{
	char *name;
	 /* end of list for the unknown string */
	int tableLength = sizeof(Tfa1DatasheetNames)/sizeof(tfaBfName_t);
	const char *unknown = Tfa1DatasheetNames[tableLength-1].bfName;

	switch (rev & 0xff) {
	case 0x88:
		name =  tfa_bf2name(Tfa2BitNames, num);
		break;
	case 0x97:
		name =  tfa_bf2name(Tfa1BitNames, num);
		break;
	case 0x92:
		name =  tfa_bf2name(Tfa9891BitNames, num);
		if (strcmp(unknown, name) == 0)
			name = tfa_bf2name(Tfa9891BitNames, num);/* try long bitname table */
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		name = tfa_bf2name(Tfa9890BitNames, num); /* my tabel 1st */
		if (strcmp(unknown, name) == 0)
			name = tfa_bf2name(Tfa1BitNames, num); /* try generic table */
		break;
	case 0x12:
		name = tfa_bf2name(Tfa9887BitNames, num); /* my tabel 1st */
		if (strcmp(unknown, name) == 0)
			name = tfa_bf2name(Tfa1BitNames, num);/* try generic table */
		break;
	default:
		PRINT_ERROR("unknown REVID:0x%0x\n", rev);
		tableLength = sizeof(Tfa1BitNames)/sizeof(tfaBfName_t); /* end of list */
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
	const char *unknown = Tfa1DatasheetNames[tableLength-1].bfName;

	switch (rev & 0xff) {
	case 0x88:
		name =  tfa_bf2name(Tfa2DatasheetNames, num);
		break;
	case 0x97:
		name =  tfa_bf2name(Tfa1DatasheetNames, num);
		break;
	case 0x92:
		name =  tfa_bf2name(Tfa9891DatasheetNames, num);
		if (strcmp(unknown, name) == 0)
			name = tfa_bf2name(Tfa9891BitNames, num);/* try long bitname table */
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		name = tfa_bf2name(Tfa9890DatasheetNames, num); /* my tabel 1st */
		if (strcmp(unknown, name) == 0)
			name = tfa_bf2name(Tfa1DatasheetNames, num); /* try generic table */
		break;
	case 0x12:
		name = tfa_bf2name(Tfa9887DatasheetNames, num); /* my tabel 1st */
		if (strcmp(unknown, name) == 0)
			name = tfa_bf2name(Tfa1DatasheetNames, num);/* try generic table */
		break;
	default:
		PRINT_ERROR("unknown REVID:0x%0x\n", rev);
		tableLength = sizeof(Tfa1DatasheetNames)/sizeof(tfaBfName_t); /* end of list */
		name = (char *)unknown;
		break;
	}
	return name;
}

uint16_t tfaContBfEnum(const char *name, unsigned short rev)
{
	uint16_t bfnum;

	switch (rev & 0xff) {
	case 0x88:
		bfnum =  tfa_name2bf(Tfa2DatasheetNames, name);
		if (bfnum == 0xffff)
			bfnum = tfa_name2bf(Tfa2BitNames, name);/* try long bitname table */
		break;
	case 0x97:
		bfnum =  tfa_name2bf(Tfa1DatasheetNames, name);
		if (bfnum == 0xffff)
			bfnum = tfa_name2bf(Tfa1BitNames, name);/* try generic table */
		break;
	case 0x92:
		bfnum =  tfa_name2bf(Tfa9891DatasheetNames, name);
		if (bfnum == 0xffff)
			bfnum = tfa_name2bf(Tfa9891BitNames, name);/* try long bitname table */
		break;
	case 0x91:
	case 0x80:
	case 0x81:
		bfnum = tfa_name2bf(Tfa9890DatasheetNames, name); /* my tabel 1st */
		if (bfnum == 0xffff)
			bfnum = tfa_name2bf(Tfa1DatasheetNames, name);/* try generic table */
		if (bfnum == 0xffff)
			bfnum = tfa_name2bf(Tfa1BitNames, name); /* try 2nd generic table */
		break;
	case 0x12:
		bfnum = tfa_name2bf(Tfa9887DatasheetNames, name); /* my tabel 1st */
		if (bfnum == 0xffff)
			bfnum = tfa_name2bf(Tfa1DatasheetNames, name);/* try generic table */
		if (bfnum == 0xffff)
			bfnum = tfa_name2bf(Tfa1BitNames, name);/* try 2nd generic table */
		break;
	default:
		PRINT_ERROR("unknown REVID:0x%0x\n", rev);
		bfnum = 0xffff;
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
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa1DatasheetNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa9891DatasheetNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa9890DatasheetNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa9887DatasheetNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		/* and then bitfield names */
		bfnum =  tfa_name2bf(Tfa2BitNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa1BitNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa9891BitNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa9890BitNames, name);
		if (bfnum != 0xffff)
			return bfnum;
		bfnum =  tfa_name2bf(Tfa9887BitNames, name);

		return bfnum;

}
