// games.ino — selector de juegos (boton jugar) y minijuegos extra.
// Se concatena detras de TamaPoke.ino (ver menu.ino): usa sus globales y su
// dibujo. El premio de cada juego lo decide Pet::miniGameResult (pet.cpp, con tests).
//
//   QUIZ      "Quien es ese Pokemon?": silueta de la miniatura, 3 nombres, 8 rondas
//   BAYAS     inclinar la placa (o tocar a un lado) para recoger bayas, 30 s
//   MEMORIA   repetir la secuencia de colores y tonos, cada vez mas larga
//   LABERINTO llevar la pokeball a la salida inclinando la placa, 60 s
//
// Inclinar: el acelerometro se lee en ejes del chip y se pasa a ejes de pantalla.
// Por el ejemplo de Waveshare (rotacion segun el eje X), la X del chip apunta hacia
// arriba de la pantalla y la Y hacia la izquierda. Si en tu placa la bola va al
// reves, cambia el signo aqui. La postura con la que empiezas cuenta como "plano".
#define TILT_SX 1.0f   // pantalla x <- TILT_SX * y del chip
#define TILT_SY 1.0f   // pantalla y <- TILT_SY * x del chip

enum : uint8_t { MG_NONE = 0, MG_PICK, MG_QUIZ, MG_BERRY, MG_SIMON, MG_MAZE };
uint8_t mgMode = MG_NONE;
uint32_t mgT0 = 0;           // inicio del juego
uint32_t mgOverUntil = 0;    // pantalla de resultado hasta aqui (0 = jugando)
uint16_t mgScore = 0;
bool mgNewHi = false;
uint8_t mgKind = 0;          // MINI_* del juego en curso (para el resultado)
uint32_t mgLastStep = 0;
float mgTilt0x = 0, mgTilt0y = 0;  // postura inicial = "plano"

bool miniOpen() { return mgMode != MG_NONE; }

// inclinacion en ejes de pantalla (g, respecto a la postura inicial); false sin sensor
static bool tiltScreen(float &sx, float &sy) {
  float x, y, z;
  if (!motionAvailable() || !motionAccel(x, y, z)) return false;
  sx = TILT_SX * (y - mgTilt0y);
  sy = TILT_SY * (x - mgTilt0x);
  return true;
}
static void tiltZero() {
  float x, y, z;
  if (motionAccel(x, y, z)) { mgTilt0x = x; mgTilt0y = y; }
  else { mgTilt0x = mgTilt0y = 0; }
}

// paso de tiempo relativo a 85 ms (el periodo de frame de los juegos, como el minijuego clasico)
static float stepK(uint32_t now) {
  float k = mgLastStep ? (now - mgLastStep) / 85.0f : 1.0f;
  mgLastStep = now;
  return k > 3.0f ? 3.0f : k;
}

// fin de partida: premio, record y pantalla de resultado
static void mgFinish(uint16_t score) {
  mgScore = score;
  mgNewHi = pet.miniGameResult(mgKind, score) && score > 0;
  sfxPlay(mgNewHi ? SFX_MEDAL : (score ? SFX_LEVEL : SFX_DENY));
  mgOverUntil = millis() + 3500;
}

static void drawResult() {
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  char b[32];
  if (mgKind == MINI_MAZE) snprintf(b, sizeof(b), T(S_TIME_FMT), mgScore);
  else snprintf(b, sizeof(b), T(S_SCORE_FMT), mgScore);
  gfx->setTextColor(ink);
  setSize(3);
  setCur(centerX(b, 3), 190);
  printT(b);
  setSize(2);
  if (mgNewHi) {
    gfx->setTextColor(UI_BAR_WARN);
    setCur(centerX(T(S_NEW_RECORD), 2), 240);
    printT(T(S_NEW_RECORD));
  } else {
    snprintf(b, sizeof(b), T(S_RECORD_FMT), pet.miniHi[mgKind]);
    gfx->setTextColor(ink);
    setCur(centerX(b, 2), 240);
    printT(b);
  }
}

// fondo claro en el circulo (memoria, laberinto, selector)
static void plainBg() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
}

// =========================================================== selector de juegos
#define PICK_W 136
#define PICK_H 74
static const int16_t PICK_XY[6][2] = { {93, 104}, {237, 104}, {93, 188}, {237, 188}, {93, 272}, {237, 272} };
// orden: pokeball (clasico), quiz, bayas, memoria, laberinto, combate

void openGamePicker() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  mgMode = MG_PICK;
}

static bool pickEnabled(int i) {
  if (i == 1) return thumbs.loaded;     // necesita las miniaturas de los sprites
  if (i == 4) return motionAvailable();  // solo se juega inclinando
  return true;
}

static const char *pickLabel(int i) {
  switch (i) {
    case 0: return "POKEBALL";
    case 1: return T(S_G_QUIZ);
    case 2: return T(S_G_BERRY);
    case 3: return T(S_G_SIMON);
    case 4: return T(S_G_MAZE);
    default: return "VS";
  }
}

static void renderPicker() {
  plainBg();
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_GAMES), 3), 50);
  printT(T(S_GAMES));
  for (int i = 0; i < 6; i++) {
    int x = PICK_XY[i][0], y = PICK_XY[i][1];
    bool on = pickEnabled(i);
    gfx->fillRoundRect(x, y, PICK_W, PICK_H, 16, on ? UI_WHITE : UI_TRACK);
    gfx->drawRoundRect(x, y, PICK_W, PICK_H, 16, i == 5 ? UI_BAR_BAD : UI_INK);
    const char *l = pickLabel(i);
    int s = textW(l, 2) <= PICK_W - 10 ? 2 : 1;  // nombres largos (aleman...) a tamano 1
    setSize(s);
    gfx->setTextColor(i == 5 ? UI_BAR_BAD : UI_INK);
    setCur(x + (PICK_W - textW(l, s)) / 2, y + PICK_H / 2 - 4 * s);
    printT(l);
  }
  gfx->setTextColor(UI_TRACK);
  setSize(2);
  setCur(centerX(T(S_BOOT_BACK), 2), 372);
  printT(T(S_BOOT_BACK));
  gfx->flush();
}

static void startQuiz();
static void startBerry();
static void startSimon();
static void startMaze();

static void pickerTap(int16_t x, int16_t y) {
  for (int i = 0; i < 6; i++) {
    int tx = PICK_XY[i][0], ty = PICK_XY[i][1];
    if (x < tx || x > tx + PICK_W || y < ty || y > ty + PICK_H) continue;
    if (!pickEnabled(i)) { sfxPlay(SFX_DENY); return; }
    sfxPlay(SFX_TAP);
    mgMode = MG_NONE;
    switch (i) {
      case 0: startGame(); break;
      case 1: startQuiz(); break;
      case 2: startBerry(); break;
      case 3: startSimon(); break;
      case 4: startMaze(); break;
      default: {
        uint16_t lv = wildLevel(pet.level());
        startBattle(pickWildDex(lv), lv, false);
      }
    }
    return;
  }
  if (y > 350 || y < 80) mgMode = MG_NONE;  // fuera de las fichas: cerrar
}

static void beginMini(uint8_t mode, uint8_t kind) {
  mgMode = mode;
  mgKind = kind;
  mgScore = 0;
  mgNewHi = false;
  mgOverUntil = 0;
  mgT0 = millis();
  mgLastStep = 0;
  tiltZero();
}

// =========================================================== quiz
#define QUIZ_ROUNDS 8
#define QUIZ_OPT_Y 286
#define QUIZ_OPT_H 40
#define QUIZ_OPT_GAP 6
struct {
  int16_t dex;
  int16_t opt[3];
  uint8_t round;
  int8_t picked;        // -1 = sin responder
  uint32_t revealUntil;
} qz;

static int16_t quizRandomDex() {
  for (int tries = 0; tries < 60; tries++) {
    int16_t d = 1 + random(DEX_COUNT);
    if (thumbs.get(d)) return d;
  }
  return 25;
}

static void quizNext() {
  qz.dex = quizRandomDex();
  int right = random(3);
  for (int i = 0; i < 3; i++) {
    if (i == right) { qz.opt[i] = qz.dex; continue; }
    int16_t d;
    bool dup;
    int guard = 0;
    do {  // otro nombre distinto del correcto y de los ya puestos
      d = 1 + random(DEX_COUNT);
      dup = (d == qz.dex);
      for (int j = 0; j < i; j++) dup = dup || (qz.opt[j] == d);
    } while (dup && ++guard < 50);
    qz.opt[i] = d;
  }
  qz.picked = -1;
  qz.revealUntil = 0;
}

static void startQuiz() {
  if (!thumbs.loaded) return;
  beginMini(MG_QUIZ, MINI_QUIZ);
  qz.round = 0;
  quizNext();
}

static void quizTap(int16_t x, int16_t y) {
  if (qz.picked >= 0) return;  // ya respondida: esperando la siguiente
  for (int i = 0; i < 3; i++) {
    int oy = QUIZ_OPT_Y + i * (QUIZ_OPT_H + QUIZ_OPT_GAP);
    if (y >= oy && y <= oy + QUIZ_OPT_H && x >= 96 && x <= 370) {
      qz.picked = i;
      qz.revealUntil = millis() + 1400;
      if (qz.opt[i] == qz.dex) { mgScore++; sfxPlay(SFX_HEART); }
      else sfxPlay(SFX_DENY);
      return;
    }
  }
}

static void renderQuiz(uint32_t now) {
  if (qz.picked >= 0 && !timeLeft(qz.revealUntil)) {
    if (++qz.round >= QUIZ_ROUNDS) { mgFinish(mgScore); return; }
    quizNext();
  }
  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  char b[40];
  snprintf(b, sizeof(b), "%u/%u", (unsigned)(qz.round + 1), (unsigned)QUIZ_ROUNDS);
  setSize(2);
  gfx->setTextColor(ink);
  setCur(centerX(b, 2), 30);
  printT(b);
  const char *q = T(S_QUIZ_Q);
  setCur(centerX(q, 2), 54);
  printT(q);
  // silueta (o la imagen de verdad una vez respondida)
  const uint8_t *th = thumbs.get(qz.dex);
  if (th) drawThumb(th, CX - GAL_CELL / 2, 120, 4, qz.picked < 0);
  for (int i = 0; i < 3; i++) {
    int oy = QUIZ_OPT_Y + i * (QUIZ_OPT_H + QUIZ_OPT_GAP);
    uint16_t bg = UI_WHITE;
    if (qz.picked >= 0) {
      if (qz.opt[i] == qz.dex) bg = UI_BAR_OK;          // la buena, siempre en verde
      else if (i == qz.picked) bg = UI_BAR_BAD;          // la elegida, si fallo
    }
    gfx->fillRoundRect(96, oy, 274, QUIZ_OPT_H, 12, bg);
    gfx->drawRoundRect(96, oy, 274, QUIZ_OPT_H, 12, UI_INK);
    const char *n = dexName(qz.opt[i]);
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(centerX(n, 2), oy + 12);
    printT(n);
  }
  (void)now;
  gfx->flush();
}

// =========================================================== bayas
#define BERRY_MS 30000
#define BERRY_N 4
#define BERRY_PET_Y 330   // altura de "la boca": ahi se recoge la baya
struct Berry { float x, y, v; uint8_t kind; bool live; };
Berry br[BERRY_N];
float bPetX = CX, bPetTarget = CX;
uint32_t bNextAt = 0;

static void startBerry() {
  beginMini(MG_BERRY, MINI_BERRY);
  bPetX = bPetTarget = CX;
  for (auto &b : br) b.live = false;
  bNextAt = millis() + 800;
}

static void berryTap(int16_t x, int16_t y) {
  bPetTarget = x;  // sin sensor (o ademas): el bicho va hacia donde tocas
  (void)y;
}

static void renderBerry(uint32_t now) {
  uint32_t el = now - mgT0;
  if (el >= BERRY_MS) { mgFinish(mgScore); return; }
  float k = stepK(now);
  // mover al bicho: inclinando, o hacia el ultimo toque
  float sx, sy;
  if (gSet.gestures && tiltScreen(sx, sy)) {
    bPetX += sx * 22.0f * k;
    bPetTarget = bPetX;
  } else {
    float d = bPetTarget - bPetX;
    float st = 9.0f * k;
    bPetX += fabsf(d) < st ? d : (d > 0 ? st : -st);
  }
  if (bPetX < 96) bPetX = 96;
  if (bPetX > 370) bPetX = 370;
  // nuevas bayas, cada vez mas seguidas y mas rapidas
  if ((int32_t)(now - bNextAt) >= 0) {
    for (auto &b : br) {
      if (b.live) continue;
      b.live = true;
      b.x = 110 + random(246);
      b.y = 70;
      b.v = 3.0f + el / 10000.0f * 1.5f;
      b.kind = random(3);
      break;
    }
    uint32_t gap = 900 - (el / 60 > 450 ? 450 : el / 60);
    bNextAt = now + gap + random(400);
  }
  for (auto &b : br) {
    if (!b.live) continue;
    b.y += b.v * k;
    if (b.y >= BERRY_PET_Y - 30 && b.y <= BERRY_PET_Y + 20 && fabsf(b.x - bPetX) < 46) {
      b.live = false;
      mgScore++;
      sfxPlay(SFX_TAP);
    } else if (b.y > 392) {
      b.live = false;  // al suelo
    }
  }

  drawGameScene();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;
  static const char *const *ICON[3] = { SPR_ICON_FOOD, SPR_ICON_BERRY_B, SPR_ICON_BERRY_G };
  for (auto &b : br)
    if (b.live) drawMap(ICON[b.kind], 16, (int)b.x - 24, (int)b.y - 24, 3, false);
  if (pmd.loaded) drawPmdAct(PMD_IDLE, (int)bPetX, 376, now, true, false, 2);
  else gfx->fillCircle((int)bPetX, 350, 24, ink);
  // marcador y tiempo
  char b[12];
  snprintf(b, sizeof(b), "%u", (unsigned)mgScore);
  gfx->setTextColor(ink);
  setSize(4);
  setCur(centerX(b, 4), 40);
  printT(b);
  int bw = 200, fw = (int)((uint32_t)bw * (BERRY_MS - el) / BERRY_MS);
  gfx->fillRoundRect(CX - bw / 2, 84, bw, 10, 4, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 84, fw, 10, 4, UI_BAR_OK);
  if (el < 2500) {
    setSize(2);
    setCur(centerX(T(S_TILT_HINT), 2), 200);
    printT(T(S_TILT_HINT));
  }
  gfx->flush();
}

// =========================================================== memoria (4 colores)
#define SIMON_MAX 32
#define SIMON_R 54
#define SIMON_OFF 122
static const int16_t SIMON_C[4][2] = { {CX, CY - SIMON_OFF}, {CX + SIMON_OFF, CY}, {CX, CY + SIMON_OFF}, {CX - SIMON_OFF, CY} };
static const uint16_t SIMON_COL[4] = { UI_BAR_BAD, 0x4C98, UI_BAR_OK, UI_BAR_WARN };
uint8_t smSeq[SIMON_MAX];
uint8_t smLen = 0, smPos = 0;
bool smInput = false;          // false = mostrando la secuencia, true = te toca
int8_t smLit = -1;             // color encendido ahora
uint32_t smNextAt = 0, smLitUntil = 0, smLastInput = 0;

static void startSimon() {
  beginMini(MG_SIMON, MINI_SIMON);
  smLen = 1;
  smSeq[0] = random(4);
  smPos = 0;
  smInput = false;
  smLit = -1;
  smNextAt = millis() + 800;
}

static void simonLight(uint8_t c, uint32_t ms) {
  smLit = c;
  smLitUntil = millis() + ms;
  sfxPlay(SFX_SIMON0 + c);
}

static void simonTap(int16_t x, int16_t y) {
  if (!smInput) return;
  for (uint8_t c = 0; c < 4; c++) {
    int dx = x - SIMON_C[c][0], dy = y - SIMON_C[c][1];
    if (dx * dx + dy * dy > (SIMON_R + 14) * (SIMON_R + 14)) continue;
    smLastInput = millis();
    if (c != smSeq[smPos]) {           // fallo: se acaba, cuenta lo completado
      mgFinish(smLen - 1);
      return;
    }
    simonLight(c, 220);
    if (++smPos >= smLen) {            // secuencia completa: una mas
      mgScore = smLen;
      if (smLen >= SIMON_MAX) { mgFinish(smLen); return; }
      smSeq[smLen++] = random(4);
      smPos = 0;
      smInput = false;
      smNextAt = millis() + 900;
    }
    return;
  }
}

static void renderSimon(uint32_t now) {
  if (smLit >= 0 && !timeLeft(smLitUntil)) smLit = -1;
  if (!smInput && (int32_t)(now - smNextAt) >= 0) {
    if (smPos < smLen) {              // mostrar el siguiente de la secuencia (cada vez mas deprisa)
      uint32_t on = smLen > 17 ? 220 : 480 - smLen * 15;
      simonLight(smSeq[smPos++], on);
      smNextAt = now + on + 160;
    } else {                          // fin de la muestra: te toca
      smInput = true;
      smPos = 0;
      smLastInput = now;
    }
  }
  if (smInput && now - smLastInput > 6000) { mgFinish(smLen - 1); return; }  // se canso de esperar

  plainBg();
  for (uint8_t c = 0; c < 4; c++) {
    uint16_t col = (smLit == (int8_t)c) ? SIMON_COL[c] : lerp565(SIMON_COL[c], UI_BG_DAY, 10, 16);
    gfx->fillCircle(SIMON_C[c][0], SIMON_C[c][1], SIMON_R, col);
    gfx->drawCircle(SIMON_C[c][0], SIMON_C[c][1], SIMON_R, UI_INK);
  }
  const char *h = T(smInput ? S_SIMON_GO : S_SIMON_WATCH);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(h, 2), CY - 8);
  printT(h);
  char b[8];
  snprintf(b, sizeof(b), "%u", smLen);
  setCur(centerX(b, 2), 30);
  printT(b);
  gfx->flush();
}

// =========================================================== laberinto
#define MZ_C 6                    // celdas por lado
#define MZ_T (2 * MZ_C + 1)       // casillas por lado (paredes incluidas)
#define MZ_PX 23                  // px por casilla: 13 x 23 = 299 px, cabe en el circulo
#define MZ_X0 (CX - MZ_T * MZ_PX / 2)
#define MZ_Y0 (CY - MZ_T * MZ_PX / 2 + 14)
#define MZ_R 7                    // radio de la bola
#define MAZE_MS 60000
uint8_t mz[MZ_T][MZ_T];           // 1 = pared
float mbx, mby, mvx, mvy;

static void mazeGenerate() {
  memset(mz, 1, sizeof(mz));
  // backtracking iterativo desde la celda (0,0)
  uint8_t stack[MZ_C * MZ_C], sp = 0;
  bool seen[MZ_C * MZ_C] = { false };
  stack[sp++] = 0;
  seen[0] = true;
  mz[1][1] = 0;
  while (sp) {
    uint8_t cur = stack[sp - 1];
    int cx = cur % MZ_C, cy = cur / MZ_C;
    int8_t nb[4], n = 0;
    static const int8_t DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
    for (int d = 0; d < 4; d++) {
      int nx = cx + DX[d], ny = cy + DY[d];
      if (nx >= 0 && ny >= 0 && nx < MZ_C && ny < MZ_C && !seen[ny * MZ_C + nx]) nb[n++] = d;
    }
    if (!n) { sp--; continue; }
    int d = nb[random(n)];
    int nx = cx + DX[d], ny = cy + DY[d];
    mz[2 * cy + 1 + DY[d]][2 * cx + 1 + DX[d]] = 0;  // tirar la pared entre las dos
    mz[2 * ny + 1][2 * nx + 1] = 0;
    seen[ny * MZ_C + nx] = true;
    stack[sp++] = ny * MZ_C + nx;
  }
}

static bool mazeWallAt(float x, float y) {
  int tx = (int)floorf((x - MZ_X0) / MZ_PX), ty = (int)floorf((y - MZ_Y0) / MZ_PX);
  if (tx < 0 || ty < 0 || tx >= MZ_T || ty >= MZ_T) return true;
  return mz[ty][tx] != 0;
}
static bool mazeHits(float x, float y) {
  return mazeWallAt(x - MZ_R, y - MZ_R) || mazeWallAt(x + MZ_R, y - MZ_R) ||
         mazeWallAt(x - MZ_R, y + MZ_R) || mazeWallAt(x + MZ_R, y + MZ_R);
}

static void startMaze() {
  if (!motionAvailable()) return;
  beginMini(MG_MAZE, MINI_MAZE);
  mazeGenerate();
  mbx = MZ_X0 + 1.5f * MZ_PX;
  mby = MZ_Y0 + 1.5f * MZ_PX;
  mvx = mvy = 0;
}

static void renderMaze(uint32_t now) {
  uint32_t el = now - mgT0;
  if (el >= MAZE_MS) { mgFinish(0); return; }
  float k = stepK(now);
  float sx = 0, sy = 0;
  tiltScreen(sx, sy);
  mvx = (mvx + sx * 2.2f * k) * 0.90f;
  mvy = (mvy + sy * 2.2f * k) * 0.90f;
  const float VMAX = 6.0f;
  if (mvx > VMAX) mvx = VMAX;
  if (mvx < -VMAX) mvx = -VMAX;
  if (mvy > VMAX) mvy = VMAX;
  if (mvy < -VMAX) mvy = -VMAX;
  // eje a eje, en pasos cortos para no atravesar paredes
  for (int i = 0; i < 3; i++) {
    float nx = mbx + mvx * k / 3;
    if (mazeHits(nx, mby)) mvx = -mvx * 0.25f; else mbx = nx;
    float ny = mby + mvy * k / 3;
    if (mazeHits(mbx, ny)) mvy = -mvy * 0.25f; else mby = ny;
  }
  int tx = (int)((mbx - MZ_X0) / MZ_PX), ty = (int)((mby - MZ_Y0) / MZ_PX);
  if (tx == MZ_T - 2 && ty == MZ_T - 2) {  // salida: puntua por el tiempo que sobra
    uint16_t left = (MAZE_MS - el) / 1000;
    mgFinish(left ? left : 1);
    return;
  }

  plainBg();
  for (int r = 0; r < MZ_T; r++)
    for (int c = 0; c < MZ_T; c++)
      if (mz[r][c]) gfx->fillRect(MZ_X0 + c * MZ_PX, MZ_Y0 + r * MZ_PX, MZ_PX, MZ_PX, UI_INK);
  gfx->fillRect(MZ_X0 + (MZ_T - 2) * MZ_PX + 3, MZ_Y0 + (MZ_T - 2) * MZ_PX + 3, MZ_PX - 6, MZ_PX - 6, UI_BAR_OK);
  gfx->fillCircle((int)mbx, (int)mby, MZ_R, UI_BAR_BAD);
  gfx->fillCircle((int)mbx - 2, (int)mby - 2, 2, UI_WHITE);
  char b[24];
  snprintf(b, sizeof(b), T(S_TIME_FMT), (unsigned)((MAZE_MS - el + 999) / 1000));
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(b, 2), 46);
  printT(b);
  gfx->flush();
}

// =========================================================== entrada comun
void miniTap(int16_t x, int16_t y) {
  if (mgMode == MG_PICK) { pickerTap(x, y); return; }
  if (mgOverUntil) {  // pantalla de resultado: un toque la cierra
    if (millis() + 3000 > mgOverUntil) mgMode = MG_NONE;
    return;
  }
  switch (mgMode) {
    case MG_QUIZ: quizTap(x, y); break;
    case MG_BERRY: berryTap(x, y); break;
    case MG_SIMON: simonTap(x, y); break;
    default: break;  // laberinto: solo inclinar
  }
}

// boton BOOT: del juego al selector, del selector a la pantalla principal (sin premio)
void miniBack() {
  if (mgMode == MG_PICK || mgOverUntil) mgMode = MG_NONE;
  else mgMode = MG_PICK;
}

void renderMini() {
  uint32_t now = millis();
  if (mgMode == MG_PICK) { renderPicker(); return; }
  if (mgOverUntil) {
    if (!timeLeft(mgOverUntil)) { mgMode = MG_NONE; return; }
    drawGameScene();
    drawResult();
    gfx->flush();
    return;
  }
  lastInteract = now;  // jugando inclinando no se toca la pantalla: que no se atenue
  switch (mgMode) {
    case MG_QUIZ: renderQuiz(now); break;
    case MG_BERRY: renderBerry(now); break;
    case MG_SIMON: renderSimon(now); break;
    case MG_MAZE: renderMaze(now); break;
    default: mgMode = MG_NONE; break;
  }
}
