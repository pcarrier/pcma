#include <stdlib.h>
#include <stdio.h>
#include <msgpack.h>
#include <unistd.h>
#include <zmq.h>
#include "common.h"
#include "mlockfile.h"
#include "pcmad.h"

void mlockfile_pack(msgpack_packer * pk, struct mlockfile *lf)
{
    size_t path_length = strlen(lf->path);
    msgpack_pack_array(pk, 2);
    msgpack_pack_raw(pk, path_length);
    msgpack_pack_raw_body(pk, lf->path, path_length);
    msgpack_pack_uint64(pk, lf->mmappedsize);
}

int mlockfile_packfn(msgpack_packer * pk, void *file)
{
    msgpack_pack_array(pk, 2);
    msgpack_pack_true(pk);

    mlockfile_pack(pk, file);
    return (0);
}

int failed_packfn(msgpack_packer * pk, void *msg)
{
    size_t msg_length = strlen((char *) msg);

    msgpack_pack_array(pk, 2);
    msgpack_pack_false(pk);

    msgpack_pack_raw(pk, msg_length);
    msgpack_pack_raw_body(pk, msg, msg_length);
    return (0);
}

int empty_ok_packfn(msgpack_packer * pk, void *ignored)
{
    msgpack_pack_array(pk, 1);
    msgpack_pack_true(pk);
    return (0);
}

int mfl_packfn(msgpack_packer * pk, void *mfl)
{
    struct mlockfile_list *itr = mfl;

    msgpack_pack_array(pk, 2);
    msgpack_pack_true(pk);

    msgpack_pack_array(pk, mfl_length(mfl));
    while (itr != NULL) {
        mlockfile_pack(pk, itr->file);
        itr = itr->next;
    }
    return (0);
}

void handle_ping_request(void *socket)
{
    pcma_send(socket, empty_ok_packfn, NULL);
}

void handle_list_request(void *socket)
{
    int ret = pcma_send(socket, mfl_packfn, mfl);
    if (ret < 0)
        LOG_ERROR("handle_list_request: pcma_send failed with %i\n", ret);
}

void handle_lock_request(void *socket, char *path)
{
    int ret;
    struct mlockfile *f;
    struct mlockfile_list *e = mfl_find_path(mfl, path);

    if (e) {
        LOG_DEBUG("handle_lock_request found %s\n", path);
        f = e->file;
    } else if (!(f = mlockfile_init(path))) {
        LOG_ERROR("mlockfile_init failed\n");
        pcma_send(socket, failed_packfn, "mlockfile_init failed");
    }

    ret = mlockfile_lock(f);
    if (ret < 0) {
        LOG_ERROR("mlockfile_lock failed with %i\n", ret);
        pcma_send(socket, failed_packfn, "mlockfile_lock failed");

        if (e) {
            ret = mfl_remove(&mfl, e);
            if (ret < 0)
                LOG_ERROR
                    ("handle_lock_request mfl_remove failed with %i\n",
                     ret);
        } else {
            mlockfile_release(f);
        }
        return;
    }

    ret = pcma_send(socket, mlockfile_packfn, f);
    if (ret < 0)
        LOG_ERROR("handle_lock_request: pcma_send failed with %i\n", ret);

    if (!e) {
        mfl_add(&mfl, f);
    }
}

void handle_unlock_request(void *socket, char *path)
{
    int ret;
    struct mlockfile_list *e = mfl_find_path(mfl, path);

    if (!e) {
        LOG_ERROR("handle_lock_request could not find %s\n", path);
        pcma_send(socket, failed_packfn, "not found");
        return;
    }

    ret = mfl_remove(&mfl, e);
    if (ret < 0) {
        LOG_ERROR("handle_unlock_request mfl_removed failed with %i", ret);
        pcma_send(socket, failed_packfn, "could not remove");
    } else {
        pcma_send(socket, empty_ok_packfn, NULL);
    }
}

#define PING_COMMAND_ID 1
#define PING_COMMAND "ping"
#define PING_COMMAND_SIZE 4
#define LIST_COMMAND_ID 2
#define LIST_COMMAND "list"
#define LIST_COMMAND_SIZE 4
#define LOCK_COMMAND_ID 3
#define LOCK_COMMAND "lock"
#define LOCK_COMMAND_SIZE 4
#define UNLOCK_COMMAND_ID 4
#define UNLOCK_COMMAND "unlock"
#define UNLOCK_COMMAND_SIZE 6

int handle_req(void *socket, zmq_msg_t * msg)
{
    int command_id;
    char *query, *path = NULL;
    msgpack_object obj;
    msgpack_unpacked pack;

    msgpack_unpacked_init(&pack);

    if (!msgpack_unpack_next
        (&pack, zmq_msg_data(msg), zmq_msg_size(msg), NULL)) {
        LOG_ERROR("handle_req: msgpack_unpack_next failed\n");
        return (-1);
    }

    obj = pack.data;

    if (obj.type != MSGPACK_OBJECT_ARRAY) {
        LOG_ERROR("handle_req: not an array\n");
        pcma_send(socket, failed_packfn, "not an array");
        return (-2);
    }

    char *command = (char *) obj.via.array.ptr[0].via.raw.ptr;
    int command_size = obj.via.array.ptr[0].via.raw.size;
    if (!command) {
        LOG_ERROR("handle_req: no command\n");
        pcma_send(socket, failed_packfn, "no command");
        return (-3);
    }

    if (command_size == PING_COMMAND_SIZE &&
        !bcmp(PING_COMMAND, command, PING_COMMAND_SIZE)) {
        command_id = PING_COMMAND_ID;
    } else if (command_size == LIST_COMMAND_SIZE &&
               !bcmp(LIST_COMMAND, command, LIST_COMMAND_SIZE)) {
        command_id = LIST_COMMAND_ID;
    } else if (command_size == LOCK_COMMAND_SIZE &&
               !bcmp(LOCK_COMMAND, command, LOCK_COMMAND_SIZE)) {
        command_id = LOCK_COMMAND_ID;
    } else if (command_size == UNLOCK_COMMAND_SIZE &&
               !bcmp(UNLOCK_COMMAND, command, UNLOCK_COMMAND_SIZE)) {
        command_id = UNLOCK_COMMAND_ID;
    } else {
        LOG_ERROR("handle_req: unknown command\n");
        pcma_send(socket, failed_packfn, "unknown command");
        return (-4);
    }

    switch (command_id) {
    case PING_COMMAND_ID:
    case LIST_COMMAND_ID:
        if (obj.via.array.size != 1) {
            LOG_ERROR("handle_req: no parameter expected\n");
            pcma_send(socket, failed_packfn, "no parameter expected");
            return (-5);
        }
        break;
    case LOCK_COMMAND_ID:
    case UNLOCK_COMMAND_ID:
        if (obj.via.array.size != 2) {
            LOG_ERROR("handle_req: 1 parameter expected\n");
            pcma_send(socket, failed_packfn, "1 parameter expected");
            return (-6);
        }
        if (obj.via.array.ptr[1].type != MSGPACK_OBJECT_RAW) {
            LOG_ERROR("handle_req: RAW parameter expected\n");
            pcma_send(socket, failed_packfn, "RAW parameter expected");
            return (-7);
        }
        path = raw_to_string(&obj.via.array.ptr[1].via.raw);
        if (!path) {
            perror("raw_to_string");
            pcma_send(socket, failed_packfn, "raw_to_string failure");
        }
    }

    switch (command_id) {
    case PING_COMMAND_ID:
        handle_ping_request(socket);
        break;
    case LIST_COMMAND_ID:
        handle_list_request(socket);
        break;
    case LOCK_COMMAND_ID:
        handle_lock_request(socket, path);
        break;
    case UNLOCK_COMMAND_ID:
        handle_unlock_request(socket, path);
        break;
    }

    msgpack_unpacked_destroy(&pack);

    if (path)
        free(path);

    return 0;
}

int loop(void *socket)
{
    int ret = 0;
    zmq_msg_t msg;

    for (;;) {
        if (zmq_msg_init(&msg) < 0) {
            perror("zmq_msg_init");
            goto loop_goes_on;
        }

        if (zmq_recv(socket, &msg, 0) < 0) {
            perror("zmq_recv");
            return (-1);
        }

        ret = handle_req(socket, &msg);
        if (ret < 0) {
            LOG_ERROR("handle_req failed with %i\n", ret);
        }

        if (zmq_msg_close(&msg) < 0) {
            perror("zmq_msg_close");
            goto loop_goes_on;
        }

        continue;

      loop_goes_on:
        LOG_ERROR("loop must go on!\n");
    }
    return (-1);
}

void help(char *name)
{
    char **disp_name = &name;
    if (!disp_name)
        disp_name = &default_name;

    fprintf(stderr,
            "Usage: %s [-v...] [-e ENDPOINT]\n",
            *disp_name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    int ret, opt;
    void *ctx = NULL, *socket = NULL;
    char **endpoint = &default_ep;

    while ((opt = getopt(argc, argv, "ve:")) != -1) {
        switch(opt) {
        case 'v':
            log_level++;
            break;
        case 'e':
            endpoint = &optarg;
            break;
        default:
            if(argc > 0)
                help(argv[0]);
            else
                help(NULL);
        }
    }

    LOG_INFO("using endpoint %s\n", *endpoint);

    if (!(ctx = zmq_init(1)))
        MAIN_ERR_FAIL("zmq_init");
    if (!(socket = zmq_socket(ctx, ZMQ_REP)))
        MAIN_ERR_FAIL("zmq_socket");
    if (zmq_bind(socket, *endpoint) < 0)
        MAIN_ERR_FAIL("zmq_bind");

    ret = loop(socket);
    if (ret < 0) {
        LOG_ERROR("loop returned %i\n", ret);
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
    return (EXIT_FAILURE);
}
