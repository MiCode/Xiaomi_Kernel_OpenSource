#ifndef __SSUSB_QMU_H__
#define __SSUSB_QMU_H__

#include <linux/mu3d/hal/mu3d_hal_qmu_drv.h>

#ifdef USE_SSUSB_QMU

#undef EXTERN
#define EXTERN

//Sanity CR check in
void qmu_done_tasklet(unsigned long data);
void qmu_error_recovery(unsigned long data);
void qmu_exception_interrupt(struct musb *musb, DEV_UINT32 wQmuVal);

#endif
#endif
