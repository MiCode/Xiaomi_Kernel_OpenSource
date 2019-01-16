/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/common/dbgce.c#1 $
*/

/*! \file   dbgce.c
    \brief  This file contains the debug routines of Windows CE driver.
 */



/*
** $Log: dbgce.c $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 01 04 2011 cp.wu
 * [WCXRP00000338] [MT6620 Wi-Fi][Driver] Separate kalMemAlloc into kmalloc and vmalloc implementations to ease physically continous memory demands
 * separate kalMemAlloc() into virtually-continous and physically-continous type to ease slab system pressure
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-09-09 17:27:53 GMT mtk01084
**  modify for debug functions
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-04-17 18:15:55 GMT mtk01426
**  Don't use dynamic memory allocate for debug message
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:36:47 GMT mtk01426
**  Init for develop
**
*/

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include "gl_kal.h"

/*******************************************************************************
*                          C O N S T A N T S
********************************************************************************
*/
#define MAX_RECORD_NUM      128
#define MAX_STR_LEN         255
#define MAX_STR_BUF_SIZE    (MAX_STR_LEN + 1)

#define DEBUG_FILE_NAME     "\\temp\\mt_log.txt"

/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/
typedef struct _DEBUG_MSG_FORMAT {
	UINT_32 id;
	UINT_32 sysTime;
	CHAR dbgStr[MAX_STR_BUF_SIZE];
} DEBUG_MSG_FORMAT, *PDEBUG_MSG_FORMAT;

typedef struct _DEBUG_MSG_POOL {
	UINT_32 currentIndex;
	DEBUG_MSG_FORMAT dbgMsg[1];
} DEBUG_MSG_POOL, *PDEBUG_MSG_POOL;

/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/
#if DBG
#if LOG_METHOD == MSG_DEBUG_MSG
static UINT_32 g_logId;
static HANDLE hMapFile;
static PDEBUG_MSG_POOL pBuf;

TCHAR szName[] = TEXT("MyFileMappingObject");
#endif
#endif
/*******************************************************************************
*                             M A C R O S
********************************************************************************
*/

/*******************************************************************************
*              F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/
#if DBG
#if LOG_METHOD == MSG_DEBUG_MSG
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to create a log file.
*
* \param none
*
* \retval 0 If create or open the log file successfully
* \retval 1 If fail to create or open the log file
* \retval 2 If fail to map the log file
*
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS dbgFileCreate(VOID)
{
	hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE,
				     NULL,
				     PAGE_READWRITE,
				     0,
				     MAX_RECORD_NUM * sizeof(DEBUG_MSG_FORMAT) +
				     sizeof(DEBUG_MSG_POOL), szName);

	if (hMapFile == NULL) {
		TCHAR buf[256];

		_stprintf(buf, TEXT("CreateFileMapping Error. Code:%d\n"), GetLastError());
		MessageBox(NULL, buf, TEXT("failed."), MB_OK);
		return 1;
	}

	pBuf = (PDEBUG_MSG_POOL) MapViewOfFile(hMapFile,	/* handle to map object */
					       FILE_MAP_ALL_ACCESS,	/* read/write permission */
					       0,
					       0,
					       MAX_RECORD_NUM * sizeof(DEBUG_MSG_FORMAT) +
					       sizeof(DEBUG_MSG_POOL));

	if (pBuf == NULL) {
		TCHAR buf[256];

		_stprintf(buf, TEXT("MapViewOfFile Error. Code:%d\n"), GetLastError());
		MessageBox(NULL, buf, TEXT("MapViewOfFile"), MB_OK);
		return 2;
	}

	pBuf->currentIndex = 0;
	return 0;
}				/* dbgFileCreate */

/*----------------------------------------------------------------------------*/
/*!
* \brief Close the log file.
*
* \param none
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID closeLog(void)
{
	UnmapViewOfFile(pBuf);

	CloseHandle(hMapFile);
}				/* closeLog */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write message to a log file.
*
* \param[in] debugStr printf style format string
* \param[in] ... value for format string
*
* \retval NDIS_STATUS_SUCCESS If log the message successfully
* \retval NDIS_STATUS_FAILURE If fail to log the message
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS dbgLogWr(IN PINT_8 debugStr, IN ...
    )
{
	va_list paramList;
	PINT_8 buf_p = NULL;
	UINT_32 systemUpTime;

	if (pBuf == NULL) {
		return NDIS_STATUS_FAILURE;
	}

	/* Format log message: systemTime + message */
	buf_p = pBuf->dbgMsg[pBuf->currentIndex].dbgStr;

	va_start(paramList, debugStr);
	vsprintf(buf_p, debugStr, paramList);	/* 11: 10-digit time and 1 space */

	/* Get system time */
	NdisGetSystemUpTime(&systemUpTime);
	pBuf->dbgMsg[pBuf->currentIndex].sysTime = systemUpTime;
	pBuf->dbgMsg[pBuf->currentIndex].id = g_logId;

	g_logId++;
	pBuf->currentIndex++;
	if (pBuf->currentIndex >= MAX_RECORD_NUM) {
		pBuf->currentIndex = (pBuf->currentIndex) % MAX_RECORD_NUM;
	}

	return NDIS_STATUS_SUCCESS;

}				/* dbgLogWr */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write message to a log file.
*        (Without timestamp)
*
* \param[in] debugStr printf style format string
* \param[in] ... value for format string
*
* \retval NDIS_STATUS_SUCCESS If log the message successfully
* \retval NDIS_STATUS_FAILURE If fail to log the message
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS dbgLogWr1(IN PINT_8 debugStr, IN ...
    )
{
	PINT_8 buf_p = NULL;
	UINT_32 systemUpTime;
	va_list paramList;

	if (pBuf == NULL) {
		return NDIS_STATUS_FAILURE;
	}

	/* Format log message: systemTime + message */
	buf_p = pBuf->dbgMsg[pBuf->currentIndex].dbgStr;

	va_start(paramList, debugStr);
	vsprintf(buf_p, debugStr, paramList);	/* 11: 10-digit time and 1 space */

	/* Get system time */
	NdisGetSystemUpTime(&systemUpTime);
	pBuf->dbgMsg[pBuf->currentIndex].sysTime = systemUpTime;
	pBuf->dbgMsg[pBuf->currentIndex].id = g_logId;

	g_logId++;
	pBuf->currentIndex++;
	if (pBuf->currentIndex >= MAX_RECORD_NUM) {
		pBuf->currentIndex = (pBuf->currentIndex) % MAX_RECORD_NUM;
	}

	return NDIS_STATUS_SUCCESS;
}				/* dbgLogWr1 */

#elif LOG_METHOD == FILE_DEBUG_MSG
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to create a log file.
*
* \param none
*
* \retval NDIS_STATUS_SUCCESS If create or open the log file successfully
* \retval NDIS_STATUS_FAILURE If fail to create or open the log file
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS dbgFileCreate(VOID)
{
	HANDLE FileHandle = INVALID_HANDLE_VALUE;
	NDIS_STATUS status = NDIS_STATUS_SUCCESS;

	/* Check whether if the log file exist or not */
	FileHandle = CreateFile(TEXT(DEBUG_FILE_NAME), GENERIC_WRITE, 0,	/* No sharing */
				NULL,	/* Handle cannot be inherited */
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	/* If the log file does not exist, create it. */
	if (FileHandle == INVALID_HANDLE_VALUE) {
		FileHandle = CreateFile(TEXT(DEBUG_FILE_NAME), GENERIC_WRITE, 0,	/* No sharing */
					NULL,	/* Handle cannot be inherited */
					CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (FileHandle == INVALID_HANDLE_VALUE) {
			status = NDIS_STATUS_FAILURE;
		}
	}

	if (FileHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(FileHandle);
	}

	return status;

}				/* dbgFileCreate */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write message with timestamp to a log file.
*
* \param[in] debugStr printf style format string
* \param[in] ... value for format string
*
* \retval NDIS_STATUS_SUCCESS If log the message successfully
* \retval NDIS_STATUS_FAILURE If fail to log the message
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS dbgLogWr(IN PINT_8 debugStr, IN ...
    )
{
#define TMP_BUF_LEN  256

	int strLen;
	DWORD BytesWritten;
	PINT_8 buf_p = NULL;
	HANDLE FileHandle = NULL;
	va_list paramList;
	UINT_32 systemUpTime;

	/* Open the log file */
	FileHandle = CreateFile(TEXT(DEBUG_FILE_NAME), GENERIC_WRITE, 0,	/* No sharing */
				NULL,	/* Handle cannot be inherited */
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (FileHandle == INVALID_HANDLE_VALUE) {
		return NDIS_STATUS_FAILURE;
	}

	/* Create log message buffer */
	buf_p = (PUINT_8) kalMemAlloc(TMP_BUF_LEN, VIR_MEM_TYPE);
	if (buf_p == NULL) {
		CloseHandle(FileHandle);
		return NDIS_STATUS_FAILURE;
	}

	/* Get system time */
	NdisGetSystemUpTime(&systemUpTime);

	/* Format log message: systemTime + message */
	kalMemZero(buf_p, TMP_BUF_LEN);
	va_start(paramList, debugStr);
	sprintf(buf_p, "%10d ", systemUpTime);

	/* 11: 10-digit time and 1 space */
	_vsnprintf(buf_p + 11, TMP_BUF_LEN - 12, debugStr, paramList);

	strLen = strlen(buf_p);

	/* Write message log to log file */
	SetFilePointer(FileHandle, 0, 0, FILE_END);
	WriteFile(FileHandle, buf_p, strLen, &BytesWritten, NULL);

	CloseHandle(FileHandle);
	kalMemFree(buf_p, VIR_MEM_TYPE, TMP_BUF_LEN);

	return NDIS_STATUS_SUCCESS;

}				/* dbgLogWr */

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to write message to a log file.
*        (Without timestamp)
*
* \param[in] debugStr printf style format string
* \param[in] ... value for format string
*
* \retval NDIS_STATUS_SUCCESS If log the message successfully
* \retval NDIS_STATUS_FAILURE If fail to log the message
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS dbgLogWr1(IN PINT_8 debugStr, IN ...
    )
{
#define TMP_BUF_LEN  256

	HANDLE FileHandle = NULL;
	DWORD BytesWritten;
	va_list paramList;
	PINT_8 buf_p = NULL;
	int strLen;

	/* Open the log file */
	FileHandle = CreateFile(TEXT(DEBUG_FILE_NAME), GENERIC_WRITE, 0,	/* No sharing */
				NULL,	/* Handle cannot be inherited */
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (FileHandle == INVALID_HANDLE_VALUE) {
		return NDIS_STATUS_FAILURE;
	}

	/* Create log message buffer */
	buf_p = (PUINT_8) kalMemAlloc(TMP_BUF_LEN);
	if (buf_p == NULL) {
		CloseHandle(FileHandle);
		return NDIS_STATUS_FAILURE;
	}

	/* Format log message: systemTime + message */
	kalMemZero(buf_p, TMP_BUF_LEN);
	va_start(paramList, debugStr);
	_vsnprintf(buf_p, TMP_BUF_LEN - 1, debugStr, paramList);

	strLen = strlen(buf_p);

	/* Write message log to log file */
	SetFilePointer(FileHandle, 0, 0, FILE_END);
	WriteFile(FileHandle, buf_p, strLen, &BytesWritten, NULL);

	CloseHandle(FileHandle);
	kalMemFree(buf_p, TMP_BUF_LEN);

	return NDIS_STATUS_SUCCESS;

}				/* dbgLogWr1 */
#endif

#elif UNICODE_MESSAGE
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to print unicode message.
*
* \param[in] debugStr printf style format string
* \param[in] ... value for format string
*
* \retval NDIS_STATUS_SUCCESS If log the message successfully
* \retval NDIS_STATUS_FAILURE If fail to log the message
*
/*----------------------------------------------------------------------------*/
NDIS_STATUS dbgLogWr2(IN PINT_8 debugStr, IN ...
    )
{
#define TMP_BUF_LEN   256
#define TMP_WBUF_LEN  (TMP_BUF_LEN * 2)

	va_list paramList;
	PINT_16 wbuf_p;
	PINT_8 buf_p;
	INT_32 strLen;

	/* Create log message buffer */
	buf_p = (PINT_8) kalMemAlloc(TMP_BUF_LEN, VIR_MEM_TYPE);
	if (buf_p == NULL) {
		return NDIS_STATUS_FAILURE;
	}
	wbuf_p = (PINT_16) kalMemAlloc(TMP_WBUF_LEN, VIR_MEM_TYPE);
	if (wbuf_p == NULL) {
		kalMemFree(buf_p, VIR_MEM_TYPE, TMP_BUF_LEN);
		return NDIS_STATUS_FAILURE;
	}

	/* Format message */
	kalMemZero(buf_p, TMP_BUF_LEN);
	kalMemZero(wbuf_p, TMP_WBUF_LEN);
	va_start(paramList, debugStr);
	_vsnprintf(buf_p, TMP_BUF_LEN - 1, debugStr, paramList);
	va_end(paramList);
	strLen = strlen(buf_p);

	/* Converts a sequence of multibyte characters to a corresponding sequence
	   of wide characters. */
	mbstowcs(wbuf_p, buf_p, strLen);

	/* Print unicode message */
	NKDbgPrintfW(TEXT("%s"), wbuf_p);

	/* Free message buffer */
	kalMemFree(buf_p, VIR_MEM_TYPE, TMP_BUF_LEN);
	kalMemFree(wbuf_p, VIR_MEM_TYPE, TMP_WBUF_LEN);

	return NDIS_STATUS_SUCCESS;
}				/* dbgLogWr2 */
#endif				/* UNICODE_MESSAGE */

#endif				/* #if DBG */
