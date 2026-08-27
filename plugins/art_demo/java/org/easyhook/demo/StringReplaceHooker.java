package org.easyhook.demo;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * 2026年08月28日00时58分
 * ART hook 的 Java 回调载体。
 *
 * LSPlant 的方法级 hook 要求调用方提供两样东西：一个承载上下文的对象，
 * 以及该对象上签名为 "Object 名字(Object[])" 的回调方法。
 * 注入型框架无法指望目标进程里存在自己的类，因此本类被编译成 dex 内嵌进 so，
 * 运行时再注入目标进程。
 *
 * 本类刻意只依赖 java.lang 与 java.lang.reflect，不触及任何 android.* 类型，
 * 这样用普通 JDK 的 javac 就能编译，构建链上不需要 android.jar。
 *
 * 匹配串与替换串由 native 侧在实例化后写入字段，因此同一份 dex 可服务于任意字符串替换，
 * 换一个替换目标不需要重新编译 dex。
 * 2026 BY white
 */
public class StringReplaceHooker {

    /**
     * 2026年08月28日00时58分
     * 原方法的备份。由 native 侧在 LSPlant 完成 hook 之后写入，
     * 回调内靠它取得未被改写的原始返回值。
     * 2026 BY white
     */
    public Method backup;

    /**
     * 2026年08月28日00时58分
     * 待匹配的原始文本与替换后的文本，均由 native 侧写入。
     * 2026 BY white
     */
    public String match;
    public String replacement;

    /**
     * 2026年08月28日00时58分
     * LSPlant 的统一回调入口。
     * args[0] 是目标方法的接收者(实例方法才有)，其余为目标方法的实参。
     * 这里先调用原方法拿到真实返回值，只有在其文本与 match 完全一致时才替换，
     * 其他调用一律原样放行，避免影响目标进程里其余成千上万次取字符串的调用。
     *
     * 反射调用抛出的 InvocationTargetException 必须解包后重新抛出，
     * 否则调用方看到的会是包装异常而不是原方法真正抛出的异常，行为与未 hook 时不一致。
     * 2026 BY white
     */
    public Object callback(Object[] args) throws Throwable {
        final Object receiver = args.length > 0 ? args[0] : null;
        final Object[] rest = new Object[args.length > 0 ? args.length - 1 : 0];
        for (int i = 1; i < args.length; i++) {
            rest[i - 1] = args[i];
        }

        /**
         * 2026年08月28日01时24分
         * 先改写入参中命中的文本，再调用原方法。
         * 有些文案并不是被"取出来"的，而是被"传进去"的——
         * 例如 TextView.setText 收到文本后直接显示，返回值是 void，
         * 只看返回值永远拦不到这类目标。
         * 仅改写 CharSequence 类型的实参，避免破坏其他类型的参数。
         * 2026 BY white
         */
        if (match != null) {
            for (int i = 0; i < rest.length; i++) {
                if (rest[i] instanceof CharSequence && match.equals(rest[i].toString())) {
                    rest[i] = replacement;
                }
            }
        }

        final Object result;
        try {
            result = backup.invoke(receiver, rest);
        } catch (InvocationTargetException error) {
            throw error.getCause();
        }

        if (result != null && match != null && match.equals(result.toString())) {
            return replacement;
        }
        return result;
    }
}
