#ifndef _CONFIG_H
#define _CONFIG_H

/* ======== */
/*   LORA   */
/* ======== */
// Pin definitions per SX1262
#define LORA_SS SS      
#define LORA_DIO1 2
#define LORA_NRESET 22
#define LORA_BUSY 4

// Configuracions per defecte
#define LORA_FREQ 868.0 // En MHz 
#define LORA_BW 125.0   // En kHz 
#define LORA_SF 7   // Entre 7 i 12 (a menor SF, major velocitat, però menor distància)
#define LORA_CODERATE 5 // Denominador de coderate. Valor entre 5 i 8, resultant en CR = [4/5, 4/6, 4/7, 4/8]
#define LORA_MAX_TX_POW -9 // en dBm, entre -9 i 22. A EU, màxim de 14 dBm
#define LORA_TX_POW -9  // en dBm, entre -9 i 22
#define LORA_SYNC_WORD 0x23 // Sync word privat (per defecte) per evitar interferències amb altres xarxes


#define RADIOLIB_LORAWAN_JOIN_EUI  0x0000000000000000

// the Device EUI & two keys can be generated on the TTN console
#define RADIOLIB_LORAWAN_DEV_EUI   0x70B3D57ED006EF81
#define RADIOLIB_LORAWAN_APP_KEY   0x30, 0x45, 0xF3, 0xB9, 0xB1, 0xBC, 0xE5, 0xB5, 0xF9, 0x6A, 0xB4, 0xA7, 0x79, 0x39, 0x74, 0xE6
#define RADIOLIB_LORAWAN_NWK_KEY   0xC6, 0xC7, 0x8D, 0x65, 0xF8, 0xAD, 0x37, 0xBF, 0x6E, 0x9C, 0x52, 0x8D, 0x52, 0x63, 0x1F, 0x56

// Defineix si els uplinks esperaran confirmació o no, per LoRaWAN
#define LW_CONFIRMED_UPLINKS 0
// Port utilitzat per defecte per enviar uplinks a lorawan
#define LW_DEFAULT_UPLINK_PORT 1

/* ======= */
/*   MAC   */
/* ======= */
// Número màxim de reintents, sense comptar primera transmissió (així, serien 4 intents)
// Aquest define és ÚNICAMENT per tenir-ho tot en un mateix lloc. Incrementar-lo a més de 3 generaria problemes
// ja que camp que indica reintents a PDU és de 2 bits (i per tant valor màxim 3)
#define MAC_MAX_RETRIES 3

// Valor màxim de reintents de backoff (Backoff seguirà fent-se, però no augmentarà més, per evitar desbordar uint32 i temps excessiu)
// Per valor de 10 s'obté un temps màxim aproximat de 100 segons (2^10*100/1000)
#define MAC_MAX_BEB_RETRY 10

// Slots de temps en ms per BEB. Per MAC_MAX_BEB_RETRY, el temps màxim serà MAC_BEB_SLOT_MS * 2^MAC_MAX_BEB_RETRY
#define MAC_BEB_SLOT_MS 100 

// Factor de temps addicional per recepció d'ACK, en funció de time on air de la mida d'un ACK enviat (mida headers MAC)
// Si factor és 5 i time on air és 1ms, el timeout serà de 5 ms (dues vegades el temps esperat, anada+tornada)
// Inici de TOUT es genera després de realitzat la transmissió
#define MAC_ACK_TIMEOUT_FACTOR 3

// Número d'IDs de frames anteriors rebuts guardats (per si cal enviar "ACK")
#define MAC_QUEUE_SIZE 5

// Polinomi per CRC8 (x^8+x^2+1). 
#define MAC_CRC8_POLY 0x07

// NO verificat. Implementació molt bàsica. Limita el temps de cicle d'un node per evitar superar el limit de temps d'airtime. En %.
// No definir per no utilitzar-lo. 
// #define MAC_DUTY_CYCLE 0

/* =========== */
/*   ROUTING   */
/* =========== */
// TTL per a cada paquet. Es descarta si arriba a 0.
#define ROUTING_MAX_TTL 5

/* ============= */
/*   TRANSPORT   */
/* ============= */
// Nombre màxim de reintents si no es rep ACK (i sol·licitat)
#define TRANSPORT_MAX_RETRIES 3 

// Temps d'espera d'ACK, en `ms`. Hauria de considerar mida de les dades, número de salts, i altres factors (BEB, reintents, etc.)
// Després de cada reintent, el temps d'espera incrementa exponencialment. 
// En un intent de transmissió `r`, i per un temps base `t`, el temps d'espera serà t*2^(r-1) 
#define TRANSPORT_RETRY_DELAY 10000 

// Mida de cua d'últims segments rebuts, per filtrar repeticions
#define TRANSPORT_QUEUE_SIZE 10


/* ========= */
/*   SLEEP   */
/* ========= */
// Port que utilitza l'aplicació de SLEEP
#define SLEEP_PORT 0x01

// Defineix si és un node "iniciador" de xarxa. Enviarà SYNC sense esperar a rebre'l. 
// #define SLEEP_IS_INITIATOR !IS_GATEWAY
#define SLEEP_IS_INITIATOR false

// Durada d'un cicle de funcionament, en `ms`
#define SLEEP_CYCLE_DURATION (uint64_t) MIN_TO_MS(1)

// Error del clock, en `ppm`
#define SLEEP_CLOCK_ERROR (0)
// Temps de correcció d'error del clock, en `ms`
#define SLEEP_CLOCK_CORRECTION (int64_t)((SLEEP_CYCLE_DURATION * SLEEP_CLOCK_ERROR / 1000000.0))

// Temps extra d'espera de recepció de SYNC, i previ a primera recepció esperada, en `ms`
// No hauria de ser molt gran; temps tolerància ja considera el temps màxim teòric
#define SLEEP_EXTRA_TIME (uint64_t) S_TO_MS(5) 

// Percentatge addicional que s'aplicarà a temps de tolerància, per considerar CSMA, etc.
#define SLEEP_DELTA_EXTRA 0.25

// Bytes de dades que cada node pot afegir
#define SLEEP_DATASIZE_PER_NODE 1 

// Quantitat de dispositius de la xarxa. Ha de ser igual a major als dispositius reals de la xarxa.
// Es recomana que sigui major per facilitar la tasca d'afegir-ne de nous, i evitar haver de programar-los tots de nou
#define SLEEP_QUANTITAT_DISPOSITIUS 5

/* =========== */
/*   GENERAL   */
/* =========== */
// Nivell d'error per les traces. 4 = Desactivat, 1 = Informació, 2 = Advertència, 3 = Error
#define LOG_LEVEL 1 

// Adreça del dispositiu (uint8_t)
#define SELF_ADDRESS 0x03 
// Especifica si té capacitats LoRaWAN (extensió de gateway)
#define IS_GATEWAY false  

#endif
