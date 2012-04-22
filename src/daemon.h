#ifndef PCMA__DAEMON_H
#define PCMA__DAEMON_H

const char *default_name = "pcmad";
void *pcmad_ctx = NULL, *pcmad_sock = NULL;
GHashTable *lockfiles = NULL;

#endif /* PCMA__DAEMON_H */
