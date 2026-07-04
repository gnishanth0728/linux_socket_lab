package com.lab.agent;

final class Span {
    final String stage;
    final String className;
    final String methodName;
    final String methodDesc;
    final String sql;
    final int depth;
    final long startNs;

    long endNs;
    boolean threw;

    Span(String stage,
         String className,
         String methodName,
         String methodDesc,
         String sql,
         int depth,
         long startNs) {
        this.stage = stage;
        this.className = className;
        this.methodName = methodName;
        this.methodDesc = methodDesc;
        this.sql = sql;
        this.depth = depth;
        this.startNs = startNs;
    }

    void finish(long endNs, boolean threw) {
        this.endNs = endNs;
        this.threw = threw;
    }

    long durationNs() {
        return Math.max(0L, endNs - startNs);
    }
}
