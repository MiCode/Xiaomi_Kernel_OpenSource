#ifndef __SI_CRA_DRV_INTERNAL_H__
#define __SI_CRA_DRV_INTERNAL_H__
typedef enum _SiiDrvCraError_t {
	RESULT_CRA_SUCCESS,
	RESULT_CRA_FAIL,
	RESULT_CRA_INVALID_PARAMETER,
} SiiDrvCraError_t;
typedef struct _CraInstanceData_t {
	int structVersion;
	int instanceIndex;
	SiiDrvCraError_t lastResultCode;
	uint16_t statusFlags;
} CraInstanceData_t;
extern CraInstanceData_t craInstance;
#endif
