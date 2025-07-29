#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <signal.h>

#define DEFAULT_SOCKET_PATH "/tmp/command_socket"

static int socket_fd = -1;
static char* socket_path = NULL;

static void signal_handler(int sig) {
  fprintf(stderr, "Caught signal %d, cleaning up...\n", sig);
  if (socket_fd != -1) {
    close(socket_fd);
  }
  if (socket_path != NULL) {
    unlink(socket_path);
    free(socket_path);
  }
  exit(0);
}

static void sigchld_handler(int sig) {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(void) {
  char* tmpdir = getenv("TMPDIR");
  if (tmpdir == NULL) {
    socket_path = strdup(DEFAULT_SOCKET_PATH);
    if (socket_path == NULL) {
      perror("strdup");
      return 1;
    }
  } else {
    socket_path = malloc(strlen(tmpdir) + strlen("/command_socket") + 1);
    if (socket_path == NULL) {
      perror("malloc");
      return 1;
    }
    strcat(socket_path, tmpdir);
    strcat(socket_path, "/command_socket");
  }

  socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    perror("socket");
    free(socket_path);
    return 1;
  }

  struct sockaddr_un addr = { .sun_family = AF_UNIX };
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
  unlink(socket_path);

  if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(socket_fd);
    free(socket_path);
    return 1;
  }

  if (listen(socket_fd, 1) == -1) {
    perror("listen");
    close(socket_fd);
    free(socket_path);
    return 1;
  }

  printf("Listening on socket: %s\n", socket_path);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGQUIT, signal_handler);

  struct sigaction child_sa = {
    .sa_handler = sigchld_handler,
    .sa_flags = SA_RESTART | SA_NOCLDSTOP // SA_RESTART很重要
  };
  sigemptyset(&child_sa.sa_mask);
  sigaction(SIGCHLD, &child_sa, NULL);

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
      ssize_t read;
   
      while ((read = getline(&line, &len, client_stream)) != -1) {
        if (line[read - 1] == '\n') {
          line[read - 1] = '\0';
          read--;
        }
   
        if (read > 0) {
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
