#include "mail_server.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int cmp_mail_server_info(const void *left, const void *right) {
  return ((struct mail_server_info *)left)->priority -
         ((struct mail_server_info *)right)->priority;
}

struct mail_server_info *get_mail_servers(char *domain, int *count) {
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid_t pid = fork();
  if (pid == -1) {
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    close(pipefd[0]);

    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    char *args[] = {"dig", "mx", domain, NULL};
    execvp(args[0], args);

    perror("execvp");
    exit(EXIT_FAILURE);
  }

  close(pipefd[1]);

  char buffer[8196];
  ssize_t nbytes = read(pipefd[0], buffer, sizeof(buffer) - 1);
  close(pipefd[0]);
  wait(NULL);

  char *p = strstr(buffer, "ANSWER:");
  p += 7;
  char *answer_count_pointer = p;
  p = strchr(p, ',');
  p = 0;
  p++;
  int answer_count = atoi(answer_count_pointer);
  if (answer_count == 0) {
    *count = 0;
    return NULL;
  }

  struct mail_server_info *mail_server_infos =
      malloc(sizeof(struct mail_server_info) * answer_count);

  p = strstr(buffer, "ANSWER SECTION");
  p = strchr(p, '\n') + 1;

  char *entry;
  int counter = 0;
  while (p && counter < answer_count) {
    entry = p;
    p = strchr(p, '\n');
    *p = 0;
    p++;

    entry = strtok(entry, "\t");
    entry = strtok(NULL, "\t");
    entry = strtok(NULL, "\t");
    entry = strtok(NULL, "\t");
    entry = strtok(NULL, "\t");

    entry = strtok(entry, " ");
    struct mail_server_info info;
    info.priority = atoi(entry);

    entry = strtok(NULL, " ");
    strcpy(info.server_hostname, entry);
    mail_server_infos[counter++] = info;
  }
  *count = counter;

  qsort(mail_server_infos, counter, sizeof(struct mail_server_info),
        cmp_mail_server_info);

  return mail_server_infos;
}
