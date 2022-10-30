/****************************************************************************
 * Bruno Eduardo Machado Sequeira nº 2020235721                             *
 * Dinu Nicolae Bosîi nº 2019237103                                         *
 * comando para compilação:                                                 *
 * gcc -Wall -g -pthread stock_server.c -o stock_server                     *
 * **************************************************************************/

#include <sys/sem.h>
#include <semaphore.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORTO1 6000
#define PORTO2 6001
#define TAM 50
#define BUFLEN 1024
#define WORD 128
#define GROUP_1 "239.0.0.1"
#define GROUP_2 "239.0.0.2"



void funcaoadicionaStock();
void lerConfig(char config[30]);
void coloca_Zeros();
void gestao_mercados();

void printINFO(int posicao);

void carteira_carteita(int client_fd, int posicao);

void vender_Acoes(int client_fd, int posicao);
void comprar_Acoes(int client_fd, int posicao);

int verifica_User(char nome[TAM], char password[TAM]);
bool verificaUserPertence(int posicao, char mercado[TAM]);
int posicaoAcaoMercado(int posicao, char nome[TAM]);
int verificaAcaoVenda(int posicao, char nome[TAM]);
void Subscrever(int client_fd, int posicao);
int verificaMercado(char nome[TAM]);

void Porto_Config(int Porta);
void process_client(int client_fd);
int Autenticacao(int client_fd);
int VerificaNumero(char *s);

int qtdespacos(char *str);
void AddUser(char *final, char string[][TAM], int espacos);
void Refresh(char *final, int tempo);
void DeleteUser(char *final, char nome[TAM]);
void ListUser(char *final);

void erro(char *s);

void HandlerSIGKILL(int signum);

int Nuser = 0;

struct acoes {
    char nome[TAM];
    int stock;
    double preco_compra;
    double preco_venda;
};

struct carteira {
    int numAcao;
    int quantidade[6];
    char Acoes[6];
};


struct utilizador {
    pid_t my_id;
    char name[TAM];
    char password[TAM];
    int numB;
    char bolsa1[TAM];
    char bolsa2[TAM];
    double saldo;
    bool ADMIN;
    struct carteira cart;
    int Subs;
};


struct mercado {
    char id[TAM];
    struct acoes acao[3];
};


struct memory {
    int NumeroClientes;
    struct utilizador users[11];
    struct mercado mercados[2];
    int REFRESH_TIME;
};
struct memory *shm_mem;
int shmid_mem;

void erro(char *s) {
    perror(s);
    exit(1);
}

sem_t *mutex_Shared;
pthread_t mercado_thr;

struct sockaddr_in multi1;
int sock_multi1;
struct sockaddr_in multi2;
int sock_multi2;
int multicastTTL = 255;
struct sockaddr_in addrF, client_addr;
int fd;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("stock_server <porto_bolsa> <port_config> <ficheiro de configuracao>\n");
        exit(-1);
    }
    sem_unlink("MUTEX_SHARED");
    mutex_Shared = sem_open("MUTEX_SHARED", O_CREAT | O_EXCL, 0700, 1);

    lerConfig(argv[3]);
    funcaoadicionaStock();
    coloca_Zeros();
    sem_wait(mutex_Shared);
    shm_mem->NumeroClientes = 0;
    sem_post(mutex_Shared);

    //Porto_config
    if (fork() == 0) {
        Porto_Config(atoi(argv[2]));
        exit(0);
    }
    signal(SIGKILL, HandlerSIGKILL);

    //-------------------------------------criação do socket para multicast---------------------------------------------

    if ((sock_multi1 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        erro("abrir socket do multicast");
    }
    int multicastTTL = 255;
    if (setsockopt(sock_multi1, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &multicastTTL, sizeof(multicastTTL)) < 0) {
        erro("na interface local");
    }

    bzero((char *) &multi1, sizeof(multi1));
    multi1.sin_family = AF_INET;
    multi1.sin_addr.s_addr = htonl(INADDR_ANY);
    multi1.sin_port = htons(PORTO1);
    multi1.sin_addr.s_addr = inet_addr(GROUP_1);


    if ((sock_multi2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        erro("abrir socket do multicast");
    }

    if (setsockopt(sock_multi2, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &multicastTTL, sizeof(multicastTTL)) < 0) {
        erro("na interface local");
    }

    bzero((char *) &multi2, sizeof(multi2));
    multi2.sin_family = AF_INET;
    multi2.sin_addr.s_addr = htonl(INADDR_ANY);
    multi2.sin_port = htons(PORTO2);
    multi2.sin_addr.s_addr = inet_addr(GROUP_2);

    //------------------------------------------------thread-mercado----------------------------------------------------

    int index;
    if ((pthread_create(&mercado_thr, NULL, (void *) gestao_mercados, &index)) != 0) {
        erro("a criar a thread mercado");
    }

    //-------------------------------------------------socket TCP-------------------------------------------------------


    int client_addr_size;

    bzero((void *) &addrF, sizeof(addrF));
    addrF.sin_family = AF_INET;
    addrF.sin_addr.s_addr = htonl(INADDR_ANY);
    addrF.sin_port = htons((short) atoi(argv[1]));

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        erro("na funcao socket");
    if (bind(fd, (struct sockaddr *) &addrF, sizeof(addrF)) < 0)
        erro("na funcao bind...");
    if (listen(fd, 5) < 0)
        erro("na funcao listen");
    client_addr_size = sizeof(client_addr);


    printf("SERVIDOR ABERTO\n");
    int client;

    while (1) { //receção de clientes

        while (waitpid(-1, NULL, WNOHANG) > 0);

        //wait for new connection
        client = accept(fd, (struct sockaddr *) &client_addr, (socklen_t * ) & client_addr_size);

        if (client > 0) {

            if (fork() == 0) {
                close(fd);
                process_client(client);
                exit(0);
            }
            close(client);
        }

    }

    while (waitpid(-1, NULL, WNOHANG) > 0);
    return 0;
}


//--------------------------------------------FUNÇÔES - CONFIGURAÇÃO----------------------------------------------------


void HandlerSIGKILL(int signum) {
    close(sock_multi1);
    close(sock_multi2);
    sem_close(mutex_Shared);
    printf("SERVIDOR FECHADO\n");
    exit(0);
}

void printINFO(int posicao) {
    for (int i = 0; i < shm_mem->users[posicao].cart.numAcao; i++) {
        printf("%s-\n", &shm_mem->users[posicao].cart.Acoes[i]);
    }
}

void lerConfig(char *config) {
    if ((shmid_mem = shmget(IPC_PRIVATE, sizeof(struct memory), IPC_CREAT | 0700)) < 0) {
        perror("ERRO ao criar a memoria partilhada\n");
        exit(1);
    }
    if ((shm_mem = (struct memory *) shmat(shmid_mem, NULL, 0)) < (struct memory *) 1) {
        perror("ERRO na memoria partilhada\n");
        exit(1);
    }


    FILE *file = fopen(config, "r");
    if (file == NULL) {
        erro("Ficheiro config não aberto");
    }
    int count = 0, CountUser = 0, num_users, CountAcao = 0, CountMercado = 0;
    char linha[BUFLEN];

    while (!feof(file)) {
        if (fgets(linha, BUFLEN, file) != NULL) {
            linha[strlen(linha) - 1] = '\0';
        }
        if (count == 0) {
            sem_wait(mutex_Shared);
            strcpy(shm_mem->users[CountUser].name, strtok(linha, "/"));
            strcpy(shm_mem->users[CountUser].password, strtok(NULL, ";"));
            shm_mem->users[CountUser].ADMIN = true;
            shm_mem->users->numB = 0;
            sem_post(mutex_Shared);
            Nuser++;
            CountUser++;
        } else if (count == 1) {
            num_users = atoi(strtok(linha, ";"));
        } else if (count > 1 && count < num_users + 2) {
            sem_wait(mutex_Shared);
            strcpy(shm_mem->users[CountUser].name, strtok(linha, ";"));
            strcpy(shm_mem->users[CountUser].password, strtok(NULL, ";"));
            shm_mem->users[CountUser].saldo = atof(strtok(NULL, ";"));
            shm_mem->users->numB = 0;
            sem_post(mutex_Shared);
            CountUser++;
            Nuser++;
        } else if (count > num_users + 1 && count <= 10) {
            if (CountAcao == 3) {
                CountAcao = 0;
                CountMercado++;
            }
            sem_wait(mutex_Shared);
            strcpy(shm_mem->mercados[CountMercado].id, strtok(linha, ";"));
            strcpy(shm_mem->mercados[CountMercado].acao[CountAcao].nome, strtok(NULL, ";"));
            shm_mem->mercados[CountMercado].acao[CountAcao].preco_compra = atof(strtok(NULL, ";"));
            shm_mem->mercados[CountMercado].acao[CountAcao].preco_venda =
                    shm_mem->mercados[CountMercado].acao[CountAcao].preco_compra + 0.02;
            CountAcao++;
            sem_post(mutex_Shared);
        }
        count++;
    }
    shm_mem->REFRESH_TIME = 2;
    fclose(file);


}

void funcaoadicionaStock() {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            sem_wait(mutex_Shared);
            shm_mem->mercados[i].acao[j].stock = 100;
            sem_post(mutex_Shared);
        }
    }
}

void coloca_Zeros() {
    for (int i = 0; i < Nuser; i++) {
        sem_wait(mutex_Shared);
        shm_mem->users[i].Subs = 0;
        shm_mem->users[i].cart.numAcao = 0;
        for (int j = 0; j < 6; j++) {
            shm_mem->users[i].cart.quantidade[j] = 0;
        }
        sem_post(mutex_Shared);
    }
}


//--------------------------------------------FUNÇÕES PORTO_BOLSA-------------------------------------------------------


void process_client(int client_fd) {
    char buffer[BUFLEN];
    char option[WORD];
    int login = 0;
    int posicao = -1;
    int nread;
    if ((posicao = Autenticacao(client_fd)) >= 0) {
        login = 1;
    }
    sem_wait(mutex_Shared);
    shm_mem->users[posicao].my_id = getpid();
    sem_post(mutex_Shared);
    //printf("id ==== %d\n", shm_mem->users[posicao].my_id = getpid());

    if (login == 0) {
        close(client_fd);
        return;
    }

    while (1) {
        nread = read(client_fd, option, WORD);
        if (nread <= 0)
            break;
        option[nread] = '\0';

        switch (atoi(option)) {
            case 1: //Comprar
                printf("Cliente %s a comprar ações.\n", shm_mem->users[posicao].name);
                comprar_Acoes(client_fd, posicao);
                break;

            case 2: //Vender
                printf("Cliente %s a vender ações.\n", shm_mem->users[posicao].name);
                vender_Acoes(client_fd, posicao);
                break;

            case 3: //Mercado
                printf("Cliente %s a subscrever mercados.\n", shm_mem->users[posicao].name);
                Subscrever(client_fd, posicao);
                break;

            case 4: //feed
                break;

            case 5: //saldo
                printf("Cliente %s a consultar saldo.\n", shm_mem->users[posicao].name);
                sprintf(buffer, "Saldo: %.3f\n", shm_mem->users[posicao].saldo); // adicionar o nº de ações
                write(client_fd, buffer, strlen(buffer));
                break;

            case 6: //consultar ações
                printf("Cliente %s a consultar a carteira de ações.\n", shm_mem->users[posicao].name);
                carteira_carteita(client_fd, posicao);
                break;

            case 7: //exit
                printf("Cliente %s a desconectar-se.\n", shm_mem->users[posicao].name);
                sem_wait(mutex_Shared);
                shm_mem->users[posicao].my_id = -1;
                sem_post(mutex_Shared);
                return;

        }
    }
    close(client_fd);
}

int Autenticacao(int client_fd) {
    char buffer[BUFLEN];
    int nread;
    char name[WORD] = "", password[WORD] = "";
    //obter o nome
    sprintf(buffer, "Insira as suas credenciais.\nUsername:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, name, BUFLEN);
    name[nread] = '\0';
    //obter password
    sprintf(buffer, "Password:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, password, BUFLEN);
    password[nread] = '\0';
    int posicao = -1;

    if ((posicao = verifica_User(name, password)) != -1) {
        sem_wait(mutex_Shared);
        shm_mem->NumeroClientes++;
        sem_post(mutex_Shared);
        sprintf(buffer, "Autenticação Aceite\n");
        write(client_fd, buffer, strlen(buffer));
        printf("CLiente autenticado:%s\n", name);
        return posicao;

    } else {
        sprintf(buffer, "Autenticação Não aceite\n");
        write(client_fd, buffer, strlen(buffer));
        return -1;
    }
    if (shm_mem->NumeroClientes >= 5) {
        sprintf(buffer, "MAX\n");
        write(client_fd, buffer, strlen(buffer));
        return -1;
    }
}

void carteira_carteita(int client_fd, int posicao) {
    char buffer[BUFLEN];
    sprintf(buffer, "Ações que possui:\n");
    if (shm_mem->users[posicao].cart.numAcao == 0) {
        sprintf(buffer, "Não contem Ações na sua carteira!!!\n");
    } else {
        char auxiliar[BUFLEN];
        //printf("NUM %d\n\n", shm_mem->users[posicao].cart.numAcao);
        for (int i = 0; i < shm_mem->users[posicao].cart.numAcao; i++) {
            sprintf(auxiliar, "Nome: %s ------ Quantidade: %d\n", &shm_mem->users[posicao].cart.Acoes[i],
                    shm_mem->users[posicao].cart.quantidade[i]);
            strcat(buffer, auxiliar);
        }
    }
    write(client_fd, buffer, strlen(buffer));
}

void Subscrever(int client_fd, int posicao) {
    int nread;
    char buffer[BUFLEN], nomemercado[WORD];
    sprintf(buffer, "Nome do mercado a subscrever:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, nomemercado, BUFLEN);
    nomemercado[nread] = '\0';

    if (verificaMercado(nomemercado) < 0) {
        sprintf(buffer, "Mercado Inexistente\n");
    } else if (strcmp(shm_mem->users[posicao].bolsa1, nomemercado) == 0 ||
               strcmp(shm_mem->users[posicao].bolsa2, nomemercado) == 0) {
        if (verificaMercado(nomemercado) == 0) {
            sprintf(buffer, GROUP_1);
        } else {
            sprintf(buffer, GROUP_2);
        }
    } else {
        sprintf(buffer, "Não tem Acesso ao mercado.\n");
    }

    write(client_fd, buffer, strlen(buffer));

}

void comprar_Acoes(int client_fd, int posicao) {
    int nread;
    char buffer[BUFLEN], nomemercado[WORD], nomeAcao[WORD], quantidade[WORD], preco[TAM];
    sprintf(buffer, "Insira as informações da compra.\nNome do mercado:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, nomemercado, BUFLEN);
    nomemercado[nread] = '\0';
    // obter nome da ação
    sprintf(buffer, "Nome da ação:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, nomeAcao, BUFLEN);
    nomeAcao[nread] = '\0';
    //obter Quantidade
    sprintf(buffer, "Quantidade:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, quantidade, BUFLEN);
    quantidade[nread] = '\0';
    //Obter preco
    sprintf(buffer, "Preco:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, preco, BUFLEN);
    preco[nread] = '\0';
    int quantidadeF = atoi(quantidade);
    float precoUser = atof(preco);

    if (verificaUserPertence(posicao, nomemercado)) {
        int pos = verificaMercado(nomemercado);

        int posiCmP = verificaAcaoVenda(posicao, nomeAcao);
        int posiAcao = posicaoAcaoMercado(pos, nomeAcao);

        if (posiAcao == -2) {
            sprintf(buffer, "A Ação não existe!\n");
        } else {
            if (posiCmP == -2) {
                posiCmP = shm_mem->users[posicao].cart.numAcao;
            }
            //printf("----%d-----\n\n\n", posiCmP);

            if (quantidadeF % 10 != 0 || quantidadeF < 0 || quantidadeF > 100) {
                sprintf(buffer, "Quantidade tem de ser múltiplo de 10, maior que 10 e menor que 101!!\n");
            } else {
                if (shm_mem->mercados[pos].acao[posiAcao].stock - quantidadeF < 0) {
                    quantidadeF = shm_mem->mercados[pos].acao[posiAcao].stock;
                }
                float preco = shm_mem->mercados[pos].acao[posiAcao].preco_venda;
                float final = preco * quantidadeF;
                if (final > precoUser) {
                    sprintf(buffer, "Preco de venda(%.2f) é superior ao preco enviado(%.2f)!!\n", final, precoUser);
                } else {
                    if (precoUser > shm_mem->users[posicao].saldo) {
                        sprintf(buffer, "Preco é superior ao seu saldo!!\n");
                    } else {

                        sem_wait(mutex_Shared);
                        shm_mem->mercados[pos].acao[posiAcao].stock -= quantidadeF;
                        strcpy(&shm_mem->users[posicao].cart.Acoes[posiCmP], nomeAcao);
                        shm_mem->users[posicao].cart.quantidade[posiCmP] += quantidadeF;
                        shm_mem->users[posicao].saldo -= final;
                        //printf("%s///\n", &shm_mem->users[posicao].cart.Acoes[0]);
                        if (shm_mem->users[posicao].cart.quantidade[posiCmP] == quantidadeF) {
                            shm_mem->users[posicao].cart.numAcao++;
                        }
                        sem_post(mutex_Shared);

                        sprintf(buffer, "Compra realizada!(Quant: %d -- Nome: %s -- Preço: %.2f)\n", quantidadeF,
                                nomeAcao,
                                final);
                    }
                }
            }
        }

    } else {
        sprintf(buffer, "Sem permissão\n");
    }
    printINFO(posicao);
    write(client_fd, buffer, strlen(buffer));


}

bool verificaUserPertence(int posicao, char mercado[TAM]) {
    if (shm_mem->users[posicao].numB == 1) {
        if (strcmp(shm_mem->users[posicao].bolsa1, mercado) == 0) {
            return true;
        } else {
            return false;
        }
    } else if (shm_mem->users[posicao].numB == 2) {
        if (strcmp(shm_mem->users[posicao].bolsa1, mercado) == 0 ||
            strcmp(shm_mem->users[posicao].bolsa2, mercado) == 0) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }


}

int posicaoAcaoMercado(int posicao, char nome[TAM]) {
    for (int i = 0; i < 3; i++) {
        if (strcmp(shm_mem->mercados[posicao].acao[i].nome, nome) == 0) {
            return i;
        }
    }
    return -2;
}

void vender_Acoes(int client_fd, int posicao) {
    int nread;
    char buffer[BUFLEN], nomemercado[WORD], nomeAcao[WORD], quantidade[WORD], preco[TAM];
    sprintf(buffer, "Insira as informações da compra.\nNome do mercado:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, nomemercado, BUFLEN);
    nomemercado[nread] = '\0';
    // obter nome da ação
    sprintf(buffer, "Nome da ação:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, nomeAcao, BUFLEN);
    nomeAcao[nread] = '\0';
    //obter Quantidade
    sprintf(buffer, "Quantidade:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, quantidade, BUFLEN);
    quantidade[nread] = '\0';
    //Obter preco
    sprintf(buffer, "Preco:\n");
    write(client_fd, buffer, strlen(buffer));
    nread = read(client_fd, preco, BUFLEN);
    preco[nread] = '\0';
    int quantidadeF = atoi(quantidade);
    float precoUser = atof(preco);
    int posCliAcao;
    if (verificaUserPertence(posicao, nomemercado)) {

        int pos = verificaMercado(nomemercado);

        int posiAcao = posicaoAcaoMercado(pos, nomeAcao);
        if (posiAcao == -2) {
            sprintf(buffer, "A Ação não existe!\n");
        } else if ((posCliAcao = verificaAcaoVenda(posicao, nomeAcao)) == -2) {
            sprintf(buffer, "Não contem essa Ação!\n");
        } else {
            if (quantidadeF % 10 != 0 || quantidadeF < 0 || quantidadeF > 100) {
                sprintf(buffer, "Quantidade tem de ser múltiplo de 10, maior que 10 e menor que 101!!\n");
            } else {
                //printf("|%d|\n", shm_mem->users[posicao].cart.quantidade[posCliAcao]);
                if (shm_mem->users[posicao].cart.quantidade[posCliAcao] - quantidadeF < 0) {
                    sprintf(buffer, "A quantidade que deseja vender é superior à que tem!!\n");
                } else {
                    float preco = shm_mem->mercados[pos].acao[posiAcao].preco_compra;
                    float final = preco * quantidadeF;

                    if (final < precoUser) {
                        sprintf(buffer, "Preco de compra (%.2f) é inferior ao preco enviado(%.2f)!!\n", final,
                                precoUser);
                    } else {
                        //printf("---^%d^----\n", shm_mem->users[posicao].cart.quantidade[posCliAcao]);
                        sem_wait(mutex_Shared);
                        shm_mem->mercados[pos].acao[posiAcao].stock += quantidadeF;
                        shm_mem->users[posicao].saldo += final;
                        shm_mem->users[posicao].cart.quantidade[posCliAcao] -= quantidadeF;
                        if (shm_mem->users[posicao].cart.quantidade[posCliAcao] == 0) {
                            for (int i = posCliAcao; i < shm_mem->users[posicao].cart.numAcao; i++) {
                                shm_mem->users[posicao].cart.Acoes[i] = shm_mem->users[posicao].cart.Acoes[i + 1];
                                shm_mem->users[posicao].cart.quantidade[i] = shm_mem->users[posicao].cart.quantidade[i +
                                                                                                                     1];
                            }
                            shm_mem->users[posicao].cart.numAcao--;
                        }
                        sem_post(mutex_Shared);
                        sprintf(buffer, "Venda realizada!(Quant: %d -- Nome: %s -- Preço: %.3f)\n", quantidadeF,
                                nomeAcao, final);
                    }
                }
            }
        }
    } else {
        sprintf(buffer, "Sem permissão\n");
    }
    write(client_fd, buffer, strlen(buffer));
}

int verificaAcaoVenda(int posicao, char nome[TAM]) {

    for (int i = 0; i < shm_mem->users[posicao].cart.numAcao; i++) {

        if (strcmp(&shm_mem->users[posicao].cart.Acoes[i], nome) == 0) {
            return i;
        }
    }
    return -2;
}

int verificaMercado(char nome[TAM]) {
    for (int i = 0; i < 2; i++) {
        if (strcmp(shm_mem->mercados[i].id, nome) == 0) {
            return i;
        }
    }
    return -1;
}

int verifica_User(char nome[TAM], char password[TAM]) {
    for (int i = 1; i < Nuser + 1; i++) {


        if ((strcmp(shm_mem->users[i].name, nome) == 0) && (strcmp(shm_mem->users[i].password, password) == 0)) {
            return i;
        }
    }
    return -1;
}


//--------------------------------------------------PORTO_CONFIG--------------------------------------------------------


void Porto_Config(int Porta) {
    struct sockaddr_in si_minha, si_outra;

    int s, recv_len;
    int slen = sizeof(si_outra);
    char buf[BUFLEN];
    // Cria um socket para recepção de pacotes UDP
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        erro("Erro na criação do socket");
    }
    // preenche a zeros
    memset((char *) &si_minha, 0, sizeof(si_minha));

    // Preenchimento da socket address structure
    si_minha.sin_family = AF_INET;
    si_minha.sin_port = htons(Porta);
    si_minha.sin_addr.s_addr = htonl(INADDR_ANY);

    // Associa o socket à informação de endereço
    if (bind(s, (struct sockaddr *) &si_minha, sizeof(si_minha)) == -1) {
        erro("Erro no bind");
    }

    int login = 0;

    while (1) {
        fflush(stdout);
        if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, (socklen_t * ) & slen)) == -1) {
            erro("Erro no recvfrom");
        }
        buf[recv_len] = '\0';

        if (strcmp(buf, "X") == 0) {
            continue;
        } else {
            break;
        }
    }

    while (1) {
        fflush(stdout);

        if (login == 0) {
            char nome[WORD];
            char password[WORD];
            sprintf(buf, "Introduza as suas credenciais.\nUsername:\n");
            if (sendto(s, buf, strlen(buf), 0, (struct sockaddr *) &si_outra, slen) == -1) {
                erro("sendto()");
            }
            if ((recv_len = recvfrom(s, nome, WORD, 0, (struct sockaddr *) &si_outra, (socklen_t * ) & slen)) == -1) {
                erro("Erro no recvfrom");
            }
            nome[recv_len - 1] = '\0';

            sprintf(buf, "Password:\n");
            if (sendto(s, buf, strlen(buf), 0, (struct sockaddr *) &si_outra, slen) == -1) {
                erro("sendto()");
            }
            if ((recv_len = recvfrom(s, password, WORD, 0, (struct sockaddr *) &si_outra, (socklen_t * ) & slen)) ==
                -1) {
                erro("Erro no recvfrom");
            }
            password[recv_len - 1] = '\0';

            if (strcmp(nome, shm_mem->users[0].name) == 0 && strcmp(password, shm_mem->users[0].password) == 0) {
                login = 1;
                printf("administrador %s conectado á consola.\n", nome);
                sprintf(buf, "Autorização concedida.\n");
                if (sendto(s, buf, strlen(buf), 0, (struct sockaddr *) &si_outra, slen) == -1) {
                    erro("sendto()");
                }
            } else {
                sprintf(buf, "Dados de acesso incorretos.\n");
                if (sendto(s, buf, strlen(buf), 0, (struct sockaddr *) &si_outra, slen) == -1) {
                    erro("sendto()");
                }
            }
        } else {
            if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, (socklen_t * ) & slen)) == -1) {
                erro("Erro no recvfrom");
            }

            buf[recv_len] = '\0';

            int i = 0, qtd = qtdespacos(buf);
            char strings[qtd + 1][TAM];
            char *pal = strtok(buf, " ");

            char msg[BUFLEN] = "";

            while (pal != NULL) {
                strcpy(&strings[i][0], pal);
                pal = strtok(NULL, " ");
                i++;
            }

            if (strcmp(strings[0], "LIST\n") == 0) {
                ListUser(msg);


            } else if (strcmp(strings[0], "X") == 0) {
                continue;
            } else if (strcmp(strings[0], "DEL") == 0) {
                if (qtd == 1) {
                    DeleteUser(msg, strings[1]);
                } else {
                    strcpy(msg, "formato inválido: DEL {username}\n");
                }

            } else if (strcmp(strings[0], "QUIT_SERVER\n") == 0) {
                sprintf(msg, "Servidor a fechar...\n\n");
                kill(getppid(), SIGKILL);
                //close(s);
                //exit(0);
                break;
            } else if (strcmp(strings[0], "ADD_USER") == 0) {
                if (qtd == 4 || qtd == 5) {
                    AddUser(msg, strings, qtd);
                } else {
                    strcpy(msg, "formato inválido: ADD_USER {username} {password} {bolsas} {saldo}\n");
                }
            } else if (strcmp(strings[0], "QUIT\n") == 0) {
                strcpy(msg, "A fechar consola...\n\n");
                login = 0;
            } else if (strcmp(strings[0], "REFRESH") == 0) {
                if (qtd == 1) {
                    int tempo = atoi(strings[1]);
                    if (VerificaNumero(strings[1]) == 0) {
                        sprintf(msg, "O tempo possui caracteres não numéricos!\n");
                    } else {
                        Refresh(msg, tempo);
                    }
                } else {
                    strcpy(msg, "formato inválido: REFRESH {tempo}\n");
                }
            }
            else {

                    strcpy(msg, "Comando errado\n\n");
            }
            if (sendto(s, msg, strlen(msg), 0, (struct sockaddr *) &si_outra, slen) == -1) {
                erro("sendto()");
            }


        }
    }

    close(s);

}


//------------------------------------------- FUNÇỖES  PORTO_CONFIG ----------------------------------------------------


void ListUser(char *final) {
    strcpy(final, "LISTA DE UTILIZADORES\n");

    if (Nuser == 1) {
        strcat(final, "SEM UTILIZADORES\n");
    } else {
        int len = strlen(final);
        for (int i = 1; i < Nuser; i++) {
            //printf("%s<->%s\n", shm_mem->users[i].bolsa1, shm_mem->users[i].bolsa2);
            len += sprintf(final + len, "Nome: %s Pass: %s Saldo: %.3f\n", shm_mem->users[i].name,
                           shm_mem->users[i].password, shm_mem->users[i].saldo);
        }
    }

}

void DeleteUser(char *final, char nome[TAM]) {
    int posicao = -1;
    char aux[BUFLEN];
    nome[strlen(nome) - 1] = '\0';
    for (int i = 1; i < Nuser; i++) {

        if (strcmp(shm_mem->users[i].name, nome) == 0) {
            Nuser--;
            sprintf(aux, "User %s eliminado!\n\n", shm_mem->users[i].name);
            strcpy(final, aux);
            posicao = i;
            break;
        }
    }
    if (posicao != -1) {
        sem_wait(mutex_Shared);
        for (int i = posicao; i < Nuser; i++) {
            //memcpy(&shm_mem->users[i], &shm_mem->users[i + 1], sizeof(shm_mem->users[i]));
            shm_mem->users[i] = shm_mem->users[i + 1];
        }
        sem_post(mutex_Shared);

    } else {
        strcpy(final, "User não encontrado\n\n");

    }

}

void Refresh(char *final, int tempo) {
    sem_wait(mutex_Shared);
    for (int i = 0; i < 2; i++) {
        for (int k = 0; k < 3; k++) {
            shm_mem->REFRESH_TIME = tempo;
        }
    }
    sem_post(mutex_Shared);
    strcpy(final, "Tempo atualizado!\n\n");

}

void AddUser(char *final, char string[][TAM], int espacos) {

    int posicao = -1;
    for (int i = 1; i < Nuser; i++) {     //verificação se existe!
        if (strcmp(shm_mem->users[i].name, string[1]) == 0 &&
            strcmp(shm_mem->users[i].password, string[2]) == 0) { // se existe, atualiza valores
            posicao = 0;
            //printf("espaço = %d\n", espacos);
            if (espacos == 5) {
                if (VerificaNumero(string[5]) == 0) {
                    sprintf(final, "Saldo possui um caracter não numérico(ex:use como separador .)\n");
                    return;
                }
                sem_wait(mutex_Shared);
                shm_mem->users[i].numB = 2;
                strcpy(shm_mem->users[i].bolsa1, string[3]);
                strcpy(shm_mem->users[i].bolsa2, string[4]);
                //printf("%s - %s\n", shm_mem->users[i].bolsa1, shm_mem->users[i].bolsa2);
                shm_mem->users[Nuser].saldo = atof(string[5]);
                sem_post(mutex_Shared);
            } else {
                if (VerificaNumero(string[4]) == 0) {
                    sprintf(final, "1Saldo possui um caracter não numérico(ex:use como separador .)\n");
                    return;
                }
                sem_wait(mutex_Shared);
                shm_mem->users[i].numB = 1;
                shm_mem->users[i].numB = 1;
                strcpy(shm_mem->users[i].bolsa1, string[3]);
                shm_mem->users[i].saldo = atof(string[4]);
                sem_post(mutex_Shared);
            }

            break;
        }
    }
    if (posicao == 0) {
        sprintf(final, "Utilizador %s atualizado!\n\n", string[1]);
        return;
    }

// não exite nenhum user com este nome//

    bool veri = false;

    if (espacos == 6) {
        if ((strcmp(shm_mem->mercados[0].id, string[3]) == 0 || strcmp(shm_mem->mercados[1].id, string[3]) == 0) &&
            (strcmp(shm_mem->mercados[0].id, string[4]) == 0 || strcmp(shm_mem->mercados[1].id, string[4]) == 0)) {
            veri = true;
        } else {
            veri = false;
        }
    } else {
        if (strcmp(shm_mem->mercados[0].id, string[3]) == 0 || strcmp(shm_mem->mercados[1].id, string[3]) == 0) {
            veri = true;
        } else {
            veri = false;
        }
    }
    if (veri == false) {
        strcpy(final, "Mercados indisponiveis!!\n\n");
        return;

    }
    sem_wait(mutex_Shared);
    strcpy(shm_mem->users[Nuser].name, string[1]);
    strcpy(shm_mem->users[Nuser].password, string[2]);
    sem_post(mutex_Shared);
    if (espacos == 5) {
        if (VerificaNumero(string[5]) == 0) {
            sprintf(final, "Saldo possui um caracter não numérico(ex:use como separador .)\n");
            return;
        }
        sem_wait(mutex_Shared);
        shm_mem->users[Nuser].numB = 2;
        strcpy(shm_mem->users[Nuser].bolsa1, string[3]);
        strcpy(shm_mem->users[Nuser].bolsa2, string[4]);
        shm_mem->users[Nuser].saldo = atof(string[5]);
        shm_mem->users[Nuser].cart.numAcao = 0;

        sem_post(mutex_Shared);
    } else {
        if (VerificaNumero(string[4]) == 0) {
            sprintf(final, "Saldo possui um caracter não numérico(ex:use como separador .)\n");
            return;
        }
        sem_wait(mutex_Shared);
        shm_mem->users[Nuser].numB = 1;
        strcpy(shm_mem->users[Nuser].bolsa1, string[3]);
        shm_mem->users[Nuser].saldo = atof(string[4]);
        shm_mem->users[Nuser].cart.numAcao = 0;
        for (int j = 0; j < 6; j++) {
            shm_mem->users[Nuser].cart.quantidade[j] = 0;
        }
        sem_post(mutex_Shared);
    }
    sem_wait(mutex_Shared);
    shm_mem->users[Nuser].ADMIN = false;
    Nuser++;
    sem_post(mutex_Shared);
    sprintf(final, "User %s adicionado ao sistema!\n\n", string[1]);
}

int qtdespacos(char *str) {
    int espacos = 0;
    while (*str) {
        if (*str == ' ') {
            espacos++;
        }
        str++;
    }
    return espacos;
}

int VerificaNumero(char *s) {
    for (int i = 0; s[i] != '\n'; i++) {
        if (s[i] != '.') {
            if (isdigit(s[i]) == 0) {
                return 0;
            }
        }
    }
    return 1;
}


//-----------------------------------------gestao do mercado/multicast--------------------------------------------------


void gestao_mercados() {
    int rand;
    char buffer[BUFLEN] = "";
    double values[2] = {0.01, -0.01};
    srand(time(NULL));
    while (1) {
        sleep(shm_mem->REFRESH_TIME);
        sem_wait(mutex_Shared);
        for (int i = 0; i < 2; i++) {
            for (int k = 0; k < 3; k++) {
                rand = random() % 2;
                if (shm_mem->mercados[i].acao[k].preco_compra == 0.01 && rand == 2)
                    continue;
                shm_mem->mercados[i].acao[k].preco_compra = shm_mem->mercados[i].acao[k].preco_compra + values[rand];
                shm_mem->mercados[i].acao[k].preco_venda = shm_mem->mercados[i].acao[k].preco_venda + values[rand];
            }
        }
        sem_post(mutex_Shared);
        //ATUALIZAÇÕES DO MERCADO 1

        sprintf(buffer, "[%s]:\n"
                        "valores de %s : compra - %.2f | venda - %.2f\n"
                        "valores de %s : compra - %.2f | venda - %.2f\n"
                        "valores de %s : compra - %.2f | venda - %.2f\n",
                shm_mem->mercados[0].id,
                shm_mem->mercados[0].acao[0].nome, shm_mem->mercados[0].acao[0].preco_compra,
                shm_mem->mercados[0].acao[0].preco_venda,
                shm_mem->mercados[0].acao[1].nome, shm_mem->mercados[0].acao[1].preco_compra,
                shm_mem->mercados[0].acao[1].preco_venda,
                shm_mem->mercados[0].acao[2].nome, shm_mem->mercados[0].acao[2].preco_compra,
                shm_mem->mercados[0].acao[2].preco_venda);

        if (sendto(sock_multi1, buffer, strlen(buffer), 0, (struct sockaddr *) &multi1, sizeof(multi1)) <
            0)
            erro("a enviar informação do mercado...1");
        //printf("buffer:%s", buffer);
        //ATUALIZAÇÕES DO MERCADO 2

        sprintf(buffer, "[%s]:\n"
                        "valores de %s : compra - %.2f | venda - %.2f\n"
                        "valores de %s : compra - %.2f | venda - %.2f\n"
                        "valores de %s : compra - %.2f | venda - %.2f\n",
                shm_mem->mercados[1].id,
                shm_mem->mercados[1].acao[0].nome, shm_mem->mercados[1].acao[0].preco_compra,
                shm_mem->mercados[1].acao[0].preco_venda,
                shm_mem->mercados[1].acao[1].nome, shm_mem->mercados[1].acao[1].preco_compra,
                shm_mem->mercados[1].acao[1].preco_venda,
                shm_mem->mercados[1].acao[2].nome, shm_mem->mercados[1].acao[2].preco_compra,
                shm_mem->mercados[1].acao[2].preco_venda);
        if (sendto(sock_multi2, buffer, strlen(buffer), 0, (struct sockaddr *) &multi2, sizeof(multi2)) < 0)
            erro("a enviar informaadadação do mercado.2");
        //printf("buffer:%s", buffer);
    }
}

