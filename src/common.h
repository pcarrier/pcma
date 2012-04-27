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
void setup_sig(int signum, void (*sh) (int), int keep_ignoring);
void setup_logging();
void string_pack(gpointer data, gpointer user_data);

#ifndef g_info
#define g_info(...) g_log(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, __VA_ARGS__)
#endif

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
#define RELEASETAG_COMMAND_ID 5
#define RELEASETAG_COMMAND "releasetag"
#define RELEASETAG_COMMAND_SIZE 10

#endif                          /* PCMA__COMMON_H */
