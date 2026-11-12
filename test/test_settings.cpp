// Tests de los ajustes (settings.cpp) y de los premios de los minijuegos extra.
#include "framework.h"
#include "shim/Arduino.h"
#include "../settings.h"
#include "../pet.h"
#include "../power.h"

TEST(settings, volumen_por_niveles_y_acotado) {
  CHECK_EQ(volumeRegFor(3), (uint8_t)0xBF);  // el de siempre: 0 dB
  CHECK(volumeRegFor(1) < volumeRegFor(2));
  CHECK(volumeRegFor(2) < volumeRegFor(3));
  CHECK(volumeRegFor(3) < volumeRegFor(4));
  CHECK_EQ(volumeRegFor(0), volumeRegFor(1));
  CHECK_EQ(volumeRegFor(99), volumeRegFor(SET_VOLUME_LEVELS));
}

TEST(settings, brillo_escalado_sin_bajar_de_lo_legible) {
  CHECK_EQ(brightnessScaledFor(180, 3), (uint8_t)180);
  CHECK(brightnessScaledFor(180, 1) < brightnessScaledFor(180, 2));
  CHECK_EQ(brightnessScaledFor(0, 1), (uint8_t)0);   // apagada sigue apagada
  CHECK_EQ(brightnessScaledFor(8, 1), (uint8_t)8);   // casi apagada: no se toca
  CHECK(brightnessScaledFor(10, 1) >= 8);
}

TEST(settings, tiempo_de_pantalla) {
  // normal = power.h tal cual; breve atenua a los 30 s; largo, a los 3 min
  CHECK_EQ(idleScaledFor(12345, 1), (uint32_t)12345);
  CHECK_EQ(dimStageFor(idleScaledFor(31000, 0)), (uint8_t)DIM_LOW);
  CHECK_EQ(dimStageFor(idleScaledFor(31000, 1)), (uint8_t)DIM_AWAKE);
  CHECK_EQ(dimStageFor(idleScaledFor(170000, 2)), (uint8_t)DIM_AWAKE);
  CHECK_EQ(dimStageFor(idleScaledFor(190000, 2)), (uint8_t)DIM_LOW);
  CHECK_EQ(idleScaledFor(0xF0000000UL, 0), (uint32_t)0xFFFFFFFFUL);  // sin desbordar
}

TEST(settings, se_guardan_y_se_cargan) {
  mockNvsReset();
  gSet = Settings();
  gSet.volume = 1;
  gSet.bright = 2;
  gSet.dimMode = 2;
  gSet.mic = false;
  gSet.wild = false;
  settingsSave();
  gSet = Settings();
  settingsLoad();
  CHECK_EQ(gSet.volume, (uint8_t)1);
  CHECK_EQ(gSet.bright, (uint8_t)2);
  CHECK_EQ(gSet.dimMode, (uint8_t)2);
  CHECK(!gSet.mic);
  CHECK(!gSet.wild);
  CHECK(gSet.gestures);  // lo no tocado, por defecto
}

TEST(settings, valores_corruptos_vuelven_al_defecto) {
  mockNvsReset();
  gSet = Settings();
  gSet.volume = 0;
  gSet.dimMode = 9;
  settingsSave();
  settingsLoad();
  CHECK_EQ(gSet.volume, Settings().volume);
  CHECK_EQ(gSet.dimMode, Settings().dimMode);
}

// ---------------------------------------------------------------- minijuegos
static void hatched(Pet &p) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap(); p.eggTap(); p.eggTap();
  p.fullness = p.joy = p.energy = p.hygiene = 50;
}

TEST(minijuegos, record_por_juego) {
  Pet p;
  hatched(p);
  CHECK(p.miniGameResult(MINI_QUIZ, 5));
  CHECK(!p.miniGameResult(MINI_QUIZ, 3));   // peor: no es record
  CHECK(p.miniGameResult(MINI_SIMON, 1));   // cada juego lleva el suyo
  CHECK_EQ(p.miniHi[MINI_QUIZ], (uint16_t)5);
  CHECK(!p.miniGameResult(MINI_COUNT, 50)); // juego inexistente: nada
}

TEST(minijuegos, bayas_alimentan_y_entrenan_velocidad) {
  Pet p;
  hatched(p);
  uint8_t spe = p.trSpe;
  p.miniGameResult(MINI_BERRY, 12);
  CHECK_EQ(p.fullness, (uint8_t)74);
  CHECK_EQ(p.trSpe, (uint8_t)(spe + 2));
  CHECK_EQ(p.energy, (uint8_t)42);
}

TEST(minijuegos, laberinto_solo_premia_si_se_termina) {
  Pet p;
  hatched(p);
  p.miniGameResult(MINI_MAZE, 0);
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.energy, (uint8_t)44);
  p.miniGameResult(MINI_MAZE, 20);
  CHECK_EQ(p.joy, (uint8_t)65);
}

TEST(minijuegos, los_records_se_guardan) {
  Pet a;
  hatched(a);
  a.miniGameResult(MINI_SIMON, 9);
  Pet b;
  b.begin();
  CHECK_EQ(b.miniHi[MINI_SIMON], (uint16_t)9);
}

TEST(minijuegos, borrar_pasos_desde_el_menu) {
  Pet a;
  hatched(a);
  a.walk(500);
  a.resetSteps();
  Pet b;
  b.begin();
  CHECK_EQ(b.stepsTotal, (uint32_t)0);
}
