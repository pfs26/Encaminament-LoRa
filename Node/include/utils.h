#include "config.h"

#ifdef DEBUG
#define _PM(a) Serial.print(millis()); Serial.print(": "); Serial.println(a)
#define _PP(a) Serial.print(a)
#define _PL(a) Serial.println(a)
#define _PX(a) Serial.print(a, HEX)
#else
#define _PM(a)
#define _PP(a)
#define _PL(a)
#define _PX(a)
#endif

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))