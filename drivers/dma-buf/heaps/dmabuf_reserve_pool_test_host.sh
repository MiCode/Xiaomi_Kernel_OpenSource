#!/bin/bash

# Copy the tests to device.
adb sync

failures=0

if [ -e "$ANDROID_PRODUCT_OUT/testcases/dmabuf_reserve_pool_test_device/arm64/dmabuf_reserve_pool_test_device.sh" ]; then
  adb shell 'sh /data/nativetest64/dmabuf_reserve_pool_test_device/dmabuf_reserve_pool_test_device.sh'
   if [ $? -eq 0 ]; then
     echo -e "dmabuf_reserve_pool_test_device [PASS]"
   else
     failures=$(($failures+1))
   fi
fi

if [ $failures -ne 0 ]; then
  echo -e "FAILED: $failures"
fi
exit $failures
