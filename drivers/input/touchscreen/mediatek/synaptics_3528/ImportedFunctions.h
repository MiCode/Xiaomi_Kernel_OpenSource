// ImportedFunctions.h
//
// $Id: ImportedFunctions.h,v 1.24 2012/05/11 07:12:22 ltu Exp $

#ifdef IMPORTEDFUNCTIONS_C
  #define EXTERN
#else
  #define EXTERN extern
#endif

#ifdef _cplusplus
extern "c" {
#endif
#ifndef IMPORTEDFUNCTIONS_H
#define IMPORTEDFUNCTIONS_H
//error codes returned by the API functions
//#include <windows.h>
#include "cdci/CdciErrors.h"
#include "cdci/CrocodileTypes.h"
#include "cdci/CdciInterfaces.h"
#include "cdci/DllVersion.h"

EXTERN bool ControlBridgeRMI(bool bLoadCdciApi);
EXTERN void UnloadLibrary();

typedef void CCdciApi;

/* 
    Line Callback procedure type for receving state changes for an IO line
    \params
        context [in] - a pointer to a context that the user wants to receive back 
                        when the callback procedure gets invoked                    
        sName[out] - a pointer to string name of the IO line
        bState[out] - state of the IO line
    Note: a callback procedure of this type with get invoked from a different thread
          than your main thread. Do not do anything blocking in this callback procedure.
*/
typedef void ( __cdecl   * LineCallback_T)(void         * context,
                                           const char   * sName,
                                           bool           bState,
                                           EError         error,
                                           const char   * pErrorMsg);

typedef void ( __stdcall * StdLineCallback_T)(void        * context,
                                              const char  * sName,
                                              bool          bState,
                                              EError        error,
                                              const char  * pErrorMsg);

typedef void ( __cdecl * PacketRxCallback_T)(void           * context,
                                             unsigned int     uCount, 
                                             unsigned char  * pData, 
                                             EError           error, 
                                             const char     * pErrorMsg);

// Added by Chenchu
typedef CCdciApi * (*InitializeFunc) ();
typedef EError (*UninitializeFunc) (CCdciApi *   ulHandle);
typedef CCdciStreamInterface * (*CdciStream_CreateByCdciTypeFunc)(int iConnectionType);
typedef EError (*CDCI_CreateByCdciTypeFunc)(int iConnectionType, CCdciApi ** ppCDCIApi);
typedef void                   (*CdciStream_DestroyFunc)(CCdciStreamInterface *pMpcUsb);
typedef CCdciApi *             (*CreateFromStreamFunc)(CCdciStreamInterface * pStream);
typedef void                   (*CDCI_DestroyFunc)(CCdciApi * pCDCIApi);

typedef EError (*ConnectFunc) (CCdciApi *   ulHandle);
typedef EError (*DisconnectFunc) (CCdciApi *   ulHandle);
typedef EError (*CreateBySerialNumberFunc) (const char * sSerialNumber, CCdciApi *&ulHandle);
typedef bool(*IsConnectedFunc)(CCdciApi * pCDCIApi);

typedef EError (*ConfigI2CRegExFunc) (CCdciApi *    ulHandle,  
                                 unsigned short   target,
                                 ERmiAddress      RmiAddressingMode,
                                 EPullups         pull_ups,  
                                 EI2CSpeed        i2cSpeed,                             
                                 EAttention       attn /* = ENone*/,
                                 unsigned long    dwTimeout /* = INFINITE*/);

typedef EError (*ConfigI2CRegFunc) (CCdciApi *    ulHandle,  
                                 unsigned short   target,
                                 bool             pull_ups,  
                                 EI2CSpeed        i2cSpeed,                             
                                 EAttention       attn /* = ENone*/,
                                 unsigned long    dwTimeout /* = INFINITE*/);

typedef EError (*ConfigSPIRegFunc) (CCdciApi *    ulHandle,
                                 unsigned short   target,  
                                 ERmiAddress      addressing_mode,
                                 EModuleMode      module_mode,
                                 EPullups         pull_ups, 
                                 ESlaveSelect     slave_select,
                                 unsigned int     byte_delay,
                                 unsigned int     bit_rate,
                                 unsigned int     spi_mode,
                                 EAttention       attn /* = ENone*/,
                                 unsigned long    dwTimeout /* = INFINITE*/);

typedef EError (*ConfigPS2Func) (CCdciApi *   ulHandle,
                              unsigned short  target, 
                              bool            resend_enable,
                              unsigned long   dwTimeout /* = INFINITE*/);

typedef EError (*PowerOnVoltageFunc) (CCdciApi *   ulHandle, 
                                   unsigned short  target, 
                                   unsigned int    vdd_millivolts,
                                   unsigned int    vdevpullup_millivolts,
                                   unsigned int    vled_millivolts,
                                   unsigned long   dwTimeout /* = INFINITE*/);

typedef EError (*PowerOffFunc) (CCdciApi *   ulHandle,
                             unsigned short  target,
                             unsigned long   dwTimeout/*= INFINITE*/);

//Write and Read via handle
typedef EError (*WriteRegister8DataFunc) (CCdciApi *    ulHandle,
                                       unsigned short   target, 
                                       unsigned short   uBusAddress,
                                       ERmiAddress      RmiAddressingMode,
                                       unsigned short   uRmiAddress, 
                                       unsigned char  * data,
                                       unsigned int     length,
                                       unsigned int     &lengthWritten,
                                       unsigned long    dwTimeout );

typedef EError (*ReadRegister8DataFunc) (CCdciApi *   ulHandle,
                                      unsigned short  target,
                                      unsigned short  uBusAddress, //i2c address
                                      ERmiAddress     RmiAddressingMode,
                                      unsigned short  uRmiAddress, //register address
                                      unsigned char * data,
                                      unsigned int    length, 
                                      unsigned int    &lengthRead,
                                      unsigned long   dwTimeout /* = INFINITE*/);

typedef EError (*PacketSetRxCallbackFunc) (CCdciApi *    ulHandle,
                                        PacketRxCallback_T,
                                        void           * context);

typedef EError (*PacketTxFunc) (CCdciApi *   ulHandle,
                             unsigned short  target,
                             unsigned char * pData,
                             unsigned int    uCount,
                             unsigned long   dwTimeout /*= INFINITE*/ );

typedef EError (*Line_SetCallbackFunc) (CCdciApi *         ulHandle, 
                                        StdLineCallback_T, 
                                        void              * context);

typedef void (*GetLastErrorMsgStringFunc)(CCdciApi * pCDCIApi, char *strValue);

typedef ICdciRmiTransaction * (*RmiTransactionNewFunc) (CCdciApi * pCDCIApi);
typedef void (*RmiTransactionDeleteFunc) (CCdciApi * pCDCIApi, ICdciRmiTransaction * pTrans);

typedef EError (*GetControllerInfoFunc)(CCdciApi * pCDCIApi, unsigned short &Id,              //! Controller ID
    unsigned int &FWVersionMajor,     //! Controller Major Version
    unsigned int &FWVersionMinor,     //! Controller Minor Version
    unsigned int &CDCIVersionMajor,  //! CDCI Major Version
    unsigned int &CDCIVersionMinor, //! CDCI Minor Version
    char *strInfo,
    unsigned int &uBoardNumber,
    unsigned int &uSerialNumber);

//! Configures a controller for Rmi over SMBus communication
typedef EError (*ConfigRmiOverSMBusFunc) (CCdciApi *    ulHandle,
                                          unsigned short  target,
                                          EPullups        Pullups,
                                          EI2CSpeed       i2cSpeed, 
                                          DWORD           dwTimeout /*= INFINITE*/);

typedef EError (*ConfigHidOverI2CFunc) (CCdciApi *      pCDCIApi,
                                        unsigned short  target,
                                        ERmiAddress     rmiAddressingMode,
                                        EPullups        pullups,
                                        EPullups        attnPullups,
                                        EI2CSpeed       i2cSpeed, 
                                        EAttention      attn/* = EAttnNone*/,
                                        DWORD           dwTimeout/* = INFINITE*/);

typedef EError (*ConfigHidOverUsbFunc) (CCdciApi * pCDCIApi,
                                        EAttention attn,
                                        DWORD dwTimeout);

typedef EError (*ConfigRmiOverSMBusAddressFunc) ( CCdciApi *    ulHandle,
                                                  unsigned short  target,
                                                  EPullups        Pullups,
                                                  EI2CSpeed       i2cSpeed, 
                                                  unsigned short  i2cHostAddress,
                                                  DWORD           dwTimeout /*= INFINITE*/);

typedef EError (*ConfigRmiNativeFunc) ( CCdciApi *      ulHandle,
                                        unsigned short  target,
                                        EAttention      attn,
                                        DWORD           dwTimeout /*= INFINITE*/);

typedef EError (*CreateByCdciTypeFunc) (int iConnectionType, CCdciApi ** ppCDCIApi);

typedef EError (*SetStreamOptionFunc) (CCdciApi *       ulHandle, 
                                       int              iOptionID, 
                                       const char *     psValue);

typedef EError (*SetTargetOptionAsUInteger32Func) ( CCdciApi * ulHandle,
                                                    unsigned short target,
                                                    int iOption,
                                                    unsigned int uValue,
                                                    DWORD dwTimeout /* = INFINITE*/);

//! constructor for CdciApi 
// By creating a Cdci object, a connection over USB to the MC02 controller is created
bool ControlBridgeRMI(bool bLoadCdciApi);
void Line_FlushAttention();
bool Line_AttentionIsAsserted();
EError Line_WaitForAttentionAsserted(DWORD dwMilliseconds);
EError Line_WaitForAttentionDeasserted(DWORD dwMilliseconds);
EError Line_WaitForAttention(DWORD dwMilliseconds);

void UnloadLibrary();
const int DefaultTimeout = 3000; // In milliseconds
const unsigned short DefaultTarget = 0;
#endif

EXTERN HMODULE g_hDll;
EXTERN HANDLE  g_hMutex;                    //FIXME //mutex for creating stream by serialnumber when multi-threads
EXTERN HANDLE  g_hLoadHandleMutex;
EXTERN HANDLE  m_hAttenEvent;
#ifndef REFLASH_DLL
  EXTERN int g_DllCount;
#endif


EXTERN CdciStream_CreateByCdciTypeFunc  g_CdciStream_CreateByCdciType;
EXTERN CDCI_CreateByCdciTypeFunc        g_CdciCreateByCdciType;
EXTERN CdciStream_DestroyFunc           g_CdciStream_Destroy;
EXTERN CreateFromStreamFunc             CreateFromStream;
EXTERN CDCI_DestroyFunc                 Destroy;
EXTERN InitializeFunc                   InitializeDevice;
EXTERN UninitializeFunc                 UninitializeDevice;
EXTERN ConnectFunc                      ConnectDevice;
EXTERN DisconnectFunc                   DisconnectDevice;
EXTERN IsConnectedFunc                  IsConnected;
EXTERN CreateBySerialNumberFunc	        CreateBySerialNumber;
EXTERN ConfigI2CRegExFunc               ConfigI2CReg;
EXTERN ConfigSPIRegFunc                 ConfigSPIReg;
EXTERN ConfigPS2Func                    ConfigPS2;
EXTERN PowerOnVoltageFunc               PowerOnDevice;
EXTERN PowerOffFunc                     PowerOffDevice;
EXTERN WriteRegister8DataFunc           WriteRegister8;
EXTERN ReadRegister8DataFunc            ReadRegister8;
EXTERN PacketSetRxCallbackFunc          PacketSetRxCallback;
EXTERN PacketTxFunc                     PacketTx;
EXTERN Line_SetCallbackFunc             Line_SetCallback;
EXTERN RmiTransactionNewFunc            RmiTransactionNew;
EXTERN RmiTransactionDeleteFunc         RmiTransactionDelete;
EXTERN GetLastErrorMsgStringFunc        GetLastErrorMsgString;
EXTERN PFNCDCI_GetDllVersion            GetCDCIApiDllVersion;
EXTERN ConfigRmiOverSMBusFunc           ConfigRmiOverSMBus;
EXTERN GetControllerInfoFunc            GetControllerInfo;
EXTERN ConfigRmiNativeFunc              ConfigRmiNative;
EXTERN CreateByCdciTypeFunc             CreateByCdciType;
EXTERN SetStreamOptionFunc              SetStreamOption;
EXTERN SetTargetOptionAsUInteger32Func  SetTargetOption;
EXTERN ConfigHidOverI2CFunc             ConfigHidOverI2C;
EXTERN ConfigHidOverUsbFunc             ConfigHidOverUsb;

#ifdef _cplusplus
}
#endif
