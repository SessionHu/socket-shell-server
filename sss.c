#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define DEFAULT_SOCKET_PATH "/tmp/command_socket"

static int socket_fd = -1;
static char* socket_path = NULL;

static void cleanup_and_exit(int status) {
  if (socket_fd != -1) {
    close(socket_fd);
    socket_fd = -1;
  }
  if (socket_path != NULL) {
    if (status != 2) unlink(socket_path);
    free(socket_path);
    socket_path = NULL;
  }
  exit(status);
}

static void signal_handler(int sig) {
  fprintf(stderr, "Caught signal %d, cleaning up...\n", sig);
  cleanup_and_exit(0);
}

static void sigchld_handler(int sig) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}

int main(void) {
  // 构建 socket_path, 并分配内存
  char* tmpdir = getenv("TMPDIR");
  if (tmpdir == NULL || tmpdir[0] == '\0') {
    socket_path = strdup(DEFAULT_SOCKET_PATH);
    if (socket_path == NULL) {
      perror("strdup");
      return 1;
    }
  } else {
    const char* socket_name = "/command_socket";
    size_t path_len = strlen(tmpdir) + strlen(socket_name);
    socket_path = malloc(path_len + 1);
    if (socket_path == NULL) {
      perror("malloc");
      return 1;
    }
    snprintf(socket_path, path_len + 1, "%s%s", tmpdir, socket_name);
  }

  // 设置信号处理
  struct sigaction sa = { .sa_handler = signal_handler };
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  struct sigaction child_sa = {
    .sa_handler = sigchld_handler,
    .sa_flags = SA_RESTART | SA_NOCLDSTOP
  };
  sigemptyset(&child_sa.sa_mask);
  sigaction(SIGCHLD, &child_sa, NULL);

  // 创建和绑定 socket
  struct sockaddr_un addr = { .sun_family = AF_UNIX };
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
  addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    perror("socket");
    cleanup_and_exit(1);
  }
  socket_start:;
  if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    if (EADDRINUSE == errno) {
      if (connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        unlink(socket_path);
        goto socket_start;
      }
    }
    perror("bind");
    cleanup_and_exit(2);
  }

  // 开始监听
  if (listen(socket_fd, 1) == -1) {
    perror("listen");
    cleanup_and_exit(1);
  }

  printf("Listening on socket: %s\n", socket_path);

  // main loop
  while (1) {
    int client_fd = accept(socket_fd, NULL, NULL);
    if (client_fd == -1) {
      perror("accept");
      continue;
    }

    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      close(client_fd);
      continue;
    }

    if (pid == 0) {
      // child process
      close(socket_fd);
      FILE *client_stream = fdopen(client_fd, "r");
      if (client_stream == NULL) {
        perror("fdopen");
        close(client_fd);
        exit(1);
      }

      char* line = NULL;
      size_t len = 0;
      ssize_t read_bytes;

      while ((read_bytes = getline(&line, &len, client_stream)) != -1) {
        if (read_bytes > 0 && line[read_bytes - 1] == '\n') {
          line[read_bytes - 1] = '\0';
        }

        if (read_bytes > 0) {
          printf("Executing command: %s\n", line);
          int status = system(line);
          if (status == -1) {
            perror("system");
          } else {
            printf("Command exited with status: %d\n", status);
          }
        }
      }

      if (ferror(client_stream)) {
        perror("getline");
      }

      free(line);
      fclose(client_stream);
      exit(0);
    } else {
      // parent process
      close(client_fd);
    }
  }

  return 0;
}
