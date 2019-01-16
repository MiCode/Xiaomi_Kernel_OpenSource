#include <linux/delay.h>

#include "sii_hal.h"
#include "sii_hal_priv.h"
void HalTimerWait(uint16_t m_sec)
{
	unsigned long time_usec = m_sec;
	msleep(time_usec);
}
