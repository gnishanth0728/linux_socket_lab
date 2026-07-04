package com.lab.agent;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;

final class FlowTimeline {
    private boolean active;
    private long requestId;
    private long startNs;
    private long endNs;
    private long threadId;
    private String threadName;

    private final Deque<Span> stack = new ArrayDeque<>();
    private final List<Span> spans = new ArrayList<>();

    void start(long requestId) {
        this.active = true;
        this.requestId = requestId;
        this.startNs = System.nanoTime();
        this.endNs = 0L;
        this.threadId = Thread.currentThread().getId();
        this.threadName = Thread.currentThread().getName();
        this.stack.clear();
        this.spans.clear();
    }

    void finish() {
        this.endNs = System.nanoTime();
        this.active = false;
    }

    boolean isActive() {
        return active;
    }

    long requestId() {
        return requestId;
    }

    long startNs() {
        return startNs;
    }

    long endNs() {
        return endNs;
    }

    long totalNs() {
        long effectiveEnd = (endNs == 0L) ? System.nanoTime() : endNs;
        return Math.max(0L, effectiveEnd - startNs);
    }

    long threadId() {
        return threadId;
    }

    String threadName() {
        return threadName;
    }

    int depth() {
        return stack.size();
    }

    void push(Span span) {
        stack.push(span);
    }

    Span pop(String className, String methodName) {
        if (stack.isEmpty()) {
            return null;
        }

        Span top = stack.peek();
        if (top.className.equals(className) && top.methodName.equals(methodName)) {
            return stack.pop();
        }

        return stack.pop();
    }

    void add(Span span) {
        spans.add(span);
    }

    List<Span> spans() {
        return spans;
    }

    void reset() {
        active = false;
        requestId = 0L;
        startNs = 0L;
        endNs = 0L;
        threadId = 0L;
        threadName = null;
        stack.clear();
        spans.clear();
    }
}
