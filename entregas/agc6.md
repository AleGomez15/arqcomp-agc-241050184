# Lab 11: DMA y video

**Nombre:** Alejandra Gomez
**Matrícula:** 241050184
**Equipo:** AGC - 241050184

## Funciona

La AGC ya tiene framebuffer (VRAM) en `0x800 - 0x9FF` y controlador DMA en `0x04E - 0x052` (Origen, Destino, Contador, Control, Estado).

Con `V35` se prende el modo video y se dibuja la imagen que está en ROM en `0x140`: marco + 2 diagonales. El DMA la copia de ROM a VRAM sin que la CPU tenga que mover palabra por palabra.

Tiene 2 modos:
- Burst: copia todo de golpe, turnos CPU = 0
- Normal: copia 1 palabra por turno, comparte el bus con la CPU

También tiene doble buffer con `IO_BUFFER_ACTIVO`, protección de ROM (si intentas escribir en ROM marca error y prende alarma) y la CPU sigue haciendo la suma 1..7 = 28 en RAM[10].

Código: `agc/agc.ino`

## Midió

- Suma CPU 1..7 = 28 en RAM[10]
- DMA copia 512 palabras de ROM a VRAM
- En modo burst: turnos CPU = 0, es más rápido pero bloquea el bus
- En modo normal: turnos CPU = 512, dura más porque comparte el bus
- Comando `D` muestra: turnos CPU, duración en us y estado del DMA (0 libre, 1 ocupado, 2 listo, 3 error)
- Comando `B` mide el bloque de 1024 bytes en us
- Cola de tareas: 8 espacios, si se llena prende alarma 1202
- Filtro: F1 / F8 / F64, Timer: comando T

## Argumentó

El DMA sí libera a la CPU. En burst es mejor cuando necesitas mover toda la imagen rápido, pero bloquea el bus. En modo normal es mejor cuando la CPU tiene que seguir trabajando, aunque tarde más.

La VRAM permite dibujar por pixeles sin usar la CPU para cada pixel, y el doble buffer evita que se vea parpadeo al cambiar de imagen. Por eso el Lab 11 sí funciona como sistema de video y no solo como DSKY.

## Evidencia

- Video: https://drive.google.com/file/d/1C3XWAEXZCjNdK77zvu0r5q5ROFSoURqD/view?usp=drivesdk
- PDF: https://drive.google.com/file/d/1o_pQHyNsoKeKxqA-ph_U0JUvad-x4Inx/view?usp=drivesdk
- Código: `agc/agc.ino`


## Declaración de uso de IA

Usé IA para ayuda en la redacción y revisión del código, la implementación y las pruebas son mías.
