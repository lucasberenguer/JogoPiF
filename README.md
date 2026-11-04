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
- F3: mostrar a máscara de colisão e os limites externos.
- E perto do pedestal: inspecionar a saída ainda selada.
- Esc: sair.

O personagem usa uma folha de sprites com quatro direções, uma pose parada e
quatro quadros de caminhada por direção, extraída de
`design/protagonista_referencia.png` pelo script `tools/extract_protagonist.py`.
A pose parada de cada direção vem de `design/protagonista_idle_gerado.png`;
os quatro quadros de caminhada de costas vêm de
`design/protagonista_costas_andando.png`.
A direção exibida acompanha o deslocamento real e a animação avança conforme
a distância percorrida.

As colisões seguem a linha central
dos traços vermelhos, com uma margem pequena para o tamanho dele. Os galhos
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
