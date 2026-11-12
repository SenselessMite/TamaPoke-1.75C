// menu.ino — boton BOOT + menu de ajustes.
// El IDE de Arduino concatena este fichero detras de TamaPoke.ino, asi que aqui
// se ven todos sus globales y funciones de dibujo (gfx, setSize, printT, pet...).
// Lo que TamaPoke.ino llama de aqui son funciones (sus prototipos los genera el IDE).
//
// Boton BOOT (GPIO0, libre tras el arranque):
//   pulsacion corta  = en la pantalla principal abre el menu; en cualquier otra
//                      pantalla, "volver" (cierra galeria, ficha, juegos...)
//   pulsacion larga  = sonido on/off al momento
// Con la pantalla atenuada o apagada, el primer toque solo la despierta.

#define BOOT_LONG_MS 800  // KEY_BOOT_PIN esta en pin_config.h (lo usa tambien el light sleep)

void bootButtonSetup() { pinMode(KEY_BOOT_PIN, INPUT_PULLUP); }

// "volver" desde donde se este (la logica de cada pantalla vive en TamaPoke.ino)
void bootBack() {
  if (menuOpen()) { closeMenu(); return; }
  if (miniOpen()) { miniBack(); return; }
  if (battleOpen || sackOpen) return;      // a mitad de combate o del saco no se abandona por aqui
  if (gameOpen) { gameOpen = false; return; }
  if (kbOpen) { kbOpen = false; return; }
  if (clockOpen) { clockOpen = false; return; }
  if (galleryOpen) { galleryOpen = false; galleryPmd.unload(); return; }
  if (cardOpen) { cardOpen = false; return; }
  if (feedMenuUntil || confirmUntil || choiceKind) {
    feedMenuUntil = 0;
    confirmUntil = 0;
    choiceKind = 0;
    return;
  }
  if (pet.awaitingStarter() || pet.ceremony) return;
  openMenu();
}

void pollBootButton(uint32_t now) {
  static bool down = false, longFired = false;
  static uint32_t t0 = 0;
  bool pressed = digitalRead(KEY_BOOT_PIN) == LOW;
  if (pressed && !down) {
    down = true;
    longFired = false;
    t0 = now;
  } else if (pressed && down && !longFired && now - t0 >= BOOT_LONG_MS) {
    longFired = true;  // pulsacion larga: sonido on/off
    audioSetEnabled(!audioEnabled());
    if (audioEnabled()) sfxPlay(SFX_TAP);
    wakeUI();
  } else if (!pressed && down) {
    down = false;
    if (longFired || now - t0 < 30) return;  // rebote
    bool dark = screenOff || dimStage > 0;
    screenOff = false;
    wakeUI();
    if (!dark) bootBack();
    sfxPlay(SFX_TAP);
  }
}

// ---------- menu de ajustes: 4 paginas (deslizar), cada fila se toca para cambiarla ----------
#define MENU_PAGES 4
#define MENU_ROW_Y0 104
#define MENU_ROW_H 54
#define MENU_ROW_X 62
#define MENU_ROW_W 342
#define MENU_ROWS 4

bool menuIsOpen = false;
uint8_t menuPage = 0;
bool menuDirty = false;          // hay que guardar los ajustes al cerrar
int8_t menuConfirmRow = -1;      // pagina de reinicio: fila armada esperando el segundo toque
uint32_t menuConfirmUntil = 0;

bool menuOpen() { return menuIsOpen; }

void openMenu() {
  menuIsOpen = true;
  menuPage = 0;
  menuConfirmRow = -1;
}

void closeMenu() {
  menuIsOpen = false;
  if (menuDirty) {  // la NVS congela ~1 s: una sola escritura al salir, no en cada toque
    settingsSave();
    menuDirty = false;
  }
}

void menuSwipe(int dir) {
  int p = (int)menuPage + (dir > 0 ? -1 : 1);  // izquierda avanza, como la ficha
  menuPage = p < 0 ? 0 : (p >= MENU_PAGES ? MENU_PAGES - 1 : p);
  menuConfirmRow = -1;
}

static const char *onOff(bool v) { return v ? T(S_YES) : T(S_NO); }

// fila: etiqueta a la izquierda, valor a la derecha
static void menuRow(int i, const char *label, const char *value, bool armed) {
  int y = MENU_ROW_Y0 + i * MENU_ROW_H;
  gfx->fillRoundRect(MENU_ROW_X, y, MENU_ROW_W, MENU_ROW_H - 10, 12, armed ? UI_BAR_BAD : UI_WHITE);
  gfx->drawRoundRect(MENU_ROW_X, y, MENU_ROW_W, MENU_ROW_H - 10, 12, UI_INK);
  gfx->setTextColor(armed ? UI_WHITE : UI_INK);
  setSize(2);
  int lw = textW(label, 2), vw = value ? textW(value, 2) : 0;
  if (!value) {  // fila-boton: texto centrado
    setCur(CX - lw / 2, y + 14);
    printT(label);
    return;
  }
  setCur(MENU_ROW_X + 16, y + 14);
  printT(label);
  if (lw + vw + 48 > MENU_ROW_W) return;  // no cabe el valor (idioma largo): se ve al tocar
  gfx->setTextColor(armed ? UI_WHITE : 0x4C98);
  setCur(MENU_ROW_X + MENU_ROW_W - 16 - vw, y + 14);
  printT(value);
}

// barra de nivel "###--" en texto (todas las fuentes la tienen)
static const char *levelBar(uint8_t v, uint8_t n) {
  static char b[8];
  uint8_t i = 0;
  for (; i < n && i < 7; i++) b[i] = i < v ? '#' : '-';
  b[i] = 0;
  return b;
}

static void menuInfoLine(int i, const char *txt) {
  setSize(2);
  gfx->setTextColor(UI_INK);
  setCur(centerX(txt, 2), MENU_ROW_Y0 + 6 + i * 34);
  printT(txt);
}

void renderMenu() {
  if (menuConfirmRow >= 0 && !timeLeft(menuConfirmUntil)) menuConfirmRow = -1;
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  static const StrId TITLES[MENU_PAGES] = { S_MENU_SETTINGS, S_MENU_SENSORS, S_MENU_INFO, S_MENU_RESET };
  const char *title = T(TITLES[menuPage]);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(title, 3), 50);
  printT(title);

  if (menuPage == 0) {
    menuRow(0, T(S_SET_VOLUME), levelBar(gSet.volume, SET_VOLUME_LEVELS), false);
    menuRow(1, T(S_SET_BRIGHT), levelBar(gSet.bright, SET_BRIGHT_LEVELS), false);
    static const StrId DIM[SET_DIM_MODES] = { S_DIM_SHORT, S_DIM_NORMAL, S_DIM_LONG };
    menuRow(2, T(S_SET_DIM), T(DIM[gSet.dimMode % SET_DIM_MODES]), false);
    menuRow(3, T(S_SET_USB), onOff(gSet.usbAwake), false);
  } else if (menuPage == 1) {
    menuRow(0, T(S_SET_MIC), micAvailable() ? onOff(gSet.mic) : "-", false);
    menuRow(1, T(S_SET_GESTURES), motionAvailable() ? onOff(gSet.gestures) : "-", false);
    menuRow(2, T(S_SET_STEPS), motionAvailable() ? onOff(gSet.steps) : "-", false);
    menuRow(3, T(S_SET_WILD), onOff(gSet.wild), false);
  } else if (menuPage == 2) {
    char b[48];
    snprintf(b, sizeof(b), "TamaPoke v%s", FW_VERSION);
    menuInfoLine(0, b);
    int pct = batPercent();
    if (pct >= 0) snprintf(b, sizeof(b), "BAT %d%%  %d mV%s", pct, batMillivolts(), batCharging() ? " +" : "");
    else snprintf(b, sizeof(b), "BAT --  USB");
    menuInfoLine(1, b);
    snprintf(b, sizeof(b), T(S_STEPS_FMT), (unsigned long)pet.stepsTotal);
    menuInfoLine(2, b);
    snprintf(b, sizeof(b), T(S_BTL_RANK_FMT), pet.trainerRank(), pet.battleWins);
    menuInfoLine(3, b);
    snprintf(b, sizeof(b), "RAM %uK  PSRAM %uK", (unsigned)(ESP.getFreeHeap() / 1024),
             (unsigned)(ESP.getFreePsram() / 1024));
    menuInfoLine(4, b);
    uint32_t used, total;
    if (spriteFsUsage(used, total)) snprintf(b, sizeof(b), "SPRITES %lu/%luK", (unsigned long)used, (unsigned long)total);
    else snprintf(b, sizeof(b), "SPRITES --");
    menuInfoLine(5, b);
    snprintf(b, sizeof(b), "UP %lum  IMU %s  MIC %s", (unsigned long)(millis() / 60000),
             motionAvailable() ? "ok" : "-", micAvailable() ? "ok" : "-");
    menuInfoLine(6, b);
  } else {
    menuRow(0, T(S_RESET_STEPS), nullptr, menuConfirmRow == 0);
    menuRow(1, T(S_RESET_GAME), nullptr, menuConfirmRow == 1);
    if (menuConfirmRow >= 0) {
      const char *c = T(S_CONFIRM_AGAIN);
      setSize(2);
      gfx->setTextColor(UI_BAR_BAD);
      setCur(centerX(c, 2), MENU_ROW_Y0 + 2 * MENU_ROW_H + 20);
      printT(c);
    }
  }

  // paginas + como salir
  for (int i = 0; i < MENU_PAGES; i++) {
    if (i == menuPage) gfx->fillCircle(194 + i * 26, 352, 5, UI_INK);
    else gfx->drawCircle(194 + i * 26, 352, 4, UI_INK);
  }
  gfx->setTextColor(UI_TRACK);
  setSize(2);
  setCur(centerX(T(S_BOOT_BACK), 2), 376);
  printT(T(S_BOOT_BACK));
  gfx->flush();
}

void menuTap(int16_t x, int16_t y) {
  // fuera de las filas (arriba del todo o por debajo de los puntos): cerrar
  if (y < 84 || y > 340) { closeMenu(); return; }
  if (menuPage == 2) return;  // info: solo lectura
  if (x < MENU_ROW_X || x > MENU_ROW_X + MENU_ROW_W || y < MENU_ROW_Y0) return;
  int row = (y - MENU_ROW_Y0) / MENU_ROW_H;
  if (row < 0 || row >= MENU_ROWS) return;
  sfxPlay(SFX_TAP);

  if (menuPage == 0) {
    if (row == 0) {
      gSet.volume = gSet.volume % SET_VOLUME_LEVELS + 1;
      audioSetVolume(volumeRegFor(gSet.volume));
    } else if (row == 1) {
      gSet.bright = gSet.bright % SET_BRIGHT_LEVELS + 1;
    } else if (row == 2) {
      gSet.dimMode = (gSet.dimMode + 1) % SET_DIM_MODES;
    } else {
      gSet.usbAwake = !gSet.usbAwake;
    }
    menuDirty = true;
  } else if (menuPage == 1) {
    if (row == 0 && micAvailable()) gSet.mic = !gSet.mic;
    else if (row == 1 && motionAvailable()) gSet.gestures = !gSet.gestures;
    else if (row == 2 && motionAvailable()) gSet.steps = !gSet.steps;
    else if (row == 3) gSet.wild = !gSet.wild;
    menuDirty = true;
  } else if (menuPage == 3 && row <= 1) {
    // doble toque: el primero arma la fila (roja), el segundo en 3 s la ejecuta
    if (menuConfirmRow != row || !timeLeft(menuConfirmUntil)) {
      menuConfirmRow = row;
      menuConfirmUntil = millis() + 3000;
      return;
    }
    menuConfirmRow = -1;
    if (row == 0) {
      pet.resetSteps();
      sfxPlay(SFX_BYE);
    } else {
      // partida nueva: borra la NVS del juego (no los ajustes) y reinicia
      closeMenu();
      pet.factoryReset();
      delay(200);
      ESP.restart();
    }
  }
}
