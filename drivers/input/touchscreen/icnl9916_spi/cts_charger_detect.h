#ifndef CTS_CHARGER_DETECT_H
#define CTS_CHARGER_DETECT_H

#include "cts_config.h"

struct chipone_ts_data;

#ifdef CONFIG_CTS_CHARGER_DETECT
extern int cts_charger_detect_init(struct chipone_ts_data *cts_data);
extern int cts_charger_detect_deinit(struct chipone_ts_data *cts_data);
extern int cts_start_charger_detect(struct chipone_ts_data *cts_data);
extern int cts_stop_charger_detect(struct chipone_ts_data *cts_data);
extern int cts_is_charger_attached(struct chipone_ts_data *cts_data,
        bool *attached);
#else /* CONFIG_CTS_CHARGER_DETECT */
static inline int cts_charger_detect_init(struct chipone_ts_data *cts_data)
{
    return -ENOTSUPP;
}

static inline int cts_charger_detect_deinit(struct chipone_ts_data *cts_data)
{
    return -ENOTSUPP;
}

static inline int cts_start_charger_detect(struct chipone_ts_data *cts_data)
{
    return -ENODEV;
}

static inline int cts_stop_charger_detect(struct chipone_ts_data *cts_data)
{
    return -ENODEV;
}

static inline int cts_is_charger_attached(struct chipone_ts_data *cts_data,
        bool *attached)
{
    return -ENODEV;
}
#endif /* CONFIG_CTS_CHARGER_DETECT */

#endif /* CTS_CHARGER_DETECT_H */
