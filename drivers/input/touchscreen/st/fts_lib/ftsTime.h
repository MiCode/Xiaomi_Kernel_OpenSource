/*

**************************************************************************
**						STMicroelectronics						**
**************************************************************************
**						marco.cali@st.com				**
**************************************************************************
*																		*
*				  FTS Utility for mesuring/handling the time		 *
*																		*
**************************************************************************
**************************************************************************

*/

#include "ftsCrossCompile.h"

#include <linux/time.h>

typedef struct {
	struct timespec start, end;
} StopWatch;

void startStopWatch(StopWatch *w);
void stopStopWatch(StopWatch *w);
int elapsedMillisecond(StopWatch *w);
int elapsedNanosecond(StopWatch *w);
char *timestamp(void);
void stdelay(unsigned long ms);
