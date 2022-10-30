/****************************************************************************
 * Bruno Eduardo Machado Sequeira nº 2020235721                             *
 * Dinu Nicolae Bosîi nº 2019237103                                         *
 * comando para compilação:                                                 *
 * gcc -Wall -g -pthread operations_terminal.c -o operations_terminal       *
 * **************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORTO1 6000
#define PORTO2 6001
#define GROUP_1 "239.0.0.1"
#define GROUP_2 "239.0.0.2"
#define TRUE 1
#define FALSE 0
#define BUFLEN    1024
#define WORD 128
#define TAM 50

pthread_t Market[2];
char IPS[2][TAM];
int fd;
int mc_read;

void *threadHelpful(void *arg);
void erro(char *msg);
void check(int c);
void AddGroup(int sock, const char* group_add, struct ip_mreq mreq);
void HandlerTSTP(int signum);
void LeaveGroup(int sock, const char* group_add, struct ip_mreq mreq);


int groups = 0;
char addr1[WORD] = "";
char addr2[WORD] = "";
int subs[2];

int multi_len1,multi_len2;
struct sockaddr_in multi1,multi2;
int sock_multi1, sock_multi2;

struct ip_mreq grp_mreq;

char buf[BUFLEN];

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("operations_terminal <host> <port> \n");
        exit(-1);
    }

    char endServer[100];
    struct hostent *hostPtr;

    strcpy(endServer, argv[1]);
    if ((hostPtr = gethostbyname(endServer)) == 0)
        erro("Não consegui obter endereço");

    char mensagem[BUFLEN];
    char buffer[BUFLEN];
    struct sockaddr_in addr;

    int nread = 0;

    bzero((void *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ((struct in_addr *) (hostPtr->h_addr))->s_addr;
    addr.sin_port = htons((short) atoi(argv[2]));

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        erro("socket");
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        erro("Connect");

    int login = 0;
    while (1) {
        nread = read(fd, buffer, BUFLEN);
        buffer[nread] = '\0';
        printf("%s", buffer);
        if (strcmp(buffer, "Insira as suas credenciais.\nUsername:\n") == 0) {
            scanf("%s", mensagem);
            write(fd, mensagem, strlen(mensagem));
        }
        else if ((strcmp(buffer, "Password:\n")) == 0) {
            scanf("%s", mensagem);
            write(fd, mensagem, strlen(mensagem));
        }
        else if ((strcmp(buffer, "Autenticação Aceite\n")) == 0) {
            //printf("Login: Sucesso\n");
            login = 1;
            break;
        }else if ((strcmp(buffer, "Autenticação Não aceite\n")) == 0) {
            //printf("Falha no Login\n");
            break;
        }else if((strcmp(buffer, "MAX\n")) == 0){
            printf("Número máximo de clientes\n");
            break;
        }
        else {
            printf("%s\n", buffer);
            break;
        }

    }
    if (login == 0) {
        close(fd);
        return 0;
    }
 

    if((sock_multi1 = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ){
        perror("ERRO NA FUNCAO SOCKET MULTI\n");
    }

    int multicastTTL = 255; // by default TTL=1; the packet is not transmitted to other networks
    if (setsockopt(sock_multi1, SOL_SOCKET, SO_REUSEADDR, (void *) &multicastTTL, sizeof(multicastTTL)) < 0){
        perror("socket opt");
        exit(1);
    }
    bzero((char*)&multi1,sizeof(multi1));
    multi1.sin_family = AF_INET;
    multi1.sin_addr.s_addr = htonl(INADDR_ANY);
    multi1.sin_port = htons(PORTO1);
    multi1.sin_addr.s_addr = inet_addr(GROUP_1);

    if (bind(sock_multi1, (struct sockaddr*)&multi1, sizeof(multi1))) {
        close(sock_multi1);
        erro("binding datagram socket");
    }

    if((sock_multi2 = socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("ERRO MULTIAST");

    }
    if (setsockopt(sock_multi2, SOL_SOCKET, SO_REUSEADDR, (void *) &multicastTTL, sizeof(multicastTTL)) < 0){
        perror("socket opt");
        exit(1);
    }
    bzero((char*)&multi2,sizeof(multi2));
    multi2.sin_family = AF_INET;
    multi2.sin_addr.s_addr = htonl(INADDR_ANY);
    multi2.sin_port = htons(PORTO2);
    multi2.sin_addr.s_addr = inet_addr(GROUP_2);
 
    if (bind(sock_multi2, (struct sockaddr*)&multi2, sizeof(multi2))) {
        close(sock_multi2);
        erro("binding datagram socket");
    }

    

    char menu[BUFLEN] = "---------------------------------------------------------------\n"
                        "(1) Comprar Ação\n"
                        "(2) Vender Ação\n"
                        "(3) Subscrever Mercado\n"
                        "(4) Feed de atualizações\n"
                        "(5) Consultar saldo\n"
                        "(6) Ver Ações que possui\n"
                        "(7) Sair\n"
                        "Opção:\n";

    char option[WORD];

  
    
    while(1) {
        fflush(stdout);
        printf("%s", menu);
        scanf("%s", option);

        write(fd, option, strlen(option));

        if ((strcmp(option, "1")) == 0 || (strcmp(option,"2")==0)) { //COMPRAR/VENDER
            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';
            printf("%s",buffer);
            scanf("%s", mensagem);
            write(fd,mensagem,strlen(mensagem));
            //nome Ação
            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';
            printf("%s",buffer);
            scanf("%s", mensagem);
            write(fd,mensagem,strlen(mensagem));
            //quantidade
            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';
            printf("%s",buffer);
            scanf("%s", mensagem);
            write(fd,mensagem,strlen(mensagem));
            //preco
            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';
            printf("%s",buffer);
            scanf("%s", mensagem);
            write(fd,mensagem,strlen(mensagem));

            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';
            printf("%s",buffer);

   
        }
       
        else if ((strcmp(option, "3")) == 0) {  // SUBSCRIÇÕES
            //escolha do mercado
            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';
            printf("%s",buffer);
            scanf("%s", mensagem);
            write(fd,mensagem,strlen(mensagem));

            //receção do endereço/erro
            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';

            if(strcmp(buffer, "Mercado Inexistente\n") == 0) {
                printf("%s",buffer);
                continue;
            }
            if(strcmp(buffer, "Não tem Acesso ao mercado.\n") == 0) {
                printf("%s", buffer);
                continue;
            }


            if (groups == 0) {
                printf("endereço obtido :%s\n", buffer);
                strcpy(addr1, buffer);
                if(strcmp(addr1,"239.0.0.1")==0){
                    AddGroup(sock_multi1, buffer, grp_mreq);
                }else{
                    AddGroup(sock_multi2, buffer, grp_mreq);
                }
                
                groups += 1;
            }
            else if(groups == 1){
                printf("endereço obtido :%s\n", buffer);
                strcpy(addr2, buffer);
                 if(strcmp(addr2,"239.0.0.1")==0){
                    AddGroup(sock_multi1, buffer, grp_mreq);
                }else{
                    AddGroup(sock_multi2, buffer, grp_mreq);
                }
                groups += 1;
            }
            else {
                printf("já subscreveu a todos os mercados a que tem acesso\n");
            }
            
        }

        else if ((strcmp(option, "4")) == 0) {  //FEED DE ATUALIZAÇÕES
            printf("Apresentação do mercado\n");
            signal(SIGTSTP,HandlerTSTP);
            int conta;
            
            
            if(groups == 0){
                printf("Não tem subscrições!\n");
            }
            else if(groups == 1){
                if(strcmp("239.0.0.1",addr1)==0){
                    conta =  sock_multi1;
                   
                }else{
                    conta =  sock_multi2;
                }   
                pthread_create(&Market[0],NULL,threadHelpful,&conta);
                pthread_join(Market[0],NULL);
                }
            else if(groups == 2){
                pthread_create(&Market[0],NULL,threadHelpful,&sock_multi1);
                pthread_create(&Market[1],NULL,threadHelpful,&sock_multi2);
                pthread_join(Market[0],NULL);
                pthread_join(Market[1],NULL);
            }

            
            
        

        }else if ((strcmp(option, "5")) == 0 || strcmp(option,"6")==0) { //CARTEIRA
            check(nread = read(fd, buffer, BUFLEN));
            buffer[nread] = '\0';
            printf("%s",buffer);
        }

        else if ((strcmp(option, "7")) == 0) { //SAIR
            if(groups == 1){
                if (strcmp(addr1, "239.0.0.1") == 0){
                    LeaveGroup(sock_multi1, addr1, grp_mreq);
                } else {
                    LeaveGroup(sock_multi2, addr1, grp_mreq);
                }
            }
            else if(groups == 2) {
                if (strcmp(addr1, "239.0.0.1") == 0){
                    LeaveGroup(sock_multi1, addr1, grp_mreq);
                    LeaveGroup(sock_multi2, addr2, grp_mreq);
                }
                else {
                    LeaveGroup(sock_multi1, addr2, grp_mreq);
                    LeaveGroup(sock_multi2, addr1, grp_mreq);
                }
            }
            break;
        }
        else{
            printf("Opção Incorreta\n");
        }
    }
    close(fd);
    return 0;
}


//------------------------------------------------FUNÇÕES DE SUPORTE----------------------------------------------------


void check(int c){
    if (c < 0) {
        erro("em read()");
    }
    else
        return;
}

void erro(char *msg) {
    perror(msg);
    exit(1);
}

void AddGroup(int sock, const char* group_add, struct ip_mreq mreq) {

    mreq.imr_multiaddr.s_addr = inet_addr(group_add);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if ((setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                    (void*) &mreq, sizeof(mreq))) < 0) {
        close(sock);
        erro("setsockopt() failed");
    }
}

void LeaveGroup(int sock, const char* group_add, struct ip_mreq mreq) {

    mreq.imr_multiaddr.s_addr = inet_addr(group_add);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if ((setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                    (void*) &mreq, sizeof(mreq))) < 0) {
        close(sock);
        erro("setsockopt() failed");
    }
}

void HandlerTSTP(int signum){
    pthread_cancel(Market[0]);
    if(groups == 2){
        pthread_cancel(Market[1]);
    }       
    printf("\nFecho da apresentação do mercado\n");
}

void *threadHelpful(void *arg){
    int sock = *((int *)arg);
    struct sockaddr_in auxi;
    
    int mc_read = 2;
    if(sock == sock_multi1){
        auxi = multi1;
    }else{
        auxi = multi2;
    }
    while(mc_read>0){
        int Len = sizeof(auxi);
        mc_read = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *) &auxi, (socklen_t*)&Len);
        buf[mc_read] = '\0';
        if(mc_read == 0){
            break;
        }
        printf("%s",buf);
    }
    return 0;
}

