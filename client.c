#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include "incident.h"

#define PORT      8080
#define SERVER_IP "127.0.0.1"
#define QUEUE_MAX 10

static int  sock_global = -1;
static char mon_pseudo[PSEUDO_MAX] = {0};

static pthread_mutex_t mtx_affichage = PTHREAD_MUTEX_INITIALIZER;

/* pour synchronisation ACK */
static char            dernier_ack[ACK_MAX] = {0};
static pthread_mutex_t mtx_ack  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond_ack = PTHREAD_COND_INITIALIZER;
static int             ack_recu = 0;

void horodatage_maintenant(char *buf, int taille) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, taille, "%Y-%m-%d %H:%M:%S", tm_info);
}

void saisir_chaine(const char *invite, char *dest, int taille) {
    printf("%s", invite);
    fflush(stdout);
    if (fgets(dest, taille, stdin) != NULL)
        dest[strcspn(dest, "\n")] = '\0';
}

void afficher_alerte(const Incident *inc) {
    const char *couleur = "\033[0m";
    if      (strcmp(inc->type, TYPE_ACCIDENT) == 0) couleur = "\033[1;31m";
    else if (strcmp(inc->type, TYPE_INCENDIE) == 0) couleur = "\033[1;33m";
    else if (strcmp(inc->type, TYPE_PANNE)    == 0) couleur = "\033[1;34m";
    else if (strcmp(inc->type, TYPE_ROUTE)    == 0) couleur = "\033[1;35m";

    pthread_mutex_lock(&mtx_affichage);
    printf("\n%s", couleur);
    printf("+---------------------------------------------------+\n");
    printf("|                 ALERTE RECUE                      |\n");
    printf("+---------------------------------------------------+\n");
    printf("| Type         : %-35s |\n", inc->type);
    printf("| Localisation : %-35s |\n", inc->localisation);
    printf("| Description  : %-35s |\n", inc->description);
    printf("| Signale par  : %-35s |\n", inc->pseudo);
    printf("| Heure        : %-35s |\n", inc->horodatage);
    printf("+---------------------------------------------------+\n");
    printf("\033[0m");
    printf("Votre choix > ");
    fflush(stdout);
    pthread_mutex_unlock(&mtx_affichage);
}


void *recevoir_incidents(void *arg) {
    int sock = *(int *)arg;
    int count = 0;

    /* Historique au moment de la connexion */
    if (recv(sock, &count, sizeof(int), 0) > 0 && count > 0) {
        pthread_mutex_lock(&mtx_affichage);
        printf("\n=== INCIDENTS EXISTANTS (%d) ===\n", count);
        Incident inc;
        for (int k = 0; k < count; k++) {
            if (recv(sock, &inc, sizeof(Incident), 0) > 0) {
                printf("[%d] %s | %s | %s | %s | %s\n",
                       k + 1, inc.type, inc.localisation,
                       inc.description, inc.pseudo,
                       inc.horodatage);
            }
        }
        printf("====================================\n\n");
        pthread_mutex_unlock(&mtx_affichage);
    }

    /* Boucle de réception en temps réel */
    while (1) {
        char buf[512] = {0};
        int bytes = recv(sock, buf, sizeof(buf), 0);
        if (bytes <= 0) {
            pthread_mutex_lock(&mtx_affichage);
            printf("\n[!] Connexion au serveur perdue.\n");
            pthread_mutex_unlock(&mtx_affichage);

            pthread_mutex_lock(&mtx_ack);
            strncpy(dernier_ack, "", ACK_MAX);
            ack_recu = 1;
            pthread_cond_signal(&cond_ack);
            pthread_mutex_unlock(&mtx_ack);
            break;
        }

        /* Vérifier si c'est un ACK */
        if (bytes == ACK_MAX) {
            if (strcmp(buf, ACK_LU) == 0) {
                pthread_mutex_lock(&mtx_affichage);
                printf("\n\033[1;32m");
                printf("+---------------------------------------------------+\n");
                printf("| NOTIFICATION ADMINISTRATEUR                        |\n");
                printf("+---------------------------------------------------+\n");
                printf("| Votre message d'urgence a bien ete recu.           |\n");
                printf("| Merci.                                             |\n");
                printf("+---------------------------------------------------+\n");
                printf("\033[0m");
                printf("Votre choix > ");
                fflush(stdout);
                pthread_mutex_unlock(&mtx_affichage);
            } else {
                pthread_mutex_lock(&mtx_ack);
                memcpy(dernier_ack, buf, ACK_MAX);
                ack_recu = 1;
                pthread_cond_signal(&cond_ack);
                pthread_mutex_unlock(&mtx_ack);
            }
            continue;
        }

        /* Sinon c'est un Incident broadcasté */
        if (bytes == (int)sizeof(Incident)) {
            Incident inc;
            memcpy(&inc, buf, sizeof(Incident));
            afficher_alerte(&inc);
        }
    }
    return NULL;
}

void afficher_menu(void) {
    pthread_mutex_lock(&mtx_affichage);
    printf("\n");
    printf("+==================================================+\n");
    printf("| SMART CITY — Bonjour %-27s|\n", mon_pseudo);
    printf("+==================================================+\n");
    printf("| 1. Signaler un ACCIDENT                          |\n");
    printf("| 2. Signaler un INCENDIE                          |\n");
    printf("| 3. Signaler une PANNE ELECTRIQUE                 |\n");
    printf("| 4. Signaler une ROUTE BLOQUEE                    |\n");
    printf("| 5. Envoyer un message d'urgence a l'admin        |\n");
    printf("| 0. Quitter                                       |\n");
    printf("+==================================================+\n");
    printf("Votre choix > ");
    fflush(stdout);
    pthread_mutex_unlock(&mtx_affichage);
}

int remplir_incident(int choix, Incident *inc) {
    memset(inc, 0, sizeof(Incident));
    switch (choix) {
        case 1: strncpy(inc->type, TYPE_ACCIDENT, TYPE_MAX - 1); break;
        case 2: strncpy(inc->type, TYPE_INCENDIE, TYPE_MAX - 1); break;
        case 3: strncpy(inc->type, TYPE_PANNE,    TYPE_MAX - 1); break;
        case 4: strncpy(inc->type, TYPE_ROUTE,    TYPE_MAX - 1); break;
        default:
            printf("[!] Choix invalide\n");
            return -1;
    }
    saisir_chaine("Localisation : ", inc->localisation, LOCALISATION_MAX);
    saisir_chaine("Description  : ", inc->description,  DESCRIPTION_MAX);
    if (strlen(inc->localisation) == 0 || strlen(inc->description) == 0) {
        printf("[!] Champs invalides\n");
        return -1;
    }
    strncpy(inc->pseudo, mon_pseudo, PSEUDO_MAX - 1);
    horodatage_maintenant(inc->horodatage, HORODATAGE_MAX);
    return 0;
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\n[*] Deconnexion...\n");
    if (sock_global != -1)
        close(sock_global);
    exit(0);
}

static void attendre_ack(char *ack_out) {
    pthread_mutex_lock(&mtx_ack);
    while (!ack_recu)
        pthread_cond_wait(&cond_ack, &mtx_ack);
    memcpy(ack_out, dernier_ack, ACK_MAX);
    ack_recu = 0;
    pthread_mutex_unlock(&mtx_ack);
}

int main(void) {
    int sock;
    struct sockaddr_in serv_addr;
    pthread_t thread_reception;
    int choix;
    Incident inc;

    signal(SIGINT, handle_sigint);

    printf("+==============================================+\n");
    printf("| SMART CITY — Systeme de signalement          |\n");
    printf("+==============================================+\n");
    saisir_chaine("Votre pseudo : ", mon_pseudo, PSEUDO_MAX);
    if (strlen(mon_pseudo) == 0)
        strncpy(mon_pseudo, "Anonyme", PSEUDO_MAX - 1);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    sock_global = sock;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "IP invalide\n");
        close(sock);
        return 1;
    }

    printf("[*] Connexion au serveur...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    printf("[OK] Connecte au serveur.\n");

    char role[16] = "CLIENT";
    send(sock, role, sizeof(role), 0);

    pthread_create(&thread_reception, NULL, recevoir_incidents, &sock);
    // Permet au thread de se nettoyer tout seul à sa fin
    pthread_detach(thread_reception);

    while (1) {
        afficher_menu();
        if (scanf("%d", &choix) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }
        getchar();

        if (choix == 0) {
            printf("[*] Au revoir %s\n", mon_pseudo);
            break;
        }
        else if (choix >= 1 && choix <= 4) {
            if (remplir_incident(choix, &inc) == 0) {
                if (send(sock, &inc, sizeof(Incident), 0) < 0) {
                    perror("send");
                    break;
                }
                printf("[OK] Incident envoye.\n");
            }
        }
        else if (choix == 5) {
            MessageUrgence msg;
            memset(&msg, 0, sizeof(msg));
            strncpy(msg.pseudo, mon_pseudo, PSEUDO_MAX - 1);
            horodatage_maintenant(msg.horodatage, HORODATAGE_MAX);
            saisir_chaine("Message d'urgence : ", msg.contenu, MSG_MAX);
            if (strlen(msg.contenu) == 0) continue;
            if (send(sock, &msg, sizeof(MessageUrgence), 0) < 0) {
                perror("send");
                break;
            }

            char ack[ACK_MAX] = {0};
            attendre_ack(ack);

            if (strcmp(ack, ACK_ENVOYE) == 0) {
                printf("\n\033[1;32m");
                printf("+---------------------------------------------------+\n");
                printf("| Votre message a ete envoye a l'administrateur.    |\n");
                printf("| Vous serez notifie quand il sera lu.               |\n");
                printf("+---------------------------------------------------+\n");
                printf("\033[0m");
            } else if (strcmp(ack, ACK_EN_ATTENTE) == 0) {
                printf("\n\033[1;33m");
                printf("+---------------------------------------------------+\n");
                printf("| Votre message est en liste d'attente.             |\n");
                printf("| La boite est occupee pour le moment.              |\n");
                printf("| Vous serez notifie des que votre message est lu.  |\n");
                printf("+---------------------------------------------------+\n");
                printf("\033[0m");
            } else if (strcmp(ack, ACK_PLEIN) == 0) {
                printf("\n\033[1;31m");
                printf("+---------------------------------------------------+\n");
                printf("| La file d'attente est pleine (%d messages).      |\n", QUEUE_MAX);
                printf("| Reessayez plus tard.                              |\n");
                printf("+---------------------------------------------------+\n");
                printf("\033[0m");
            }
        }
        else {
            printf("[!] Choix invalide.\n");
        }
    }

    close(sock);
    return 0;
}