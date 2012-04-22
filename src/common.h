#ifndef PCMA__COMMON_H
#define PCMA__COMMON_H

#include <glib.h>
#include <msgpack.h>
#include <zmq.h>

extern const char *default_ep;

char *raw_to_string(msgpack_object_raw * raw);
void zmq_free_helper(void *data, void *hint);
int pcma_send(void *socket,
    int (*pack_fn) (msgpack_packer *, void *), void *data);
void setup_sig(int signum, void (*sh)(int), int keep_ignoring);

#ifndef g_info
#define g_info(...) g_log (G_LOG_DOMAIN,G_LOG_LEVEL_INFO, __VA_ARGS__)
#endif

#endif /* PCMA__COMMON_H */
