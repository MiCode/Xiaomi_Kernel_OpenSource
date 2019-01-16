#ifndef  MT_SD_FUNC_H
#define  MT_SD_FUNC_H

#ifdef CONFIG_SDIOAUTOK_SUPPORT
int sdio_stop_transfer(void);
int sdio_start_ot_transfer(void);
int autok_abort_action(void);
int autok_is_vol_done(unsigned int voltage, int id);
#else
static inline int sdio_stop_transfer(void){return 0;}
static inline int sdio_start_ot_transfer(void){return 0;}
static inline int autok_abort_action(void){return 0;}
static inline int autok_is_vol_done(unsigned int voltage, int id){return 2;}
#endif
#endif /* end of  MT_SD_FUNC_H */

