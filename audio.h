#pragma once
#include <stddef.h>
#include <stdint.h>

// Efectos de sonido del juego (cola, no bloqueante). El orden coincide con la
// tabla SFX de audio.cpp.
enum Sfx : uint8_t {
  SFX_TAP = 0,  // tocar / boton
  SFX_EAT,      // comer
  SFX_PLAY,     // punto del minijuego / golpe
  SFX_HEART,    // le gusta / mimo
  SFX_HATCH,    // eclosion
  SFX_EVOLVE,   // evolucion
  SFX_MEDAL,    // medalla / hito
  SFX_DENY,     // accion no permitida
  SFX_BYE,      // despedida
  SFX_LEVEL,    // sube de nivel
  SFX_SIMON0,   // los cuatro tonos del minijuego de memoria (uno por color)
  SFX_SIMON1,
  SFX_SIMON2,
  SFX_SIMON3,
  SFX_COUNT
};

void audioBegin();          // init ES8311 + I2S + amplificador + tarea de audio
void sfxPlay(uint8_t id);   // encola un efecto (no bloquea el loop)
void audioSetEnabled(bool on);
bool audioEnabled();
void audioSetSleeping(bool sleeping);  // dormida: amplificador apagado
bool audioBusy();                      // un efecto pedido en reposo aun puede estar sonando: no dormir la CPU
void audioSetIdle(bool idle);          // sin uso reciente: amplificador apagado (un efecto lo enciende 4 s)
void audioSetVolume(uint8_t dacReg);   // registro 0x32 del ES8311 (ver volumeRegFor en settings.h)

// entrada de microfono (mic.cpp): comparte el bus I2S con el altavoz
bool audioPlaying();                            // sonando (o recien acabado): el microfono lo oiria
size_t audioMicRead(int16_t *buf, size_t bytes);  // bloquea; solo desde una tarea propia
