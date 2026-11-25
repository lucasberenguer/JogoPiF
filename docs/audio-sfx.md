# Efeitos sonoros do The Last Rite

O jogo roda em C e raylib. O loop de `src/game.c` entrega a posição do jogador e o estado da janela ao gerenciador de áudio. Armas e inimigos emitem eventos nos pontos em que o combate já prepara, executa ou resolve cada ação. `src/game_audio.c` concentra a reprodução, a atenuação e a posição estéreo. O barramento geral tem um limitador estéreo de pico. Os sons são gerados ou carregados uma vez na abertura.

## Jogador e interface

| Evento | Som | Gatilho no código |
|---|---|---|
| Katana | Corte, acerto, onda de choque e impacto na parede | `game.c`, início do golpe, janela de acerto e colisão da onda |
| Arma de Fogo | Disparo, acerto e impacto na parede | `gun.c`, `UpdateGun` quando termina o atraso da bala e quando ela bate. `game.c`, dano confirmado |
| Bíblia | Invocação, contato dos livros, bloqueio e água benta | `game.c`, início da invocação, bloqueio de projétil e dano confirmado. `bible.c`, criação da poça em `UpdateBible` |
| Esquiva | Rajada durante o dash e aviso discreto quando volta a ficar disponível | `game.c`, início da esquiva e cruzamento de zero no tempo de recarga, inclusive por acerto da Katana |
| Dano, cura, coleta e morte | Quatro efeitos distintos | `game.c`, mudança de vida durante uma partida. `enemy.c`, coleta da cura |
| Porta e transição de mapa | Passagem e transição | `game.c`, passagem confirmada e troca do mapa ativo |
| Menu e pausa | Foco e confirmação por clique ou tecla | `menu.c`, mudança de botão selecionado e confirmação |

As armas não têm crítico ou recarga próprios no código atual. O corte e o disparo tocam mesmo quando erram. A Bíblia aplica dano por contato e por poça, sem mecânica de tiro contra parede.

## Inimigos e lutas

Os nomes em MAIÚSCULAS são as famílias de sons da tabela de presets. `WARN` sinaliza o início do preparo. Dano e derrota passam por `DamageEnemyFrom` ou `DamageNecromancer`. As transições de forma têm o som `BOSS_PHASE`. Os pares marcados como compartilhados usam o fallback da família.

| Local ou inimigo | Habilidades e sons | Gatilho no código |
|---|---|---|
| Clareira, inimigos comuns | Golpe, flecha e investida. Compartilham MELEE, RANGED e CHARGE, com aviso e impacto de projétil | `enemy.c`, `UpdateEnemies` |
| Guardião da Clareira, duas fases | Golpe próximo, raízes, onda de raízes, maldição, ecos e esquiva lateral. Compartilha MELEE, AREA e CHARGE, com as erupções posicionadas no solo | `enemy.c`, `UpdateGuardianMelee`, `StartGuardianCast` e atualização dos perigos em `UpdateEnemies` |
| Cemitério, zumbi, conjurador e golem | Golpe, projétil mágico, avanço e impacto do golem. Compartilham MELEE, MAGIC, CHARGE e AREA | `enemy.c`, `UpdateEnemies`. `cemetery_enemy.c`, `CemeteryUpdateSpecial` |
| Coveiro, duas fases | Golpe próximo, raízes, ida e volta da pá, investidas simples e duplas e Pá Cravada. SHOVEL é próprio. MELEE, AREA, CHARGE e avisos são compartilhados | `cemetery_enemy.c`, `StartCoveiroSkill`, `CemeteryUpdateBoss`, `UpdateCoveiroBuriedCharge` e `CemeteryUpdateProjectiles` |
| Chã da Tristeza, horda e Gordão | Golpe e impacto no chão. Compartilham MELEE e AREA. O surgimento usa `emergencia.wav`, com SUMMON como fallback | `enemy.c`, `UpdateEnemies`. `village_enemy.c`, `VillageUpdateBruteAttack`. `game.c`, surgimento da horda |
| Líder da Vila, duelo | Machado, investida e pulso dos selos. Compartilha MELEE, CHARGE e AREA. O pulso está mapeado no código da IA, embora o duelo atual termine com um acerto | `village_enemy.c`, `VillageUpdateBoss` |
| Capela, Porão e Capela Invertida | Passagens e eventos gerais de jogador e interface. Essas áreas não têm chefes próprios na progressão atual | `encounter.c`, `StartStage`. `game.c`, passagem e troca de mapa |
| Terra dos Ossos, Carniçal, Cultista e Carapaça | Golpe, magia com projétil, impacto da carapaça. Compartilham MELEE, MAGIC e AREA | `enemy.c`, `UpdateEnemies` e `UpdateBoneShell` |
| Rasga-Ossos, duas fases | Garra, projétil, chuva de ossos, investida e explosão. Compartilha MELEE, MAGIC, AREA e CHARGE | `enemy.c`, `UpdateBoneLord` |
| Campo dos Templários, tropa | Golpe de escudo, avanço e golpe do Executor, besta e aura do Estandarte ao fincar a bandeira. Compartilham MELEE, CHARGE, RANGED e SUMMON | `enemy.c`, `UpdateEnemies` |
| Igris, duas fases e forma final | Corpo a corpo, Eclipse, Erupção, lanças, Vórtice, garra, percurso em Z, captura e ecos. Compartilha MELEE, CHARGE, AREA, MAGIC e CHAINS | `enemy.c`, `UpdateIgris`, `UpdateIgrisZ`, `UpdateAbyssGrab` e `UpdateIgrisEchoes` |
| Necromante, três formas, e caveiras | Sepulcro, círculos, cajado, passo profano, halo, runas, espada, voo, espadas espectrais, prisão e feixe, correntes, explosão de caveira e saraivadas automáticas. Feixe e espada espectral têm presets próprios. As outras ações usam as famílias compartilhadas | `necromancer.c`, `StartSkill`, `UpdateSkill`, `UpdateWraiths` e `UpdateNecromancer` |

## Como ouvir e trocar

**F10** abre a lista de efeitos e pausa a partida. Escolha com ↑/↓ e toque com Enter ou clique na linha. Use Tab para alternar o barramento e ←/→ para ajustar o volume. O painel mostra a fonte de cada som, o último evento e as vozes ativas.

Em **Configurações → Áudio**, ajuste geral, música e efeitos. **M** alterna o mudo nessa tela. As preferências ficam em `audio_pif.cfg`, ao lado do executável.

Para trocar um efeito, use o nome que aparece no painel, como `assets/audio/sfx/coveiro_pa.ogg`. A ordem de busca é OGG, MP3 e WAV. Na ausência de arquivo válido, o sintetizador usa três variantes originais, em rodízio, com variação de volume e tom. Novos efeitos são cadastrados em `src/sfx_presets.h` e emitidos via `PlaySfx`, `EmitSfx` ou `EmitSfxTimed`. O código não inclui novos áudios de terceiros.

## Validação

A build Release e os dez testes passaram. O teste de áudio verificou limite global de 14 vozes, limite por efeito, intervalo mínimo, rodízio, variações, atenuação, desbloqueio, pausa, fallback, substituição por WAV e a saída do limitador para sinais acima do pico. O executável abriu e fechou com o dispositivo de áudio ativo e sem erros no log de inicialização. Os testes também cobriram combate, percurso da campanha, mapas e colisões.

Ficam para revisão em partida a escuta dos timbres e a medição de FPS com o áudio ativo durante uma horda. Aviso de pouca vida e passos por material são opcionais. As trilhas individuais por chefe e seus testes pertencem à etapa 2.
