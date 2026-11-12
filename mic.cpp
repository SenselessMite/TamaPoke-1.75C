#include "mic.h"
#include "audio.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// ES7210: ADC de 4 canales. En la 1.75C los dos microfonos van a MIC1/MIC2 y la
// salida SDOUT1 (MIC1 = izquierda, MIC2 = derecha, I2S estandar 16 bit) llega
// al GPIO10, el DIN del I2S que ya abre audio.cpp. El ES7210 es esclavo: toma
// BCLK/LRCK del ESP32 y el MCLK (256 x 16 kHz = 4.096 MHz) del pin I2S_MCK_IO.
// Secuencia sacada del driver del ejemplo oficial de Waveshare (06_ES7210),
// reducida a lo que usamos: esclavo, 16 kHz, MIC1+MIC2, MIC3/4 apagados.
// ---------------------------------------------------------------------------
#define ES7210_ADDR 0x40
#define MIC_GAIN 10  // 0..14 en pasos de 3 dB: 10 = 30 dB (en el ejemplo, 0 dB: muy flojo para palmadas lejos)

// Umbrales sobre el nivel medio |muestra| de cada bloque de 16 ms (sin continua).
// Ajustables mirando el comando serie SENS: "lvl" es el nivel actual, "floor"
// el ruido de fondo que se va aprendiendo.
#define FRAME_SAMPLES 256       // muestras por canal y bloque: 16 ms a 16 kHz
#define CLAP_RATIO 6            // palmada: N veces el ruido de fondo...
#define CLAP_MIN 1200           // ...y nunca por debajo de esto
#define CLAP_MAX_FRAMES 20      // y corta: como mucho 320 ms
#define BLOW_RATIO 3            // soplido: menos fuerte que una palmada...
#define BLOW_MIN 500
#define BLOW_FRAMES 42          // ...pero sostenido ~670 ms
#define BLOW_GAP_FRAMES 3       // huecos cortos dentro del soplido se perdonan
#define EVENT_COOLDOWN_MS 1200

static bool ok = false;
static volatile bool active = true;
static volatile uint32_t events = 0;  // 32 bit: atomicos nativos en el Xtensa
static volatile int32_t dbgLevel = 0, dbgFloor = 0, dbgPeak = 0;

static bool w(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES7210_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static uint8_t r(uint8_t reg) {
  Wire.beginTransmission(ES7210_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES7210_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}
static void upd(uint8_t reg, uint8_t mask, uint8_t val) { w(reg, (r(reg) & ~mask) | (val & mask)); }

static bool es7210Init() {
  Wire.beginTransmission(ES7210_ADDR);
  if (Wire.endTransmission() != 0) return false;

  w(0x00, 0xFF); w(0x00, 0x41);          // reset
  w(0x01, 0x1F);                         // relojes parados mientras se configura
  w(0x09, 0x30); w(0x0A, 0x30);          // tiempos de arranque
  w(0x40, 0xC3);                         // analogico: VDDA 3.3 V, VMID 5k al arrancar
  w(0x41, 0x70); w(0x42, 0x70);          // MICBIAS 2.87 V
  w(0x07, 0x20);                         // OSR
  // reloj para MCLK 4.096 MHz / 16 kHz: adc_div=1, doubler=1, dll=1; LRCK = MCLK/256
  w(0x02, 0xC1);
  w(0x04, 0x01); w(0x05, 0x00);

  // seleccion de microfonos: MIC1 y MIC2 (los de la placa)
  for (uint8_t g = 0x43; g <= 0x46; g++) upd(g, 0x10, 0x00);
  w(0x4B, 0xFF); w(0x4C, 0xFF);          // todo apagado...
  upd(0x01, 0x0B, 0x00);                 // ...relojes de ADC1/2 en marcha
  w(0x4B, 0x00);                         // MICBIAS/ADC/PGA de MIC1-2 encendidos
  upd(0x43, 0x10, 0x10); upd(0x44, 0x10, 0x10);
  upd(0x43, 0x0F, MIC_GAIN); upd(0x44, 0x0F, MIC_GAIN);

  // formato: I2S estandar 16 bit, ADC1/2 por SDOUT1
  uint8_t f = r(0x11);
  f = (uint8_t)(((f & 0x1F) | 0x60) & 0xFC);
  w(0x11, f);
  w(0x12, 0x00);

  // arranque
  uint8_t clk = r(0x01);
  w(0x01, clk);
  w(0x06, 0x00);                         // sin power down
  w(0x47, 0x00); w(0x48, 0x00);          // MIC1/MIC2 encendidos
  w(0x49, 0xFF); w(0x4A, 0xFF);          // MIC3/MIC4 apagados
  return true;
}

// Tarea de escucha: lee bloques de 16 ms y busca palmadas y soplidos.
static void micTask(void *) {
  static int16_t buf[FRAME_SAMPLES * 2];  // estereo intercalado
  int32_t prevIn = 0, hp = 0;             // filtro paso alto (quita la continua)
  int32_t floorLvl = 0;                   // ruido de fondo aprendido
  uint16_t clapRun = 0, blowRun = 0, blowGap = 0;
  bool blowFired = false;
  uint32_t coolUntil = 0;
  for (;;) {
    size_t n = audioMicRead(buf, sizeof(buf));
    if (n < sizeof(buf)) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

    int32_t sum = 0, peak = 0;
    for (int i = 0; i < FRAME_SAMPLES; i++) {
      int32_t in = ((int32_t)buf[2 * i] + buf[2 * i + 1]) / 2;
      hp = in - prevIn + (hp * 31) / 32;  // paso alto ~80 Hz
      prevIn = in;
      int32_t a = hp < 0 ? -hp : hp;
      sum += a;
      if (a > peak) peak = a;
    }
    int32_t lvl = sum / FRAME_SAMPLES;

    // fondo: baja rapido, sube muy despacio (un ruido sostenido no lo dispara para siempre)
    if (floorLvl == 0) floorLvl = lvl;
    else if (lvl < floorLvl) floorLvl += (lvl - floorLvl) / 8;
    else floorLvl += (lvl - floorLvl) / 512 + 1;
    if (floorLvl < 8) floorLvl = 8;
    dbgLevel = lvl; dbgFloor = floorLvl;
    if (peak > dbgPeak) dbgPeak = peak;

    // el altavoz propio, los microfonos apagados o un evento reciente no cuentan
    if (!active || audioPlaying() || (int32_t)(millis() - coolUntil) < 0) {
      clapRun = blowRun = blowGap = 0;
      blowFired = false;
      continue;
    }

    int32_t clapThr = floorLvl * CLAP_RATIO; if (clapThr < CLAP_MIN) clapThr = CLAP_MIN;
    int32_t blowThr = floorLvl * BLOW_RATIO; if (blowThr < BLOW_MIN) blowThr = BLOW_MIN;

    // palmada: rafaga fuerte que vuelve al silencio enseguida
    if (lvl > clapThr) {
      if (clapRun < 255) clapRun++;
    } else if (clapRun) {
      if (clapRun <= CLAP_MAX_FRAMES && !blowFired) {
        __atomic_fetch_or(&events, (uint32_t)MIC_CLAP, __ATOMIC_RELAXED);
        coolUntil = millis() + EVENT_COOLDOWN_MS;
      }
      clapRun = 0;
    }

    // soplido: nivel alto sostenido (perdona huecos de un par de bloques)
    if (lvl > blowThr) {
      blowGap = 0;
      if (blowRun < 1000) blowRun++;
      if (blowRun >= BLOW_FRAMES && !blowFired) {
        __atomic_fetch_or(&events, (uint32_t)MIC_BLOW, __ATOMIC_RELAXED);
        blowFired = true;  // un evento por soplido
        clapRun = 0;       // lo que sigue no es una palmada
        coolUntil = millis() + EVENT_COOLDOWN_MS;
      }
    } else if (blowRun && ++blowGap > BLOW_GAP_FRAMES) {
      blowRun = blowGap = 0;
      blowFired = false;
    }
  }
}

bool micBegin() {
  ok = es7210Init();
  if (!ok) {
    Serial.println("ES7210 no detectado (sin microfono)");
    return false;
  }
  xTaskCreatePinnedToCore(micTask, "mic", 3072, nullptr, 1, nullptr, 0);
  Serial.println("ES7210 listo (palmadas + soplidos)");
  return true;
}

bool micAvailable() { return ok; }

void micSetActive(bool on) {
  if (!ok || on == active) return;
  active = on;
  // MICBIAS + ADC + PGA de MIC1/2: fuera con la pantalla apagada (I2C desde el loop, como el resto)
  w(0x4B, on ? 0x00 : 0xFF);
}

uint8_t micTakeEvents() { return (uint8_t)__atomic_exchange_n(&events, 0u, __ATOMIC_RELAXED); }

void micDebug() {
  if (!ok) { Serial.println("mic=off"); return; }
  Serial.printf("mic=%s lvl=%ld floor=%ld peak=%ld (palmada > %ld, soplido > %ld)\n",
                active ? "on" : "paused", (long)dbgLevel, (long)dbgFloor, (long)dbgPeak,
                (long)max((int32_t)CLAP_MIN, (int32_t)(dbgFloor * CLAP_RATIO)),
                (long)max((int32_t)BLOW_MIN, (int32_t)(dbgFloor * BLOW_RATIO)));
  dbgPeak = 0;
}
