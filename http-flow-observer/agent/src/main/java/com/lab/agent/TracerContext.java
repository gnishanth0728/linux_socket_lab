package com.lab.agent;

import java.lang.reflect.Method;

/**
 * Per-thread HTTP request context.
 * Populated by the MethodTransformer when DispatcherServlet.doDispatch fires.
 * Read by Tracer.dump() to include the URI in the merged request view.
 */
public final class TracerContext {

    private static final ThreadLocal<String> URI    = new ThreadLocal<>();
    private static final ThreadLocal<String> METHOD = new ThreadLocal<>();
    private static final ThreadLocal<String> VERSION = new ThreadLocal<>();
    private static final ThreadLocal<String> HOST = new ThreadLocal<>();
    private static final ThreadLocal<String> USER_AGENT = new ThreadLocal<>();
    private static final ThreadLocal<String> ACCEPT = new ThreadLocal<>();
    private static final ThreadLocal<String> CONNECTION = new ThreadLocal<>();
    private static final ThreadLocal<Long> BODY_SIZE = new ThreadLocal<>();
    private static final ThreadLocal<String> CLIENT = new ThreadLocal<>();
    private static final ThreadLocal<String> SERVER = new ThreadLocal<>();

    private TracerContext() {}

    public static void setRequestUri(String uri) {
        if (uri != null) URI.set(uri);
    }

    public static void setRequestMethod(String method) {
        if (method != null) METHOD.set(method);
    }

    public static void setRequestVersion(String version) {
        if (version != null) VERSION.set(version);
    }

    public static void setHeaderHost(String value) {
        if (value != null) HOST.set(value);
    }

    public static void setHeaderUserAgent(String value) {
        if (value != null) USER_AGENT.set(value);
    }

    public static void setHeaderAccept(String value) {
        if (value != null) ACCEPT.set(value);
    }

    public static void setHeaderConnection(String value) {
        if (value != null) CONNECTION.set(value);
    }

    public static void setBodySize(long size) {
        BODY_SIZE.set(size);
    }

    public static void captureRequest(Object request) {
        if (request == null) {
            return;
        }

        trySetString(request, "getRequestURI", TracerContext::setRequestUri);
        trySetString(request, "getMethod", TracerContext::setRequestMethod);
        trySetString(request, "getProtocol", TracerContext::setRequestVersion);
        trySetHeader(request, "Host", TracerContext::setHeaderHost);
        trySetHeader(request, "User-Agent", TracerContext::setHeaderUserAgent);
        trySetHeader(request, "Accept", TracerContext::setHeaderAccept);
        trySetHeader(request, "Connection", TracerContext::setHeaderConnection);
        trySetLong(request, "getContentLengthLong", TracerContext::setBodySize);
    }

    public static String getRequestUri() {
        String s = URI.get();
        return (s != null && !s.isEmpty()) ? s : "/";
    }

    public static String getRequestMethod() {
        String s = METHOD.get();
        return (s != null && !s.isEmpty()) ? s : "GET";
    }

    public static String getRequestVersion() {
        String s = VERSION.get();
        return (s != null && !s.isEmpty()) ? s : "HTTP/1.1";
    }

    public static String getHeaderHost() {
        String s = HOST.get();
        return (s != null && !s.isEmpty()) ? s : "?";
    }

    public static String getHeaderUserAgent() {
        String s = USER_AGENT.get();
        return (s != null && !s.isEmpty()) ? s : "?";
    }

    public static String getHeaderAccept() {
        String s = ACCEPT.get();
        return (s != null && !s.isEmpty()) ? s : "*/*";
    }

    public static String getHeaderConnection() {
        String s = CONNECTION.get();
        return (s != null && !s.isEmpty()) ? s : "keep-alive";
    }

    public static long getBodySize() {
        Long v = BODY_SIZE.get();
        return (v != null && v >= 0L) ? v : 0L;
    }

    public static void setClient(String addr) { CLIENT.set(addr); }
    public static void setServer(String addr) { SERVER.set(addr); }

    public static String getClient() {
        String s = CLIENT.get();
        return (s != null) ? s : "?";
    }

    public static String getServer() {
        String s = SERVER.get();
        return (s != null) ? s : "?";
    }

    public static void clear() {
        URI.remove();
        METHOD.remove();
        VERSION.remove();
        HOST.remove();
        USER_AGENT.remove();
        ACCEPT.remove();
        CONNECTION.remove();
        BODY_SIZE.remove();
        CLIENT.remove();
        SERVER.remove();
    }

    private static void trySetString(Object target,
                                     String methodName,
                                     StringConsumer consumer) {
        try {
            Method method = target.getClass().getMethod(methodName);
            Object value = method.invoke(target);
            if (value instanceof String) {
                consumer.accept((String) value);
            }
        } catch (ReflectiveOperationException ignored) {
        }
    }

    private static void trySetHeader(Object target,
                                     String headerName,
                                     StringConsumer consumer) {
        try {
            Method method = target.getClass().getMethod("getHeader", String.class);
            Object value = method.invoke(target, headerName);
            if (value instanceof String) {
                consumer.accept((String) value);
            }
        } catch (ReflectiveOperationException ignored) {
        }
    }

    private static void trySetLong(Object target,
                                   String methodName,
                                   LongConsumer consumer) {
        try {
            Method method = target.getClass().getMethod(methodName);
            Object value = method.invoke(target);
            if (value instanceof Long) {
                consumer.accept((Long) value);
            }
        } catch (ReflectiveOperationException ignored) {
        }
    }

    @FunctionalInterface
    private interface StringConsumer {
        void accept(String value);
    }

    @FunctionalInterface
    private interface LongConsumer {
        void accept(long value);
    }
}
