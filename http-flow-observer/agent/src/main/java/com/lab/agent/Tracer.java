package com.lab.agent;

import java.util.List;
import java.util.concurrent.atomic.AtomicLong;

public final class Tracer {
    private static final String TOMCAT = "TOMCAT";
    private static final String SPRING_MVC = "SPRING_MVC";
    private static final String SQL_QUERY = "SQL_QUERY";
    private static final String POSTGRESQL = "POSTGRESQL";
    private static final String CONNECTION_POOL = "CONNECTION_POOL";

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
