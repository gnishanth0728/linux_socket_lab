package com.lab.agent;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;

public final class Tracer {
    private static final String TOMCAT = "TOMCAT";
    private static final String SPRING_MVC = "SPRING_MVC";
    private static final boolean DEBUG = "1".equals(System.getenv("HTTP_FLOW_AGENT_DEBUG"));

    private static final String SECTION_NETWORK = "Network layer";
    private static final String SECTION_KERNEL_SOCKET = "Kernel socket layer";
    private static final String SECTION_WEB_SERVER = "Web server";
    private static final String SECTION_APPLICATION = "Application";
    private static final String SECTION_DATABASE = "Database";
    private static final String SECTION_RESPONSE = "Response path";

    private static final String C_RESET = "\u001B[0m";
    private static final String C_HEADER = "\u001B[38;5;45m";
    private static final String C_SECTION = "\u001B[38;5;220m";
    private static final String C_STEP = "\u001B[38;5;82m";
    private static final String C_DETAIL = "\u001B[38;5;110m";
    private static final String C_SUMMARY = "\u001B[38;5;141m";
    private static final DateTimeFormatter TS_FMT = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss.SSSSSS");

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

    public static void enterPrepared(String stage,
                                     String className,
                                     String methodName,
                                     String methodDesc,
                                     Object statement) {
        String sql = TracerContext.lookupPreparedStatementSql(statement);
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
            if (DEBUG && isDbStage(stage)) {
                System.out.println("[FlowAgent] dropped DB span without active request context: "
                        + className + "#" + methodName + " thread=" + Thread.currentThread().getName());
            }
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
        String method = TracerContext.getRequestMethod();
        String version = TracerContext.getRequestVersion();
        String host = TracerContext.getHeaderHost();
        String userAgent = TracerContext.getHeaderUserAgent();
        String accept = TracerContext.getHeaderAccept();
        String connHeader = TracerContext.getHeaderConnection();
        long bodySize = TracerContext.getBodySize();
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
        List<MergedStep> mergedSteps = buildMergedSteps(kernelSteps, spans, timeline.threadName());
        String process = chooseRequestProcess(mergedSteps, timeline.threadName());
        int cpu = firstCpu(kernelSteps);
        String pid = firstPid(kernelSteps);

        System.out.println();
        printCoreHeader(timeline.requestId(), totalNs, cpu);
        printConnectionBlock(client, server, process, pid, timeline.threadName(), kernelSocket(kernelSteps));
        printHttpBlock(method, uri, version, host, userAgent, accept, connHeader, bodySize);
        printStageBlock("NETWORK LAYER", mergedSteps, SECTION_NETWORK);
        printStageBlock("TCP LAYER", mergedSteps, "TCP");
        printStageBlock("SOCKET LAYER", mergedSteps, SECTION_KERNEL_SOCKET);
        printStageBlock("SCHEDULER", mergedSteps, "SCHEDULER");
        printStageBlock("NGINX", mergedSteps, SECTION_WEB_SERVER);
        printStageBlock("SPRING BOOT", mergedSteps, SECTION_APPLICATION);
        printStageBlock("DATABASE", mergedSteps, SECTION_DATABASE);
        printResponseBlock();
        printTransmitBlock();
        printLatencyBreakdown(mergedSteps, kernelNs, appNs, totalNs);
        System.out.println();

        writeMergedRequest(timeline.requestId(), uri, client, server,
                totalNs, mergedSteps);
    }

    private static void printCoreHeader(long requestId, long totalNs, int cpu) {
        banner("HTTP FLOW OBSERVER", C_HEADER);
        System.out.printf("Request ID     : #%d%n", requestId);
        System.out.printf("Timestamp      : %s%n", LocalDateTime.now().format(TS_FMT));
        System.out.printf("Kernel         : %s %s%n", System.getProperty("os.name", "Linux"), System.getProperty("os.version", "?"));
        System.out.printf("CPU            : %d%n", cpu);
        System.out.printf("Duration       : %.3f ms%n", totalNs / 1_000_000.0);
        System.out.println();
    }

    private static void printConnectionBlock(String client,
                                             String server,
                                             String process,
                                             String pid,
                                             String thread,
                                             String socket) {
        banner("CONNECTION", C_SECTION);

        String[] clientParts = splitHostPort(client);
        String[] serverParts = splitHostPort(server);

        System.out.println("Client");
        System.out.println("------");
        System.out.printf("IP            : %s%n", clientParts[0]);
        System.out.printf("Port          : %s%n", clientParts[1]);
        System.out.println();
        System.out.println("        |");
        System.out.println("        v");
        System.out.println();

        System.out.println("Server");
        System.out.println("------");
        System.out.printf("IP            : %s%n", serverParts[0]);
        System.out.printf("Port          : %s%n", serverParts[1]);
        System.out.printf("Process       : %s%n", safe(process, "?"));
        System.out.printf("PID           : %s%n", safe(pid, "?"));
        System.out.printf("Thread        : %s%n", safe(thread, "?"));
        System.out.println();

        System.out.println("Socket");
        System.out.println("------");
        System.out.printf("Socket Addr   : %s%n", safe(socket, "?"));
        System.out.printf("Socket Cookie : %s%n", "n/a");
        System.out.printf("Protocol      : %s%n", "TCP");
        System.out.printf("Family        : %s%n", "IPv4");
        System.out.printf("State         : %s%n", "ESTABLISHED");
        System.out.println();
    }

    private static void printHttpBlock(String method,
                                       String uri,
                                       String version,
                                       String host,
                                       String userAgent,
                                       String accept,
                                       String connection,
                                       long bodySize) {
        banner("HTTP REQUEST", C_SECTION);

        System.out.printf("Method        : %s%n", safe(method, "GET"));
        System.out.printf("URI           : %s%n", safe(uri, "/"));
        System.out.printf("Version       : %s%n", safe(version, "HTTP/1.1"));
        System.out.println();
        System.out.println("Headers");
        System.out.println();
        System.out.printf("Host          : %s%n", safe(host, "?"));
        System.out.printf("User-Agent    : %s%n", safe(userAgent, "?"));
        System.out.printf("Accept        : %s%n", safe(accept, "*/*"));
        System.out.printf("Connection    : %s%n", safe(connection, "keep-alive"));
        System.out.println();
        System.out.printf("Body Size     : %d bytes%n", Math.max(0L, bodySize));
        System.out.println();
    }

    private static void printStageBlock(String title, List<MergedStep> steps, String selector) {
        banner(title, C_SECTION);

        boolean printed = false;
        for (MergedStep step : steps) {
            if (!matchesBlock(selector, step)) {
                continue;
            }
            printed = true;

            System.out.printf("%s%s %s%s%n",
                    color(C_STEP, useColor()),
                    "+",
                    step.desc,
                    color(C_RESET, useColor()));

            System.out.printf("      %-15s: %s%n", "Stage", step.stage);
            System.out.printf("      %-15s: %s%n", "Layer", step.layer);
            System.out.printf("      %-15s: %s%n", "Process", step.process);
            System.out.printf("      %-15s: %s%n",
                    "Timing",
                    "kernel".equals(step.layer) ? ("+" + formatLatencyUs(step.timeUs)) : formatLatencyUs(step.timeUs));
            if (step.sql != null && !step.sql.isEmpty()) {
                System.out.printf("      %-15s: %s%n", "SQL", trim(step.sql, 180));
            }
            System.out.println();
        }

        if (!printed) {
            System.out.println("(no events captured for this stage on this request)");
            System.out.println();
        }
    }

    private static void printResponseBlock() {
        banner("HTTP RESPONSE", C_SECTION);
        System.out.println("Status");
        System.out.println();
        System.out.println("200 OK");
        System.out.println();
        System.out.println("Headers");
        System.out.println();
        System.out.println("Content-Type");
        System.out.println("application/json");
        System.out.println();
        System.out.println("Content-Length");
        System.out.println("n/a");
        System.out.println();
        System.out.println("Body");
        System.out.println("n/a");
        System.out.println();
    }

    private static void printTransmitBlock() {
        banner("TRANSMIT PATH", C_SECTION);
        System.out.println("sendmsg()");
        System.out.println("|");
        System.out.println("v");
        System.out.println("tcp_sendmsg()");
        System.out.println("|");
        System.out.println("v");
        System.out.println("tcp_write_xmit()");
        System.out.println("|");
        System.out.println("v");
        System.out.println("ip_output()");
        System.out.println("|");
        System.out.println("v");
        System.out.println("dev_queue_xmit()");
        System.out.println("|");
        System.out.println("v");
        System.out.println("NIC TX Queue");
        System.out.println("|");
        System.out.println("v");
        System.out.println("Wire");
        System.out.println();
    }

    private static void printLatencyBreakdown(List<MergedStep> steps,
                                              long kernelNs,
                                              long appNs,
                                              long totalNs) {
        banner("LATENCY BREAKDOWN", C_SUMMARY);

        Map<String, Double> sums = new LinkedHashMap<>();
        sums.put("NIC Receive", 0.0);
        sums.put("IPv4", 0.0);
        sums.put("TCP", 0.0);
        sums.put("Socket Queue", 0.0);
        sums.put("Scheduler", 0.0);
        sums.put("nginx", 0.0);
        sums.put("Spring", 0.0);
        sums.put("PostgreSQL", 0.0);
        sums.put("TCP Send", 0.0);

        for (MergedStep s : steps) {
            String stage = s.stage == null ? "" : s.stage;

            if ("NET_RX".equals(stage) || "NAPI_POLL".equals(stage) || "ETHERNET_RX".equals(stage) || "IRQ_ENTRY".equals(stage) || "SOFTIRQ_ENTRY".equals(stage)) {
                sums.put("NIC Receive", sums.get("NIC Receive") + s.timeUs);
            } else if ("IP_RCV".equals(stage) || "NETFILTER_HOOK".equals(stage) || "ROUTE_LOOKUP".equals(stage)) {
                sums.put("IPv4", sums.get("IPv4") + s.timeUs);
            } else if ("TCP_V4_RCV".equals(stage) || "TCP_STATE_MACHINE".equals(stage)) {
                sums.put("TCP", sums.get("TCP") + s.timeUs);
            } else if ("TCP_DATA_QUEUE".equals(stage) || "SOCK_READABLE".equals(stage) || "RECVFROM_ENTER".equals(stage) || "RECVFROM_EXIT".equals(stage)) {
                sums.put("Socket Queue", sums.get("Socket Queue") + s.timeUs);
            } else if ("SCHED_WAKEUP".equals(stage) || "SCHED_SWITCH".equals(stage)) {
                sums.put("Scheduler", sums.get("Scheduler") + s.timeUs);
            } else if (SECTION_WEB_SERVER.equals(s.section)) {
                sums.put("nginx", sums.get("nginx") + s.timeUs);
            } else if (SECTION_APPLICATION.equals(s.section)) {
                sums.put("Spring", sums.get("Spring") + s.timeUs);
            } else if (SECTION_DATABASE.equals(s.section)) {
                sums.put("PostgreSQL", sums.get("PostgreSQL") + s.timeUs);
            } else if (SECTION_RESPONSE.equals(s.section)) {
                sums.put("TCP Send", sums.get("TCP Send") + s.timeUs);
            }
        }

        for (Map.Entry<String, Double> e : sums.entrySet()) {
            if (e.getValue() <= 0.0) {
                continue;
            }
            System.out.printf("%-24s %s%n", e.getKey(), formatLatencyUs(e.getValue()));
            System.out.println();
        }

        System.out.printf("%-24s %s%n", "Kernel", formatLatencyUs(kernelNs / 1000.0));
        System.out.println();
        System.out.printf("%-24s %.3f ms%n", "Application", appNs / 1_000_000.0);
        System.out.println();
        System.out.printf("%-24s %.3f ms%n", "Total", totalNs / 1_000_000.0);
        System.out.println();
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
                                           List<MergedStep> mergedSteps) {
        String path = outputDir() + "/merged_requests.jsonl";

        try (PrintWriter pw = new PrintWriter(new FileWriter(path, true))) {
            StringBuilder sb = new StringBuilder();
            sb.append('{');
            sb.append("\"id\":").append(id).append(',');
            sb.append("\"uri\":\"").append(jsonEsc(uri)).append("\",");
            sb.append("\"client\":\"").append(jsonEsc(client)).append("\",");
            sb.append("\"server\":\"").append(jsonEsc(server)).append("\",");
            sb.append("\"total_ms\":").append(String.format("%.3f", totalNs / 1_000_000.0)).append(',');
            appendSectionsJson(sb, mergedSteps);
            sb.append(',');
            sb.append("\"steps\":[");

            int written = 0;

            for (MergedStep s : mergedSteps) {
                if (written++ > 0) sb.append(',');
                sb.append('{');
                sb.append("\"n\":").append(s.n).append(',');
                sb.append("\"section\":\"").append(jsonEsc(s.section)).append("\",");
                sb.append("\"layer\":\"").append(jsonEsc(s.layer)).append("\",");
                sb.append("\"stage\":\"").append(jsonEsc(s.stage)).append("\",");
                sb.append("\"desc\":\"").append(jsonEsc(s.desc)).append("\",");
                sb.append("\"process\":\"").append(jsonEsc(s.process)).append("\",");
                if ("kernel".equals(s.layer)) {
                    sb.append("\"delta_us\":").append(String.format("%.3f", s.timeUs));
                } else {
                    sb.append("\"duration_us\":").append(String.format("%.3f", s.timeUs));
                }
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

    private static void appendSectionsJson(StringBuilder sb, List<MergedStep> steps) {
        String[] sections = {
                SECTION_NETWORK,
                SECTION_KERNEL_SOCKET,
                SECTION_WEB_SERVER,
                SECTION_APPLICATION,
                SECTION_DATABASE,
                SECTION_RESPONSE
        };

        sb.append("\"sections\":[");
        int written = 0;

        for (String section : sections) {
            int count = 0;
            double totalUs = 0.0;

            for (MergedStep s : steps) {
                if (section.equals(s.section)) {
                    count++;
                    totalUs += s.timeUs;
                }
            }

            if (written++ > 0) {
                sb.append(',');
            }

            sb.append('{');
            sb.append("\"name\":\"").append(jsonEsc(section)).append("\",");
            sb.append("\"steps\":").append(count).append(',');
            sb.append("\"total_us\":").append(String.format("%.3f", totalUs));
            sb.append('}');
        }

        sb.append(']');
    }

    private static List<MergedStep> buildMergedSteps(List<KernelStep> kernelSteps,
                                                     List<Span> jvmSpans,
                                                     String threadName) {
        List<MergedStep> ordered = new ArrayList<>();
        List<MergedStep> response = new ArrayList<>();

        for (KernelStep ks : kernelSteps) {
            String section = kernelSection(ks.name);
            MergedStep step = MergedStep.kernel(section,
                    ks.name,
                    ks.desc,
                    ks.deltaUs,
                    kernelProcess(ks));
            if (SECTION_RESPONSE.equals(section)) {
                response.add(step);
            } else {
                ordered.add(step);
            }
        }

        for (Span span : jvmSpans) {
            String section = jvmSection(span.stage);
            ordered.add(MergedStep.jvm(section,
                    span.stage,
                    jvmDesc(span.stage, span),
                    span.durationNs() / 1000.0,
                    threadName == null ? "jvm-thread" : threadName,
                    span.sql));
        }

        ordered.addAll(response);

        for (int i = 0; i < ordered.size(); i++) {
            ordered.get(i).n = i + 1;
        }

        return ordered;
    }

    private static String kernelProcess(KernelStep step) {
        if (step.comm != null && !step.comm.isEmpty()) {
            return step.comm;
        }
        return "kernel";
    }

    private static String chooseRequestProcess(List<MergedStep> steps, String fallback) {
        for (MergedStep s : steps) {
            if (s.process != null && !s.process.isEmpty() && !"kernel".equals(s.process)) {
                return s.process;
            }
        }
        for (MergedStep s : steps) {
            if (s.process != null && !s.process.isEmpty()) {
                return s.process;
            }
        }
        return (fallback == null || fallback.isEmpty()) ? "unknown" : fallback;
    }

    private static void banner(String title, String color) {
        String c = color(color, useColor());
        String r = color(C_RESET, useColor());
        System.out.println(c + "=========================================================================================" + r);
        System.out.printf(c + "%s%n" + r, title);
        System.out.println(c + "=========================================================================================" + r);
        System.out.println();
    }

    private static String[] splitHostPort(String value) {
        if (value == null || value.isEmpty() || "?".equals(value)) {
            return new String[] {"?", "?"};
        }

        int idx = value.lastIndexOf(':');
        if (idx <= 0 || idx == value.length() - 1) {
            return new String[] {value, "?"};
        }

        return new String[] {value.substring(0, idx), value.substring(idx + 1)};
    }

    private static String safe(String value, String def) {
        return (value == null || value.isEmpty()) ? def : value;
    }

    private static boolean isDbStage(String stage) {
        return "SQL_QUERY".equals(stage) || "POSTGRESQL".equals(stage);
    }

    private static boolean matchesBlock(String selector, MergedStep step) {
        if ("TCP".equals(selector)) {
            return "TCP_V4_RCV".equals(step.stage)
                    || "TCP_STATE_MACHINE".equals(step.stage)
                    || "TCP_DATA_QUEUE".equals(step.stage)
                    || "TCP_SENDMSG".equals(step.stage)
                    || "TCP_WRITE_XMIT".equals(step.stage);
        }

        if ("SCHEDULER".equals(selector)) {
            return "SCHED_WAKEUP".equals(step.stage) || "SCHED_SWITCH".equals(step.stage);
        }

        return selector.equals(step.section);
    }

    private static int firstCpu(List<KernelStep> steps) {
        for (KernelStep s : steps) {
            if (s.cpu >= 0) {
                return (int)s.cpu;
            }
        }
        return 0;
    }

    private static String firstPid(List<KernelStep> steps) {
        for (KernelStep s : steps) {
            if (s.pid > 0) {
                return String.valueOf(s.pid);
            }
        }
        return "?";
    }

    private static String kernelSection(String eventName) {
        switch (eventName) {
            case "NET_RX":
            case "NAPI_POLL":
            case "ETHERNET_RX":
            case "IRQ_ENTRY":
            case "SOFTIRQ_ENTRY":
            case "IP_RCV":
            case "NETFILTER_HOOK":
            case "ROUTE_LOOKUP":
            case "TCP_V4_RCV":
            case "TCP_STATE_MACHINE":
                return SECTION_NETWORK;

            case "TCP_DATA_QUEUE":
            case "SOCK_READABLE":
            case "SCHED_WAKEUP":
            case "SCHED_SWITCH":
            case "RECVFROM_ENTER":
            case "RECVFROM_EXIT":
                return SECTION_KERNEL_SOCKET;

            case "ACCEPT4_ENTER":
            case "ACCEPT4_EXIT":
            case "NGINX_HTTP_PARSE":
            case "NGINX_REVERSE_PROXY":
            case "NGINX_BACKEND_SOCKET":
                return SECTION_WEB_SERVER;

            case "NGINX_RESPONSE_GEN":
            case "NGINX_RESPONSE_TX":
            case "SENDTO_ENTER":
            case "SENDTO_EXIT":
            case "TCP_SENDMSG":
            case "TCP_WRITE_XMIT":
            case "IP_OUTPUT":
            case "NET_DEV_QUEUE":
                return SECTION_RESPONSE;

            default:
                return SECTION_KERNEL_SOCKET;
        }
    }

    private static String jvmSection(String stage) {
        switch (stage) {
            case "SQL_QUERY":
            case "POSTGRESQL":
                return SECTION_DATABASE;

            default:
                return SECTION_APPLICATION;
        }
    }

    private static String jsonEsc(String s) {
        if (s == null) {
            return "";
        }
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    private static String formatLatencyUs(double us) {
        if (us >= 1000.0) {
            return String.format("%.3f ms", us / 1000.0);
        }
        return String.format("%.3f us", us);
    }

    private static boolean useColor() {
        String env = System.getenv("HTTP_FLOW_COLOR");
        if ("0".equals(env)) {
            return false;
        }
        String term = System.getenv("TERM");
        return term != null && !"dumb".equalsIgnoreCase(term);
    }

    private static String color(String code, boolean enabled) {
        return enabled ? code : "";
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
        String process;
        long processPid;
        String comm;
        long pid;
        long cpu;
    }

    private static class MergedStep {
        int n;
        String section;
        String layer;
        String stage;
        String desc;
        double timeUs;
        String process;
        String sql;

        static MergedStep kernel(String section,
                                 String stage,
                                 String desc,
                                 double deltaUs,
                                 String process) {
            MergedStep step = new MergedStep();
            step.section = section;
            step.layer = "kernel";
            step.stage = stage;
            step.desc = desc;
            step.timeUs = deltaUs;
            step.process = process;
            return step;
        }

        static MergedStep jvm(String section,
                              String stage,
                              String desc,
                              double durationUs,
                              String process,
                              String sql) {
            MergedStep step = new MergedStep();
            step.section = section;
            step.layer = "jvm";
            step.stage = stage;
            step.desc = desc;
            step.timeUs = durationUs;
            step.process = process;
            step.sql = sql;
            return step;
        }
    }

    private static List<KernelStep> readKernelSteps() {
        List<KernelStep> steps = new ArrayList<>();
        String path = outputDir() + "/kernel_request.txt";

        String client = "?";
        String server = "?";
        String socket = "?";
        String process = "?";
        long processPid = 0;
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

                    case "PROCESS":
                        process = p.length > 1 ? p[1] : "?";
                        if (p.length > 2) {
                            processPid = parseLongSafe(p[2]);
                        }
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
                            ks.comm = p.length > 5 ? p[5].trim() : process;
                            ks.pid = p.length > 6 ? parseLongSafe(p[6]) : processPid;
                            ks.cpu = p.length > 7 ? parseLongSafe(p[7]) : 0;
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
            ks.process = process;
            ks.processPid = processPid;
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

    private static long parseLongSafe(String v) {
        try {
            return Long.parseLong(v.trim());
        } catch (Exception ignored) {
            return 0L;
        }
    }
}
