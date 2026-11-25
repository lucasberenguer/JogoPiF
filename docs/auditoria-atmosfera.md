# Auditoria de atmosfera dos oito mapas

## Estado antes e depois

I = implementado, P = parcial, A = ausente, C = corrigido com movimento detectado, — = efeito sem fonte na arte ou fora da configuração daquele mapa. Cada célula mostra **antes → depois**. O grupo de partículas inclui as rajadas no centro, a poeira, as brasas, as cinzas, a fumaça e os vaga-lumes, conforme o mapa.

| Mapa | Luz refletida | Rajadas no centro | Vegetação | Chamas | Feixes e poeira | Névoa, sombras e partículas | Runas, fendas e céu |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Clareira | P → C | P → C | A → C | — | P → C | I → I | P → C, selo |
| Cemitério | P → C | P → C | A → C | A → C | — | I → I | I → I, cristais |
| Chã da Tristeza | P → C | A → C | A → C | A → C, velas e forja | — | I → I | P → C, runas |
| Capela do Anjo | P → C | — | A → C, musgo | A → C | P → C | I → I | P → C, círculos |
| Capela Invertida | P → C | — | A → C, vinhas | A → C | P → C | I → I | P → C, círculo, vitral e cristais |
| Terra dos Ossos | P → C | P → C | A → C | — | P → C | I → I | P → C, fendas |
| Campo dos Templários | P → C | P → C | — | — | A → C | I → I | P → C, escadaria, fendas e céu |
| Arena do Necromante | P → C | P → C | — | — | A → C | I → I | P → C, lua, chifres, octógono e céu |

A luz aditiva amostra a própria textura do cenário. Velas, forja e reflexo local compartilham a variação de intensidade. A vegetação usa regiões calibradas e deslocamentos horizontais inteiros de até três pixels. A textura de ruído é criada na inicialização e os emissores reutilizam um conjunto fixo de partículas. Os sprites recebem a cor da luz próxima. As sombras combinam direção do mapa com a luz pontual mais forte no alcance do personagem. O octógono recebe o valor de fase pelo parâmetro `bossPhase` do desenho do mapa.

## Como reproduzir

A partir de `pif-teste-atual`, compile `out/review` e rode `ctest --test-dir out/review -R atmosphere_audit --output-on-failure`. As evidências ficam em `out/review/audit/atmosfera/`, fora do Git.

- `medicoes.csv` contém diferenças médias por efeito isolado e FPS nas três qualidades. `quadros.csv` compara as regiões dos quadros de 0,5 s, 1 s e 2 s com o de 0 s. O sprite permanece na mesma posição.
- `mapa-*-{0,500,1000,2000}ms.png` são os 32 quadros renderizados em 1280 × 720 com o carregador e o renderizador de produção. `mapa-*-guias.png` mostra as fontes de luz, as chamas, as colisões e as passagens. As capturas anteriores estão em `out/review/audit/antes-*.png`.
- Durante a partida, F7 abre o painel e Tab alterna colisões e passagens. As teclas 7 e 8 alternam vento e fogo. F8 troca a qualidade e F9 ativa movimento reduzido. A preferência de animações do Windows é lida na inicialização.
- A janela oculta e o modo reduzido tiveram diferença RGB igual a zero nos oito mapas. Todos os grupos de regiões configuradas em `quadros.csv` apresentaram movimento em pelo menos uma comparação. A troca entre a primeira e a terceira forma do Necromante apresentou diferença média de 0,122676 em torno do octógono.
- FPS medidos numa Intel Iris Xe, com um personagem em cena e leitura do quadro da GPU. São médias de ensaios curtos do renderizador, sem o limite do jogo e sem uma horda ou batalha ativa. O CSV inclui alta, média e baixa por mapa.

| Mapa | Alta | Média | Baixa |
| --- | ---: | ---: | ---: |
| Clareira | 115,3 | 125,8 | 126,2 |
| Cemitério | 116,7 | 116,7 | 127,8 |
| Chã da Tristeza | 126,0 | 122,7 | 130,0 |
| Capela do Anjo | 139,1 | 131,1 | 135,7 |
| Capela Invertida | 111,2 | 113,4 | 115,5 |
| Terra dos Ossos | 142,0 | 141,4 | 146,4 |
| Campo dos Templários | 119,6 | 137,2 | 145,9 |
| Arena do Necromante | 138,0 | 135,6 | 142,2 |

Nas amostras de troncos, pedras e muralhas com os efeitos ativos, a diferença média máxima foi de 0,913 em 255 níveis por canal. A suíte completa passou 9/9.

## Colisão e escopo

A auditoria preservou as colisões aprovadas, o ponto de entrada e os gatilhos de passagem. O carregador abriu os oito mapas e os pontos de entrada estavam livres. A saída da Terra dos Ossos continua discreta na arte, no caminho escuro ao norte, e algumas pontas dos bancos quebrados da Capela Invertida ficam fora da máscara compartilhada. A saída do Cemitério continua junto à árvore da direita. Esses pontos são observações da guia.

O teste do Cemitério ainda esperava encontrar bloqueio acima de uma lápide. O teste agora distingue o topo visual, transitável segundo a marcação aprovada, da base bloqueada. A build de testes mantém as asserções ativas em Release. A execução inicial do jogo compilou os shaders e abriu o menu sem erros. A suíte narrativa emite avisos preexistentes de arquivos da cutscene da Terra dos Ossos que não estão presentes, e de faixas de questões com fallback.

O deslocamento da folhagem é localizado em regiões, com transição suave nas margens. Distorção térmica da forja e balanço de teias, bandeiras e correntes ficaram fora desta revisão. O Porão não faz parte dos oito mapas deste ensaio. A estabilidade de 60 FPS em combate depende de teste no hardware de apresentação.

Arquivos desta revisão incluem `src/atmosphere.c`, `src/atmosphere.h`, `src/atmosphere_config.h`, `src/game.c`, `tests/atmosphere_audit.c`, `tests/enemy_audit_test.c` e `CMakeLists.txt`.
