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
    const char *nginx_bin;
    const char *experimental_probes;
    const char *nginx_env;
    int enable_experimental_probes = 0;

    struct ring_buffer *rb;

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);

    skel = tracer_bpf__open();

    if (!skel)
    {
        fprintf(stderr, "Failed to open skeleton\n");
        return 1;
    }

    nginx_env = getenv("HTTP_FLOW_NGINX_BIN");
    nginx_bin = (nginx_env && *nginx_env) ? nginx_env : "/usr/sbin/nginx";

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
            "[http-flow-observer] nginx binary not found at %s; nginx uprobes disabled (set HTTP_FLOW_NGINX_BIN)\n",
                nginx_bin);
    }

    experimental_probes = getenv("HTTP_FLOW_EXPERIMENTAL_PROBES");
    if (experimental_probes && *experimental_probes && experimental_probes[0] != '0')
        enable_experimental_probes = 1;

    /* Some deep kernel hooks are not available on all kernels/distros.
     * Keep them optional so the observer can attach portably. */
    if (!enable_experimental_probes)
    {
        bpf_program__set_autoload(skel->progs.trace_napi_gro_receive, false);
        bpf_program__set_autoload(skel->progs.trace_eth_type_trans, false);
        bpf_program__set_autoload(skel->progs.trace_nf_hook_slow, false);
        bpf_program__set_autoload(skel->progs.trace_ip_route_input_noref, false);

        fprintf(stderr,
                "[http-flow-observer] experimental deep-kernel probes disabled (set HTTP_FLOW_EXPERIMENTAL_PROBES=1 to enable)\n");
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
