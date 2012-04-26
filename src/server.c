#include <glib.h>
#include <glib/gprintf.h>
#include <msgpack.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>
#include "common.h"
#include "server.h"
#include "mlockfile.h"

void lockfile_print_tag(gpointer data, gpointer user_data)
{
    const gchar *tag = (const gchar *) data;
    if (g_printf("%s ", tag) < 0)
        g_critical("lockfile_print_tag: g_printf: %s", strerror(errno));
}

void lockfile_print(gpointer key, gpointer value, gpointer user_data)
{
    const gchar *errmsg = "lockfile_print: g_printf: %s";
    const gchar *name = (const gchar *) key;
    struct mlockfile *f = (struct mlockfile *) value;

    if (g_printf
        ("%s, %li bytes, fd: %i, tags: ", name, f->fd, f->mmappedsize) < 0)
        g_critical(errmsg, strerror(errno));

    g_list_foreach(f->tags, lockfile_print_tag, NULL);

    if (g_printf("\n") < 0)
        g_critical(errmsg, strerror(errno));
}

void lockfiles_print(GHashTable * lockfiles)
{
    const gchar *errmsg = "lockfiles_print: g_printf: %s";

    if (g_printf("Locked files: ---\n") < 0)
        g_critical(errmsg, strerror(errno));

    g_hash_table_foreach(lockfiles, lockfile_print, NULL);

    if (g_printf("--- end of list ---\n") < 0)
        g_critical(errmsg, strerror(errno));
}

void mlockfile_pack(msgpack_packer * pk, struct mlockfile *f)
{
    msgpack_pack_array(pk, 3);
    msgpack_pack_uint64(pk, f->fd);
    msgpack_pack_uint64(pk, f->mmappedsize);
    msgpack_pack_array(pk, g_list_length(f->tags));
    g_list_foreach(f->tags, string_pack, pk);
}

int mlockfile_packfn(msgpack_packer * pk, void *lockfile)
{
    msgpack_pack_array(pk, 2);
    msgpack_pack_true(pk);

    mlockfile_pack(pk, lockfile);
    return (0);
}

void lockfiles_entry_packfn(gpointer key, gpointer value,
                            gpointer user_data)
{
    const gchar *name = (const gchar *) key;
    struct mlockfile *lockfile = (struct mlockfile *) value;
    msgpack_packer *pk = (msgpack_packer *) user_data;
    int namelen = strlen(name);

    msgpack_pack_raw(pk, namelen);
    msgpack_pack_raw_body(pk, name, namelen);

    mlockfile_pack(pk, lockfile);
}

int lockfiles_packfn(msgpack_packer * pk, void *lf)
{
    GHashTable *lockfiles = (GHashTable *) lf;

    msgpack_pack_array(pk, 2);

    msgpack_pack_true(pk);

    msgpack_pack_map(pk, g_hash_table_size(lockfiles));
    g_hash_table_foreach(lockfiles, lockfiles_entry_packfn, (gpointer) pk);
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

void handle_ping_request(void *socket)
{
    g_info("ping request");

    pcma_send(socket, empty_ok_packfn, NULL);
}

void handle_list_request(void *socket)
{
    g_info("list request");

    int ret = pcma_send(socket, lockfiles_packfn, lockfiles);
    if (ret < 0)
        g_critical("handle_list_request: pcma_send: %i", ret);
}

void add_new_tags_to_mlockfile(gpointer data, gpointer user_data)
{
    struct mlockfile *file = (struct mlockfile *) user_data;

    if (!g_list_find_custom(file->tags, data, g_strcmp0))
        file->tags = g_list_prepend(file->tags, g_strdup(data));
}

void handle_lock_request(void *socket, const gchar * path, GList * tags)
{
    int ret;

    g_info("lock request (%s)", path);

    struct mlockfile *file, *found = g_hash_table_lookup(lockfiles, path);

    if (found) {
        g_debug("handle_lock_request: found lock for %s", path);
        file = found;
    } else {
        g_debug("handle_lock_request: first lock for %s", path);
        if (!(file = mlockfile_init(path))) {
            g_critical("mlockfile_init failed");
            pcma_send(socket, failed_packfn, "mlockfile_init failed");
        }
    }

    g_list_foreach(tags, add_new_tags_to_mlockfile, file);

    ret = mlockfile_lock(path, file);
    if (ret < 0) {
        g_critical("mlockfile_lock: %i", ret);
        pcma_send(socket, failed_packfn, "mlockfile_lock failed");
    }

    if (!found)
        g_hash_table_insert(lockfiles, g_strdup(path), file);

    ret = pcma_send(socket, mlockfile_packfn, file);
    if (ret < 0)
        g_critical("handle_lock_request: pcma_send: %i", ret);

    g_info("locked %s", path);
}

void handle_unlock_request(void *socket, const gchar * path)
{
    int ret;
    struct mlockfile *file =
        (struct mlockfile *) g_hash_table_lookup(lockfiles, path);

    g_info("unlock request (%s)", path);

    if (!file) {
        g_warning("handle_lock_request could not find %s", path);
        pcma_send(socket, failed_packfn, "not found");
        return;
    }

    ret = mlockfile_unlock(file);
    if (ret < 0) {
        g_critical("handle_unlock_request: mlockfile_unlock: %i", ret);
        pcma_send(socket, failed_packfn, "could not unlock");
        return;
    }

    if (g_hash_table_remove(lockfiles, path) == FALSE)
        g_error("handle_unlock_request: g_hash_table_remove failed");

    pcma_send(socket, empty_ok_packfn, NULL);
    g_info("unlocked %s", path);
}

struct release_tag_data {
    guint64 untagged;
    guint64 unlocked;
    guint64 untouched;
    guint64 failed;
    const gchar *tag;
};

int release_tag_data_packfn(msgpack_packer * pk, void *rtdp)
{
    struct release_tag_data *data = (struct release_tag_data *) rtdp;
    msgpack_pack_array(pk, 2);
    msgpack_pack_true(pk);
    msgpack_pack_array(pk, 4);
    msgpack_pack_uint64(pk, data->untagged);
    msgpack_pack_uint64(pk, data->unlocked);
    msgpack_pack_uint64(pk, data->untouched);
    msgpack_pack_uint64(pk, data->failed);
    return (0);
}

void releasetag(gpointer key, gpointer value, gpointer user_data)
{
    struct release_tag_data *data = user_data;
    const gchar *name = (const gchar *) key;
    struct mlockfile *file = (struct mlockfile *) value;
    GList *found = NULL;
    int ret;

    found = g_list_find_custom(file->tags, data->tag, g_strcmp0);
    if (found) {
        file->tags = g_list_remove(file->tags, found->data);
        data->untagged++;

        if (g_list_length(file->tags) == 0) {
            ret = mlockfile_unlock(file);
            if (ret < 0) {
                g_critical("relesetag: mlockfile_unlock: %i", ret);
                data->failed++;
            } else {
                data->unlocked++;
            }

            if (g_hash_table_remove(lockfiles, key) == FALSE)
                g_error("releasetag: g_hash_table_remove failed");
        }
    } else {
        data->untouched++;
    }
}

void handle_releasetag_request(void *socket, const gchar * tag)
{
    struct release_tag_data data = { 0, 0, 0, 0, NULL };
    data.tag = tag;

    g_info("releasetag request (%s)", tag);

    g_hash_table_foreach(lockfiles, releasetag, &data);

    if (data.untagged == 0) {
        g_warning("handle_releasetag_request: nothing was tagged %s", tag);
        pcma_send(socket, failed_packfn, "nothing was tagged");
    } else
        pcma_send(socket, release_tag_data_packfn, &data);
}

void announce_failure(void *socket, char *msg)
{
    g_warning("handle_req: %s", msg);
    pcma_send(socket, failed_packfn, msg);
}

int handle_req(void *socket, zmq_msg_t * msg)
{
    int command_id, i;
    const gchar *query, *tag, *path = NULL;
    GList *tags = NULL;

    msgpack_object obj;
    msgpack_unpacked pack;

    msgpack_unpacked_init(&pack);

    if (!msgpack_unpack_next
        (&pack, zmq_msg_data(msg), zmq_msg_size(msg), NULL)) {
        announce_failure(socket, "msgpack_unpack_next failed");
        return (-1);
    }

    obj = pack.data;

    if (obj.type != MSGPACK_OBJECT_ARRAY) {
        announce_failure(socket, "not an array");
        return (-2);
    }

    const gchar *command =
        (const gchar *) obj.via.array.ptr[0].via.raw.ptr;
    int command_size = obj.via.array.ptr[0].via.raw.size;
    if (!command) {
        announce_failure(socket, "no command");
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
    } else if (command_size == RELEASETAG_COMMAND_SIZE &&
               !bcmp(RELEASETAG_COMMAND, command,
                     RELEASETAG_COMMAND_SIZE)) {
        command_id = RELEASETAG_COMMAND_ID;
    } else {
        announce_failure(socket, "unknown command");
        return (-4);
    }

    switch (command_id) {
    case PING_COMMAND_ID:
    case LIST_COMMAND_ID:
        if (obj.via.array.size != 1) {
            announce_failure(socket, "no parameter expected");
            return (-5);
        }
        break;
    case UNLOCK_COMMAND_ID:
    case RELEASETAG_COMMAND_ID:
        if (obj.via.array.size != 2) {
            announce_failure(socket, "1 parameter expected");
            return (-6);
        }
        /* fallthrough */
    case LOCK_COMMAND_ID:
        if (obj.via.array.ptr[1].type != MSGPACK_OBJECT_RAW) {
            announce_failure(socket, "RAW parameter expected");
            return (-7);
        }
        path = raw_to_string(&obj.via.array.ptr[1].via.raw);
        if (!path) {
            announce_failure(socket, "raw_to_string failed");
            return (-8);
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
        if (obj.via.array.size > 2) {
            if (obj.via.array.ptr[2].type != MSGPACK_OBJECT_ARRAY)
                announce_failure(socket, "tags should be a list");
            else {
                for (i = 0; i < obj.via.array.ptr[2].via.array.size; i++) {
                    if (obj.via.array.ptr[2].via.array.ptr[i].type !=
                        MSGPACK_OBJECT_RAW)
                        continue;       /* drops a format error silently */
                    tag =
                        raw_to_string(&
                                      (obj.via.array.ptr[2].via.
                                       array.ptr[i]).via.raw);
                    if (tag)
                        tags = g_list_prepend(tags, (gchar *) tag);
                    else {
                        announce_failure(socket,
                                         "tag raw_to_string failed");
                        free((void *) path);
                        g_list_free(tags);
                        return (-10);
                    }
                }
                handle_lock_request(socket, path, tags);
            }
        } else {
            handle_lock_request(socket, path, NULL);
        }
        break;
    case UNLOCK_COMMAND_ID:
        handle_unlock_request(socket, path);
        break;
    case RELEASETAG_COMMAND_ID:
        handle_releasetag_request(socket, path);
        break;
    }

    msgpack_unpacked_destroy(&pack);

    if (path)
        free((char *) path);
    if (tags)
        g_list_free(tags);

    return 0;
}

int loop(void *socket)
{
    int ret = 0;
    zmq_msg_t msg;

    for (;;) {
        if (zmq_msg_init(&msg) < 0) {
            g_warning("loop: zmq_msg_init: %s", strerror(errno));
            continue;
        }

        if (zmq_recv(socket, &msg, 0) < 0) {
            if (errno == EINTR)
                continue;
            else
                g_error("loop: zmq_recv: %s", strerror(errno));
        }

        if ((ret = handle_req(socket, &msg)) < 0)
            g_warning("loop: handle_req: %i", ret);

        if (zmq_msg_close(&msg) < 0)
            g_warning("loop: zmq_msg_close: %s", strerror(errno));
    }

    return (-42);               /* Yiipee! */
}

void help(const gchar * name)
{
    const gchar *disp_name = name;
    if (!disp_name)
        disp_name = default_name;

    fprintf(stderr, "Usage: %s [-e ENDPOINT]\n", disp_name);
    exit(EXIT_FAILURE);
}

int server_leave(int signum)
{
    g_info("Signal %i received", signum);

    lockfiles_print(lockfiles);

    if (pcmad_sock && (zmq_close(pcmad_sock) < 0))
        return (-1);
    if (pcmad_ctx && (zmq_term(pcmad_ctx) < 0))
        return (-2);
    if (lockfiles)
        g_hash_table_unref(lockfiles);

    return (0);
}

void sh_termination(int signum)
{
    if (server_leave(signum) < 0)
        exit(EXIT_FAILURE);
    exit(EXIT_SUCCESS);
}

void sh_abrt(int signum)
{
    server_leave(signum);
    exit(EXIT_FAILURE);
}

void sh_usr1(int signum)
{
    g_info("SIGUSR1 received");
    lockfiles_print(lockfiles);
}

void setup_signals()
{
    setup_sig(SIGTERM, sh_termination, 1);
    setup_sig(SIGINT, sh_termination, 1);
    setup_sig(SIGQUIT, sh_termination, 1);
    setup_sig(SIGUSR1, sh_usr1, 0);
    setup_sig(SIGABRT, sh_abrt, 0);
}

int main(int argc, char **argv)
{
    int ret, opt;
    const gchar *endpoint = default_ep;

    lockfiles =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                              mlockfile_destroy);

    setup_logging();
    setup_signals();

    while ((opt = getopt(argc, argv, "e:")) != -1) {
        switch (opt) {
        case 'e':
            endpoint = optarg;
            break;
        default:
            if (argc > 0)
                help(argv[0]);
            else
                help(NULL);
        }
    }

    g_info("using endpoint %s", endpoint);

    if (!(pcmad_ctx = zmq_init(1)))
        g_error("zmq_init: %s", strerror(errno));

    if (!(pcmad_sock = zmq_socket(pcmad_ctx, ZMQ_REP)))
        g_error("zmq_socket: %s", strerror(errno));

    if (zmq_bind(pcmad_sock, endpoint) < 0)
        g_error("zmq_bind: %s", strerror(errno));

    loop(pcmad_sock);

    if (pcmad_sock && zmq_close(pcmad_sock) < 0)
        g_error("zmq_close: %s", strerror(errno));
    if (pcmad_ctx && zmq_term(pcmad_ctx) < 0)
        g_error("zmq_term: %s", strerror(errno));

    return (EXIT_SUCCESS);
}
