#ifndef _PIXART_OTS_H_
#define _PIXART_OTS_H_

#include  "pixart_platform.h"

/* export funtions */
bool OTS_Sensor_Init(void);
void OTS_Sensor_ReadMotion(int16_t *dx, int16_t *dy);

#endif
