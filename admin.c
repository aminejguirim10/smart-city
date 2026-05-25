#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "incident.h"

#define PORT      8080
#define SERVER_IP "127.0.0.1"


void afficher_message(const MessageUrgence *msg) {
    printf("\n\033[1;31m");
    printf("+-----------------------------------------------+\n");
    printf("|         MESSAGE D'URGENCE RECU                |\n");
    printf("+-----------------------------------------------+\n");
    printf("| De      : %-35s |\n", msg->pseudo);
    printf("| Heure   : %-35s |\n", msg->horodatage);
    printf("| Contenu : %-35s |\n", msg->contenu);
    printf("+-----------------------------------------------+\n");
    printf("\033[0m\n");
}

static int recevoir_exact(int sock, void *buf, size_t taille) {
    size_t recu = 0;
    char *dest = buf;

    while (recu < taille) {
        int n = recv(sock, dest + recu, taille - recu, 0);
        if (n <= 0)
            return n;
        recu += (size_t)n;
    }
    return (int)recu;
}

void afficher_menu(void) {
    printf("\n");
    printf("+==================================================+\n");
    printf("|        SMART CITY — CONSOLE ADMINISTRATEUR       |\n");
    printf("+==================================================+\n");
    printf("| 1. Lire le message d'urgence en attente          |\n");
    printf("| 0. Quitter                                       |\n");
    printf("+==================================================+\n");
    printf("Votre choix > ");
    fflush(stdout);
}

int main(void) {
    int sock;
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    printf("[*] Connexion au serveur...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }
    printf("[OK] Connecte.\n");

    char role[16] = "ADMIN";
    send(sock, role, sizeof(role), 0);
    printf("[OK] Identifie comme administrateur.\n");

    int choix;
    while (1) {
        afficher_menu();
        if (scanf("%d", &choix) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        getchar();

        if (choix == 0) {
            printf("[*] Au revoir.\n");
            break;
        }
        else if (choix == 1) {
            char cmd[16] = "LIRE";
            send(sock, cmd, sizeof(cmd), 0);

            printf("\n[*] Demande envoyee au serveur...\n");
            MessageUrgence msg = {0};
            int n = recevoir_exact(sock, &msg, sizeof(MessageUrgence));
            if (n <= 0) {
                printf("[!] Connexion perdue.\n");
                break;
            }
            if (msg.pseudo[0] == '\0') {
                printf("\n[!] Aucun message d'urgence a lire pour le moment.\n");
                continue;
            }
            afficher_message(&msg);
        }
        else {
            printf("[!] Choix invalide.\n");
        }
    }

    close(sock);
    return 0;
}