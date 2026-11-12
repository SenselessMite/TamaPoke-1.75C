#pragma once
#include <stdint.h>

// Microfonos de la placa via el ADC ES7210 (I2C 0x40, datos por el I2S del
// audio). Solo cuenta el VOLUMEN, no reconoce palabras: distingue un sonido
// corto y fuerte (palmada, "eh!") de uno largo y sostenido (soplar).
// Si el ES7210 no responde, todo lo de aqui no hace nada.

enum : uint8_t {
  MIC_CLAP = 1 << 0,  // palmada o llamada corta
  MIC_BLOW = 1 << 1,  // soplar al microfono (~0.7 s sostenido)
};

bool micBegin();              // init ES7210 + tarea de escucha (llamar despues de audioBegin)
bool micAvailable();
void micSetActive(bool on);   // apaga los microfonos con la pantalla apagada o durmiendo (ahorro)
uint8_t micTakeEvents();      // eventos desde la ultima llamada (y los borra)
void micDebug();              // vuelca niveles por serie (comando SENS) para ajustar umbrales
