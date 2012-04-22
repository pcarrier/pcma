#ifndef PCMA__DAEMON_H
#define PCMA__DAEMON_H

struct mlockfile_list *mfl = NULL;
const char *default_name = "pcmad";
int should_exit = 0;
void *pcmad_ctx = NULL;

#endif /* PCMA__DAEMON_H */
