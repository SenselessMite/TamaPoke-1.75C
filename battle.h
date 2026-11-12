#pragma once
#include <stdint.h>

// Logica de combate (sin pantalla ni hardware: se prueba en el PC, test/test_battle.cpp).
// La pantalla y el ritmo de los turnos viven en TamaPoke.ino (renderBattle).
//
// Combate por tiempo: en cada turno una aguja barre una barra y el jugador toca
//   - en SU turno, para atacar: zona central = critico, zona buena = normal, fuera = flojo
//   - en el del rival, para defenderse: centro = esquiva, zona buena = bloqueo, fuera = recibe entero
// El dano sale de nivel, ATQ/DEF, tipo (x2 / x0.5) y esa calidad del toque.

#define BATTLE_DAILY 5          // combates a peticion por dia (los salvajes del paseo no gastan)
#define BATTLE_MIN_ENERGY 20    // con menos energia no quiere pelear
#define BATTLE_WINS_PER_RANK 5  // victorias para subir de rango de entrenador
#define BATTLE_MAX_RANK 8
#define BATTLE_MAX_TURNS 16     // tope de ataques: si nadie cae, gana quien conserve mas % de PS
#define WILD_STEPS 400          // cada 400 pasos de paseo...
#define WILD_CHANCE_PCT 50      // ...esta probabilidad de que salte un salvaje

enum : uint8_t { HIT_MISS = 0, HIT_GOOD, HIT_PERFECT };

struct Fighter {
  int16_t dex = 0;
  uint16_t level = 1;
  uint16_t maxHp = 1, hp = 1;
  uint16_t atk = 1, def = 1, spe = 1;
  uint8_t type = 0;
};

// eficacia en % (50, 100 o 200) del tipo atacante contra el defensor. Las
// inmunidades de la gen 1 cuentan como x0.5: un combate sin dano no tiene gracia.
uint8_t typeEffPct(uint8_t atkType, uint8_t defType);

uint16_t hpStat(uint8_t baseHp, uint16_t level);
// rival salvaje: genes neutros (100) y sin entrenamiento, mismas formulas que la mascota
Fighter makeFighter(int16_t dex, uint16_t level, uint8_t geneAtk = 100, uint8_t geneDef = 100,
                    uint8_t geneSpe = 100, uint8_t trAtk = 0, uint8_t trDef = 0, uint8_t trSpe = 0);

// calidad del toque segun lo lejos que quedo la aguja del centro (en px)
uint8_t timingQuality(int16_t offsetFromCenter);
#define TIMING_PERFECT_PX 14
#define TIMING_GOOD_PX 46

// multiplicador (%) por la calidad del toque: atacando, MISS=50 GOOD=100 PERFECT=150;
// defendiendo, lo que recibe: MISS=100 GOOD=50 PERFECT=0 (esquiva)
uint8_t attackMulPct(uint8_t quality);
uint8_t guardMulPct(uint8_t quality);

// dano de a sobre d con un multiplicador total (%) y una variacion 85..100 (%)
uint16_t battleDamage(const Fighter &a, const Fighter &d, uint16_t mulPct, uint8_t rndPct);

// salvaje para un nivel de mascota: especie comun (o rara) que sale de huevo,
// evolucionada si su nivel ya lo permite; nunca legendaria
uint16_t wildLevel(uint16_t petLevel);
int16_t pickWildDex(uint16_t level);

// gana quien deja al otro a 0; al llegar al tope de turnos, el que conserva mas % de PS
bool battleWon(const Fighter &me, const Fighter &foe);
