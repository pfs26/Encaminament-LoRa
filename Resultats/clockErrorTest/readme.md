Obtenir PPM
===========

S'ha utilitzat el fitxer de `exemples/provaCalibrarClock.cpp` per obtenir l'error en PPM.
S'enviava per consola quan es despertava, i a través de l'eina `ts` s'imprimia per consola el temps amb precisió de milisegons de quan es rebia:
```sh
picocom --omap spchex,tabhex,crhex,lfhex --baud 921600 --imap lfcrlf /dev/ttyUSB0 \
| ts '%H:%M:%.S' >> resultVella.log
```

Es pot calcular llavors la diferència entre l'anterior despert i l'actual, i comparar-ho amb el temps de sleep que indicava l'anterior missatge.
Determinant l'error `E`, i el temps de dormir `D`, es pot calcular l'error en PPM amb `E/D*1e6`.
