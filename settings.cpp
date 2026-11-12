#include "settings.h"
#include <Preferences.h>

Settings gSet;

void settingsLoad() {
  Preferences p;
  p.begin("tpcfg", true);
  Settings d;  // valores por defecto
  gSet.volume = p.getUChar("vol", d.volume);
  gSet.bright = p.getUChar("bri", d.bright);
  gSet.dimMode = p.getUChar("dim", d.dimMode);
  gSet.usbAwake = p.getBool("usbon", d.usbAwake);
  gSet.mic = p.getBool("mic", d.mic);
  gSet.gestures = p.getBool("gest", d.gestures);
  gSet.steps = p.getBool("steps", d.steps);
  gSet.wild = p.getBool("wild", d.wild);
  p.end();
  // valores fuera de rango (NVS corrupta o de otra version): por defecto
  if (gSet.volume < 1 || gSet.volume > SET_VOLUME_LEVELS) gSet.volume = d.volume;
  if (gSet.bright < 1 || gSet.bright > SET_BRIGHT_LEVELS) gSet.bright = d.bright;
  if (gSet.dimMode >= SET_DIM_MODES) gSet.dimMode = d.dimMode;
}

void settingsSave() {
  Preferences p;
  p.begin("tpcfg", false);
  p.putUChar("vol", gSet.volume);
  p.putUChar("bri", gSet.bright);
  p.putUChar("dim", gSet.dimMode);
  p.putBool("usbon", gSet.usbAwake);
  p.putBool("mic", gSet.mic);
  p.putBool("gest", gSet.gestures);
  p.putBool("steps", gSet.steps);
  p.putBool("wild", gSet.wild);
  p.end();
}

uint8_t volumeRegFor(uint8_t level) {
  // -15.5 dB, -7.5 dB, 0 dB (lo de siempre), +4.5 dB
  static const uint8_t REG[SET_VOLUME_LEVELS] = { 0xA0, 0xB0, 0xBF, 0xC8 };
  if (level < 1) level = 1;
  if (level > SET_VOLUME_LEVELS) level = SET_VOLUME_LEVELS;
  return REG[level - 1];
}

uint8_t brightnessScaledFor(uint8_t target, uint8_t level) {
  static const uint8_t PCT[SET_BRIGHT_LEVELS] = { 45, 70, 100 };
  if (level < 1) level = 1;
  if (level > SET_BRIGHT_LEVELS) level = SET_BRIGHT_LEVELS;
  if (target <= 8) return target;  // apagada o casi: no se toca
  uint16_t v = (uint16_t)target * PCT[level - 1] / 100;
  return v < 8 ? 8 : (uint8_t)v;
}

uint32_t idleScaledFor(uint32_t idleMs, uint8_t dimMode) {
  if (dimMode == 0) return idleMs > 0x55555555UL ? 0xFFFFFFFFUL : idleMs * 3;  // breve: 30 s atenua, 100 s apaga
  if (dimMode == 2) return idleMs / 2;                                          // largo: 3 min atenua, 10 min apaga
  return idleMs;
}
