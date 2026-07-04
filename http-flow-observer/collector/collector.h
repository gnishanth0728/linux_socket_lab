#ifndef HTTP_FLOW_COLLECTOR_H
#define HTTP_FLOW_COLLECTOR_H

#include <bpf/libbpf.h>

/*
 * Ring-buffer callback — pass to ring_buffer__new().
 */
int collector_handle_event(void *ctx, void *data, size_t len);

/*
 * Main event loop.  Blocks until SIGINT / SIGTERM.
 * Returns 0 on clean shutdown.
 */
int collector_run(struct ring_buffer *rb);

#endif
