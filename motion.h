#pragma once
#include <stdint.h>

// Sensor de movimiento QMI8658 (acelerometro + podometro por hardware).
// Esta en la 1.75C (y en la 1.75); se detecta en el arranque: si no responde,
// todo lo de aqui no hace nada y el juego sigue igual.
//
// Gestos que se reconocen (sondeando el acelerometro desde el loop):
//   MOTION_SHAKE  agitar la placa con fuerza unos instantes (jugar / mecer el huevo)
//   MOTION_MOVED  un meneo suave, p. ej. al cogerla de la mesa (despierta la pantalla)
// Los pasos los cuenta el propio chip, tambien con la CPU en light sleep.

enum : uint8_t {
  MOTION_SHAKE = 1 << 0,
  MOTION_MOVED = 1 << 1,
};

bool motionBegin();                // detecta y configura el QMI8658 (llamar tras Wire.begin)
bool motionAvailable();
void motionPoll(uint32_t nowMs);   // llamar en cada pase del loop (se autolimita)
uint8_t motionTakeEvents();        // gestos ocurridos desde la ultima llamada (y los borra)
uint16_t motionTakeSteps();        // pasos nuevos del podometro desde la ultima llamada
void motionDebug();                // vuelca el estado por serie (comando SENS)
