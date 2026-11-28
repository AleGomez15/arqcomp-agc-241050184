// agc.ino - Lab 11: Framebuffer y controlador DMA en la AGC
// Alejandra Gomez - 241050184
// ESP32 + OLED SSD1306 + MPU6050 + boton ENTR
// Wokwi, Serial 115200 8N1

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define PIN_ENTR 32
#define MPU_ADDR 0x68

// ---------- Constantes AGC ----------
#define N_SUMA 7
typedef int16_t palabra_t;
#define RAM_TAM 64
#define IO_TAM 32
#define ROM_TAM 1024
#define VRAM_TAM 512
#define VRAM_INICIO 0x800
#define ROM_INICIO 0x100
#define IO_INICIO 0x040

// Indices dentro de IOREG (direccion = 0x040 + indice)
#define IO_ALARMA 10
#define IO_INCL 11
#define IO_GYRO_X 12
#define IO_GYRO_Y 13
#define IO_DMA_ORIGEN 14   // 0x04E
#define IO_DMA_DESTINO 15  // 0x04F
#define IO_DMA_CONTADOR 16 // 0x050
#define IO_DMA_CTRL 17     // 0x051
#define IO_DMA_ESTADO 18   // 0x052
#define IO_BUFFER_ACTIVO 19

palabra_t RAM[RAM_TAM];
palabra_t IOREG[IO_TAM];
palabra_t ROM_MEM[ROM_TAM];
palabra_t VRAM[VRAM_TAM];
palabra_t VRAM_B[VRAM_TAM];

struct CPU { palabra_t PC, A, Q, IR; bool halt; long ciclos; };
CPU cpu;

bool modoVideo = false;
bool alarmaMem = false;
long ticks = 0;
int verbo = 6, sustantivo = 21;
int filtroVentana = 1;
int periodoTimer = 100;

// ---------- DMA ----------
struct DMA_t {
  bool activo = false;
  bool burst = false;
  int restantes = 0;
  long turnosCPU = 0;
  unsigned long tInicio = 0;
  unsigned long duracionUs = 0;
  int estado = 0; // 0 libre, 1 ocupado, 2 listo, 3 error
};
DMA_t dma;

// ---------- Cola de tareas (8 espacios) ----------
#define COLA_TAM 8
int cola[COLA_TAM];
int colaHead = 0, colaTail = 0, colaCount = 0;
volatile bool flagTimer = false;

bool encolar(int t) {
  if (colaCount >= COLA_TAM) {
    IOREG[IO_ALARMA] = 1202;
    return false;
  }
  cola[colaTail] = t; colaTail = (colaTail + 1) % COLA_TAM; colaCount++;
  return true;
}
int desencolar() {
  if (colaCount == 0) return -1;
  int t = cola[colaHead]; colaHead = (colaHead + 1) % COLA_TAM; colaCount--;
  return t;
}

// ---------- Memoria ----------
palabra_t leerMem(int dir) {
  if (dir >= 0 && dir < RAM_TAM) return RAM[dir];
  if (dir >= IO_INICIO && dir < IO_INICIO + IO_TAM) return IOREG[dir - IO_INICIO];
  if (dir >= VRAM_INICIO && dir < VRAM_INICIO + VRAM_TAM) return VRAM[dir - VRAM_INICIO];
  if (dir >= ROM_INICIO && dir < ROM_INICIO + ROM_TAM) return ROM_MEM[dir - ROM_INICIO];
  return 0;
}
bool escribirMem(int dir, palabra_t val) {
  if (dir >= 0 && dir < RAM_TAM) { RAM[dir] = val; return true; }
  if (dir >= IO_INICIO && dir < IO_INICIO + IO_TAM) {
    IOREG[dir - IO_INICIO] = val;
    if (dir == IO_INICIO + IO_DMA_CTRL) arrancarDMA();
    return true;
  }
  if (dir >= VRAM_INICIO && dir < VRAM_INICIO + VRAM_TAM) { VRAM[dir - VRAM_INICIO] = val; return true; }
  if (dir >= ROM_INICIO && dir < ROM_INICIO + ROM_TAM) {
    alarmaMem = true;
    return false;
  }
  return false;
}

// ---------- Imagen en ROM: marco + 2 diagonales ----------
void quemarImagen() {
  int base = 0x140 - ROM_INICIO;
  for (int i = 0; i < VRAM_TAM; i++) ROM_MEM[base + i] = 0;
  for (int fila = 0; fila < 32; fila++) {
    for (int col = 0; col < 16; col++) {
      int idx = fila * 16 + col;
      palabra_t w = 0;
      if (fila == 0 || fila == 31) w = (palabra_t)0xFFFF;
      if (col == 0) w |= (palabra_t)0x8000;
      if (col == 15) w |= (palabra_t)0x0001;
      if (fila == col) w |= (palabra_t)0x8000;
      if (fila + col == 31) w |= (palabra_t)0x0001;
      ROM_MEM[base + idx] = w;
    }
  }
}

// ---------- DMA ----------
void arrancarDMA() {
  palabra_t ctrl = IOREG[IO_DMA_CTRL];
  if ((ctrl & 0x01) == 0) return;
  dma.activo = true;
  dma.burst = (ctrl & 0x02) != 0;
  dma.restantes = IOREG[IO_DMA_CONTADOR];
  dma.turnosCPU = 0;
  dma.tInicio = micros();
  dma.estado = 1;
  IOREG[IO_DMA_ESTADO] = 1;
  alarmaMem = false;
}
void pasoDMA() {
  if (!dma.activo) return;
  int origen = IOREG[IO_DMA_ORIGEN];
  int destino = IOREG[IO_DMA_DESTINO];
  int total = IOREG[IO_DMA_CONTADOR];
  int hecho = total - dma.restantes;
  if (dma.burst) {
    for (int i = 0; i < dma.restantes; i++) {
      palabra_t v = leerMem(origen + hecho + i);
      if (!escribirMem(destino + hecho + i, v)) { dma.estado = 3; IOREG[IO_DMA_ESTADO] = 3; dma.activo = false; return; }
    }
    dma.restantes = 0;
  } else {
    palabra_t v = leerMem(origen + hecho);
    if (!escribirMem(destino + hecho, v)) { dma.estado = 3; IOREG[IO_DMA_ESTADO] = 3; dma.activo = false; return; }
    dma.restantes--;
    dma.turnosCPU++;
  }
  if (dma.restantes <= 0) {
    dma.activo = false; dma.estado = 2; IOREG[IO_DMA_ESTADO] = 2;
    dma.duracionUs = micros() - dma.tInicio;
    IOREG[IO_BUFFER_ACTIVO] = 1 - IOREG[IO_BUFFER_ACTIVO];
  }
}

// ---------- CPU simulada: suma 1..N_SUMA ----------
void cpuReiniciar() { cpu.PC = 0; cpu.A = 0; cpu.Q = 0; cpu.IR = 0; cpu.halt = false; cpu.ciclos = 0; }
void cpuPaso() {
  static int n = 1; static int suma = 0;
  if (n <= N_SUMA) { suma += n; n++; cpu.ciclos++; }
  else { RAM[10] = suma; cpu.halt = true; }
}

// ---------- MPU6050 ----------
void leerMPU() {
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);
  if (Wire.available() < 14) return;
  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read();
  int16_t gx = (Wire.read() << 8) | Wire.read();
  int16_t gy = (Wire.read() << 8) | Wire.read();
  int16_t gz = (Wire.read() << 8) | Wire.read();
  float ang = atan2((float)ay, (float)az) * 180.0 / PI;
  static float buf[64]; static int bi = 0;
  buf[bi] = ang; bi = (bi + 1) % 64;
  float s = 0; for (int i = 0; i < filtroVentana; i++) s += buf[(bi - 1 - i + 64) % 64];
  float filtrada = s / filtroVentana;
  IOREG[IO_INCL] = (palabra_t)(filtrada * 100);
  IOREG[IO_GYRO_X] = gx; IOREG[IO_GYRO_Y] = gy;
  (void)ax; (void)az; (void)gz;
}

// ---------- Pantalla ----------
void dibujarVRAM() {
  display.clearDisplay();
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      int palabraIdx = (y / 2) * 8 + (x / 16);
      if (palabraIdx >= VRAM_TAM) continue;
      int bit = 15 - (x % 16);
      if (VRAM[palabraIdx] & (1 << bit)) display.drawPixel(x, y, SSD1306_WHITE);
    }
  }
  display.display();
}
void dibujarDSKY() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("V%02d N%02d\n", verbo, sustantivo);
  palabra_t r1 = 0, r2 = 0, r3 = 0;
  if (sustantivo == 1) { r1 = RAM[0]; r2 = RAM[10]; r3 = RAM[20]; }
  else if (sustantivo == 20) { r1 = IOREG[IO_GYRO_X]; r2 = IOREG[IO_GYRO_Y]; r3 = 0; }
  else if (sustantivo == 21) { r1 = colaCount; r2 = IOREG[IO_ALARMA]; r3 = ticks; }
  else if (sustantivo == 22) { r1 = dma.restantes; r2 = IOREG[IO_DMA_ESTADO]; r3 = dma.turnosCPU; }
  else if (sustantivo == 23) { r1 = IOREG[IO_INCL]; r2 = 0; r3 = 0; }
  display.printf("R1 %+06d\nR2 %+06d\nR3 %+06d\n", r1, r2, r3);
  display.display();
}
void refrescarPantalla() {
  if (modoVideo) dibujarVRAM(); else dibujarDSKY();
}

// ---------- Comandos serie ----------
void atenderSerie() {
  if (!Serial.available()) return;
  String linea = Serial.readStringUntil('\n'); linea.trim(); linea.toUpperCase();
  if (linea.length() == 0) return;
  Serial.println(linea);
  char c = linea.charAt(0);
  if (c == 'V') {
    if (linea == "V35") { modoVideo = true; Serial.println("Modo video ON"); return; }
    if (linea == "V36") { for (int i = 0; i < VRAM_TAM; i++) VRAM[i] = 0; Serial.println("VRAM limpia"); return; }
    if (linea == "V37") { cpuReiniciar(); Serial.println("CPU reiniciada"); return; }
    if (linea.length() >= 5) {
      verbo = linea.substring(1, 3).toInt();
      sustantivo = linea.substring(4, 6).toInt();
      Serial.printf("V=%02d N=%02d\n", verbo, sustantivo);
    }
  } else if (c == 'W') {
    int coma = linea.indexOf(',');
    int dir = strtol(linea.substring(1, coma).c_str(), NULL, 16);
    int val = strtol(linea.substring(coma + 1).c_str(), NULL, 16);
    if (escribirMem(dir, (palabra_t)val)) Serial.printf("OK %03X = %04X\n", dir, val);
    else Serial.printf("ERR %03X protegido\n", dir);
  } else if (c == 'B') {
    unsigned long t0 = micros();
    for (int i = 0; i < 1024; i++) Serial.print('U');
    Serial.println(); Serial.printf("Bloque 1024 bytes: %lu us\n", micros() - t0);
  } else if (c == 'T') {
    periodoTimer = linea.substring(1).toInt();
    Serial.printf("Periodo %d ms\n", periodoTimer);
  } else if (c == 'F') {
    int f = linea.substring(1).toInt();
    if (f == 1 || f == 8 || f == 64) filtroVentana = f;
    Serial.printf("Filtro F%d\n", filtroVentana);
  } else if (c == 'D') {
    Serial.printf("DMA: turnos CPU=%ld duracion=%lu us estado=%d\n", dma.turnosCPU, dma.duracionUs, dma.estado);
  }
}

void IRAM_ATTR onTimer() { flagTimer = true; }
void IRAM_ATTR onEntr() { encolar(3); }

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  pinMode(PIN_ENTR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ENTR), onEntr, FALLING);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0); Wire.endTransmission(true);
  for (int i = 0; i < RAM_TAM; i++) RAM[i] = 0;
  for (int i = 0; i < IO_TAM; i++) IOREG[i] = 0;
  for (int i = 0; i < ROM_TAM; i++) ROM_MEM[i] = 0;
  for (int i = 0; i < VRAM_TAM; i++) { VRAM[i] = 0; VRAM_B[i] = 0; }
  quemarImagen();
  cpuReiniciar();
  Serial.printf("ANCLA %04X-Nov 25 2026-04:12:27-%d\n", (int)random(0xFFFF), (int)random(99999));
  Serial.println("AGC listo. Escribe V06N20 y presiona Enter.");
}

void loop() {
  atenderSerie();
  pasoDMA();
  if (flagTimer) {
    flagTimer = false; ticks++;
    encolar(1); encolar(2);
  }
  int tarea = desencolar();
  if (tarea == 1) leerMPU();
  else if (tarea == 2) { }
  else if (tarea == 3) refrescarPantalla();
  static int vueltas = 0;
  if (++vueltas % 5 == 0) { for (int i = 0; i < 10; i++) cpuPaso(); }
  static unsigned long lastTel = 0;
  if (millis() - lastTel > 1000) {
    lastTel = millis();
    Serial.printf("T=%ld V=%02d N=%02d R1=%d R2=%d R3=%ld\n", ticks, verbo, sustantivo, 0, RAM[10], ticks);
  }
  delay(10);
}
