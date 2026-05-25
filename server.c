#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include "incident.h"

#define PORT           8080
#define MAX_CLIENTS    10
#define HISTORIQUE_MAX 100
#define QUEUE_MAX      10

/* File d'attente des messages d'urgence */
typedef struct {
    MessageUrgence msg;
    int            sock_expediteur;
} EntreeQueue;

static EntreeQueue queue[QUEUE_MAX];
static int         queue_debut = 0;
static int         queue_fin   = 0;
static int         queue_taille = 0;

static sem_t           sem_vide;
static sem_t           sem_plein;
static pthread_mutex_t mutex_queue = PTHREAD_MUTEX_INITIALIZER;

/* Historique des incidents */
static Incident        historique[HISTORIQUE_MAX];
static int             nb_incidents = 0;
static pthread_mutex_t mutex_hist   = PTHREAD_MUTEX_INITIALIZER;

/* Liste des clients */
static int             clients[MAX_CLIENTS];
static int             nb_clients = 0;
static pthread_mutex_t mutex_clients = PTHREAD_MUTEX_INITIALIZER;

static void broadcast_incident(const Incident *inc) {
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++)
        send(clients[i], inc, sizeof(Incident), 0);
    pthread_mutex_unlock(&mutex_clients);
}

static void ajouter_client(int sock) {
    pthread_mutex_lock(&mutex_clients);
    if (nb_clients < MAX_CLIENTS)
        clients[nb_clients++] = sock;
    pthread_mutex_unlock(&mutex_clients);
}

static void retirer_client(int sock) {
    pthread_mutex_lock(&mutex_clients);
    for (int i = 0; i < nb_clients; i++) {
        if (clients[i] == sock) {
            clients[i] = clients[--nb_clients];
            break;
        }
    }
    pthread_mutex_unlock(&mutex_clients);
    close(sock);
}


static void envoyer_ack(int sock, const char *ack) {
    char buf[ACK_MAX] = {0};
    strncpy(buf, ack, ACK_MAX - 1);
    send(sock, buf, ACK_MAX, 0);
}

static void traiter_urgence(int sock, const MessageUrgence *msg) {
    int val_vide;
    sem_getvalue(&sem_vide, &val_vide);

    if (val_vide == 0) {
        printf("[URGENCE] File pleine — message de '%s' refuse.\n", msg->pseudo);
        envoyer_ack(sock, ACK_PLEIN);
        return;
    }

    sem_trywait(&sem_vide);
    pthread_mutex_lock(&mutex_queue);
    int position_avant = queue_taille;
    queue[queue_fin].msg = *msg;
    queue[queue_fin].sock_expediteur = sock;
    queue_fin = (queue_fin + 1) % QUEUE_MAX;
    queue_taille++;
    pthread_mutex_unlock(&mutex_queue);

    sem_post(&sem_plein);

    if (position_avant == 0) {
        envoyer_ack(sock, ACK_ENVOYE);
        printf("[URGENCE] Message de '%s' en tete de file.\n", msg->pseudo);
    } else {
        envoyer_ack(sock, ACK_EN_ATTENTE);
        printf("[URGENCE] Message de '%s' en attente (position %d).\n",
               msg->pseudo, position_avant + 1);
    }
}

static void gerer_admin(int sock) {
    printf("[ADMIN] Administrateur connecte (sock=%d)\n", sock);
    while (1) {
        char cmd[16] = {0};
        int n = recv(sock, cmd, sizeof(cmd), 0);
        if (n <= 0) break;

        if (strcmp(cmd, "LIRE") == 0) {
            printf("[ADMIN] Commande LIRE — attente message...\n");
            int ret;
            do {
                ret = sem_trywait(&sem_plein);
            } while (ret != 0 && errno == EINTR);

            if (ret != 0) {
                if (errno == EAGAIN) {
                    MessageUrgence vide = {0};
                    send(sock, &vide, sizeof(MessageUrgence), 0);
                    printf("[ADMIN] Aucun message d'urgence a lire.\n");
                    continue;
                }
                perror("sem_trywait");
                break;
            }

            pthread_mutex_lock(&mutex_queue);
            EntreeQueue entree = queue[queue_debut];
            queue_debut = (queue_debut + 1) % QUEUE_MAX;
            queue_taille--;
            int restant = queue_taille;
            pthread_mutex_unlock(&mutex_queue);

            sem_post(&sem_vide);

            send(sock, &entree.msg, sizeof(MessageUrgence), 0);
            printf("[ADMIN] Message de '%s' transmis. (%d en attente)\n",
                   entree.msg.pseudo, restant);

            if (entree.sock_expediteur != -1) {
                envoyer_ack(entree.sock_expediteur, ACK_LU);
                printf("[ACK] '%s' notifie : message lu.\n", entree.msg.pseudo);
            }
        }
    }
    printf("[ADMIN] Administrateur deconnecte.\n");
    close(sock);
}

static void *gerer_client(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    char role[16] = {0};
    int nr = recv(sock, role, sizeof(role), 0);
    if (nr <= 0) {
        close(sock);
        return NULL;
    }

    if (strcmp(role, "ADMIN") == 0) {
        gerer_admin(sock);
        return NULL;
    }

    /* On ajoute à la liste des clients broadcast */
    ajouter_client(sock);

    /* Envoyer l'historique des incidents */
    pthread_mutex_lock(&mutex_hist);
    int count = nb_incidents;
    send(sock, &count, sizeof(int), 0);
    for (int i = 0; i < count; i++)
        send(sock, &historique[i], sizeof(Incident), 0);
    pthread_mutex_unlock(&mutex_hist);

    /* Boucle de réception des incidents et messages d'urgence */
    char buf[512];
    while (1) {
        memset(buf, 0, sizeof(buf));
        nr = recv(sock, buf, sizeof(buf), 0);
        if (nr <= 0) break;

        if (nr == (int)sizeof(Incident)) {
            Incident inc;
            memcpy(&inc, buf, sizeof(Incident));
            printf("[INCIDENT] %s | %s | %s\n",
                   inc.pseudo, inc.type, inc.localisation);
            pthread_mutex_lock(&mutex_hist);
            if (nb_incidents < HISTORIQUE_MAX)
                historique[nb_incidents++] = inc;
            pthread_mutex_unlock(&mutex_hist);
            broadcast_incident(&inc);
        }
        else if (nr == (int)sizeof(MessageUrgence)) {
            MessageUrgence msg;
            memcpy(&msg, buf, sizeof(MessageUrgence));
            traiter_urgence(sock, &msg);
        }
    }

    retirer_client(sock);
    printf("[INFO] Client deconnecte (sock=%d)\n", sock);
    return NULL;
}

int main(void) {
    int server_sock;
    struct sockaddr_in addr;

    sem_init(&sem_vide, 0, QUEUE_MAX);
    sem_init(&sem_plein, 0, 0);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    listen(server_sock, MAX_CLIENTS);

    printf("+==============================================+\n");
    printf("| SMART CITY — Serveur demarre sur port %d   |\n", PORT);
    printf("| File d'attente : %d messages max           |\n", QUEUE_MAX);
    printf("+==============================================+\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int *csock = malloc(sizeof(int));
        if (!csock) continue;
        *csock = accept(server_sock, (struct sockaddr *)&client_addr, &len);
        if (*csock < 0) { free(csock); continue; }

        printf("[INFO] Nouvelle connexion : %s (sock=%d)\n",
               inet_ntoa(client_addr.sin_addr), *csock);

        pthread_t tid;
        pthread_create(&tid, NULL, gerer_client, csock);
        // Permet au thread de se nettoyer tout seul à sa fin
        pthread_detach(tid);
    }

    sem_destroy(&sem_vide);
    sem_destroy(&sem_plein);
    close(server_sock);
    return 0;
}