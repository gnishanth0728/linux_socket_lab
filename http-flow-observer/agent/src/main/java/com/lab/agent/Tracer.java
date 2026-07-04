package com.lab.agent;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;

public final class Tracer {
    private static final String TOMCAT = "TOMCAT";
    private static final String SPRING_MVC = "SPRING_MVC";

    private static final AtomicLong REQUEST_SEQ = new AtomicLong(1L);
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

    private static void dump(FlowTimeline timeline) {
        List<Span> spans = timeline.spans();
        if (spans.isEmpty()) {
            return;
        }

        String uri = TracerContext.getRequestUri();
        String client = TracerContext.getClient();
        String server = TracerContext.getServer();

        List<KernelStep> kernelSteps = readKernelSteps();

        if ("?".equals(client)) {
            client = kernelClient(kernelSteps);
        }
        if ("?".equals(server)) {
            server = kernelServer(kernelSteps);
        }

        long kernelNs = 0;
        if (!kernelSteps.isEmpty()) {
            kernelNs = (long)(kernelSteps.get(kernelSteps.size() - 1).totalUs * 1000.0);
        }

        long appNs = timeline.totalNs();
        long totalNs = kernelNs + appNs;

        System.out.println();
        System.out.println("========================================================");
        System.out.printf("HTTP Request #%d%n", timeline.requestId());
        System.out.printf("Socket %s%n", kernelSocket(kernelSteps));
        System.out.printf("Client %s%n", client);
        System.out.printf("Server %s%n", server);
        System.out.println("--------------------------------------------------------");

        int n = 1;
        for (KernelStep ks : kernelSteps) {
            System.out.printf("[✓] STEP %d %s%n", n++, ks.desc);
        }

        for (Span s : spans) {
            System.out.printf("[✓] STEP %d %s%n", n++, jvmDesc(s.stage, s));
        }

        System.out.printf("Kernel latency %.3f us%n", kernelNs / 1000.0);
        System.out.printf("Application latency %.3f ms%n", appNs / 1_000_000.0);
        System.out.printf("Total %.3f ms%n", totalNs / 1_000_000.0);
        System.out.println("========================================================");
        System.out.println();

        writeMergedRequest(timeline.requestId(), uri, client, server,
                totalNs, kernelSteps, spans);
    }

    private static String jvmDesc(String stage, Span span) {
        switch (stage) {
            case "TOMCAT":
                return "Tomcat";
            case "SPRING_MVC":
                return "DispatcherServlet";
            case "CONTROLLER":
                return shortClass(span.className);
            case "SERVICE":
                return shortClass(span.className);
            case "REPOSITORY":
                return shortClass(span.className);
            case "SQL_QUERY":
            case "POSTGRESQL":
                return "JDBC";
            case "CONNECTION_POOL":
                return "Connection Pool";
            default:
                return shortClass(span.className) + "#" + span.methodName;
        }
    }

    private static String shortClass(String slashClass) {
        int idx = slashClass.lastIndexOf('/');
        return idx >= 0 ? slashClass.substring(idx + 1) : slashClass;
    }

    private static void writeMergedRequest(long id,
                                           String uri,
                                           String client,
                                           String server,
                                           long totalNs,
                                           List<KernelStep> kernelSteps,
                                           List<Span> jvmSpans) {
        String path = outputDir() + "/merged_requests.jsonl";

        try (PrintWriter pw = new PrintWriter(new FileWriter(path, true))) {
            StringBuilder sb = new StringBuilder();
            sb.append('{');
            sb.append("\"id\":").append(id).append(',');
            sb.append("\"uri\":\"").append(jsonEsc(uri)).append("\",");
            sb.append("\"client\":\"").append(jsonEsc(client)).append("\",");
            sb.append("\"server\":\"").append(jsonEsc(server)).append("\",");
            sb.append("\"total_ms\":").append(String.format("%.3f", totalNs / 1_000_000.0)).append(',');
            sb.append("\"steps\":[");

            int written = 0;
            int n = 1;

            for (KernelStep ks : kernelSteps) {
                if (written++ > 0) sb.append(',');
                sb.append('{');
                sb.append("\"n\":").append(n++).append(',');
                sb.append("\"layer\":\"kernel\",");
                sb.append("\"stage\":\"").append(jsonEsc(ks.name)).append("\",");
                sb.append("\"desc\":\"").append(jsonEsc(ks.desc)).append("\",");
                sb.append("\"delta_us\":").append(String.format("%.3f", ks.deltaUs));
                sb.append('}');
            }

            for (Span s : jvmSpans) {
                if (written++ > 0) sb.append(',');
                sb.append('{');
                sb.append("\"n\":").append(n++).append(',');
                sb.append("\"layer\":\"jvm\",");
                sb.append("\"stage\":\"").append(jsonEsc(s.stage)).append("\",");
                sb.append("\"desc\":\"").append(jsonEsc(jvmDesc(s.stage, s))).append("\",");
                sb.append("\"duration_us\":").append(String.format("%.3f", s.durationNs() / 1000.0));
                if (s.sql != null) {
                    sb.append(",\"sql\":\"").append(jsonEsc(trim(s.sql, 120))).append('"');
                }
                sb.append('}');
            }

            sb.append("]}");
            pw.println(sb);
        } catch (Exception ignored) {
        }
    }

    private static String jsonEsc(String s) {
        if (s == null) {
            return "";
        }
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    private static class KernelStep {
        int n;
        String name;
        String desc;
        double deltaUs;
        double totalUs;
        String client;
        String server;
        String socket;
    }

    private static List<KernelStep> readKernelSteps() {
        List<KernelStep> steps = new ArrayList<>();
        String path = outputDir() + "/kernel_request.txt";

        String client = "?";
        String server = "?";
        String socket = "?";
        double totalUs = 0.0;

        try (BufferedReader br = new BufferedReader(new FileReader(path))) {
            String line;
            while ((line = br.readLine()) != null) {
                String[] p = line.split("\\|");
                if (p.length == 0) {
                    continue;
                }

                switch (p[0]) {
                    case "CLIENT":
                        client = p.length > 1 ? p[1] : "?";
                        break;

                    case "SERVER":
                        server = p.length > 1 ? p[1] : "?";
                        break;

                    case "SOCKET":
                        socket = p.length > 1 ? p[1] : "?";
                        break;

                    case "TOTAL_NS":
                        if (p.length > 1) {
                            totalUs = Long.parseLong(p[1].trim()) / 1000.0;
                        }
                        break;

                    case "STEP":
                        if (p.length >= 5) {
                            KernelStep ks = new KernelStep();
                            ks.n = Integer.parseInt(p[1].trim());
                            ks.name = p[2].trim();
                            ks.desc = p[3].trim();
                            ks.deltaUs = Long.parseLong(p[4].trim()) / 1000.0;
                            steps.add(ks);
                        }
                        break;

                    default:
                        break;
                }
            }
        } catch (Exception ignored) {
            // Kernel observer may not be running; JVM-only request view still works.
        }

        for (KernelStep ks : steps) {
            ks.client = client;
            ks.server = server;
            ks.totalUs = totalUs;
            ks.socket = socket;
        }

        return steps;
    }

    private static String kernelClient(List<KernelStep> steps) {
        return steps.isEmpty() ? "?" : steps.get(0).client;
    }

    private static String kernelServer(List<KernelStep> steps) {
        return steps.isEmpty() ? "?" : steps.get(0).server;
    }

    private static String kernelSocket(List<KernelStep> steps) {
        return steps.isEmpty() ? "?" : steps.get(0).socket;
    }

    private static String outputDir() {
        String env = System.getenv("HTTP_FLOW_OUTPUT_DIR");
        return (env != null && !env.isEmpty()) ? env : "output";
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
        if (value == null || value.length() <= max) {
            return value;
        }
        if (max < 4) {
            return value.substring(0, max);
        }
        return value.substring(0, max - 3) + "...";
    }
}
