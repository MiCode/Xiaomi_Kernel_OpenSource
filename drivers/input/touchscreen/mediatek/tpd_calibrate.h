#ifndef TPD_CALIBRATE_H
#define TPD_CALIBRATE_H
#ifdef TPD_HAVE_CALIBRATION

#ifndef TPD_CUSTOM_CALIBRATION

extern int tpd_calmat[8];
extern int tpd_def_calmat[8];

#endif

void tpd_calibrate(int *x, int *y);
#else

#define tpd_calibrate(x, y)

#endif

#endif
