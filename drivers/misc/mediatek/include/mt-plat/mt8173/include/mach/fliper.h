int fliper_set_bw(int bw);
int fliper_restore_bw(void);

#define BW_THRESHOLD_HIGH 7000
#define BW_THRESHOLD_LOW 800

extern unsigned long long get_mem_bw(void);

