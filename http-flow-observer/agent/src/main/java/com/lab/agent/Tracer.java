package com.lab.agent;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;

public final class Tracer {
    private static final String TOMCAT          = "TOMCAT";
    private static final String SPRING_MVC      = "SPRING_MVC";
    private static final String SQL_QUERY       = "SQL_QUERY";
    private static final String POSTGRESQL      = "POSTGRESQL";
    private static final String CONNECTION_POOL = "CONNECTION_POOL";

    private static final AtomicLong REQUEST_SEQ = new AtomicLong(1L);
    private static final ThreadLocal<FlowTimeline> TL =
            ThreadLocal.withInitial(FlowTimeline::new);

    private Tracer() {}

    /* --------------------------------------------------------
     * Instrumented entry points
     * -------------------------------------------------------- */

    public static void enter(String stage,
                             String className,
                             String methodName,
                             String methodDesc) {
        enterInternal(stage, className, methodName, methodDesc, null);
    }

    public static void enterSql(String stage,
                                String className,
                                String methodName,
                                String methodDesc,
                                String sql) {
        enterInternal(stage, className, methodName, methodDesc, normaliseSql(sql));
    }

    private static void enterInternal(String stage,
                                      String className,
                                      String methodName,
                                      String methodDesc,
                                      String sql) {
        FlowTimeline timeline = TL.get();

        if (!timeline.isActive() && (TOMCAT.equals(stage) || SPRING_MVC.equals(stage))) {
            timeline.start(REQUEST_SEQ.getAndIncrement());
        }

        if (!timeline.isActive()) {
            return;
        }

        Span span = new Span(stage, className, methodName, methodDesc, sql,
                             timeline.depth(), System.nanoTime());
        timeline.push(span);
    }

    public static void exit(String stage,
                            String className,
                            String methodName,
                            boolean threw) {
        FlowTimeline timeline = TL.get();
        if (!timeline.isActive()) {
            return;
        }

        Span span = timeline.pop(className, methodName);
        if (span == null) {
            return;
        }

        span.finish(System.nanoTime(), threw);
        timeline.add(span);

        if (TOMCAT.equals(stage)) {
            timeline.finish();
            dump(timeline);
            timeline.reset();
            TracerContext.clear();
        }
    }

    /* --------------------------------------------------------
     * Request-centric dump
     *
     * Merges kernel steps (from output/kernel_request.txt written
     * by the C observer on TCP_SENDMSG) with the JVM spans
     * collected by the agent, then prints one numbered timeline.
     * -------------------------------------------------------- */

    private static void dump(FlowTimeline timeline) {
        List<Span> spans = timeline.spans();
        if (spans.isEmpty()) {
            return;
        }

        String uri    = TracerContext.getRequestUri();
        String client = TracerContext.getClient();
        String server = TracerContext.getServer();

        List<KernelStep> kernelSteps = readKernelSteps();

        System.out.println();
        System.out.println("====================================================================");
        System.out.printf("HTTP Request #%d  GET %s%n", timeline.requestId(), uri);
        System.out.printf("Client: %-24s  Server: %s%n",
                client.equals("?") ? kernelClient(kernelSteps) : client,
                server.equals("?") ? kernelServer(kernelSteps) : server);
        System.out.println("====================================================================");

        /* ----- Kernel steps (NIC → Socket) ----- */
        for (KernelStep ks : kernelSteps) {
            System.out.printf("%nSTEP %-2d  %s%n", ks.n, ks.desc);
            if (ks.n == 1) {
                System.out.printf("         ↓ %s%n", ks.name);
            } else {
                System.out.printf("         ↓ %-28s  Δ %.3f us%n", ks.name, ks.deltaUs);
            }
        }

        /* ----- JVM steps (Tomcat → response) ----- */
        int base = kernelSteps.size();
        for (int i = 0; i < spans.size(); i++) {
            Span s   = spans.get(i);
            int  num = base + i + 1;
            System.out.printf("%nSTEP %-2d  %s%n", num, jvmDesc(s.stage));
            String indent = "  ".repeat(Math.max(0, s.depth - 1));
            String cls    = s.className.substring(s.className.lastIndexOf('/') + 1);
            System.out.printf("         %s↓ %-30s  %.3f us%s%n",
                    indent, cls + "#" + s.methodName,
                    s.durationNs() / 1_000.0,
                    s.threw ? "  [threw]" : "");
            if (s.sql != null) {
                System.out.printf("           SQL: %s%n", trim(s.sql, 80));
            }
        }

        long totalNs = timeline.totalNs();
        if (!kernelSteps.isEmpty()) {
            totalNs += (long)(kernelSteps.get(kernelSteps.size() - 1).totalUs * 1_000);
        }

        System.out.println();
        System.out.println("====================================================================");
        System.out.printf("Total latency: %.2f ms  (%d kernel steps + %d JVM spans)%n",
                totalNs / 1_000_000.0,
                kernelSteps.size(),
                spans.size());
        System.out.println("====================================================================");
        System.out.println();

        writeMergedRequest(timeline.requestId(), uri,
                kernelClient(kernelSteps), kernelServer(kernelSteps),
                totalNs, kernelSteps, spans);
    }

    /* --------------------------------------------------------
     * Write merged_requests.jsonl for the live UI
     *
     * Each completed request appends one JSON line containing
     * every kernel step (from kernel_request.txt) plus every
     * JVM span.  The live UI reads this file, picks the last
     * line, and renders the full OS + JVM step list.
     * -------------------------------------------------------- */

    private static void writeMergedRequest(long id, String uri,
                                           String client, String server,
                                           long totalNs,
                                           List<KernelStep> kernelSteps,
                                           List<Span> jvmSpans) {
        String path = outputDir() + "/merged_requests.jsonl";
        try (java.io.PrintWriter pw = new java.io.PrintWriter(
                new java.io.FileWriter(path, true))) {

            StringBuilder sb = new StringBuilder();
            sb.append('{');
            sb.append("\"id\":").append(id).append(',');
            sb.append("\"uri\":\"").append(jsonEsc(uri)).append("\",");
            sb.append("\"client\":\"").append(jsonEsc(client)).append("\",");
            sb.append("\"server\":\"").append(jsonEsc(server)).append("\",");
            sb.append("\"total_ms\":").append(String.format("%.3f", totalNs / 1_000_000.0)).append(',');
            sb.append("\"steps\":[");

            int n = 0;

            // kernel steps
            for (KernelStep ks : kernelSteps) {
                if (n++ > 0) sb.append(',');
                sb.append('{');
                sb.append("\"n\":").append(ks.n).append(',');
                sb.append("\"layer\":\"kernel\",");
                sb.append("\"stage\":\"").append(jsonEsc(ks.name)).append("\",");
                sb.append("\"desc\":\"").append(jsonEsc(ks.desc)).append("\",");
                sb.append("\"delta_us\":").append(String.format("%.3f", ks.deltaUs));
                sb.append('}');
            }

            // JVM spans
            int base = kernelSteps.size();
            for (int i = 0; i < jvmSpans.size(); i++) {
                Span s = jvmSpans.get(i);
                if (n++ > 0) sb.append(',');
                String cls = s.className.substring(s.className.lastIndexOf('/') + 1);
                sb.append('{');
                sb.append("\"n\":").append(base + i + 1).append(',');
                sb.append("\"layer\":\"jvm\",");
                sb.append("\"stage\":\"").append(jsonEsc(s.stage)).append("\",");
                sb.append("\"desc\":\"").append(jsonEsc(jvmDesc(s.stage))).append("\",");
                sb.append("\"method\":\"").append(jsonEsc(cls + "#" + s.methodName)).append("\",");
                sb.append("\"duration_us\":").append(String.format("%.3f", s.durationNs() / 1_000.0));
                if (s.sql != null)
                    sb.append(",\"sql\":\"").append(jsonEsc(trim(s.sql, 120))).append('"');
                if (s.threw)
                    sb.append(",\"threw\":true");
                sb.append('}');
            }

            sb.append("]}");
            pw.println(sb);
        } catch (Exception ignored) {}
    }

    private static String jsonEsc(String s) {
        if (s == null) return "";
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    /* --------------------------------------------------------
     * Kernel handoff file reader
     * -------------------------------------------------------- */

    private static class KernelStep {
        int n; String name; String desc; double deltaUs; double totalUs;
        String client; String server;
    }

    private static List<KernelStep> readKernelSteps() {
        List<KernelStep> steps = new ArrayList<>();
        String dir  = outputDir();
        String path = dir + "/kernel_request.txt";
        String client = "?"; String server = "?"; double totalUs = 0;

        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            String line;
            while ((line = br.readLine()) != null) {
                String[] p = line.split("\\|");
                if (p.length == 0) continue;
                switch (p[0]) {
                    case "CLIENT": client = p.length > 1 ? p[1] : "?"; break;
                    case "SERVER": server = p.length > 1 ? p[1] : "?"; break;
                    case "TOTAL_NS":
                        if (p.length > 1) totalUs = Long.parseLong(p[1].trim()) / 1_000.0;
                        break;
                    case "STEP":
                        if (p.length >= 5) {
                            KernelStep ks = new KernelStep();
                            ks.n       = Integer.parseInt(p[1].trim());
                            ks.name    = p[2].trim();
                            ks.desc    = p[3].trim();
                            ks.deltaUs = Long.parseLong(p[4].trim()) / 1_000.0;
                            steps.add(ks);
                        }
                        break;
                }
            }
        } catch (Exception ignored) { /* kernel observer not running — show JVM-only */ }

        // backfill client/server and totalUs into last step
        for (KernelStep ks : steps) {
            ks.client  = client;
            ks.server  = server;
            ks.totalUs = totalUs;
        }
        return steps;
    }

    private static String kernelClient(List<KernelStep> steps) {
        return steps.isEmpty() ? "?" : steps.get(0).client;
    }

    private static String kernelServer(List<KernelStep> steps) {
        return steps.isEmpty() ? "?" : steps.get(0).server;
    }

    private static String outputDir() {
        String env = System.getenv("HTTP_FLOW_OUTPUT_DIR");
        return (env != null && !env.isEmpty()) ? env : "output";
    }

    /* --------------------------------------------------------
     * Helpers
     * -------------------------------------------------------- */

    private static String jvmDesc(String stage) {
        switch (stage) {
            case "TOMCAT":           return "Tomcat worker thread processes connection";
            case "SPRING_MVC":       return "DispatcherServlet matches and dispatches handler";
            case "CONTROLLER":       return "Controller handles HTTP endpoint";
            case "SERVICE":          return "Service layer executes business logic";
            case "REPOSITORY":       return "Repository queries the data store";
            case "SQL_QUERY":        return "JDBC executes SQL statement";
            case "POSTGRESQL":       return "PostgreSQL driver sends query to database";
            case "CONNECTION_POOL":  return "Connection pool acquires database connection";
            case "APP":              return "Application code executes";
            default:                 return stage;
        }
    }

    private static String normaliseSql(String sql) {
        if (sql == null) return null;
        String s = sql.replaceAll("\\s+", " ").trim();
        return s.isEmpty() ? null : trim(s, 180);
    }

    private static String trim(String value, int max) {
        if (value == null || value.length() <= max) return value;
        return max < 4 ? value.substring(0, max) : value.substring(0, max - 3) + "...";
    }
}
    private static final ThreadLocal<FlowTimeline> TL =
            ThreadLocal.withInitial(FlowTimeline::new);

    private Tracer() {}

    public static void enter(String stage,
                             String className,
                             String methodName,
                             String methodDesc) {
        enterInternal(stage, className, methodName, methodDesc, null);
    }

    public static void enterSql(String stage,
                                String className,
                                String methodName,
                                String methodDesc,
                                String sql) {
        enterInternal(stage, className, methodName, methodDesc, normaliseSql(sql));
    }

    private static void enterInternal(String stage,
                                      String className,
                                      String methodName,
                                      String methodDesc,
                                      String sql) {
        FlowTimeline timeline = TL.get();

        if (!timeline.isActive() && (TOMCAT.equals(stage) || SPRING_MVC.equals(stage))) {
            timeline.start(REQUEST_SEQ.getAndIncrement());
        }

        if (!timeline.isActive()) {
            return;
        }

        Span span = new Span(stage,
                             className,
                             methodName,
                             methodDesc,
                             sql,
                             timeline.depth(),
                             System.nanoTime());
        timeline.push(span);
    }

    public static void exit(String stage,
                            String className,
                            String methodName,
                            boolean threw) {
        FlowTimeline timeline = TL.get();
        if (!timeline.isActive()) {
            return;
        }

        Span span = timeline.pop(className, methodName);
        if (span == null) {
            return;
        }

        span.finish(System.nanoTime(), threw);
        timeline.add(span);

        if (TOMCAT.equals(stage)) {
            timeline.finish();
            dump(timeline);
            timeline.reset();
        }
    }

    private static void dump(FlowTimeline timeline) {
        List<Span> spans = timeline.spans();
        if (spans.isEmpty()) {
            return;
        }

        System.out.println();
        System.out.println("============================================================");
        System.out.printf("JAVA TIMELINE request=%d thread=%s(%d)%n",
                timeline.requestId(), timeline.threadName(), timeline.threadId());
        System.out.println("============================================================");
        System.out.printf("%-14s %-52s %-12s %-7s%n",
                "Stage", "Method", "Duration(us)", "Thrown");
        System.out.println("------------------------------------------------------------");

        for (Span s : spans) {
            String method = s.className.replace('/', '.') + "#" + s.methodName;
            String indentedMethod = "  ".repeat(Math.max(0, s.depth)) + method;
            System.out.printf("%-14s %-52s %12.3f %-7s%n",
                    s.stage,
                    trim(indentedMethod, 52),
                    s.durationNs() / 1000.0,
                    s.threw ? "yes" : "no");
        }

        System.out.println("------------------------------------------------------------");
        System.out.printf("Total request time: %.3f ms%n", timeline.totalNs() / 1_000_000.0);
        System.out.println("============================================================");
        dumpSqlTimeline(timeline);
        System.out.println();
    }

    private static void dumpSqlTimeline(FlowTimeline timeline) {
        List<Span> spans = timeline.spans();
        boolean hasSql = false;
        for (Span span : spans) {
            if (SQL_QUERY.equals(span.stage)
                    || POSTGRESQL.equals(span.stage)
                    || CONNECTION_POOL.equals(span.stage)) {
                hasSql = true;
                break;
            }
        }

        if (!hasSql) {
            return;
        }

        System.out.println("SQL / DB TIMELINE");
        System.out.printf("%-16s %-12s %-64s%n", "Stage", "Latency(us)", "Query / Method");
        System.out.println("------------------------------------------------------------");

        for (Span span : spans) {
            if (!(SQL_QUERY.equals(span.stage)
                    || POSTGRESQL.equals(span.stage)
                    || CONNECTION_POOL.equals(span.stage))) {
                continue;
            }

            String label;
            if (span.sql != null && !span.sql.isBlank()) {
                label = span.sql;
            } else {
                label = span.className.replace('/', '.') + "#" + span.methodName;
            }

            System.out.printf("%-16s %12.3f %-64s%n",
                    span.stage,
                    span.durationNs() / 1000.0,
                    trim(label, 64));
        }
    }

    private static String normaliseSql(String sql) {
        if (sql == null) {
            return null;
        }

        String singleLine = sql.replaceAll("\\s+", " ").trim();
        if (singleLine.isEmpty()) {
            return null;
        }

        return trim(singleLine, 180);
    }

    private static String trim(String value, int max) {
        if (value.length() <= max) {
            return value;
        }
        if (max < 4) {
            return value.substring(0, max);
        }
        return value.substring(0, max - 3) + "...";
    }
}
