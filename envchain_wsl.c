#define _GNU_SOURCE

#include "envchain.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *
windows_envchain_path(void)
{
  const char *path = getenv("ENVCHAIN_WINDOWS_BIN");
  if (path == NULL || path[0] == '\0') {
    return "envchain.exe";
  }
  return path;
}

static void
strip_line_ending(char *line)
{
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    line[len - 1] = '\0';
    len--;
  }
}

static int
run_windows_envchain_stdout(const char *const *args,
                            envchain_search_callback value_callback,
                            envchain_namespace_search_callback namespace_callback,
                            void *data)
{
  int pipe_fd[2];
  pid_t pid;
  FILE *reader;
  char *line = NULL;
  size_t linecap = 0;
  int status;

  if (pipe(pipe_fd) < 0) {
    fprintf(stderr, "%s: pipe failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }

  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "%s: fork failed: %s\n", envchain_name, strerror(errno));
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 1;
  }

  if (pid == 0) {
    if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
      _exit(127);
    }
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    execvp(args[0], (char *const *)args);
    _exit(127);
  }

  close(pipe_fd[1]);
  reader = fdopen(pipe_fd[0], "r");
  if (reader == NULL) {
    close(pipe_fd[0]);
    waitpid(pid, &status, 0);
    fprintf(stderr, "%s: fdopen failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }

  while (getline(&line, &linecap, reader) >= 0) {
    strip_line_ending(line);
    if (line[0] == '\0') {
      continue;
    }

    if (value_callback != NULL) {
      char *sep = strchr(line, '=');
      if (sep == NULL) {
        continue;
      }
      *sep = '\0';
      value_callback(line, sep + 1, data);
    } else {
      namespace_callback(line, data);
    }
  }

  free(line);
  fclose(reader);

  if (waitpid(pid, &status, 0) < 0) {
    fprintf(stderr, "%s: waitpid failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return 1;
  }

  return 0;
}

static int
run_windows_envchain_with_stdin(const char *const *args, const char *stdin_text)
{
  int pipe_fd[2];
  pid_t pid;
  int status;
  size_t len;
  size_t offset;
  ssize_t wrote;

  if (pipe(pipe_fd) < 0) {
    fprintf(stderr, "%s: pipe failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }

  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "%s: fork failed: %s\n", envchain_name, strerror(errno));
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return 1;
  }

  if (pid == 0) {
    if (dup2(pipe_fd[0], STDIN_FILENO) < 0) {
      _exit(127);
    }
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    execvp(args[0], (char *const *)args);
    _exit(127);
  }

  close(pipe_fd[0]);
  len = strlen(stdin_text);
  offset = 0;
  while (offset < len) {
    wrote = write(pipe_fd[1], stdin_text + offset, len - offset);
    if (wrote <= 0) {
      close(pipe_fd[1]);
      waitpid(pid, &status, 0);
      fprintf(stderr, "%s: write failed: %s\n", envchain_name, strerror(errno));
      return 1;
    }
    offset += (size_t)wrote;
  }
  wrote = write(pipe_fd[1], "\n", 1);
  close(pipe_fd[1]);
  if (wrote < 0) {
    waitpid(pid, &status, 0);
    fprintf(stderr, "%s: write failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }

  if (waitpid(pid, &status, 0) < 0) {
    fprintf(stderr, "%s: waitpid failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return 1;
  }
  return 0;
}

static int
run_windows_envchain_noio(const char *const *args)
{
  pid_t pid = fork();
  int status;

  if (pid < 0) {
    fprintf(stderr, "%s: fork failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }
  if (pid == 0) {
    execvp(args[0], (char *const *)args);
    _exit(127);
  }
  if (waitpid(pid, &status, 0) < 0) {
    fprintf(stderr, "%s: waitpid failed: %s\n", envchain_name, strerror(errno));
    return 1;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return 1;
  }
  return 0;
}

int
envchain_search_namespaces(envchain_namespace_search_callback callback, void *data)
{
  const char *bin = windows_envchain_path();
  const char *args[] = {bin, "--list", NULL};
  return run_windows_envchain_stdout(args, NULL, callback, data);
}

int
envchain_search_values(const char *name, envchain_search_callback callback, void *data)
{
  const char *bin = windows_envchain_path();
  const char *args[] = {bin, "--list", "--show-value", name, NULL};
  return run_windows_envchain_stdout(args, callback, NULL, data);
}

void
envchain_save_value(const char *name, const char *key, char *value, int require_passphrase)
{
  const char *bin = windows_envchain_path();
  const char *args_default[] = {bin, "--set", name, key, NULL};
  const char *args_require[] = {bin, "--set", "-p", name, key, NULL};
  const char *args_norequire[] = {bin, "--set", "-P", name, key, NULL};
  const char *const *args = args_default;

  if (require_passphrase == 1) {
    args = args_require;
  }
  else if (require_passphrase == 0) {
    args = args_norequire;
  }

  if (run_windows_envchain_with_stdin(args, value) != 0) {
    fprintf(stderr, "%s: failed to save value via %s\n", envchain_name, bin);
  }
}

void
envchain_delete_value(const char *name, const char *key)
{
  const char *bin = windows_envchain_path();
  const char *args[] = {bin, "--unset", name, key, NULL};

  if (run_windows_envchain_noio(args) != 0) {
    fprintf(stderr, "%s: failed to delete value via %s\n", envchain_name, bin);
  }
}
