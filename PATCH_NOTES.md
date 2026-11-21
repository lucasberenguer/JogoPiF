# PATCH NOTES — Sistema de Armas (5 Níveis), Raridades, Sorteio e Dano Aditivo

## 1. Nível Único de Arma e Fórmula de Dano Aditivo

- **Nível único de arma (`0` a `5`, teto `MAX_WEAPON_LEVEL = 5` em `src/run_progress.h`)**:
  - Todas as armas compartilham um único nível na tentativa (`SetRunWeaponLevel` sincroniza `weaponLevel[0..2]`).
  - Ao trocar de arma em um cartão raro, a nova arma assume imediatamente o nível atual da largada/tentativa.
  - Ao atingir o nível `5`, o upgrade de arma (`REWARD_WEAPON_UPGRADE`) sai do pool de sorteio (tanto nas telas normais quanto na tela épica).
- **Dano Base do Golpe Principal (`WeaponBaseDamage(arma)` em `src/run_progress.h`)**:
  - Arma de fogo (`0`): `5` (tiro — `SHOT_DAMAGE` em `src/gun.c`)
  - Bíblia (`1`): `12` (contato — `BibleContactDamage()` em `src/bible.c`)
  - Katana (`2`): `20` (golpe principal — `WeaponBaseDamage(WEAPON_KATANA)` em `src/game.c`)
- **Fórmula de Dano (`EncounterDamage` em `src/encounter.c`)**:
  $$\text{EncounterDamage} = \text{damage} \times (1 + 0{,}2 \times \text{nivel} + 0{,}5 \times \text{epicDamageStacks} + \text{bonusDaArma})$$

| Parâmetro / Valor | Valor | Constante / Função | Arquivo |
| --- | --- | --- | --- |
| Teto de nível da arma | `5` | `MAX_WEAPON_LEVEL` | `src/run_progress.h` |
| Bônus por nível da arma | `+0.20` (`+20%` por nível) | `WEAPON_LEVEL_DAMAGE_BONUS` | `src/run_progress.h` |
| Bônus por acúmulo épico de dano | `+0.50` (`+50%` por stack) | `EPIC_DAMAGE_STACK_BONUS` | `src/run_progress.h` |
| `bonusDaArma` (Bíblia `nivel >= 1`) | `+0.15` (`+15%`) | `BIBLE_L1_DAMAGE_BONUS` | `src/run_progress.h` |
| `bonusDaArma` (Arma de fogo `nivel >= 2`) | `+0.25` (`+25%`) | `GUN_L2_DAMAGE_BONUS` | `src/run_progress.h` |
| Dano base por arma (`{5, 12, 20}`) | `5` / `12` / `20` | `WeaponBaseDamage(int weapon)` | `src/run_progress.h` |

Todo o cálculo é aditivo, acumulativo e sem teto. O dano final aplicado em cada acerto é `ceil(danoBaseDoGolpe * EncounterDamage)`.

---

## 2. Dados Base e os 5 Níveis de Cada Arma

### Katana (`weapon = 2`)

| Nível | Efeito / Regra | Valor | Constante / Identificador | Arquivo |
| --- | --- | --- | --- | --- |
| Base (N0) | Dano base do golpe | `20` (era `25`, `-20%`) | `WeaponBaseDamage(WEAPON_KATANA)` | `src/run_progress.h` / `src/game.c` |
| Base (N0) | Arco de corte (`170°`, `cos(85°)`) | `+0.0872f` | `KATANA_BASE_ARC_DOT` | `src/game.c` |
| Base (N0) | Alcance base | `105.0f px` | `KATANA_BASE_RANGE` | `src/game.c` |
| Base (N0) | Recuperação entre golpes | `0.45f s` | `KATANA_BASE_RECOVERY` | `src/game.c` |
| Base (N0) | Recarga base da esquiva | `5.0f s` | `dodgeRecovery` | `src/game.c` |
| Nível 1 | Redução da recarga da esquiva ao acertar `>= 1` inimigo (1x por golpe) | `0.8f s` | `KATANA_L1_DODGE_REDUCTION` | `src/game.c` |
| Nível 2 | Alcance ampliado | `105 -> 123.0f px` | `KATANA_L2_RANGE` | `src/game.c` |
| Nível 2 | Recuperação reduzida *(a confirmar)* | `0.45 -> 0.40f s` | `KATANA_L2_RECOVERY` | `src/game.c` |
| Nível 3 | Arco ampliado (`170° -> 194°`) | `-0.12f` | `KATANA_L3_ARC_DOT` | `src/game.c` |
| Nível 3 | Recuo nos inimigos atingidos | `42.0f px` | `KATANA_L3_KNOCKBACK` | `src/game.c` |
| Nível 4 | Onda de choque frontal — dano base *(a confirmar)* | `22` | `KATANA_SHOCKWAVE_DAMAGE` | `src/game.c` |
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
| Nível 1 | Intervalo *(a confirmar)* e velocidade do tiro | `0.14f s` / `860.0f px/s` | `GUN_L1_SHOT_INTERVAL`, `GUN_L1_SHOT_SPEED` | `src/gun.h` |
| Nível 2 | Bônus de dano da arma (`+25%`) | `+0.25f` | `GUN_L2_DAMAGE_BONUS` | `src/run_progress.h` |
| Nível 3 | Cone de 2 tiros por disparo | `+-5.0f°` | `GUN_L3_SPREAD_DEG` | `src/gun.h` |
| Nível 4 | Tiros perfurantes (atravessam 1 inimigo, acertam até 2 distintos com dano integral no 2º) | `2` acertos máx. | `GUN_L4_MAX_HITS` | `src/gun.h` |
| Nível 5 | Duas pistolas (`4` tiros por disparo) | `+-13.0f px` perpendicular | `GUN_L5_DUAL_OFFSET` | `src/gun.h` |

---

## 3. Raridades, Valores dos Buffs e Regras de Sorteio

### Valores dos Buffs e Pesos

| Raridade / Regra | Tipo (`RewardKind`) | Valor | Constante / Tabela | Arquivo |
| --- | --- | --- | --- | --- |
| Comum (`RARITY_COMMON`) | Dano (`REWARD_DAMAGE`) | `+0.15f` | `COMMON_DAMAGE_BONUS` | `src/encounter.c` |
| Comum (`RARITY_COMMON`) | Vida máxima + cura (`REWARD_HEALTH`) | `+20` vida máx e `+20` cura | `COMMON_HEALTH_BONUS` | `src/encounter.c` |
| Comum (`RARITY_COMMON`) | Velocidade (`REWARD_SPEED`) | `+15.0f` | `COMMON_SPEED_BONUS` | `src/encounter.c` |
| Raro (`RARITY_RARE`) | Troca de arma (`REWARD_SWAP`) | Outra arma (mesmo nível) + `2` buffs comuns distintos | `COMMON_DAMAGE_BONUS`, `COMMON_HEALTH_BONUS`, `COMMON_SPEED_BONUS` | `src/encounter.c` |
| Épico (`RARITY_EPIC`) | Upgrade da arma (`REWARD_WEAPON_UPGRADE`) | `nivel + 1` (somente se `< 5`) | `MAX_WEAPON_LEVEL` | `src/run_progress.h` |
| Épico (`RARITY_EPIC`) | Poder das Armas (`REWARD_EPIC_DAMAGE`) | `+50%` aditivo (`epicDamageStacks++`) | `EPIC_DAMAGE_STACK_BONUS` | `src/run_progress.h` |
| Épico (`RARITY_EPIC`) | Status (`REWARD_EPIC_STATUS`) | Vida `+30..+60` (múltiplos de `10`), dano `+0.25f` (`+25%`), vel. `+60.0f` (`+25%` da base `240`) | `EPIC_STATUS_MIN_TENS`, `EPIC_STATUS_MAX_TENS`, `EPIC_STATUS_DAMAGE_BONUS`, `EPIC_STATUS_SPEED_BONUS` | `src/encounter.c` |
| Épico (`RARITY_EPIC`) | Vida plena (`REWARD_EPIC_FULL_HEAL`) | `+50` vida máx e cura total | `EPIC_FULL_HEAL_MAX_HP_BONUS` | `src/encounter.c` |
| Pesos de sorteio | Comum / Raro / Épico | `70` / `25` / `5` | `RARITY_WEIGHT_COMMON`, `RARITY_WEIGHT_RARE`, `RARITY_WEIGHT_EPIC`, `mapRarityWeights` | `src/encounter.c` |

- Todos os buffs são acumulativos e sem teto (exceto o nível `5` da arma).
- **Telas normais (após ondas 1 a 3)**: `3` cartões. Para cada vaga, sorteia-se a raridade ponderada entre as raridades que ainda têm tipos livres naquela tela e, dentro dela, um tipo inédito na tela. Nunca aparecem dois cartões do mesmo `kind` na mesma tela; a mesma raridade pode repetir.
- **Reroll por cartão (telas normais)**: `1` uso por tela; botão individual abaixo e fora de cada cartão (`RerollButtonRect`); renova só aquele cartão sem repetir nenhum tipo presente nem o anterior; depois do uso, os `3` botões ficam desativados. Removido o reroll geral da tecla `R`.
- **Tela Épica (após a onda 4 / antes do chefe)**: `3` cartões épicos distintos entre os disponíveis, **sem reroll**. O checkpoint só é salvo após a escolha.

---

## 4. Versão do Save e Migração (`PIF-RUN-8`)

- **Novo formato (`src/menu.c`)**: `PIF-RUN-8` acrescenta o campo inteiro `epicDamageStacks` ao final da linha principal e grava o nível único da arma nas 3 posições de `weaponLevel`.
- **Validação (`LoadClearingCheckpoint` em `src/menu.c`)**: `weaponLevel` aceita `0..5` (`MAX_WEAPON_LEVEL`) e `epicDamageStacks` aceita `0..100`.
- **Retrocompatibilidade e migração**:
  - Lê saves antigos (`PIF-RUN-7`, `PIF-RUN-6`, `PIF-RUN-5`, `PIF-CLEARING-4`, `PIF-CLEARING-3`, `PIF-CLEARING-2`).
  - Ao carregar formatos anteriores a `PIF-RUN-8`: se `strongWeapon >= 0`, converte para `epicDamageStacks = 1` (caso contrário `0`); se os níveis por arma forem diferentes, aplica o nível da arma equipada (`weaponLevel[weapon]`) para todas as 3 armas.

---

## 5. Contrato de Arte da UI

Quando as artes definitivas forem adicionadas em `assets/ui/`, o motor as carregará automaticamente uma única vez (`EnsureRewardUiAssetsLoaded` em `src/encounter.c`), substituindo o placeholder procedural:

| Elemento | Valor / Regra | Constante | Arquivo |
| --- | --- | --- | --- |
| Moldura Comum | `"assets/ui/card_frame_common.png"` | `UI_CARD_FRAME_COMMON_PATH` | `src/encounter.c` |
| Moldura Rara | `"assets/ui/card_frame_rare.png"` | `UI_CARD_FRAME_RARE_PATH` | `src/encounter.c` |
| Moldura Épica | `"assets/ui/card_frame_epic.png"` | `UI_CARD_FRAME_EPIC_PATH` | `src/encounter.c` |
| Tira do Botão de Reroll | `"assets/ui/reroll_button.png"` | `UI_REROLL_BUTTON_PATH` | `src/encounter.c` |
| Tamanho do cartão (`ChoiceRect`) | `330 x 140 px` (largura reduz em janelas `< 1120 px`) | `ChoiceRect()` | `src/encounter.c` |
| Corte do 9-slice (`DrawTextureNPatch`) | `16 px` nas 4 bordas (`left/top/right/bottom = 16`, `NPATCH_NINE_PATCH`) | `UI_CARD_NPATCH_SLICE` | `src/encounter.c` |
| Tamanho e posição do botão de reroll | `150 x 34 px`, centralizado `10 px` abaixo do cartão (`card.y + card.height + 10`) | `RerollButtonRect()` | `src/encounter.c` |
| Ordem dos 3 estados na tira do botão | Horizontal (`largura / 3`): `0 = Normal`, `1 = Hover`, `2 = Desativado` | `DrawRerollButton()` | `src/encounter.c` |

---

## 6. Riscos de Balanceamento e Protocolo de Teste

### Estimativas teóricas de DPS contra 1 alvo (estimativas, não medidas em partida)
Considerando `damage = 1.0` e `epicDamageStacks = 0`:
- **Katana (Nível 0 -> Nível 5)**: `~44 DPS` (`20` dano / `0,45 s`) -> `~62 a 87 DPS` (corte de `40` dano a cada `0,40 s` = `100 DPS` corpo a corpo puro, ou `~62 a 87 DPS` considerando tempo de aproximação/janela de corte + onda de choque de `44` dano a cada `1,8 s`).
- **Bíblia (Nível 0 -> Nível 5)**: `~12 DPS` (contato orbital sem boost) -> `~41 a 51 DPS` (`6` livros com `+115%` de dano = `26` por contato + ciclo de boost `1,65 s / 2,8 s` + poça de água benta causando `18` de dano a cada `0,45 s`).
- **Arma de Fogo (Nível 0 -> Nível 5)**: `25 DPS` (`5` dano / `0,20 s`, `1` tiro) -> `~179 DPS` (`4` tiros por disparo a cada `0,14 s` caso os 4 projéteis acertem o mesmo alvo a curta/média distância, com `+125%` de dano = `12` por tiro, além de perfuração contra grupos).

### Principais pontos de atenção no balanceamento
1. **Arma de fogo acima das outras no Nível 5**: o salto multiplicativo de cadência (`0,20 -> 0,14 s`), bônus próprio (`+25%`), cone duplo (`2` tiros no N3), perfuração (`2` alvos no N4) e pistolas duplas (`4` tiros no N5) coloca o teto de DPS da arma de fogo substancialmente acima da Katana e da Bíblia.
2. **Bíblia sem empurrão no Nível 0**: como a Bíblia começa sem empurrão na base e o upgrade de arma agora é de raridade **Épica** (peso `5` nas ondas 1–3, ou garantido apenas na tela épica após a onda 4), o jogador de Bíblia pode passar toda a primeira área (ondas 1 a 4) sem repulsão de contato contra inimigos corpo a corpo rápidos.

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

## 7. Status das Decisões e Valores

### Decididos pelo dono do projeto
- Pesos de raridade: **Comum `70` / Raro `25` / Épico `5`** (`RARITY_WEIGHT_COMMON`, `RARITY_WEIGHT_RARE`, `RARITY_WEIGHT_EPIC` em `src/encounter.c`).
- Arma de fogo N4 (tiros perfurantes): **dano integral no 2º inimigo atingido** (`GunHitEnemyCircle` em `src/gun.c`).

### A confirmar (usando valores provisórios configurados em constantes)
- **Recuperação da Katana no Nível 2**: `0.45 -> 0.40 s` (`KATANA_L2_RECOVERY` em `src/game.c`).
- **Intervalo da Arma de Fogo no Nível 1**: `0.20 -> 0.14 s` (`GUN_L1_SHOT_INTERVAL` em `src/gun.h`).
- **Dano base da onda de choque da Katana (N4/N5)**: `22` de dano (`KATANA_SHOCKWAVE_DAMAGE` em `src/game.c`).
- **Texto do cartão épico de +50% (`REWARD_EPIC_DAMAGE`)**: exibe apenas o dano do golpe principal `antes->depois` (`ceil(WeaponBaseDamage * EncounterDamage)` em `RewardText`, `src/encounter.c`).

### Em espera
- Aumento progressivo da chance de cartão Épico por mapa na tabela `mapRarityWeights` em `src/encounter.c`.
