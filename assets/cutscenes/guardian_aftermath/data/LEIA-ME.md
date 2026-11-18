# Libertação do Guardião e passagem pelo altar

## Acionamento e estados

`AnswerEncounterSeal` confirma o selo correto do boss da Clareira e salva imediatamente `GUARDIAN_LIBERATION_PENDING`. Derrotar sem selar, errar a resposta ou concluir outro mapa não aciona a cena. O selo errado mantém a revanche e o fortalecimento existentes.

`game.c` abre a mesma tela cinematográfica da introdução. Movimentação, armas e interação ficam fora do loop de gameplay durante a cena. A primeira parte termina em `GUARDIAN_ALTAR_READY`, com o objetivo **Investigue o altar da clareira**. Espaço/Enter completa a digitação e depois avança; Esc pula a parte atual; P pausa; F1 preserva a pausa e o áudio. Controle usa A/B/Start.

A primeira parte devolve controle para caminhar até o altar. Próximo dele aparece **E: Interagir com o Altar Antigo**. Usar E salva `GUARDIAN_PASSAGE_PENDING` antes de carregar a segunda parte e bloqueia ativações repetidas. A segunda parte conclui a ativação, carrega o cemitério na primeira onda e salva `GUARDIAN_PASSAGE_COMPLETE`. A cartela tem 0,9 s de entrada, 3 s de leitura e 0,9 s de saída, seguida do fade para o mapa.

Pular a libertação libera o altar e posiciona o jogador num ponto livre próximo. Pular a passagem conclui o transporte. Se a sequência não carregar, aplica-se o mesmo estado final; se o mapa do cemitério falhar, o altar volta a ficar utilizável. Fechar a janela durante uma das partes mantém o estado pendente no checkpoint, e Continuar a retoma sem repetir o combate. Depois de concluída, não volta a tocar na mesma partida.

`PIF-RUN-7` preserva o formato anterior e acrescenta o estado pós-Guardião (0–4). Saves antigos de Clareira já concluída migram para altar pronto; saves em mapas posteriores migram para passagem concluída. O modo prática não modifica o checkpoint da campanha.

## Estrutura e arquivos

- `libertacao.ini`: 16 enquadramentos, oito falas; aproximadamente 60 s, com leitura manual livre.
- `passagem.ini`: sete enquadramentos, duas falas; aproximadamente 30 s mais a cartela.
- `../images/quadro_01.png` a `quadro_11.png`: os 11 anexos, copiados sem edição (SHA-256 conferido).
- `../audio/`: trilha convertida e seis efeitos; fontes e limitações em `CREDITOS.md`.
- `guardian_cutscene.c/.h`: carregador, pan/zoom proporcional, transições, caixa de diálogo, música contínua, pausa e descarregamento compartilhados com a cena anterior ao boss.
- `encounter.c/.h`, `run_progress.h`, `menu.c`: purificação, altar, passagem e checkpoint.
- `game.c`, `game_audio.c/.h`: telas, bloqueio de controles, encerramento da música anterior e retomada musical do gameplay.
- `dev_tools.c/.h`: F1 → **Libertação do Guardião** / **Altar e passagem**, em prática. **Testar selo do boss** permite conferir acerto/erro; **Encerrar onda atual**, num boss, derruba-o para o selo.

As chaves existentes de câmera, falas, `audio_at`, `audio_gain`, duração e transição continuam válidas. Novos efeitos: `purify` (partículas roxas enfraquecem), `cyan` e `gold` (luz/partículas), `wisp` (luz se dirige ao ponto `seal`). `music_start` posiciona a trilha somente ao abrir um stream novo. `chapter_card=true`, `chapter`, `title`, `subtitle` e `tagline` compõem a cartela sobre preto, usando as fontes reais do jogo. A capitalização do título foi ajustada para leitura da fonte gótica. A arte 11 permanece intacta como referência da cartela; seu texto já pintado não é sobreposto ao título renderizado.

## Correspondência dos 23 enquadramentos com os anexos

O roteiro descreve 22 imagens narrativas e uma cartela, mas foram fornecidos dez quadros narrativos e uma cartela. As repetições abaixo são intencionais; não existem arquivos referenciados ausentes nem artes substitutas geradas.

| Enquadramentos do roteiro | Arquivos usados | Tratamento |
| --- | --- | --- |
| 01.1 / 01.2 | `quadro_01.png` | Queda e aproximação no mesmo quadro, com focos diferentes |
| 01.3 / 01.4 | `quadro_02.png` | Rosto e peito do Guardião |
| 02.1 / 02.2 | `quadro_03.png` | Livro e símbolos dourados |
| 02.3 / 02.4 | `quadro_04.png` | Partículas e consciência recuperada |
| 03.1–03.5 | `quadro_05.png` | Altar, Guardião, protagonista, floresta e mão/luz |
| 04.1 | `quadro_04.png` | Folhas e dissolução por transição para a clareira |
| 04.2 / 04.3 | `quadro_06.png` | Recuperação parcial da clareira e foco no altar |
| 05.1–05.3 | `quadro_07.png` | Toque, livro e passagem luminosa |
| 06.1 | `quadro_08.png` / `quadro_09.png` | Transporte acolhedor em raízes douradas, com plano aberto e próximo |
| 06.2 / 06.3 | `quadro_10.png` | Chegada e cemitério no mesmo plano; fala da voz desconhecida |
| Cartela | `quadro_11.png` como referência | Título composto em tempo real sobre preto |

Para ter uma ilustração exclusiva por momento, faltam artes dedicadas a **01.2, 01.4, 02.2, 02.4, 03.2, 03.3, 03.4, 03.5, 04.1, 04.3, 05.2, 05.3 e 06.3**. O plano próximo extra do transporte usa a arte 09 já fornecida. Esses itens são oportunidades de refinamento, não arquivos fictícios exigidos pelo carregador.

## Validação

- `clearing_encounter`: selo errado, confirmação correta, bloqueio antes da cena, liberação do altar, interação próxima por E, prevenção de repetição, recarga dos estados pendentes e chegada ao mapa 1; migração e rejeição de saves inválidos; prática preserva campanha.
- `guardian_cutscene`: as dez falas exatas e seus personagens, 16+7 eventos, imagens/sons/trilha carregados, stream contínuo, digitação/manual, avanço automático dos planos, pausa, fade de saída após falas e pulo/limpeza de áudio; capturas em 1280×720 e 800×450.
- `guardian_story_flow`: executa o loop real de `game.c` com entradas simuladas, erra e acerta o selo, conclui ou pula as duas partes, verifica ausência da música de combate, controle devolvido entre partes e save final no cemitério. Usa checkpoint isolado em `out/review/audit/story_flow`.

As durações são estimativas; diálogos aguardam o jogador. Não há dublagem ou trilha de 90 s fornecidas. As animações são de câmera e camadas de luz/partículas sobre as artes originais, sem animação óssea dos personagens. Controle físico não foi testado.

Build de 18/11 em `out/review` concluído; 6/6 testes passaram (incluindo as três auditorias existentes de inimigos, conexão dos mapas e colisão). As capturas citadas foram inspecionadas. Os seis WAVs foram conferidos como PCM de 16 bits com pico abaixo de 0 dBFS.
