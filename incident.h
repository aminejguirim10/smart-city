#ifndef INCIDENT_H
#define INCIDENT_H

#include <stdint.h>

#define TYPE_MAX         32
#define LOCALISATION_MAX 64
#define DESCRIPTION_MAX  128
#define PSEUDO_MAX       32
#define HORODATAGE_MAX   32

#define NB_TYPES      4
#define TYPE_ACCIDENT "ACCIDENT"
#define TYPE_INCENDIE "INCENDIE"
#define TYPE_PANNE    "PANNE ELECTRIQUE"
#define TYPE_ROUTE    "ROUTE BLOQUEE"

typedef struct {
    char type[TYPE_MAX];
    char localisation[LOCALISATION_MAX];
    char description[DESCRIPTION_MAX];
    char pseudo[PSEUDO_MAX];
    char horodatage[HORODATAGE_MAX];
} Incident;

/* Message d'urgence client -> admin */
#define MSG_MAX    256
#define ACK_MAX    16

#define ACK_ENVOYE    "MSG_ENVOYE"
#define ACK_EN_ATTENTE "MSG_ATTENTE"
#define ACK_PLEIN     "MSG_PLEIN"
#define ACK_LU        "MSG_LU"

typedef struct {
    char pseudo[PSEUDO_MAX];
    char contenu[MSG_MAX];
    char horodatage[HORODATAGE_MAX];
} MessageUrgence;

/* Tag pour distinguer les paquets */
#define TAG_INCIDENT 0x01
#define TAG_URGENCE  0x02
#define TAG_ROLE     0x03

/* Paquet générique : 1 byte de tag + payload */
typedef struct {
    uint8_t tag;
    Incident incident;
} PaquetIncident;

typedef struct {
    uint8_t tag;
    MessageUrgence urgence;
} PaquetUrgence;

typedef struct {
    uint8_t tag;
    char    role[16];
} PaquetRole;

#endif /* INCIDENT_H */