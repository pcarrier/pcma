#include <glib.h>
#include <limits.h>
#include <msgpack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>
#include "common.h"
#include "client.h"

struct pcma_req {
    int argc;
    char **argv;
};

int pcma_req_packfn(msgpack_packer * pk, void *req)
{
    int i, len;
    const char *str;
    const struct pcma_req *rreq = (struct pcma_req *) req;
    GList *tags = NULL;

    if (!strcmp(rreq->argv[0], LOCK_COMMAND)) {
        if (rreq->argc > 2)
            msgpack_pack_array(pk, 3);
        else if (rreq->argc == 2)
            msgpack_pack_array(pk, 2);
        else
            g_error("lock expects a path");

        string_pack(rreq->argv[0], pk);
        string_pack(rreq->argv[1], pk);

        if (rreq->argc > 2) {
            msgpack_pack_array(pk, rreq->argc - 2);
            for (i = 2; i < rreq->argc; i++)
                string_pack(rreq->argv[i], pk);
        }
    } else {
        msgpack_pack_array(pk, rreq->argc);
        for (i = 0; i < rreq->argc; i++)
            string_pack(rreq->argv[i], pk);
    }

    return (0);
}

int handle_rep(zmq_msg_t * msg)
{
    msgpack_object obj;
    msgpack_unpacked pack;
    char *errmsg;

    msgpack_unpacked_init(&pack);

    if (!msgpack_unpack_next
        (&pack, zmq_msg_data(msg), zmq_msg_size(msg), NULL))
        g_error("handle_rep: msgpack_unpack_next failed");

    obj = pack.data;

    if (obj.type != MSGPACK_OBJECT_ARRAY)
        g_error("handle_rep: not an array");

    if (obj.via.array.size < 1)
        g_error("handle_rep: empty array");

    if (obj.via.array.ptr[0].type != MSGPACK_OBJECT_BOOLEAN)
        g_error("handle_rep: first entry is not a boolean");

    if (obj.via.array.ptr[0].via.boolean == false) {
        if (obj.via.array.size > 1
            && obj.via.array.ptr[1].type == MSGPACK_OBJECT_RAW) {
            errmsg = raw_to_string(&(obj.via.array.ptr[1].via.raw));
            if (errmsg) {
                g_critical("server: %s", errmsg);
                free(errmsg);
            }
        }
        msgpack_unpacked_destroy(&pack);
        return (1);
    }

    if (obj.via.array.size > 1) {
        /* Technically speaking unspecified, but I feel lazy */
        msgpack_object_print(stdout, obj.via.array.ptr[1]);
        printf("\n");
    }
    return (0);
}

void help(const char *name)
{
    const char *disp_name = name;
    if (!disp_name)
        disp_name = default_name;

    fprintf(stderr,
            "Usage: %s [-t TIMEOUT] [-e ENDPOINT] REQUEST [PARAMETER...]\n",
            disp_name);
    exit(EXIT_LOCAL_FAILURE);
}

int client_leave(int signum)
{
    if (signum >= 0)
        g_info("Signal %i received", signum);

    if (pcmac_sock && (zmq_close(pcmac_sock) < 0))
        return (-1);
    if (pcmac_ctx && (zmq_term(pcmac_ctx) < 0))
        return (-2);
    return (0);
}

void main_exit()
{
    if (client_leave(-1) < 0 && client_exit_code == EXIT_OK)
        exit(EXIT_LOCAL_FAILURE);
    exit(client_exit_code);
}

void sh_termination(int signum)
{
    if (client_leave(signum) < 0)
        exit(EXIT_LOCAL_FAILURE);
    exit(EXIT_OK);
}

void sh_abrt(int signum)
{
    client_leave(signum);
    exit(EXIT_LOCAL_FAILURE);
}

void setup_signals()
{
    setup_sig(SIGTERM, sh_termination, 1);
    setup_sig(SIGINT, sh_termination, 1);
    setup_sig(SIGQUIT, sh_termination, 1);
    setup_sig(SIGABRT, sh_abrt, 0);
}

int main(int argc, char **argv)
{
    int ret, opt;
    void *socket = NULL;
    const char *endpoint = default_ep;
    struct pcma_req req;
    zmq_pollitem_t pollitem;
    zmq_msg_t msg;

    setup_logging();
    setup_signals();

    if (argc < 1)
        help(NULL);
    if (argc < 2)
        help(argv[0]);

    while ((opt = getopt(argc, argv, "e:t:")) != -1) {
        switch (opt) {
        case 'e':
            endpoint = optarg;
            break;
        case 't':
            timeout = atol(optarg);
            if (timeout >= LONG_MAX / 1000L)
                g_error("timeout %li too high", timeout);
            break;
        default:
            help(argv[0]);
        }
    }

    g_info("using endpoint %s", endpoint);
    if (timeout >= 0) {
        g_info("using a %li ms timeout", timeout);
    }

    if (!(pcmac_ctx = zmq_init(1)))
        g_error("zmq_init: %s", strerror(errno));

    if (!(pcmac_sock = zmq_socket(pcmac_ctx, ZMQ_REQ)))
        g_error("zmq_socket: %s", strerror(errno));

    if (zmq_connect(pcmac_sock, endpoint) < 0)
        g_error("zmq_connect: %s", strerror(errno));

    if (optind >= argc)
        g_error("command expected");

    req.argc = argc - optind;
    req.argv = argv + optind;

    if ((ret = pcma_send(pcmac_sock, pcma_req_packfn, &req)) < 0)
        g_error("pcma_send: %i", ret);

    if (timeout >= 0) {
        pollitem.socket = pcmac_sock;
        pollitem.events = ZMQ_POLLIN;

        ret = zmq_poll(&pollitem, 1, timeout * 1000L);
        if (ret < 0)
            g_error("zmq_poll: %s", strerror(errno));
        if (ret == 0) {
            int zero = 0;
            zmq_setsockopt(pcmac_sock, ZMQ_LINGER, &zero, sizeof(zero));
            g_error("timeout after %li ms", timeout);
        }
    }

    if (zmq_msg_init(&msg) < 0)
        g_error("zmq_msg_init: %s", strerror(errno));

    if (zmq_recv(pcmac_sock, &msg, 0) < 0)
        g_error("zmq_recv: %s", strerror(errno));

    ret = handle_rep(&msg);
    if (ret > 0) {
        client_exit_code = EXIT_REMOTE_FAILURE;
        main_exit();
    }
    if (ret != 0) {
        g_error("handle_rep: %i", ret);
    }

    if (zmq_msg_close(&msg) < 0)
        g_error("zmq_msg_close: %s", strerror(errno));

    client_exit_code = EXIT_OK;
    main_exit();
}
