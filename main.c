#include <fuse3/fuse_log.h>
#include <stdlib.h>
#define FUSE_USE_VERSION 31

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse3/fuse.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_FILES 1000

static struct options {
  const char *project_name;
  int show_help;
} options;

#define OPTION(t, p)                                                           \
  { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("--name", project_name), OPTION("-n", show_help),
    OPTION("-h", show_help), OPTION("--help", show_help), FUSE_OPT_END};

char *workdir;

struct file {
  char *path;
  int fd;
};

struct file *files[MAX_FILES];
int iterator = 0;

char *get_file_path(const char *path) {
  int len1 = strlen(workdir);
  int len2 = strlen(path);
  char *file_path = malloc(len1 + len2 + 1);
  strcpy(file_path, workdir);
  strcat(file_path, path);
  return file_path;
}

void new_fd(char *path, int fd) {
  struct file *f = malloc(sizeof(struct file));
  f->path = strdup(path);
  f->fd = fd;
  files[iterator] = f;
  iterator++;
}

int remove_fd(char *path) {
  for (int i = 0; i < iterator; i++) {
    if (strcmp(files[i]->path, path) == 0) {
      close(files[i]->fd);
      free(files[i]->path);
      free(files[i]);
      files[i] = NULL;
      if (iterator == 1) {
        iterator = 0;
        return 0;
      }
      files[i] = files[iterator - 1];
      iterator--;
      return 0;
    }
  }
  return -1;
}

int get_fd(char *path) {
  for (int i = 0; i < iterator; i++) {
    if (strcmp(files[i]->path, path) == 0) {
      return files[i]->fd;
    }
  }
  return -1;
}

static void *hello_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  cfg->kernel_cache = 1;
  struct passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;
  char *path = malloc(strlen(homedir) + strlen(options.project_name) + 2);
  strcpy(path, homedir);
  strcat(path, "/");
  strcat(path, options.project_name);
  mkdir(path, 0777);
  workdir = path;
  return NULL;
}

static int hello_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
  (void)fi;
  int res = 0;
  char *file_path = get_file_path(path);
  if (access(file_path, F_OK) == -1) {
    res = -ENOENT;
    return res;
  }
  struct stat file_stat;
  stat(file_path, &file_stat);
  stbuf->st_mode = file_stat.st_mode;
  stbuf->st_nlink = file_stat.st_nlink;
  stbuf->st_size = file_stat.st_size;
  stbuf->st_uid = file_stat.st_uid;
  stbuf->st_gid = file_stat.st_gid;
  stbuf->st_blksize = file_stat.st_blksize;
  stbuf->st_blocks = file_stat.st_blocks;
  stbuf->st_atime = file_stat.st_atime;
  stbuf->st_mtime = file_stat.st_mtime;
  stbuf->st_ctime = file_stat.st_ctime;
  free(file_path);
  return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
  (void)flags;
  (void)offset;
  (void)fi;
  char *file_path = get_file_path(path);

  DIR *d;
  struct dirent *dir;
  d = opendir(file_path);
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      filler(buf, dir->d_name, NULL, 0, FUSE_FILL_DIR_PLUS);
    }
    closedir(d);
  }
  free(file_path);
  return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
  char *file_path = get_file_path(path);
  if (access(file_path, F_OK) == -1) {
    return -ENOENT;
  }
  int fd = open(file_path, fi->flags);
  if (fd == -1) {
    return -errno;
  }
  new_fd(file_path, fd);
  return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  (void)fi;
  char *file_path = get_file_path(path);
  int fd = get_fd(file_path);
  if (fd == -1) {
    return EBADF;
  }
  int res = pread(fd, buf, size, offset);
  fuse_log(FUSE_LOG_DEBUG, "buf: %s\n", buf);
  return res;
}

static int hello_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
  (void)fi;
  char *file_path = get_file_path(path);
  int fd = get_fd(file_path);
  if (fd == -1) {
    return -EBADF;
  }
  else {
    fuse_log(FUSE_LOG_DEBUG, "fd: %d\n", fd);
  }
  int res = pwrite(fd, buf, size, offset);
  fuse_log(FUSE_LOG_DEBUG, "res: %d %d \n", res, size);
  if (res == -1) {
    return -errno;
  }
  return res;
}

static int hello_create(const char *path, mode_t mode,
                        struct fuse_file_info *fi) {
  (void)fi;
  char *file_path = get_file_path(path);
  int fd = creat(file_path, mode);
  if (fd == -1) {
    return -errno;
  }
  new_fd(file_path, fd);
  free(file_path);
  return 0;
}

static int hello_release(const char *path, struct fuse_file_info *fi) {
  (void)fi;
  char *file_path = get_file_path(path);
  int err = remove_fd(file_path);
  free(file_path);
  if (err == -1) {
    return -EBADF;
  }
  return 0;
}

static int hello_mkdir(const char *path, mode_t mode) {
  char *file_path = get_file_path(path);
  mkdir(file_path, mode);
  free(file_path);
  return 0;
}

static int hello_unlink(const char *path) {
  char *file_path = get_file_path(path);
  remove_fd(file_path);
  remove(file_path);
  free(file_path);
  return 0;
}

static int hello_rmdir(const char *path) {
  char *file_path = get_file_path(path);
  rmdir(file_path);
  free(file_path);
  return 0;
}

static const struct fuse_operations hello_oper = {
    .init = hello_init,
    .getattr = hello_getattr,
    .readdir = hello_readdir,
    .open = hello_open,
    .read = hello_read,
    .write = hello_write,
    .create = hello_create,
    .mkdir = hello_mkdir,
    .release = hello_release,
    .unlink = hello_unlink,
    .rmdir = hello_rmdir,
};

static void show_help(const char *progname) {
  printf("usage: %s [options] <mountpoint>\n\n", progname);
  printf("File-system specific options:\n"
         "    --name=<s>          Name of project\n"
         "                        (default: \"project\")\n"
         "\n");
}

int main(int argc, char *argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  options.project_name = strdup("name");

  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;

  if (options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }

  ret = fuse_main(args.argc, args.argv, &hello_oper, NULL);
  fuse_opt_free_args(&args);
  return ret;
}
