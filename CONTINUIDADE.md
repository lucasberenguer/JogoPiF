# Jogo PIF — continuidade entre computadores e chats

Atualizado em 18/11/2026. Leia junto do código e de `git status` antes de retomar.

## Retomada rápida

Projeto universitário em C99 + raylib 5.5, aprovado pelo professor. Equipe de quatro pessoas, prazo apertado. **Roguelike** 2D top-down em pixel art sombrio (ao morrer, perde tudo e reinicia). O usuário participa de game design, arte e código. Clareira e cemitério estão jogáveis e foram considerados bem nivelados pelo usuário.

Repositório: https://github.com/lucasberenguer/JogoPiF.git — branch `main`.
Última entrega de gameplay publicada antes deste documento: `45c550b` (polimento do cemitério, sprites, apresentações de área e modo de teste).

**Estado atual:** Clareira, Cemitério, Vila Lascada e exploração da Capela estão implementados; conferir o código para a etapa de saída da Capela. A entrega atual acrescenta a libertação do Guardião, a investigação do altar e a passagem cinematográfica ao Cemitério.

No novo chat: ler `AGENTS.md` e `CONTINUIDADE.md`, conferir o Git e retomar a partir do estado implementado abaixo.

## Progressão confirmada pelo usuário

| Área | Estrutura e função |
| --- | --- |
| Clareira do Selo | Tutorial, quatro ondas e miniboss; primeiro contato com o selo. |
| Cemitério das Raízes | Quatro ondas e Coveiro; corrupção dos mortos. |
| Vila Lascada | Consequências para os habitantes; ondas e miniboss, seguindo a base dos primeiros mapas. |
| Capela do Anjo | Exploração jogável sem combate até o encontro com o anjo e a revelação sobre o necromante; desmoronamento leva ao portal. |
| Inferno Inicial | Mapa simples arruinado, fogo e inimigos demoníacos; duas ondas e miniboss com cutscene. |
| Palácio do Necromante | Combate em ondas, última defesa antes do chefe. Quantidade ainda pendente; três ondas sem miniboss foi apenas proposta. |
| Topo do Palácio | Arena da batalha final, selo do necromante e desfecho. |

O Pátio dos Guardiões foi retirado da sequência. Cutscene de descoberta do palácio entre inferno e palácio: proposta de reaproveitar a composição da cena do menu, com os inimigos de pé.

## Narrativa e mecânicas de design já discutidas

- Necromante é um anjo caído e irmão celestial do anjo guardião. Revelações ambientais conduzem ao encontro na capela; preservar a história compacta em dois atos.
- Primeiro contato misterioso num selo antigo da floresta: "A morte não é a maldição. O que a desafia é."
- Combate em tempo real, ataques básicos, esquiva e desafios lógicos de selamento. Ultimate: acerto lógico amplifica dano; erro produz dano base com chance de esquiva.
- Selo errado permite que inimigo ressuscite corrompido pelo necromante.
- Anjo: poder uma vez por run, protege até três selamentos com vida baixa; se não usado antes, pode salvar o selo final. Falha final sem essa proteção encerra a run.
- Estes itens são contexto de design, **não uma declaração de que todos já estão implementados**. Verificar cada sistema no código antes de planejar alterações.
- Ideia de a capela estar sobre um selo rompido pelo necromante foi proposta do assistente; causa exata da queda ainda não confirmada.

## Estado implementado

- Clareira e cemitério: ondas de 7, 11, 15 e 19 inimigos; quinta onda com miniboss. Para avançar ao cemitério, sele o Guardião da Clareira, conclua ou pule a libertação e pressione **E** junto ao altar ciano; a segunda parte cinematográfica conclui o transporte. A saída inferior e o atalho do menu foram desativados. **Continuar Jogo** retoma o mapa de um checkpoint existente. A saída do cemitério fica à direita.
- As colisões dos dois mapas seguem revisões desenhadas pelo usuário, com tratamento de deslizamento para reduzir agarramento nas bordas. O protagonista inicia no centro da clareira. Game Over oferece uma nova tentativa na clareira.
- Durante a partida, **Esc** pausa e retoma. A tela oferece Continuar Jogo, Configurações, Controles e Voltar ao Menu. Os botões seguem o estilo do menu inicial e o cenário anima a lua, as bandeiras e o rio.
- Revisão visual da pausa: a moldura tem o quadriculado removido antes da filtragem, o foco é aplicado na própria arte dos botões e os painéis usam o mesmo estilo do menu inicial. As folhas oscilam em ritmos diferentes, a névoa deriva em duas camadas e o pulso lunar ilumina o halo, a névoa e os reflexos do rio.
- O shader da pausa aceita tanto a moldura RGB enviada por builds antigos quanto a moldura com alfa pré-multiplicado do build atual. Isso evita que o quadriculado branco apareça ao carregar os assets do repositório com um executável antigo.
- Tipografia compartilhada em `src/game_text.c`: UnifrakturMaguntia nos títulos e botões, Crimson Text Semibold no HUD, nas descrições, nos textos de selamento e nas legendas da HQ. O menu, a pausa e a seleção de armas desenham os rótulos pelas fontes do jogo sobre as artes existentes. Os arquivos de fonte e as licenças SIL OFL ficam em `assets/fonts/` e são copiados com os demais assets.
- Ajuste de legibilidade: texto corrido 10% maior, espaçamento um pouco mais aberto na fonte gótica e Crimson Text nos botões secundários de configurações e controles. O HUD tem números maiores e contorno escuro. As cartas usam descrições mais curtas, e as legendas e o aviso de continuar têm mais contraste.
- A katana mira no cursor no momento do clique. O arco visual e o dano usam a direção exata do golpe e o alcance base de 105 unidades. A pose usa a direção cardinal mais próxima.
- A mira da arma de fogo usa os perfis da folha de espera na ordem direita e esquerda. Caminhada e disparo usam a ordem esquerda e direita.
- O inimigo corpo a corpo comum da Clareira tem novas poses nas quatro direções, com cinco quadros de espera e oito de caminhada e ataque. O ataque à esquerda espelha a sequência direita para manter a orientação durante todo o golpe. O dano coincide com o quinto quadro, e o fundo vinho da folha de ataque é filtrado no carregamento.
- O arqueiro da Clareira usa quatro quadros de espera e oito de caminhada e ataque por direção. A flecha sai no quadro de soltura da corda, aos 0,32 s do ataque. O filtro de fundo vinho é aplicado às três folhas durante o carregamento.
- O especial corpo a corpo da Clareira usa a armadura vermelha, com uma pose de espera e quatro quadros de caminhada por direção. A última folha de ataque enviada é usada na investida, com o corte principal aos 0,5 s. O selo vermelho aparece acima de especiais e chefes derrubados quando já podem ser selados, na Clareira e no Cemitério.
- Vida de inimigos do cemitério: +10% em relação à base da clareira, arredondada para cima. Guardião 2.200 HP; Coveiro 2.420 HP.
- Direções de esqueletos corrigidas com frente/costas e espelhamento lateral. Magos usam linhas direcionais corretas; fogo verde tem animação de voo/impacto e colisão ajustada. Esquiva e invulnerabilidade ainda impedem dano intencionalmente.
- Coveiro possui raízes, arremesso da pá e investida; segunda fase acrescenta raízes laterais, três pás e segunda investida com novo aviso. Novos atlas evitam recortes de pés/cabeça.
- Caminhada e arremesso do Coveiro: 12 quadros por direção. Caminhada 18 quadros/s, arremesso com preparação até 0,8 s e recuperação até 1,65 s; movimento corporal e pequeno avanço nos golpes. Após teste do usuário, deslocamento corporal foi reduzido 8% (fator 0,92), incluindo investidas. Velocidade das pás preservada.
- Pás: ida 620/740 unidades/s nas fases 1/2, retorno 660/760, raio de contato 43; oito quadros de giro, vento e poeira. Um acerto por trajeto de ida/volta; movimento subdividido para evitar atravessar colisões.
- Ao entrar numa área: apresentação **ENTRANDO EM + nome**, 2,8 s, seguida da apresentação da onda. Combate pausado durante apresentações.
- Os anúncios de onda usam Crimson Text Semibold, com número em destaque na faixa escura. O anúncio de miniboss também usa a fonte de leitura no número da onda e no nome do chefe, com espaço para o retrato em janelas menores.
- Depois da apresentação do Guardião da Clareira, ele emerge do chão em 23 quadros, ao longo de 3 s. A sequência pula uma pose em que ele afundava durante a subida. Durante a entrada, fica parado e invulnerável. O combate começa após a animação.
- Ao perder toda a vida na segunda fase, o Guardião cai em oito quadros na direção em que estava voltado. A queda dura 0,85 s e a última pose fica no chão até o selamento. O teste de encontro cobre a queda, a abertura do selo e a revanche.
- A abertura em HQ toca uma vez por sessão, após a confirmação da arma em uma nova partida. Usa oito ilustrações em seis momentos narrativos e dura cerca de 41 s. O quarto momento, Juramento, inclui uma ilustração vertical no painel lateral. **Espaço** ou **Enter** avança e reposiciona a trilha, e **Esc** segue para a Clareira. A transição reutiliza a apresentação de área com o subtítulo **A Terra dos que Não Descansam**. Checkpoints e novas tentativas após Game Over seguem direto ao jogo. A trilha foi integrada. Os WAVs de efeitos narrativos continuam opcionais.
- O Guardião da Clareira tem uma cutscene própria antes do combate, após a recompensa da quarta onda. São 16 ilustrações fornecidas pelo usuário, organizadas originalmente em 24 enquadramentos e seis atos, incluindo o flashback do protetor da floresta e as nove falas do roteiro. `assets/cutscenes/boss_clareira/data/sequencia.ini` controla ordem, duração, câmera, transições, efeitos, falas e áudio. As imagens completas recebem pan/zoom discretos, tremor amortecido, folhas, poeira, névoa e pulsação do selo.
- Narração cinematográfica do Guardião implementada em 18/11: 17 textos exatos do novo roteiro, separados das nove falas manuais. A sequência agora possui 30 eventos tipados (`narration`, `dialogue`, `cinematic`, `effect`, `transition`), usando as mesmas 16 artes e preservando os 24 enquadramentos anteriores com seis permanências adicionais. Legendas inferiores em marfim, fonte Crimson, sombra, gradiente, até duas linhas e fade de 0,5 s; tempos e pausas ficam no INI. A configuração rejeita sobreposição de narração, fala, nome ou título. Os efeitos de impacto continuam disparando uma única vez nos momentos configurados.
- O novo pedido cita 23 imagens; existem apenas `quadro_01.png` a `quadro_16.png`. As sete demais não estão disponíveis no repositório. A correspondência entre os 23 momentos narrativos e as artes reais está em `assets/cutscenes/boss_clareira/data/LEIA-ME.md`. Nenhuma imagem foi criada ou substituída. O anexo termina em “Pausa entre legend”; foi adotado um intervalo editável de 0,35 s ao final de cada legenda. Narração por voz continua pendente de gravação; esta entrega implementa as legendas pedidas.
- Na cutscene do Guardião, Espaço/Enter completa a fala e um novo toque avança. Esc pula, P pausa. Controle usa A, B e Start nas posições equivalentes do raylib. Cenas exclusivamente visuais avançam por tempo. A identificação final é **GUARDIÃO DAS RAÍZES — O PROTETOR APRISIONADO**. Ao concluir ou pular, a arena faz fade e o chefe mantém sua entrada de três segundos antes de atacar. O Cemitério mantém seu fluxo anterior.
- **Pós-boss do Guardião (18/11):** o selo correto confirma a purificação, salva o estado e inicia a libertação. São duas partes em `assets/cutscenes/guardian_aftermath/data/libertacao.ini` (16 eventos/oito falas) e `passagem.ini` (sete eventos/duas falas), reutilizando o renderizador de `guardian_cutscene.c`. Movimentação, armas e interação ficam bloqueadas enquanto a tela cinematográfica está ativa. Errar o selo mantém a revanche e não inicia a cena.
- Após a primeira parte, o objetivo é **Investigue o altar da clareira**, com controle livre. Próximo do altar ciano aparece **E: Interagir com o Altar Antigo**. O comando inicia a segunda parte e bloqueia novas ativações. Pular a primeira parte libera o altar e posiciona num ponto livre próximo; pular a segunda conclui o transporte para a onda 1 do Cemitério. A cartela final é **CAPÍTULO II — O Cemitério dos Esquecidos — Onde a morte perdeu seu silêncio**, composta sobre preto com as fontes do jogo; o nome técnico/antigo do mapa permanece no sistema de ondas.
- **Checkpoint atual `PIF-RUN-7`:** acrescenta `guardianAftermath` (não purificado, libertação pendente, altar pronto, passagem pendente, passagem concluída). A confirmação do selo e o início da passagem são salvos antes das cenas, permitindo retomá-las após fechar o jogo. A conclusão salva o novo estado; completar novamente não repete a transição. Saves anteriores já na Clareira concluída recebem altar pronto, e mapas posteriores recebem passagem concluída. A prática preserva o checkpoint da campanha.
- Os 11 anexos do pós-boss foram copiados sem edição para `guardian_aftermath/images/quadro_01.png` a `quadro_11.png`, com identidade SHA-256 conferida. O roteiro descreve 22 planos narrativos mais a cartela, mas há dez artes narrativas e uma cartela: closes e permanências reaproveitam as artes, com correspondência e sugestões de quadros exclusivos em `data/LEIA-ME.md`. A arte 11 permanece como referência; a cartela exibida usa texto do motor sobre preto. Não foram geradas imagens substitutas.
- Trilha `synthwave-nostalgic-take-1.mp3` do pós-boss integrada em `guardian_aftermath/audio/trilha.ogg`: fonte de 60,029 s, loop de 58,029 s com sobreposição de dois segundos. A passagem começa aos 40 s; não reinicia entre planos e baixa nas falas. A faixa não contém os 90 s segmentados descritos no roteiro; o loop acomoda leitura livre. Encerrar a primeira parte faz fade e permite exploração silenciosa até o altar. A música anterior é interrompida antes de abrir cada parte, e o gameplay retoma com fade no Cemitério.
- Efeitos discretos do pós-boss: luz/partículas ciano e douradas, partículas roxas enfraquecendo e uma luz seguindo até o altar. Seis WAVs: raízes CC0 já existentes e cinco sons procedurais de energia, runas, partículas, vento e sino. Fonte reproduzível em `tools/prepare_guardian_aftermath.py`; licenças e origem em `guardian_aftermath/audio/CREDITOS.md`. F1 inclui **Libertação do Guardião** e **Altar e passagem** em prática. Encerrar onda em um boss o derruba para resolver o selo, preservando a exigência de purificação.
- O formato anterior de checkpoint `PIF-RUN-6` acrescentou `guardianIntroSeen`, salvo quando a cutscene termina ou é pulada. Continuar e as revanches da mesma partida preservam a flag. Reiniciar uma tentativa completa reinicia a narrativa. Saves antigos no chefe da Clareira recebem a nova cena uma vez; mapas já concluídos e o Cemitério são migrados como vistos. Versões antigas de checkpoint continuam legíveis.
- A cutscene do Guardião usa a faixa enviada `synthwave-nostalgic-take-3.mp3`, convertida em `assets/cutscenes/boss_clareira/audio/trilha.ogg`. O loop de 58 segundos tem sobreposição de dois segundos entre fim e começo. Os eventos mantêm a mesma reprodução, com redução suave do volume nas falas e fade final. Os eventos opcionais, caminhos e conteúdo esperado estão documentados em `assets/cutscenes/boss_clareira/data/LEIA-ME.md`. O trecho final do anexo estava cortado no exemplo JSON; a configuração INI implementa todos os atos e falas completos recebidos.
- Os impactos da cutscene têm quatro efeitos integrados, combinados a partir de explosões graves e impactos de madeira/pedra dos pacotes CC0 Impact Sounds e Sci-fi Sounds, de Kenney. São `ruptura_solo.wav`, `raizes.wav`, `selo_furia.wav` e `impacto_raizes.wav`, com fontes e licenças em `audio/CREDITOS-SFX.md`. Os campos `audio_at` e `audio_gain` da sequência ajustam o instante e a intensidade de cada efeito. Pausa e F1 congelam os disparos pendentes; avançar ou pular encerra o efeito da cena anterior. O ganho usa o volume de efeitos das configurações. Os demais ambientes e a dublagem continuam pendentes.
- F1 abre as ferramentas de desenvolvimento no menu, na partida, na pausa e nas cutscenes. O painel permite escolher Clareira/Cemitério, ondas 1–4 ou boss, arma e upgrade 0–3, invulnerabilidade e colisões. Há acesso direto às duas fases dos bosses, à abertura, à cutscene do Guardião, ao selo do boss, à cura e ao encerramento de uma onda. Mouse ou setas/Enter operam o painel; F1/Esc fecha.
- Os cenários do painel usam `Encounter.practice`. Recompensas, revanches, morte e passagem pelo altar preservam esse modo e o checkpoint da campanha. A repetição de um teste após morrer mantém o mapa, a onda e os atributos do teste, incluindo a fase 2 quando selecionada. A abertura funciona como prévia e retorna ao painel. O botão Menu principal permite voltar e usar Continuar para retomar o checkpoint. Abrir o painel em cutscenes pausa os temporizadores e o áudio e preserva uma pausa que já estivesse ativa.
- Menu, ondas normais e minibosses têm faixas próprias em loop com sobreposição entre fim e começo. A troca de faixa tem fade e a música de combate baixa de volume nas pausas e apresentações. Configurações no menu inicial e na pausa oferecem volumes independentes de música e efeitos, salvos em `audio_pif.cfg` ao lado do executável.
- Ao concluir: mensagem de área limpa e depois **NOVA AREA DESBLOQUEADA + próximo nome**, 3,2 s. Atualmente o anúncio após o cemitério ainda diz "Vila Abandonada"; trocar para **Vila Lascada** ao integrar a vila. O anúncio não significa que a vila já seja acessível.
- Upgrades de armas expandidos para **4 níveis** (`weaponLevel` 0 a 4) em `src/encounter.c`, `src/menu.c`, `src/bible.c`, `src/gun.c` e `src/game.c`, mantendo dano +20% por nível e adicionando habilidades:
  - **Katana:** Nv. 1 alcance 130; Nv. 2 acertar golpe reduz cooldown do dash em 0,6 s; Nv. 3 arco ampliado e empurrão; Nv. 4 onda de choque vermelha a longa distância (cooldown 2,5 s).
  - **Bíblia Sagrada:** Nv. 1 5 livros e raio 102; Nv. 2 aceleração 1,65 s e cooldown 2,8 s; Nv. 3 6 livros e destruição de projéteis inimigos durante aceleração; Nv. 4 poça de água benta no chão (cooldown 3,5 s) causando dano contínuo.
  - **Arma de Fogo:** Nv. 1 cadência 0,14 s e projétil mais rápido; Nv. 2 disparo duplo; Nv. 3 disparo triplo; Nv. 4 duas pistolas simultâneas herdando todos os upgrades anteriores.
- Validação em `out/review` após merge com `a474bb4` (tipografia própria em `src/game_text.c`), upgrades de arma, arte final da Vila Lascada, sprites do Líder da Vila, horda de aldeões necróticos e botão **Teste** no menu: build concluído e 4/4 testes aprovados (`enemy_audit`, `map_connectivity`, `player_collision`, `clearing_encounter`).
- **Capela do Anjo (`map == 3`), Porão Secreto (`map == 4`) e Capela Invertida (`map == 5`):**
  - **Capela Humana (`map == 3`):** `assets/capela.png` e `assets/capela_collision.png` (`1672×941`), conectada pela Vila Lascada (`CanEnterChapel`) e botão **Capela** no menu. Falar (**E**) no Altar Central abre a descida pela porta superior esquerda (`CanEnterBasement`).
  - **Porão da Capela (`map == 4`):** `assets/capela_porao.png` e `assets/capela_porao_collision.png` (`1672×941`). Puzzle dedutivo conectado: **4 placas na parede norte** (`E`) dão pistas únicas para ativar os **4 pilares** (`V`/`F`), acendendo cada canal e quadrante do grande selo central; ao completar os 4, transporta para a **Capela Invertida** (`CanEnterInvertedChapel`).
  - **Capela Invertida (`map == 5`):** `assets/capela_invertida.png` (reaproveitando `assets/capela_collision.png`), contraparte carmesim com esporos animados, estátua do Guardião em ruínas, estátua do Necromante intacta e vestígios dourados no altar. Saída para os mapas do Inferno pendente para a próxima etapa.
- **Vila Lascada integrada como mapa funcional (`map == 2`):**
  - Assets `assets/vila.png` (paleta avermelhada/ferrugem mais quente diferenciada do cemitério) e `assets/vila_collision.png` (`1672×941`) + polígono caminhável em `src/clearing.c` (`InitVillage`, `ConfigureVillageBoundary`), com spawn na estrada esquerda e saída no arco superior.
  - Transição conectada pela saída à direita do Cemitério (`CanEnterVillage` em `src/encounter.c` e `src/game.c`) e suporte em checkpoints (`src/menu.c`).
  - **Líder da Vila (`500 HP`, 100% humano):** duelo direto ao entrar na Vila usando os spritesheets dedicados `assets/lider_vila_walk.png`, `assets/lider_vila_ataque.png` e `assets/lider_vila_investida.png` (`src/village_enemy.c`), com golpe de machado, investida e pulso de selo pálido na fase 2 (`< 50% HP`), caindo como humano ao ser derrotado.
  - **Carta de morte (`ENCOUNTER_LETTER`) e horda necrótica:** ao derrotar o Líder, exibe o modal com seu Último Registro; ao fechar, inicia `StartVillageHorde` com `assets/aldeoes_necroticos.png` (cabelo castanho escuro, 19 iniciais + 12 emergindo da terra ao longo do tempo). Ao eliminar toda a horda, libera `VILA LASCADA LIMPA` / `Capela do Anjo` e a saída pelo arco ao topo.

- **Item de cura (drop de vida dos inimigos):** substituído o desenho de círculos (`DrawCircleV`) por um coração procedural (`DrawHealthPickup` em `src/enemy.c`, compartilhado por Clareira, Cemitério e Vila Lascada em `src/cemetery_enemy.c` e `src/village_enemy.c`), mantendo as mesmas cores (`Fade(GOLD, 0.65f)` na borda externa e `RED` no interior) e a mesma proporção/tamanho em tela.
- **Caixa de diálogo nas cutscenes (`src/guardian_cutscene.c`):** as instruções de avançar/pular/pausar nos momentos de fala (`GUARDIAN_DIALOGUE` — introdução do Guardião, Libertação e Passagem) foram movidas da barra inferior externa para dentro da própria caixa de fala (alinhadas no canto inferior direito do quadro de diálogo).
- **Selo no chão para bosses derrotados (`src/enemy.c`, `src/cemetery_enemy.c`):** ao derrotar um boss que exige selamento (`ENEMY_FALLEN` na Clareira e no Cemitério), o marcador `assets/inimigo_selo.png` não fica mais pequeno flutuando acima da cabeça; em vez disso, é desenhado grande e semitransparente (`DrawBossGroundSeal`, `186×148`, opacidade `0.52`) diretamente no chão ao redor e sob o corpo derrotado no centro. Inimigos especiais comuns continuam com o ícone menor acima deles.
- **Correção de Nova Partida após visitar a Capela (`src/game.c`, `src/dev_tools.c`):** ao iniciar uma nova partida (`MENU_START` → seleção de arma) estando com `clearing.chapel`, `clearing.basement` ou `clearing.invertedChapel` carregado na memória, `game.c` agora descarrega o mapa da Capela e recarrega a Clareira (`InitClearing(&next)`) antes de chamar `BeginEncounter`, impedindo que `BeginEncounter` sobrescreva `progress.map` de volta para a Capela.

## Decisões confirmadas para o futuro (ainda não implementadas)

- **Roguelike puro:** se morrer, perde tudo e reinicia a run.
- **Escolha de arma integrada à narrativa:** substituir a seleção fria no menu por uma mini cutscene com o Anjo na entrada da Clareira oferecendo as armas sagradas, reformulando as armas para armas de época (ex.: Espada Sagrada, Arco Sagrado, Bíblia Sagrada). Polimento das animações de habilidades fica para depois.
- **Escopo enxuto dos mapas restantes:** Capela (sem combate), Inferno Inicial (2 ondas + miniboss), Palácio (2–3 ondas sem miniboss, com templários da Ordem), Topo do Palácio (arena direta do Necromante).

## Arquivos úteis

- `src/game.c`: loop, telas, entrada, transições e prática.
- `src/enemy.c` / `src/enemy.h`: ondas, inimigos, vida, tiros e carregamento de assets.
- `src/cemetery_enemy.c`: comportamento, animações e VFX do Coveiro; desenho dos inimigos do cemitério.
- `src/clearing.c`: mapas e colisões.
- `src/encounter.c`, `src/presentation.c`: progressão, selos, recompensas e apresentações.
- `src/cutscene.c` e `assets/cutscenes/intro/`: duração, legendas, composição, ilustrações e caminhos de áudio da abertura.
- `src/guardian_cutscene.c`, `src/guardian_cutscene.h` e `assets/cutscenes/boss_clareira/`: cutscene configurável do Guardião, ilustrações originais, diálogos e eventos de áudio.
- `src/game_audio.c` e `assets/audio/`: reprodução das faixas do jogo, volumes e persistência.
- `src/dev_tools.c` e `src/dev_tools.h`: painel F1, seletores e preparação dos encontros de prática. `src/game.c` integra ações, prévias, pausa e proteção do checkpoint.
- `assets/coveiro_walk_v4.png`, `coveiro_throw_v4.png`, `coveiro_slam_v3.png`, `coveiro_charge_v3.png`, `coveiro_pa_vento_v4.png`: assets atuais do chefe.
- `design/coveiro_v4_notas.md`: especificações e notas das artes geradas com imagegen.
- `README.md` contém partes históricas do protótipo, inclusive informações antigas sobre inimigos/checkpoints; conferir implementação e este documento.

## Proposta visual da Vila Lascada / Corrompida e chefe aprovado

- **Cenário (em refinamento visual):** Vila medieval abandonada e arruinada com casas variadas nas bordas, praça central ampla para combate, poço, fornalha/ferraria, bancas, carroça, barricadas e selos sagrados nas portas. Entrada à esquerda e saída no alto por arco com runas rumo à capela. Última referência em `design/vila_lascada_conceito_v10.jpg` (campo central amplo, parte superior arruinada e árvore destruída no canto superior direito).
- **Miniboss aprovado — O Líder da Vila (humano, sem transformação):** Único sobrevivente do massacre da vila pelo exército do Necromante; enterrou pessoalmente seu povo no Cemitério das Raízes e viu os mortos retornarem. Reconhece o poder celestial no protagonista, confunde com a origem do Necromante e luta por vingança direcionada à pessoa errada. É **completamente humano**, sem transformação monstruosa antes, durante ou após a luta. Após a derrota, o jogador encontra um **último registro** escrito por ele revelando sua culpa por ter sobrevivido (texto definitivo ainda a desenvolver). Substitui a proposta antiga de "Ferreiro Corrompido". Conceito visual em `design/lider_vila_conceito_v2.jpg`.

## Preparar outro notebook Windows

Instalar Git, VS Code e ferramentas C/CMake/Ninja (na máquina anterior: MSYS2 UCRT64 em `C:\msys64\ucrt64\bin`). Clonar o repositório e adicioná-lo como projeto no aplicativo. Não copiar caches CMake entre máquinas; recompilar.

```powershell
git clone https://github.com/lucasberenguer/JogoPiF.git
cd JogoPiF
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
cmake -S . -B out/review -G Ninja -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe
cmake --build out/review
ctest --test-dir out/review --output-on-failure
Push-Location out/review
.\JogoPiF.exe
Pop-Location
```

Ajustar caminhos se a instalação diferir. Primeira configuração precisa de internet para baixar raylib. Diretório `out/review` é a versão local de testes; `build/JogoPiF.exe` pode ser antigo. Os assets são copiados pelo build.
Na inicialização, o executável usa sua própria pasta quando ela contém `assets/`. Após corrigir o caso em que a abertura era pulada pelo executável antigo, `build` e `out/review` foram recompilados com os sete quadros. A verificação iniciou o jogo de outra pasta e confirmou a abertura após selecionar a arma com Enter e com clique.

Validação da integração em `out/review`: build concluído e quatro testes aprovados (`enemy_audit`, `map_connectivity`, `player_collision`, `clearing_encounter`). Uma verificação adicional em janela raylib carregou as artes e o shader da pausa e confirmou a navegação por teclado e as ações de retomar e voltar ao menu. A verificação adicional exercitou o módulo do menu diretamente.

A revisão visual passou pelos mesmos quatro testes e pela navegação de menu. Capturas em 1280×720 e 800×700 foram inspecionadas. A comparação entre quadros confirmou movimento nas bandeiras, folhas, névoa, lua e rio e estabilidade dos botões. A pausa manteve 60 FPS durante uma medição de 180 quadros em 1920×1080, renderizados em textura na Intel UHD local.

A correção do branco foi conferida em capturas da pausa com o renderizador antigo e o atual. Os quatro testes passaram após essa correção e a mudança da mira da katana.

O teste de encontro também confere que a queda do Guardião ainda exige o selo, que a apresentação bloqueia a interação e que, depois dela, apenas a interação junto ao altar libera a passagem. A captura do altar liberado fica em `out/review/audit/clearing_altar_open.png`.

A abertura foi conferida em 1280×720 e 800×700. O fluxo de jogo foi exercitado com entradas simuladas até a primeira onda e a repetição após Game Over, tanto no avanço automático quanto ao pular com Esc. As capturas ficam em `out/review/audit/intro_*.png`.

Áudio validado com os quatro testes do projeto e um teste adicional em janela raylib: repetição das três faixas, avanço da trilha da abertura, alteração por teclado e persistência dos dois volumes. A captura de Configurações fica em `out/review/audit/pausa_config_audio.png`.

Tipografia validada com os quatro testes de `out/review`, verificação de acentos nas duas fontes e capturas em 1280×720 e 800×450 do menu, pausa, configurações, controles, seleção de armas, abertura, HUD e créditos. As capturas locais ficam em `out/review/audit/typography_*.png`. Os testes de encontro também renderizam as apresentações de área, recompensas e HUD do chefe com as fontes do jogo.

O ajuste de legibilidade passou pelos mesmos quatro testes. A revisão comparou o menu, a pausa, as configurações, as cartas, a abertura e o HUD em capturas de 1280×720 e 800×450. As capturas atualizadas ficam em `out/review/audit/legibility_*.png`.

Os anúncios de onda normal e miniboss foram renderizados durante a entrada e após a faixa se estabilizar, em 1280×720 e 800×450. Capturas em `out/review/audit/wave_text_*.png`.

Na cutscene do Guardião, `guardian_cutscene_test` confere as 16 imagens, os 30 eventos, a ordem e o conteúdo das 17 narrações e nove falas, completar/avançar, pausa, pular e liberação de recursos. Também compara pixels entre instantes diferentes para verificar movimento real. Capturas em `out/review/audit/guardian_cinematic_*.png`, em 1280×720 e 800×450. O teste de encontro verifica o bloqueio do combate, a persistência da flag, os checkpoints antigos e a ausência de uma segunda apresentação após a cena.

Build de 17/11 concluído em `out/review` e os cinco testes passaram. Uma auditoria local adicional executou o loop real de `game.c` com ações simuladas de Continuar, concluir e pular a cutscene, chegando ao Guardião ativo nos dois casos. A câmera e os diálogos foram inspecionados nas capturas. Os comandos de controle estão mapeados; não houve teste com controle físico.

Na entrega da soundtrack e do painel F1, os cinco testes passaram em `out/review`. O teste de encontro verifica as quatro ondas nos dois mapas, as duas fases dos bosses, o selo e a preservação do checkpoint na prática. O teste da cutscene confere a faixa de 58 segundos, a continuidade entre todas as cenas e a pausa/reprodução do stream. O painel foi inspecionado em 1280×720 e 800×450, em `out/review/audit/dev_tools_*.png`. Uma auditoria local do loop real testou F1, onda no Cemitério, boss na fase 2, repetição após morte, cura, encerramento da onda, pausa/pulo da cutscene do Guardião e prévia da abertura. O checkpoint isolado da auditoria permaneceu idêntico.

Na integração dos efeitos de raízes, os cinco testes passaram. `guardian_cutscene_test` confere o carregamento dos quatro WAVs, o atraso do disparo, pausa antes e durante o efeito, retomada e parada ao concluir ou pular. Os arquivos foram verificados como PCM estéreo de 16 bits/44,1 kHz, com duração entre 2 e 4 segundos e pico abaixo de 0 dBFS.

Na entrega da narração de 18/11, o build em `out/review` e os cinco testes passaram. Após o ajuste final de tamanho e quebra equilibrada das legendas, o teste da cutscene foi reexecutado. Ele percorre a sequência até o encerramento automático, verifica os 17 textos exatos na ordem, as nove falas manuais, exclusividade, fades, pausa, pulo durante fala/narração e parada do áudio. Todas as legendas foram medidas em 1280×720 e 800×450: até duas linhas, sem perda de palavras. Capturas da narração longa e dos diálogos foram inspecionadas nessas resoluções em `out/review/audit/guardian_cinematic_*.png`. Sem teste com controle físico ou dublagem.

Na entrega do pós-boss de 18/11, build em `out/review` concluído e seis testes aprovados (6/6): `enemy_audit`, `map_connectivity`, `player_collision`, `clearing_encounter`, `guardian_cutscene` e `guardian_story_flow`. Os testes ampliados verificam selo errado/correto, migração e retomada dos estados pendentes, prevenção de passagem dupla, prática, dez falas exatas, continuidade musical e parada dos sons. O teste novo executa o loop real de `game.c` com entradas simuladas até o cemitério, concluindo e pulando ambas as partes, com checkpoint isolado em `out/review/audit/story_flow`. Capturas dos diálogos, da cartela e do painel F1 foram inspecionadas em 1280×720 e 800×450. Não houve teste com controle físico. Dublagem, ilustrações exclusivas para cada um dos planos e uma composição de 90 s segmentada por atos continuam como refinamentos de assets, documentados no LEIA-ME.

## O que é salvo e o que precisa de backup

- Código, assets e documentos rastreados chegam ao outro notebook por clone/pull depois do push.
- Instruções em `AGENTS.md` pedem atualização deste arquivo nas próximas entregas. Não existe serviço em segundo plano salvando decisões da conversa: o agente precisa atualizar o arquivo, e publicação continua dependendo de autorização.
- Histórico do aplicativo não é copiado por Git; não há garantia aqui de sincronização desta conversa entre computadores.
- Há muitos rascunhos antigos não rastreados em `design/`, além de arquivos locais fora do repositório. Não foram incluídos no push de gameplay. Preservar a pasta inteira via backup/OneDrive se forem desejados no novo notebook.
- Builds, `.vscode` e outros arquivos ignorados não são enviados pelo Git. Checkpoint `partida_pif.sav` fica ao lado do executável e precisa de cópia separada para levar a partida atual.
- Confirmar manualmente que o OneDrive terminou de sincronizar; este trabalho não verificou nem configurou o serviço.

- **20/11/2026 — Pré-boss do Coveiro:** 43 s com aparição distante, flashback de Aldric, retorno ao presente, olhos roxos e cartela. Inicia após a recompensa da quarta onda do Cemitério. F1 permite testar pelo cenário Cemitério, onda 5. A sequência, os nove anexos, a trilha enviada e os créditos dos efeitos ficam em `assets/cutscenes/coveiro/`. Os acessos diretos às fases e checkpoints na quinta onda entram no combate. Planos exclusivos e áudio dedicado estão descritos no LEIA-ME da sequência.
- Validação do Coveiro em `out/review`: build concluído. Os seis testes passaram considerando a bateria inicial e a reexecução de `clearing_encounter` e `guardian_cutscene` com as expectativas do novo fluxo. Foram verificados bloqueio e liberação da barra, carregamento das artes e áudios, duração, conclusão automática e pulo. Sem revisão manual da mixagem.
