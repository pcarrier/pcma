#ifndef PCMA__MLOCKFILE_H
#define PCMA__MLOCKFILE_H

struct mlockfile {
    int fd;
    char *path;
    size_t mmappedsize;
    void *mmapped;
};

struct mlockfile_list {
    struct mlockfile *file;
    struct mlockfile_list *next;
};

int mlockfile_lock(struct mlockfile *f);
void mlockfile_release(struct mlockfile *f);

struct mlockfile *mlockfile_init(const char *path);
size_t mfl_length(struct mlockfile_list *list);
int mfl_add(struct mlockfile_list **l,struct mlockfile *f);
int mfl_remove(struct mlockfile_list **list,struct mlockfile_list *entry);
struct mlockfile_list *mfl_find_path(struct mlockfile_list *head, const char *path);

#endif /* PCMA__MLOCKFILE_H */
