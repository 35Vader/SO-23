//
// Created by Alexandra Candeias on 03/05/2023.
//
#include "sserver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

volatile sig_atomic_t keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

typedef enum {
    RUNNING,
    SUCCESS,
    FAILURE
} ExecutionState;

typedef struct {
    char *programName;
    char **args;
    int numArgs;
    time_t startTime;
    ExecutionState state;
} ExecutionInfo;

ExecutionInfo *executions = NULL;
int numExecutions = 0;


char *findProgram(char *programName) {
    // Procurar programa no PATH
    char *path = getenv("PATH");
    char *token = strtok(path, ":");
    char *fullPath = malloc(BUFFER_SIZE * sizeof(char));

    while (token != NULL) {
        snprintf(fullPath, BUFFER_SIZE, "%s/%s", token, programName);
        if (access(fullPath, F_OK | X_OK) == 0) {
            return fullPath;
        }
        token = strtok(NULL, ":");
    }

    free(fullPath);
    return NULL;
}

void handleExecution(int sockfd, char *programName, int numArgs, char **args) {
    // Executar programa
    int pid = fork();
    if (pid == 0) {
        char *fullPath = findProgram(programName);
        if (fullPath == NULL) {
            perror("Programa não encontrado");
            exit(1);
        }
        char **newArgs = malloc((numArgs + 2) * sizeof(char *));
        newArgs[0] = fullPath;
        for (int i = 0; i < numArgs; i++) {
            newArgs[i + 1] = args[i];
        }
        newArgs[numArgs + 1] = NULL;
        execv(fullPath, newArgs);
        perror("Erro na execução");
        exit(1);
    } else if (pid < 0) {
        perror("Erro no fork");
        return;
    }
    // Armazenar informações sobre a execução
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    printf("Programa '%s' executado às %02d:%02d:%02d\n", programName, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

void handleClient(int sockfd) {
    char buffer[BUFFER_SIZE];
    int numArgs;

    while (keepRunning) {
        // Receber nome do programa
        if (recv(sockfd, buffer, BUFFER_SIZE, 0) < 0) {
            perror("Erro na receção");
            return;
        }
        char *programName = strtok(buffer, " ");

        // Receber número de argumentos
        if (recv(sockfd, &numArgs, sizeof(int), 0) < 0) {
            perror("Erro na receção");
            return;
        }

        // Receber argumentos
        char **args = malloc(numArgs * sizeof(char *));
        for (int i = 0; i < numArgs; i++) {
            if (recv(sockfd, buffer, BUFFER_SIZE, 0) < 0) {
                perror("Erro na receção");
                return;
            }
            args[i] = strdup(buffer);
        }

        // Executar programa
        handleExecution(sockfd, programName, numArgs, args);

        // Libertar memória dos argumentos
        for (int i = 0; i < numArgs; i++) {
            free(args[i]);
        }
        free(args);
    }
}

int createSocket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        return -1;
    }

    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Erro ao definir opções do socket");
        close(sockfd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Erro ao fazer bind do socket");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, MAX_CLIENTS) < 0) {
        perror("Erro ao ouvir conexões no socket");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int acceptConnection(int sockfd) {
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    int connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
    if (connfd < 0) {
        if (errno == EINTR) {
            return -1;
        }
        perror("Erro ao aceitar conexão");
        return -1;
    }

    return connfd;
}


int main() {
    int sockfd = createSocket();
    if (sockfd < 0) {
        exit(1);
    }

    signal(SIGINT, intHandler);

    while (keepRunning) {
        int connfd = acceptConnection(sockfd);
        if (connfd < 0) {
            continue;
        }

        handleClient(connfd);

        close(connfd);
    }

}
