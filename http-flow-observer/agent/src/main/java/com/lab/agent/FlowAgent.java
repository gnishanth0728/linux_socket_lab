package com.lab.agent;

import java.lang.instrument.Instrumentation;

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
        premain(args, inst);
    }

    /* --------------------------------------------------------
     * Helpers
     * -------------------------------------------------------- */

    private static String normalise(String args) {
        if (args == null || args.isBlank()) return "";
        return args.trim().replace('.', '/');
    }
}
