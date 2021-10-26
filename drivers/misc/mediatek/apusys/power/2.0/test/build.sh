#!/bin/bash
gcc -Wformat=0 --debug -DBUILD_POLICY_TEST ../apusys_power_ctl.c \
../mt6885/apusys_power_cust.c policy_constraints_test.c -I../ -I../mt6885 \
 -I./ -o policy_constraints_test

# usage :
#	./policy_constraints_test 5 10 1
#
#	 5 as power on round
#	10 as opp change round
#	 1 as enable fail stop
