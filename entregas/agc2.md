# agc2 - Lab 7: CPU e ISA

**Autora:** Alejandra Gómez - 241050184
**Lab:** 7 - CPU de 16 bits, ISA y DSKY
**Plataforma:** Wokwi + ESP32

## Funciona

- CPU de 16 bits con ciclo fetch-decode-execute: FETCH lee la instrucción que apunta PC y la deja en IR, DECODE separa opcode y dirección, EXECUTE realiza la operación.
- Formato de instrucción: 4 bits superiores = opcode, 12 bits inferiores = dirección. `codificar` desplaza opcode 12 posiciones, `decodificar` separa los dos campos con entero sin signo para no propagar el bit de signo.
- ISA de 8 instrucciones: TC, CA, CS, AD, TS, XCH, BZF, HALT.
- Mapa de memoria del Lab 6 sin cambios: RAM 0x000-0x03F, E/S 0x040-0x04F, ROM 0x100-0x13F.
- CPU separada en `agc_cpu.h` y `agc_cpu.cpp`, memoria en `agc_mem.h` y `agc_mem.cpp`.
- DSKY conserva controles del Lab 6 y suma: V15 paso a paso, V16 correr hasta HALT, V37 reiniciar CPU, N70 ver PC, A e IR, N71 ver Q, conteo de ciclos y bandera de detención.
- Programa de prueba suma 1 hasta N, cargado en ROM desde 0x110. Variables en RAM: 0x00A suma (RAM[10]), 0x020 contador, 0x022 constante -1.

## Programa suma 1 hasta N

- 0x110: CA 0x020 - A = cont
- 0x111: BZF 0x119 - si cont = 0, ir a fin
- 0x112: CA 0x00A - A = suma
- 0x113: AD 0x020 - A = suma + cont
- 0x114: TS 0x00A - suma = A
- 0x115: CA 0x020 - A = cont
- 0x116: AD 0x022 - A = cont + (-1)
- 0x117: TS 0x020 - cont = A
- 0x118: TC 0x110 - repetir
- 0x119: HALT - fin

## Midió

- Comprobación fórmula ciclos = 9N + 3, 9 instrucciones por vuelta + 3 del final (CA, BZF, HALT):
- N=4: RAM[10]=10, ciclos medidos 39, 9N+3=39.
- N=3: RAM[10]=6, ciclos medidos 30, 9N+3=30.
- Tiempo por opcode, promedio de 100 repeticiones, CPU a 240 MHz, ya restado sobrecosto de medición de 30.0 ciclos:
- TC: 10.69 micros/paso, 1344.8 ciclos/paso, 5603 ns/paso.
- CA: 12.17 micros/paso, 1629.0 ciclos/paso, 6788 ns/paso.
- CS: 11.41 micros/paso, 1619.2 ciclos/paso, 6747 ns/paso.
- AD: 11.30 micros/paso, 1626.5 ciclos/paso, 6777 ns/paso.
- TS: 11.29 micros/paso, 1537.3 ciclos/paso, 6405 ns/paso.
- XCH: 13.20 micros/paso, 2060.5 ciclos/paso, 8586 ns/paso.
- BZF: 10.70 micros/paso, 1564.6 ciclos/paso, 6519 ns/paso.
- HALT: 10.86 micros/paso, 1319.4 ciclos/paso, 5498 ns/paso.
- Prueba salto inválido, con N_SUMA=3 y PRUEBA_SALTO=1:
- CPU_PROTEGE 0: 20,000 ciclos (tope de seguridad), no se detuvo, alarma activa pero ignorada.
- CPU_PROTEGE 1: 9 ciclos, sí se detuvo, alarma activa y atendida.

## Argumentó

En fetch el PC avanza e IR cambia, en decode se separa opcode y dirección sin cambiar registros de datos, y en execute es cuando cambia A, memoria o PC según la operación. Por eso dividir el ciclo ayuda a ver dónde falló algo.

TC guarda en Q la dirección de regreso, PC_anterior + 1, antes de saltar. Así una subrutina puede terminar y volver como un return, sin eso no sabría a dónde regresar.

El ciclo de reloj es un pulso del oscilador, el ciclo de máquina es una operación completa que abarca varios ciclos de reloj. Por eso micros() mide microsegundos y hay que convertir a ciclos con la frecuencia de 240 MHz, y restar el sobrecosto de medición.

Si la ROM empezara en 0x1000 no se podría referenciar: el campo de dirección solo tiene 12 bits, rango alcanzable 0x000 a 0xFFF. La dirección 0x1000 queda fuera y ninguna instrucción podría llegar a ella.

No basta con que la alarma se encienda: la CPU tiene que revisarla y reaccionar deteniéndose. Sin protección la alarma se activa pero el procesador sigue hasta el tope de 20,000 ciclos interpretando datos inválidos como instrucciones. Con protección se detiene en 9 ciclos.

## Evidencia

- Video: [Video Lab 7](https://drive.google.com/file/d/1bdK2cnQhG1BypPZTzng1Hn1tZp8MWGIP/view?usp=drivesdk)
- PDF: [Reporte Lab 7](https://drive.google.com/file/d/1jvxc4bFGKQN7vpt68VrpLcX4G3hMX371/view?usp=drivesdk)


## Declaración de uso de IA

Se utilizó IA como apoyo para entender conceptos de CPU, ciclo fetch-decode-execute y protección de memoria, y para organizar los resultados. El montaje, pruebas y mediciones son propias.
