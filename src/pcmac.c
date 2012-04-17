#include <limits.h>
#include <msgpack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>
#include "common.h"
#include "pcmac.h"

struct pcma_req {
    int argc;
    char **argv;
};

int pcma_req_packfn(msgpack_packer * pk, void *req)
{
    int i, len;
    char *str;
    struct pcma_req *rreq = (struct pcma_req *)req;

    msgpack_pack_array(pk, (rreq->argc));
    for (i = 0; i < rreq->argc; i++) {
        str = rreq->argv[i];
        len = strlen(str);
        msgpack_pack_raw(pk, len);
        msgpack_pack_raw_body(pk, str, len);
    }
    return (0);
}

int handle_rep(zmq_msg_t * msg) {
    msgpack_object obj;
    msgpack_unpacked pack;
    char *errmsg;

    msgpack_unpacked_init(&pack);

    if (!msgpack_unpack_next
        (&pack, zmq_msg_data(msg), zmq_msg_size(msg), NULL)) {
        LOG_ERROR("handle_rep: msgpack_unpack_next failed\n");
        return (-1);
    }

    obj = pack.data;

    if (obj.type != MSGPACK_OBJECT_ARRAY) {
        LOG_ERROR("handle_rep: not an array\n");
        return (-2);
    }

    if (obj.via.array.size < 1) {
        LOG_ERROR("handle_rep: empty array\n");
        return (-3);
    }

    if (obj.via.array.ptr[0].type != MSGPACK_OBJECT_BOOLEAN) {
        LOG_ERROR("handle_rep: first entry is nota  boolean\n");
        return (-4);
    }

    if (obj.via.array.ptr[0].via.boolean == false) {
        if (obj.via.array.size > 1 && obj.via.array.ptr[1].type == MSGPACK_OBJECT_RAW) {
            errmsg = raw_to_string(&(obj.via.array.ptr[1].via.raw));
            if (errmsg) {
                LOG_SERV("%s\n", errmsg);
                free(errmsg);
            }
        }
        msgpack_unpacked_destroy(&pack);
        return (1);
    }

    if (obj.via.array.size > 1) {
        /* Technically speaking unspecified, but I feel lazy */
        msgpack_object_print (stdout, obj.via.array.ptr[1]);
        printf("\n");
    }
    return (0);
}

void help(char *name)
{
    char **disp_name = &name;
    if (!disp_name)
        disp_name = &default_name;

    fprintf(stderr,
            "Usage: %s [-t TIMEOUT] [-v...] [-e ENDPOINT] REQUEST [PARAMETER...]\n",
            *disp_name);
    exit(EXIT_LOCAL_FAILURE);
}

int main(int argc, char **argv)
{
    int ret, opt, exit_code = EXIT_LOCAL_FAILURE;
    void *ctx = NULL, *socket = NULL;
    char **endpoint = &default_ep;
    struct pcma_req req;
    zmq_pollitem_t pollitem;
    zmq_msg_t msg;

    while ((opt = getopt(argc, argv, "ve:t:")) != -1) {
        switch(opt) {
        case 'v':
            log_level++;
            break;
        case 'e':
            endpoint = &optarg;
            break;
        case 't':
            timeout = atol(optarg);
            if (timeout >= LONG_MAX / 1000L) {
                LOG_ERROR("timeout too high\n");
                goto err;
            }
            break;
        default:
            if(argc > 0)
                help(argv[0]);
            else
                help(NULL);
        }
    }

    LOG_INFO("using endpoint %s\n", *endpoint);
    if(timeout >= 0) {
        LOG_INFO("using a %li ms timeout\n", timeout);
    }

    if (!(ctx = zmq_init(1)))
        MAIN_ERR_FAIL("zmq_init");
    if (!(socket = zmq_socket(ctx, ZMQ_REQ)))
        MAIN_ERR_FAIL("zmq_socket");
    if (zmq_connect(socket, *endpoint) < 0)
        MAIN_ERR_FAIL("zmq_connect");

    if (optind >= argc) {
        LOG_ERROR("command expected\n");
        goto err;
    }

    req.argc = argc - optind;
    req.argv = argv + optind;

    ret = pcma_send(socket, pcma_req_packfn, &req);
    if (ret < 0) {
        LOG_ERROR("pcma_send failed with %i\n", ret);
        goto err;
    }

    if (timeout >= 0) {
        pollitem.socket = socket;
        pollitem.events = ZMQ_POLLIN;

        ret = zmq_poll(&pollitem, 1, timeout * 1000L);
        if (ret < 0)
            MAIN_ERR_FAIL("zmq_poll");
        if (ret == 0) {
            LOG_ERROR("timeout after %li ms\n", timeout);
            goto fastfail;
        }
    }

    if (zmq_msg_init(&msg) < 0)
        MAIN_ERR_FAIL("zmq_msg_init");

    if (zmq_recv(socket, &msg, 0) < 0)
        MAIN_ERR_FAIL("zmq_recv");

    ret = handle_rep(&msg);
    if (ret > 0)
        exit_code = EXIT_REMOTE_FAILURE;
    if (ret != 0) {
        LOG_ERROR("handle_rep failed with %i\n", ret);
        goto err;
    }

    if (zmq_msg_close(&msg) < 0) {
        perror("zmq_msg_close");
        goto err;
    }

    if (zmq_close(socket) < 0)
        MAIN_ERR_FAIL("zmq_close");
    if (zmq_term(ctx) < 0)
        MAIN_ERR_FAIL("zmq_term");

    return (EXIT_SUCCESS);

  err:
    if (socket)
        zmq_close(socket);
    if (ctx)
        zmq_term(ctx);
  fastfail:
    return (exit_code);
}
