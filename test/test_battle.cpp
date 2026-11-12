// Tests de la logica de combate (battle.cpp) y de lo que el combate cambia en
// la mascota (Pet::battleEnd & co.).
#include "framework.h"
#include "shim/Arduino.h"
#include "../battle.h"
#include "../dex.h"
#include "../pet.h"

// ---------------------------------------------------------------- tipos
TEST(battle, triangulo_inicial) {
  CHECK_EQ(typeEffPct(T_FUEGO, T_PLANTA), (uint8_t)200);
  CHECK_EQ(typeEffPct(T_PLANTA, T_AGUA), (uint8_t)200);
  CHECK_EQ(typeEffPct(T_AGUA, T_FUEGO), (uint8_t)200);
  CHECK_EQ(typeEffPct(T_FUEGO, T_AGUA), (uint8_t)50);
  CHECK_EQ(typeEffPct(T_AGUA, T_PLANTA), (uint8_t)50);
  CHECK_EQ(typeEffPct(T_PLANTA, T_FUEGO), (uint8_t)50);
}

TEST(battle, inmunidades_cuentan_como_poco_eficaz) {
  CHECK_EQ(typeEffPct(T_NORMAL, T_FANTASMA), (uint8_t)50);
  CHECK_EQ(typeEffPct(T_ELECTRICO, T_TIERRA), (uint8_t)50);
  CHECK_EQ(typeEffPct(T_NORMAL, T_NORMAL), (uint8_t)100);
}

TEST(battle, todo_valor_de_la_tabla_es_50_100_o_200) {
  for (uint8_t a = 0; a < T_COUNT; a++)
    for (uint8_t d = 0; d < T_COUNT; d++) {
      uint8_t e = typeEffPct(a, d);
      CHECK(e == 50 || e == 100 || e == 200);
    }
  CHECK_EQ(typeEffPct(T_COUNT, 0), (uint8_t)100);  // fuera de rango: neutro
}

TEST(battle, cada_especie_tiene_un_tipo_valido) {
  for (int d = 0; d <= DEX_COUNT; d++) CHECK(DEX_TBL[d].type < T_COUNT);
  CHECK_EQ(DEX_TBL[4].type, (uint8_t)T_FUEGO);    // Charmander
  CHECK_EQ(DEX_TBL[7].type, (uint8_t)T_AGUA);     // Squirtle
  CHECK_EQ(DEX_TBL[1].type, (uint8_t)T_PLANTA);   // Bulbasaur
}

// ---------------------------------------------------------------- toque
TEST(battle, calidad_del_toque_por_distancia_al_centro) {
  CHECK_EQ(timingQuality(0), (uint8_t)HIT_PERFECT);
  CHECK_EQ(timingQuality(-TIMING_PERFECT_PX), (uint8_t)HIT_PERFECT);
  CHECK_EQ(timingQuality(TIMING_PERFECT_PX + 1), (uint8_t)HIT_GOOD);
  CHECK_EQ(timingQuality(-TIMING_GOOD_PX), (uint8_t)HIT_GOOD);
  CHECK_EQ(timingQuality(TIMING_GOOD_PX + 1), (uint8_t)HIT_MISS);
  CHECK_EQ(attackMulPct(HIT_PERFECT), (uint8_t)150);
  CHECK_EQ(guardMulPct(HIT_PERFECT), (uint8_t)0);  // esquiva
  CHECK_EQ(guardMulPct(HIT_MISS), (uint8_t)100);
}

// ---------------------------------------------------------------- dano
TEST(battle, esquivar_no_hace_dano_y_un_golpe_nunca_es_cero) {
  Fighter a = makeFighter(4, 20), d = makeFighter(1, 20);
  CHECK_EQ(battleDamage(a, d, 0, 100), (uint16_t)0);
  Fighter debil = makeFighter(129, 2), tanque = makeFighter(95, 60);  // Magikarp vs Onix
  CHECK(battleDamage(debil, tanque, 50, 85) >= 1);
}

TEST(battle, tipo_y_critico_multiplican) {
  Fighter fuego = makeFighter(4, 30), planta = makeFighter(1, 30), agua = makeFighter(7, 30);
  uint16_t vsPlanta = battleDamage(fuego, planta, 100, 100);
  uint16_t vsAgua = battleDamage(fuego, agua, 100, 100);
  CHECK_MSG(vsPlanta > vsAgua * 3, "fuego: x2 contra planta, x0.5 contra agua");
  CHECK(battleDamage(fuego, planta, 150, 100) > vsPlanta);
}

TEST(battle, un_combate_parejo_dura_varios_golpes_a_cualquier_nivel) {
  for (uint16_t lv : { 5, 20, 60, 150, 400 }) {
    Fighter a = makeFighter(19, lv), d = makeFighter(19, lv);  // Rattata contra Rattata
    uint16_t dmg = battleDamage(a, d, 100, 100);
    uint16_t hits = (d.maxHp + dmg - 1) / dmg;
    CHECK_RANGE(hits, 3, 12);
  }
}

TEST(battle, el_entrenamiento_y_los_genes_cuentan) {
  Fighter base = makeFighter(4, 30);
  Fighter fuerte = makeFighter(4, 30, 110, 100, 100, 50, 0, 0);
  CHECK(fuerte.atk > base.atk);
  CHECK_EQ(fuerte.def, base.def);
}

TEST(battle, gana_quien_tumba_al_otro_o_conserva_mas_porcentaje) {
  Fighter me = makeFighter(4, 20), foe = makeFighter(16, 20);
  foe.hp = 0;
  CHECK(battleWon(me, foe));
  foe.hp = foe.maxHp;
  me.hp = 0;
  CHECK(!battleWon(me, foe));
  me.hp = me.maxHp / 2;
  foe.hp = foe.maxHp / 4;
  CHECK(battleWon(me, foe));
  foe.hp = foe.maxHp;
  CHECK(!battleWon(me, foe));
}

// ---------------------------------------------------------------- salvajes
TEST(battle, los_salvajes_nunca_son_legendarios_y_evolucionan_con_el_nivel) {
  randomSeed(7);
  for (int i = 0; i < 400; i++) {
    uint16_t lv = 2 + i;
    int16_t d = pickWildDex(lv);
    CHECK(d >= 1 && d <= DEX_COUNT);
    CHECK(DEX_TBL[d].rarity != R_LEGENDARIO);
    // si su forma tenia evolucion a un nivel ya alcanzado, ha evolucionado
    const DexEntry &e = DEX_TBL[d];
    CHECK_MSG(!e.evolvesTo || !e.evolveLevel || lv < e.evolveLevel || e.rarity == R_EVO,
              "salvaje sin evolucionar por encima de su nivel de evolucion");
  }
}

TEST(battle, nivel_del_salvaje_cerca_del_de_la_mascota) {
  randomSeed(3);
  for (int i = 0; i < 200; i++) {
    CHECK_RANGE(wildLevel(30), 27, 31);
    CHECK(wildLevel(1) >= 2);
  }
}

// ---------------------------------------------------------------- mascota
static void freshPet(Pet &p) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap(); p.eggTap(); p.eggTap();
}

TEST(battle, no_pelea_cansado_dormido_ni_siendo_huevo) {
  Pet p;
  freshPet(p);
  p.energy = 50;
  CHECK(p.canBattle());
  p.energy = BATTLE_MIN_ENERGY - 1;
  CHECK(!p.canBattle());
  p.energy = 50;
  p.sleeping = true;
  CHECK(!p.canBattle());
  Pet h;
  mockNvsReset();
  h.begin();
  h.chooseStarter(1);
  CHECK(!h.canBattle());
}

TEST(battle, cupo_diario_de_combates) {
  Pet p;
  freshPet(p);
  p.setClock(20000UL * 86400);
  for (int i = 0; i < BATTLE_DAILY; i++) {
    CHECK(p.battleQuotaLeft());
    p.useBattleQuota();
  }
  CHECK(!p.battleQuotaLeft());
  p.setClock(20001UL * 86400);  // otro dia
  CHECK(p.battleQuotaLeft());
}

TEST(battle, ganar_premia_y_entrena_perder_solo_desanima) {
  Pet p;
  freshPet(p);
  p.fullness = p.joy = p.energy = p.hygiene = 60;
  uint16_t trBefore = p.trAtk + p.trDef + p.trSpe;
  p.battleEnd(true);
  CHECK_EQ(p.battleWins, (uint16_t)1);
  CHECK_EQ(p.joy, (uint8_t)72);
  CHECK_EQ(p.energy, (uint8_t)50);
  CHECK_EQ((uint16_t)(p.trAtk + p.trDef + p.trSpe), (uint16_t)(trBefore + 2));
  p.battleEnd(false);
  CHECK_EQ(p.battleWins, (uint16_t)1);
  CHECK_EQ(p.joy, (uint8_t)66);
}

TEST(battle, rango_de_entrenador_y_se_guarda) {
  Pet a;
  freshPet(a);
  a.battleWins = BATTLE_WINS_PER_RANK * 3 + 2;
  CHECK_EQ(a.trainerRank(), (uint8_t)3);
  a.battleWins = 60000;
  CHECK_EQ(a.trainerRank(), (uint8_t)BATTLE_MAX_RANK);
  a.battleWins = 7;
  a.energy = 60;
  a.battleEnd(true);  // guarda
  Pet b;
  b.begin();
  CHECK_EQ(b.battleWins, (uint16_t)8);
}
