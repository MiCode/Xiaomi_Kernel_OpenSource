#ifndef _MT_IDLE_H
#define _MT_IDLE_H
enum idle_lock_spm_id{
    IDLE_SPM_LOCK_VCORE_DVFS= 0,
};
extern void idle_lock_spm(enum idle_lock_spm_id id);
extern void idle_unlock_spm(enum idle_lock_spm_id id);

extern void enable_dpidle_by_bit(int id);
extern void disable_dpidle_by_bit(int id);
extern void enable_soidle_by_bit(int id);
extern void disable_soidle_by_bit(int id);


#endif
