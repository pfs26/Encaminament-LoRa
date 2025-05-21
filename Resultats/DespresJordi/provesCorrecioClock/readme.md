Es fan proves amb `provaCalibrarClock` amb un sleep de 5 minuts.
Els dispositius fan una cosa similar a quan és en funcionament normal en sleep:
1. Despertar
2. Realitzar transmissió per transport
3. En finalitzar TX, dormir. La TX fallarà, ja que l'aderça a qui ho envien no existeix i no reben ACK.

El temps de dormir es calcula amb CICLE - DONE on DONE es quan han acabat de fer la TX,
just abans d'anar a dormir.

En ambdós dispositius s'observa un error d'aproximadament 2.5 segons:
https://www.calculator.net/time-calculator.html?tcexpression=13h27m33.708026s%2B299.129s-13h32m30.066362s-0.079s&ctype=3&x=Calculate#expression
https://www.calculator.net/time-calculator.html?tcexpression=13h22m36.665431s%2B299.128s-13h27m32.917435s-0.079s&ctype=3&x=Calculate#expression

que equival a 2.5/5/60 = +8333 PPM, valor fora del normal


**Augmentant** el temps a 10 minuts (prova 4) s'obté que l'error és de 6.3 segons aproximadament:
https://www.calculator.net/time-calculator.html?tcexpression=29m26.056983s%2B599.129s-39m18.708318s-0.079s&ctype=3&x=Calculate#expression

No té massa sentit que una TX que falla faci augmentar l'error del clock?


Segona prova
====

A la segona prova no es realitza la transmissió després de despertar, i directament imprimeix per pantalla
i torna a dormir. El temps de dormir es continua calculant de la mateixa manera.

De nou amb la placa nova, s'obté un error de 1.6 segons:
https://www.calculator.net/time-calculator.html?tcexpression=13h45m19.760106s%2B299.906s-13h50m17.973822s-0.08s&ctype=3&x=Calculate#expression

que equival a 1.6/5/60= 5333 PPM, valor correcte

**Augmentant** el temps a 10 minuts (prova 3) s'obté un error d'aproximadament 3.2 segons, que és el doble que en la prova de 5 minuts.
En aquesta prova tampoc s'han enviat dades, i únicament es despertava i tornava a dormir.
https://www.calculator.net/time-calculator.html?tcexpression=14h10m29.060870s%2B599.906s-14h20m25.646177s-0.08s&ctype=3&x=Calculate#expression

El comportament en la placa antiga és el mateix, amb un error una mica superior però coherent amb la prova de 5 minuts.


Tercera prova
====

Es realitza transmissió de capa d'accés al medi, per veure si el que introdueix l'error addicional és l'enviament a MAC o alguna
cosa d'entremig.

S'obté un error d'aproximadament 8 segons?
https://www.calculator.net/time-calculator.html?tcexpression=17h48m08.947463s%2B599.169s-17h57m59.508761s-0.08s&ctype=3&x=Calculate#expression
Al fitxer 5 es mostra el log.

S'ha repetit de nou amb transport, i s'obté una altra vegada l'error pròxim a 8 segons. Aquest canvi vers l'anterior pot ser degut a canvis de temperatura.
Així l'error prové d'una transmissió a nivell de MAC o inferior, ja que també es produeix.

