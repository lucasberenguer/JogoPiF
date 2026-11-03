# Jogo PIF

Protótipo da clareira em C com raylib 5.5. A arte de fundo está em
`assets/clareira.png`. O mapa marcado pelo criador está em
`design/clareira_marcada.jpg`; `assets/clareira_collision.png` é a máscara de
colisão gerada a partir das linhas vermelhas. Azul marca o pedestal e amarelo
marca a posição inicial do jogador.

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

O personagem ainda é um círculo provisório. As colisões seguem os traços
vermelhos, com uma margem para o tamanho dele. Pedras pequenas sem marcação não
bloqueiam movimento. Para atualizar a máscara após editar a imagem anotada no
mesmo tamanho (1672 × 941 pixels), rode:

```powershell
python tools/generate_clearing_mask.py
cmake --build build
```

O script de geração usa Pillow, NumPy e OpenCV; o jogo compilado não depende
desses pacotes Python.
