#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bpf/libbpf.h>

#include "kernel/tracer.skel.h"
#include "collector/collector.h"

/* ============================================================
 * Main
 * ============================================================
 */

int main(void)
{
    struct tracer_bpf *skel;
    const char *nginx_bin = "/usr/sbin/nginx";

    struct ring_buffer *rb;

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    skel = tracer_bpf__open();

    if (!skel)
    {
        fprintf(stderr, "Failed to open skeleton\n");
        return 1;
    }

    /* Disable nginx uprobes when nginx is not present at the hardcoded path,
     * so core kernel tracing can still run. */
    if (access(nginx_bin, F_OK) != 0)
    {
        bpf_program__set_autoload(skel->progs.uprobe_ngx_http_process_request_line, false);
        bpf_program__set_autoload(skel->progs.uprobe_ngx_http_upstream_init_request, false);
        bpf_program__set_autoload(skel->progs.uprobe_ngx_http_upstream_send_request, false);
        bpf_program__set_autoload(skel->progs.uprobe_ngx_http_finalize_request, false);
        bpf_program__set_autoload(skel->progs.uprobe_ngx_http_writer, false);

        fprintf(stderr,
                "[http-flow-observer] nginx binary not found at %s; nginx uprobes disabled\n",
                nginx_bin);
    }

    if (tracer_bpf__load(skel))
    {
        fprintf(stderr, "Failed to load BPF\n");
        return 1;
    }

    if (tracer_bpf__attach(skel))
    {
        fprintf(stderr, "Failed to attach\n");
        return 1;
    }

    rb = ring_buffer__new(
            bpf_map__fd(skel->maps.events),
            collector_handle_event,
            NULL,
            NULL);

    if (!rb)
    {
        fprintf(stderr, "Failed to create ring buffer\n");
        return 1;
    }

    printf("\n");
    printf("===============================================\n");
    printf("HTTP FLOW OBSERVER\n");
    printf("===============================================\n\n");

    collector_run(rb);

    ring_buffer__free(rb);

    tracer_bpf__destroy(skel);

    return 0;
}
