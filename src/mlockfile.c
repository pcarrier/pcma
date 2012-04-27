#include <glib.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "common.h"
#include "mlockfile.h"

struct mlockfile *mlockfile_init()
{
    struct mlockfile *f = g_new0(struct mlockfile, 1);
    f->fd = -1;
    return (f);
}

void mlockfile_destroy(gpointer p)
{
    struct mlockfile *f = (struct mlockfile *) p;

    int res;
    if ((res = mlockfile_unlock(f)) < 0)
        g_critical("mlockfile_release: mlockfile_unlock: %i", res);

    g_list_free(f->tags);
    g_free(f);
}

int mlockfile_lock(const gchar * path, struct mlockfile *f)
{
    struct stat stats;
    char *mmapped;

    if (f->fd < 0) {
        f->fd = open(path, O_RDONLY);
        if (f->fd < 0) {
            g_critical("mlockfile_lock: open: %s", strerror(errno));
            return (-1);
        }
        g_debug("opened fd %i for %s", f->fd, path);
    }

    if (fstat(f->fd, &stats) < 0) {
        g_critical("mlockfile_lock: stat: %s", strerror(errno));
        return (-2);
    }

    mmapped = mmap(NULL, stats.st_size,
                   PROT_READ, MAP_SHARED | MAP_FILE, f->fd, (off_t) 0);
    if (mmapped == MAP_FAILED) {
        g_critical("mlockfile_lock: mmap: %s", strerror(errno));
        return (-3);
    }

    if (mlock(mmapped, stats.st_size) < 0) {
        g_critical("mlockfile_lock: mlock: %s", strerror(errno));
        if (munmap(mmapped, stats.st_size) < 0)
            g_critical("mlockfile_lock: mlock failure: munmap: ");
        return (-4);
    }

    if (f->mmapped) {
        g_debug("relocked %s (%li -> %li bytes)",
                path, (long) f->mmappedsize, (long) stats.st_size);
        if (munlock(f->mmapped, f->mmappedsize) < 0)
            g_critical("mlockfile_lock: munlock: %s", strerror(errno));
        if (munmap(f->mmapped, f->mmappedsize) < 0)
            g_critical("mlockfile_lock: munmap: %s", strerror(errno));
    }

    f->mmapped = mmapped;
    f->mmappedsize = stats.st_size;

    return (0);
}

int mlockfile_unlock(struct mlockfile *f)
{
    if (f->mmapped) {
        if (munlock(f->mmapped, f->mmappedsize) < 0) {
            g_critical("mlockfile_unlock: munlock: %s", strerror(errno));
            return (-1);
        }
        if (munmap(f->mmapped, f->mmappedsize) < 0) {
            g_critical("mlockfile_unlock: munmap: %s", strerror(errno));
            return (-2);
        }

        f->mmapped = NULL;
    }
    if (f->fd > -1) {
        if (close(f->fd) < 0) {
            g_critical("mlockfile_unlock: close: %s", strerror(errno));
            return (-3);
        }
        f->fd = -1;
        g_debug("closed fd %i", f->fd);
    }
    return (0);
}
