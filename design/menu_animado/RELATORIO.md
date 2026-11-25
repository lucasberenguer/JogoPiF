# Menu animado de The Last Rite

A cena conserva a composição enviada pelo Arthur, com o protagonista no canto inferior esquerdo, o necromante diante da lua e o título sobre o eixo dos cinco botões. A arte de referência fica em `assets/menu_principal_arte.png` e a folha de botões em `assets/menu_principal_botoes.png`.

## Arte e ciclos

| Conjunto | Metodologia e ciclos |
| --- | --- |
| Céu e arquitetura | Placa de céu, céu pintado, ruínas em planos, escadaria, pilares e primeiro plano. Os pixels visíveis da referência conservam suas posições. A arte gerada fornece textura para as regiões reparadas. |
| Lua e iluminação | Disco com 20 poses de superfície e luminosidade em 10 s, sem variar seu diâmetro. Aura e quatro máscaras regionais acompanham o mesmo pulso. |
| Nuvens e névoa | Três planos de nuvem e três de névoa. Cada sequência tem 20 poses interpoladas a partir de seis chaves, com ciclos entre 16 e 26 s e deslocamentos distintos. A névoa reaproveita a família de formas baixas com tratamento e composição próprios. |
| Protagonista | Capa com 32 poses. Respiração, cabelo e vestimentas têm 16 poses cada. Os recortes vêm da imagem aprovada e o deslocamento das dobras parte da mesma pose e mantém pontos fixos. |
| Necromante | Corpo com 16 poses, manto com 32 e aura com 24. A figura mantém o capuz fechado. A rotação do círculo usa camadas de anel, marcas e símbolos, com voltas de 52 e 46 s. |
| Vegetação e partículas | Oito tufos de 14 poses, trepadeira com 16, oito estilos de estrela com 10 quadros e 36 variantes de partículas em seis famílias com oito quadros. |
| Título | Letras e ornamentos em camadas, com espadas alinhadas aos botões. Os ornamentos pulsam em 16 passos. O reflexo passa pelas letras em intervalos de 20 s. |
| Botões | PNGs individuais de cinco estados para cada ação e atlas de 60 quadros de brilho. O jogo usa a folha aprovada para as inscrições e compõe o estado ativo sobre ela. |

As poses de tecido e vegetação usam deformação localizada e alterações de luz em pixels da referência. Nuvens e névoa usam edição incremental por fluxo entre chaves. Estrelas, traços arcanos e partículas têm desenho em grid de pixels. A versão usa a arte reconstruída apenas nas regiões necessárias para movimentar os elementos, com a arquitetura fixa.

## Integração

- `src/menu_scene.c` carrega os atlas RGBA com filtragem de vizinho mais próximo, lê os metadados de posição, célula e ciclo e desenha cada plano na ordem definida.
- `src/menu.c` conserva os destinos dos cinco botões e integra entrada de 3,8 s que pode ser pulada, destaque da seleção e transição de 0,68 s até a escolha de arma.
- `src/video_settings.c` e `src/video_settings.h` persistem **Movimento do menu → Reduzido**. Essa opção exibe a composição estática e libera o menu sem as transições.
- `src/game.c` integra o término da transição e `src/weapon_select.c` aceita direcional, confirmar e voltar por controle.
- `assets/menu/metadata/layers.json` descreve a montagem e `layers.txt` contém a configuração lida pela engine. `tools/prepare_menu_scene.py --frames out/review/menu-animado/frames` recompõe os atlas e exporta a sequência individual de cada ciclo.

## Fontes geradas

O ImageGen integrado recebeu a referência do jogo para três gerações. Os PNGs usados na produção estão em `fontes/`.

- **`ceu.png`** — “Paint only its pixel-art atmospheric sky, no text, no moon, no characters, no ground, no stars. Same dark plum near-black at top and pink carmine horizon glow bottom, distant dithering of the exact original pixel scale.”
- **`clima.png`** — “Make a game sprite atlas of translucent pixel art wind clouds and curling wisps of fog. A precise grid of 6 columns and 3 rows with 18 isolated horizontal formations. Each row has exactly six distinct morph key shapes of the same family.” As três famílias são nuvens escuras com bordas escarlates, nuvens vinho com curvas e névoa baixa.
- **`lua.png`** — “Create a standalone, circular pixel art crimson moon sprite. Reconstruct the unobscured disc in the image reference, flat on to the camera, same aged scarlet surface and dark red hand-dithered details.” A textura ajuda nas mudanças da superfície lunar.

## Validação e entrega

- Foram empacotadas 48 camadas, com 892 quadros de animação da cena e mais 60 quadros de brilho dos botões. Os 952 quadros individuais estão em `out/review/menu-animado/quadros_png.zip`, acompanhados dos metadados.
- A build Release fica em `out/review/release/JogoPiF.exe`. O teste gráfico conferiu a abertura e o escurecimento final, a navegação com mouse e teclado, as entradas simuladas de controle, a confirmação de nova partida, o save e a persistência do modo reduzido. As capturas em 1280×720 e 800×450 e a passagem pelo renderizador a 50% foram verificadas.
- `clearing_encounter` e `guardian_story_flow` passaram no CTest Debug. A medição local de 150 quadros da janela oculta registrou aproximadamente 0,48 ms por quadro em 1280×720 e 0,63 ms em 800×450. A medição inclui desenho e troca de buffers, sem exportar imagens. As texturas da cena ocupam aproximadamente 125 MiB em RGBA.
- `out/review/menu-animado/` contém a gravação completa em MP4 e GIF, uma prévia ampliada dos personagens, uma das seis camadas de clima e uma do reflexo do título.
- Algumas formações do céu permanecem na pintura fixa para conservar a composição. As regiões antes ocultas foram reconstruídas localmente, e as poses dos personagens foram derivadas dos recortes da referência. A validação de controle foi feita com entradas simuladas.
