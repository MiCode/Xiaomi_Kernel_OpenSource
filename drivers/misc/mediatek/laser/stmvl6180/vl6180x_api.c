/*******************************************************************************
Copyright ?2014, STMicroelectronics International N.V.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STMicroelectronics nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED. 
IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************************/

/*
 * $Date: 2015-05-13 13:56:33 +0200 (Wed, 13 May 2015) $
 * $Revision: 2287 $
 */
#include "vl6180x_api.h"

#define VL6180x_9to7Conv(x) (x)

/* TODO when set all "cached" value with "default init" are updated after init from register read back */
#define REFRESH_CACHED_DATA_AFTER_INIT  1


#define IsValidGPIOFunction(x) ((x)==GPIOx_SELECT_GPIO_INTERRUPT_OUTPUT || (x)==GPIOx_SELECT_OFF)


/** default value ECE factor Molecular */
#define DEF_ECE_FACTOR_M    85
/** default value ECE factor Denominator */
#define DEF_ECE_FACTOR_D    100
/** default value ALS integration time */
#define DEF_INT_PEFRIOD     100
/** default value ALS gain */
#define DEF_ALS_GAIN        1
/** default value ALS scaler */
#define DEF_ALS_SCALER      1
/** default value for DMAX Enbale */
#define DEF_DMAX_ENABLE     1
/** default ambient tuning factor %x1000 */
#define DEF_AMBIENT_TUNING  80

#if VL6180x_SINGLE_DEVICE_DRIVER
extern  struct VL6180xDevData_t SingleVL6180xDevData;
#define VL6180xDevDataGet(dev, field) (SingleVL6180xDevData.field)
#define VL6180xDevDataSet(dev, field, data) SingleVL6180xDevData.field=(data)
#endif

#define LUXRES_FIX_PREC 8
#define GAIN_FIX_PREC    8  /* ! if not sme as LUX_PREC then :( adjust GetLux */
#define AN_GAIN_MULT    (1<<GAIN_FIX_PREC)


static int32_t _GetAveTotalTime(VL6180xDev_t dev);
static int VL6180x_RangeSetEarlyConvergenceEestimateThreshold(VL6180xDev_t dev);

/**
 * ScalerLookUP scaling factor-1 to register #RANGE_SCALER lookup
 */
static const uint16_t ScalerLookUP[]      ROMABLE_DATA ={ 253, 127,  84}; /* lookup table for scaling->scalar 1x2x 3x */
/**
 * scaling factor to Upper limit look up
 */
static const uint16_t UpperLimitLookUP[]  ROMABLE_DATA ={ 185, 370,  580}; /* lookup table for scaling->limit  1x2x3x */
/**
 * Als Code gain to fix point gain lookup
 */
static const uint16_t AlsGainLookUp[8]    ROMABLE_DATA = {
        (uint16_t)(20.0f * AN_GAIN_MULT),
        (uint16_t)(10.0f * AN_GAIN_MULT),
        (uint16_t)(5.0f  * AN_GAIN_MULT),
        (uint16_t)(2.5f  * AN_GAIN_MULT),
        (uint16_t)(1.67f * AN_GAIN_MULT),
        (uint16_t)(1.25f * AN_GAIN_MULT),
        (uint16_t)(1.0f  * AN_GAIN_MULT),
        (uint16_t)(40.0f * AN_GAIN_MULT),
};


#if VL6180x_RANGE_STATUS_ERRSTRING
const char * ROMABLE_DATA VL6180x_RangeStatusErrString[]={
       "No Error",
       "VCSEL Continuity Test",
       "VCSEL Watchdog Test",
       "VCSEL Watchdog",
       "PLL1 Lock",
       "PLL2 Lock",
       "Early Convergence Estimate",
       "Max Convergence",
       "No Target Ignore",
       "Not used 9",
       "Not used 10",
       "Max Signal To Noise Ratio",
       "Raw Ranging Algo Underflow",
       "Raw Ranging Algo Overflow",
       "Ranging Algo Underflow",
       "Ranging Algo Overflow",

       "Filtered by post processing"
};

const char * VL6180x_RangeGetStatusErrString(uint8_t RangeErrCode){
    if( RangeErrCode  > sizeof(VL6180x_RangeStatusErrString)/sizeof(VL6180x_RangeStatusErrString[0]) )
        return NULL;
    return  VL6180x_RangeStatusErrString[RangeErrCode];
}
#endif

#if VL6180x_UPSCALE_SUPPORT == 1
    #define _GetUpscale(dev, ... )  1
    #define _SetUpscale(...) -1
    #define DEF_UPSCALE 1
#elif VL6180x_UPSCALE_SUPPORT == 2
    #define _GetUpscale(dev, ... )  2
    #define _SetUpscale(...)
    #define DEF_UPSCALE 2
#elif  VL6180x_UPSCALE_SUPPORT == 3
    #define _GetUpscale(dev, ... )  3
    #define _SetUpscale(...)
    #define DEF_UPSCALE 3
#else
    #define DEF_UPSCALE (-(VL6180x_UPSCALE_SUPPORT))
    #define _GetUpscale(dev, ... ) VL6180xDevDataGet(dev, UpscaleFactor)
    #define _SetUpscale(dev, Scaling ) VL6180xDevDataSet(dev, UpscaleFactor, Scaling)
#endif


#if VL6180x_SINGLE_DEVICE_DRIVER
/**
 * the unique driver data  When single device driver is active
 */
struct VL6180xDevData_t VL6180x_DEV_DATA_ATTR  SingleVL6180xDevData={
        .EceFactorM        = DEF_ECE_FACTOR_M,
        .EceFactorD        = DEF_ECE_FACTOR_D,
#ifdef VL6180x_HAVE_UPSCALE_DATA
        .UpscaleFactor     = DEF_UPSCALE,
#endif
#ifdef VL6180x_HAVE_ALS_DATA
        .IntegrationPeriod = DEF_INT_PEFRIOD,
        .AlsGainCode       = DEF_ALS_GAIN,
        .AlsScaler         = DEF_ALS_SCALER,
#endif
#ifdef VL6180x_HAVE_DMAX_RANGING
        .DMaxEnable =   DEF_DMAX_ENABLE,
#endif
};
#endif /* VL6180x_SINGLE_DEVICE_DRIVER */



#define Fix7_2_KCPs(x) ((((uint32_t)(x))*1000)>>7)


#if VL6180x_WRAP_AROUND_FILTER_SUPPORT || VL6180x_HAVE_DMAX_RANGING
static int _GetRateResult(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData);
#endif

#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
static int _filter_Init(VL6180xDev_t dev);
static int _filter_GetResult(VL6180xDev_t dev, VL6180x_RangeData_t *pData);
    #define _IsWrapArroundActive(dev) VL6180xDevDataGet(dev,WrapAroundFilterActive)
#else
    #define _IsWrapArroundActive(dev) 0
#endif


#if VL6180x_HAVE_DMAX_RANGING
   void _DMax_OneTimeInit(VL6180xDev_t dev);
   static int _DMax_InitData(VL6180xDev_t dev);
   static int _DMax_Compute(VL6180xDev_t dev, VL6180x_RangeData_t *pRange);
    #define _IsDMaxActive(dev) VL6180xDevDataGet(dev,DMaxEnable)
#else
    #define _DMax_InitData(...) 0 /* success */
    #define _DMax_OneTimeInit(...) (void)0
    #define _IsDMaxActive(...) 0
#endif

static int VL6180x_RangeStaticInit(VL6180xDev_t dev);
static int  VL6180x_UpscaleStaticInit(VL6180xDev_t dev);

int VL6180x_WaitDeviceBooted(VL6180xDev_t dev){
	uint8_t FreshOutReset;
	int status;
	LOG_FUNCTION_START("");
	do{
		status = VL6180x_RdByte(dev,SYSTEM_FRESH_OUT_OF_RESET, &FreshOutReset);
	}
	while( FreshOutReset!=1 && status==0);
	LOG_FUNCTION_END(status);
	return status;
}

int VL6180x_InitData(VL6180xDev_t dev){
    int status, dmax_status ;
    int8_t offset;
    uint8_t FreshOutReset;
    uint32_t CalValue;
    uint16_t u16;
    uint32_t XTalkCompRate_KCps;

    LOG_FUNCTION_START("");

    VL6180xDevDataSet(dev, EceFactorM , DEF_ECE_FACTOR_M);
    VL6180xDevDataSet(dev, EceFactorD , DEF_ECE_FACTOR_D);

#ifdef VL6180x_HAVE_UPSCALE_DATA
    VL6180xDevDataSet(dev, UpscaleFactor ,  DEF_UPSCALE);
#endif

#ifdef VL6180x_HAVE_ALS_DATA
    VL6180xDevDataSet(dev, IntegrationPeriod, DEF_INT_PEFRIOD);
    VL6180xDevDataSet(dev, AlsGainCode, DEF_ALS_GAIN);
    VL6180xDevDataSet(dev, AlsScaler, DEF_ALS_SCALER);
#endif

#ifdef  VL6180x_HAVE_WRAP_AROUND_DATA
    VL6180xDevDataSet(dev, WrapAroundFilterActive, (VL6180x_WRAP_AROUND_FILTER_SUPPORT >0));
    VL6180xDevDataSet(dev, DMaxEnable, DEF_DMAX_ENABLE);
#endif

    _DMax_OneTimeInit(dev);
    do{

        /* backup offset initial value from nvm these must be done prior any over call that use offset */
        status = VL6180x_RdByte(dev,SYSRANGE_PART_TO_PART_RANGE_OFFSET, (uint8_t*)&offset);
        if( status ){
            VL6180x_ErrLog("SYSRANGE_PART_TO_PART_RANGE_OFFSET rd fail");
            break;
        }
        VL6180xDevDataSet(dev, Part2PartOffsetNVM, offset);

        status=VL6180x_RdDWord( dev, SYSRANGE_RANGE_IGNORE_THRESHOLD, &CalValue);
        if( status ){
            VL6180x_ErrLog("Part2PartAmbNVM rd fail");
            break;
        }
        if( (CalValue&0xFFFF0000) == 0 ){
            CalValue=0x00CE03F8;
        }
        VL6180xDevDataSet(dev, Part2PartAmbNVM, CalValue);

        status = VL6180x_RdWord(dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE ,&u16);
        if( status){
            VL6180x_ErrLog("SYSRANGE_CROSSTALK_COMPENSATION_RATE rd fail ");
            break;
        }
        XTalkCompRate_KCps = Fix7_2_KCPs(u16);
        VL6180xDevDataSet(dev, XTalkCompRate_KCps , XTalkCompRate_KCps );

        dmax_status = _DMax_InitData(dev);
        if( dmax_status < 0 ){
            VL6180x_ErrLog("DMax init failure");
            break;
        }

        /* Read or wait for fresh out of reset  */
        status = VL6180x_RdByte(dev,SYSTEM_FRESH_OUT_OF_RESET, &FreshOutReset);
        if( status )  {
            VL6180x_ErrLog("SYSTEM_FRESH_OUT_OF_RESET rd fail");
            break;
        }
        if( FreshOutReset!= 1 || dmax_status )
            status = CALIBRATION_WARNING;

    }
    while(0);

    LOG_FUNCTION_END(status);
    return status;
}

int8_t VL6180x_GetOffsetCalibrationData(VL6180xDev_t dev)
{
    int8_t offset;
    LOG_FUNCTION_START("");
    offset = VL6180xDevDataGet(dev, Part2PartOffsetNVM);
    LOG_FUNCTION_END( offset );
    return offset;
}

void VL6180x_SetOffsetCalibrationData(VL6180xDev_t dev, int8_t offset)
{
    LOG_FUNCTION_START("%d", offset);
    VL6180xDevDataSet(dev, Part2PartOffsetNVM, offset);
    LOG_FUNCTION_END(0);
}

int  VL6180x_SetXTalkCompensationRate(VL6180xDev_t dev, FixPoint97_t Rate)
{
    int status;
    LOG_FUNCTION_START("%d", Rate);
    status = VL6180x_WrWord(dev, SYSRANGE_CROSSTALK_COMPENSATION_RATE, Rate);
    if( status ==0 ){
        uint32_t XTalkCompRate_KCps;
        XTalkCompRate_KCps = Fix7_2_KCPs(Rate);
        VL6180xDevDataSet(dev, XTalkCompRate_KCps , XTalkCompRate_KCps );
        /* update dmax whenever xtalk rate changes */
        status = _DMax_InitData(dev);
    }
    LOG_FUNCTION_END(status);
    return status;
}

void VL6180x_SetOffset(VL6180xDev_t dev, int8_t offset)
{
	int8_t scaling;
	int8_t Offset;
    LOG_FUNCTION_START("%d", offset);
    VL6180xDevDataSet(dev, Part2PartOffsetNVM, offset);
	scaling = _GetUpscale(dev);

    /* Apply scaling on  part-2-part offset */
    Offset = VL6180xDevDataGet(dev, Part2PartOffsetNVM)/scaling;

   	VL6180x_WrByte(dev, SYSRANGE_PART_TO_PART_RANGE_OFFSET, Offset);

    LOG_FUNCTION_END(0);
}

int VL6180x_SetI2CAddress(VL6180xDev_t dev, uint8_t NewAddress){
    int status;
    LOG_FUNCTION_START("");

    status = VL6180x_WrByte(dev, I2C_SLAVE_DEVICE_ADDRESS, NewAddress/2);
    if( status ){
        VL6180x_ErrLog("new i2c addr Wr fail");
    }
    LOG_FUNCTION_END(status);
    return status;
}

uint16_t VL6180x_GetUpperLimit(VL6180xDev_t dev) {
    uint16_t limit;
    int scaling;

    LOG_FUNCTION_START("");

    scaling = _GetUpscale(dev);
    /* FIXME we do assume here _GetUpscale is valid if  user call us prior to init we may overflow the LUT  mem area */
    limit = UpperLimitLookUP[scaling - 1];

    LOG_FUNCTION_END((int )limit);
    return limit;
}



int VL6180x_StaticInit(VL6180xDev_t dev){
    int status=0, init_status;
    LOG_FUNCTION_START("");

    /* TODO doc When using configurable scaling but using 1x as start condition
     * load tunning upscale  or not ??? */
    if( _GetUpscale(dev) == 1 && !(VL6180x_UPSCALE_SUPPORT<0))
    	init_status=VL6180x_RangeStaticInit(dev);
    else
    	init_status=VL6180x_UpscaleStaticInit(dev);

    if( init_status <0 ){
    	VL6180x_ErrLog("StaticInit fail");
    	goto error;
    }
    else if(init_status > 0){
    	VL6180x_ErrLog("StaticInit warning");
    }

#if REFRESH_CACHED_DATA_AFTER_INIT
    /* update cached value after tuning applied */
    do{
#ifdef  VL6180x_HAVE_ALS_DATA
        uint8_t data;
        status=  VL6180x_RdByte(dev, FW_ALS_RESULT_SCALER, &data);
        if( status ) break;
        VL6180xDevDataSet(dev, AlsScaler, data);

        status=  VL6180x_RdByte(dev, SYSALS_ANALOGUE_GAIN, &data);
        if( status ) break;
        VL6180x_AlsSetAnalogueGain(dev, data);
#endif
    }
    while(0);
#endif /* REFRESH_CACHED_DATA_AFTER_INIT */
    if( status < 0 ){
        VL6180x_ErrLog("StaticInit fail");
    }
    if( !status && init_status){
    	status = init_status;
    }
error:
    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_SetGroupParamHold(VL6180xDev_t dev, int Hold)
{
    int status;
    uint8_t value;

    LOG_FUNCTION_START("%d", Hold);
    if( Hold )
        value = 1;
    else
        value = 0;
    status = VL6180x_WrByte(dev, SYSTEM_GROUPED_PARAMETER_HOLD, value);

    LOG_FUNCTION_END(status);
    return status;

}

int VL6180x_Prepare(VL6180xDev_t dev)
{
    int status;
    LOG_FUNCTION_START("");

    do{
        status=VL6180x_StaticInit(dev);
        if( status<0) break;

        /* set range InterruptMode to new sample */
        status=VL6180x_RangeConfigInterrupt(dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY );
        if( status)
            break;

        /* set default threshold */
        status=VL6180x_RangeSetRawThresholds(dev, 10, 200);
        if( status ){
            VL6180x_ErrLog("VL6180x_RangeSetRawThresholds fail");
            break;
        }
#if VL6180x_ALS_SUPPORT
        status =VL6180x_AlsSetIntegrationPeriod(dev, 100);
        if( status ) break;
        status = VL6180x_AlsSetInterMeasurementPeriod(dev,  200);
        if( status ) break;
        status = VL6180x_AlsSetAnalogueGain(dev,  0);
        if( status ) break;
        status = VL6180x_AlsSetThresholds(dev, 0, 0xFF);
        if( status ) break;
        /* set Als InterruptMode to new sample */
        status=VL6180x_AlsConfigInterrupt(dev, CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY);
        if( status ) {
            VL6180x_ErrLog("VL6180x_AlsConfigInterrupt fail");
            break;
        }
#endif
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
        _filter_Init(dev);
#endif
        /* make sure to reset any left previous condition that can hangs first poll */
        status=VL6180x_ClearAllInterrupt(dev);
    }
    while(0);
    LOG_FUNCTION_END(status);

    return status;
}

#if VL6180x_ALS_SUPPORT
int VL6180x_AlsGetLux(VL6180xDev_t dev, lux_t *pLux)
{
    int status;
    uint16_t RawAls;
    uint32_t luxValue = 0;
    uint32_t IntPeriod;
    uint32_t AlsAnGain;
    uint32_t GainFix;
    uint32_t AlsScaler;

#if LUXRES_FIX_PREC !=  GAIN_FIX_PREC
#error "LUXRES_FIX_PREC != GAIN_FIX_PREC  review these code to be correct"
#endif
    const uint32_t LuxResxIntIme =(uint32_t)(0.56f* DEF_INT_PEFRIOD *(1<<LUXRES_FIX_PREC));

    LOG_FUNCTION_START("%p", pLux);

    status = VL6180x_RdWord( dev, RESULT_ALS_VAL, &RawAls);
    if( !status){
        /* wer are yet here at no fix point */
        IntPeriod=VL6180xDevDataGet(dev, IntegrationPeriod);
        AlsScaler=VL6180xDevDataGet(dev, AlsScaler);
        IntPeriod++; /* what stored is real time  ms -1 and it can be 0 for or 0 or 1ms */
        luxValue = (uint32_t)RawAls * LuxResxIntIme; /* max # 16+8bits + 6bit (0.56*100)  */
        luxValue /= IntPeriod;                         /* max # 16+8bits + 6bit 16+8+1 to 9 bit */
        /* between  29 - 21 bit */
        AlsAnGain = VL6180xDevDataGet(dev, AlsGainCode);
        GainFix = AlsGainLookUp[AlsAnGain];
        luxValue = luxValue / (AlsScaler * GainFix);
        *pLux=luxValue;
    }

    LOG_FUNCTION_END_FMT(status, "%x",(int)*pLux);
    return status;
}

int VL6180x_AlsGetMeasurement(VL6180xDev_t dev, VL6180x_AlsData_t *pAlsData)
{
    int status;
    uint8_t ErrStatus;

    LOG_FUNCTION_START("%p", pAlsData);

    status = VL6180x_AlsGetLux(dev, &pAlsData->lux);
    if( !status ){
        status = VL6180x_RdByte(dev, RESULT_ALS_STATUS, & ErrStatus);
        pAlsData->errorStatus = ErrStatus>>4;
    }
    LOG_FUNCTION_END_FMT(status,"%d %d", (int)pAlsData->lux,  (int)pAlsData->errorStatus);

    return status;
}


int VL6180x_AlsPollMeasurement(VL6180xDev_t dev, VL6180x_AlsData_t *pAlsData) {
    int status;
    int ClrStatus;
    uint8_t IntStatus;

    LOG_FUNCTION_START("%p", pAlsData);
#if VL6180X_SAFE_POLLING_ENTER
    /* if device get stopped with left interrupt uncleared , it is required to clear them now or poll for new condition will never occur*/
    status=VL6180x_AlsClearInterrupt(dev);
    if(status){
        VL6180x_ErrLog("VL6180x_AlsClearInterrupt fail");
        goto over;
    }
#endif

    status=VL6180x_AlsSetSystemMode(dev, MODE_START_STOP|MODE_SINGLESHOT);
    if( status){
        VL6180x_ErrLog("VL6180x_AlsSetSystemMode fail");
        goto over;
    }

    /* poll for new sample ready */
    while (1 ) {
        status = VL6180x_AlsGetInterruptStatus(dev, &IntStatus);
        if (status) {
            break;
        }
        if (IntStatus == RES_INT_STAT_GPIO_NEW_SAMPLE_READY) {
            break; /* break on new data (status is 0)  */
        }

        VL6180x_PollDelay(dev);
    };

    if (!status) {
        status = VL6180x_AlsGetMeasurement(dev, pAlsData);
    }

    ClrStatus = VL6180x_AlsClearInterrupt(dev);
    if (ClrStatus) {
        VL6180x_ErrLog("VL6180x_AlsClearInterrupt fail");
        if (!status) {
            status = ClrStatus; /* leave previous if already on error */
        }
    }
over:
    LOG_FUNCTION_END(status);

    return status;
}

int VL6180x_AlsGetInterruptStatus(VL6180xDev_t dev, uint8_t *pIntStatus) {
    int status;
    uint8_t IntStatus;
    LOG_FUNCTION_START("%p", pIntStatus);

    status = VL6180x_RdByte(dev, RESULT_INTERRUPT_STATUS_GPIO, &IntStatus);
    *pIntStatus= (IntStatus>>3)&0x07;

    LOG_FUNCTION_END_FMT(status, "%d", (int)*pIntStatus);
    return status;
}

int VL6180x_AlsWaitDeviceReady(VL6180xDev_t dev, int MaxLoop ){
    int status;
    int  n;
    uint8_t u8;
    LOG_FUNCTION_START("%d", (int)MaxLoop);
    if( MaxLoop<1){
          status=INVALID_PARAMS;
     }
    else{
        for( n=0; n < MaxLoop ; n++){
            status=VL6180x_RdByte(dev, RESULT_ALS_STATUS, &u8);
            if( status)
                break;
            u8 = u8 & ALS_DEVICE_READY_MASK;
            if( u8 )
                break;

        }
        if( !status && !u8 ){
            status = TIME_OUT;
        }
    }
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_AlsSetSystemMode(VL6180xDev_t dev, uint8_t mode)
{
    int status;
    LOG_FUNCTION_START("%d", (int)mode);
    /* FIXME if we are called back to back real fast we are not checking
     * if previous mode "set" got absorbed => bit 0 must be 0 so that wr 1 work */
    if( mode <= 3){
        status=VL6180x_WrByte(dev, SYSALS_START, mode);
    }
    else{
        status = INVALID_PARAMS;
    }
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_AlsConfigInterrupt(VL6180xDev_t dev, uint8_t ConfigGpioInt)
{
    int status;

    if( ConfigGpioInt<= CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY){
        status = VL6180x_UpdateByte(dev, SYSTEM_INTERRUPT_CONFIG_GPIO, (uint8_t)(~CONFIG_GPIO_ALS_MASK), (ConfigGpioInt<<3));
    }
    else{
        VL6180x_ErrLog("Invalid config mode param %d", (int)ConfigGpioInt);
        status = INVALID_PARAMS;
    }
    LOG_FUNCTION_END(status);
    return status;
}



int VL6180x_AlsSetThresholds(VL6180xDev_t dev, uint8_t low, uint8_t high) {
    int status;

    LOG_FUNCTION_START("%d %d", (int )low, (int)high);

    status = VL6180x_WrByte(dev, SYSALS_THRESH_LOW, low);
    if(!status ){
        status = VL6180x_WrByte(dev, SYSALS_THRESH_HIGH, high);
    }

    LOG_FUNCTION_END(status) ;
    return status;
}


int VL6180x_AlsSetAnalogueGain(VL6180xDev_t dev, uint8_t gain) {
    int status;
    uint8_t GainTotal;

    LOG_FUNCTION_START("%d", (int )gain);
    gain&=~0x40;
    if (gain > 7) {
        gain = 7;
    }
    GainTotal = gain|0x40;

    status = VL6180x_WrByte(dev, SYSALS_ANALOGUE_GAIN, GainTotal);
    if( !status){
        VL6180xDevDataSet(dev, AlsGainCode, gain);
    }

    LOG_FUNCTION_END_FMT(status, "%d %d", (int ) gain, (int )GainTotal);
    return status;
}

int VL6180x_AlsSetInterMeasurementPeriod(VL6180xDev_t dev,  uint16_t intermeasurement_period_ms)
{
    int status;

    LOG_FUNCTION_START("%d",(int)intermeasurement_period_ms);
        /* clipping: range is 0-2550ms */
        if (intermeasurement_period_ms >= 255 *10)
                intermeasurement_period_ms = 255 *10;
    status=VL6180x_WrByte(dev, SYSALS_INTERMEASUREMENT_PERIOD, (uint8_t)(intermeasurement_period_ms/10));

    LOG_FUNCTION_END_FMT(status, "%d", (int) intermeasurement_period_ms);
    return status;
}


int VL6180x_AlsSetIntegrationPeriod(VL6180xDev_t dev, uint16_t period_ms)
{
    int status;
    uint16_t SetIntegrationPeriod;

    LOG_FUNCTION_START("%d", (int)period_ms);

    if( period_ms>=1 )
        SetIntegrationPeriod = period_ms - 1;
    else
        SetIntegrationPeriod = period_ms;

    if (SetIntegrationPeriod > 464) {
        SetIntegrationPeriod = 464;
    }
    else if (SetIntegrationPeriod == 255)   {
        SetIntegrationPeriod++; /* can't write 255 since this causes the device to lock out.*/
    }

    status =VL6180x_WrWord(dev, SYSALS_INTEGRATION_PERIOD, SetIntegrationPeriod);
    if( !status ){
        VL6180xDevDataSet(dev, IntegrationPeriod, SetIntegrationPeriod) ;
    }
    LOG_FUNCTION_END_FMT(status, "%d", (int)SetIntegrationPeriod);
    return status;
}

#endif /* HAVE_ALS_SUPPORT */


int VL6180x_RangePollMeasurement(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData)
{
    int status;
    int ClrStatus;
    IntrStatus_t IntStatus;

    LOG_FUNCTION_START("");
    /* start single range measurement */


#if VL6180X_SAFE_POLLING_ENTER
    /* if device get stopped with left interrupt uncleared , it is required to clear them now or poll for new condition will never occur*/
    status=VL6180x_RangeClearInterrupt(dev);
    if(status){
        VL6180x_ErrLog("VL6180x_RangeClearInterrupt fail");
        goto done;
    }
#endif
    /* //![single_shot_snipet] */
    status=VL6180x_RangeSetSystemMode(dev, MODE_START_STOP|MODE_SINGLESHOT);
    if( status ){
        VL6180x_ErrLog("VL6180x_RangeSetSystemMode fail");
        goto done;
    }


    /* poll for new sample ready */
    while(1 ){
        status=VL6180x_RangeGetInterruptStatus(dev, &IntStatus.val);
        if( status ){
            break;
        }
        if( IntStatus.status.Error !=0 ){
          VL6180x_ErrLog("GPIO int Error report %d",(int)IntStatus.val);
          status = RANGE_ERROR;
          break;
        }
        else
        if( IntStatus.status.Range == RES_INT_STAT_GPIO_NEW_SAMPLE_READY){
            break;
        }

        VL6180x_PollDelay(dev);
    }
 /* //![single_shot_snipet] */

    if ( !status ){
        status = VL6180x_RangeGetMeasurement(dev, pRangeData);
    }

    /*  clear range interrupt source */
    ClrStatus = VL6180x_RangeClearInterrupt(dev);
    if( ClrStatus ){
        VL6180x_ErrLog("VL6180x_RangeClearInterrupt fail");
        /*  leave initial status if already in error  */
        if( !status ){
            status=ClrStatus;
        }
    }
done:
    LOG_FUNCTION_END(status);
    return status;
}



int VL6180x_RangeGetMeasurement(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData)
{
    int status;
    uint16_t RawRate;
    uint8_t RawStatus;

    LOG_FUNCTION_START("");

    status = VL6180x_RangeGetResult(dev, &pRangeData->range_mm);
    if( !status ){
        status = VL6180x_RdWord(dev,RESULT_RANGE_SIGNAL_RATE, &RawRate );
        if( !status ){
            pRangeData->signalRate_mcps = VL6180x_9to7Conv(RawRate);
            status = VL6180x_RdByte(dev, RESULT_RANGE_STATUS, &RawStatus);
            if( !status ){
                pRangeData->errorStatus = RawStatus >>4;
            }
            else{
                VL6180x_ErrLog("Rd RESULT_RANGE_STATUS fail");
            }
	#if VL6180x_WRAP_AROUND_FILTER_SUPPORT || VL6180x_HAVE_DMAX_RANGING
            status = _GetRateResult(dev, pRangeData);
            if( status )
            	goto error;
	#endif
    #if VL6180x_WRAP_AROUND_FILTER_SUPPORT
            /* if enabled run filter */
            if( _IsWrapArroundActive(dev) ){
                status=_filter_GetResult(dev, pRangeData);
                if( !status){
                    /* patch the range status and measure if it is filtered */
                    if( pRangeData->range_mm != pRangeData->FilteredData.range_mm) {
                        pRangeData->errorStatus=RangingFiltered;
                        pRangeData->range_mm = pRangeData->FilteredData.range_mm;
                    }
                }
            }
    #endif

#if VL6180x_HAVE_DMAX_RANGING
            if(_IsDMaxActive(dev) ){
                _DMax_Compute(dev, pRangeData);
            }
#endif
        }
        else{
            VL6180x_ErrLog("Rd RESULT_RANGE_SIGNAL_RATE fail");
        }
    }
    else{
        VL6180x_ErrLog("VL6180x_GetRangeResult fail");
    }
error:
    LOG_FUNCTION_END_FMT(status, "%d %d %d", (int)pRangeData->range_mm, (int)pRangeData->signalRate_mcps,  (int)pRangeData->errorStatus) ;
    return status;
}


int VL6180x_RangeGetMeasurementIfReady(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData)
{
    int status;
    IntrStatus_t IntStatus;

    LOG_FUNCTION_START();

    status = VL6180x_RangeGetInterruptStatus(dev, &IntStatus.val);
    if( status ==0 ){
        if( IntStatus.status.Error !=0 ){
            VL6180x_ErrLog("GPIO int Error report %d",(int)IntStatus.val);
            status = RANGE_ERROR;
        }
        else
        if( IntStatus.status.Range == RES_INT_STAT_GPIO_NEW_SAMPLE_READY){
           status = VL6180x_RangeGetMeasurement(dev,pRangeData );
           if( status == 0){
               /*  clear range interrupt source */
               status = VL6180x_RangeClearInterrupt(dev);
               if( status ){
                   VL6180x_ErrLog("VL6180x_RangeClearInterrupt fail");
               }
           }
        }
        else{
            status = NOT_READY;
        }
    }
    else{
        VL6180x_ErrLog("fail to get interrupt status");
    }
    LOG_FUNCTION_END(status) ;
    return status;
}

int VL6180x_FilterSetState(VL6180xDev_t dev, int state){
    int status;
    LOG_FUNCTION_START("%d", state);
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
    VL6180xDevDataSet(dev,WrapAroundFilterActive, state);
    status = 0;
#else
    status =  NOT_SUPPORTED;
#endif
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_FilterGetState(VL6180xDev_t dev){
    int status;
    LOG_FUNCTION_START("");
#if VL6180x_WRAP_AROUND_FILTER_SUPPORT
    status = VL6180xDevDataGet(dev,WrapAroundFilterActive);
#else
    status = 0;
#endif
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_RangeGetResult(VL6180xDev_t dev, int32_t *pRange_mm) {
    int status;
    uint8_t RawRange;
    int32_t Upscale;

    LOG_FUNCTION_START("%p",pRange_mm);

    status = VL6180x_RdByte(dev, RESULT_RANGE_VAL, &RawRange);
    if( !status ){
        Upscale = _GetUpscale(dev);
        *pRange_mm= Upscale*(int32_t)RawRange;
    }
    LOG_FUNCTION_END_FMT(status, "%d", (int)*pRange_mm);
    return status;
}

int VL6180x_RangeSetRawThresholds(VL6180xDev_t dev, uint8_t low, uint8_t high)
{
    int status;
    LOG_FUNCTION_START("%d %d", (int) low, (int)high);
    /* TODO we can optimize here grouping high/low in a word but that's cpu endianness dependent */
    status=VL6180x_WrByte(dev, SYSRANGE_THRESH_HIGH,high);
    if( !status){
        status=VL6180x_WrByte(dev, SYSRANGE_THRESH_LOW, low);
    }

    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_RangeSetThresholds(VL6180xDev_t dev, uint16_t low, uint16_t high, int UseSafeParamHold)
{
    int status;
    int scale;
    LOG_FUNCTION_START("%d %d", (int) low, (int)high);
    scale=_GetUpscale(dev,UpscaleFactor);
    if( low>scale*255 || high >scale*255){
        status = INVALID_PARAMS;
    }
    else{
        do{
            if( UseSafeParamHold ){
                status=VL6180x_SetGroupParamHold(dev, 1);
                if( status )
                    break;
            }
            status=VL6180x_RangeSetRawThresholds(dev, (uint8_t)(low/scale), (uint8_t)(high/scale));
            if( status ){
                VL6180x_ErrLog("VL6180x_RangeSetRawThresholds fail");
            }
            if( UseSafeParamHold ){
                int HoldStatus;
                /* tryt to unset param hold vene if previous fail */
                HoldStatus=VL6180x_SetGroupParamHold(dev, 0);
                if( !status)
                    status=HoldStatus;
            }
        }
        while(0);
    }

    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_RangeGetThresholds(VL6180xDev_t dev, uint16_t *low, uint16_t *high)
{
    int status;
    uint8_t RawLow, RawHigh;
    int scale;

    LOG_FUNCTION_START("%p %p", low , high);

    scale=_GetUpscale(dev,UpscaleFactor);
    do{
        if( high != NULL ){
            status=VL6180x_RdByte(dev, SYSRANGE_THRESH_HIGH,&RawHigh);
            if( status ){
                VL6180x_ErrLog("rd SYSRANGE_THRESH_HIGH fail");
                break;
            }
            *high=(uint16_t)RawHigh*scale;
        }
        if( low != NULL ) {
            status=VL6180x_RdByte(dev, SYSRANGE_THRESH_LOW, &RawLow);
            if( status ){
                VL6180x_ErrLog("rd SYSRANGE_THRESH_LOW fail");
                break;
            }
            *low=(uint16_t)RawLow*scale;
        }
    }
    while(0);
    LOG_FUNCTION_END_FMT(status, "%d %d",(int)*low ,(int)*high);
    return status;
}


int VL6180x_RangeGetInterruptStatus(VL6180xDev_t dev, uint8_t *pIntStatus) {
    int status;
    uint8_t IntStatus;
    LOG_FUNCTION_START("%p", pIntStatus);
    /* FIXME we are grouping "error" with over status the user must check implicitly for it
     * not just new sample or over status , that will nevr show up in case of error*/
    status = VL6180x_RdByte(dev, RESULT_INTERRUPT_STATUS_GPIO, &IntStatus);
    *pIntStatus= IntStatus&0xC7;

    LOG_FUNCTION_END_FMT(status, "%d", (int)*pIntStatus);
    return status;
}


int VL6180x_GetInterruptStatus(VL6180xDev_t dev, uint8_t *IntStatus)
{
    int status;
    LOG_FUNCTION_START("%p" , IntStatus);
    status = VL6180x_RdByte(dev, RESULT_INTERRUPT_STATUS_GPIO, IntStatus);
    LOG_FUNCTION_END_FMT(status, "%d", (int)*IntStatus);
    return status;
}

int VL6180x_ClearInterrupt(VL6180xDev_t dev, uint8_t IntClear )
{
    int status;
    LOG_FUNCTION_START("%d" ,(int)IntClear);
    if( IntClear <= 7 ){
        status=VL6180x_WrByte( dev, SYSTEM_INTERRUPT_CLEAR, IntClear);
    }
    else{
        status = INVALID_PARAMS;
    }
    LOG_FUNCTION_END(status);
    return status;
}


static int VL6180x_RangeStaticInit(VL6180xDev_t dev)
{
    int status;
    LOG_FUNCTION_START("");

    /* REGISTER_TUNING_SR03_270514_CustomerView.txt */
    VL6180x_WrByte( dev, 0x0207, 0x01);
    VL6180x_WrByte( dev, 0x0208, 0x01);
    VL6180x_WrByte( dev, 0x0096, 0x00);
    VL6180x_WrByte( dev, 0x0097, 0xfd);
    VL6180x_WrByte( dev, 0x00e3, 0x00);
    VL6180x_WrByte( dev, 0x00e4, 0x04);
    VL6180x_WrByte( dev, 0x00e5, 0x02);
    VL6180x_WrByte( dev, 0x00e6, 0x01);
    VL6180x_WrByte( dev, 0x00e7, 0x03);
    VL6180x_WrByte( dev, 0x00f5, 0x02);
    VL6180x_WrByte( dev, 0x00d9, 0x05);
    VL6180x_WrByte( dev, 0x00db, 0xce);
    VL6180x_WrByte( dev, 0x00dc, 0x03);
    VL6180x_WrByte( dev, 0x00dd, 0xf8);
    VL6180x_WrByte( dev, 0x009f, 0x00);
    VL6180x_WrByte( dev, 0x00a3, 0x3c);
    VL6180x_WrByte( dev, 0x00b7, 0x00);
    VL6180x_WrByte( dev, 0x00bb, 0x3c);
    VL6180x_WrByte( dev, 0x00b2, 0x09);
    VL6180x_WrByte( dev, 0x00ca, 0x09);
    VL6180x_WrByte( dev, 0x0198, 0x01);
    VL6180x_WrByte( dev, 0x01b0, 0x17);
    VL6180x_WrByte( dev, 0x01ad, 0x00);
    VL6180x_WrByte( dev, 0x00ff, 0x05);
    VL6180x_WrByte( dev, 0x0100, 0x05);
    VL6180x_WrByte( dev, 0x0199, 0x05);
    VL6180x_WrByte( dev, 0x01a6, 0x1b);
    VL6180x_WrByte( dev, 0x01ac, 0x3e);
    VL6180x_WrByte( dev, 0x01a7, 0x1f);
    VL6180x_WrByte( dev, 0x0030, 0x00);

    /* Recommended : Public registers - See data sheet for more detail */
    VL6180x_WrByte( dev, 0x0011, 0x10); /* Enables polling for New Sample ready when measurement completes */
    VL6180x_WrByte( dev, 0x010a, 0x30); /* Set the averaging sample period (compromise between lower noise and increased execution time) */
    VL6180x_WrByte( dev, 0x003f, 0x46); /* Sets the light and dark gain (upper nibble). Dark gain should not be changed.*/
    VL6180x_WrByte( dev, 0x0031, 0xFF); /* sets the # of range measurements after which auto calibration of system is performed */
    VL6180x_WrByte( dev, 0x0040, 0x63); /* Set ALS integration time to 100ms */
    VL6180x_WrByte( dev, 0x002e, 0x01); /* perform a single temperature calibration of the ranging sensor */

    /* Optional: Public registers - See data sheet for more detail */
    VL6180x_WrByte( dev, 0x001b, 0x09); /* Set default ranging inter-measurement period to 100ms */
    VL6180x_WrByte( dev, 0x003e, 0x31); /* Set default ALS inter-measurement period to 500ms */
    VL6180x_WrByte( dev, 0x0014, 0x24); /* Configures interrupt on New sample ready */


    status=VL6180x_RangeSetMaxConvergenceTime(dev, 50); /*  Calculate ece value on initialization (use max conv) */
    LOG_FUNCTION_END(status);

    return status;
}

#if VL6180x_UPSCALE_SUPPORT != 1

static int _UpscaleInitPatch0(VL6180xDev_t dev){
    int status;
    uint32_t CalValue=0;
    CalValue= VL6180xDevDataGet(dev, Part2PartAmbNVM);
    status=VL6180x_WrDWord( dev, 0xDA, CalValue);
    return status;
}

/* only include up-scaling register setting when up-scale support is configured in */
int VL6180x_UpscaleRegInit(VL6180xDev_t dev)
{
    /*  apply REGISTER_TUNING_ER02_100614_CustomerView.txt */
    VL6180x_WrByte( dev, 0x0207, 0x01);
    VL6180x_WrByte( dev, 0x0208, 0x01);
    VL6180x_WrByte( dev, 0x0096, 0x00);
    VL6180x_WrByte( dev, 0x0097, 0x54);
    VL6180x_WrByte( dev, 0x00e3, 0x00);
    VL6180x_WrByte( dev, 0x00e4, 0x04);
    VL6180x_WrByte( dev, 0x00e5, 0x02);
    VL6180x_WrByte( dev, 0x00e6, 0x01);
    VL6180x_WrByte( dev, 0x00e7, 0x03);
    VL6180x_WrByte( dev, 0x00f5, 0x02);
    VL6180x_WrByte( dev, 0x00d9, 0x05);

    _UpscaleInitPatch0(dev);

    VL6180x_WrByte( dev, 0x009f, 0x00);
    VL6180x_WrByte( dev, 0x00a3, 0x28);
    VL6180x_WrByte( dev, 0x00b7, 0x00);
    VL6180x_WrByte( dev, 0x00bb, 0x28);
    VL6180x_WrByte( dev, 0x00b2, 0x09);
    VL6180x_WrByte( dev, 0x00ca, 0x09);
    VL6180x_WrByte( dev, 0x0198, 0x01);
    VL6180x_WrByte( dev, 0x01b0, 0x17);
    VL6180x_WrByte( dev, 0x01ad, 0x00);
    VL6180x_WrByte( dev, 0x00ff, 0x05);
    VL6180x_WrByte( dev, 0x0100, 0x05);
    VL6180x_WrByte( dev, 0x0199, 0x05);
    VL6180x_WrByte( dev, 0x01a6, 0x1b);
    VL6180x_WrByte( dev, 0x01ac, 0x3e);
    VL6180x_WrByte( dev, 0x01a7, 0x1f);
    VL6180x_WrByte( dev, 0x0030, 0x00);
    VL6180x_WrByte( dev, 0x0011, 0x10);
    VL6180x_WrByte( dev, 0x010a, 0x30);
    VL6180x_WrByte( dev, 0x003f, 0x46);
    VL6180x_WrByte( dev, 0x0031, 0xFF);
    VL6180x_WrByte( dev, 0x0040, 0x63);
    VL6180x_WrByte( dev, 0x002e, 0x01);
    VL6180x_WrByte( dev, 0x002c, 0xff);
    VL6180x_WrByte( dev, 0x001b, 0x09);
    VL6180x_WrByte( dev, 0x003e, 0x31);
    VL6180x_WrByte( dev, 0x0014, 0x24);
#if VL6180x_EXTENDED_RANGE
    VL6180x_RangeSetMaxConvergenceTime(dev, 63);
#else
    VL6180x_RangeSetMaxConvergenceTime(dev, 50);
#endif
    return 0;
}
#else
#define VL6180x_UpscaleRegInit(...) -1
#endif

int VL6180x_UpscaleSetScaling(VL6180xDev_t dev, uint8_t scaling)
{
    int status;
    uint16_t Scaler;
    int8_t  Offset;

    LOG_FUNCTION_START("%d",(int) scaling);

#ifdef VL6180x_HAVE_UPSCALE_DATA
    #define min_scaling 1
    #define max_scaling sizeof(ScalerLookUP)/sizeof(ScalerLookUP[0])
#else
     /* we are in fixed config so only allow configured factor */
    #define min_scaling VL6180x_UPSCALE_SUPPORT
    #define max_scaling VL6180x_UPSCALE_SUPPORT
#endif

    if( scaling>=min_scaling  && scaling<= max_scaling ){

        Scaler = ScalerLookUP[scaling-1];
        status = VL6180x_WrWord(dev, RANGE_SCALER, Scaler);
        _SetUpscale(dev, scaling );

        /* Apply scaling on  part-2-part offset */
        Offset = VL6180xDevDataGet(dev, Part2PartOffsetNVM)/scaling;
        status = VL6180x_WrByte(dev, SYSRANGE_PART_TO_PART_RANGE_OFFSET, Offset);
#if ! VL6180x_EXTENDED_RANGE
        if( status ==0 ){
            status = VL6180x_RangeSetEceState(dev, scaling == 1); /* enable ece only at 1x scaling */
        }
        if( status == 0 && !VL6180x_EXTENDED_RANGE && scaling!=1 ){
        	status = NOT_GUARANTEED ;
        }
#endif
    }
    else{
        status = INVALID_PARAMS;
    }
#undef min_scaling
#undef max_scaling
    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_UpscaleGetScaling(VL6180xDev_t dev)
{
    int status;
    LOG_FUNCTION_START("");
    status=_GetUpscale(dev );
    LOG_FUNCTION_END(status);

    return status;
}


static int  VL6180x_UpscaleStaticInit(VL6180xDev_t dev)
{
    /* todo make these a fail macro in case only 1x is suppoted */
    int status;

    LOG_FUNCTION_START("");
    do{
        status=VL6180x_UpscaleRegInit(dev);
        if( status){
            VL6180x_ErrLog("regInit fail");
            break;
        }
#if VL6180x_EXTENDED_RANGE
        status = VL6180x_RangeSetEceState(dev, 0);
        if( status){
            VL6180x_ErrLog("VL6180x_RangeSetEceState fail");
            break;
        }
#endif
    } while(0);
    if( !status){
        /*  must write the scaler at least once to the device to ensure the scaler is in a known state. */
        status=VL6180x_UpscaleSetScaling(dev, _GetUpscale(dev));
        VL6180x_WrByte( dev, 0x016, 0x00); /* change fresh out of set status to 0 */
    }
    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_SetGPIOxPolarity(VL6180xDev_t dev, int pin, int active_high)
{
    int status;
    LOG_FUNCTION_START("%d %d",(int) pin, (int)active_high);

    if( pin ==0  || pin ==1  ){
       uint16_t RegIndex;
       uint8_t  DataSet;
       if( pin==0 )
           RegIndex= SYSTEM_MODE_GPIO0;
       else
           RegIndex= SYSTEM_MODE_GPIO1;

       if (active_high )
           DataSet = GPIOx_POLARITY_SELECT_MASK;
       else
           DataSet = 0;

       status = VL6180x_UpdateByte(dev, RegIndex, (uint8_t)~GPIOx_POLARITY_SELECT_MASK, DataSet);
    }
    else{
        VL6180x_ErrLog("Invalid pin param %d", (int)pin);
        status = INVALID_PARAMS;
    }

    LOG_FUNCTION_END(status);

    return status;
}

int VL6180x_SetGPIOxFunctionality(VL6180xDev_t dev, int pin, uint8_t functionality)
{
    int status;

    LOG_FUNCTION_START("%d %d",(int) pin, (int)functionality);

    if( ((pin ==0)  || (pin ==1))  && IsValidGPIOFunction(functionality)  ){
       uint16_t RegIndex;

       if( pin==0 )
           RegIndex= SYSTEM_MODE_GPIO0;
       else
           RegIndex= SYSTEM_MODE_GPIO1;

       status = VL6180x_UpdateByte(dev, RegIndex, (uint8_t)~GPIOx_FUNCTIONALITY_SELECT_MASK,  functionality<<GPIOx_FUNCTIONALITY_SELECT_SHIFT);
       if( status){
           VL6180x_ErrLog("Update SYSTEM_MODE_GPIO%d fail", (int)pin);
       }
    }
    else{
        VL6180x_ErrLog("Invalid pin %d  or function %d", (int)pin, (int) functionality);
        status = INVALID_PARAMS;
    }

    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_SetupGPIOx(VL6180xDev_t dev, int pin,  uint8_t IntFunction, int  ActiveHigh)
{
    int status;

    LOG_FUNCTION_START("%d %d",(int) pin, (int)IntFunction);

    if( ((pin ==0)  || (pin ==1))  && IsValidGPIOFunction(IntFunction)  ){
       uint16_t RegIndex;
       uint8_t value=0;

       if( pin==0 )
           RegIndex= SYSTEM_MODE_GPIO0;
       else
           RegIndex= SYSTEM_MODE_GPIO1;

       if( ActiveHigh  )
           value|=GPIOx_POLARITY_SELECT_MASK;

       value |=  IntFunction<<GPIOx_FUNCTIONALITY_SELECT_SHIFT;
       status = VL6180x_WrByte(dev, RegIndex, value);
       if( status ){
           VL6180x_ErrLog("SYSTEM_MODE_GPIO%d wr fail", (int)pin-SYSTEM_MODE_GPIO0);
       }
    }
    else{
        VL6180x_ErrLog("Invalid pin %d or function %d", (int)pin, (int) IntFunction);
        status = INVALID_PARAMS;
    }

    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_DisableGPIOxOut(VL6180xDev_t dev, int pin) {
    int status;

    LOG_FUNCTION_START("%d",(int)pin);

    status=VL6180x_SetGPIOxFunctionality(dev, pin, GPIOx_SELECT_OFF);

    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_SetupGPIO1(VL6180xDev_t dev, uint8_t IntFunction, int ActiveHigh)
{
    int status;
    LOG_FUNCTION_START("%d %d",(int)IntFunction, (int)ActiveHigh  );
    status=VL6180x_SetupGPIOx(dev, 1 , IntFunction, ActiveHigh);
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_RangeConfigInterrupt(VL6180xDev_t dev, uint8_t ConfigGpioInt)
{
    int status;

    if( ConfigGpioInt<= CONFIG_GPIO_INTERRUPT_NEW_SAMPLE_READY){
        status = VL6180x_UpdateByte(dev, SYSTEM_INTERRUPT_CONFIG_GPIO, (uint8_t)(~CONFIG_GPIO_RANGE_MASK), ConfigGpioInt);
    }
    else{
        VL6180x_ErrLog("Invalid config mode param %d", (int)ConfigGpioInt);
        status = INVALID_PARAMS;
    }
    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_RangeSetEceFactor(VL6180xDev_t dev, uint16_t  FactorM, uint16_t FactorD){
    int status;
    uint8_t u8;

    LOG_FUNCTION_START("%d %d", (int)FactorM, (int)FactorD );
    do{
        /* D cannot be 0 M must be <=D and >= 0 */
        if( FactorM <= FactorD  && FactorD> 0){
            VL6180xDevDataSet(dev, EceFactorM, FactorM);
            VL6180xDevDataSet(dev, EceFactorD, FactorD);
            /* read and re-apply max conv time to get new ece factor set */
            status = VL6180x_RdByte(dev, SYSRANGE_MAX_CONVERGENCE_TIME, &u8);
            if( status){
               VL6180x_ErrLog("SYSRANGE_MAX_CONVERGENCE_TIME rd fail ");
               break;
            }
            status = VL6180x_RangeSetMaxConvergenceTime(dev, u8);
            if( status <0 ){
                VL6180x_ErrLog("fail to apply time after ece m/d change");
                break;
            }
        }
        else{
            VL6180x_ErrLog("invalid factor %d/%d", (int)FactorM, (int)FactorD );
            status = INVALID_PARAMS;
        }
    }
    while(0);
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_RangeSetEceState(VL6180xDev_t dev, int enable ){
    int status;
    uint8_t or_mask;

    LOG_FUNCTION_START("%d", (int)enable);
    if( enable )
        or_mask = RANGE_CHECK_ECE_ENABLE_MASK;
    else
        or_mask = 0;

    status =VL6180x_UpdateByte(dev, SYSRANGE_RANGE_CHECK_ENABLES, ~RANGE_CHECK_ECE_ENABLE_MASK, or_mask);
    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_RangeSetMaxConvergenceTime(VL6180xDev_t dev, uint8_t  MaxConTime_msec)
{
    int status = 0;
    LOG_FUNCTION_START("%d",(int)MaxConTime_msec);
    do{
        status=VL6180x_WrByte(dev, SYSRANGE_MAX_CONVERGENCE_TIME, MaxConTime_msec);
        if( status ){
            break;
        }
        status=VL6180x_RangeSetEarlyConvergenceEestimateThreshold(dev);
        if( status){
            break;
        }
        status = _DMax_InitData(dev);
    }
    while(0);
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_RangeSetInterMeasPeriod(VL6180xDev_t dev, uint32_t  InterMeasTime_msec){
    uint8_t SetTime;
    int status;

    LOG_FUNCTION_START("%d",(int)InterMeasTime_msec);
    do {
        if( InterMeasTime_msec > 2550 ){
            status = INVALID_PARAMS;
            break;
        }
        /* doc in not 100% clear and confusing about the limit practically all value are OK but 0
         * that can hang device in continuous mode */
        if( InterMeasTime_msec < 10 ) {
            InterMeasTime_msec=10;
        }
        SetTime=(uint8_t)(InterMeasTime_msec/10);
        status=VL6180x_WrByte(dev, SYSRANGE_INTERMEASUREMENT_PERIOD, SetTime);
        if( status ){
            VL6180x_ErrLog("SYSRANGE_INTERMEASUREMENT_PERIOD wr fail");
        }
        else
        if( SetTime != InterMeasTime_msec /10 ) {
            status = MIN_CLIPED;  /* on success change status to clip if it did */
        }
    }while(0);
    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_RangeGetDeviceReady(VL6180xDev_t dev, int * Ready){
    int status;
    uint8_t u8;
    LOG_FUNCTION_START("%p", (int)Ready);
    status=VL6180x_RdByte(dev, RESULT_RANGE_STATUS, &u8);
    if( !status)
        *Ready = u8&RANGE_DEVICE_READY_MASK;
    LOG_FUNCTION_END_FMT(status,"%d", *Ready);
    return status;
}


int VL6180x_RangeWaitDeviceReady(VL6180xDev_t dev, int MaxLoop ){
    int status; /* if user specify an invalid <=0 loop count we'll return error */
    int  n;
    uint8_t u8;
    LOG_FUNCTION_START("%d", (int)MaxLoop);
    if( MaxLoop<1){
        status=INVALID_PARAMS;
    }
    else{
        for( n=0; n < MaxLoop ; n++){
            status=VL6180x_RdByte(dev, RESULT_RANGE_STATUS, &u8);
            if( status)
                break;
            u8 = u8 & RANGE_DEVICE_READY_MASK;
            if( u8 )
                break;

        }
        if( !status && !u8 ){
            status = TIME_OUT;
        }
    }
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_RangeSetSystemMode(VL6180xDev_t dev, uint8_t  mode)
{
    int status;
    LOG_FUNCTION_START("%d", (int)mode);
    /* FIXME we are not checking device is ready via @a VL6180x_RangeWaitDeviceReady
     * so if called back to back real fast we are not checking
     * if previous mode "set" got absorbed => bit 0 must be 0 so that it work
     */
    if( mode <= 3){
        status=VL6180x_WrByte(dev, SYSRANGE_START, mode);
        if( status ){
            VL6180x_ErrLog("SYSRANGE_START wr fail");
        }
    }
    else{
        status = INVALID_PARAMS;
    }
    LOG_FUNCTION_END(status);
    return status;
}


int VL6180x_RangeStartContinuousMode(VL6180xDev_t dev)
{
    int status;
    LOG_FUNCTION_START("");
    status= VL6180x_RangeSetSystemMode(dev, MODE_START_STOP | MODE_CONTINUOUS);
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_RangeStartSingleShot(VL6180xDev_t dev) {
    int status;
    LOG_FUNCTION_START("");
    status = VL6180x_RangeSetSystemMode(dev, MODE_START_STOP|MODE_SINGLESHOT);
    LOG_FUNCTION_END(status);
    return status;
}


static int VL6180x_RangeSetEarlyConvergenceEestimateThreshold(VL6180xDev_t dev)
{
    int status;

    const uint32_t cMicroSecPerMilliSec  = 1000;
    const uint32_t cEceSampleTime_us     = 500;
    uint32_t ece_factor_m          = VL6180xDevDataGet(dev, EceFactorM);
    uint32_t ece_factor_d          = VL6180xDevDataGet(dev, EceFactorD);
    uint32_t convergTime_us;
    uint32_t fineThresh;
    uint32_t eceThresh;
    uint8_t  u8;
    uint32_t maxConv_ms;
    int32_t AveTime;

    LOG_FUNCTION_START("");

    do{
        status = VL6180x_RdByte(dev, SYSRANGE_MAX_CONVERGENCE_TIME, &u8);
        if( status ){
            VL6180x_ErrLog("SYSRANGE_MAX_CONVERGENCE_TIME rd fail");
            break;
        }
        maxConv_ms = u8;
        AveTime = _GetAveTotalTime(dev);
        if( AveTime <0 ){
            status=-1;
            break;
        }

        convergTime_us = maxConv_ms * cMicroSecPerMilliSec - AveTime;
        status = VL6180x_RdDWord(dev, 0xB8, &fineThresh);
        if( status ) {
            VL6180x_ErrLog("reg 0xB8 rd fail");
            break;
        }
        fineThresh*=256;
        eceThresh      = ece_factor_m * cEceSampleTime_us * fineThresh/(convergTime_us * ece_factor_d);

        status=VL6180x_WrWord(dev, SYSRANGE_EARLY_CONVERGENCE_ESTIMATE, (uint16_t)eceThresh);
    }
    while(0);

    LOG_FUNCTION_END(status);
    return status;
}

/*
 * Return >0 = time
 *       <0 1 if fail to get read data from device to compute time
 */
static int32_t _GetAveTotalTime(VL6180xDev_t dev) {
    uint32_t cFwOverhead_us = 24;
    uint32_t cVcpSetupTime_us = 70;
    uint32_t cPLL2_StartupDelay_us = 200;
    uint8_t cMeasMask = 0x07;
    uint32_t Samples;
    uint32_t SamplePeriod;
    uint32_t SingleTime_us;
    int32_t TotalAveTime_us;
    uint8_t u8;
    int status;

    LOG_FUNCTION_START("");

    status = VL6180x_RdByte(dev, 0x109, &u8);
    if (status) {
        VL6180x_ErrLog("rd 0x109 fail");
        return -1;
    }
    Samples = u8 & cMeasMask;
    status = VL6180x_RdByte(dev, READOUT_AVERAGING_SAMPLE_PERIOD, &u8);
    if (status) {
        VL6180x_ErrLog("i2c READOUT_AVERAGING_SAMPLE_PERIOD fail");
        return -1;
    }
    SamplePeriod = u8;
    SingleTime_us = cFwOverhead_us + cVcpSetupTime_us + (SamplePeriod * 10);
    TotalAveTime_us = (Samples + 1) * SingleTime_us + cPLL2_StartupDelay_us;

    LOG_FUNCTION_END(TotalAveTime_us);
    return TotalAveTime_us;
}

#if VL6180x_HAVE_DMAX_RANGING
#define _GetDMaxDataRetSignalAt400mm(dev) VL6180xDevDataGet(dev, DMaxData.retSignalAt400mm)
#else
#define _GetDMaxDataRetSignalAt400mm(dev) 375 // Use a default high value
#endif


#if VL6180x_WRAP_AROUND_FILTER_SUPPORT

#define FILTER_STDDEV_SAMPLES           6
#define MIN_FILTER_STDDEV_SAMPLES       3
#define MIN_FILTER_VALID_STDDEV_SAMPLES 3
#define FILTER_INVALID_DISTANCE     65535

#define _FilterData(field) VL6180xDevDataGet(dev, FilterData.field)
/*
 * One time init
 */
int _filter_Init( VL6180xDev_t dev) {
    int i;
    _FilterData(MeasurementIndex) = 0;

    _FilterData(Default_ZeroVal) = 0;
    _FilterData(Default_VAVGVal) = 0;
    _FilterData(NoDelay_ZeroVal) = 0;
    _FilterData(NoDelay_VAVGVal) = 0;
    _FilterData(Previous_VAVGDiff) = 0;

    _FilterData(StdFilteredReads) = 0;

    for (i = 0; i < FILTER_NBOF_SAMPLES; i++) {
        _FilterData(LastTrueRange)[i] = FILTER_INVALID_DISTANCE;
        _FilterData(LastReturnRates)[i] = 0;
    }
    return 0;
}


static uint32_t _filter_StdDevDamper(uint32_t AmbientRate, uint32_t SignalRate, const uint32_t StdDevLimitLowLight, const uint32_t StdDevLimitLowLightSNR, const uint32_t StdDevLimitHighLight, const uint32_t StdDevLimitHighLightSNR) {
    uint32_t newStdDev;
    uint16_t SNR;

    if (AmbientRate > 0)
        SNR = (uint16_t) ((100 * SignalRate) / AmbientRate);
    else
        SNR = 9999;

    if (SNR >= StdDevLimitLowLightSNR) {
        newStdDev = StdDevLimitLowLight;
    } else {
        if (SNR <= StdDevLimitHighLightSNR)
            newStdDev = StdDevLimitHighLight;
        else {
            newStdDev = (uint32_t) (StdDevLimitHighLight + (SNR - StdDevLimitHighLightSNR) * (int) (StdDevLimitLowLight - StdDevLimitHighLight) / (StdDevLimitLowLightSNR - StdDevLimitHighLightSNR));
        }
    }

    return newStdDev;
}


/*
 * Return <0 on error
 */
static int32_t _filter_Start(VL6180xDev_t dev, uint16_t m_trueRange_mm, uint16_t m_rawRange_mm, uint32_t m_rtnSignalRate, uint32_t m_rtnAmbientRate, uint16_t errorCode) {
    int status;
    uint16_t m_newTrueRange_mm = 0;

    uint16_t i;
    uint16_t bypassFilter = 0;

    uint16_t registerValue;

    uint32_t register32BitsValue1;
    uint32_t register32BitsValue2;

    uint16_t ValidDistance = 0;

    uint16_t WrapAroundFlag = 0;
    uint16_t NoWrapAroundFlag = 0;
    uint16_t NoWrapAroundHighConfidenceFlag = 0;

    uint16_t FlushFilter = 0;
    uint32_t RateChange = 0;

    uint16_t StdDevSamples = 0;
    uint32_t StdDevDistanceSum = 0;
    uint32_t StdDevDistanceMean = 0;
    uint32_t StdDevDistance = 0;
    uint32_t StdDevRateSum = 0;
    uint32_t StdDevRateMean = 0;
    uint32_t StdDevRate = 0;
    uint32_t StdDevLimitWithTargetMove = 0;

    uint32_t VAVGDiff;
    uint32_t IdealVAVGDiff;
    uint32_t MinVAVGDiff;
    uint32_t MaxVAVGDiff;

    /* Filter Parameters */
    static const uint16_t ROMABLE_DATA WrapAroundLowRawRangeLimit = 60;
    static const uint32_t ROMABLE_DATA WrapAroundLowReturnRateLimit_ROM = 800; // Shall be adapted depending on crossTalk
    static const uint16_t ROMABLE_DATA WrapAroundLowRawRangeLimit2 = 165;
    static const uint32_t ROMABLE_DATA WrapAroundLowReturnRateLimit2_ROM = 180; // Shall be adapted depending on crossTalk and device sensitivity

    static const uint32_t ROMABLE_DATA WrapAroundLowReturnRateFilterLimit_ROM = 850; // Shall be adapted depending on crossTalk and device sensitivity
    static const uint16_t ROMABLE_DATA WrapAroundHighRawRangeFilterLimit = 350;
    static const uint32_t ROMABLE_DATA WrapAroundHighReturnRateFilterLimit_ROM = 1400; // Shall be adapted depending on crossTalk and device sensitivity

    static const uint32_t ROMABLE_DATA WrapAroundMaximumAmbientRateFilterLimit = 7500;

    /*  Temporal filter data and flush values */
    static const uint32_t ROMABLE_DATA MinReturnRateFilterFlush = 75;
    static const uint32_t ROMABLE_DATA MaxReturnRateChangeFilterFlush = 50;

    /* STDDEV values and damper values */

    static const uint32_t ROMABLE_DATA StdDevLimitLowLight = 300;
    static const uint32_t ROMABLE_DATA StdDevLimitLowLightSNR = 30; /* 0.3 */
    static const uint32_t ROMABLE_DATA StdDevLimitHighLight = 2500;
    static const uint32_t ROMABLE_DATA StdDevLimitHighLightSNR = 5; /* 0.05 */

    static const uint32_t ROMABLE_DATA StdDevHighConfidenceSNRLimit = 8;

    static const uint32_t ROMABLE_DATA StdDevMovingTargetStdDevLimit = 90000;

    static const uint32_t ROMABLE_DATA StdDevMovingTargetReturnRateLimit = 3500;
    static const uint32_t ROMABLE_DATA StdDevMovingTargetStdDevForReturnRateLimit = 5000;

    static const uint32_t ROMABLE_DATA MAX_VAVGDiff = 1800;

    /* WrapAroundDetection variables */
    static const uint16_t ROMABLE_DATA WrapAroundNoDelayCheckPeriod = 2;
    static const uint16_t ROMABLE_DATA StdFilteredReadsIncrement = 2;
    static const uint16_t ROMABLE_DATA StdMaxFilteredReads = 4;
    
    uint32_t SignalRateDMax;
    uint32_t WrapAroundLowReturnRateLimit; 
    uint32_t WrapAroundLowReturnRateLimit2;
    uint32_t WrapAroundLowReturnRateFilterLimit;
    uint32_t WrapAroundHighReturnRateFilterLimit; 

    uint8_t u8, u8_2;
    uint32_t XTalkCompRate_KCps;
    uint32_t StdDevLimit = 300;
    uint32_t MaxOrInvalidDistance =   255*_GetUpscale(dev);
    /* #define MaxOrInvalidDistance  (uint16_t) (255 * 3) */

    /* Check if distance is Valid or not */
    switch (errorCode) {
    case 0x0C:
        m_trueRange_mm = MaxOrInvalidDistance;
        ValidDistance = 0;
        break;
    case 0x0D:
        m_trueRange_mm = MaxOrInvalidDistance;
        ValidDistance = 1;
        break;
    case 0x0F:
        m_trueRange_mm = MaxOrInvalidDistance;
        ValidDistance = 1;
        break;
    default:
        if (m_rawRange_mm >= MaxOrInvalidDistance) {
            ValidDistance = 0;
        } else {
            ValidDistance = 1;
        }
        break;
    }
    m_newTrueRange_mm = m_trueRange_mm;
    
    XTalkCompRate_KCps = VL6180xDevDataGet(dev, XTalkCompRate_KCps );

    
    //Update signal rate limits depending on crosstalk
    SignalRateDMax = (uint32_t)_GetDMaxDataRetSignalAt400mm(dev) + XTalkCompRate_KCps;
    WrapAroundLowReturnRateLimit = WrapAroundLowReturnRateLimit_ROM  + XTalkCompRate_KCps; 
    WrapAroundLowReturnRateLimit2 = ((WrapAroundLowReturnRateLimit2_ROM * SignalRateDMax) / 312) + XTalkCompRate_KCps;
    WrapAroundLowReturnRateFilterLimit = ((WrapAroundLowReturnRateFilterLimit_ROM * SignalRateDMax) / 312) + XTalkCompRate_KCps;
    WrapAroundHighReturnRateFilterLimit = ((WrapAroundHighReturnRateFilterLimit_ROM * SignalRateDMax) / 312) + XTalkCompRate_KCps; 


    /* Checks on low range data */
    if ((m_rawRange_mm < WrapAroundLowRawRangeLimit) && (m_rtnSignalRate < WrapAroundLowReturnRateLimit)) {
        m_newTrueRange_mm = MaxOrInvalidDistance;
        bypassFilter = 1;
    }
    if ((m_rawRange_mm < WrapAroundLowRawRangeLimit2) && (m_rtnSignalRate < WrapAroundLowReturnRateLimit2)) {
        m_newTrueRange_mm = MaxOrInvalidDistance;
        bypassFilter = 1;
    }

    /* Checks on Ambient rate level */
    if (m_rtnAmbientRate > WrapAroundMaximumAmbientRateFilterLimit) {
        /* Too high ambient rate */
        FlushFilter = 1;
        bypassFilter = 1;
    }
    /*  Checks on Filter flush */
    if (m_rtnSignalRate < MinReturnRateFilterFlush) {
        /* Completely lost target, so flush the filter */
        FlushFilter = 1;
        bypassFilter = 1;
    }
    if (_FilterData(LastReturnRates)[0] != 0) {
        if (m_rtnSignalRate > _FilterData(LastReturnRates)[0])
            RateChange = (100 * (m_rtnSignalRate - _FilterData(LastReturnRates)[0])) / _FilterData(LastReturnRates)[0];
        else
            RateChange = (100 * (_FilterData(LastReturnRates)[0] - m_rtnSignalRate)) / _FilterData(LastReturnRates)[0];
    } else
        RateChange = 0;
    if (RateChange > MaxReturnRateChangeFilterFlush) {
        FlushFilter = 1;
    }
/* TODO optimize filter  using circular buffer */
    if (FlushFilter == 1) {
        _FilterData(MeasurementIndex) = 0;
        for (i = 0; i < FILTER_NBOF_SAMPLES; i++) {
            _FilterData(LastTrueRange)[i] = FILTER_INVALID_DISTANCE;
            _FilterData(LastReturnRates)[i] = 0;
        }
    } else {
        for (i = (uint16_t) (FILTER_NBOF_SAMPLES - 1); i > 0; i--) {
            _FilterData(LastTrueRange)[i] = _FilterData(LastTrueRange)[i - 1];
            _FilterData(LastReturnRates)[i] = _FilterData(LastReturnRates)[i - 1];
        }
    }
    if (ValidDistance == 1)
        _FilterData(LastTrueRange)[0] = m_trueRange_mm;
    else
        _FilterData(LastTrueRange)[0] = FILTER_INVALID_DISTANCE;
    _FilterData(LastReturnRates)[0] = m_rtnSignalRate;

    /* Check if we need to go through the filter or not */
    if (!(((m_rawRange_mm < WrapAroundHighRawRangeFilterLimit) && (m_rtnSignalRate < WrapAroundLowReturnRateFilterLimit)) || ((m_rawRange_mm >= WrapAroundHighRawRangeFilterLimit) && (m_rtnSignalRate < WrapAroundHighReturnRateFilterLimit))))
        bypassFilter = 1;

    /* Check which kind of measurement has been made */
    status = VL6180x_RdByte(dev, 0x01AC, &u8 );
    if( status ){
        VL6180x_ErrLog("0x01AC rd fail");
        goto done_err;
    }
    registerValue =u8;

    /* Read data for filtering */
    status = VL6180x_RdByte(dev, 0x10C, &u8 ); /* read only 8 lsb bits */
    if( status ){
        VL6180x_ErrLog("0x010C rd fail");
        goto done_err;
    }
    register32BitsValue1=u8;
    status = VL6180x_RdByte(dev, 0x0110, &u8); /* read only 8 lsb bits */
    if( status ){
        VL6180x_ErrLog("0x0110 rd fail");
        goto done_err;
    }
    register32BitsValue2 = u8;

    if (registerValue == 0x3E) {
        _FilterData(Default_ZeroVal) = register32BitsValue1;
        _FilterData(Default_VAVGVal) = register32BitsValue2;
    } else {
        _FilterData(NoDelay_ZeroVal) = register32BitsValue1;
        _FilterData(NoDelay_VAVGVal) = register32BitsValue2;
    }

    if (bypassFilter == 1) {
        /* Do not go through the filter */
        if (registerValue != 0x3E) {
            status = VL6180x_WrByte(dev, 0x1AC, 0x3E);
            if( status ){
                VL6180x_ErrLog("0x01AC bypass wr fail");
                goto done_err;
            }
            status = VL6180x_WrByte(dev, 0x0F2, 0x01);
            if( status ){
                VL6180x_ErrLog("0x0F2 bypass wr fail");
                goto done_err;
            }
        }
        /* Set both Default and NoDelay To same value */
        _FilterData(Default_ZeroVal) = register32BitsValue1;
        _FilterData(Default_VAVGVal) = register32BitsValue2;
        _FilterData(NoDelay_ZeroVal) = register32BitsValue1;
        _FilterData(NoDelay_VAVGVal) = register32BitsValue2;
        _FilterData(MeasurementIndex) = 0;

        return m_newTrueRange_mm;
    }

    if (_FilterData(MeasurementIndex) % WrapAroundNoDelayCheckPeriod == 0) {
        u8=0x3C;
		u8_2 = 0x05;
    } else {
        u8=0x3E;
        u8_2 = 0x01;
    }
    status = VL6180x_WrByte(dev, 0x01AC, u8);
    if( status ){
        VL6180x_ErrLog("0x01AC wr fail");
        goto done_err;
    }
    status = VL6180x_WrByte(dev, 0x0F2, u8_2);
    if( status ){
        VL6180x_ErrLog("0x0F2  wr fail");
        goto done_err;
    }


    _FilterData(MeasurementIndex)++;

    /* Computes current VAVGDiff */
    if (_FilterData(Default_VAVGVal) > _FilterData(NoDelay_VAVGVal))
        VAVGDiff = _FilterData(Default_VAVGVal) - _FilterData(NoDelay_VAVGVal);
    else
        VAVGDiff = 0;
    _FilterData(Previous_VAVGDiff) = VAVGDiff;

    /* Check the VAVGDiff */
    if (_FilterData(Default_ZeroVal) > _FilterData(NoDelay_ZeroVal))
        IdealVAVGDiff = _FilterData(Default_ZeroVal) - _FilterData(NoDelay_ZeroVal);
    else
        IdealVAVGDiff = _FilterData(NoDelay_ZeroVal) - _FilterData(Default_ZeroVal);
    if (IdealVAVGDiff > MAX_VAVGDiff)
        MinVAVGDiff = IdealVAVGDiff - MAX_VAVGDiff;
    else
        MinVAVGDiff = 0;
    MaxVAVGDiff = IdealVAVGDiff + MAX_VAVGDiff;
    if (VAVGDiff < MinVAVGDiff || VAVGDiff > MaxVAVGDiff) {
        WrapAroundFlag = 1;
    } else {
        /* Go through filtering check */

        /* StdDevLimit Damper on SNR */
        StdDevLimit = _filter_StdDevDamper(m_rtnAmbientRate, m_rtnSignalRate, StdDevLimitLowLight, StdDevLimitLowLightSNR, StdDevLimitHighLight, StdDevLimitHighLightSNR);

        /* Standard deviations computations */
        StdDevSamples = 0;
        StdDevDistanceSum = 0;
        StdDevDistanceMean = 0;
        StdDevDistance = 0;
        StdDevRateSum = 0;
        StdDevRateMean = 0;
        StdDevRate = 0;
        for (i = 0; (i < FILTER_NBOF_SAMPLES) && (StdDevSamples < FILTER_STDDEV_SAMPLES); i++) {
            if (_FilterData(LastTrueRange)[i] != FILTER_INVALID_DISTANCE) {
                StdDevSamples = (uint16_t) (StdDevSamples + 1);
                StdDevDistanceSum = (uint32_t) (StdDevDistanceSum + _FilterData(LastTrueRange)[i]);
                StdDevRateSum = (uint32_t) (StdDevRateSum + _FilterData(LastReturnRates)[i]);
            }
        }
        if (StdDevSamples > 0) {
            StdDevDistanceMean = (uint32_t) (StdDevDistanceSum / StdDevSamples);
            StdDevRateMean = (uint32_t) (StdDevRateSum / StdDevSamples);
        }
        /* TODO optimize shorten Std dev in aisngle loop computation using sum of x2 - (sum of x)2 */
        StdDevSamples = 0;
        StdDevDistanceSum = 0;
        StdDevRateSum = 0;
        for (i = 0; (i < FILTER_NBOF_SAMPLES) && (StdDevSamples < FILTER_STDDEV_SAMPLES); i++) {
            if (_FilterData(LastTrueRange)[i] != FILTER_INVALID_DISTANCE) {
                StdDevSamples = (uint16_t) (StdDevSamples + 1);
                StdDevDistanceSum = (uint32_t) (StdDevDistanceSum + (int) (_FilterData(LastTrueRange)[i] - StdDevDistanceMean) * (int) (_FilterData(LastTrueRange)[i] - StdDevDistanceMean));
                StdDevRateSum = (uint32_t) (StdDevRateSum + (int) (_FilterData(LastReturnRates)[i] - StdDevRateMean) * (int) (_FilterData(LastReturnRates)[i] - StdDevRateMean));
            }
        }
        if (StdDevSamples >= MIN_FILTER_STDDEV_SAMPLES) {
            StdDevDistance = (uint16_t) (StdDevDistanceSum / StdDevSamples);
            StdDevRate = (uint16_t) (StdDevRateSum / StdDevSamples);
        } else {
            StdDevDistance = 0;
            StdDevRate = 0;
        }

        /* Check Return rate standard deviation */
        if (StdDevRate < StdDevMovingTargetStdDevLimit) {
            if (StdDevSamples < MIN_FILTER_VALID_STDDEV_SAMPLES) {
                m_newTrueRange_mm = MaxOrInvalidDistance;
            } else {
                /* Check distance standard deviation */
                if (StdDevRate < StdDevMovingTargetReturnRateLimit)
                    StdDevLimitWithTargetMove = StdDevLimit + (((StdDevMovingTargetStdDevForReturnRateLimit - StdDevLimit) * StdDevRate) / StdDevMovingTargetReturnRateLimit);
                else
                    StdDevLimitWithTargetMove = StdDevMovingTargetStdDevForReturnRateLimit;

                if ((StdDevDistance * StdDevHighConfidenceSNRLimit) < StdDevLimitWithTargetMove) {
                    NoWrapAroundHighConfidenceFlag = 1;
                } else {
                    if (StdDevDistance < StdDevLimitWithTargetMove) {
                        if (StdDevSamples >= MIN_FILTER_VALID_STDDEV_SAMPLES) {
                            NoWrapAroundFlag = 1;
                        } else {
                            m_newTrueRange_mm = MaxOrInvalidDistance;
                        }
                    } else {
                        WrapAroundFlag = 1;
                    }
                }
            }
        } else {
            WrapAroundFlag = 1;
        }
    }

    if (m_newTrueRange_mm == MaxOrInvalidDistance) {
        if (_FilterData(StdFilteredReads) > 0)
            _FilterData(StdFilteredReads) = (uint16_t) (_FilterData(StdFilteredReads) - 1);
    } else {
        if (WrapAroundFlag == 1) {
            m_newTrueRange_mm = MaxOrInvalidDistance;
            _FilterData(StdFilteredReads) = (uint16_t) (_FilterData(StdFilteredReads) + StdFilteredReadsIncrement);
            if (_FilterData(StdFilteredReads) > StdMaxFilteredReads)
                _FilterData(StdFilteredReads) = StdMaxFilteredReads;
        } else {
            if (NoWrapAroundFlag == 1) {
                if (_FilterData(StdFilteredReads) > 0) {
                    m_newTrueRange_mm = MaxOrInvalidDistance;
                    if (_FilterData(StdFilteredReads) > StdFilteredReadsIncrement)
                        _FilterData(StdFilteredReads) = (uint16_t) (_FilterData(StdFilteredReads) - StdFilteredReadsIncrement);
                    else
                        _FilterData(StdFilteredReads) = 0;
                }
            } else {
                if (NoWrapAroundHighConfidenceFlag == 1) {
                    _FilterData(StdFilteredReads) = 0;
                }
            }
        }
    }

    return m_newTrueRange_mm;
    done_err:
    return -1;

    #undef MaxOrInvalidDistance
}


static int _filter_GetResult(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData) {
    uint32_t m_rawRange_mm = 0;
    int32_t  FilteredRange;
    const uint8_t scaler = _GetUpscale(dev);
    uint8_t u8;
    int status;

    do {
        status = VL6180x_RdByte(dev, RESULT_RANGE_RAW, &u8);
        if (status) {
            VL6180x_ErrLog("RESULT_RANGE_RAW rd fail");
            break;
        }
        m_rawRange_mm = u8;

        FilteredRange = _filter_Start(dev, pRangeData->range_mm, (m_rawRange_mm * scaler), pRangeData->rtnRate, pRangeData->rtnAmbRate, pRangeData->errorStatus);
        if( FilteredRange<0 ){
            status = -1;
            break;
        }
        pRangeData->FilteredData.range_mm= FilteredRange;
        pRangeData->FilteredData.rawRange_mm = m_rawRange_mm * scaler;
    } while (0);
    return status;
}

#undef _FilterData
#undef FILTER_STDDEV_SAMPLES
#undef MIN_FILTER_STDDEV_SAMPLES
#undef MIN_FILTER_VALID_STDDEV_SAMPLES
#undef FILTER_INVALID_DISTANCE

#endif /* VL6180x_WRAP_AROUND_FILTER_SUPPORT */

#ifdef VL6180x_HAVE_RATE_DATA

static int _GetRateResult(VL6180xDev_t dev, VL6180x_RangeData_t *pRangeData) {
    uint32_t m_rtnConvTime = 0;
    uint32_t m_rtnSignalRate = 0;
    uint32_t m_rtnAmbientRate = 0;
    uint32_t m_rtnSignalCount = 0;
    uint32_t m_rtnAmbientCount = 0;
    uint32_t m_refConvTime = 0;
    uint32_t cRtnSignalCountMax = 0x7FFFFFFF;
    uint32_t cDllPeriods = 6;
    uint32_t calcConvTime = 0;

    int status;

    do {

        status = VL6180x_RdDWord(dev, RESULT_RANGE_RETURN_SIGNAL_COUNT, &m_rtnSignalCount);
        if (status) {
            VL6180x_ErrLog("RESULT_RANGE_RETURN_SIGNAL_COUNT rd fail");
            break;
        }
        if (m_rtnSignalCount > cRtnSignalCountMax) {
            m_rtnSignalCount = 0;
        }

        status = VL6180x_RdDWord(dev, RESULT_RANGE_RETURN_AMB_COUNT, &m_rtnAmbientCount);
        if (status) {
            VL6180x_ErrLog("RESULT_RANGE_RETURN_AMB_COUNTrd fail");
            break;
        }


        status = VL6180x_RdDWord(dev, RESULT_RANGE_RETURN_CONV_TIME, &m_rtnConvTime);
        if (status) {
            VL6180x_ErrLog("RESULT_RANGE_RETURN_CONV_TIME rd fail");
            break;
        }

        status = VL6180x_RdDWord(dev, RESULT_RANGE_REFERENCE_CONV_TIME, &m_refConvTime);
        if (status) {
            VL6180x_ErrLog("RESULT_RANGE_REFERENCE_CONV_TIME rd fail");
            break;
        }

        pRangeData->rtnConvTime = m_rtnConvTime;
        pRangeData->refConvTime = m_refConvTime;

        calcConvTime = m_refConvTime;
        if (m_rtnConvTime > m_refConvTime) {
            calcConvTime = m_rtnConvTime;
        }
        if (calcConvTime == 0)
            calcConvTime = 63000;

        m_rtnSignalRate = (m_rtnSignalCount * 1000) / calcConvTime;
        m_rtnAmbientRate = (m_rtnAmbientCount * cDllPeriods * 1000) / calcConvTime;

        pRangeData->rtnRate = m_rtnSignalRate;
        pRangeData->rtnAmbRate = m_rtnAmbientRate;


    } while (0);
    return status;
}
#endif /* VL6180x_HAVE_RATE_DATA */


int VL6180x_DMaxSetState(VL6180xDev_t dev, int state){
    int status;
    LOG_FUNCTION_START("%d", state);
#if VL6180x_HAVE_DMAX_RANGING
    VL6180xDevDataSet(dev,DMaxEnable, state);
    if( state ){
        status = _DMax_InitData(dev);
    }
    else {
        status = 0;
    }
#else
    status =  NOT_SUPPORTED;
#endif
    LOG_FUNCTION_END(status);
    return status;
}

int VL6180x_DMaxGetState(VL6180xDev_t dev){
    int status;
    LOG_FUNCTION_START("");
#if VL6180x_HAVE_DMAX_RANGING
    status = VL6180xDevDataGet(dev,DMaxEnable);
#else
    status = 0;
#endif
    LOG_FUNCTION_END(status);
    return status;
}


#if VL6180x_HAVE_DMAX_RANGING

#define _DMaxData(field) VL6180xDevDataGet(dev, DMaxData.field)
/*
 * Convert fix point  x.7 to KCpount per sec
 */

#ifndef VL6180x_PLATFORM_PROVIDE_SQRT

/*
 * 32 bit integer square root with not so bad precision (integer result) and is quite fast
 * see http://en.wikipedia.org/wiki/Methods_of_computing_square_roots
 */
uint32_t VL6180x_SqrtUint32(uint32_t num) {
    uint32_t res = 0;
    uint32_t bit = 1 << 30; /* The second-to-top bit is set: 1 << 30 for 32 bits */

    /* "bit" starts at the highest power of four <= the argument. */
    while (bit > num)
        bit >>= 2;

    while (bit != 0) {
        if (num >= res + bit) {
            num -= res + bit;
            res = (res >> 1) + bit;
        }
        else
            res >>= 1;
        bit >>= 2;
    }
    return res;
}
#endif


/* DMax one time init */
void _DMax_OneTimeInit(VL6180xDev_t dev){
    _DMaxData(ambTuningWindowFactor_K)=DEF_AMBIENT_TUNING;
}


static uint32_t _DMax_RawValueAtRateKCps(VL6180xDev_t dev, int32_t rate){
    uint32_t snrLimit_K;
    int32_t DMaxSq;
    uint32_t RawDMax;
    DMaxFix_t retSignalAt400mm;
    uint32_t ambTuningWindowFactor_K;


    ambTuningWindowFactor_K = _DMaxData(ambTuningWindowFactor_K);
    snrLimit_K              = _DMaxData(snrLimit_K);
    retSignalAt400mm        = _DMaxData(retSignalAt400mm); /* 12 to 18 bits Kcps */
    if( rate > 0 ){
        DMaxSq = 400*400*1000 / rate -(400*400/330); /* K of (1/RtnAmb -1/330 )=> 30bit- (12-18)bit  => 12-18 bits*/
        if( DMaxSq<= 0){
            RawDMax = 0;
        }
        else{
            /* value can be more 32 bit so base on raneg apply *retSignalAt400mm before or after division to presevr accuracy */
            if( DMaxSq< (2<<12)  ){
                DMaxSq = DMaxSq*retSignalAt400mm/(snrLimit_K+ambTuningWindowFactor_K);       /* max 12 + 12 to 18 -10 => 12-26 bit */
            }else{
                DMaxSq = DMaxSq/(snrLimit_K+ambTuningWindowFactor_K)*retSignalAt400mm;       /* 12 to 18 -10 + 12 to 18 *=> 12-26 bit */
            }
            RawDMax=VL6180x_SqrtUint32(DMaxSq);
        }
    }
    else{
        RawDMax = 0x7FFFFFFF; /* bigest possibmle 32bit signed value */
    }
    return RawDMax;
}

/*
 * fetch static data from register to avoid re-read
 * precompute all intermediate constant and cliipings
 *
 * to be re-used/call on  changes of :
 *  0x2A
 *  SYSRANGE_MAX_AMBIENT_LEVEL_MULT
 *  Dev Data XtalkComRate_KCPs
 *  SYSRANGE_MAX_CONVERGENCE_TIME
 *  SYSRANGE_RANGE_CHECK_ENABLES    mask RANGE_CHECK_RANGE_ENABLE_MASK
 *  range 0xb8-0xbb (0xbb)
 */
static int _DMax_InitData(VL6180xDev_t dev){
    int status, warning;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint32_t Reg2A_KCps;
    uint32_t RegB8;
    uint8_t  MaxConvTime;
    uint32_t XTalkCompRate_KCps;
    uint32_t RangeIgnoreThreshold;
    int32_t minSignalNeeded;
    uint8_t SysRangeCheckEn;
    uint8_t snrLimit;
    warning=0;

    static const int ROMABLE_DATA MaxConvTimeAdjust=-4;

    LOG_FUNCTION_START("");
    do{
        status = VL6180x_RdByte(dev, 0x02A ,&u8);
        if( status ){
            VL6180x_ErrLog("Reg 0x02A rd fail");
            break;
        }

        if( u8 == 0 ) {
            warning = CALIBRATION_WARNING;
            u8 = 40; /* use a default average value */
        }
        Reg2A_KCps = Fix7_2_KCPs(u8); /* convert to KCPs */

        status = VL6180x_RdByte(dev, SYSRANGE_RANGE_CHECK_ENABLES, &SysRangeCheckEn);
        if (status) {
            VL6180x_ErrLog("SYSRANGE_RANGE_CHECK_ENABLES rd fail ");
            break;
        }

        status = VL6180x_RdByte(dev, SYSRANGE_MAX_CONVERGENCE_TIME, &MaxConvTime);
        if( status){
            VL6180x_ErrLog("SYSRANGE_MAX_CONVERGENCE_TIME rd fail ");
            break;
        }

        status = VL6180x_RdDWord(dev, 0x0B8, &RegB8);
        if( status ){
            VL6180x_ErrLog("reg 0x0B8 rd fail ");
            break;
        }

        status = VL6180x_RdByte(dev, SYSRANGE_MAX_AMBIENT_LEVEL_MULT, &snrLimit);
        if( status){
            VL6180x_ErrLog("SYSRANGE_MAX_AMBIENT_LEVEL_MULT rd fail ");
            break;
        }
        _DMaxData(snrLimit_K) = (int32_t)16*1000/snrLimit;
        XTalkCompRate_KCps =   VL6180xDevDataGet(dev, XTalkCompRate_KCps );

        if( Reg2A_KCps >= XTalkCompRate_KCps){
            _DMaxData(retSignalAt400mm)=( Reg2A_KCps - XTalkCompRate_KCps);
        }
        else{
            _DMaxData(retSignalAt400mm)=0;             /* Reg2A_K - XTalkCompRate_KCp <0 is invalid */
        }

        /* if xtalk range check is off omit it in snr clipping */
        if( SysRangeCheckEn&RANGE_CHECK_RANGE_ENABLE_MASK ){
            status = VL6180x_RdWord(dev, SYSRANGE_RANGE_IGNORE_THRESHOLD, &u16);
            if( status){
                VL6180x_ErrLog("SYSRANGE_RANGE_IGNORE_THRESHOLD rd fail ");
                break;
            }
            RangeIgnoreThreshold = Fix7_2_KCPs(u16);
        }
        else{
            RangeIgnoreThreshold  = 0;
        }

        minSignalNeeded = (RegB8*256)/((int32_t)MaxConvTime+(int32_t)MaxConvTimeAdjust); /* KCps 8+8 bit -(1 to 6 bit) => 15-10 bit */
        /* minSignalNeeded = max ( minSignalNeeded,  RangeIgnoreThreshold - XTalkCompRate_KCps) */
        if( minSignalNeeded  <= RangeIgnoreThreshold - XTalkCompRate_KCps )
            minSignalNeeded  =  RangeIgnoreThreshold - XTalkCompRate_KCps;

        u32 = (minSignalNeeded*(uint32_t)snrLimit)/16;
        _DMaxData(ClipSnrLimit ) = _DMax_RawValueAtRateKCps(dev, u32 ); /* clip to dmax to min signal snr limit rate*/
    }
    while(0);
    if( !status )
        status = warning;
    LOG_FUNCTION_END(status);
    return status;
}

static int _DMax_Compute(VL6180xDev_t dev, VL6180x_RangeData_t *pRange){
    uint32_t rtnAmbRate;
    int32_t DMax;
    int scaling;
    uint16_t HwLimitAtScale;
    static const int ROMABLE_DATA rtnAmbLowLimit_KCps=330*1000;

    rtnAmbRate = pRange->rtnAmbRate;
    if( rtnAmbRate  < rtnAmbLowLimit_KCps ){
        DMax = _DMax_RawValueAtRateKCps( dev, rtnAmbRate);
        scaling = _GetUpscale(dev);
        HwLimitAtScale=UpperLimitLookUP[scaling - 1];

        if( DMax > _DMaxData(ClipSnrLimit) ){
            DMax=_DMaxData(ClipSnrLimit);
        }
        if( DMax > HwLimitAtScale ){
            DMax=HwLimitAtScale;
        }
        pRange->DMax=DMax;
    }
    else{
        pRange->DMax = 0;
    }
    return 0;
}

#undef _DMaxData
#undef Fix7_2_KCPs

#endif /* VL6180x_HAVE_DMAX_RANGING */



