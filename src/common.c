#include <glib.h>
#include <msgpack.h>
#include <signal.h>
#include <string.h>
#include <zmq.h>

const char *default_ep = "ipc:///var/run/pcma.socket";

void setup_sig(int signum, void (*sh) (int), int keep_ignoring)
{
    struct sigaction new, old;

    sigemptyset(&new.sa_mask);
    new.sa_flags = 0;
    new.sa_handler = sh;

    if (sigaction(signum, NULL, &old) < 0)
        g_error("sigaction(%i,old): %s", signum, strerror(errno));

    if (keep_ignoring && old.sa_handler == SIG_IGN) {
        g_debug("ignoring signal %i", signum);
    } else {
        if (sigaction(signum, &new, NULL) < 0)
            g_error("sigaction(%i,new): %s", signum, strerror(errno));
    }
}

char *raw_to_string(msgpack_object_raw * raw)
{
    char *res = malloc(raw->size + 1);

    if (res) {
        memcpy(res, raw->ptr, raw->size);
        res[raw->size] = '\0';
    } else {
        g_critical("raw_to_string: %s", strerror(errno));
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

    if (!buffer) {
        g_critical("pcma_send: msgpack_sbuffer_new failed");
        return (-1);
    }

    msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    if (!pk) {
        g_critical("pcma_send: msgpack init failed");
        return (-2);
    }

    if (pack_fn(pk, data) < 0) {
        g_critical("pcma_send: pack function failed");
        return (-3);
    }

    msgpack_packer_free(pk);

    if (zmq_msg_init_data(&msg,
                          (void *) buffer->data, buffer->size,
                          zmq_free_helper, buffer) < 0) {
        g_critical("pcma_send: zmq_msg_init_data: %s", strerror(errno));
        return (-4);
    }

    if (zmq_send(socket, &msg, 0) < 0) {
        g_critical("pcma_send: zmq_send: %s", strerror(errno));
        return (-5);
    }

    if (zmq_msg_close(&msg) < 0) {
        g_critical("pcma_send: zmq_msg_close: %s", strerror(errno));
    }

    return (0);
}

void string_pack(gpointer data, gpointer user_data)
{
    msgpack_packer *pk = (msgpack_packer *) user_data;
    const gchar *tag = (const gchar *) data;
    size_t taglen = strlen(tag);
    msgpack_pack_raw(pk, taglen);
    msgpack_pack_raw_body(pk, tag, taglen);
}
