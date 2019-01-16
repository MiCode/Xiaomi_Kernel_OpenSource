adb devices
adb shell "cat /sys/kernel/debug/mtkfb_layer%1 > /data/fb_data.bin"
adb pull /data/fb_data.bin ./%2
adb shell "rm /data/fb_data.bin"