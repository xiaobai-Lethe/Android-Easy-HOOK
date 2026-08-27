#!/bin/bash
#[[
# 2026年08月28日00时32分
# 核实 ROM 中 libwhite.so 的实际工作机制：它内嵌了哪些目标符号与 Java 类名。
# 用于确认屏下指纹修复是否依赖 JVM，而非仅靠 sysfs 节点。
# 2026 BY white
#]]
SO=/mnt/f/AB_DW/MIO-KITCHEN-4.1.8-win/14U/system/system/lib64/libwhite.so

echo "=== 内嵌的原生 hook 目标 ==="
strings -a "$SO" | grep -E '^(JNI_CreateJavaVM|_ZN7android14AndroidRuntime|transactNative|transact)$' | sort -u | sed 's/^/  /'

echo
echo "=== 内嵌的 Java 框架类名（JNI 反射目标）==="
strings -a "$SO" | grep -E 'com/android/server/biometrics|android/hardware/fingerprint|android/os/Parcel|android/os/ServiceManager' | sort -u | head -12 | sed 's/^/  /'

echo
echo "=== 内嵌的 Java 字段与方法名 ==="
strings -a "$SO" | grep -E '^(mScheduler|mCurrentOperation|mSensorProperties|sensorType|halControlsIllumination|getInternalCallback|startHalOperation)$' | sort -u | sed 's/^/  /'

echo
echo "=== 厂商输出端（sysfs 与 AIDL）==="
strings -a "$SO" | grep -E 'mi_display|disp_param|IXiaomiFingerprint|ITouchFeature' | sort -u | head -8 | sed 's/^/  /'
