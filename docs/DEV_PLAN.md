# Jogo PIF — elicitação de requisitos e sprint de três semanas

Este é o registro original do planejamento. A versão integrada já usa quatro
hordas e um chefe em cada arena, com checkpoint por mapa, apresentações e
selos. As descrições de implementação abaixo refletem a base do sprint.

## 1. Objetivo e escopo confirmado

Demonstrar uma partida jogável que começa no menu, passa pelas cinco ondas e pelo Guardião da Clareira, segue para o Cemitério das Raízes e termina com o selamento do Coveiro. A demonstração deve usar o fluxo normal do jogo.

- **Programação e integração:** Arthur e Berenguer. Neste projeto, backend significa lógica local do jogo em C/raylib, incluindo física, combate, progressão e salvamento.
- **Visual:** Marcos, Silvio, Lira, JP e Caua. A prioridade confirmada é completar os sprites que faltam e melhorar a leitura dos golpes. A divisão individual abaixo é uma proposta para distribuir a carga.
- **Cadência:** um sprint de três semanas, com uma versão jogável e revisável ao fim de cada semana.

**Recorte proposto para caber no prazo.** Manter no cemitério os quatro esqueletos e dois magos já implementados, seguidos pelo Coveiro. As cinco ondas do cemitério, a terceira arena, a campanha final, a ultimate, a ajuda do anjo e a metaprogressão após a derrota ficam no backlog da campanha. O objetivo deste sprint é fechar uma partida nas duas arenas existentes. Para a pá do Coveiro, a versão base usa um arremesso que termina no impacto, na parede ou ao expirar. O retorno e a captura são extras, condicionados à conclusão do fluxo principal.

## 2. Estado atual e pontos de integração

| Parte | Base existente | Lacuna do sprint |
| --- | --- | --- |
| Menu, entrada e partida | `src/menu.c`, `src/weapon_select.c` e `src/game.c` carregam a partida, recebem comandos e trocam de mapa. | A saída inferior da clareira já carrega o cemitério, mas atualmente permite a troca antes do selo. A tecla F7 pula direto para a segunda área. |
| Mundo e física | `src/clearing.c` usa posição pelos pés e coordenadas do mundo. A clareira tem máscara de colisão e camada de primeiro plano. O cemitério usa bases retangulares. | Validar pontos de surgimento e caminho até o jogador nas duas áreas. Revisar túmulos, capela, bordas, saída e trajetórias rápidas com F3. |
| Combate | `src/bible.c`, `src/gun.c` e `src/game.c` controlam os ataques das armas. `src/enemy.c` e `src/cemetery_enemy.c` controlam inimigos e chefes. | Conferir as janelas de dano e avisos. Magos ainda podem disparar contra obstáculos. A escala e os quadros de vários inimigos do cemitério são provisórios. |
| Ondas e selos | `src/encounter.c` já controla ondas, recompensas e selos da clareira. O Coveiro tem três ataques e fica invulnerável até os seis guardas morrerem. | O controlador de encontros da clareira é suspenso no cemitério. Faltam selamento do Coveiro, tratamento de derrota da segunda área e confirmação de conclusão da partida. |
| Salvamento | `src/run_progress.h` e `src/menu.c` armazenam arma, build, vida e etapa da clareira. | Salvar a área e restaurar a etapa certa ao usar Continuar no cemitério. Preservar a leitura dos saves antigos. |
| Verificação e arte | CMake, CTest, `tests/enemy_audit_test.c`, `tests/encounter_test.c`, `assets/` e `design/`. | Os testes cobrem partes do combate e da clareira. Acrescentar a travessia real, o selo do Coveiro e o carregamento da segunda área. Revisar os novos sprites dentro do jogo. |

Fontes do levantamento: [base de design](GAME_DESIGN.md), [implementação do cemitério](cemiterio_implementacao.md), [sprites do Guardião](../design/boss_clareira.md) e [referências do Coveiro](../design/coveiro_ataques_v2/LEIA-ME.md). Quando uma nota antiga divergir, usar o comportamento observado no código como ponto de partida e o requisito deste sprint como destino.

## 3. Requisitos e critérios de aceite

**P0** bloqueia a demonstração. **P1** entra depois de a sequência principal estar integrada. Os parâmetros de balanceamento continuam ajustáveis até o teste de jogo da segunda semana.

| ID | Prioridade | Requisito | Como aceitar |
| --- | --- | --- | --- |
| R01 | P0 | Começar pelo menu, escolher uma das três armas e manter uma arma equipada por vez. | Iniciar uma partida com cada arma e conferir movimento, espera, ataque e esquiva. |
| R02 | P0 | Mover, mirar e esquivar de forma consistente. WASD move, botão esquerdo ataca com katana ou arma de fogo, Espaço esquiva, botão direito acelera a Bíblia equipada ou esquiva com as outras armas. | Comparar movimento reto e diagonal. Repetir a 30 e 60 FPS. A distância por segundo e a duração da esquiva devem permanecer equivalentes. Um clique gera um ataque. |
| R03 | P0 | Usar o ponto dos pés e colisores do mundo para movimento. Projéteis e avanços rápidos respeitam obstáculos. | Com F3, percorrer limites, túmulos, bases da capela, galhos e passagens. Encostar, deslizar e esquivar junto às bordas. Disparar contra paredes e conferir que a trajetória termina na colisão. |
| R04 | P0 | Fazer inimigos nascerem em posições livres da arena, alcançáveis a partir da área do jogador. | Repetir o surgimento em diferentes posições do jogador na clareira. Conferir os sete pontos do cemitério e os pontos isolados que já causaram travamento. Concluir as ondas e o encontro sem inimigos inacessíveis. |
| R05 | P0 | Preservar cinco ondas na clareira, recompensas entre ondas e bônus forte antes do Guardião. | Confirmar contagem de inimigos, especial por onda, escolhas de bônus, reroll e avanço só após resolver os inimigos que exigem selo. O Guardião aparece após a escolha forte. |
| R06 | P0 | Abrir a passagem normal para o cemitério após o selo do Guardião. | Antes do selo, a saída informa o objetivo e bloqueia a transição. Depois do selo, a abertura inferior leva à entrada do cemitério, preservando arma, vida e build. Tiros, ataques e inimigos da área anterior são limpos. F7 fica restrito à verificação de desenvolvimento. |
| R07 | P0 | Resolver o encontro do cemitério com esqueletos corpo a corpo, magos e Coveiro. | Derrotar os seis guardas, confirmar que o Coveiro só então fica vulnerável e observar o ciclo raízes, pá e investida. Testar esquiva, dano, colisão, aviso e recuperação de cada golpe. |
| R08 | P0 | Usar E perto do Guardião ou do Coveiro caído para abrir o selo. Pausar a simulação durante a escolha. | Disparos, inimigos e temporizadores ficam parados no desafio. Acertar conclui a área. Errar restaura o chefe mais forte e permite outra tentativa após a revanche. Confirmar conclusão ao selar o Coveiro e oferecer retorno ao menu. |
| R09 | P0 | Continuar no início da onda ou do encontro salvo, inclusive no cemitério. | Fechar e reabrir em uma onda, na entrada do cemitério e após falhar no selo do Coveiro. Restaurar área, etapa, arma, vida e bônus. Testar save antigo da clareira e derrota com retorno ao menu nas duas arenas. |
| R10 | P0 | Entregar sprites integrados, efeitos legíveis e HUD suficiente para entender o combate. | Esqueleto, mago e chefes mostram claramente direção e estado. Avisos indicam onde o golpe vai atingir, durante tempo suficiente para reagir. Ataques conectam no quadro previsto e o jogador entende onda, vida, selo e saída. |
| R11 | P1 | Melhorar comportamento perto de obstáculos e acabamento da apresentação. | Magos buscam uma linha de tiro quando bloqueados. Resolver casos reproduzíveis de perseguição presa, usar ícones finais nas escolhas e revisar impactos, transições e feedback de dano. |

**Decisões de design para fechar na semana 1.** A proposta é manter a travessia automática pela abertura inferior após o selo e mostrar um aviso visível antes de liberar a passagem. Na entrada do cemitério, salvar o começo do encontro com os guardas. Após derrubá-los, salvar o começo da luta do Coveiro. O erro no selo salva a revanche. Arthur e Berenguer registram os limites de fortalecimento, tempos de aviso, alcance e dano após a primeira partida em dupla. O uso de animações de queda ou de uma pose temporária para cada chefe é decidido junto à equipe visual no primeiro teste de escala.

## 4. Contrato entre programação e visual

O fluxo segue `menu → escolha da arma → clareira → selo do Guardião → cemitério → selo do Coveiro → conclusão`. `game.c` orquestra a área e a entrada do jogador. `encounter.c` aplica as regras de progressão e selo. Cada área conserva suas configurações de inimigos e colisão. O salvamento local recebe a identificação da área e o ponto de retorno, com leitura compatível com o formato anterior.

| Pacote visual | Responsável proposto | Entrega mínima para programação |
| --- | --- | --- |
| Esqueletos comuns | Marcos | Espera, caminhada, ataque, dano e queda. Quatro direções com recorte e pivô dos pés consistentes. Começar por uma direção piloto. |
| Magos e projétil | Silvio | Espera, caminhada, conjuração, dano e queda. Quadro de lançamento marcado, projétil e impacto em folhas separadas. |
| Coveiro | Lira | Revisar as três sequências existentes, incluindo preparação e recuperação. Completar espera, dano e queda onde faltarem. Conferir a pá erguida, margens e proporção entre quadros. |
| Guardião e efeitos de combate | JP | Revisar a animação de raízes e completar a leitura de dano e queda. Entregar avisos de área e direção para os chefes e conferir efeito de impacto. |
| Cenários, HUD e UI | Caua | Marcar bases de colisão e saída sobre cópia do cenário, preparar elementos de onda, vida do chefe, selo e conclusão. Validar legibilidade na resolução da demonstração. |

Cada entrega inclui o **PNG com transparência**, o original em `design/` e a versão de execução em `assets/`, além de uma ficha com nome, direções, número de quadros, tamanho da célula, pivô, duração e quadro de disparo ou impacto. Usar a ordem do jogo **baixo, cima, esquerda, direita** ou informar explicitamente a ordem diferente para conversão. Separar efeitos do corpo quando eles tiverem posição ou duração próprias. A caixa de colisão é definida pela programação a partir da referência visual. A arte é aprovada com fundo de contraste e no tamanho real em jogo, incluindo rosto, pescoço, pés e pontas de arma. Cada pacote usa o contrato do atlas atual ou recebe atualização de código junto com a troca de formato.

## 5. Um sprint de três semanas

| Semana | Arthur | Berenguer | Visual | Marco de aceite |
| --- | --- | --- | --- | --- |
| **1 — Física e contrato** | Reproduzir a travessia, fechar a saída até o selo, marcar pontos de surgimento e testar colisores da clareira. | Revisar a simulação dos inimigos, colisões do cemitério e os três ataques do Coveiro. Definir janela de aviso e quadro de impacto com a arte. | Marcos e Silvio entregam uma direção piloto de cada inimigo. Lira revisa os recortes do Coveiro. JP testa um aviso de golpe. Caua entrega a marcação de colisões e a tela de selo para revisão. | Clareira termina pelo fluxo normal e leva ao cemitério. Um esqueleto e um mago com arte piloto funcionam no jogo. A equipe aprova escala, pivô e ordem das direções. |
| **2 — Integração de combate e progressão** | Integrar conclusão e derrota das duas áreas, passagem pós-selo, checkpoints do cemitério e compatibilidade com saves antigos. | Integrar animações e hitboxes dos inimigos, aplicar janelas de ataque e ligar o Coveiro ao sistema de selos e revanche. | Marcos e Silvio fecham folhas direcionais. Lira entrega os três golpes e a pose de queda. JP fecha efeitos de aviso, impacto e raízes. Caua finaliza elementos do HUD, selo e saída. | Uma partida inteira, da escolha da arma ao selo do Coveiro, funciona sem F7. Continuar retoma a área correta. Falha e acerto no selo têm desfechos verificáveis. |
| **3 — Teste, correções e demonstração** | Executar regressão de transição, controles, salva e carrega, e preparar o executável com a pasta de assets. | Corrigir travamentos, contagem de inimigos, dano fora da janela e balanceamento observado. Rodar o teste de integração das duas áreas. | Todos revisam seus sprites dentro do jogo. Marcos e Silvio corrigem passos e disparos. Lira e JP revisam as três janelas visuais do Coveiro e as raízes do Guardião. Caua verifica legibilidade, colisores e telas de resultado. | Rodar a demonstração do início ao fim com uma arma. Exercitar as outras duas separadamente. Conferir retorno do checkpoint e entregar uma versão que outra pessoa da equipe consiga abrir e jogar. |

O primeiro teste de combate depende das folhas piloto, não das folhas completas. Programação pode usar as imagens existentes enquanto a arte fecha os quadros. A troca para os arquivos finais entra na segunda semana. Se houver atraso, manter a arte que já funciona e priorizar as poses e efeitos que o jogador usa para decidir quando atacar ou esquivar.

## 6. Validação e contingência

- **A cada integração:** executar `cmake --build build --config Release` e `ctest --test-dir build --output-on-failure`. Aproveitar os testes existentes para acrescentar casos que cubram R04, R06, R08 e R09.
- **Teste manual em dupla:** uma pessoa joga, outra registra área, arma, estado, passos de reprodução e captura. Verificar a partida completa, incluindo derrota, erro e acerto no selo. Usar F3 para colisão e F7 apenas para reproduzir problemas do cemitério.
- **Aceite visual:** revisar as folhas nas quatro direções com fundo claro e escuro e no mapa. Pausar no primeiro quadro ativo do golpe e comparar o aviso e o efeito com a área real de dano. Registrar o nome do asset e o estado quando houver corte, cintilação ou diferença de escala.
- **Pronto para entregar:** todos os P0 passam, os testes automatizados passam, o pacote inclui executável e assets, e outra pessoa da equipe consegue jogar a demonstração a partir de um save novo.
- **Contingência:** manter a última versão jogável semanal. Se um atlas novo quebrar a leitura, voltar o asset e sua configuração em conjunto. Se uma extensão não fechar a tempo, retirar primeiro o retorno da pá e os P1. Testar a migração do save em uma cópia antes de adotar o novo formato na demonstração.

**Pontos ainda abertos para a equipe:** referência de máquina e resolução da demonstração, quantidade de horas de cada pessoa, calibragem de dificuldade após a primeira partida completa e disponibilidade de arte de queda final para os chefes. Essas decisões entram na reunião de início e no aceite semanal sem mudar o prazo de três semanas.
