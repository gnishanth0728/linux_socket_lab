package com.lab.agent;

import java.lang.instrument.Instrumentation;
import java.lang.instrument.UnmodifiableClassException;

/**
 * Java agent entry point.
 *
 * Attach at JVM startup:
 *   -javaagent:/path/to/http-flow-agent-1.0.0.jar=com.example
 *
 * The argument is the base package of the Spring Boot application
 * (dot or slash-separated).  It is used to discover @Controller,
 * @Service and @Repository classes for instrumentation.
 * Omit the argument to trace only Tomcat + Spring MVC.
 */
public final class FlowAgent {

    private FlowAgent() {}

    /* --------------------------------------------------------
     * Startup attach  (-javaagent:...)
     * -------------------------------------------------------- */

    public static void premain(String args, Instrumentation inst) {
        String pkg = normalise(args);
        System.out.println("[FlowAgent] HTTP Flow Observer attached."
                           + "  package=" + (pkg.isEmpty() ? "(all)" : pkg));
        inst.addTransformer(new MethodTransformer(pkg), true);
    }

    /* --------------------------------------------------------
     * Dynamic attach  (Attach API)
     * -------------------------------------------------------- */

    public static void agentmain(String args, Instrumentation inst) {
        String pkg = normalise(args);
        System.out.println("[FlowAgent] HTTP Flow Observer attached."
                           + "  package=" + (pkg.isEmpty() ? "(all)" : pkg)
                           + "  mode=dynamic");

        MethodTransformer transformer = new MethodTransformer(pkg);
        inst.addTransformer(transformer, true);
        retransformLoadedClasses(inst, transformer);
    }

    /* --------------------------------------------------------
     * Helpers
     * -------------------------------------------------------- */

    private static String normalise(String args) {
        if (args == null || args.isBlank()) return "";
        return args.trim().replace('.', '/');
    }

    private static void retransformLoadedClasses(Instrumentation inst,
                                                 MethodTransformer transformer) {
        if (!inst.isRetransformClassesSupported()) {
            System.err.println("[FlowAgent] Retransform not supported; only newly loaded classes will be instrumented.");
            return;
        }

        Class<?>[] loaded = inst.getAllLoadedClasses();
        for (Class<?> type : loaded) {
            if (type == null || !inst.isModifiableClass(type)) {
                continue;
            }

            String className = type.getName().replace('.', '/');
            if (!transformer.shouldInstrumentClass(className)) {
                continue;
            }

            try {
                inst.retransformClasses(type);
            } catch (UnmodifiableClassException | InternalError ex) {
                System.err.println("[FlowAgent] retransform failed for " + className + ": " + ex);
            }
        }
    }
}
