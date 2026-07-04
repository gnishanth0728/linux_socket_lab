package com.lab.agent;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.security.ProtectionDomain;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.commons.AdviceAdapter;

public final class MethodTransformer implements ClassFileTransformer {
    private final String appPackage;

    public MethodTransformer(String appPackage) {
        this.appPackage = appPackage == null ? "" : appPackage;
    }

    @Override
    public byte[] transform(ClassLoader loader,
                            String className,
                            Class<?> classBeingRedefined,
                            ProtectionDomain protectionDomain,
                            byte[] classfileBuffer) throws IllegalClassFormatException {
        if (className == null || classfileBuffer == null) {
            return null;
        }

        if (className.startsWith("com/lab/agent/")) {
            return null;
        }

        if (!shouldInstrumentClass(className)) {
            return null;
        }

        try {
            ClassReader cr = new ClassReader(classfileBuffer);
            ClassWriter cw = new ClassWriter(cr, ClassWriter.COMPUTE_MAXS);

            ClassVisitor cv = new ClassVisitor(Opcodes.ASM9, cw) {
                @Override
                public MethodVisitor visitMethod(int access,
                                                 String name,
                                                 String descriptor,
                                                 String signature,
                                                 String[] exceptions) {
                    MethodVisitor mv = super.visitMethod(access, name, descriptor, signature, exceptions);

                    String stage = stageFor(className, name, descriptor, access);
                    if (stage == null) {
                        return mv;
                    }

                    return new AdviceAdapter(Opcodes.ASM9, mv, access, name, descriptor) {
                        private final boolean captureSql = descriptor.startsWith("(Ljava/lang/String;");

                        @Override
                        protected void onMethodEnter() {
                            visitLdcInsn(stage);
                            visitLdcInsn(className);
                            visitLdcInsn(name);
                            visitLdcInsn(descriptor);

                            if (captureSql) {
                                loadArg(0);
                                visitMethodInsn(INVOKESTATIC,
                                        "com/lab/agent/Tracer",
                                        "enterSql",
                                        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                        false);
                            } else {
                                visitMethodInsn(INVOKESTATIC,
                                        "com/lab/agent/Tracer",
                                        "enter",
                                        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                        false);
                            }
                        }

                        @Override
                        protected void onMethodExit(int opcode) {
                            visitLdcInsn(stage);
                            visitLdcInsn(className);
                            visitLdcInsn(name);
                            push(opcode == ATHROW);
                            visitMethodInsn(INVOKESTATIC,
                                    "com/lab/agent/Tracer",
                                    "exit",
                                    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V",
                                    false);
                        }
                    };
                }
            };

            cr.accept(cv, ClassReader.EXPAND_FRAMES);
            return cw.toByteArray();
        } catch (Throwable t) {
            System.err.println("[FlowAgent] transform failed for " + className + ": " + t);
            return null;
        }
    }

    private boolean shouldInstrumentClass(String className) {
        if ("org/apache/catalina/core/StandardWrapperValve".equals(className)) {
            return true;
        }

        if ("org/springframework/web/servlet/DispatcherServlet".equals(className)) {
            return true;
        }

        if (isPoolClass(className)) {
            return true;
        }

        if (!appPackage.isEmpty() && className.startsWith(appPackage)) {
            return true;
        }

        return isJdbcClass(className);
    }

    private static boolean isJdbcClass(String className) {
        return className.startsWith("org/postgresql/")
                || className.contains("/jdbc/")
                || className.endsWith("Statement")
                || className.endsWith("PreparedStatement");
    }

    private static boolean isPoolClass(String className) {
        return className.startsWith("com/zaxxer/hikari/")
                || className.startsWith("org/apache/tomcat/jdbc/pool/")
                || className.startsWith("org/apache/commons/dbcp2/");
    }

    private String stageFor(String className, String methodName, String descriptor, int access) {
        if ("<init>".equals(methodName) || "<clinit>".equals(methodName)) {
            return null;
        }

        if ("org/apache/catalina/core/StandardWrapperValve".equals(className)
                && "invoke".equals(methodName)) {
            return "TOMCAT";
        }

        if ("org/springframework/web/servlet/DispatcherServlet".equals(className)
                && ("doDispatch".equals(methodName) || "doService".equals(methodName))) {
            return "SPRING_MVC";
        }

        if (isPoolClass(className)
                && ("getConnection".equals(methodName)
                    || "borrowConnection".equals(methodName)
                    || "createPoolEntry".equals(methodName))) {
            return "CONNECTION_POOL";
        }

        if (!appPackage.isEmpty() && className.startsWith(appPackage)) {
            if ((access & Opcodes.ACC_ABSTRACT) != 0) {
                return null;
            }

            if (className.endsWith("Controller") || className.contains("/controller/")) {
                return "CONTROLLER";
            }

            if (className.endsWith("Service") || className.contains("/service/")) {
                return "SERVICE";
            }

            if (className.endsWith("Repository") || className.contains("/repository/")) {
                return "REPOSITORY";
            }

            return "APP";
        }

        if (className.startsWith("org/postgresql/")
                && (methodName.startsWith("prepare")
                    || methodName.startsWith("exec")
                    || methodName.startsWith("query"))) {
            return "POSTGRESQL";
        }

        if (isJdbcClass(className)
                && (methodName.startsWith("execute")
                    || methodName.startsWith("prepare"))) {
            return "SQL_QUERY";
        }

        return null;
    }
}
