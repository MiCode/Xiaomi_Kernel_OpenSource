#ifndef _SI_DRVISRCONFIG_H_
#define _SI_DRVISRCONFIG_H_
void SiiMhlTxDeviceIsr(void);
void SiiExtDeviceIsr(void);
#ifdef __KERNEL__
uint8_t SiiCheckDevice(uint8_t dev);
#endif
#define SII_MHL_TX_ISR SiiMhlTxDeviceIsr
#define SII_EXT_ISR SiiExtDeviceIsr
void SiiMhlTxDeviceTimerIsr(uint8_t timerIndex);
#define CALL_SII_MHL_TX_DEVICE_TIMER_ISR(index)
#endif
