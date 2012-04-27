#ifndef PCMA__MLOCKFILE_H
#define PCMA__MLOCKFILE_H

#include <glib.h>

struct mlockfile {
    int fd;
    size_t mmappedsize;
    void *mmapped;
    GList *tags;
};

struct mlockfile *mlockfile_init();
int mlockfile_lock(const gchar * filename, struct mlockfile *f);
int mlockfile_unlock(struct mlockfile *f);
void mlockfile_destroy(gpointer f);

#endif                          /* PCMA__MLOCKFILE_H */
