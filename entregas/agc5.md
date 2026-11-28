# agc5 - Lab 10: Comunicación AGC-PC, inclinación y filtro

**Autora:** Alejandra Gómez - 241050184
**Lab:** 10 - Comunicación AGC-PC, inclinación MPU6050 y filtro de promedio móvil
**Plataforma:** Wokwi + ESP32

## Funciona

- Quinta etapa del proyecto de emulación del Apollo Guidance Computer (AGC) en ESP32.
- Montaje en Wokwi: ESP32, pantalla OLED SSD1306 de 128 x 64 píxeles por I2C con SDA en GPIO 21 y SCL en GPIO 22, sensor MPU6050 por I2C en dirección 0x68, botón ENTR en GPIO 32 con resistencia pull-up interna e interrupción por flanco de bajada.
- Memoria del AGC: RAM 0x000 a 0x03F con 64 palabras, registros de E/S 0x040 a 0x04F, ROM 0x100 a 0x13F con 64 palabras. El programa de ejemplo suma los números naturales desde 1 hasta N y guarda el resultado en RAM[10].
- CPU: tipo `palabra_t` como `int16_t`, estructura CPU con PC, A, Q, IR, bandera de detención y contador de ciclos. Instrucciones: TC, CA, CS, AD, TS, XCH, BZF y HALT, con etapas FETCH, DECODE y EXECUTE.
- MPU6050: se activa escribiendo en el registro de energía 0x6B, se piden 14 bytes desde 0x3B y se combinan bytes altos y bajos para aceleración y velocidad angular.
- Temporizador ESP32 cada 100 ms, la ISR no hace el trabajo pesado, solo encola solicitudes en una cola circular. Cuando la cola se llena se activa la Alarma 1202.
- DSKY en OLED: sustantivo 01 consulta memoria, 20 muestra datos del giróscopo, 21 presenta cola, alarma y ticks, 23 se usa para la inclinación.
- Paso 1 - Comunicación AGC-PC: telemetría periódica en línea corta con ticks, verbo, sustantivo y los 3 registros del DSKY. Ejemplo: T=1021 V=06 N=21 R1=0 R2=0 R3=1021.
- Comandos serie probados:
- V + sustantivo: seleccionar vista del DSKY, por ejemplo V06 N21 para cola, alarma y ticks
- W: escribir en RAM o registro de E/S, por ejemplo W000,0100 guarda 0100 en 0x000 y responde OK 000 = 0100
- W04A,0000: escribe 0 en IO_ALARMA, responde OK 04A = 0000 y apaga el indicador de advertencia
- B: enviar bloque de 1 024 bytes para medir ancho de banda
- T: cambiar periodo del temporizador, T1 y T100
- F: cambiar filtro, F1, F8, F64
- V27: modo instrumento
- Paso 2 - Inclinación: se agregó el registro IO_INCL para almacenar la inclinación y se habilitó el sustantivo 23. En V06 N23, R1 muestra el ángulo filtrado en centésimas de grado, R2 la lectura del acelerómetro en eje Y y R3 el valor del eje Z.
- Paso 3 - Filtro: promedio móvil con ventanas F1, F8 y F64.
- F1: usa solo la muestra actual
- F8: promedio de las últimas 8 muestras
- F64: promedio de las últimas 64 lecturas, señal más suave
- V27 activa modo instrumento: pausa la telemetría habitual y envía continuamente `filtrada:0.00, cruda:0.00` para graficar ambas señales.
- Recuperación 1202: al saturarse la cola se limpia la cola y se reinicia el estado de la CPU sin borrar los resultados guardados en RAM. Con T100 el sistema vacía la cola y continúa.

## Midió

- Preguntas previas:
- 115200 8N1: 115 200 bits por segundo, 8 bits de dato, N sin paridad, 1 bit de parada. Cada byte útil necesita 1 bit de inicio + 8 de dato + 1 de parada = 10 bits en el medio físico.
- Capacidad teórica máxima: 115 200 / 10 = 11 520 bytes por segundo.
- Sensibilidad MPU6050 en ±2 g: 16 384 LSB/g. En ±4 g: 8 192 LSB/g. En ±8 g: 4 096 LSB/g. En ±16 g: 2 048 LSB/g. A mayor rango, más amplio el rango de medición pero menor resolución.
- Paso 1 - Comando W:
- Se envió W000,0100, respuesta OK 000 = 0100, guardó el valor hexadecimal 0100 en 0x000.
- Paso 1 - Alarma 1202:
- Con T1 se redujo el periodo del temporizador, aumentó la frecuencia de solicitudes, la cola llegó a su límite y se activó la Alarma 1202, reflejada también en la OLED.
- Con T100 se regresó al periodo normal, la recuperación limpió la cola y el programa continuó.
- Con W04A,0000 se reconoció la alarma desde la PC, respuesta OK 04A = 0000.
- Paso 1 - Ancho de banda con comando B:
- Bloque de 1 024 bytes con el carácter U, 3 pruebas: 88 366 µs, 88 366 µs y 88 364 µs. Valor representativo: aprox. 88 366 µs.
- Tiempo teórico con 115 200 baudios y 8N1 para 1 024 bytes: cercano a 88 889 µs. La medición del simulador fue muy parecida al cálculo.
- Paso 1 - ¿Se pueden enviar 60 pantallas por segundo?
- Una pantalla de 128 x 64 píxeles requiere 1 024 bytes a 1 bit por píxel. Para 60 pantallas por segundo se necesitarían 61 440 bytes por segundo.
- El canal a 115 200 baudios y 8N1 solo transporta aprox. 11 520 bytes por segundo. No alcanza para 60 pantallas completas.
- Capacidad disponible: alrededor de 11 actualizaciones por segundo, sin considerar otros gastos del protocolo.
- Paso 2 - Conversión con atan2:
- Fórmula: ángulo = atan2(Ay, Az) x (180 / pi). Usa los signos de ambos valores para identificar el cuadrante.
- Como el AGC trabaja con palabras enteras, el ángulo se multiplica por 100 antes de guardarlo. Ejemplo: 45.27° se representa como 4527, en centésimas de grado.
- Pruebas de inclinación:
- Sensor horizontal: Y = 0 g, Z = 1 g, esperado 0°. Obtenido R1 = 0, R2 = 0, R3 = 16 384.
- Inclinación +90°: Y = 1 g, Z = 0 g, esperado +90°. Obtenido R1 = 9000, R2 = 16 384, R3 = 0.
- Inclinación -90°: Y invertido, esperado -90°. Obtenido R1 = -9000, R2 = -16 384, R3 = 0.
- Paso 3 - Frecuencia de muestreo:
- Temporizador cada 100 ms = 0.1 s. Frecuencia f = 1/T = 10 Hz. En el registro de 100 muestras se observó un intervalo total aprox. de 10 segundos, consistente con esa frecuencia.
- Paso 3 - Ruido, sensor horizontal, 100 muestras por configuración:
- F1: promedio 0.0000°, desviación estándar 0.0000°
- F8: promedio 0.0000°, desviación estándar 0.0000°
- F64: promedio 0.0000°, desviación estándar 0.0000°
- Los resultados fueron cero porque el sensor de Wokwi no reproduce el ruido electrónico de un componente físico.
- Paso 3 - Retardo al cambiar la inclinación, a 10 Hz, cada muestra = 0.1 s:
- F1: alrededor de 1 muestra, 0.1 s. La salida sigue casi de inmediato el cambio.
- F8: hasta 8 muestras, aprox. 0.8 s. La señal filtrada cambia gradualmente antes de estabilizarse.
- F64: cerca de 64 muestras, 6.4 s. Respuesta mucho más lenta, aunque la señal se observa más suavizada.
- Paso 3 - Prueba de esfuerzo con T10 + F64:
- Temporizador cada 10 ms, 100 muestras por segundo. El retardo de 64 muestras se reduce a aprox. 0.64 s.
- La mayor cantidad de tareas provocó que la cola alcanzara su capacidad máxima. En sustantivo 21 se observó valor 8 en el registro de cola y se activó la Alarma 1202. Al regresar a T100 el sistema pudo vaciar la cola y continuar.

## Argumentó

El AGC del Apollo necesitaba intercambiar información con el Centro de Control de Misión. Desde la nave se enviaban datos de estado, downlink, y desde tierra podían recibirse instrucciones, uplink. Por limitaciones tecnológicas no se enviaban imágenes completas ni video. El AGC compartía datos concretos, como registros internos, indicadores de error y variables de navegación. En la nave el DSKY mostraba información numérica con verbos y sustantivos, en tierra los especialistas analizaban los datos.

La plataforma inercial usaba acelerómetros y giróscopos para conocer el movimiento y orientación de la nave. Los sensores generaban señales que se convertían en valores digitales. Por sí solos esos números no representan directamente un ángulo: el programa debe interpretarlos, aplicar conversiones y, cuando es necesario, filtrar las lecturas.

Por eso conviene enviar telemetría compacta en vez de imágenes completas: una pantalla completa exige 61 440 bytes/s para 60 fps, pero el enlace solo da 11 520 bytes/s. Con datos cortos como T, V, N, R1, R2, R3 se consulta el estado sin saturar el canal.

En un sensor real, el promedio móvil disminuye las variaciones aleatorias. De forma teórica la desviación estándar del ruido disminuye en proporción a la raíz cuadrada del tamaño de la ventana: para F8 el factor es aprox. 2.83 y para F64 es 8. En Wokwi dio cero porque el simulador no reproduce el ruido eléctrico ni las vibraciones de un MPU6050 físico.

La comparación muestra el equilibrio entre estabilidad y rapidez: una ventana grande reduce las variaciones, pero hace que el sistema tarde más en responder. F1 es rápido pero ruidoso, F64 es suave pero lento con 6.4 s de retardo a 10 Hz, F8 es el punto intermedio con 0.8 s.

El puerto serie virtual depende del navegador y del equipo donde se ejecuta Wokwi, por lo que sus tiempos pueden variar. La ejecución de los temporizadores depende del rendimiento del equipo anfitrión. Por eso los resultados de tiempo y ruido se interpretan tomando en cuenta las características del simulador.

La prueba T10 demuestra que el procesamiento debe mantenerse dentro de la capacidad del microcontrolador: al pedir 100 muestras por segundo la cola se llena, llega a 8 y aparece la 1202, igual que en el Apollo cuando entraba más trabajo del que se alcanzaba a procesar.

## Evidencia

- Video: [Video Lab 10](https://drive.google.com/file/d/1YyHN5_b0uSgsOGB5EZVfHOVs4ZIKvN2D/view?usp=drivesdk)
- PDF: [Reporte Lab 10](https://drive.google.com/file/d/1MQ3jFEZvEI0QiQ3XxAQiFh-Eo8v2HXbc/view?usp=drivesdk)


## Declaración de uso de IA

Se utilizó IA como apoyo para entender conceptos de comunicación serie 115200 8N1, sensibilidad del MPU6050, conversión con atan2, promedio móvil, ruido y retardo, y para organizar los resultados. El montaje, pruebas y mediciones son propias. En el reporte original se declara el uso de IA para organizar la información, revisar conceptos y mejorar la redacción.
