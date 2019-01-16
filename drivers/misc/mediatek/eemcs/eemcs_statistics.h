#include "eemcs_ccci.h"
#include <mach/mtk_eemcs_helper.h>

typedef struct{
    unsigned int drop[2];
    unsigned int cnt[2];
    unsigned int queue[2];
}CCCI_PORT_STATISTICS;

enum {
    TX = 0,
    RX,
};

enum STATISTIC_TYPE{
    NORMAL = 0,
    DROP,
    QUEUE,
};

typedef struct{
    int md_id;
    int start;
    unsigned int inteval;
    struct timeval time;
    CCCI_PORT_STATISTICS port[CCCI_PORT_NUM_MAX];
    CCCI_PORT_STATISTICS port_total[CCCI_PORT_NUM_MAX];
}EEMCS_STATISTICS;

extern EEMCS_STATISTICS *eemcs_statistics[MAX_EXT_MD_NUM];
void eemcs_update_statistics_number(int md_id, int port, int tx_rx, int type, int number);
void eemcs_update_statistics(int md_id, int port, int tx_rx, int drop);
void eemcs_statistics_reset(void);
int eemcs_statistics_init(void);
void eemcs_statistics_exit(void);