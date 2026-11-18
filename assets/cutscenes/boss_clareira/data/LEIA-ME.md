# Cutscene do Guardião da Clareira

## Integração

O projeto usa C99 e raylib 5.5. `src/game.c` coordena as telas, `encounter.c` inicia ondas e chefes, `presentation.c` desenha os anúncios, `game_audio.c` controla os volumes e trilhas e `cutscene.c` mantém a abertura existente.

`guardian_cutscene.c` implementa a nova sequência com a tipografia compartilhada em `game_text.c`. Ela começa depois da recompensa da quarta onda da Clareira, antes da animação de surgimento em gameplay. O Cemitério continua usando sua apresentação atual.

As 16 imagens recebidas foram copiadas para `images/quadro_01.png` até `quadro_16.png`, na ordem dos anexos, preservando os arquivos. A sequência explícita em `sequencia.ini` possui 24 cenas, distribuídas nos seis atos do roteiro, com as nove falas originais. Algumas imagens reaparecem durante o diálogo.

## Configuração

Cada bloco `[scene identificador]` representa um enquadramento. A ordem dos blocos define a sequência, independentemente do nome da imagem.

- `image` indica um arquivo de `images/`. Uma imagem ausente deixa uma cena escura com a duração e o diálogo previstos e registra a ausência no log.
- `duration` define a duração das cenas visuais e o tempo do movimento de câmera nas cenas com fala.
- `next=duration` avança a cena visual automaticamente. `next=advance` mantém a fala até o comando do jogador.
- `camera` recebe seis números, foco X, foco Y e zoom inicial, depois foco X, foco Y e zoom final. Focos usam coordenadas de 0 a 1 e zoom fica entre 1 e 1,2.
- `transition_in` e `transition_out` aceitam `cut`, `fade`, `crossfade` e `white`. `fade_in` e `fade_out` definem os tempos. O crossfade é aplicado na entrada sobre a cena anterior.
- `effects` aceita `leaves`, `dust`, `fog`, `shake`, `seal` e `corruption`. O tremor é breve e amortecido. O efeito `seal` usa a coordenada normalizada de `seal` para acompanhar o enquadramento.
- `speaker` e `dialogue` definem as falas. A digitação respeita caracteres UTF-8 e as linhas são ajustadas à largura disponível.
- `title` e `subtitle` identificam o chefe no último enquadramento.
- `music` aponta para uma trilha existente no projeto. Omitir o campo deixa a cena sem trilha. Cenas consecutivas com o mesmo caminho mantêm a reprodução.
- `audio_event` procura `audio/<evento>.wav`, usando o volume de efeitos. O carregamento só ocorre se o arquivo existir.

As imagens completas recebem pan e zoom discretos. O flashback usa transições claras. As camadas de poeira, folhas, névoa e luz são desenhadas separadamente. Os personagens e os cenários permanecem nas ilustrações originais.

## Controles

Espaço ou Enter completa a fala em digitação. Um novo toque avança para a próxima fala. Nas cenas visuais, o comando avança o enquadramento. Esc pula toda a sequência. P pausa e retoma.

Em controle compatível com raylib, o botão inferior do grupo direito, normalmente A, completa ou avança. O botão direito, normalmente B, pula. Start pausa e retoma.

Ao pular ou concluir, os sons da cutscene param, as texturas e streams são liberados e o jogo faz fade para a Clareira. O Guardião mantém a animação de surgimento de três segundos e a luta começa ao término dela. Entradas da cutscene são consumidas antes da retomada dos controles de combate.

## Áudio existente e pendências

A trilha fornecida em `synthwave-nostalgic-take-3.mp3` foi convertida para `audio/trilha.ogg`. A faixa original tem cerca de 60 segundos. Uma sobreposição de dois segundos entre o fim e o começo prepara o loop de 58 segundos, que acompanha as 24 cenas continuamente e permite tempo livre de leitura das falas. A música baixa suavemente durante os diálogos e faz fade no encerramento. O volume segue o controle de música das configurações. A trilha de gameplay do miniboss é acionada pelo sistema de áudio ao retomar a arena.

F1 abre o painel de desenvolvimento durante a cena e pausa sua imagem, digitação e áudio. Ao fechar o painel, a cena retoma o estado de pausa anterior.

Os seguintes efeitos ainda precisam de arquivos em `audio/`.

| Evento | Conteúdo esperado |
| --- | --- |
| `lamento_templarios.wav` | Atmosfera de luto após o confronto |
| `vento_campo.wav` | Vento pelo campo de batalha |
| `floresta.wav` | Ambiente da floresta |
| `selo.wav` | Ressonância do selo examinado |
| `ruptura_solo.wav` | Tremor e ruptura da terra |
| `raizes.wav` | Raízes e madeira em movimento |
| `corrupcao.wav` | Presença sobrenatural |
| `memoria_floresta.wav` | Pássaros e ambiente da memória |
| `ritual.wav` | Atmosfera do ritual |
| `resistencia.wav` | Esforço do Guardião |
| `selo_furia.wav` | Pulsação violenta do selo |
| `impacto_raizes.wav` | Impacto antes do combate |

Não há dublagem fornecida. As falas são apresentadas como texto. Os efeitos podem ser adicionados nos caminhos indicados e entram na sequência pelo evento já configurado.

## Checkpoints e repetição

O formato `PIF-RUN-6` acrescenta `guardianIntroSeen`. A flag é salva ao terminar ou pular, para que Continuar e as revanches da mesma partida retomem o chefe. Uma nova tentativa completa reinicia a progressão e permite assistir novamente. Checkpoints antigos continuam legíveis. Saves anteriores no chefe da Clareira exibem a nova sequência uma vez. Saves com a Clareira concluída ou já no Cemitério são migrados como tendo assistido à cena.

Se o arquivo de sequência for inválido, o jogo usa a apresentação de chefe existente e registra o problema. O exemplo JSON do roteiro anexado estava interrompido. A configuração INI contém todos os atos e diálogos completos disponíveis no anexo.
