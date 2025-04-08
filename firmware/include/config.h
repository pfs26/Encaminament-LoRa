#ifndef _CONFIG_H
#define _CONFIG_H

#define LOG_LEVEL 1 // "4 = none; 1 = info; 2 = warn; 3 = err"

/* ======== */
/*   LORA   */
/* ======== */
// Pin definitions per SX1262
#define LORA_SS SS      
#define LORA_DIO1 2
#define LORA_NRESET 22
#define LORA_BUSY 4

// Configuracions per defecte - Modificable via UI
#define LORA_FREQ 868.0 // En MHz 
#define LORA_BW 125.0   // En kHz 
#define LORA_SF 7   // Entre 7 i 12 (a menor SF, major velocitat, però menor distància)
#define LORA_CODERATE 5 // Denominador de coderate. Valor entre 5 i 8, resultant en CR = [4/5, 4/6, 4/7, 4/8]
#define LORA_MAX_TX_POW -9 // en dBm, entre -9 i 22. A EU, màxim de 14 dBm
#define LORA_TX_POW -9  // en dBm, entre -9 i 22
#define LORA_SYNC_WORD 0x23 // Sync word privat (per defecte) per evitar interferències amb altres xarxes
// #define LORA_DATARATE 5 // entre 0 i 7

/* ======= */
/*   MAC   */
/* ======= */
// **NO** modificable via UI

// Número màxim de reintents, sense comptar primera transmissió (així, serien 4 intents)
// Aquest define és ÚNICAMENT per tenir-ho tot en un mateix lloc. Incrementar-lo a més de 3 generaria problemes
// ja que camp que indica reintents a PDU és de 2 bits (i per tant valor màxim 3)
#define MAC_MAX_RETRIES 3
// Valor màxim de reintents de backoff (Backoff seguirà fent-se, però no augmentarà més, per evitar desbordar uint32 i temps excessiu)
// Per valor de 10 s'obté un temps màxim aproximat de 100 segons (2^10*100/1000)
#define MAC_MAX_BEB_RETRY 10
// Slots de temps en ms per BEB. Per MAC_MAX_BEB_RETRY, el temps màxim serà MAC_BEB_SLOT_MS * 2^MAC_MAX_BEB_RETRY
#define MAC_BEB_SLOT_MS 100 
// Factor de temps addicional per recepció d'ACK, en funció de time on air de les dades enviades
// Si factor és 5 i time on air és 1ms, el timeout serà de 10 ms (dues vegades el temps esperat, anada+tornada)
#define MAC_ACK_TIMEOUT_FACTOR 3
// Número d'IDs de frames anteriors rebuts guardats (per si cal enviar "ACK")
#define MAC_QUEUE_SIZE 5
// Polinomi per CRC8 (x^8+x^2+1). 
#define MAC_CRC8_POLY 0x07
// Percentatge de duty cycle màxim, per complir amb regulacions, en tant per cent. No definir si no es vol complir
// #define MAC_DUTY_CYCLE 1

/* =========== */
/*   ROUTING   */
/* =========== */
// TTL per a cada paquet. Es descarta si arriba a 0. No modificable
#define ROUTING_MAX_TTL 5

/* ============= */
/*   TRANSPORT   */
/* ============= */
// Nombre màxim de reintents si no es rep ACK (i sol·licitat)
#define TRANSPORT_MAX_RETRIES 3 
/* Time before an ACK timeout; it shuold consider payload length (including overhead of headers),
datarate, maximum number of hops, delay between hops (TX+ACK+possible retries), and any other overheads
After each retry, the delay is doubled, applying an exponential backoff; thus, if the maximum retries
is set to 5 (6 total attempts), and the initial timeout is 1second, on the last attempt the timeout
will be set to 1*2^(6-1) sec. The real timeout follows `tout(attempt) = tout_base * 2^(attempt-1)` 
Value is specified in ms! */
#define TRANSPORT_RETRY_DELAY 5000 

// Mida de cua d'últims segments rebuts, per filtrar repeticions
#define TRANSPORT_QUEUE_SIZE 10


/* ========= */
/*   SLEEP   */
/* ========= */
// Defineix si és un node "iniciador" de xarxa.
// Enviarà SYNC sense esperar a rebre'l. Haurien de ser-ho els nodes amb rol de gateway, mantenint
// així la responsabilitat. Aquests són també els que tenen inicialització més crítica (LWAN)
#define SLEEP_IS_INITIATOR IS_GATEWAY

// Temps de sleep mínim que controlador P no superarà, en el cas que no rebi SYNC, en `ms`.
// Utilitzat per limitar el temps de sleep a un mínim, evitant consumir excessivament
#define SLEEP_MIN_SLEEP_TIME S_TO_MS(15)

// Durada d'un cicle de funcionament, en `ms`
#define SLEEP_CYCLE_DURATION MIN_TO_MS(1)
// Temps de funcionament normal, després de rebre SYNC, en `ms`
#define SLEEP_WORK_TIME S_TO_MS(30)
// Temps en que espera rebre SYNC i no s'haurien de realitzar altres
// transmissions, en `ms` Si no es rep SYNC, està inclòs en el temps de treball;
// si es rep, s'afegeix al temps de treball.
// **NO UTILITZAT**, només per possible ús futur
#define SLEEP_SYNC_TIME S_TO_MS(2)

// Factor de reducció del temps de sleep en cada cicle si no rep SYNC
// Si és 0.1, el temps de sleep es reduirà un 10% cada cicle si no rep SYNC.
#define SLEEP_SLEEP_TIME_FACTOR_NSYNC 0.1
// Factor de "controlador P" en rebre SYNC. Si és 0.5, el temps de sleep
// augmentarà la meitat del temps de recepció de SYNC.
#define SLEEP_SLEEP_TIME_FACTOR_SYNC 0.5

// Temps mínim que volem que estigui despert abans de rebre SYNC, en
// `milisegons` Evita que controlador P augmenti massa el temps de sleep, i
// deixa un marge de mínim aquest temps entre despertar i rebre SYNC (basant-se
// en temps SYNC anterior)
// **IMPORTANT** que deixi suficient marge per tal que la recepció de SYNC no es
// perdi Valors d'algun segon semblen ser adequats
#define SLEEP_MIN_TIME_BEFORE_SYNC S_TO_MS(2)

// Temps màxim d'espera per primera sincronització a la xarxa, en `ms`
// Com a últim recurs per evitar deixar ràdio + ESP actius permanentment
// Dormirà durant `SLEEP_SLEEP_TIME`
#define SLEEP_FIRST_BOOT_TOUT MIN_TO_MS(2)

/* =========== */
/*   GENERAL   */
/* =========== */
#define SELF_ADDRESS 0x03 // Adreça per defecte del node (uint8_t) - Modificable
#define IS_GATEWAY false  // Es un gateway? (true/false) - Modificable  

#define WIFI_SSID "LORA"       // Nom de la xarxa que genera el dispositiu
#define WIFI_PASSWORD NULL     // Contrasenya; NULL si és obert; sinó, de més de 8 caràcters en string ("12345678")

#endif