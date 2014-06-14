#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <alloca.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <bsd/stdlib.h>

/*
 * Determines whether files will be copied using mmap(2) or using a buffer.
 */
#ifndef MMAP_FILES
#define MMAP_FILES 0
#endif

int main(int, char *[]);
static char *determine_target(const char *, const char *);
#if MMAP_FILES == 0
static void copy_file_buffer(int, int, size_t);
#else
static void copy_file_mmap(int, struct stat*, int);
#endif
static void usage(void);

/*
 * Copies a source file to a target file.
 */
int
main(int argc, char *argv[])
{
  char *source_path;
  char *target_path;
  int source_fd;
  int target_fd;
  struct stat source_sb;
  struct stat target_sb;

  setprogname((char *)argv[0]);

  if (argc != 3)
    usage();
    /* NOTREACHED */

  /* Determine the paths. */
  source_path = argv[1];
  if ((stat(source_path, &source_sb) == -1) || !S_ISREG(source_sb.st_mode))
    err(EXIT_FAILURE, "source is not a regular file");
  target_path = determine_target(source_path, argv[2]);

  if (target_path == NULL) {
    if (errno == 0)
      errx(EXIT_FAILURE, "target path is invalid");
    else
      errx(EXIT_FAILURE, "target path is invalid");
  }

  if ((stat(target_path, &target_sb) != -1) && (source_sb.st_ino
        == target_sb.st_ino))
    errx(EXIT_FAILURE, "source and target are the same file");

  /* Open the files. */
  if ((source_fd = open(source_path, O_RDONLY)) == -1)
    err(EXIT_FAILURE, "source open error");
  if ((target_fd = open(target_path, O_RDWR | O_CREAT | O_TRUNC,
        source_sb.st_mode)) == -1)
    err(EXIT_FAILURE, "target open error");

  free(target_path);

  /* Copy source to target. */
#if MMAP_FILES == 0
  copy_file_buffer(source_fd, target_fd, source_sb.st_blksize);
#else
  copy_file_mmap(source_fd, &source_sb, target_fd);
#endif

  /* Close the files. */
  if (close(source_fd) == -1)
    err(EXIT_FAILURE, "source close error");
  if (close(target_fd) == -1)
    err(EXIT_FAILURE, "target close error");

  return EXIT_SUCCESS;
}

/*
 * Determines the target path name depending on the source path. If the target
 * is a directory, the returned string will be the given target path appended
 * by the source file name. If an error occurs, this function will return NULL.
 * Otherwise, this function returns the original target name.
 */
static char *
determine_target(const char *source, const char *target)
{
  struct stat sb;
  size_t target_length;
  char *target_path;

  assert(source != NULL);
  assert(target != NULL);

  target_length = strlen(target);

  if (target_length == 0)
    return NULL;

  target_path = (char *)malloc(sizeof(char) * (PATH_MAX + 1));

  if (target_path == NULL)
    err(EXIT_FAILURE, "Not enough memory for target name");

  if (stat(target, &sb) == -1) {
    /*
     * It is OK, if target refers to a non-existing file, because we may be
     * able to create it.
     */
    if (errno == ENOENT) {
      strcpy(target_path, target);
      return target_path;
    }
  } else if (S_ISDIR(sb.st_mode)) {
    char *source_dup;
    char *source_name;
    size_t source_name_length;
    int n;
    int n_expect;

    /*
     * Target is a directory.
     * Extract the file name from source and append to target.
     */
    source_dup = strdup(source);
    source_name = basename(source_dup);
    source_name_length = strlen(source_name);

    /* Add trailing / to target, if missing. */
    if (strrchr(target, '/') == (target + target_length - 1)) {
      n = snprintf(target_path, PATH_MAX + 1, "%s%s", target, source_name);
      n_expect = target_length + source_name_length;
    } else {
      n = snprintf(target_path, PATH_MAX + 1, "%s/%s", target, source_name);
      n_expect = target_length + 1 + source_name_length;
    }

    free(source_dup);

    if (n == n_expect)
      return target_path;
  } else if (S_ISREG(sb.st_mode)) {
    strcpy(target_path, target);
    return target_path;
  }

  free(target_path);
  return NULL;
}

#if MMAP_FILES == 0
/*
 * Copies the source file to the target file using a buffer.
 */
static void
copy_file_buffer(int source_fd, int target_fd, size_t buffer_size)
{
  char *buffer = alloca(buffer_size);
  int n;

  while ((n = read(source_fd, buffer, buffer_size)) > 0) {
    if (write(target_fd, buffer, n) != n)
      err(EXIT_FAILURE, "write error");
  }
}
#else
/*
 * mmap(2) the source and target files and copies the source content to the
 * target.
 */
static void
copy_file_mmap(int source_fd, struct stat *source_sb, int target_fd)
{
  char *source;
  char *target;
  size_t written;
  size_t chunk_size;

  if (ftruncate(target_fd, source_sb->st_size) == -1)
    err(EXIT_FAILURE, "target resize error");

  written = 0;
  chunk_size = sysconf(_SC_PAGE_SIZE);

  /*
   * Read/write the files in chunks.
   * Note that mmap(2) failes for files of size 0.
   */
  while (written < source_sb->st_size) {
    size_t remaining = source_sb->st_size - written;
    size_t to_write = (chunk_size < remaining)
        ? chunk_size : remaining;

    if ((source = mmap(NULL, to_write, PROT_READ, MAP_SHARED,
          source_fd, written)) == MAP_FAILED)
      err(EXIT_FAILURE, "source mmap error");

    if ((target = mmap(NULL, to_write, PROT_WRITE, MAP_SHARED,
          target_fd, written)) == MAP_FAILED)
      err(EXIT_FAILURE, "target mmap error");

    memcpy(target, source, to_write);

    if (munmap(source, to_write) == -1)
      err(EXIT_FAILURE, "source munmap error");
    if (munmap(target, to_write) == -1)
      err(EXIT_FAILURE, "target munmap error");

    written += to_write;
  }
}
#endif

/*
 * Prints usage information and exits.
 */
static void
usage(void)
{
  (void)fprintf(stderr, "usage: %s source target\n", getprogname());
  exit(EXIT_FAILURE);
}
