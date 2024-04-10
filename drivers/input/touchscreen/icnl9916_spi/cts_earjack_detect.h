#ifndef CTS_EARJACK_DETECT_H
#define CTS_EARJACK_DETECT_H

#include "cts_config.h"

struct chipone_ts_data;

#ifdef CONFIG_CTS_EARJACK_DETECT
extern int cts_earjack_detect_init(struct chipone_ts_data *cts_data);
extern int cts_earjack_detect_deinit(struct chipone_ts_data *cts_data);
extern int cts_start_earjack_detect(struct chipone_ts_data *cts_data);
extern int cts_stop_earjack_detect(struct chipone_ts_data *cts_data);
extern int cts_is_earjack_attached(struct chipone_ts_data *cts_data,
    bool *attached);
#else /* CONFIG_CTS_EARJACK_DETECT */
static inline int cts_earjack_detect_init(struct chipone_ts_data *cts_data)
{
    return -ENOTSUPP;
}
static inline int cts_earjack_detect_deinit(struct chipone_ts_data *cts_data)
{
    return -ENOTSUPP;
}
static inline int cts_start_earjack_detect(struct chipone_ts_data *cts_data)
{
    return -ENODEV;
}
static inline int cts_stop_earjack_detect(struct chipone_ts_data *cts_data)
{
    return -ENODEV;
}
static inline int cts_is_earjack_attached(struct chipone_ts_data *cts_data,
    bool *attached)
{
    return -ENODEV;
}
#endif /* CONFIG_CTS_EARJACK_DETECT */

#endif /* CTS_EARJACK_DETECT_H */

