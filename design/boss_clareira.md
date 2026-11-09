# Boss da clareira — recurso visual

Arte fornecida pelo autor em `boss_clareira_referencia.png`.
Spritesheet preparada: `assets/boss_clareira.png`.

- 6 quadros por direção, células de 256 × 272 pixels.
- Linhas: baixo, cima, esquerda, direita, na convenção do jogo.
- Origem nos pés: (128, 264). Preservar o porte maior do boss.
- Transparência original preservada, incluindo rosto e sombras internas.
- A folha mostra poses direcionais com movimento de capa e corpo; não contém
  sequências específicas de ataque, dano ou morte.

O boss pertence à clareira e aparece na quinta onda, após quatro hordas e a escolha do bônus
forte, conforme `docs/GAME_DESIGN.md`. Quando ele cai, E abre o selo de
simplificação lógica. Errar fortalece o boss e inicia uma revanche.

## Fases

O Guardião começa com 2.200 de vida. Ao chegar a 1.100, ele interrompe
o golpe em andamento e fica invulnerável por um segundo enquanto desperta.
A segunda fase começa com a Maldição das Raízes em dois pontos, marcados
em momentos diferentes. O Guardião se move 50% mais rápido e os intervalos
dos especiais e do corpo a corpo caem para 1,95 s e 1,25 s. A barra de vida
marca a metade e muda de cor. O selo só abre após vencer as duas fases,
e uma revanche volta à primeira fase com mais vida.

## Primeiro especial: raízes

Folhas fornecidas pelo autor, preservadas em
`boss_clareira_especial_1_referencia.png` e
`boss_clareira_efeitos_1_referencia.png`.

Recursos preparados por `tools/prepare_boss_special.py`:

- `assets/boss_clareira_especial_1.png`: 9 quadros por direção, células de
  384 × 288; linhas baixo, cima, esquerda e direita. Origem (192, 280).
  A escala da arte foi compensada para corresponder ao corpo da folha normal.
- `assets/boss_clareira_raizes_linha.png`: 8 quadros, células de 288 × 256.
- `assets/boss_clareira_raizes_area.png`: 8 quadros, células de 288 × 256.
- `assets/boss_clareira_folhas.png`: 8 quadros, células de 288 × 256.

A sequência do boss mostra preparação nos quadros 0 a 4, raízes nos quadros
5 e 6 e recuperação nos quadros 7 e 8 (índices iniciando em zero).
Os efeitos separados permitem posicionar as raízes no cenário; os quadros
5 e 6 já contêm raízes perto do corpo e isso deve ser considerado ao desenhar.
O primeiro especial mira a última posição do jogador e até dois pontos
próximos. Cada área usa a arte de raízes em área, tem raio de 74 unidades,
avisa por cerca de 0,8 segundo e causa 30 de dano inicial durante a erupção.
Na metade da vida, o Guardião se aproxima mais rápido, reduz o intervalo
entre ataques e marca uma segunda área na posição atual do jogador.
Alcance, intervalo e dano são valores iniciais para testar na partida.

## Segundo especial — Onda de Raízes

No segundo especial, o Guardião lança uma onda que segue a direção
do jogador no início do golpe. A espiral aparece junto ao Guardião enquanto
as áreas são marcadas no chão. As raízes avançam em até cinco erupções,
ou seis a partir da metade da vida. Rochas e paredes interrompem a onda.
Cada erupção tem raio de 60 unidades e causa 36 de dano base nos quadros
de impacto. É possível esquivar durante o impacto.

`assets/boss_clareira_onda.png` usa as duas primeiras linhas da folha
fornecida, com oito quadros de 352 × 288 por linha e transparência original.

## Terceiro especial — Maldição das Raízes

O Guardião usa os especiais na ordem raízes, onda e maldição. A maldição
marca em roxo a posição do jogador no início do golpe e carrega por um
segundo. As raízes roxas causam 42 de dano base uma vez por execução em um
raio de 110 unidades. O jogador pode sair da área ou esquivar no impacto.

`assets/boss_clareira_maldicao.png` usa oito quadros de preparação e sete
de erupção da folha fornecida. As células de 384 × 320 mantêm o ponto de
origem no chão.

## Golpe corpo a corpo

Quando o jogador chega a até 118 unidades, o Guardião pode desferir um
golpe em arco à frente. A direção é fixada na preparação e o impacto ocorre
no quinto quadro, cerca de 0,4 segundo depois. O golpe causa 26 de dano base,
com um intervalo próprio de 1,65 segundo entre inícios. Sair do arco ou
esquivar no impacto evita o dano.

`assets/boss_clareira_corpo_a_corpo.png` usa oito quadros por direção em
células de 384 × 288, com a origem nos pés em (192, 280).
