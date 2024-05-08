#!/bin/bash

# Copy the tests to device.
adb sync

failures=0

if [ -e "$ANDROID_PRODUCT_OUT/testcases/mi_mempool_test_device/arm64/mi_mempool_test_device.sh" ]; then
  adb shell 'sh /data/nativetest64/mi_mempool_test_device/mi_mempool_test_device.sh'
   if [ $? -eq 0 ]; then
     echo -e "mi_mempool_test_device [PASS]"
   else
     failures=$(($failures+1))
   fi
fi

if [ $failures -ne 0 ]; then
  echo -e "FAILED: $failures"
fi
exit $failures
