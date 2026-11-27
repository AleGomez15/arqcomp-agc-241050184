# agc3 - Lab 8: MPU6050, interrupciones y alarma 1202

**Autora:** Alejandra Gómez - 241050184
**Lab:** 8 - MPU6050, interrupciones de temporizador y pin, alarma 1202
**Plataforma:** Wokwi + ESP32

## Funciona

- Lectura del sensor MPU6050 por I2C, detectado en la dirección 0x68.
- Se piden 14 bytes consecutivos desde el registro de aceleración y con eso se actualizan los registros de entrada/salida de la AGC.
- Registros de E/S: IO_ACC_X, IO_ACC_Y, IO_ACC_Z, IO_GYR_X, IO_GYR_Y, IO_GYR_Z, IO_ALARMA, IO_COLA.
- La CPU consulta los valores del sensor desde los registros de la AGC, sin usar directamente las funciones de I2C.
- Temporizador periódico en ESP32 con Arduino-ESP32 3.x: `timerBegin()`, `timerAttachInterrupt()`, `timerAlarm()`.
- La ISR del temporizador solo incrementa el contador de ticks y activa una bandera de tarea pendiente. El `loop()` revisa la bandera y hace el trabajo pesado, incluida la lectura del sensor.
- Botón ENTR en GPIO 32, conectado a GND con resistencia pull-up, interrupción por flanco de bajada, antirrebote de 200 ms.
- Prueba complementaria: alarma 1202 y recuperación del sistema. Al saturarse la cola se atiende la alarma, se vacía la cola y se reinicia la CPU con `cpuReset()`, conservando RAM[10].

## Midió

- Sensor MPU6050: detectado en 0x68, lectura de aceleración y velocidad angular en 3 ejes funcionando por I2C.
- Temporizador: ticks incrementan con cada interrupción, bandera de tarea pendiente atendida en el ciclo principal.
- Botón ENTR, con antirrebote de 200 ms:
- Pulsaciones realizadas: 10
- Eventos detectados: 10
- Cada pulsación produjo un solo evento, sin eventos extra por rebote.
- Cola de tareas / alarma 1202:
- Con periodo de temporizador de 2 ms apareció la alarma 1202 por saturación de la cola, al generarse eventos más rápido de lo que se podían atender.
- Evidencia de 60 s: se encoló 1 tarea.
- Recuperación después de la alarma 1202:
- Alarma 1202 atendida
- Cola vaciada
- CPU reiniciada con `cpuReset()`
- RAM[10] conservada
- Solo se encoló tarea 1
- RAM[10]: 36
- RAM[20]: 526

## Argumentó

Polling es revisar continuamente si pasó un evento, aunque no haya cambios. Interrupción es que el hardware avise al procesador cuando pasa el evento, así no se pierde tiempo revisando. Pero una ISR larga retrasa otras tareas, por eso debe ser breve.

Las alarmas 1201 y 1202 son sobrecarga del ejecutivo del AGC: el procesador no alcanza a atender las tareas programadas en el tiempo disponible. La 1202 es la famosa del Apollo 11, cuando la computadora se sobrecargó de tareas. Aquí se reproduce con la saturación de la cola.

`IRAM_ATTR` pone la función en la memoria interna de instrucciones del ESP32, para que esté disponible durante la interrupción. No se debe usar `Serial.print()` dentro de una ISR porque tarda demasiado y usa recursos compartidos. La ISR solo debe registrar el evento con una variable o bandera, y los mensajes se muestran desde el ciclo principal.

Lo mismo con I2C: la lectura I2C dentro de la ISR alarga la atención del evento y afecta otras operaciones. Por eso la ISR solo marca la bandera y la lectura del sensor se hace en el `loop()`.

En un sensor físico el giroscopio en reposo no da exactamente cero por ruido electrónico, características del dispositivo y offset. En Wokwi puede mostrar cero porque depende del modelo simulado y de cómo representa el movimiento, no de un sensor real.

En Wokwi salieron exactamente 10 eventos porque la interacción es controlada. Con un botón físico los contactos mecánicos rebotan al abrirse o cerrarse y esos cambios rápidos se pueden contar como varias pulsaciones. Ese rebote eléctrico es lo que el simulador no representa completamente.

## Evidencia

- Video: [Video Lab 8](https://drive.google.com/file/d/1ZAlmhf3uGphpiDokGKTi59iHYdW2zDcP/view?usp=drivesdk)
- PDF: [Reporte Lab 8](https://drive.google.com/file/d/1AQIGA6BnM61kR38aYof1Ze-T1hlTnM76/view?usp=drivesdk)

## Declaración de uso de IA

Se utilizó IA como apoyo para entender conceptos de interrupciones, temporizadores, I2C y alarma 1202, y para organizar los resultados. 
El montaje, pruebas y mediciones son propias.



## Declaración de uso de IA

Se utilizó IA como apoyo para entender conceptos de interrupciones, temporizadores, I2C y alarma 1202, y para organizar los resultados. El montaje, pruebas y mediciones son propias.
