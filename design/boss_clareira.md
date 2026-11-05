# Boss da clareira — recurso visual

Arte fornecida pelo autor em `boss_clareira_referencia.png`.
Spritesheet preparada: `assets/boss_clareira.png`.

- 6 quadros por direção, células de 256 × 272 pixels.
- Linhas: baixo, cima, esquerda, direita, na convenção do jogo.
- Origem nos pés: (128, 264). Preservar o porte maior do boss.
- Transparência original preservada, incluindo rosto e sombras internas.
- A folha mostra poses direcionais com movimento de capa e corpo; não contém
  sequências específicas de ataque, dano ou morte.

O boss pertence à clareira e aparece após cinco ondas e a escolha do bônus
forte, conforme `docs/GAME_DESIGN.md`. Quando ele cai, E abre o selo de
simplificação lógica. Errar fortalece o boss e inicia uma revanche.

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
próximos. Cada área usa a arte de raízes em área, tem raio de 62 unidades,
avisa por cerca de 0,8 segundo e causa 24 de dano inicial durante a erupção.
Alcance, intervalo e dano são valores iniciais para testar na partida.
