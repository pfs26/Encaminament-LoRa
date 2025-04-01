#ifndef _WEBPAGE_H
#define _WEBPAGE_H

// Nombre màxim de clients que es poden connectar simultaniament. Màxim 10. Millor 1 per evitar desincronitzacions.
#define MAX_CLIENTS 1   
// Canal wifi a utilitzar (2.4GHz)
#define WIFI_CHANNEL 6  

// Interval entre comprovacions de sol·licituds de DNS en ms. 30 sembla ser un valor adequat
#define DNS_INTERVAL 30 

bool webpage_start();
void webpage_stop();

#endif