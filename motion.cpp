#include "motion.h"
#include "pin_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Driver propio del QMI8658 (registros directos por I2C, sin SensorLib).
// SensorLib 0.5.x trae DOS clases llamadas SensorQMI8658: la antigua, entera en
// src/SensorQMI8658.hpp, y la nueva en src/sensor/imu/qmi8658/ con su .cpp. Al
// incluir la antigua, el enlazador mezclaba la vtable de la nueva con el objeto
// de la antigua y la primera llamada virtual de begin() saltaba a basura
// (InstrFetchProhibited en setup, bucle de reinicios). Registros y secuencias
// sacados de SensorQMI8658_Reg.hpp / SensorQMI8658.cpp de esa misma libreria.
// Todas las esperas estan acotadas: si el chip no contesta, no se cuelga nada.
// ---------------------------------------------------------------------------
#define QMI_ADDR_L 0x6B          // SA0 a nivel alto (el de las placas Waveshare)
#define QMI_ADDR_H 0x6A
#define QMI_WHO_AM_I 0x00        // = 0x05
#define QMI_CTRL1 0x02           // bit6: auto-incremento de direccion
#define QMI_CTRL2 0x03           // acelerometro: [6:4] rango, [3:0] ODR
#define QMI_CTRL5 0x06           // filtros paso bajo
#define QMI_CTRL7 0x08           // bit0 acelerometro, bit1 giroscopo
#define QMI_CTRL8 0x09           // bit7: confirmacion de CTRL9 por STATUS_INT; bit4: podometro
#define QMI_CTRL9 0x0A           // comandos
#define QMI_CAL1_L 0x0B          // CAL1_L..CAL4_H (0x0B..0x12): parametros de los comandos
#define QMI_STATUS_INT 0x2D      // bit7: comando CTRL9 hecho
#define QMI_AX_L 0x35            // AX, AY, AZ: int16 little endian
#define QMI_RST_RESULT 0x4D      // bit7: reset terminado
#define QMI_STEP_CNT 0x5A        // 24 bit little endian
#define QMI_RESET 0x60           // escribir 0xB0
#define QMI_CMD_ACK 0x00
#define QMI_CMD_CONFIG_PEDO 0x0D
#define QMI_ACC_4G (0x01 << 4)
#define QMI_ACC_ODR_56HZ 0x07    // ~56 Hz (en el ejemplo de SensorLib, "62.5 Hz")
#define QMI_G_PER_LSB (4.0f / 32768.0f)

// ---------------------------------------------------------------------------
// Umbrales (en g). Calculados a ojo para una placa en la mano: si en la placa
// real cuesta o sobra, ajustarlos aqui mirando el comando serie SENS.
// ---------------------------------------------------------------------------
#define POLL_MS 20               // ~50 Hz; el chip muestrea a ~56 Hz
#define SHAKE_PEAK_G 1.0f        // |modulo - 1g| que cuenta como sacudida
#define SHAKE_PEAK_GAP_MS 90     // dos picos mas juntos son el mismo golpe
#define SHAKE_WINDOW_MS 700      // sin pico en este tiempo, se empieza a contar de nuevo
#define SHAKE_PEAKS 4            // picos seguidos para dar la sacudida por buena
#define SHAKE_COOLDOWN_MS 5000     // una sacudida cada 5 s como mucho (que no se farmee ni agote al bicho)
#define MOVED_DELTA_G 0.35f      // cambio del vector entre lecturas: la han cogido o girado
#define STILL_DELTA_G 0.06f      // por debajo de esto entre lecturas, la placa esta quieta
#define LIFT_STILL_MS 1500       // solo cuenta como "cogerla" si antes estaba quieta este rato
#define LIFT_WINDOW_MS 1200      // y si en este tiempo acaba mirando hacia arriba
#define LIFT_FACE_G 0.55f        // |z| minimo: pantalla mas o menos de cara (en el bolsillo va de canto)
#define MOVED_COOLDOWN_MS 1000
#define STEPS_POLL_MS 3000       // el podometro se lee cada 3 s (una lectura I2C)

static uint8_t addr = QMI_ADDR_L;
static bool ok = false;
static uint8_t events = 0;
static uint32_t lastPoll = 0;
static float px = 0, py = 0, pz = 1;   // lectura anterior
static bool havePrev = false;
static uint8_t peaks = 0;
static uint32_t lastPeak = 0, shakeUntil = 0, movedUntil = 0;
static uint32_t stillSince = 0;   // desde cuando no se mueve
static uint32_t liftUntil = 0;    // candidato a "la han cogido": esperar a verla de cara
static bool liftPending = false;
static uint32_t stepsLastPoll = 0, stepsBase = 0;
static uint16_t stepsPending = 0;
static float lastMag = 1, lastDyn = 0;

// ---- I2C ----
static bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static bool rd(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}
static uint8_t rd8(uint8_t reg) {
  uint8_t v = 0xFF;
  return rd(reg, &v, 1) ? v : 0xFF;
}
static bool present(uint8_t a) {
  Wire.beginTransmission(a);
  return Wire.endTransmission() == 0;
}

// comando por CTRL9: escribirlo, esperar el "hecho" en STATUS_INT.7, confirmarlo
// con ACK y esperar a que baje (requiere CTRL8.7 = 1). Hasta 100 ms por fase.
static bool command(uint8_t cmd) {
  if (!wr(QMI_CTRL9, cmd)) return false;
  uint32_t t0 = millis();
  while (!(rd8(QMI_STATUS_INT) & 0x80)) {
    if (millis() - t0 > 100) return false;
    delay(1);
  }
  wr(QMI_CTRL9, QMI_CMD_ACK);
  t0 = millis();
  while (rd8(QMI_STATUS_INT) & 0x80) {
    if (millis() - t0 > 100) return false;
    delay(1);
  }
  return true;
}

// CAL1..CAL3 se escriben byte bajo y luego alto; CAL4 (que identifica la tanda
// de parametros) al reves, alto y luego bajo, como hace SensorLib
static bool writeCal(uint16_t c1, uint16_t c2, uint16_t c3, uint16_t c4) {
  const uint16_t v[3] = { c1, c2, c3 };
  for (uint8_t i = 0; i < 3; i++) {
    if (!wr(QMI_CAL1_L + 2 * i, v[i] & 0xFF) || !wr(QMI_CAL1_L + 2 * i + 1, v[i] >> 8)) return false;
  }
  return wr(QMI_CAL1_L + 7, c4 >> 8) && wr(QMI_CAL1_L + 6, c4 & 0xFF);
}

static uint32_t stepCount() {
  uint8_t b[3];
  if (!rd(QMI_STEP_CNT, b, 3)) return 0;
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
}

bool motionBegin() {
  ok = false;
  if (present(QMI_ADDR_L)) addr = QMI_ADDR_L;
  else if (present(QMI_ADDR_H)) addr = QMI_ADDR_H;
  else {
    Serial.println("QMI8658 no detectado (sin gestos ni podometro)");
    return false;
  }
  // reset por software y espera a que termine (el chip no contesta unos ms)
  wr(QMI_RESET, 0xB0);
  delay(5);
  uint32_t t0 = millis();
  while (!(rd8(QMI_RST_RESULT) & 0x80) && millis() - t0 < 500) delay(10);
  uint8_t id = rd8(QMI_WHO_AM_I);
  if (id != 0x05) {
    Serial.printf("QMI8658: id 0x%02X inesperado (sin gestos ni podometro)\n", id);
    return false;
  }
  wr(QMI_CTRL1, 0x40);                       // auto-incremento para leer los 6 bytes de una vez
  wr(QMI_CTRL7, 0x00);                       // sensores parados mientras se configura
  wr(QMI_CTRL8, 0x80);                       // comandos CTRL9 confirmados por STATUS_INT
  // 4G: una sacudida fuerte pasa de 2G y con ese rango se recortaria
  wr(QMI_CTRL2, QMI_ACC_4G | QMI_ACC_ODR_56HZ);
  wr(QMI_CTRL5, 0x00);

  // Podometro del propio chip (valores del ejemplo de SensorLib): ventana 50
  // muestras, pico a pico 200 mg, pico 100 mg; paso maximo 200 muestras (~3.6 s),
  // minimo 20 (~0.36 s); cuenta a partir de 10 pasos seguidos (descarta meneos)
  // y actualiza el contador de 4 en 4. Va en dos tandas de parametros.
  bool pedo = writeCal(50, 200, 100, 0x0102) && command(QMI_CMD_CONFIG_PEDO) &&
              writeCal(200, (10 << 8) | 20, (4 << 8) | 0, 0x0202) && command(QMI_CMD_CONFIG_PEDO);

  wr(QMI_CTRL7, 0x01);                       // acelerometro en marcha
  // Pedo_EN de 0 a 1 reinicia el contador y arranca el motor de pasos
  wr(QMI_CTRL8, 0x80);
  if (pedo) wr(QMI_CTRL8, 0x80 | 0x10);
  else Serial.println("QMI8658: podometro no disponible");
  stepsBase = stepCount();
  ok = true;
  Serial.printf("QMI8658 listo en 0x%02X (gestos%s)\n", addr, pedo ? " + podometro" : "");
  return true;
}

bool motionAvailable() { return ok; }

void motionPoll(uint32_t now) {
  if (!ok || now - lastPoll < POLL_MS) return;
  lastPoll = now;

  uint8_t b[6];
  if (rd(QMI_AX_L, b, 6)) {
    float x = (int16_t)(b[0] | (b[1] << 8)) * QMI_G_PER_LSB;
    float y = (int16_t)(b[2] | (b[3] << 8)) * QMI_G_PER_LSB;
    float z = (int16_t)(b[4] | (b[5] << 8)) * QMI_G_PER_LSB;
    float mag = sqrtf(x * x + y * y + z * z);
    float dyn = fabsf(mag - 1.0f);  // aceleracion aparte de la gravedad
    lastMag = mag;
    lastDyn = dyn;

    // sacudida: varios picos fuertes seguidos
    if ((int32_t)(now - lastPeak) > SHAKE_WINDOW_MS) peaks = 0;
    if (dyn > SHAKE_PEAK_G && (int32_t)(now - lastPeak) > SHAKE_PEAK_GAP_MS) {
      lastPeak = now;
      if (++peaks >= SHAKE_PEAKS && (int32_t)(now - shakeUntil) >= 0) {
        events |= MOTION_SHAKE;
        shakeUntil = now + SHAKE_COOLDOWN_MS;
        peaks = 0;
      }
    }

    // Cogerla para mirarla: estaba quieta un rato, se mueve, y acaba con la
    // pantalla de cara (|z| alto; en el bolsillo o colgando va de canto). Asi
    // caminar con ella encima no la enciende a cada paso. Con la CPU en light
    // sleep el loop pasa cada ~200 ms, que basta para notar el cambio.
    if (havePrev) {
      float dx = x - px, dy = y - py, dz = z - pz;
      float d = sqrtf(dx * dx + dy * dy + dz * dz);
      bool moving = d > MOVED_DELTA_G || dyn > MOVED_DELTA_G;
      if (d < STILL_DELTA_G && dyn < STILL_DELTA_G) {
        if (!stillSince) stillSince = now;
      } else {
        if (moving && stillSince && now - stillSince >= LIFT_STILL_MS) {
          liftPending = true;
          liftUntil = now + LIFT_WINDOW_MS;
        }
        stillSince = 0;
      }
      if (liftPending) {
        if ((int32_t)(now - liftUntil) > 0) {
          liftPending = false;
        } else if (fabsf(z) > LIFT_FACE_G && (int32_t)(now - movedUntil) >= 0) {
          events |= MOTION_MOVED;
          movedUntil = now + MOVED_COOLDOWN_MS;
          liftPending = false;
        }
      }
    }
    px = x; py = y; pz = z;
    havePrev = true;
  }

  if (now - stepsLastPoll >= STEPS_POLL_MS) {
    stepsLastPoll = now;
    uint32_t c = stepCount();
    if (c < stepsBase) stepsBase = c;  // el chip se reinicio: se empieza de nuevo
    uint32_t d = c - stepsBase;
    if (d) {
      stepsBase = c;
      uint32_t sum = (uint32_t)stepsPending + d;
      stepsPending = sum > 60000 ? 60000 : (uint16_t)sum;
    }
  }
}

uint8_t motionTakeEvents() {
  uint8_t e = events;
  events = 0;
  return e;
}

uint16_t motionTakeSteps() {
  uint16_t s = stepsPending;
  stepsPending = 0;
  return s;
}

bool motionAccel(float &x, float &y, float &z) {
  if (!ok || !havePrev) return false;
  x = px; y = py; z = pz;
  return true;
}

void motionDebug() {
  if (!ok) { Serial.println("imu=off"); return; }
  Serial.printf("imu=on x=%.2f y=%.2f z=%.2f mag=%.2fg dyn=%.2fg quieta=%s peaks=%u steps=%lu (umbral sacudida %.1fg)\n",
                px, py, pz, lastMag, lastDyn, stillSince ? "si" : "no", peaks,
                (unsigned long)stepCount(), SHAKE_PEAK_G);
}
