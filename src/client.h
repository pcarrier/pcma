#ifndef PCMA__CLIENT_H
#define PCMA__CLIENT_H

#define EXIT_OK 0
#define EXIT_LOCAL_FAILURE 1
#define EXIT_REMOTE_FAILURE 2

const char *default_name = "pcmac";
long timeout = -1;
void *pcmac_ctx = NULL, *pcmac_sock = NULL;
int client_exit_code = EXIT_LOCAL_FAILURE;

#endif                          /* PCMA__CLIENT_H */
