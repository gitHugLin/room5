#！/bin/bash
ndk-build
adb root
adb remount
adb push /Users/linqi/WorkDir/NormalPro/wdr-cpuMode/obj/local/armeabi/test-wdr /system/bin
#adb push /Users/linqi/WorkDir/NormalPro/wdr-cpuMode/obj/local/armeabi/libopencv_java3.so /system/lib
adb shell chmod 777 /system/bin/test-wdr
adb shell /system/bin/test-wdr
sleep 1
#adb pull /sdcard/grayImage.jpg ./pictures/
#adb pull /data/local/srcImage.jpg ./
#adb pull /data/local/avgLum.jpg ./
#adb pull /data/local/nonLum.jpg ./
#adb pull /data/local/maxLum.jpg ./
#adb logcat *:E
adb logcat -s MY_LOG_TAG
adb logcat -c
