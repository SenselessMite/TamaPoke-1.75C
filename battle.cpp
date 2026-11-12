#include "battle.h"
#include <Arduino.h>  // random()
#include "dex.h"

// ---------------------------------------------------------------------------
// Tabla de tipos de la gen 1 (simplificada a un tipo por especie). Por cada
// tipo atacante: contra quien es eficaz (x2) y contra quien poco (x0.5; aqui
// tambien caen las inmunidades, ver typeEffPct).
// ---------------------------------------------------------------------------
#define B(t) ((uint16_t)(1u << (t)))

static const uint16_t SUPER[T_COUNT] = {
  /* NORMAL    */ 0,
  /* FUEGO     */ B(T_PLANTA) | B(T_HIELO) | B(T_BICHO),
  /* AGUA      */ B(T_FUEGO) | B(T_TIERRA) | B(T_ROCA),
  /* PLANTA    */ B(T_AGUA) | B(T_TIERRA) | B(T_ROCA),
  /* ELECTRICO */ B(T_AGUA),
  /* HIELO     */ B(T_PLANTA) | B(T_TIERRA) | B(T_DRAGON),
  /* LUCHA     */ B(T_NORMAL) | B(T_HIELO) | B(T_ROCA),
  /* VENENO    */ B(T_PLANTA) | B(T_BICHO),
  /* TIERRA    */ B(T_FUEGO) | B(T_ELECTRICO) | B(T_VENENO) | B(T_ROCA),
  /* PSIQUICO  */ B(T_LUCHA) | B(T_VENENO),
  /* BICHO     */ B(T_PLANTA) | B(T_PSIQUICO) | B(T_VENENO),
  /* ROCA      */ B(T_FUEGO) | B(T_HIELO) | B(T_BICHO),
  /* FANTASMA  */ B(T_FANTASMA) | B(T_PSIQUICO),
  /* DRAGON    */ B(T_DRAGON),
};

static const uint16_t WEAK[T_COUNT] = {
  /* NORMAL    */ B(T_ROCA) | B(T_FANTASMA),
  /* FUEGO     */ B(T_FUEGO) | B(T_AGUA) | B(T_ROCA) | B(T_DRAGON),
  /* AGUA      */ B(T_AGUA) | B(T_PLANTA) | B(T_DRAGON),
  /* PLANTA    */ B(T_FUEGO) | B(T_PLANTA) | B(T_VENENO) | B(T_BICHO) | B(T_DRAGON),
  /* ELECTRICO */ B(T_ELECTRICO) | B(T_PLANTA) | B(T_DRAGON) | B(T_TIERRA),
  /* HIELO     */ B(T_AGUA) | B(T_HIELO),
  /* LUCHA     */ B(T_VENENO) | B(T_PSIQUICO) | B(T_BICHO) | B(T_FANTASMA),
  /* VENENO    */ B(T_VENENO) | B(T_TIERRA) | B(T_ROCA) | B(T_FANTASMA),
  /* TIERRA    */ B(T_PLANTA) | B(T_BICHO),
  /* PSIQUICO  */ B(T_PSIQUICO),
  /* BICHO     */ B(T_FUEGO) | B(T_LUCHA) | B(T_FANTASMA),
  /* ROCA      */ B(T_LUCHA) | B(T_TIERRA),
  /* FANTASMA  */ B(T_NORMAL),
  /* DRAGON    */ 0,
};

uint8_t typeEffPct(uint8_t a, uint8_t d) {
  if (a >= T_COUNT || d >= T_COUNT) return 100;
  if (SUPER[a] & B(d)) return 200;
  if (WEAK[a] & B(d)) return 50;
  return 100;
}

uint16_t hpStat(uint8_t baseHp, uint16_t level) {
  return (uint16_t)((uint32_t)baseHp * 2 * level / 100 + level + 10);
}

// misma formula que Pet::atkStat & co. (base x gen + nivel + entrenamiento)
static uint16_t stat(uint8_t base, uint8_t gene, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base * gene / 100 + lvl + tr;
}

Fighter makeFighter(int16_t dex, uint16_t level, uint8_t gA, uint8_t gD, uint8_t gS,
                    uint8_t tA, uint8_t tD, uint8_t tS) {
  Fighter f;
  if (dex < 1 || dex > DEX_COUNT) dex = 0;
  const DexEntry &e = DEX_TBL[dex];
  f.dex = dex;
  f.level = level ? level : 1;
  f.maxHp = f.hp = hpStat(e.bHp, f.level);
  f.atk = stat(e.bAtk, gA, f.level, tA);
  f.def = stat(e.bDef, gD, f.level, tD);
  f.spe = stat(e.bSpe, gS, f.level, tS);
  f.type = e.type;
  return f;
}

uint8_t timingQuality(int16_t off) {
  if (off < 0) off = -off;
  if (off <= TIMING_PERFECT_PX) return HIT_PERFECT;
  if (off <= TIMING_GOOD_PX) return HIT_GOOD;
  return HIT_MISS;
}

uint8_t attackMulPct(uint8_t q) { return q == HIT_PERFECT ? 150 : q == HIT_GOOD ? 100 : 50; }
uint8_t guardMulPct(uint8_t q) { return q == HIT_PERFECT ? 0 : q == HIT_GOOD ? 50 : 100; }

// Formula de la saga reducida (potencia fija 40). El nivel no se acota: los
// stats crecen con el nivel y los PS tambien, asi que un combate dura ~5-6
// golpes por bando tanto a nivel 20 como a nivel 300.
uint16_t battleDamage(const Fighter &a, const Fighter &d, uint16_t mulPct, uint8_t rndPct) {
  if (mulPct == 0) return 0;
  uint32_t def = d.def ? d.def : 1;
  uint32_t base = ((uint32_t)(2 * a.level / 5 + 2) * 40 * a.atk / def) / 50 + 2;
  uint32_t dmg = base * typeEffPct(a.type, d.type) / 100;
  dmg = dmg * mulPct / 100;
  dmg = dmg * rndPct / 100;
  if (dmg < 1) dmg = 1;
  return dmg > 65535 ? 65535 : (uint16_t)dmg;
}

uint16_t wildLevel(uint16_t petLevel) {
  int32_t l = (int32_t)petLevel + (int32_t)random(-3, 2);  // de -3 a +1
  if (l < 2) l = 2;
  if (l > 999) l = 999;
  return (uint16_t)l;
}

int16_t pickWildDex(uint16_t level) {
  // 85% comunes, 15% raras; solo formas base que salen de huevo (nada legendario)
  uint8_t want = random(100) < 15 ? R_RARO : R_COMUN;
  int16_t pool[DEX_COUNT];
  int n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].rarity == want) pool[n++] = d;
  if (!n) return 16;  // no deberia pasar; Pidgey
  int16_t dex = pool[random(n)];
  // un salvaje de nivel alto ya ha evolucionado (como en los juegos)
  for (int guard = 0; guard < 3; guard++) {
    const DexEntry &e = DEX_TBL[dex];
    if (!e.evolvesTo || !e.evolveLevel || level < e.evolveLevel) break;
    dex = e.evolvesTo;
  }
  return dex;
}

bool battleWon(const Fighter &me, const Fighter &foe) {
  if (foe.hp == 0) return true;
  if (me.hp == 0) return false;
  return (uint32_t)me.hp * foe.maxHp >= (uint32_t)foe.hp * me.maxHp;
}
