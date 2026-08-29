package org.easyhook.flymevib;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * 2026年08月29日
 * Flyme 震动改道回调（ART hook 载体）。
 *
 * Flyme 把 VibratorController 的 on() 方法体改成了走 AAC RichTap
 * （vendor.aac.hardware.richtap.vibrator.IRichtap），而移植到的小米硬件上没有这个 HAL，
 * 于是每次震动都失败。小米原版 VibratorController 走的是标准 mNativeWrapper.perform/on
 * （驱动标准 android.hardware.vibrator HAL，dumpsys 已验证可用）。
 *
 * 本回调把 Flyme 的 on() 转译回标准路径：反射取 VibratorController.mNativeWrapper，
 * 调它的 perform(effectId, strength, vibrationId, stepId) 或 on(ms, vibrationId, stepId)。
 * 转译失败则回退到原方法，至少不改变现状。
 *
 * 只依赖 java.lang 与 java.lang.reflect，普通 JDK 的 javac 即可编译，不需要 android.jar；
 * 对 android.* 的引用一律走反射/Class.forName（含日志：android.util.Log）。
 * 2026 BY white
 */
public class VibratorRerouteHooker {

    private static final String TAG = "flyme-vib-java";

    /** 原方法备份，由 native 在 LSPlant 完成 hook 后写入；仅在转译失败时兜底使用。 */
    public Method backup;

    /**
     * 0 = on(PrebakedSegment, long, long)；1 = on(long, long, long)；2 = setAmplitude(float)。
     * 由 native 写入。
     */
    public int mode;

    // 标准 VibrationEffect.EFFECT_* 常量：本机 HAL 由 /vendor/etc/Hapticsconfig.xml 支持 0-5。
    private static final int EFFECT_CLICK = 0;
    private static final int EFFECT_DOUBLE_CLICK = 1;
    private static final int EFFECT_TICK = 2;
    private static final int EFFECT_THUD = 3;
    private static final int EFFECT_POP = 4;
    private static final int EFFECT_HEAVY_CLICK = 5;

    /**
     * 手感三个旋钮做成 setprop 运行时可调，改完立刻生效，不用重编 so 重刷机。
     *   persist.sys.easyhook.vib.effect      未命中专用映射时使用的标准效果(0-5)
     *   persist.sys.easyhook.vib.strength    EffectStrength：0=LIGHT 1=MEDIUM 2=STRONG，-1=沿用原值
     *   persist.sys.easyhook.vib.fallback_ms perform 仍不支持时兜底 on() 的时长
     */
    private static final String PROP_EFFECT = "persist.sys.easyhook.vib.effect";
    private static final String PROP_STRENGTH = "persist.sys.easyhook.vib.strength";
    private static final String PROP_FALLBACK_MS = "persist.sys.easyhook.vib.fallback_ms";

    /**
     * 默认取本机能给出的最强单次震动。Hapticsconfig.xml 里 EffectStrength 选的是三档
     * 强度字段：LIGHT→low(30)、MEDIUM→mid、STRONG→high(90)。HEAVY_CLICK 的 mid 是 100、
     * 脉宽 100ms，且其 mid(100) 反而高于 high(90)，所以最强组合是 HEAVY_CLICK + MEDIUM，
     * 而不是直觉上的 STRONG。原先默认的 CLICK + MEDIUM 只有 65、脉宽 35ms，所以偏弱。
     */
    private static final int DEFAULT_EFFECT = EFFECT_HEAVY_CLICK;
    private static final int DEFAULT_STRENGTH = 1;  // MEDIUM
    private static final long DEFAULT_FALLBACK_MS = 40L;

    // android.os.SystemProperties.getInt 反射句柄，解析一次复用。
    private static volatile boolean sPropResolved;
    private static Method sPropGetInt;

    // 反射目标进程内稳定，解析一次全程复用。
    private static volatile boolean sReady;
    private static Field sNativeWrapperField;
    private static Method sPerform;            // NativeWrapper.perform(long,long,long,long)
    private static Method sOn;                 // NativeWrapper.on(long,long,long)
    private static Method sSetAmplitude;       // NativeWrapper.setAmplitude(float)
    private static Method sGetEffectId;        // PrebakedSegment.getEffectId()
    private static Method sGetEffectStrength;  // PrebakedSegment.getEffectStrength()

    // android.util.Log 反射句柄，解析一次复用；解析失败则静默不打日志。
    private static volatile boolean sLogResolved;
    private static Method sLogI;
    private static Method sLogE;
    private static Method sLogStack;

    public Object callback(Object[] args) throws Throwable {
        final Object controller = args.length > 0 ? args[0] : null;
        logi("callback enter mode=" + mode + " argc=" + (args == null ? -1 : args.length)
                + " controller=" + (controller == null ? "null"
                        : controller.getClass().getName()));
        try {
            ensureReflection(controller);
            final Object nativeWrapper = sNativeWrapperField.get(controller);
            if (nativeWrapper == null) {
                loge("mNativeWrapper is null, fallback", null);
                return invokeBackup(args);
            }

            if (mode == 2) {
                /*
                 * setAmplitude(float 0.0-1.0)：Flyme 只把幅度喂给已经失效的 RichTap，
                 * 从不调用 mNativeWrapper.setAmplitude，于是所有定时/波形震动都跑在 HAL
                 * 默认幅度上——这正是“震感偏弱”的根源之一。
                 *
                 * 这里补上原生调用，再放行原方法以保留它对 mCurrentAmplitude 的记账。
                 * 幅度值本身已由 Flyme 依 flyme_haptic_feedback_intensity 缩放过，
                 * 所以接回原生后，设置里的震感滑块自然生效，不需要在此写死任何强度。
                 */
                final float amplitude = ((Float) args[1]).floatValue();
                logi("reroute setAmplitude=" + amplitude);
                sSetAmplitude.invoke(nativeWrapper, Float.valueOf(amplitude));
                return invokeBackup(args);
            }

            if (mode == 0) {
                // on(PrebakedSegment prebaked, long vibrationId, long stepId)
                final Object prebaked = args[1];
                final long vibrationId = ((Long) args[2]).longValue();
                final long stepId = ((Long) args[3]).longValue();
                final int rawEffectId = ((Integer) sGetEffectId.invoke(prebaked)).intValue();
                final int rawStrength = ((Integer) sGetEffectStrength.invoke(prebaked)).intValue();
                // Flyme 把自家 effectId(21000/30900/... 均 > 5)塞进 PrebakedSegment，标准 HAL 只认
                // 0-5，直接 perform 会返回 0(不震)。这里先映射回标准效果再 perform。
                final int stdEffectId = mapToStandardEffect(rawEffectId);
                final int configuredStrength = getPropInt(PROP_STRENGTH, DEFAULT_STRENGTH);
                final int strength = configuredStrength < 0 ? rawStrength : configuredStrength;
                logi("reroute perform rawEffectId=" + rawEffectId + " -> std=" + stdEffectId
                        + " strength=" + strength + "(raw=" + rawStrength + ")"
                        + " vibrationId=" + vibrationId + " stepId=" + stepId);
                final Object performed = sPerform.invoke(nativeWrapper,
                        Long.valueOf((long) stdEffectId), Long.valueOf((long) strength),
                        Long.valueOf(vibrationId), Long.valueOf(stepId));
                logi("reroute perform returned=" + performed);
                if (performed instanceof Long && ((Long) performed).longValue() > 0L) {
                    return performed;
                }
                // 映射后的效果 HAL 仍不支持(返回 0)：退回定时 on()，保证"任何场景都有震感"。
                final long fallbackMs = getPropInt(PROP_FALLBACK_MS, (int) DEFAULT_FALLBACK_MS);
                final Object onRet = sOn.invoke(nativeWrapper, Long.valueOf(fallbackMs),
                        Long.valueOf(vibrationId), Long.valueOf(stepId));
                logi("reroute perform unsupported, on() fallback ms=" + fallbackMs
                        + " returned=" + onRet);
                return onRet;
            }

            // on(long milliseconds, long vibrationId, long stepId)
            final long milliseconds = ((Long) args[1]).longValue();
            final long vibrationId = ((Long) args[2]).longValue();
            final long stepId = ((Long) args[3]).longValue();
            logi("reroute on ms=" + milliseconds + " vibrationId=" + vibrationId
                    + " stepId=" + stepId);
            final Object ret = sOn.invoke(nativeWrapper, Long.valueOf(milliseconds),
                    Long.valueOf(vibrationId), Long.valueOf(stepId));
            logi("reroute on returned=" + ret);
            return ret;
        } catch (Throwable rerouteError) {
            // 翻译失败：把真实异常打出来（之前这里静默吞了，导致完全无法定位），再回退原方法。
            final Throwable cause = rerouteError instanceof InvocationTargetException
                    && rerouteError.getCause() != null ? rerouteError.getCause() : rerouteError;
            loge("reroute failed, fallback to original", cause);
            return invokeBackup(args);
        }
    }

    private Object invokeBackup(Object[] args) throws Throwable {
        if (backup == null) {
            loge("backup is null, return 0", null);
            return Long.valueOf(0L);
        }
        final Object receiver = args.length > 0 ? args[0] : null;
        final Object[] rest = new Object[args.length > 0 ? args.length - 1 : 0];
        for (int i = 1; i < args.length; i++) {
            rest[i - 1] = args[i];
        }
        try {
            return backup.invoke(receiver, rest);
        } catch (InvocationTargetException error) {
            throw error.getCause();
        }
    }

    /**
     * 把 Flyme 自定义 effectId 映射回标准 HAL 支持的 0-5。
     * 0-5 原样透传；表达“错误/拒绝”的映射成 DOUBLE_CLICK 以保留区分度；
     * 其余一律用可配置的默认效果(默认 HEAVY_CLICK，本机最强的单次震动)。表可按需扩充。
     */
    private static int mapToStandardEffect(int effectId) {
        if (effectId >= EFFECT_CLICK && effectId <= EFFECT_HEAVY_CLICK) {
            return effectId;
        }
        switch (effectId) {
            case 30999:  // fingerprint_error
            case 31003:  // reject
                return EFFECT_DOUBLE_CLICK;
            default:
                return getPropInt(PROP_EFFECT, DEFAULT_EFFECT);
        }
    }

    /**
     * 读 android.os.SystemProperties.getInt(key, def)，用于运行时调手感：
     * setprop 之后下一次震动即生效，不必重编 so。反射不可用时返回默认值。
     */
    private static int getPropInt(String key, int def) {
        ensureProp();
        if (sPropGetInt == null) {
            return def;
        }
        try {
            return ((Integer) sPropGetInt.invoke(null, key, Integer.valueOf(def))).intValue();
        } catch (Throwable ignored) {
            return def;
        }
    }

    private static void ensureProp() {
        if (sPropResolved) {
            return;
        }
        synchronized (VibratorRerouteHooker.class) {
            if (sPropResolved) {
                return;
            }
            try {
                final Class<?> systemProperties = Class.forName("android.os.SystemProperties");
                sPropGetInt = systemProperties.getMethod("getInt", String.class, int.class);
            } catch (Throwable ignored) {
                sPropGetInt = null;
            }
            sPropResolved = true;
        }
    }

    private static void ensureReflection(Object controller) throws Exception {
        if (sReady) {
            return;
        }
        synchronized (VibratorRerouteHooker.class) {
            if (sReady) {
                return;
            }
            final Field nativeWrapperField =
                    controller.getClass().getDeclaredField("mNativeWrapper");
            nativeWrapperField.setAccessible(true);

            final Class<?> nativeWrapperClass = nativeWrapperField.getType();
            final Method perform = nativeWrapperClass.getDeclaredMethod(
                    "perform", long.class, long.class, long.class, long.class);
            perform.setAccessible(true);
            final Method on = nativeWrapperClass.getDeclaredMethod(
                    "on", long.class, long.class, long.class);
            on.setAccessible(true);
            final Method setAmplitude =
                    nativeWrapperClass.getDeclaredMethod("setAmplitude", float.class);
            setAmplitude.setAccessible(true);

            final Class<?> prebakedClass = Class.forName("android.os.vibrator.PrebakedSegment");
            final Method getEffectId = prebakedClass.getMethod("getEffectId");
            final Method getEffectStrength = prebakedClass.getMethod("getEffectStrength");

            sNativeWrapperField = nativeWrapperField;
            sPerform = perform;
            sOn = on;
            sSetAmplitude = setAmplitude;
            sGetEffectId = getEffectId;
            sGetEffectStrength = getEffectStrength;
            sReady = true;
            logi("reflection ready nativeWrapper=" + nativeWrapperClass.getName());
        }
    }

    // ---- 日志（全部走反射到 android.util.Log，解析失败就沉默） ----

    private static void ensureLog() {
        if (sLogResolved) {
            return;
        }
        synchronized (VibratorRerouteHooker.class) {
            if (sLogResolved) {
                return;
            }
            try {
                final Class<?> log = Class.forName("android.util.Log");
                sLogI = log.getMethod("i", String.class, String.class);
                sLogE = log.getMethod("e", String.class, String.class, Throwable.class);
                sLogStack = log.getMethod("getStackTraceString", Throwable.class);
            } catch (Throwable ignored) {
                sLogI = null;
                sLogE = null;
                sLogStack = null;
            }
            sLogResolved = true;
        }
    }

    private static void logi(String msg) {
        ensureLog();
        if (sLogI == null) {
            return;
        }
        try {
            sLogI.invoke(null, TAG, msg);
        } catch (Throwable ignored) {
            // 日志失败不影响主流程。
        }
    }

    private static void loge(String msg, Throwable error) {
        ensureLog();
        if (sLogE == null) {
            return;
        }
        try {
            sLogE.invoke(null, TAG, msg, error);
        } catch (Throwable ignored) {
            // 日志失败不影响主流程。
        }
    }
}
