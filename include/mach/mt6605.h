#ifndef _MT6605_H_
#define _MT6605_H_

// return 0, success; return <0, fail
// md_id        : modem id
// md_state   : 0, on ; 1, off ;
// vsim_state : 0, on ; 1, off;
int inform_nfc_vsim_change(int md_id, int md_state, int vsim_state);

#endif  //_MT6605_H_