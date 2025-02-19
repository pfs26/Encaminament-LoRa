#ifndef _CONFIG_H
#define _CONFIG_H

#define DEBUG

#ifdef DEBUG
#define _PM(a) Serial.print(millis()); Serial.print(": "); Serial.println(a)
#define _PP(a) Serial.print(a)
#define _PL(a) Serial.println(a)
#define _PX(a) Serial.println(a, HEX)
#else
#define _PM(a)
#define _PP(a)
#define _PL(a)
#define _PX(a)
#endif

#endif