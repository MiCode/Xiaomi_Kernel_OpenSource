#ifndef __TL_FP_API_H__
#define __TL_FP_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t FP_Result;
typedef uint8_t FP_Boolean;

typedef struct tlApiRecognizeFp_t {
    uint32_t *pIds;
    uint32_t idsLen;

    uint32_t matchFpId;
#ifdef FP_SAVE_RECOGNIZE_TIMESTAMP
    uint64_t recognizeTimestamp;
#endif

    FP_Boolean result;
} tlApiRecognizeFp_t, *tlApiRecognizeFp_ptr;

typedef struct tlApiFpTemplateInfo_t {
    uint32_t id;
    uint32_t lastRecognizedTime;

    uint32_t len;
    uint8_t *pBuf;

    uint8_t fpName[FP_TEMPLATE_NAME_MAX_LEN];
} tlApiFpTemplateInfo_t, *tlApiFpTemplateInfo_ptr;

typedef struct tlApiVerifyFpPsw_t {

    uint8_t psw[FP_PSW_LEN];
} tlApiVerifyFpPsw_t, *tlApiVerifyFpPsw_ptr;

typedef struct tlApiFpIdList_t {

    uint32_t *pIds;
    uint32_t *pIdsCount;
} tlApiFpIdList_t, *tlApiFpIdList_ptr;

typedef struct tlApiGetFpNameById_t {

    uint32_t id;
    uint8_t *pFpName;
    uint32_t *pFpNameLen;
} tlApiGetFpNameById_t, *tlApiGetFpNameById_ptr;

typedef struct tlApiChangeFpNameById_t {

    uint32_t id;

    uint8_t *pFpName;
    uint32_t fpNameLen;
} tlApiChangeFpNameById_t, *tlApiChangeFpNameById_ptr;

typedef struct tlApiChangeFpPsw_t {

    uint8_t oldPsw[FP_PSW_LEN];

    uint8_t newPsw[FP_PSW_LEN];
} tlApiChangeFpPsw_t, *tlApiChangeFpPsw_ptr;

typedef struct tlApiPduBuf_t {

    uint8_t *pBuf;

    /*
     * For KB value, len must be greater KB_PARAMS_LEN; for data key, len must be greater than DATA_KEY_LEN
     **/
    uint32_t *pBufLen;
} tlApiPduBuf_t, *tlApiPduBuf_ptr;

typedef struct tlApiInitParams_t {

    uint32_t spiPort;
    uint32_t tx_dma_addr;
    uint32_t rx_dma_addr;
} tlApiInitParams_t, *tlApiInitParams_ptr;

_TLAPI_EXTERN_C tlApiResult_t tlApiFPInit(tlApiInitParams_t *pInitParams);

_TLAPI_EXTERN_C tlApiResult_t tlApiAddFpTemplate(tlApiFpTemplateInfo_t *pFpTemplateInfo);

_TLAPI_EXTERN_C tlApiResult_t tlApiDelFpTemplate(uint32_t id);

_TLAPI_EXTERN_C tlApiResult_t tlApiReset();

_TLAPI_EXTERN_C tlApiResult_t tlApiSetMode(uint32_t mode);

_TLAPI_EXTERN_C tlApiResult_t tlApiGetMode(uint32_t *pMode);

_TLAPI_EXTERN_C tlApiResult_t tlApiRegisterFp(uint32_t *pPercent);

_TLAPI_EXTERN_C tlApiResult_t tlApiGetRegisterFpData(uint8_t *pBuf, uint32_t *pBufLen);

_TLAPI_EXTERN_C tlApiResult_t tlApiCancelRegister();

_TLAPI_EXTERN_C tlApiResult_t tlApiDriverTest();

_TLAPI_EXTERN_C tlApiResult_t tlApiRecognizeFp(tlApiRecognizeFp_ptr pRecognizeFp);

_TLAPI_EXTERN_C tlApiResult_t tlApiCancelRecognize();

_TLAPI_EXTERN_C tlApiResult_t tlApiVerifyFpPsw(uint8_t psw[FP_PSW_LEN]);

_TLAPI_EXTERN_C tlApiResult_t tlApiChangeFpPsw(uint8_t oldPsw[FP_PSW_LEN],
        uint8_t newPsw[FP_PSW_LEN]);

_TLAPI_EXTERN_C tlApiResult_t tlApiGetKbParams(uint8_t *pBuf, uint32_t len);

_TLAPI_EXTERN_C tlApiResult_t tlApiGetDataKeyParams(uint8_t *pBuf, uint32_t len);

_TLAPI_EXTERN_C tlApiResult_t tlApiInitKbParams(uint8_t *pBuf, uint32_t len);

_TLAPI_EXTERN_C tlApiResult_t tlApiInitDataKeyParams(uint8_t *pBuf, uint32_t len);

_TLAPI_EXTERN_C tlApiResult_t tlApiGetFpIdList(uint32_t *pIds, uint32_t *pIdsCount);

_TLAPI_EXTERN_C tlApiResult_t tlApiGetFpNameById(uint32_t id, int8_t *pFpName,
        uint32_t *pFpNameLen);

_TLAPI_EXTERN_C tlApiResult_t tlApiGetFpIdentifyResultById(int *pId);

_TLAPI_EXTERN_C tlApiResult_t tlApiChangeFpNameById(uint32_t id, int8_t *pFpName,
        uint32_t fpNameLen);

_TLAPI_EXTERN_C tlApiResult_t tlApiSpiEnter();
_TLAPI_EXTERN_C tlApiResult_t tlApiSpiExit();


#ifdef __cplusplus
}
#endif

#endif // __TL_FP_API_H__
