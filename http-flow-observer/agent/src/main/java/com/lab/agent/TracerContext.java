package com.lab.agent;

/**
 * Per-thread HTTP request context.
 * Populated by the MethodTransformer when DispatcherServlet.doDispatch fires.
 * Read by Tracer.dump() to include the URI in the merged request view.
 */
public final class TracerContext {

    private static final ThreadLocal<String> URI    = new ThreadLocal<>();
    private static final ThreadLocal<String> CLIENT = new ThreadLocal<>();
    private static final ThreadLocal<String> SERVER = new ThreadLocal<>();

    private TracerContext() {}

    public static void setRequestUri(String uri) {
        if (uri != null) URI.set(uri);
    }

    public static String getRequestUri() {
        String s = URI.get();
        return (s != null && !s.isEmpty()) ? s : "/";
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
        CLIENT.remove();
        SERVER.remove();
    }
}
