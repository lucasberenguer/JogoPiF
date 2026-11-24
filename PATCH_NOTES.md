# PATCH NOTES — Guia Completo de Balanceamento do Jogo

> Referência consolidada a partir do código-fonte atual (`src/run_progress.h`, `src/game.c`, `src/gun.h`, `src/bible.h`, `src/encounter.c`, `src/enemy.h`, `src/enemy.c`, `src/cemetery_enemy.c`, `src/village_enemy.c` e `src/necromancer.c`).

---

## 1. Atributos Base do Protagonista

| Atributo | Valor Inicial | Constante / Identificador | Arquivo |
| --- | --- | --- | --- |
| Vida inicial / máxima | `100` PV | `NewRunProgress()` (`health`, `maxHealth`) | `src/run_progress.h` |
| Multiplicador de dano inicial | `1.0x` | `NewRunProgress()` (`damage`) | `src/run_progress.h` |
| Velocidade de movimento base | `204.0 px/s` | `NewRunProgress()` (`speed`) | `src/run_progress.h` |
| Velocidade da esquiva (dash) | `460.0 px/s` | `dodgeSpeed` | `src/game.c` |
| Duração da esquiva (invulnerável) | `0.36 s` | `dodgeDuration` | `src/game.c` |
| Recarga base da esquiva | `5.0 s` | `dodgeRecovery` | `src/game.c` |

---

## 2. Nível Único de Arma e Fórmula de Dano Aditivo

- **Nível único de arma (`0` a `5`, teto `MAX_WEAPON_LEVEL = 5` em `src/run_progress.h`)**:
  - Todas as armas compartilham um único nível na tentativa (`SetRunWeaponLevel` sincroniza `weaponLevel[0..2]`).
  - Ao trocar de arma em um cartão raro, a nova arma assume imediatamente o nível atual da tentativa.
  - Ao atingir o nível `5`, o upgrade de arma (`REWARD_WEAPON_UPGRADE`) sai do pool de sorteio (tanto nas telas normais quanto na tela épica).
- **Dano Base do Golpe Principal (`WeaponBaseDamage(arma)` em `src/run_progress.h`)**:
  - Arma de fogo (`0`): `5` (tiro — `SHOT_DAMAGE` em `src/gun.c`)
  - Bíblia (`1`): `12` (contato — `BibleContactDamage()` em `src/bible.c`)
  - Katana (`2`): `17` (golpe principal — `WeaponBaseDamage(WEAPON_KATANA)` em `src/game.c`)
- **Fórmula de Dano (`EncounterDamage` em `src/encounter.c`)**:
  $$\text{EncounterDamage} = \text{damage} \times (1 + 0{,}2 \times \text{nivel} + 0{,}5 \times \text{epicDamageStacks} + \text{bonusDaArma})$$

| Parâmetro / Valor | Valor | Constante / Função | Arquivo |
| --- | --- | --- | --- |
| Teto de nível da arma | `5` | `MAX_WEAPON_LEVEL` | `src/run_progress.h` |
| Bônus por nível da arma | `+0.20` (`+20%` por nível) | `WEAPON_LEVEL_DAMAGE_BONUS` | `src/run_progress.h` |
| Bônus por acúmulo épico de dano | `+0.50` (`+50%` por stack) | `EPIC_DAMAGE_STACK_BONUS` | `src/run_progress.h` |
| `bonusDaArma` (Bíblia `nivel >= 1`) | `+0.15` (`+15%`) | `BIBLE_L1_DAMAGE_BONUS` | `src/run_progress.h` |
| `bonusDaArma` (Arma de fogo `nivel >= 2`) | `+0.25` (`+25%`) | `GUN_L2_DAMAGE_BONUS` | `src/run_progress.h` |
| Dano base por arma (`{5, 12, 17}`) | `5` / `12` / `17` | `WeaponBaseDamage(int weapon)` | `src/run_progress.h` |

Todo o cálculo é aditivo, acumulativo e sem teto. O dano final aplicado em cada acerto é `ceil(danoBaseDoGolpe * EncounterDamage)`.

---

## 3. Dados Base e os 5 Níveis de Cada Arma

### Katana (`weapon = 2`)

| Nível | Efeito / Regra | Valor | Constante / Identificador | Arquivo |
| --- | --- | --- | --- | --- |
| Base (N0) | Dano base do golpe | `17` | `WeaponBaseDamage(WEAPON_KATANA)` | `src/run_progress.h` / `src/game.c` |
| Base (N0) | Arco de corte (`127°`, `cos(63,5°)`) | `+0.4462f` | `KATANA_BASE_ARC_DOT` | `src/game.c` |
| Base (N0) | Alcance base | `105.0f px` | `KATANA_BASE_RANGE` | `src/game.c` |
| Base (N0) | Recuperação entre golpes | `0.45f s` | `KATANA_BASE_RECOVERY` | `src/game.c` |
| Base (N0) | Recarga base da esquiva | `5.0f s` | `dodgeRecovery` | `src/game.c` |
| Nível 1 | Alcance ampliado | `105 -> 123.0f px` | `KATANA_L1_RANGE` | `src/game.c` |
| Nível 1 | Recuperação reduzida | `0.45 -> 0.40f s` | `KATANA_L1_RECOVERY` | `src/game.c` |
| Nível 2 | Redução da recarga da esquiva ao acertar `>= 1` inimigo (1x por golpe) | `0.8f s` | `KATANA_L2_DODGE_REDUCTION` | `src/game.c` |
| Nível 3 | Arco ampliado (`127° -> 145°`, `cos(72,5°)`) | `+0.3007f` | `KATANA_L3_ARC_DOT` | `src/game.c` |
| Nível 3 | Recuo nos inimigos atingidos | `42.0f px` | `KATANA_L3_KNOCKBACK` | `src/game.c` |
| Nível 4 | Onda de choque frontal — dano base | `22` | `KATANA_SHOCKWAVE_DAMAGE` | `src/game.c` |
| Nível 4 | Onda de choque — recarga / vel. / vida / raio | `2.5f s` / `620.0f` / `0.95f s` / `32.0f` | `KATANA_SHOCKWAVE_COOLDOWN`, `KATANA_SHOCKWAVE_SPEED`, `KATANA_SHOCKWAVE_LIFE`, `KATANA_SHOCKWAVE_RADIUS` | `src/game.c` |
| Nível 5 | Onda tripla em leque (`-20°`, `0°`, `+20°`) | `20.0f°` (máx. `12` ondas) | `KATANA_L5_FAN_ANGLE_DEG`, `KATANA_MAX_SHOCKWAVES` | `src/game.c` |
| Nível 5 | Recarga da onda tripla | `2.5 -> 1.8f s` | `KATANA_L5_SHOCKWAVE_COOLDOWN` | `src/game.c` |

### Bíblia (`weapon = 1`)

| Nível | Efeito / Regra | Valor | Constante / Identificador | Arquivo |
| --- | --- | --- | --- | --- |
| Base (N0) | Quantidade inicial de livros | `4` | `BIBLE_BASE_BOOKS` | `src/bible.h` |
| Base (N0) | Raio inicial da órbita | `90.0f px` | `BIBLE_BASE_ORBIT_RADIUS` | `src/bible.h` |
| Base (N0) | Dano base de contato (sem empurrão no N0) | `12` | `WeaponBaseDamage(1)` / `BibleContactDamage()` | `src/run_progress.h` / `src/bible.c` |
| Base (N0) | Recarga de acerto por inimigo | `0.45f s` | `BIBLE_HIT_COOLDOWN` | `src/bible.h` |
| Base (N0) | Duração e recarga base do boost | `1.25f s` / `4.0f s` | `BIBLE_BASE_BOOST_DURATION`, `BIBLE_BASE_BOOST_COOLDOWN` | `src/bible.h` |
| Nível 1 | Empurrão no contato | `36.0f px` | `BIBLE_L1_KNOCKBACK` | `src/bible.h` |
| Nível 1 | Bônus de dano da arma (`+15%`) | `+0.15f` | `BIBLE_L1_DAMAGE_BONUS` | `src/run_progress.h` |
| Nível 2 | Mais livros e órbita maior | `5` livros / `102.0f px` | `BIBLE_L2_BOOKS`, `BIBLE_L2_ORBIT_RADIUS` | `src/bible.h` |
| Nível 3 | Boost mais longo e recarga menor | `1.65f s` / `2.8f s` | `BIBLE_L3_BOOST_DURATION`, `BIBLE_L3_BOOST_COOLDOWN` | `src/bible.h` |
| Nível 4 | Quantidade máxima de livros + bloqueio de projéteis no boost | `6` livros | `BIBLE_L4_BOOKS` / `BibleBlocksCircle()` | `src/bible.h` / `src/bible.c` |
| Nível 5 | Poças de água benta — dano / intervalo / duração / tick / raio / máx. | `8` / `3.5f s` / `3.0f s` / `0.45f s` / `62.0f px` / `4` | `BIBLE_POOL_DAMAGE`, `BIBLE_POOL_INTERVAL`, `BIBLE_POOL_DURATION`, `BIBLE_POOL_TICK`, `BIBLE_POOL_RADIUS`, `BIBLE_MAX_POOLS` | `src/bible.h` / `src/bible.c` |

### Arma de Fogo (`weapon = 0`)

| Nível | Efeito / Regra | Valor | Constante / Identificador | Arquivo |
| --- | --- | --- | --- | --- |
| Base (N0) | Dano base do tiro | `5` | `WeaponBaseDamage(0)` / `SHOT_DAMAGE` | `src/run_progress.h` / `src/gun.c` |
| Base (N0) | Intervalo e velocidade base (1 tiro) | `0.20f s` / `760.0f px/s` | `GUN_BASE_SHOT_INTERVAL`, `GUN_BASE_SHOT_SPEED` | `src/gun.h` |
| Base (N0) | Inimigos atingidos por tiro (sem perfuração) | `1` | `GUN_BASE_MAX_HITS` | `src/gun.h` |
| Nível 1 | Intervalo e velocidade do tiro | `0.14f s` / `860.0f px/s` | `GUN_L1_SHOT_INTERVAL`, `GUN_L1_SHOT_SPEED` | `src/gun.h` |
| Nível 2 | Bônus de dano da arma (`+25%`) | `+0.25f` | `GUN_L2_DAMAGE_BONUS` | `src/run_progress.h` |
| Nível 3 | Cone de 2 tiros por disparo | `+-5.0f°` | `GUN_L3_SPREAD_DEG` | `src/gun.h` |
| Nível 4 | Tiros perfurantes (atravessam 1 inimigo, acertam até 2 distintos com dano integral no 2º) | `2` acertos máx. | `GUN_L4_MAX_HITS` | `src/gun.h` |
| Nível 5 | Duas pistolas (`4` tiros por disparo) | `+-13.0f px` perpendicular | `GUN_L5_DUAL_OFFSET` | `src/gun.h` |

---

## 4. Raridades, Valores dos Buffs e Regras de Sorteio

### Valores dos Buffs e Pesos

| Raridade / Regra | Tipo (`RewardKind`) | Valor | Constante / Tabela | Arquivo |
| --- | --- | --- | --- | --- |
| Comum (`RARITY_COMMON`) | Dano (`REWARD_DAMAGE`) | `+0.20f` (`+20%`) | `COMMON_DAMAGE_BONUS` | `src/encounter.c` |
| Comum (`RARITY_COMMON`) | Vida máxima + cura (`REWARD_HEALTH`) | `+25` vida máx e `+25` cura | `COMMON_HEALTH_BONUS` | `src/encounter.c` |
| Comum (`RARITY_COMMON`) | Velocidade (`REWARD_SPEED`) | `+18.0f px/s` | `COMMON_SPEED_BONUS` | `src/encounter.c` |
| Raro (`RARITY_RARE`) | Troca de arma (`REWARD_SWAP`) | Outra arma (mesmo nível) + `2` buffs comuns distintos | `COMMON_DAMAGE_BONUS`, `COMMON_HEALTH_BONUS`, `COMMON_SPEED_BONUS` | `src/encounter.c` |
| Raro (`RARITY_RARE`) | Buff duplo (`REWARD_RARE_DUAL`) | Mantém a arma + `2` atributos distintos: `+10%` dano, `+12` vida máx/cura ou `+9.0f` vel. | `RARE_DUAL_DAMAGE_BONUS`, `RARE_DUAL_HEALTH_BONUS`, `RARE_DUAL_SPEED_BONUS` | `src/encounter.c` |
| Épico (`RARITY_EPIC`) | Upgrade da arma (`REWARD_WEAPON_UPGRADE`) | `nivel + 1` (somente se `< 5`) | `MAX_WEAPON_LEVEL` | `src/run_progress.h` |
| Épico (`RARITY_EPIC`) | Poder das Armas (`REWARD_EPIC_DAMAGE`) | `+50%` aditivo (`epicDamageStacks++`) | `EPIC_DAMAGE_STACK_BONUS` | `src/run_progress.h` |
| Épico (`RARITY_EPIC`) | Status (`REWARD_EPIC_STATUS`) | Vida `+50` (fixo), dano `+0.25f` (`+25%`), vel. `+51.0f` (`+25%` da base `204`) | `EPIC_STATUS_HEALTH_BONUS`, `EPIC_STATUS_DAMAGE_BONUS`, `EPIC_STATUS_SPEED_BONUS` | `src/encounter.c` |
| Épico (`RARITY_EPIC`) | Vida plena (`REWARD_EPIC_FULL_HEAL`) | `+50` vida máx e cura total | `EPIC_FULL_HEAL_MAX_HP_BONUS` | `src/encounter.c` |
| Pesos de sorteio | Comum / Raro / Épico | `60` / `30` / `10` | `RARITY_WEIGHT_COMMON`, `RARITY_WEIGHT_RARE`, `RARITY_WEIGHT_EPIC`, `mapRarityWeights` | `src/encounter.c` |

- Todos os buffs são acumulativos e sem teto (exceto o nível `5` da arma).
- **Telas normais (após as ondas anteriores à última onda do mapa)**: `3` cartões. Para cada vaga, sorteia-se a raridade ponderada entre as raridades que ainda têm tipos livres naquela tela e, dentro dela, um tipo inédito na tela. Nunca aparecem dois cartões do mesmo `kind` na mesma tela; a mesma raridade pode repetir.
- **Reroll por cartão (telas normais)**: `1` uso por tela; botão individual abaixo e fora de cada cartão (`RerollButtonRect`); renova só aquele cartão sem repetir nenhum tipo presente nem o anterior; depois do uso, os `3` botões ficam desativados.
- **Tela Épica (`IsEpicRewardStage`)**: `3` cartões épicos distintos entre os disponíveis, **sem reroll**. Ocorre após a última onda de cada mapa antes do chefe (Onda 2 na Clareira, Cemitério, Terra dos Ossos e Portão da Última Sentença) e também ao concluir e selar toda a horda de **Chã da Tristeza**. O checkpoint só é salvo após a escolha.

---

## 5. Estrutura de Ondas, Inimigos e Selos por Mapa

### Resumo de Ondas por Mapa

| Mapa (`map`) | Ondas Normais | Inimigos por Onda | Chefe / Evento Final |
| --- | --- | --- | --- |
| Clareira do Selo (`0`) | `2` (`CLEARING_WAVES`) | Onda 1: `14` / Onda 2: `20` | Guardião da Clareira |
| Cemitério das Raízes (`1`) | `2` (`CEMETERY_WAVES`) | Onda 1: `18` / Onda 2: `24` | Coveiro das Raízes |
| Chã da Tristeza (`2`) | Evento narrativo | Duelo inicial (`1` líder) + Horda (`30` iniciais + `34` reforços = `64` total; máx. `36` ativos) | Recompensa épica após eliminar e selar especiais |
| Capela / Porão / Capela Invertida (`3`, `4`, `5`) | `0` | Exploração / puzzle sem combate | — |
| Terra dos Ossos (`6`) | `2` (`BONELAND_WAVES`) | Onda 1: `26` (`7` cultistas + `19` ghouls) / Onda 2: `34` (`10` cultistas + `24` ghouls) | Rasga-Ossos (Cérbero dos Ossos) |
| Portão da Última Sentença (`7`) | `2` (`TEMPLAR_FIELD_WAVES`) | Onda 1: `5` (`3` espadachins, `2` arqueiros) / Onda 2: `5` (`2` especiais, `2` espadachins, `1` arqueiro) | Igris — O Comandante |
| Topo do Palácio (`8`) | `0` | Arena direta de chefe | Necromante |

### Regras Gerais de Inimigos Comuns e Especiais

- **Redução global de vida em ondas comuns**: inimigos das ondas da Clareira, Cemitério, Terra dos Ossos e Portão têm `-10%` de vida final (`ceil(vida * 0.90)`). Chefes, horda de Chã da Tristeza e invocações do Necromante mantêm suas vidas integrais.
- **Escala por poder do jogador (`adjustment`)**: na Clareira, Cemitério e Terra dos Ossos, a vida dos inimigos comuns escala com `1.0 + min(0.15, max(0, playerPower - 1.0) * 0.05)` (máximo `+15%`).
- **Dano base comum**: corpo a corpo `14` (`ENEMY_MELEE_ATTACK_DAMAGE`), à distância `12` (`ENEMY_ATTACK_DAMAGE`).
- **Entrada escalonada (`WaveSpawnDelay`)**: `index * (0.22 - (wave - 1) * 0.02)` s.

### Status Detalhados dos Inimigos por Mapa

#### Clareira do Selo (`map = 0`) e Cemitério das Raízes (`map = 1`)
- **Composição**: `1` especial por onda (`minimumSpecials = 1 + (wave - 1) / 2`); demais divididos em ~1/3 à distância (`i % 3 == 0`) e ~2/3 corpo a corpo.
- **Vida base (antes de `adjustment` e `-10%`)**:
  - Comum: `45 * (1 + (wave - 1) * 0.12) * 1.10` (`CLEARING_ENEMY_HEALTH_SCALE`).
  - Especial: `100 * (1 + (wave - 1) * 0.12) * 1.10`.
  - **Cemitério**: soma `+90` PV antes do fator `0.90`.
- **Vida aproximada (com `playerPower = 1.0`, após `-10%`)**:
  - Clareira Onda 1 / Onda 2: Comum `45` / `50` PV; Especial `99` / `111` PV.
  - Cemitério Onda 1 / Onda 2: Comum `126` / `131` PV; Especial `180` / `192` PV.
- **Velocidade**: `98.0 * (1 + (wave - 1) * 0.045)` px/s (Onda 1: `98.0`, Onda 2: `102.4`).
- **Escala de dano**: `power = 1.0 + (wave - 1) * 0.09`; `damageScale = 1.15` na Clareira e `1.15 * 1.30 = 1.495` no Cemitério.
- **Projéteis à distância**: vel. `365 px/s` (Clareira) / `273 px/s` (Cemitério); faixa de tiro `170` a `300 px`.

#### Chã da Tristeza / Vila (`map = 2`)
- **Líder da Vila (duelo inicial)**: `20` PV, vel. `185 px/s`.
- **Zumbi Comum**: `90` PV, vel. `104` a `120 px/s` (iniciais) / `112 px/s` (reforços), `power = 1.05`.
- **Bruto Especial (`villageBrute`)**: `1` a cada `10` comuns (`VILLAGE_COMMON_PER_BRUTE = 10`), `300` PV, dano `32`, vel. `82 px/s`, raio de impacto `118 px`, impacto aos `0.64 s`, recuperação `1.23 s`. Entra em `ENEMY_FALLEN` e exige selamento V/F.

#### Terra dos Ossos (`map = 6`)
- **Ghoul (corpo a corpo)**:
  - Onda 1: base `49` (`45` PV com `playerPower = 1.0`), vel. `118 px/s`, `power = 1.00`.
  - Onda 2: base `53` (`48` PV com `playerPower = 1.0`), vel. `125 px/s`, `power = 1.09`.
- **Cultista (à distância)**:
  - Onda 1: base `36` (`33` PV), vel. `80 px/s`, `power = 1.00`.
  - Onda 2: base `39` (`36` PV), vel. `80 px/s`, `power = 1.09`.
  - Projétil: vel. `275 px/s`, soltura em `0.58 s`, duração da conjuração `0.95 s`.

#### Portão da Última Sentença (`map = 7`)

| Inimigo (`TemplarRole`) | Tipo | Vida Base -> Final (`-10%`) | Velocidade | `power` | Observações |
| --- | --- | --- | --- | --- | --- |
| Espadachim (`TEMPLAR_SHIELD`) | Comum Melee | `242` -> `218` PV | `112 px/s` | `1.38x` | Possui defesa frontal (reduz dano pela metade). Impacto aos `0.42 s`. |
| Arqueiro (`TEMPLAR_CROSSBOW`) | Comum Ranged | `173` -> `156` PV | `125 px/s` | `1.50x` | Disparo coordenado (`attackCooldown = 0.65 + 0.1 * i`). |
| Especial / Carrasco (`TEMPLAR_EXECUTIONER`) | Especial | `311` -> `280` PV | `130 px/s` | `1.50x` | Investida de até `167 px` (vel. `620 px/s`), dano base `18` (`* power`). |

### Dificuldade dos Selos Lógicos por Mapa (`sealDifficultyTable` em `src/encounter.c`)

| Mapa | Selo Especial (V/F) `[min..max]` | Selo de Chefe (Múltipla Escolha) `[min..max]` |
| --- | --- | --- |
| Clareira do Selo (`0`) | `[2..3]` | `[4..5]` |
| Cemitério das Raízes (`1`) | `[3..4]` | `[5..6]` |
| Chã da Tristeza (`2`) | `[4..5]` | `[6..7]` |
| Terra dos Ossos (`6`) | `[5..6]` | `[7..8]` |
| Portão da Última Sentença (`7`) | `[5..6]` | `[8..9]` |
| Topo do Palácio (`8`) | — | `[10..12]` |

---

## 6. Status dos Chefes (Bosses)

> Nota: todos os chefes multiplicam sua vida máxima e seus danos por `boss->power` (`fmaxf(1, power)` na vida), além de possuírem raio de colisão de acerto `48.0 px` (`EnemyHurtRadius`).

### 6.1 Guardião da Clareira (`map = 0`)
- **Vida Base**: `2.200` PV (`ARENA_BOSS_BASE_HEALTH`).
- **Velocidade**: `100 px/s` (Fase 1) / `150 px/s` (Fase 2, `1.5x`).
- **Fases**: Fase 2 aos `50%` de vida (`1.100` PV), com transição de `1.0 s`.
- **IA e Pressão**: intervalo entre conjurações de `2.25 s` (F1) / `1.55 s` (F2); esquiva lateral de `0.22 s` (recarga `4.4 s` / `3.4 s`) entre `95` e `215 px`. Pressão corpo a corpo até `132 px`, alcance de golpe `118 px`, intervalo entre golpes melee `1.15 s` (F1) / `1.0 s` (F2).

| Ataque | Dano Base | Aviso (`warning`) | Raio / Alcance | Observações |
| --- | --- | --- | --- | --- |
| Corpo a corpo (`GUARDIAN_MELEE`) | `26` | quadro 4 / `10 fps` | `118 px` | Priorizado em curta distância |
| Raízes (`GUARDIAN_ROOTS`) | `30` | `0.80 s` | `74 px` | Duração total `1.60 s` |
| Onda de Raízes (`GUARDIAN_ROOT_WAVE`) | `36` | `0.65 s` | `60 px` | Impacto entre `+0.17 s` e `+0.42 s` após aviso |
| Maldição (`GUARDIAN_CURSE`) | `42` | `1.00 s` | `110 px` | Grande área circular |

### 6.2 Coveiro das Raízes (`map = 1`)
- **Vida Base**: `2.600` PV (`2.200 + 400`, `ARENA_BOSS_BASE_HEALTH + CEMETERY_BOSS_HEALTH_BONUS`).
- **Velocidade**: `160 * 0.92 = 147.2 px/s` (`COVEIRO_MOVEMENT_SCALE = 0.92`).
- **Fases**: Fase 2 aos `50%` de vida (`1.300` PV); abre com investida dupla.
- **Multiplicador de dano**: `1.15 * 1.10 = 1.265x` sobre os valores base do Guardião (`COVEIRO_DAMAGE_BOOST = 1.10`).

| Ataque | Dano Efetivo (base `power=1`) | Aviso / Impacto | Velocidade / Alcance | Observações |
| --- | --- | --- | --- | --- |
| Pazada corpo a corpo | `33` (`26 * 1.265`) | `0.30 s` (duração `0.72 s`) | `122 px` | Recarga melee `1.65 s` |
| Raízes | `38` (`30 * 1.265`) | `0.62 s` (fim `1.02 s`) | Área marcada | F2 adiciona raízes laterais |
| Arremesso da Pá | `46` (`36 * 1.265`) | `0.62 s` | Ida `620` / `740 px/s`; volta `660` / `760 px/s`; raio `43 px` | F2 lança 3 pás; 1 acerto na ida e 1 na volta |
| Investida | `53` (`42 * 1.265`) | `0.62 s` | `515.2 px/s` (F1) / `625.6 px/s` (F2) | F2 faz investida dupla (2º aviso de `0.33 s`) |

### 6.3 Rasga-Ossos / Cérbero dos Ossos (`map = 6`)
- **Vida Base**: `2.800` PV.
- **Velocidade**: `170 px/s`.
- **Fases**: Fase 2 aos `50%` de vida (`1.400` PV), com ataques e recuperação mais rápidos (pós-dash: `0.68 s` na F1 / `0.52 s` na F2).

| Ataque (`boneAttacks`) | Dano Base | Aviso (`windup`) | Duração Total | Alcance / Raio | Observações |
| --- | --- | --- | --- | --- | --- |
| Garra (`BONE_CLAW`) | `22` | `0.34 s` | `0.72 s` | `120 px` / `120 px` | Curta distância |
| Disparo (`BONE_RANGED`) | `25` | `0.70 s` | `1.24 s` | `550 px` / `27 px` | Projétil a `680 px/s` (`BONE_SHOT_SPEED`) |
| Chuva de Ossos (`BONE_RAIN`) | `28` | `0.74 s` | `1.51 s` | `520 px` / `104 px` | Preferido com linha bloqueada |
| Investida (`BONE_DASH`) | `32` | `0.83 s` | `1.37 s` | `440 px` / `64 px` | Flanco; após dash persegue até alcance da Explosão |
| Explosão (`BONE_BLAST`) | `38` | `0.72 s` | `1.40 s` | `220 px` / `220 px` | Onda de choque evitável com esquiva |

### 6.4 Igris — O Comandante (`map = 7`)
- **Vida Base**: `3.000` PV.
- **Velocidade**: `185 px/s` (mais rápido na forma final).
- **Fases**:
  - **Fase 2**: aos `50%` de vida (`1.500` PV).
  - **Fase 3 (Forma Final)**: aos `25%` de vida (`750` PV, `IGRIS_FINAL_HEALTH_PERCENT = 25`), fica invulnerável na transformação e assume a forma com garra gigante.

| Fase | Ataque | Dano Base | Aviso (`windup`) | Duração | Alcance / Raio | Observações |
| --- | --- | --- | --- | --- | --- | --- |
| F1 / F2 | Golpe Básico (`IGRIS_MELEE`) | `20` | `0.42 s` | `0.86 s` | `140 px` | Curta distância |
| F1 / F2 | Corte do Eclipse (`IGRIS_ECLIPSE`) | `28` | `0.62 s` | `1.19 s` | `225 px` | Cone frontal; abertura da luta |
| F1 / F2 | Erupção Abissal (`IGRIS_ERUPTION`) | `33` | `0.79 s` | `1.64 s` | `120 px` | Sob posição marcada |
| F1 / F2 | Lanças da Umbra (`IGRIS_SPEARS`) | `25` | `0.59 s` | `1.26 s` | `520 px` | 3 lanças a `540 px/s` (`UMBRA_SPEAR_SPEED`) |
| F1 / F2 | Vórtice do Caos (`IGRIS_VORTEX`) | `32` | `0.68 s` | `1.41 s` | `182 px` | Ao redor do chefe |
| F3 (Final) | Garra Final (`IGRIS_FINAL_CLAW`) | `36` | `0.37 s` | `0.79 s` | `178 px` | Golpe básico da forma final |
| F3 (Final) | Rasgo em Z (`IGRIS_Z_RIP`) | `32` (trajeto) / `40` (impacto final) | `0.50 s` | `1.90 s` | `340 px` (raio `64 px`) | Rota planejada antes do aviso; não repete seguido |
| F3 (Final) | Garra do Abismo (`IGRIS_ABYSS_GRAB`) | `24` | `0.56 s` | `1.30 s` | `295 px` (meia-larg. `48 px`) | Prende por até `0.45 s` e puxa o jogador |
| F3 (Final) | Ecos do Eclipse (`IGRIS_ECLIPSE_ECHOES`) | `28` | `0.44 s` por eco | `2.90 s` | `420 px` (larg. `48 px`) | 3 ecos em sequência com corredores marcados |

### 6.5 Necromante (`map = 8`)
- **Vida Base**: `4.200` PV.
- **Velocidade**: `140 px/s` (Formas 1 e 2) / perseguição de `336 px/s` (Forma 3).
- **Fases (3 Formas)**:
  - **Forma 1**: `100%` a `70%` (`4.200` -> `2.940` PV).
  - **Forma 2**: `70%` a `35%` (`2.940` -> `1.470` PV).
  - **Forma 3**: `35%` a `0%` (`1.470` -> `0` PV). Rajadas automáticas de `7`/`11` projéteis a cada `1.12 s` (aviso `0.32 s`).
- **Invulnerabilidade do jogador após acerto**: `0.27 s` (`playerHitCooldown`).

| Habilidade (`skills[]`) | Dano Base | Aviso (`windup`) | Duração Total | Alcance / Geometria / Observações |
| --- | --- | --- | --- | --- |
| Sepulcro (`SEPULCHER`) | `27` | `0.47 s` | `1.19 s` | Ataque da Forma 1 |
| Círculos (`CIRCLES`) | `34` | `0.46 s` | `1.53 s` | Cadência `0.28 s` entre círculos |
| Cajado / Invocação (`STAFF`) | `0` | `0.68 s` | `1.39 s` | Invoca espectros/caveiras |
| Passo Profano (`PROFANE_STEP`) | `31` | `0.46 s` | `1.18 s` | Deslocamento + impacto |
| Halo (`HALO`) | `30` | `0.49 s` | `1.34 s` | Onda radial |
| Runas (`RUNES`) | `37` | `0.52 s` | `1.71 s` | 3 pontos fixos, cadência `0.30 s`, viagem `0.40 s` |
| Lâminas dos Condenados (`CONDEMNED`) | `49` | `0.66 s` | `2.66 s` | Dispara `8` espadas espectrais em cadência |
| Prisão + Raio da Morte (`BONE_BEAM`) | `82` | `1.17 s` | `2.55 s` | Prisão fecha em `360°` aos `0.67 s` (raio `116 px`, dura `2.22 s`); raio de `1400 x 48 px` |
| Explosão Cadavérica (`CORPSE`) | `69` | `0.91 s` | `1.52 s` | Raio `156 px`; cancelável ao destruir a caveira marcada |
| Combo de Lâmina (`BLADE`) | `49` / `57` / `65` | `0.39 s` | `1.64 s` | 3 cortes (`0.39 s`, `0.75 s`, `1.19 s`) com alcances `174`, `189`, `221 px` |
| Investida Alada (`FLIGHT`) | `62` (voo) / `78` (aterrissagem) | `0.49 s` | `1.82 s` | Parada `1.06 s`, impacto `1.37 s`, raio `194 px` |
| Correntes do Submundo (`CHAINS`) | `91` | `0.61 s` | `2.16 s` | Faixa `510 x 34 px`, puxão `0.29 s`, corte aos `1.52 s` (alcance `220 px`) |

---

## 7. Versão do Save e Migração (`PIF-RUN-8`)

- **Formato (`src/menu.c`)**: `PIF-RUN-8` acrescenta o campo inteiro `epicDamageStacks` ao final da linha principal e grava o nível único da arma nas 3 posições de `weaponLevel`.
- **Validação (`LoadClearingCheckpoint` em `src/menu.c`)**: `weaponLevel` aceita `0..5` (`MAX_WEAPON_LEVEL`) e `epicDamageStacks` aceita `0..100`.
- **Retrocompatibilidade e migração**:
  - Lê saves antigos (`PIF-RUN-7` até `PIF-CLEARING-2`).
  - Checkpoints antigos na 3ª onda da Clareira, Cemitério ou Terra dos Ossos retomam diretamente no chefe.

---

## 8. Contrato de Arte da UI

| Elemento | Valor / Regra | Constante | Arquivo |
| --- | --- | --- | --- |
| Moldura Comum | `"assets/ui/card_frame_common.png"` | `UI_CARD_FRAME_COMMON_PATH` | `src/encounter.c` |
| Moldura Rara | `"assets/ui/card_frame_rare.png"` | `UI_CARD_FRAME_RARE_PATH` | `src/encounter.c` |
| Moldura Épica | `"assets/ui/card_frame_epic.png"` | `UI_CARD_FRAME_EPIC_PATH` | `src/encounter.c` |
| Tira do Botão de Reroll | `"assets/ui/reroll_button.png"` | `UI_REROLL_BUTTON_PATH` | `src/encounter.c` |
| Banner de Recompensa | `"assets/ui/reward_banner.png"` | `UI_REWARD_BANNER_PATH` | `src/encounter.c` |

---

## 9. Riscos de Balanceamento e Protocolo de Teste

### Estimativas teóricas de DPS contra 1 alvo (estimativas, não medidas em partida)
Base única para todas as armas: `damage = 1.0`, `epicDamageStacks = 0` e no Nível 5 o multiplicador de `EncounterDamage` da própria arma (`2,00x` na Katana, `2,15x` na Bíblia e `2,25x` na Arma de fogo):
- **Katana (Nível 0 -> Nível 5)**: `~37,8 DPS` (`17` dano / `0,45 s`) -> `~58 a 75 DPS` (no N5, `EncounterDamage = 2,00x`: corte de `ceil(17 * 2,00) = 34` de dano com recuperação de `0,40 s` + onda de choque de `ceil(22 * 2,00) = 44` de dano a cada `1,8 s` = `24,4 DPS` da onda; considerando `~1,0 a 1,5` cortes efetivos/s na janela de aproximação + `1` onda atingindo o alvo).
- **Bíblia (Nível 0 -> Nível 5)**: `~12 DPS` (`12` de dano de contato orbital base sem boost, `~1` acerto/s) -> `~41 a 51 DPS` (no N5, `EncounterDamage = 2,15x`: contato de `ceil(12 * 2,15) = 26` de dano com recarga de `0,45 s` por inimigo e ciclo de boost `1,65 s / 2,8 s` + poça de água benta causando `ceil(8 * 2,15) = 18` de dano a cada `0,45 s` = `40,0 DPS` da poça; variando conforme a permanência do alvo na poça e na órbita de `102 px`).
- **Arma de Fogo (Nível 0 -> Nível 5)**: `25 DPS` (`5` dano / `0,20 s`, `1` tiro por disparo) -> `~179 DPS` (no N5, `EncounterDamage = 2,25x`: cada tiro causa `ceil(5 * 2,25) = 12` de dano a cada `0,14 s` = `7,14` disparos/s; acertando `~2,1` dos `4` tiros por disparo contra um único alvo = `~179 DPS`, podendo chegar a `342,9 DPS` à queima-roupa se os `4` tiros acertarem o mesmo chefe largo).

### Principais pontos de atenção no balanceamento
1. **Arma de fogo acima das outras no Nível 5**: o salto multiplicativo de cadência (`0,20 -> 0,14 s`), bônus próprio (`+25%`), cone duplo (`2` tiros no N3), perfuração (`2` alvos no N4) e pistolas duplas (`4` tiros no N5) coloca o teto de DPS da arma de fogo substancialmente acima da Katana e da Bíblia.
2. **Bíblia sem empurrão no Nível 0**: como a Bíblia começa sem empurrão na base e o upgrade de arma é **Épico** (peso `10` nas telas normais, ou garantido na tela épica após a última onda do mapa), o jogador de Bíblia pode passar a primeira onda sem repulsão de contato contra inimigos corpo a corpo rápidos.

### Tabela de Registro de Testes Práticos (para preenchimento)

| Arma | Nível | Tempo para matar inimigo de 60 PV | Observação |
| --- | --- | --- | --- |
| Katana | 0 | | |
| Katana | 1 | | |
| Katana | 2 | | |
| Katana | 3 | | |
| Katana | 4 | | |
| Katana | 5 | | |
| Bíblia | 0 | | |
| Bíblia | 1 | | |
| Bíblia | 2 | | |
| Bíblia | 3 | | |
| Bíblia | 4 | | |
| Bíblia | 5 | | |
| Arma de fogo | 0 | | |
| Arma de fogo | 1 | | |
| Arma de fogo | 2 | | |
| Arma de fogo | 3 | | |
| Arma de fogo | 4 | | |
| Arma de fogo | 5 | | |

---

## 10. Status das Decisões e Valores

### Em vigor no código atual
- Pesos de raridade: **Comum `60` / Raro `30` / Épico `10`** (`src/encounter.c`).
- Katana N1 (`105 -> 123 px` e `0.45 -> 0.40 s`) e N2 (`-0.8 s` na esquiva ao acertar) invertidos em relação à versão inicial.
- Arma de fogo N4 (tiros perfurantes): **dano integral no 2º inimigo atingido** (`GunHitEnemyCircle` em `src/gun.c`).
- Intervalo da Arma de Fogo no Nível 1: **`0.20 -> 0.14 s`** (`GUN_L1_SHOT_INTERVAL` em `src/gun.h`).
- Dano base da onda de choque da Katana (N4/N5): **`22` de dano** (`KATANA_SHOCKWAVE_DAMAGE` em `src/game.c`).
- Inimigos de ondas comuns com **`-10%` de vida final** e dano base corpo a corpo **`14`**.

### Em espera
- Aumento progressivo da chance de cartão Épico por mapa na tabela `mapRarityWeights` em `src/encounter.c`.
