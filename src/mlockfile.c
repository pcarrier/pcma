#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "common.h"
#include "mlockfile.h"

struct mlockfile *mlockfile_init(char *path)
{
    struct mlockfile *f = malloc(sizeof(struct mlockfile));
    if (!f) {
        perror("mlockfile_init struct alloc");
        return (NULL);
    }

    int path_length = strlen(path) + 1;

    f->path = malloc(path_length);
    if (!(f->path)) {
        perror("mlockfile_init path alloc");
        free(f);
        return (NULL);
    }

    memcpy(f->path, path, path_length);
    f->fd = -1;
    f->mmappedsize = 0;
    f->mmapped = NULL;

    return (f);
}

void mlockfile_release(struct mlockfile *f)
{
    if (f->mmapped) {
        if (munlock(f->mmapped, f->mmappedsize) < 0)
            perror("mlockfile_release munlock");
        if (munmap(f->mmapped, f->mmappedsize) < 0)
            perror("mlockfile_release munmap");
    }
    if (f->fd > 0) {
        if (close(f->fd) < 0)
            perror("mlockfile_release close");
        LOG_DEBUG("closed fd %i for %s\n", f->fd, f->path);
    }

    free(f->path);
    free(f);
}

int mlockfile_lock(struct mlockfile *f)
{
    struct stat stats;
    char *mmapped;

    if (f->fd < 0) {
        f->fd = open(f->path, O_RDONLY);
        if (f->fd < 0) {
            perror("mlockfile_lock open");
            return (-1);
        }
        LOG_DEBUG("opened fd %i for %s\n", f->fd, f->path);
    }

    if (fstat(f->fd, &stats) < 0) {
        perror("mlockfile_lock stat");
        return (-2);
    }

    mmapped = mmap(NULL, stats.st_size,
                   PROT_READ, MAP_SHARED | MAP_FILE, f->fd, (off_t) 0);
    if (mmapped == MAP_FAILED) {
        perror("mlockfile_lock mmap");
        return (-3);
    }

    if (mlock(mmapped, stats.st_size) < 0) {
        perror("mlockfile_lock mlock");
        if (munmap(mmapped, stats.st_size) < 0)
            perror("mlockfile_lock mlock failure munmap");
        return (-4);
    }

    if (f->mmapped) {
        LOG_INFO("Remap for %s (%liB->%liB)\n",
                 f->path, (long) f->mmappedsize, (long) stats.st_size);
        if (munlock(f->mmapped, f->mmappedsize) < 0)
            perror("mlockfile_lock munlock");
        if (munmap(f->mmapped, f->mmappedsize) < 0)
            perror("mlockfile_lock munmap");
    }

    f->mmapped = mmapped;
    f->mmappedsize = stats.st_size;

    return (0);
}

size_t mfl_length(struct mlockfile_list * list)
{
    size_t counter = 0;
    struct mlockfile_list *itr = list;
    while (itr != NULL) {
        itr = itr->next;
        counter++;
    }
    return counter;
}

struct mlockfile_list *mfl_find_path(struct
                                     mlockfile_list
                                     *head, char *path)
{
    struct mlockfile_list *itr = head;
    while (itr != NULL) {
        if (!strcmp(itr->file->path, path))
            return itr;
        itr = itr->next;
    }
    return NULL;
}

int mfl_add(struct mlockfile_list **l, struct mlockfile *f)
{
    struct mlockfile_list *e = malloc(sizeof(struct mlockfile_list));
    if (e < 0)
        return (-1);

    e->file = f;
    e->next = *l;
    *l = e;
    return (0);
}

int mfl_remove(struct mlockfile_list **list, struct mlockfile_list *entry)
{
    struct mlockfile_list *itr = *list, *previous = NULL;

    while (itr != NULL && itr != entry) {
        previous = itr;
        itr = itr->next;
    }

    if (!itr)
        return (-1);

    if (previous == NULL)
        *list = itr->next;
    else
        previous->next = itr->next;

    mlockfile_release(itr->file);
    free(itr);
    return (0);
}
