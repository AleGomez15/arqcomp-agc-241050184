# agc1 - Lab 6: Memoria y E/S del AGC

**Autora:** Alejandra Gómez - 241050184
**Lab:** 6 - DSKY mínimo, mapa de memoria y E/S mapeada
**Plataforma:** Wokwi + ESP32

## Funciona

- Mapa de memoria sin traslapes: RAM 0x000-0x03F (64 palabras), E/S 0x040-0x04F (16 registros), ROM 0x100-0x13F (64 palabras).
- `escribirMem` protege la ROM y activa alarma en accesos fuera del mapa.
- Escáner I2C detecta 2 dispositivos: OLED 0x3C y MPU6050 0x68 (AD0 a GND).
- Botones con INPUT_PULLUP en GPIO 25, 26, 27, 32 actualizan IO_BOTONES cada 20 ms.
- DSKY muestra Verbo / Sustantivo, R1, R2, R3, comando aplicado y ANCLA.
- V01 N01 muestra la matrícula en octal. V06 N60 muestra IO_BOTONES, IO_JOY_SW e IO_TICKS.
- Comando inválido V21 N02 muestra OPR y conserva el comando anterior V01 N01.

## Midió

- I2C: Dispositivo en 0x3C y dispositivo en 0x68. Total: 2.
- ROM inicial: ROM[0]=0x2410, ROM[1]=0x5016, ROM[2]=0x0005.
- Prueba `escribirMem(0x100, 5)`: ROM[0] siguió en 0x2410, alarma ACTIVA.
- Prueba `leerMem(0x200)`: fuera del mapa, devuelve 0, alarma ACTIVA.
- Prueba `escribirMem(IO_TICKS, 999)`: valor permaneció en 123, alarma inactiva.
- Direcciones físicas: RAM = 0x3FFC1E1A, ROM = 0x3F400120.
- IO_BOTONES: Ninguno=0000, CAMPO=0001, MAS=0010, MENOS=0100, ENTR=1000, CAMPO+MAS=0011.
- V01 N01: R1=022020, R2=050026, R3=000005.
- Prueba INPUT sin pull-up: 20 lecturas en 2 arranques. Pines flotantes en GPIO 32 en una ejecución y GPIO 26 en otra.
- Rebote MAS: muestreo cada 1 ms, 1 a 2 transiciones 0->1 en una sola pulsación.
- Pulsación sostenida: MAS ~3 s, verbo pasó de 06 a 16 con un solo incremento.
- ANCLA: C40A24-Oct 15 2026-00:11:19-15894. MAC simulado fijo en Wokwi.

## Argumentó

La RAM conserva lo que se escribe, pero los registros de E/S no: unos reflejan botones y otros el tiempo. Por eso no se pueden tratar igual, aunque estén en el mismo mapa.

La protección de ROM es lógica, no física. Que la ROM esté en flash (0x3F400120) no basta por sí sola, la que evita el cambio es `escribirMem`. Lo mismo con IO_TICKS: es solo lectura, intentar escribirlo no lo cambia.

Sin INPUT_PULLUP el pin queda flotante y puede leerse como pulsación que no existió. Por eso el pull-up es necesario, y además se necesita antirrebote: el DSKY solo actúa con 2 muestras estables y flanco de subida, por eso mantener MAS 3 s solo incrementa una vez.

Si AD0 del MPU6050 se conecta a 3.3V, su dirección cambia de 0x68 a 0x69, lo que permite usar dos sensores iguales en el mismo bus. Y el ANCLA de Wokwi no es único por usuario porque la MAC simulada es fija, en una placa real el identificador viene del eFuse y es más difícil de reproducir.

## Evidencia

- Video: [Video Lab 6](https://drive.google.com/file/d/1SBuM67AbxNFzziSUyr7dhHHwuA9Fy0ZE/view?usp=drivesdk)
- PDF: [Reporte Lab 6](https://drive.google.com/file/d/1AmuTpBlbJsmlURxd2_KgNAjGf_MwD0BF/view?usp=drivesdk)


## Declaración de uso de IA

Se utilizó IA como apoyo para revisar el circuito, detectar problemas en código y conexiones de los botones, y organizar los resultados. El montaje, pruebas y mediciones son propias.
