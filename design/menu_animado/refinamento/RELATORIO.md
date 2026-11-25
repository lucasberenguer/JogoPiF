# Refinamento ambiental de The Last Rite

## Capa

Os sete atlas em `assets/menu/characters/protagonist/cape_refinada/` compartilham o mesmo recorte, a mesma âncora e uma linha de tempo contínua de 18 segundos.

| Trecho | Quadros | Duração |
| --- | ---: | ---: |
| Calmaria | 24 | 2,4 s |
| Brisa | 32 | 2,4 s |
| Vento constante | 48 | 3,2 s |
| Rajada moderada | 40 | 2,0 s |
| Rajada intensa | 48 | 2,4 s |
| Recuperação | 32 | 2,4 s |
| Transição para calmaria | 48 | 3,2 s |

Cinco faixas acopladas de tecido transmitem a pressão dos ombros às pontas com elasticidade limitada e amortecimento. A tira superior e as pontas inferiores recebem cargas diferentes. Vincos deformam localmente o tecido e mudam a área de luz carmesim. A região presa ao ombro fica fixa e o símbolo dourado acompanha a mesma malha do tecido. As chaves geradas orientam o contraste dos vincos. A silhueta, a cruz e a posição do personagem vêm dos pixels da referência.

## Vegetação e nuvens

- **Vegetação.** 16 modelos de chão e uma trepadeira. Cada conjunto tem oito posições de inclinação e oito fases de movimento das folhas, com raiz fixa. O vento compartilhado é lido com atraso e amortecimento próprios. Os juncos e os capins altos se inclinam mais que os tufos baixos.
- **Nuvens.** Três novas famílias com seis chaves cada. Os 36, 40 e 44 quadros dos três planos usam fluxo óptico também nas bordas da silhueta e variação de iluminação dentro da nuvem. Os ciclos duram 36, 28 e 22 segundos. A pintura fixa da referência continua ao fundo.

## Produção e reprodução

O ImageGen integrado gerou as chaves armazenadas nesta pasta. Os prompts pediram seis posições de capa medieval pesada e rasgada com cruz dourada, oito silhuetas de plantas enraizadas em escala de pixel art e uma folha de três famílias de nuvens carmesim com seis posições de transformação por família. Cada geração usou a arte de The Last Rite como referência.

`tools/prepare_menu_scene.py` usa `tools/menu_environment.py` para produzir os atlas e os metadados com `python -B tools/prepare_menu_scene.py`. `assets/menu/metadata/vento.txt` contém a curva compartilhada e `layers.json` registra ordem, origem, tamanho, quantidade de quadros e duração. `src/menu_scene.c` seleciona o quadro no trecho ativo da capa e combina amostras anteriores de vento para a vegetação.

## Validação

- Os 272 quadros da capa são distintos. Os pixels da região fixa do ombro conservam os valores da referência em todos os quadros. As emendas entre trechos e a volta à calmaria ficaram dentro da variação normal de silhueta observada entre quadros vizinhos.
- A versão Release foi compilada com a integração de controle de `d988ece`. Foram conferidos por hash os 61 atlas da cena e seus dois arquivos de configuração em texto.
- A auditoria gráfica passou em 1280×720 e 800×450 com navegação simulada, save isolado e movimento reduzido. Na janela de teste, a amostragem de desenho e troca de buffers ficou abaixo de 1 ms por quadro. A cena aloca cerca de 206 MiB em texturas RGBA.
- `out/review/menu-ambiente/ambiente_refinado.mp4` mostra o ciclo de 18 segundos. `capa_e_vegetacao.gif` amplia o primeiro plano e `capa_272_quadros.zip` contém os PNGs individuais e os metadados da capa.
