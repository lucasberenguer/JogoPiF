# Jogo PIF

Protótipo da clareira em C com raylib 5.5. A arte de fundo está em
`assets/clareira.png`. O mapa marcado pelo criador está em
`design/clareira_marcada.png`. As linhas vermelhas geram
`assets/clareira_collision.png`; as áreas pretas geram
`assets/clareira_foreground_mask.png` para desenhar os galhos sobre o jogador.
As correções feitas sobre o print do jogo estão em `design/clareira_ajustes.png`:
preto acrescenta galho em primeiro plano e ciano libera espaço para andar.
Amarelo marca a posição inicial; o pedestal mantém sua área de interação
definida no código.

## Compilar no Windows (MSYS2 UCRT64)

```powershell
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe
cmake --build build
./build/JogoPiF.exe
```

O CMake baixa a raylib automaticamente. O executável e os assets copiados ficam
em `build/`.

## Testar a clareira

- WASD: mover.
- Espaço ou botão direito do mouse: esquiva curta na direção atual; parado,
  usa a direção em que o personagem olha. A esquiva recarrega em 5 segundos
  e respeita as colisões.
- F3: mostrar a máscara de colisão e os limites externos.
- F4/F5: tirar ou recuperar 5 pontos de vida para testar os estados da HUD
  enquanto o combate ainda não está implementado.
- E perto do pedestal: inspecionar a saída ainda selada.
- Esc: sair.

O personagem usa uma folha de sprites com quatro direções, uma pose parada e
quatro quadros de caminhada por direção, extraída de
`design/protagonista_referencia.png` pelo script `tools/extract_protagonist.py`.
A pose parada de cada direção vem de `design/protagonista_idle_gerado.png`;
os quatro quadros de caminhada de costas vêm de
`design/protagonista_costas_andando.png`.
A esquiva usa `design/protagonista_dash_referencia_v2.png`, recortado por
`tools/extract_dash.py` em `assets/protagonista_dash.png`. Os quadros verticais
mantêm o personagem legível durante todo o impulso, e a pose inicial/final
usa o mesmo rosto da animação normal.
A direção exibida acompanha o deslocamento real e a animação avança conforme
a distância percorrida.

A HUD no canto superior direito usa `design/hud_personagem_referencia.png`.
`tools/prepare_hud.py` separa sua moldura e os preenchimentos da vida e da
esquiva. A vida começa em 100/100; a barra azul recarrega após a esquiva.
As artes `design/hud_vida_media_referencia.png`,
`design/hud_vida_critica_referencia.png` e `design/hud_derrota_referencia.png`
aparecem automaticamente com 36–60%, 1–35% e 0% da vida máxima,
respectivamente.

As colisões seguem a linha central dos traços vermelhos, com uma margem pequena
para o tamanho do personagem. Pedras e pilares grandes têm áreas sólidas
arredondadas, e o movimento desliza pelo eixo livre quando encontra uma borda.
Os galhos
marcados de preto aparecem acima do personagem; ficam parcialmente transparentes
quando ele passa por trás. Objetos sem marcação não bloqueiam movimento.
Para atualizar a máscara após editar a imagem anotada no
mesmo tamanho (1672 × 941 pixels), rode:

```powershell
python tools/generate_clearing_mask.py
cmake --build build
```

O script de geração usa Pillow, NumPy e OpenCV; o jogo compilado não depende
desses pacotes Python.
