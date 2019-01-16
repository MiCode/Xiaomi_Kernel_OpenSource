// use accdet + EINT solution
#define ACCDET_EINT
#ifndef ACCDET_EINT
#define ACCDET_EINT_IRQ
#endif

#define ACCDET_HIGH_VOL_MODE
#ifdef ACCDET_HIGH_VOL_MODE
#define ACCDET_MIC_VOL 6     //2.5v
#else
#define ACCDET_MIC_VOL 2     //1.9v
#endif

#define ACCDET_SHORT_PLUGOUT_DEBOUNCE
#define ACCDET_SHORT_PLUGOUT_DEBOUNCE_CN 20


