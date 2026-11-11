# Jogo PIF

Para retomar o projeto em outro computador ou chat, leia primeiro
[CONTINUIDADE.md](CONTINUIDADE.md). O arquivo registra a situação atual,
decisões de design e passos para compilar. Algumas seções abaixo descrevem
versões antigas do protótipo.

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
cmake -S . -B out/review -G Ninja -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe
cmake --build out/review
Push-Location out/review
.\JogoPiF.exe
Pop-Location
```

O CMake baixa a raylib automaticamente. O executável e os assets copiados ficam
em `out/review/`.

## Menu

O jogo abre no menu, com as artes enviadas em `design/menu_*_referencia.png`.
`tools/prepare_menu.py` recorta os botões em imagens transparentes em `assets/`.
Clique nos botões ou use as setas e Enter. **Iniciar Jogo** abre a escolha entre
Arma de Fogo, Bíblia Sagrada e Katana. Clique numa carta ou use as setas e
Enter; Esc volta ao menu. A escolha cria o checkpoint inicial; se já houver
progresso, o menu pede confirmação antes de substituí-lo.
**Continuar Jogo** retoma do checkpoint inicial da clareira com a arma escolhida,
o único checkpoint disponível no protótipo. **Configurações** permite alternar tela cheia com F ou Enter;
**Créditos** abre uma tela reservada aos nomes da equipe.
O arquivo `partida_pif.sav` fica ao lado do executável.
As cartas e a moldura da escolha estão em `assets/`. A arma selecionada aparece
na interface da clareira.
Com a Katana equipada, o clique esquerdo executa um corte na direção do cursor.
Cada clique inicia um golpe. Segurar o botão não repete o ataque. O efeito foi recortado
de `design/katana_ataque_referencia.png` por `tools/extract_katana.py`.
O mesmo script também produz `assets/katana_corpo.png`, com poses do personagem
sincronizadas com o corte nas quatro direções.
Com a Bíblia equipada, quatro livros giram automaticamente em torno do
personagem em um raio de 90 unidades. O clique esquerdo acelera a órbita por
1,25 s e recarrega em 5 s.
Quando a aceleração começa, o personagem abre a Bíblia numa animação curta.
Os livros usam oito quadros recortados de `design/biblia_frames_referencia.png`
por `tools/extract_bible.py`. O rastro dourado e as partículas acompanham a
órbita. O contato com os inimigos causa dano periodicamente.
Com a Arma de Fogo equipada, cada clique esquerdo dispara um projétil na direção
do cursor. Segurar o botão não repete o tiro. O personagem anima a postura de
disparo a cada clique; os tiros param nos obstáculos da clareira e
mostram clarão e impacto recortados de `design/arma_fogo_referencia.png` por
`tools/extract_gun.py`. Cada projétil causa dano ao primeiro inimigo atingido.
Quatro inimigos encapuzados aparecem na clareira para testar o combate. Eles
perseguem o jogador, atacam de perto, sofrem dano das três armas e deixam um
item vermelho que recupera 15 pontos de vida ao ser recolhido. A esquiva evita
o golpe durante sua animação; após receber dano, o jogador tem um breve
intervalo antes de outro acerto. As poses originais estão em
`design/inimigo_encapuzado_referencia.png`; a vista de costas gerada está em
`design/inimigo_encapuzado_costas_gerado.png`, e a frontal em
`design/inimigo_encapuzado_frente_gerado.png`. O script
`tools/extract_enemy.py` monta as folhas de sprites usadas pelo jogo.
Depois da escolha ou de **Continuar**, a clareira surge com um fade de 1,15 s.
O movimento e a esquiva começam quando a transição termina.

## Abertura em HQ

A abertura roda na primeira partida nova da sessão, depois da escolha da
arma. Espaço ou Enter avança um momento e Esc segue para a Clareira do Selo.

Em `src/cutscene.c`, a tabela `beats` define as durações, as legendas e os
nomes dos efeitos. Ajuste a segunda legenda do juramento em `oathCaption`.
`DrawOpeningCutscene` define posições dos painéis, recortes, zoom e tremor.

Em `assets/cutscenes/intro/`, os sete PNGs mostram o campo, o sobrevivente,
a descida do anjo, o despertar, o juramento, a mão e o confronto. A descida e
o despertar ocupam o mesmo momento narrativo. O painel lateral do juramento
usa a ilustração vertical `quadro_04_complemento.png`. Para adicionar áudio, coloque
`trilha.ogg` e os WAVs nomeados em `beats` nessa pasta. Todos são opcionais.

## Testar a clareira

- WASD: mover.
- Espaço ou botão direito do mouse: esquiva curta na direção atual; parado,
  usa a direção em que o personagem olha. A esquiva recarrega em 5 segundos
  e respeita as colisões.
- F3: mostrar a máscara de colisão e os limites externos.
- F4/F5: tirar ou recuperar 5 pontos de vida para testar os estados da HUD.
- E perto do altar dourado: entrar no Cemitério das Raízes após selar o Guardião da Clareira.
- Esc: voltar ao menu.

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
