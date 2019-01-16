#include "tpd.h"

/* #ifndef TPD_CUSTOM_TREMBLE_TOLERANCE */
int tpd_trembling_tolerance(int t, int p)
{
	if (t > 5 || p > 120)
		return 200;
	if (p > 90)
		return 64;
	if (p > 80)
		return 36;
	return 26;
}

/* #endif */
