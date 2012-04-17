#include <msgpack.h>
#include <string.h>
#include <zmq.h>
#include "macros.h"

int log_level;
char *default_ep = "ipc:///tmp/pcma.socket";

char *raw_to_string(msgpack_object_raw * raw)
{
    char *res = malloc(raw->size + 1);

    if (res) {
        memcpy(res, raw->ptr, raw->size);
        res[raw->size] = '\0';
    }
    return res;
}

void zmq_free_helper(void *data, void *hint)
{
    if (hint)
        msgpack_packer_free(hint);
}

int pcma_send(void *socket,
               int (*pack_fn) (msgpack_packer *, void *), void *data)
{
    zmq_msg_t msg;

    msgpack_sbuffer *buffer = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    if (!(buffer && pk)) {
        LOG_ERROR("msgpack init failed in pcma_send\n");
        return (-1);
    }

    if (pack_fn(pk, data) < 0) {
        LOG_ERROR("pack function failed in pcma_send\n");
        return (-2);
    }

    msgpack_packer_free(pk);

    if (zmq_msg_init_data(&msg,
                          (void *) buffer->data, buffer->size,
                          zmq_free_helper, buffer) < 0) {
        perror("zmq_msg_init_data");
        LOG_ERROR("pcma_send could not proceed\n");
        return (-3);
    }

    if (zmq_send(socket, &msg, 0) < 0) {
        perror("zmq_send");
        LOG_ERROR("pcma_send could not proceed\n");
        return (-4);
    }

    if (zmq_msg_close(&msg) < 0) {
        perror("zmq_msg_close");
        LOG_ERROR("pcma_send is likely leaking\n");
    }

    return (0);
}
