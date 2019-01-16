/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ce/hif/sdio/ndisload.c#1 $
*/

/*! \file   ndisload.c
    \brief  Provide WinCE SDIO DLL related entry functions

*/



/*
** $Log: ndisload.c $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:29:27 GMT mtk01426
**  Init for develop
**
*/

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"

/*******************************************************************************
*                          C O N S T A N T S
********************************************************************************
*/
#define MAX_NUMBER_OF_ADAPTERS                  8


/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/

#define LOADER_INSTANCE_KEY     TEXT("Instance")


#if 0
#define MAX_MINIPORT_NAME_PATH                  256

    /*  miniport instance information */
typedef struct _MINIPORT_INSTANCE_INFO {
	WCHAR MiniportName[MAX_MINIPORT_NAME_PATH];
	WCHAR MiniportInstance[MAX_MINIPORT_NAME_PATH];
	WCHAR RegPath[MAX_MINIPORT_NAME_PATH];
	WCHAR ActiveKeyPath[MAX_MINIPORT_NAME_PATH];
	ULONG InstanceNumber;
} MINIPORT_INSTANCE_INFO, *PMINIPORT_INSTANCE_INFO;
#endif

BOOL AllocatedInstance[MAX_NUMBER_OF_ADAPTERS];

CRITICAL_SECTION LoaderCriticalSection;

BOOL devicePowerDown;

/*******************************************************************************
*                             M A C R O S
********************************************************************************
*/
#define SdDbgPrint(_Module, _Fmt)       DbgPrintZo(_Module, _Fmt)

/*******************************************************************************
*              F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
BOOL LoaderEntry(HINSTANCE hInstance, ULONG Reason, LPVOID pReserved);

BOOL LoadMiniport(PMINIPORT_INSTANCE_INFO pInstance);

VOID UnloadMiniport(PMINIPORT_INSTANCE_INFO pInstance);

/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief DLLL Entry
*
* \param[in] hDLL       handle to the DLL module
* \param[in] dwReason   reason to invoke this function
* \param[in] lpReserved
*
* \retval TRUE  success
*
/*----------------------------------------------------------------------------*/
BOOL WINAPI DllEntry(HINSTANCE hInstance, INT Reason, LPVOID Reserved)
{
	if (Reason == DLL_PROCESS_ATTACH) {
		/* Create debug file to log message */
		CREATE_LOG_FILE();

		/*  register debug zones for this module */
		SD_DEBUG_ZONE_REGISTER(hInstance, SDNDIS_REG_PATH);
#ifdef MT6620
		SDInitializeMemoryTagging(2, TEXT("MT6620SD"));
#endif
		DisableThreadLibraryCalls((HMODULE) hInstance);
	}

	if (Reason == DLL_PROCESS_DETACH) {
#if DEBUG
		SDCheckMemoryTags(NULL);
#endif

		SDDeleteMemoryTagging();

		SdDbgPrint(SDCARD_ZONE_INIT,
			   (TEXT("SDNdis: DllEntry - Reason == DLL_PROCESS_DETACH\n")));
	}

	return LoaderEntry(hInstance, Reason, Reserved);
}				/* DllEntry */

/*----------------------------------------------------------------------------*/
/*!
* \brief Init loader
*
* \param[in] hInstance  the instance that is attaching
* \param[in] Reason     the reason for attaching
* \param[in] pReserved  not much
*
* \retval TRUE  success
*
* \note     This is only used to initialize the zones
*
/*----------------------------------------------------------------------------*/
BOOL LoaderEntry(HINSTANCE hInstance, ULONG Reason, LPVOID pReserved)
{
	if (Reason == DLL_PROCESS_ATTACH) {
		DEBUGREGISTER(hInstance);
		InitializeCriticalSection(&LoaderCriticalSection);
		memset(&AllocatedInstance, 0, sizeof(AllocatedInstance));
	}

	if (Reason == DLL_PROCESS_DETACH) {
		DeleteCriticalSection(&LoaderCriticalSection);
	}

	return (TRUE);
}				/* LoaderEntry */

/*----------------------------------------------------------------------------*/
/*!
* \brief Load the miniport for this instance
*
* \param[in] pInstance  information for this instance
*
* \retval TRUE  success
*
/*----------------------------------------------------------------------------*/
BOOL LoadMiniport(PMINIPORT_INSTANCE_INFO pInstance)
{
#define STRING_BUF_SZ       128
#define INSTANCE_NAME_SZ    32
#define INSTANCE_NUMBER_SZ  10

	HKEY hKey;		/* registry key */
	DWORD win32Status;	/* status */
	DWORD dataSize;		/*  data size for query */
	WCHAR stringBuff[STRING_BUF_SZ];	/*  string buffer */
	WCHAR instanceKey[INSTANCE_NAME_SZ];	/*  instance name */
	WCHAR instanceNumber[INSTANCE_NUMBER_SZ];	/*  instance number */
	WCHAR *token;		/*  tokenizer */
	NDIS_STATUS NdisStatus;	/*  ndis status */

	/* open the registry path for this instance */
	if ((win32Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					pInstance->RegPath, 0, 0, &hKey)) != ERROR_SUCCESS) {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR:Failed to open path %s; %d\n"),
					       pInstance->RegPath, win32Status));
		return FALSE;
	}

	dataSize = sizeof(stringBuff);

	/* build up the instance key */
	wcscpy(instanceKey, LOADER_INSTANCE_KEY);
	_ultow(pInstance->InstanceNumber, instanceNumber, INSTANCE_NUMBER_SZ);
	wcscat(instanceKey, instanceNumber);

	/* retrieve the real reg path to the device parameters */
	if (RegQueryValueEx(hKey,
			    instanceKey,
			    0, NULL, (PUCHAR) stringBuff, &dataSize) != ERROR_SUCCESS) {

		SdDbgPrint(SDCARD_ZONE_ERROR,
			   (TEXT("SDNDISLDR: Failed to get the instance key : %d\n"),
			    instanceKey));
		RegCloseKey(hKey);
		return FALSE;
	}

	RegCloseKey(hKey);

	SdDbgPrint(SDCARD_ZONE_INIT,
		   (TEXT("SDNDISLDR: Tokenizing instance information: %s\n"), stringBuff));

	/* extract the miniport name and instance name, in the form of
	 *  "<Miniport Name>:<Miniport Instance>
	 */
	token = wcstok(stringBuff, TEXT(":"));

	if (token != NULL) {

		wcscpy(pInstance->MiniportName, token);

		/* search for the next one */
		token = wcstok(NULL, TEXT(":"));

		if (token != NULL) {
			wcscpy(pInstance->MiniportInstance, token);
		} else {
			SdDbgPrint(SDCARD_ZONE_ERROR,
				   (TEXT("SDNDISLDR: Failed to get miniport instance\n")));
			return FALSE;
		}
	} else {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR: Failed to get miniport name\n")));
		return FALSE;
	}

	/* build up the miniport instance path in order to stick in the
	   "ActivePath" key */
	wcscpy(stringBuff, TEXT("\\Comm\\"));
	wcscat(stringBuff, pInstance->MiniportInstance);
	wcscat(stringBuff, TEXT("\\Parms"));

	SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: Miniport instance path %s\n"), stringBuff));

	if ((win32Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					stringBuff, 0, 0, &hKey)) != ERROR_SUCCESS) {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR:Failed to open path %s; %d\n"),
					       stringBuff, win32Status));
		return FALSE;
	}

	/* make sure the key is deleted first */
	RegDeleteValue(hKey, TEXT("ActivePath"));

	SdDbgPrint(SDCARD_ZONE_INIT,
		   (TEXT("SDNDISLDR: Storing ActiveKey Path %s\n"), pInstance->ActiveKeyPath));

	/* save the active ActivePath in the registry path for the miniport.
	   The miniport portion will look up this key */
	if (RegSetValueEx(hKey,
			  TEXT("ActivePath"),
			  0,
			  REG_SZ,
			  (PUCHAR) pInstance->ActiveKeyPath,
			  ((sizeof(WCHAR)) * (wcslen(pInstance->ActiveKeyPath) + 1))) !=
	    ERROR_SUCCESS) {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR: Failed to set ActiveKey path\n")));
		RegCloseKey(hKey);
		return FALSE;
	}

	/* close the key */
	RegCloseKey(hKey);

	/* build up the miniport name path in order to add the "Group" key */
	wcscpy(stringBuff, TEXT("\\Comm\\"));
	wcscat(stringBuff, pInstance->MiniportName);
	SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: Miniport name path %s\n"), stringBuff));

	if ((win32Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					stringBuff, 0, 0, &hKey)) != ERROR_SUCCESS) {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR:Failed to open path %s; %d\n"),
					       stringBuff, win32Status));
		return FALSE;
	}

	/* Set the "Group" in the registry path for the miniport */
	wcscpy(stringBuff, TEXT("NDIS"));
	if (RegSetValueEx(hKey,
			  TEXT("Group"),
			  0,
			  REG_SZ,
			  (PUCHAR) stringBuff,
			  ((sizeof(WCHAR)) * (wcslen(stringBuff) + 1))) != ERROR_SUCCESS) {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR: Failed to set Group entry\n")));
		RegCloseKey(hKey);
		return FALSE;
	}

	/* close the key */
	RegCloseKey(hKey);

	/* register the adapter */
	NdisRegisterAdapter(&NdisStatus, pInstance->MiniportName, pInstance->MiniportInstance);

	if (!NDIS_SUCCESS(NdisStatus)) {
		SdDbgPrint(SDCARD_ZONE_ERROR,
			   (TEXT("SDNDISLDR: Failed to register the adapter\n")));
		return FALSE;
	}

	/* build up the miniport instance path in order to stick in the "ActivePath" key */
	wcscpy(stringBuff, TEXT("\\Comm\\"));
	wcscat(stringBuff, pInstance->MiniportInstance);
	wcscat(stringBuff, TEXT("\\Parms"));


	if ((win32Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					stringBuff, 0, 0, &hKey)) != ERROR_SUCCESS) {
		return FALSE;
	}
	RegDeleteValue(hKey, TEXT("ActivePath"));
	RegCloseKey(hKey);

	/* build up the miniport name path in order to delete "Group" entry */
	wcscpy(stringBuff, TEXT("\\Comm\\"));
	wcscat(stringBuff, pInstance->MiniportName);
	if ((win32Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					stringBuff, 0, 0, &hKey)) != ERROR_SUCCESS) {
		return FALSE;
	}
	RegDeleteValue(hKey, TEXT("Group"));
	RegCloseKey(hKey);

	return TRUE;

}				/* LoadMiniport */

/*----------------------------------------------------------------------------*/
/*!
* \brief Unload the miniport
*
* \param[in] pInstance  the instance to unload
*
* \retval none
*
/*----------------------------------------------------------------------------*/
VOID UnloadMiniport(PMINIPORT_INSTANCE_INFO pInstance)
{
	NDIS_STATUS NdisStatus;

	SdDbgPrint(SDCARD_ZONE_INIT,
		   (TEXT("SDNDISLDR: Unloading Miniport Instance %s\n"),
		    pInstance->MiniportInstance));

	NdisDeregisterAdapter(&NdisStatus, pInstance->MiniportInstance);

	SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: Miniport Unloaded 0x%08X\n"), NdisStatus));
}				/* UnloadMiniport */

/*----------------------------------------------------------------------------*/
/*!
* \brief The deinit entry point for this driver
*
* \param[in] hDeviceContext     the context returned from NDL_Init
*
* \retval TRUE  success
*
/*----------------------------------------------------------------------------*/
BOOL NDL_Deinit(DWORD hDeviceContext)
{
	PMINIPORT_INSTANCE_INFO pInstance;	/*  memory card instance */

	SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: +NDL_Deinit\n")));

	pInstance = (PMINIPORT_INSTANCE_INFO) hDeviceContext;

	/* unload the miniport */
	UnloadMiniport(pInstance);

	EnterCriticalSection(&LoaderCriticalSection);

	/* free our instance number */
	AllocatedInstance[pInstance->InstanceNumber] = FALSE;

	LeaveCriticalSection(&LoaderCriticalSection);

	LocalFree(pInstance);

	SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: -NDL_Deinit\n")));

	return TRUE;
}				/* NDL_Deinit */

/*----------------------------------------------------------------------------*/
/*!
* \brief The init entry point
*
* \param[in] dwContext  the context for this init
*
* \retval returns a non-zero context
*
/*----------------------------------------------------------------------------*/
DWORD NDL_Init(DWORD dwContext)
{
	PMINIPORT_INSTANCE_INFO pInstance;	/* this instance of the device */
	ULONG ii;


	SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: +NDL_Init\n")));

	pInstance = (PMINIPORT_INSTANCE_INFO)
	    LocalAlloc(LPTR, sizeof(MINIPORT_INSTANCE_INFO));

	if (pInstance == NULL) {
		SdDbgPrint(SDCARD_ZONE_ERROR,
			   (TEXT("SDNDISLDR: Failed to allocate device info\n")));
		SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: -NDL_Init\n")));
		return 0;
	}

	/* On CE, the dwContext is a pointer to a string to the "Active" registry path
	 * we pass this to the NDIS driver
	 */
	wcscpy(pInstance->ActiveKeyPath, (PWCHAR) dwContext);

	if (SDGetRegPathFromInitContext((PWCHAR) dwContext,
					pInstance->RegPath,
					sizeof(pInstance->RegPath)) != ERROR_SUCCESS) {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR: Failed to get reg path\n")));
		LocalFree(pInstance);
		return 0;
	}

	EnterCriticalSection(&LoaderCriticalSection);

	/*  walk through the array and find an open instance number */
	for (ii = 0; ii < MAX_NUMBER_OF_ADAPTERS; ii++) {
		if (AllocatedInstance[ii] == FALSE) {
			/*  mark that it has been allocated */
			AllocatedInstance[ii] = TRUE;
			/*  save off the index */
			pInstance->InstanceNumber = ii;
			break;
		}
	}

	LeaveCriticalSection(&LoaderCriticalSection);

	if (ii >= MAX_NUMBER_OF_ADAPTERS) {
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR: Max instances exceeded\n")));
		LocalFree(pInstance);
		return 0;
	}


	if (!LoadMiniport(pInstance)) {
		LocalFree(pInstance);
		SdDbgPrint(SDCARD_ZONE_ERROR, (TEXT("SDNDISLDR: LoadMiniport fail\n")));
		return 0;
	}

	SdDbgPrint(SDCARD_ZONE_INIT, (TEXT("SDNDISLDR: -NDL_Init\n")));

	return (DWORD) pInstance;
}				/* NDL_Init */

/*----------------------------------------------------------------------------*/
/*!
* \brief The I/O control entry point
*
* \param[in] Handle     the context returned from NDL_Open
* \param[in] IoctlCode  the ioctl code
* \param[in] pInBuf     the input buffer from the user
* \param[in] InBufSize  the length of the input buffer
* \param[in] pOutBuf    the output buffer from the user
* \param[in] InBufSize  the length of the output buffer
* \param[in] pBytesReturned  the size of the transfer
*
* \retval TRUE  Ioctl was handled
*
* \note Not used
*
/*----------------------------------------------------------------------------*/
BOOL
NDL_IOControl(DWORD Handle,
	      DWORD IoctlCode,
	      PBYTE pInBuf, DWORD InBufSize, PBYTE pOutBuf, DWORD OutBufSize, PDWORD pBytesReturned)
{
	return FALSE;

}				/* NDL_IOControl */

/*----------------------------------------------------------------------------*/
/*!
* \brief The open entry point
*
* \param[in] hDeviceContext the device context from NDL_Init
* \param[in] AccessCode     the desired access
* \param[in] ShareMode      the desired share mode
*
* \retval   an open context
*
* \note Not used
*
/*----------------------------------------------------------------------------*/
DWORD NDL_Open(DWORD hDeviceContext, DWORD AccessCode, DWORD ShareMode)
{
	/* just return the instance */
	return hDeviceContext;
}				/* NDL_Open */

/*----------------------------------------------------------------------------*/
/*!
* \brief The power down entry point for the bus driver
*
* \param[in] hDeviceContext the device context from NDL_Init
*
* \retval none
*
* \note performs no actions
*
/*----------------------------------------------------------------------------*/
VOID NDL_PowerDown(DWORD hDeviceContext)
{
	devicePowerDown = TRUE;
}				/* NDL_PowerDown */

/*----------------------------------------------------------------------------*/
/*!
* \brief The power up entry point for the CE file system wrapper
*
* \param[in] hDeviceContext the device context from NDL_Init
*
* \retval none
*
* \note performs no actions
*
/*----------------------------------------------------------------------------*/
VOID NDL_PowerUp(DWORD hDeviceContext)
{
	return;
}				/* NDL_PowerUp */

/*----------------------------------------------------------------------------*/
/*!
* \brief The read entry point
*
* \param[in] hOpenContext   the context from NDL_Open
* \param[in] pBuffer        the user's buffer
* \param[in] Count          the size of the transfer
*
* \retval zero (not implemented)
*
* \note Not used
*
/*----------------------------------------------------------------------------*/
DWORD NDL_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD Count)
{
	return 0;
}				/* NDL_Read */

/*----------------------------------------------------------------------------*/
/*!
* \brief The seek entry point
*
* \param[in] hOpenContext   the context from NDL_Open
* \param[in] Amount         the amount to seek
* \param[in] Type           the type of seek
*
* \retval zero (not implemented)
*
* \note Not used
*
/*----------------------------------------------------------------------------*/
DWORD NDL_Seek(DWORD hOpenContext, LONG Amount, DWORD Type)
{
	return 0;
}				/* NDL_Seek */

/*----------------------------------------------------------------------------*/
/*!
* \brief The write entry point
*
* \param[in] hOpenContext   the context from NDL_Open
* \param[in] pBuffer        the user's buffer
* \param[in] Count          the size of the transfer
*
* \retval zero (not implemented)
*
* \note Not used
*
/*----------------------------------------------------------------------------*/
DWORD NDL_Write(DWORD hOpenContext, LPCVOID pBuffer, DWORD Count)
{
	return 0;
}				/* NDL_Write */

/*----------------------------------------------------------------------------*/
/*!
* \brief The close entry point
*
* \param[in] hOpenContext   the context returned from NDL_Open
*
* \retval TRUE
*
* \note Not used
*
/*----------------------------------------------------------------------------*/
BOOL NDL_Close(DWORD hOpenContext)
{
	return TRUE;
}				/* NDL_Close */
