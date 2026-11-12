#pragma once
#include <stdint.h>

// Ajustes del jugador (menu de ajustes, boton BOOT). Se guardan en la NVS, en un
// espacio de nombres propio ("tpcfg") para no mezclarse con la partida: un
// "nueva partida" no los borra. Las funciones *For son puras (test/test_settings.cpp).

struct Settings {
  uint8_t volume = 3;     // 1..4 (el sonido on/off sigue siendo aparte, en audio)
  uint8_t bright = 3;     // 1..3: tope del brillo del panel
  uint8_t dimMode = 1;    // 0 breve, 1 normal, 2 largo: cuanto tarda en atenuarse/apagarse
  bool usbAwake = false;  // con USB, la pantalla no se atenua
  bool mic = true;        // palmadas y soplidos
  bool gestures = true;   // agitar / coger la placa
  bool steps = true;      // podometro (premios del paseo)
  bool wild = true;       // salvajes al pasear
};
extern Settings gSet;

#define SET_VOLUME_LEVELS 4
#define SET_BRIGHT_LEVELS 3
#define SET_DIM_MODES 3

void settingsLoad();
void settingsSave();

// registro 0x32 (volumen del DAC) del ES8311 para cada nivel: 0xBF = 0 dB, pasos de 0.5 dB
uint8_t volumeRegFor(uint8_t level);
// brillo final: el de power.h escalado por el nivel elegido (sin bajar de 8, el minimo legible)
uint8_t brightnessScaledFor(uint8_t target, uint8_t level);
// inactividad "virtual" para la politica de power.h: breve = 3x mas deprisa, largo = 2x mas despacio
uint32_t idleScaledFor(uint32_t idleMs, uint8_t dimMode);
