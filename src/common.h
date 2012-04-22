#ifndef PCMA__COMMON_H
#define PCMA__COMMON_H

#include <msgpack.h>
#include <zmq.h>
#include "macros.h"

extern const char *default_ep;

char *raw_to_string(msgpack_object_raw * raw);
void zmq_free_helper(void *data, void *hint);
int pcma_send(void *socket,
    int (*pack_fn) (msgpack_packer *, void *), void *data);

#endif /* PCMA__COMMON_H */
