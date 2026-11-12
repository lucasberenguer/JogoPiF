# Jogo PIF — continuidade entre computadores e chats

Atualizado em 11/11/2026. Leia junto do código e de `git status` antes de retomar.

## Retomada rápida

Projeto universitário em C99 + raylib 5.5, aprovado pelo professor. Equipe de quatro pessoas, prazo apertado. Roguelite 2D top-down em pixel art sombrio. O usuário participa de game design, arte e código. Clareira e cemitério estão jogáveis e foram considerados bem nivelados pelo usuário.

Repositório: https://github.com/lucasberenguer/JogoPiF.git — branch `main`.
Última entrega de gameplay publicada antes deste documento: `45c550b` (polimento do cemitério, sprites, apresentações de área e modo de teste).

**Próximo trabalho:** definir e produzir o design da **Vila Lascada**. Ainda não há mapa, inimigos ou miniboss da vila implementados. A proposta visual abaixo não foi aprovada em detalhe.

No novo chat: pedir ao agente para ler `AGENTS.md` e `CONTINUIDADE.md`, conferir o Git e continuar o design da Vila Lascada.

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

- Clareira e cemitério: ondas de 7, 11, 15 e 19 inimigos; quinta onda com miniboss. Para avançar ao cemitério, sele o Guardião da Clareira e pressione **E** junto ao altar iluminado. A saída inferior e o atalho do menu foram desativados. **Continuar Jogo** retoma o mapa de um checkpoint existente. A saída do cemitério fica à direita.
- As colisões dos dois mapas seguem revisões desenhadas pelo usuário, com tratamento de deslizamento para reduzir agarramento nas bordas. O protagonista inicia no centro da clareira. Game Over oferece uma nova tentativa na clareira.
- Durante a partida, **Esc** pausa e retoma. A tela oferece Continuar Jogo, Configurações, Controles e Voltar ao Menu. Os botões seguem o estilo do menu inicial e o cenário anima a lua, as bandeiras e o rio.
- Revisão visual da pausa: a moldura tem o quadriculado removido antes da filtragem, o foco é aplicado na própria arte dos botões e os painéis usam o mesmo estilo do menu inicial. As folhas oscilam em ritmos diferentes, a névoa deriva em duas camadas e o pulso lunar ilumina o halo, a névoa e os reflexos do rio.
- O shader da pausa aceita tanto a moldura RGB enviada por builds antigos quanto a moldura com alfa pré-multiplicado do build atual. Isso evita que o quadriculado branco apareça ao carregar os assets do repositório com um executável antigo.
- A katana mira no cursor no momento do clique. O arco visual e o dano usam a direção exata do golpe e o alcance base de 105 unidades. A pose usa a direção cardinal mais próxima.
- A mira da arma de fogo usa os perfis da folha de espera na ordem direita e esquerda. Caminhada e disparo usam a ordem esquerda e direita.
- O inimigo corpo a corpo comum da Clareira tem novas poses nas quatro direções, com cinco quadros de espera e oito de caminhada e ataque. O ataque à esquerda espelha a sequência direita para manter a orientação durante todo o golpe. O dano coincide com o quinto quadro, e o fundo vinho da folha de ataque é filtrado no carregamento.
- Vida de inimigos do cemitério: +10% em relação à base da clareira, arredondada para cima. Guardião 2.200 HP; Coveiro 2.420 HP.
- Direções de esqueletos corrigidas com frente/costas e espelhamento lateral. Magos usam linhas direcionais corretas; fogo verde tem animação de voo/impacto e colisão ajustada. Esquiva e invulnerabilidade ainda impedem dano intencionalmente.
- Coveiro possui raízes, arremesso da pá e investida; segunda fase acrescenta raízes laterais, três pás e segunda investida com novo aviso. Novos atlas evitam recortes de pés/cabeça.
- Caminhada e arremesso do Coveiro: 12 quadros por direção. Caminhada 18 quadros/s, arremesso com preparação até 0,8 s e recuperação até 1,65 s; movimento corporal e pequeno avanço nos golpes. Após teste do usuário, deslocamento corporal foi reduzido 8% (fator 0,92), incluindo investidas. Velocidade das pás preservada.
- Pás: ida 620/740 unidades/s nas fases 1/2, retorno 660/760, raio de contato 43; oito quadros de giro, vento e poeira. Um acerto por trajeto de ida/volta; movimento subdividido para evitar atravessar colisões.
- Ao entrar numa área: apresentação **ENTRANDO EM + nome**, 2,8 s, seguida da apresentação da onda. Combate pausado durante apresentações.
- Depois da apresentação do Guardião da Clareira, ele emerge do chão em 23 quadros, ao longo de 1,15 s. A sequência pula uma pose em que ele afundava durante a subida. Durante a entrada, fica parado e invulnerável. O combate começa após a animação.
- Ao perder toda a vida na segunda fase, o Guardião cai em oito quadros na direção em que estava voltado. A queda dura 0,85 s e a última pose fica no chão até o selamento. O teste de encontro cobre a queda, a abertura do selo e a revanche.
- A abertura em HQ toca uma vez por sessão, após a confirmação da arma em uma nova partida. Usa oito ilustrações em seis momentos narrativos e dura cerca de 41 s. O quarto momento, Juramento, inclui uma ilustração vertical no painel lateral. **Espaço** ou **Enter** avança e reposiciona a trilha, e **Esc** segue para a Clareira. A transição reutiliza a apresentação de área com o subtítulo **A Terra dos que Não Descansam**. Checkpoints e novas tentativas após Game Over seguem direto ao jogo. A trilha foi integrada. Os WAVs de efeitos narrativos continuam opcionais.
- Menu, ondas normais e minibosses têm faixas próprias em loop com sobreposição entre fim e começo. A troca de faixa tem fade e a música de combate baixa de volume nas pausas e apresentações. Configurações no menu inicial e na pausa oferecem volumes independentes de música e efeitos, salvos em `audio_pif.cfg` ao lado do executável.
- Ao concluir: mensagem de área limpa e depois **NOVA AREA DESBLOQUEADA + próximo nome**, 3,2 s. Atualmente o anúncio após o cemitério ainda diz "Vila Abandonada"; trocar para **Vila Lascada** ao integrar a vila. O anúncio não significa que a vila já seja acessível.

## Arquivos úteis

- `src/game.c`: loop, telas, entrada, transições e prática.
- `src/enemy.c` / `src/enemy.h`: ondas, inimigos, vida, tiros e carregamento de assets.
- `src/cemetery_enemy.c`: comportamento, animações e VFX do Coveiro; desenho dos inimigos do cemitério.
- `src/clearing.c`: mapas e colisões.
- `src/encounter.c`, `src/presentation.c`: progressão, selos, recompensas e apresentações.
- `src/cutscene.c` e `assets/cutscenes/intro/`: duração, legendas, composição, ilustrações e caminhos de áudio da abertura.
- `src/game_audio.c` e `assets/audio/`: reprodução das faixas do jogo, volumes e persistência.
- `assets/coveiro_walk_v4.png`, `coveiro_throw_v4.png`, `coveiro_slam_v3.png`, `coveiro_charge_v3.png`, `coveiro_pa_vento_v4.png`: assets atuais do chefe.
- `design/coveiro_v4_notas.md`: especificações e notas das artes geradas com imagegen.
- `README.md` contém partes históricas do protótipo, inclusive informações antigas sobre inimigos/checkpoints; conferir implementação e este documento.

## Proposta visual da Vila Lascada — ainda a aprovar

Praça rural ampla, casas deterioradas nas extremidades, madeira escura, pedra cinza, telhados quebrados e vegetação seca. Entrada à esquerda, saída no alto para a capela. Poço fora do centro, oficina/ferraria numa lateral, carroça quebrada e horta morta nas bordas. Casas apenas exteriores para reduzir escopo. Barricadas rompidas e pertences abandonados mostram fuga; sinais do anjo indicam abrigo na capela.

Miniboss proposto: Ferreiro Corrompido; não confirmado. Não produzir personagem definitivo ou tratar layout como aprovado sem continuar a discussão.

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

## O que é salvo e o que precisa de backup

- Código, assets e documentos rastreados chegam ao outro notebook por clone/pull depois do push.
- Instruções em `AGENTS.md` pedem atualização deste arquivo nas próximas entregas. Não existe serviço em segundo plano salvando decisões da conversa: o agente precisa atualizar o arquivo, e publicação continua dependendo de autorização.
- Histórico do aplicativo não é copiado por Git; não há garantia aqui de sincronização desta conversa entre computadores.
- Há muitos rascunhos antigos não rastreados em `design/`, além de arquivos locais fora do repositório. Não foram incluídos no push de gameplay. Preservar a pasta inteira via backup/OneDrive se forem desejados no novo notebook.
- Builds, `.vscode` e outros arquivos ignorados não são enviados pelo Git. Checkpoint `partida_pif.sav` fica ao lado do executável e precisa de cópia separada para levar a partida atual.
- Confirmar manualmente que o OneDrive terminou de sincronizar; este trabalho não verificou nem configurou o serviço.
