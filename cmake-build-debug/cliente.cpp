//
// Created by Alexandra Candeias on 03/05/2023.
//

#include "cliente.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345
#define SUCCESS 0
#define PROGRAM_NOT_FOUND 1
#define PROGRAM_ERROR 2
#define SERVER_COMMUNICATION_ERROR 3

#define PROGRAM_NOT_FOUND_MSG "O programa não foi encontrado ou não pôde ser executado."
#define PROGRAM_ERROR_MSG "O programa foi executado, mas retornou um erro."
#define SERVER_COMMUNICATION_ERROR_MSG "Ocorreu um erro ao se comunicar com o servidor."


int main(int argc, char *argv[]) {
    // Verifica se a opção "execute" foi passada na linha de comando
    if (argc < 3 || strcmp(argv[1], "execute") != 0) {
        printf("Opção inválida\n");
        exit(1);
    }

    // Obtém o nome do programa a ser executado
    char *program_name = argv[2];

    // Obtém os argumentos adicionais
    char **args = &argv[3];
    int num_args = argc - 3;

    // Cria um novo processo filho para executar o programa
    int pid = fork();
    if (pid == 0) {
        // Processo filho: executa o programa
        execvp(program_name, args);
        perror("execvp");
        exit(1);
    } else if (pid > 0) {
        // Processo pai: espera pela conclusão da execução do programa filho
        int status;
        wait(&status);
        // Informa o estado da execução ao servidor
        if (WIFEXITED(status)) {
            printf("Programa executado com sucesso\n");
        } else {
            printf("Ocorreu um erro na execução do programa\n");
        }
    } else {
        // Erro ao criar o processo filho
        perror("fork");
        exit(1);
    }

    // Extrai o nome do programa e os argumentos da linha de comando
    char *programa = argv[2];
    char *argumentos = "";
    for (int i = 3; i < argc; i++) {
        strcat(argumentos, argv[i]);
        strcat(argumentos, " ");
    }

    // Cria um socket para a conexão com o servidor
    int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Não foi possível criar o socket\n");
        return 1;
    }

    // Define o endereço do servidor
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    // Conecta ao servidor
    if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Erro ao conectar-se ao servidor\n");
        return 1;
    }

    // Envia as informações sobre a execução do programa ao servidor
    char mensagem[200];
    sprintf(mensagem, "%s %s", programa, argumentos);
    if (write(socket_desc, mensagem, strlen(mensagem)) < 0) {
        printf("Erro ao enviar mensagem ao servidor\n");
        return 1;
    }

    // Fecha a conexão com o servidor e o socket
    close(socket_desc);
    return 0;
}

struct ProgramExecution {
    char programName[256];
    char **args;
    int status;
};

int connectToServer() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("Não foi possível criar o socket\n");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Não foi possível conectar-se ao servidor\n");
        return -1;
    }

    return sockfd;
}

int countArgs(char **args) {
    int count = 0;
    while (args[count] != NULL) {
        count++;
    }
    return count;
}

void sendExecutionInfo(struct ProgramExecution execInfo) {
    // Conectar ao servidor
    int sockfd = connectToServer();
    if (sockfd < 0) {
        printf("%s\n", SERVER_COMMUNICATION_ERROR_MSG);
        exit(SERVER_COMMUNICATION_ERROR);
    }

    // Enviar nome do programa e argumentos
    int numArgs = countArgs(execInfo.args);
    char msg[1024];
    sprintf(msg, "EXECUTE:%s:%d", execInfo.programName, numArgs);
    for (int i = 0; i < numArgs; i++) {
        sprintf(msg + strlen(msg), ":%s", execInfo.args[i]);
    }
    write(sockfd, msg, strlen(msg));

    // Receber resposta do servidor
    char response[1024];
    int numBytes = read(sockfd, response, 1024);
    response[numBytes] = '\0';

    // Analisar resposta do servidor
    int code = atoi(response);
    if (code == 0) {
        execInfo.status = SUCCESS;
    } else if (code == 1) {
        execInfo.status = PROGRAM_NOT_FOUND;
        printf("%s\n", PROGRAM_NOT_FOUND_MSG);
    } else if (code == 2) {
        execInfo.status = PROGRAM_ERROR;
        printf("%s\n", PROGRAM_ERROR_MSG);
    } else {
        execInfo.status = SERVER_COMMUNICATION_ERROR;
        printf("%s\n", SERVER_COMMUNICATION_ERROR_MSG);
    }

    // Fechar conexão com o servidor
    close(sockfd);
}
