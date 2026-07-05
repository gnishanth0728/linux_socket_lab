#include "timeline.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ============================================================
 * Storage
 * ============================================================
 */

#define TIMELINE_TABLE_SIZE 1024

#define OUTPUT_DIR      "output"
#define REQUESTS_JSONL  OUTPUT_DIR "/requests.jsonl"
#define METRICS_JSON    OUTPUT_DIR "/metrics.json"
#define FLAME_FOLDED    OUTPUT_DIR "/flame.folded"
#define SEQUENCE_MMD    OUTPUT_DIR "/sequence.mmd"
#define UI_HTML         OUTPUT_DIR "/index.html"

struct perf_metrics
{
    uint64_t request_count;
    uint64_t total_latency_ns;
    uint64_t max_latency_ns;
    uint64_t min_latency_ns;
    uint64_t total_events;
};

static struct timeline timelines[TIMELINE_TABLE_SIZE];
static struct perf_metrics metrics;

/* ============================================================
 * Basic Helpers
 * ============================================================
 */

static unsigned int hash_socket(uint64_t socket)
{
    return (unsigned int)((socket ^ (socket >> 32)) % TIMELINE_TABLE_SIZE);
}

static struct timeline *timeline_find(uint64_t socket)
{
    unsigned int idx = hash_socket(socket);
    unsigned int start = idx;

    do
    {
        if (timelines[idx].occupied && timelines[idx].socket == socket)
            return &timelines[idx];

        idx = (idx + 1) % TIMELINE_TABLE_SIZE;
    }
    while (idx != start);

    return NULL;
}

static struct timeline *find_or_create(uint64_t socket)
{
    struct timeline *tl = timeline_find(socket);
    unsigned int idx;
    unsigned int start;

    if (tl)
        return tl;

    idx = hash_socket(socket);
    start = idx;

    do
    {
        if (!timelines[idx].occupied)
        {
            memset(&timelines[idx], 0, sizeof(timelines[idx]));
            timelines[idx].socket = socket;
            timelines[idx].occupied = true;
            return &timelines[idx];
        }

        idx = (idx + 1) % TIMELINE_TABLE_SIZE;
    }
    while (idx != start);

    return NULL;
}

static const char *event_name(uint32_t event)
{
    switch (event)
    {
        case EVENT_NET_RX:            return "NET_RX";
        case EVENT_IRQ_ENTRY:         return "IRQ_ENTRY";
        case EVENT_SOFTIRQ_ENTRY:     return "SOFTIRQ_ENTRY";
        case EVENT_IP_RCV:            return "IP_RCV";
        case EVENT_TCP_V4_RCV:        return "TCP_V4_RCV";
        case EVENT_TCP_DATA_QUEUE:    return "TCP_DATA_QUEUE";
        case EVENT_SOCK_DEF_READABLE: return "SOCK_READABLE";
        case EVENT_ACCEPT4_ENTER:     return "ACCEPT4_ENTER";
        case EVENT_ACCEPT4_EXIT:      return "ACCEPT4_EXIT";
        case EVENT_RECVFROM_ENTER:    return "RECVFROM_ENTER";
        case EVENT_RECVFROM_EXIT:     return "RECVFROM_EXIT";
        case EVENT_SENDTO_ENTER:      return "SENDTO_ENTER";
        case EVENT_SENDTO_EXIT:       return "SENDTO_EXIT";
        case EVENT_TCP_SENDMSG:       return "TCP_SENDMSG";
        case EVENT_TCP_WRITE_XMIT:    return "TCP_WRITE_XMIT";
        case EVENT_IP_OUTPUT:         return "IP_OUTPUT";
        case EVENT_NET_DEV_QUEUE:     return "NET_DEV_QUEUE";
        default:                      return "UNKNOWN";
    }
}

static const char *event_desc(uint32_t event)
{
    switch (event)
    {
        case EVENT_NET_RX:            return "NIC receives Ethernet frame";
        case EVENT_IRQ_ENTRY:         return "CPU enters IRQ handler";
        case EVENT_SOFTIRQ_ENTRY:     return "SoftIRQ (NET_RX_SOFTIRQ) scheduled";
        case EVENT_IP_RCV:            return "IPv4 layer validates header";
        case EVENT_TCP_V4_RCV:        return "TCP layer finds destination socket";
        case EVENT_TCP_DATA_QUEUE:    return "Socket receive queue copies payload";
        case EVENT_SOCK_DEF_READABLE: return "Socket marked readable, recv() wakes up";
        case EVENT_ACCEPT4_ENTER:     return "accept() called by application";
        case EVENT_ACCEPT4_EXIT:      return "accept() returned new socket fd";
        case EVENT_RECVFROM_ENTER:    return "recv() called, waiting for HTTP request";
        case EVENT_RECVFROM_EXIT:     return "recv() returned HTTP request data";
        case EVENT_SENDTO_ENTER:      return "send() called with HTTP response";
        case EVENT_SENDTO_EXIT:       return "send() returned";
        case EVENT_TCP_SENDMSG:       return "TCP transmit path entered";
        case EVENT_TCP_WRITE_XMIT:    return "TCP segments enqueued for transmit";
        case EVENT_IP_OUTPUT:         return "IPv4 routes and transmits packet";
        case EVENT_NET_DEV_QUEUE:     return "NIC transmit queue enqueued";
        default:                      return "kernel event";
    }
}

static void ip_to_str(uint32_t addr, char *buf)
{
    sprintf(buf,
            "%u.%u.%u.%u",
            addr & 0xff,
            (addr >> 8) & 0xff,
            (addr >> 16) & 0xff,
            (addr >> 24) & 0xff);
}

static uint64_t timeline_total_ns(const struct timeline *tl)
{
    if (!tl || tl->count < 2)
        return 0;

    return tl->entries[tl->count - 1].e.timestamp - tl->entries[0].e.timestamp;
}

static int ensure_output_dir(void)
{
    struct stat st;

    if (stat(OUTPUT_DIR, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return 0;

        fprintf(stderr, "[timeline] '%s' exists but is not a directory\n", OUTPUT_DIR);
        return -1;
    }

    if (mkdir(OUTPUT_DIR, 0755) == 0)
        return 0;

    if (errno == EEXIST)
        return 0;

    fprintf(stderr, "[timeline] failed to create '%s': %s\n", OUTPUT_DIR, strerror(errno));
    return -1;
}

static void truncate_file(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return;

    fclose(f);
}

static void json_escape(const char *src, char *dst, size_t dst_len)
{
    size_t i = 0;

    if (!dst || dst_len == 0)
        return;

    dst[0] = '\0';

    if (!src)
        return;

    while (*src && i + 1 < dst_len)
    {
        char ch = *src++;

        if ((ch == '\\' || ch == '"') && i + 2 < dst_len)
        {
            dst[i++] = '\\';
            dst[i++] = ch;
            continue;
        }

        if ((unsigned char)ch < 0x20)
            continue;

        dst[i++] = ch;
    }

    dst[i] = '\0';
}

static const char *seq_actor(uint32_t event)
{
    switch (event)
    {
        case EVENT_NET_RX:
        case EVENT_IP_RCV:
        case EVENT_TCP_V4_RCV:
            return "NET";

        case EVENT_IRQ_ENTRY:
            return "IRQ";

        case EVENT_SOFTIRQ_ENTRY:
            return "SOFTIRQ";

        case EVENT_TCP_DATA_QUEUE:
        case EVENT_SOCK_DEF_READABLE:
            return "KERNEL";

        case EVENT_RECVFROM_ENTER:
        case EVENT_RECVFROM_EXIT:
        case EVENT_SENDTO_ENTER:
        case EVENT_SENDTO_EXIT:
        case EVENT_ACCEPT4_ENTER:
        case EVENT_ACCEPT4_EXIT:
            return "APP";

        case EVENT_TCP_SENDMSG:
        case EVENT_TCP_WRITE_XMIT:
        case EVENT_IP_OUTPUT:
        case EVENT_NET_DEV_QUEUE:
            return "TX";

        default:
            return "KERNEL";
    }
}

/* ============================================================
 * Build 6 Export
 * ============================================================
 */

static void update_metrics(const struct timeline *tl)
{
    uint64_t total_ns;

    if (!tl)
        return;

    total_ns = timeline_total_ns(tl);

    metrics.request_count++;
    metrics.total_latency_ns += total_ns;
    metrics.total_events += tl->count;

    if (metrics.request_count == 1 || total_ns < metrics.min_latency_ns)
        metrics.min_latency_ns = total_ns;

    if (total_ns > metrics.max_latency_ns)
        metrics.max_latency_ns = total_ns;
}

static void export_metrics_json(void)
{
    FILE *f;
    double avg_us = 0.0;

    if (metrics.request_count > 0)
        avg_us = (double)metrics.total_latency_ns / (double)metrics.request_count / 1000.0;

    f = fopen(METRICS_JSON, "w");
    if (!f)
        return;

    fprintf(f, "{\n");
    fprintf(f, "  \"requests\": %" PRIu64 ",\n", metrics.request_count);
    fprintf(f, "  \"avg_latency_us\": %.3f,\n", avg_us);
    fprintf(f, "  \"max_latency_us\": %.3f,\n", (double)metrics.max_latency_ns / 1000.0);
    fprintf(f, "  \"min_latency_us\": %.3f,\n", (double)metrics.min_latency_ns / 1000.0);
    fprintf(f, "  \"total_events\": %" PRIu64 "\n", metrics.total_events);
    fprintf(f, "}\n");

    fclose(f);
}

static void export_request_jsonl(const struct timeline *tl)
{
    FILE *f;
    unsigned int i;
    uint64_t total_ns;

    if (!tl || tl->count == 0)
        return;

    f = fopen(REQUESTS_JSONL, "a");
    if (!f)
        return;

    total_ns = timeline_total_ns(tl);

    fprintf(f,
            "{\"socket\":\"0x%llx\",\"events\":%u,\"total_us\":%.3f,\"timeline\":[",
            (unsigned long long)tl->socket,
            tl->count,
            (double)total_ns / 1000.0);

    for (i = 0; i < tl->count; i++)
    {
        const struct timeline_entry *te = &tl->entries[i];
        const struct event *ev = &te->e;
        char comm_esc[64];

        json_escape(ev->comm, comm_esc, sizeof(comm_esc));

        if (i > 0)
            fprintf(f, ",");

        fprintf(f,
                "{\"idx\":%u,\"name\":\"%s\",\"delta_us\":%.3f,\"pid\":%u,\"cpu\":%u,\"comm\":\"%s\"}",
                i + 1,
                event_name(ev->event),
                (double)te->delta_ns / 1000.0,
                ev->pid,
                ev->cpu,
                comm_esc);
    }

    fprintf(f, "]}\n");
    fclose(f);
}

static void export_flame_folded(const struct timeline *tl)
{
    FILE *f;
    unsigned int i;

    if (!tl || tl->count == 0)
        return;

    f = fopen(FLAME_FOLDED, "a");
    if (!f)
        return;

    for (i = 0; i < tl->count; i++)
    {
        const struct timeline_entry *te = &tl->entries[i];
        const char *name = event_name(te->e.event);
        uint64_t value_us = te->delta_ns / 1000;

        if (value_us == 0)
            value_us = 1;

        fprintf(f,
                "request_0x%llx;%s %" PRIu64 "\n",
                (unsigned long long)tl->socket,
                name,
                value_us);
    }

    fclose(f);
}

static void export_sequence_mmd(const struct timeline *tl)
{
    FILE *f;
    unsigned int i;

    if (!tl || tl->count < 2)
        return;

    f = fopen(SEQUENCE_MMD, "a");
    if (!f)
        return;

    fprintf(f, "\n    %% request socket=0x%llx\n", (unsigned long long)tl->socket);

    for (i = 1; i < tl->count; i++)
    {
        const struct event *prev = &tl->entries[i - 1].e;
        const struct event *cur = &tl->entries[i].e;
        const char *from = seq_actor(prev->event);
        const char *to = seq_actor(cur->event);

        fprintf(f,
                "    %s->>%s: %s (%.3f us)\n",
                from,
                to,
                event_name(cur->event),
                (double)tl->entries[i].delta_ns / 1000.0);
    }

    fclose(f);
}

static void write_ui_html(void)
{
    FILE *f = fopen(UI_HTML, "w");

    if (!f)
        return;

    /* ---- HTML + CSS ---- */
    fprintf(f,
            "<!doctype html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "  <meta charset=\"utf-8\">\n"
            "  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
            "  <title>HTTP Flow Live UI</title>\n"
            "  <style>\n"
            "    :root{--bg:#0b1220;--card:#111a2c;--line:#223555;--txt:#e8eefc;"
            "--muted:#9eb0cf;--ok:#3ddc97;--warn:#ffcc66;--k:#7eb8f7;--j:#a78bfa;}\n"
            "    *{box-sizing:border-box;}\n"
            "    body{margin:0;font-family:'IBM Plex Sans','Segoe UI',sans-serif;"
            "background:radial-gradient(circle at top right,#1a2a4a 0,#0b1220 55%%);"
            "color:var(--txt);}\n"
            "    .wrap{max-width:960px;margin:24px auto;padding:0 16px;}\n"
            "    h1{margin:0 0 4px;font-size:26px;}\n"
            "    .sub{color:var(--muted);font-size:13px;margin-bottom:16px;}\n"
            "    .row{display:grid;gap:10px;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));margin-bottom:14px;}\n"
            "    .card{background:linear-gradient(180deg,#15213a,#10182a);"
            "border:1px solid var(--line);border-radius:12px;padding:14px;}\n"
            "    .label{color:var(--muted);font-size:11px;text-transform:uppercase;letter-spacing:.7px;}\n"
            "    .value{font-size:22px;margin-top:5px;color:var(--ok);}\n"
            "    .panel{background:var(--card);border:1px solid var(--line);"
            "border-radius:12px;padding:14px;margin-bottom:14px;}\n"
            "    .ptitle{color:var(--muted);font-size:11px;text-transform:uppercase;"
            "letter-spacing:.7px;margin-bottom:10px;}\n"
            "    .step{display:flex;align-items:flex-start;gap:10px;padding:7px 0;"
            "border-bottom:1px solid var(--line);}\n"
            "    .step:last-child{border-bottom:none;}\n"
            "    .snum{min-width:30px;text-align:right;color:var(--muted);font-size:12px;padding-top:1px;}\n"
            "    .sdetail{flex:1;}\n"
            "    .sdesc{font-size:13px;font-weight:600;}\n"
            "    .smeta{font-size:11px;color:var(--muted);margin-top:2px;}\n"
            "    .badge-k{display:inline-block;font-size:10px;padding:1px 6px;"
            "border-radius:4px;background:#1e3a5f;color:var(--k);margin-right:6px;}\n"
            "    .badge-j{display:inline-block;font-size:10px;padding:1px 6px;"
            "border-radius:4px;background:#2d1f4a;color:var(--j);margin-right:6px;}\n"
            "    .toolbar{display:flex;gap:8px;align-items:center;margin:8px 0 10px;}\n"
            "    .btn{border:1px solid var(--line);background:#15213a;color:var(--txt);"
            "padding:5px 10px;border-radius:8px;cursor:pointer;font-size:12px;}\n"
            "    .btn:hover{filter:brightness(1.08);}\n"
            "    .btn:disabled{opacity:.45;cursor:not-allowed;}\n"
            "    .hint{color:var(--muted);font-size:12px;}\n"
            "    .step.done{opacity:.72;}\n"
            "    .step.active{border-left:3px solid var(--warn);padding-left:7px;background:#15213a;}\n"
            "    .empty{color:var(--muted);font-size:13px;text-align:center;padding:20px;}\n"
            "  </style>\n"
            "</head>\n"
            "<body>\n"
            "  <div class=\"wrap\">\n"
            "    <h1>HTTP Flow Observer</h1>\n"
            "    <div class=\"sub\">Complete OS + JVM request path &mdash; auto-refreshes every 1 s</div>\n");

    /* ---- metric cards ---- */
    fprintf(f,
            "    <div class=\"row\">\n"
            "      <div class=\"card\"><div class=\"label\">Requests</div>"
            "<div class=\"value\" id=\"req\">0</div></div>\n"
            "      <div class=\"card\"><div class=\"label\">Avg Latency (us)</div>"
            "<div class=\"value\" id=\"avg\">0</div></div>\n"
            "      <div class=\"card\"><div class=\"label\">Max Latency (us)</div>"
            "<div class=\"value\" id=\"max\">0</div></div>\n"
            "      <div class=\"card\"><div class=\"label\">Total Events</div>"
            "<div class=\"value\" id=\"evt\">0</div></div>\n"
            "    </div>\n");

    /* ---- latest request step panel ---- */
    fprintf(f,
            "    <div class=\"panel\">\n"
            "      <div class=\"ptitle\">Latest Request &mdash; complete network path</div>\n"
            "      <div id=\"reqmeta\" class=\"smeta\" style=\"margin-bottom:8px\"></div>\n"
            "      <div class=\"toolbar\">\n"
            "        <button class=\"btn\" id=\"prevBtn\">Previous</button>\n"
            "        <button class=\"btn\" id=\"nextBtn\">Next</button>\n"
            "        <button class=\"btn\" id=\"resetBtn\">Reset</button>\n"
            "        <span id=\"playHint\" class=\"hint\">waiting for first request...</span>\n"
            "      </div>\n"
            "      <div id=\"steps\"><div class=\"empty\">waiting for first request...</div></div>\n"
            "    </div>\n");

    /* ---- JavaScript ---- */
    fprintf(f,
            "  </div>\n"
            "  <script>\n"
            "    let latestReq = null;\n"
            "    let playbackIndex = -1;\n"
            "\n"
            "    function safeNum(x, d=0) {\n"
            "      const n = Number(x);\n"
            "      return Number.isFinite(n) ? n : d;\n"
            "    }\n"
            "\n"
            "    function renderPlayback() {\n"
            "      const stepsEl = document.getElementById('steps');\n"
            "      const hintEl  = document.getElementById('playHint');\n"
            "      const prevBtn = document.getElementById('prevBtn');\n"
            "      const nextBtn = document.getElementById('nextBtn');\n"
            "      const resetBtn= document.getElementById('resetBtn');\n"
            "\n"
            "      if (!latestReq || !Array.isArray(latestReq.steps) || !latestReq.steps.length) {\n"
            "        stepsEl.innerHTML = '<div class=\"empty\">waiting for first request...</div>';\n"
            "        hintEl.textContent = 'waiting for first request...';\n"
            "        prevBtn.disabled = true; nextBtn.disabled = true; resetBtn.disabled = true;\n"
            "        return;\n"
            "      }\n"
            "\n"
            "      const steps = latestReq.steps;\n"
            "      const maxIndex = steps.length - 1;\n"
            "      if (playbackIndex < 0) playbackIndex = 0;\n"
            "      if (playbackIndex > maxIndex) playbackIndex = maxIndex;\n"
            "\n"
            "      prevBtn.disabled = playbackIndex <= 0;\n"
            "      nextBtn.disabled = playbackIndex >= maxIndex;\n"
            "      resetBtn.disabled = steps.length === 0;\n"
            "\n"
            "      hintEl.textContent = 'Showing step ' + (playbackIndex + 1) + ' of ' + steps.length;\n"
            "\n"
            "      stepsEl.innerHTML = steps.map((s, idx) => {\n"
            "        const badge = s.layer === 'kernel'\n"
            "          ? '<span class=\\\"badge-k\\\">KERNEL</span>'\n"
            "          : '<span class=\\\"badge-j\\\">JVM</span>';\n"
            "        const timing = s.layer === 'kernel'\n"
            "          ? (safeNum(s.delta_us) > 0 ? '+' + safeNum(s.delta_us).toFixed(3) + ' us' : 'start')\n"
            "          : safeNum(s.duration_us).toFixed(3) + ' us';\n"
            "        const sql = s.sql ? '<br><span style=\\\"color:#7dd3fc\\\">SQL: ' + s.sql + '</span>' : '';\n"
            "\n"
            "        let cls = 'step';\n"
            "        if (idx < playbackIndex) cls += ' done';\n"
            "        if (idx === playbackIndex) cls += ' active';\n"
            "\n"
            "        return '<div class=\\\"' + cls + '\\\">'\n"
            "          + '<div class=\\\"snum\\\">' + s.n + '</div>'\n"
            "          + '<div class=\\\"sdetail\\\">'\n"
            "          + '<div class=\\\"sdesc\\\">' + badge + s.desc + '</div>'\n"
            "          + '<div class=\\\"smeta\\\">' + (s.method || s.stage) + '&nbsp;&nbsp;' + timing + sql + '</div>'\n"
            "          + '</div></div>';\n"
            "      }).join('');\n"
            "    }\n"
            "\n"
            "    async function refresh() {\n"
            "      try {\n"
            "        const m = await fetch('metrics.json?ts='+Date.now());\n"
            "        if (m.ok) {\n"
            "          const j = await m.json();\n"
            "          document.getElementById('req').textContent = j.requests ?? 0;\n"
            "          document.getElementById('avg').textContent ="
            " Number(j.avg_latency_us ?? 0).toFixed(2);\n"
            "          document.getElementById('max').textContent ="
            " Number(j.max_latency_us ?? 0).toFixed(2);\n"
            "          document.getElementById('evt').textContent = j.total_events ?? 0;\n"
            "        }\n"
            "      } catch (_) {}\n"
            "      try {\n"
            "        const r = await fetch('merged_requests.jsonl?ts='+Date.now());\n"
            "        if (r.ok) {\n"
            "          const txt = await r.text();\n"
            "          const lines = txt.trim().split(/\\n/).filter(Boolean);\n"
            "          if (!lines.length) { renderPlayback(); return; }\n"
            "          const req = JSON.parse(lines[lines.length - 1]);\n"
            "          document.getElementById('reqmeta').textContent =\n"
            "            'Request #' + req.id + '  ' + req.uri +\n"
            "            '   Client: ' + req.client + '  Server: ' + req.server +\n"
            "            '   Total: ' + req.total_ms + ' ms';\n"
            "          if (!latestReq || latestReq.id !== req.id) {\n"
            "            latestReq = req;\n"
            "            playbackIndex = 0;\n"
            "          } else {\n"
            "            latestReq = req;\n"
            "          }\n"
            "          renderPlayback();\n"
            "        }\n"
            "      } catch (_) {}\n"
            "    }\n"
            "\n"
            "    document.getElementById('prevBtn').addEventListener('click', () => {\n"
            "      playbackIndex = Math.max(0, playbackIndex - 1);\n"
            "      renderPlayback();\n"
            "    });\n"
            "\n"
            "    document.getElementById('nextBtn').addEventListener('click', () => {\n"
            "      if (!latestReq || !latestReq.steps) return;\n"
            "      playbackIndex = Math.min(latestReq.steps.length - 1, playbackIndex + 1);\n"
            "      renderPlayback();\n"
            "    });\n"
            "\n"
            "    document.getElementById('resetBtn').addEventListener('click', () => {\n"
            "      playbackIndex = 0;\n"
            "      renderPlayback();\n"
            "    });\n"
            "\n"
            "    refresh();\n"
            "    setInterval(refresh, 1000);\n"
            "  </script>\n"
            "</body>\n"
            "</html>\n");

    fclose(f);
}

static void export_build6_artifacts(const struct timeline *tl)
{
    update_metrics(tl);
    export_metrics_json();
    export_request_jsonl(tl);
    export_flame_folded(tl);
    export_sequence_mmd(tl);
}

/* ============================================================
 * Kernel Handoff File  (read by Java agent for merged view)
 * ============================================================
 *
 * Written to output/kernel_request.txt when timeline_finish()
 * fires (TCP_SENDMSG).  The Java agent reads this file at the
 * end of Tomcat.invoke() to prepend kernel steps to its output.
 *
 * Format (pipe-delimited for easy Java parsing):
 *   CLIENT|<ip>:<port>
 *   SERVER|<ip>:<port>
 *   SOCKET|0x<hex>
 *   STEP|<n>|<event_name>|<description>|<delta_ns>
 *   ...
 *   TOTAL_NS|<total>
 *   COUNT|<n>
 * ============================================================
 */

#define KERNEL_REQUEST_FILE OUTPUT_DIR "/kernel_request.txt"

static void write_kernel_request_file(const struct timeline *tl)
{
    FILE *f;
    unsigned int i;
    char src[32];
    char dst[32];
    uint64_t total_ns;
    const struct event *first;

    if (!tl || tl->count == 0)
        return;

    f = fopen(KERNEL_REQUEST_FILE, "w");
    if (!f)
        return;

    first = &tl->entries[0].e;
    ip_to_str(first->saddr, src);
    ip_to_str(first->daddr, dst);
    total_ns = timeline_total_ns(tl);

    fprintf(f, "CLIENT|%s:%u\n", src, first->sport);
    fprintf(f, "SERVER|%s:%u\n", dst, first->dport);
    fprintf(f, "SOCKET|0x%llx\n", (unsigned long long)tl->socket);

    for (i = 0; i < tl->count; i++)
    {
        const struct timeline_entry *te = &tl->entries[i];

        fprintf(f, "STEP|%u|%s|%s|%llu\n",
                i + 1,
                event_name(te->e.event),
                event_desc(te->e.event),
                (unsigned long long)te->delta_ns);
    }

    fprintf(f, "TOTAL_NS|%llu\n", (unsigned long long)total_ns);
    fprintf(f, "COUNT|%u\n", tl->count);

    fclose(f);
}

/* ============================================================
 * Timeline mechanics
 * ============================================================
 */

static void append(struct timeline *tl, const struct event *e)
{
    struct timeline_entry *te;

    if (tl->count >= TIMELINE_MAX_EVENTS)
        return;

    te = &tl->entries[tl->count];
    te->e = *e;

    if (tl->count == 0)
        te->delta_ns = 0;
    else
        te->delta_ns = e->timestamp - tl->entries[tl->count - 1].e.timestamp;

    tl->count++;
}

void timeline_init(void)
{
    memset(timelines, 0, sizeof(timelines));
    memset(&metrics, 0, sizeof(metrics));

    if (ensure_output_dir() == 0)
    {
        truncate_file(REQUESTS_JSONL);
        truncate_file(FLAME_FOLDED);
        truncate_file(METRICS_JSON);
        truncate_file(SEQUENCE_MMD);

        write_ui_html();

        {
            FILE *seq = fopen(SEQUENCE_MMD, "w");
            if (seq)
            {
                fprintf(seq, "sequenceDiagram\n");
                fprintf(seq, "    autonumber\n");
                fprintf(seq, "    participant APP\n");
                fprintf(seq, "    participant NET\n");
                fprintf(seq, "    participant IRQ\n");
                fprintf(seq, "    participant SOFTIRQ\n");
                fprintf(seq, "    participant KERNEL\n");
                fprintf(seq, "    participant TX\n");
                fclose(seq);
            }
        }
    }
}

void timeline_start(uint64_t socket, const struct event *e)
{
    struct timeline *tl;

    if (!socket || !e)
        return;

    tl = find_or_create(socket);
    if (!tl)
        return;

    tl->count = 0;
    tl->start_ts = e->timestamp;
    tl->active = true;

    append(tl, e);
}

void timeline_add_event(uint64_t socket, const struct event *e)
{
    struct timeline *tl;

    if (!socket || !e)
        return;

    tl = timeline_find(socket);
    if (!tl || !tl->active)
        return;

    append(tl, e);
}

void timeline_finish(uint64_t socket, const struct event *e)
{
    struct timeline *tl;
    const char      *min_env;
    unsigned int     min_steps = 3;   /* skip noise like 1-2 event internal conns */

    if (!socket || !e)
        return;

    tl = timeline_find(socket);
    if (!tl || !tl->active)
        return;

    append(tl, e);

    /* allow override via env: HTTP_FLOW_MIN_STEPS=1 to see everything */
    min_env = getenv("HTTP_FLOW_MIN_STEPS");
    if (min_env && *min_env)
    {
        int v = atoi(min_env);
        if (v >= 1)
            min_steps = (unsigned int)v;
    }

    if (tl->count < min_steps)
    {
        /* too few events — internal/background connection, silently discard */
        tl->active = false;
        tl->count  = 0;
        return;
    }

    write_kernel_request_file(tl);
    timeline_print(socket);
    export_build6_artifacts(tl);

    tl->active = false;
    tl->count = 0;
}

void timeline_print(uint64_t socket)
{
    struct timeline *tl = timeline_find(socket);
    unsigned int i;
    uint64_t total = 0;
    char src[32];
    char dst[32];
    uint32_t sa = 0, da = 0;
    uint16_t sp = 0, dp = 0;
    const char *comm;

    if (!tl || tl->count == 0)
        return;

    /* find first entry that has valid IP addresses */
    for (i = 0; i < tl->count; i++)
    {
        const struct event *ev = &tl->entries[i].e;
        if (ev->saddr || ev->daddr)
        {
            sa = ev->saddr; da = ev->daddr;
            sp = ev->sport; dp = ev->dport;
            break;
        }
    }

    ip_to_str(sa, src);
    ip_to_str(da, dst);
    comm = tl->entries[0].e.comm;

    printf("\n");
    printf("========================================================\n");
    printf("KERNEL REQUEST  socket=0x%llx  pid=%-6u  proc=%s\n",
           (unsigned long long)socket,
           tl->entries[0].e.pid,
           (comm && comm[0]) ? comm : "?");
    printf("Client : %s:%-5u    Server : %s:%u\n",
           src, sp, dst, dp);
    printf("--------------------------------------------------------\n");

    for (i = 0; i < tl->count; i++)
    {
        const struct timeline_entry *te = &tl->entries[i];
        const struct event *ev = &te->e;

        if (i == 0)
            printf("[✓] STEP %-2u  %-38s\n",
                   i + 1, event_desc(ev->event));
        else
            printf("[✓] STEP %-2u  %-38s  +%.3f us\n",
                   i + 1, event_desc(ev->event),
                   (double)te->delta_ns / 1000.0);
    }

    if (tl->count > 1)
        total = tl->entries[tl->count - 1].e.timestamp
              - tl->entries[0].e.timestamp;

    printf("\n");
    printf("Kernel path : %.3f us  (%u steps)\n",
           (double)total / 1000.0, tl->count);
    printf("========================================================\n\n");
}

void timeline_clear(uint64_t socket)
{
    struct timeline *tl = timeline_find(socket);

    if (!tl)
        return;

    tl->count = 0;
    tl->active = false;
    tl->start_ts = 0;
}
