# agc4 - Lab 9: Cola de tareas, alarma 1202 y recuperación

**Autora:** Alejandra Gómez - 241050184
**Lab:** 9 - Cola de tareas, alarma 1202 y recuperación
**Plataforma:** Wokwi + ESP32

## Funciona

- Preguntas previas: chipset, DMA y bus mastering.
- Mi PC es una laptop LANIX con Intel Celeron N4020 a 1.10 GHz, familia Gemini Lake Refresh. Es un SoC, por lo que no lleva chipset separado: los controladores vienen integrados dentro del mismo chip.
- Con PowerShell identifiqué el chipset por los componentes: LPC Controller - 31E8 y Host Bridge - 31F0, todos los IDs empiezan con 31, de la plataforma Gemini Lake.
- Controladores que gestiona: USB 3.0 (xHCI) - 31A8, SATA AHCI - 31E3, 2x SD Host - 31CC y 31D0, Smart Sound - 3198, 3x I2C - 31AC, 31B4 y 31BA, 3x SPI - 31C2, 31C4 y 31C6, 4x UART - 31BC, 31BE, 31C0 y 31EE, LPC - 31E8, Host Bridge - 31F0, UHD Graphics 600 - 3185, Trusted Execution Engine - 319A, Dynamic Tuning - 318C y Gaussian Mixture Model - 31D4.
- Paso 0 - Verificación: circuito en Wokwi con ESP32, MPU6050, pantalla OLED, joystick y botón ENTR funcionando. Ancla de sesión: 1A25-99832.
- Paso 1 - Cola de tareas: se sustituyeron las banderas por una cola circular de 8 espacios. `encolar()` agrega al final y devuelve false cuando está llena. `desencolar()` extrae la que lleva más tiempo esperando. Ambas usan `portENTER_CRITICAL_SAFE` para que la interrupción y `loop()` no la corrompan al mismo tiempo.
- Tabla de tareas:
- 1: Leer sensor MPU6050 y actualizar registros de E/S - Temporizador (cada 100 ms)
- 2: Leer los botones - Temporizador (cada 100 ms)
- 3: Refrescar pantalla OLED - Botón ENTR y tarea 2
- 4: Ejecutar 10 pasos de la CPU - loop(), cada 5 vueltas
- El temporizador y ENTR solo encolan, `loop()` atiende una por una. Cada 5 ciclos se encola la tarea 4. `IOREG[IO_COLA]` guarda cuántas tareas hay pendientes para mostrar en pantalla. Se agregó el sustantivo 21 al DSKY para monitorear la cola.
- La tarea 2 agrega la tarea 3, así la pantalla se refresca cuando se modifican valores del DSKY.
- Paso 2 - Provocar la 1202: se probaron periodos de 100 ms a 2 ms. Con 100 ms y 70 ms no hubo alarma en 60 s. De 50 ms hacia abajo la cola se llenó y apareció la 1202.
- Paso 3 - Recuperación: cuando `encolar()` detecta cola llena y registra la 1202, el sistema se recupera solo:
- 1. Vaciar la cola
- 2. Reiniciar la CPU del AGC 2 con V37 automático, conservando RAM[10]
- RAM[20] se inicializa en 0 una única vez en `setup()`, no en `cpuReiniciar()`, así acumula todas las alarmas desde el encendido y no se borra con cada recuperación.
- Después de la recuperación el sistema sigue mostrando el giro del sensor en V06 N20, aunque la alarma se repita.

## Midió

- Paso 0:
- Ticks en V06 N21, R3: sube de 5 en 5 cada medio segundo (1015, 1020, 1025...), 10 ticks por segundo, confirma temporizador cada 100 ms.
- ENTR por interrupción: 10 pulsaciones, 10 eventos detectados, una sola vez cada uno.
- Paso 1 - Verificación en DSKY V06 N21:
- R1: +00001, cola casi siempre vacía o con 1 tarea
- R2: +00000, sin alarma
- R3: +00042, aprox. 4 s después de iniciar, a 10 ticks por segundo
- A 100 ms el sistema atiende todas las tareas a tiempo y la cola nunca se satura.
- Paso 2 - Periodos probados:
- 100 ms: No apareció, 599 ticks en 60 s
- 70 ms: No apareció, 857 ticks en 60 s
- 50 ms: Sí apareció, 58 ticks hasta la alarma, aprox. 2.9 s
- 40 ms: Sí apareció, 14 ticks, 560 ms
- 30 ms: Sí apareció, 8 ticks, 240 ms
- 10 ms: Sí apareció, 6 ticks, 60 ms
- 2 ms: Sí apareció, 6 ticks, 12 ms
- Periodo más corto que NO activó la alarma: 70 ms. Periodo más largo que SÍ la activó: 50 ms. Punto de saturación entre 50 y 70 ms.
- Duración de cada tarea, con `micros()`, promedio de 20 ejecuciones:
- T1 IMU: 1 978 µs
- T2 entrada: 53 µs
- T3 OLED: 50 949 µs
- T4 CPU x10: 20 µs
- Cálculo de carga: cada tick genera T1 + T2 + T3 = 1 978 + 53 + 50 949 = 52 980 µs, porque la tarea 2 agrega la tarea 3.
- Con 50 ms: 52 980 / 50 000 x 100 = 106 %. Se exigió más trabajo del que cabe en ese intervalo.
- Con 70 ms, sin saturación: 52 980 / 70 000 x 100 = 75.7 %. Todavía quedaba tiempo libre.
- La carga llega justo al 100 % con un periodo de 52 980 µs, aprox. 53 ms, coincide con lo observado: con 50 ms hubo saturación y con 70 ms no.
- Si se aplica solo la fórmula de la guía (T1 + T2 = 2 031 µs), con 50 ms daría 4.1 %, valor que no justifica la alarma porque ignora la tarea 3, que es la más pesada.
- Paso 3 - Recuperación:
- Con temporizador a 50 ms durante 60 s, en V06 N01: RAM[20] = +00010, la 1202 se activó 10 veces y en cada una el sistema se recuperó solo, sin detenerse.
- Al cambiar a V06 N20 y mover el MPU6050, los valores siguieron cambiando. RAM[10] mantuvo su valor después de todas las recuperaciones.
- Prueba rómpelo a propósito, 60 s:
- Con vaciado: 10 alarmas 1202, V06 N20 sigue vivo: Sí
- Sin vaciado: más de 1200 alarmas 1202, V06 N20 sigue vivo: No, pantalla y registros se congelaron
- Sin vaciar la cola queda llena de tareas antiguas, entra en ciclo continuo de alarmas y se congela. Esto comprueba que vaciar la cola es indispensable.

## Argumentó

DMA: la CPU deja de mover los datos uno a uno. Solo escribe en los registros origen, destino y cantidad, da la orden de arranque y continúa con otras tareas. Al terminar, el DMA le avisa con una sola señal.

Bus mastering: es la capacidad de un dispositivo para tomar el control del bus y leer o escribir directamente en memoria, sin que la CPU haga la transferencia. Antes existía un solo controlador DMA compartido, el 8237, como una sola fotocopiadora para toda una oficina. Hoy las tarjetas PCIe manejan tanta velocidad que cada una incorpora su propio motor DMA, así no se forman filas y varias pueden transferir al mismo tiempo.

Alarmas 1201 y 1202: las dos indican que la computadora del Apollo se quedó sin espacio para registrar más tareas. La 1202 se activa cuando se agotan los core sets, espacios donde se registra cada tarea pendiente. La 1201 se activa cuando se agotan las áreas VAC, bloques de memoria más grandes para operaciones con vectores. En el Apollo 11 se presentaron cuatro alarmas 1202 y una 1201, por datos innecesarios del radar. La computadora se reinició sola, conservó las tareas críticas y el módulo pudo alunizar.

Comparación con el 15 % del Apollo 11: mi sistema se saturó cuando las tareas del temporizador requerían el 106 % del tiempo disponible, mientras la computadora del Apollo se saturó con el radar consumiendo alrededor del 15 %. La comparación no es directa: ese 15 % se sumó a una computadora que ya trabajaba cerca de su límite con otras tareas, y mi 106 % representa la carga total. Lo que sí tienen en común es el mecanismo: cuando entra más trabajo del que se alcanza a procesar, la cola de tamaño fijo se llena y se activa la 1202.

La tarea más pesada es la pantalla OLED, que representa el 96 % del trabajo de cada tick (50 949 de 52 980 µs). La pantalla transmite su imagen completa por el bus I2C a 400 kHz, y ese bus es el verdadero cuello de botella, no el procesador de 240 MHz.

Prioridad - tarea 1 (IMU) sobrevive y tarea 3 (OLED) se descarta: en un módulo lunar la decisión correcta es conservar el sensor y descartar la pantalla. El sensor indica la orientación de la nave, sin esa información no se puede maniobrar y si ocurre durante el descenso el módulo podría estrellarse. La pantalla solo despliega información, si se detiene unos instantes la nave sigue volando. Es lo que hizo la computadora del Apollo 11 ante la 1202: descartó lo no esencial, mantuvo la navegación y Armstrong logró alunizar.

En un tablero de museo la decisión correcta es al revés: conservar la pantalla y descartar el sensor. En una exhibición lo importante es que el público vea algo funcionando, si la pantalla se congela parece que el equipo está descompuesto. Si se pierde una lectura del sensor nadie se da cuenta y no hay consecuencias graves.

Conclusión: ninguna tarea es prioritaria por sí sola, su importancia depende de las consecuencias de que falle. En la nave, perder el sensor puede poner vidas en riesgo; en el museo, perder la pantalla solo arruina la experiencia. La estrategia de recuperación se define según la gravedad de cada falla.

## Evidencia

- Video: [Video Lab 9](https://drive.google.com/file/d/1ZAlmhf3uGphpiDokGKTi59iHYdW2zDcP/view?usp=drivesdk)
- PDF: [Reporte Lab 9](https://drive.google.com/file/d/1Vkyao-auuNOOIheMnJjGOEbY2qnNGoty/view?usp=drivesdk)



## Declaración de uso de IA

Se utilizó IA como apoyo para entender conceptos de chipset, DMA, bus mastering, cola de tareas, alarma 1202 y recuperación, y para organizar los resultados. El montaje, pruebas y mediciones son propias. En el reporte original se declara el uso de Gemini y Claude para comprender instrucciones, depurar código, seleccionar comandos de PowerShell y redactar explicaciones.
