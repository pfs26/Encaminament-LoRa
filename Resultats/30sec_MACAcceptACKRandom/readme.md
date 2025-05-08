Tests amb aleatorietat a capa MAC
=====

El node iniciador té funcionament normal.
El node receptor té la capa MAC modificada, on en rebre un frame a capa MAC
té una probabilitat aleatoria P d'acceptar-lo (i generar ACK).
Així, hi ha la probabilitat `P^r`, on `r` és el nombre màxim d'intents de transmissió
que no accepti el frame (i per tant com si no rebés el missatge de SYNC).

Aquest node no té definit a cap node següent per reenviar el SYNC, així que per aquest
motiu es simula, definit que té un temps per realitzar la tasca aleatori,
introduint un temps variable i per tant canvis en el temps de sleep.
Això simularia els intents de transmissió que aquest podria fer per reenviar-ho al següent node.
El temps aleatori és entre `[TX, r·k·TX]`, on `k` és el factor de recepció d'ACK, i `r` el nombre 
d'intents màxim, i `TX` el temps per realitzar una transmissió.
Per simplicitat, s'ha definit aquest temps de transmissió al calculat per transmetre 25B
