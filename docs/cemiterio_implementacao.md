# Cemitério das Raízes — implementação técnica 0.1

## Estado desta entrega
Base funcional em C/raylib: transição da clareira, mapa do cemitério, colisões aproximadas, quatro esqueletos corpo a corpo, dois magos com projéteis verdes, Coveiro com três ataques, efeitos visuais e barras de vida. Compilação e testes automatizados aprovados. Ainda não houve validação visual/interativa desta versão. Não considerar a fase finalizada.

## Como experimentar
Compile com CMake e execute build/JogoPiF.exe a partir de build, onde está a cópia de assets. Inicie uma partida e escolha uma arma. Caminhe pela abertura inferior marcada da clareira: a passagem para o cemitério é automática, sem E e sem exigir derrotar os inimigos. Para desenvolvimento, F7 na clareira entra diretamente no cemitério. F3 mostra limites, bases bloqueadas e região da saída. ESC volta ao menu; iniciar ou continuar recria a clareira. O checkpoint existente continua salvando somente a arma, não a fase ou estado da run.

## Mapa: desenho e sistema são coisas diferentes
assets/cemiterio.png é o desenho completo aprovado, 1670 x 942. As coordenadas do jogador representam seus pés. O mapa é ajustado à janela com Camera2D, mas movimento, alcance e colisões usam pixels do mundo, não da tela.

As colisões são retângulos independentes da arte em InitCemetery (src/clearing.c). A base da capela e grupos de túmulos bloqueiam o movimento. A copa ou altura visual não deve virar uma parede invisível. Atualmente há um limite retangular da arena e 15 bases estimadas; não há máscara detalhada de árvores nem camada de primeiro plano no cemitério. Isso exige ajuste manual com F3 antes de aprovar navegação. A capela é cenário, sem interior nesta versão.

Entrada: centro superior, em 50% da largura e 22% da altura. Saída futura: lateral direita, entre 46% e 58% da altura. A região está marcada em verde no debug; a vila ainda não existe e não é carregada. A passagem da clareira é livre por enquanto: cruzar a região inferior marcada carrega o cemitério automaticamente. A abertura atravessa o limite antigo e a máscara de colisão apenas nessa faixa.

## Encontro atual e valores de teste
Estes números são propostas de balanceamento, não decisões definitivas.

| Entidade | Quantidade | Vida | Movimento e ataque |
|---|---:|---:|---|
| Esqueleto comum | 4 | 60 | Persegue a 92 px/s; alcance 64 px; dano 12; intervalo 1,35 s |
| Esqueleto mago | 2 | 60 | Aproxima acima de 340 px, recua abaixo de 170 px a 70 px/s; começa conjuração até 480 px |
| Coveiro | 1 | 480 | Inativo e invulnerável enquanto os seis inimigos não terminarem a morte; alterna três ataques |

Mago: registra a direção ao iniciar conjuração, lança em 0,55 s e termina em 0,95 s. Intervalo entre inícios: 2,1 s. Bola verde: 210 px/s, dano 10, duração máxima 4 s; desaparece ao atingir parede ou jogador. Hoje a bola é desenhada com círculos de luz, não uma animação de fogo final. Não há verificação de linha de visão: um mago atrás de obstáculo pode disparar contra a parede.

Coveiro: ciclo determinístico raízes → pá → investida, para facilitar aprendizado e testes. Cada animação dura 2 s; intervalo entre inícios de 3,2 s. Mira é registrada no início da preparação, permitindo que o jogador se reposicione. Vida e arma são preservadas na transição; disparos e animações temporárias são limpos.

1. Raízes: círculo de aviso no local marcado por 0,8 s; raio 95 px; dano 24 entre 0,8 e 1,2 s. Efeito continua se dissipando depois da janela de dano. Não aplica imobilização.
2. Pá giratória: lançamento em 0,8 s; velocidade 310 px/s; dano 22; duração máxima 2 s. Nesta versão a pá desaparece no impacto/parede/fim do tempo. Retorno ao chefe e captura ainda precisam de implementação; as poses de captura na referência não representam uma mecânica pronta.
3. Investida: linha de aviso durante 0,8 s; avanço reto de 430 px/s entre 0,8 e 1,35 s, respeitando colisões; dano 20 em contato a menos de 58 px; recuperação até 2 s. Não muda a direção para perseguir o jogador durante o avanço.

O dano inimigo compartilha imunidade de 0,65 s. A esquiva impede dano. Projéteis percorrem pequenos passos de até aproximadamente 4 px para não atravessar obstáculos entre frames. Movimentação também é subdividida. O movimento junto a obstáculos ainda é deslizamento local, sem pathfinding: inimigos podem prender em layouts complexos.

## Como a sprite entra no código
A lógica decide estado, posição, alvo e momento de dano. O desenho escolhe a célula da folha correspondente. Trocar o visual não deve mudar alcance ou vida.

As folhas do Coveiro são lidas em 9 colunas e 4 direções (frente, costas, esquerda, direita). O esqueleto usa provisoriamente a folha lateral 5 x 5; o mago usa a referência 4 x 4. Os efeitos usam 4 x 2. São grades proporcionais às dimensões das imagens, ainda sem atlas final recortado e normalizado. O tamanho de desenho é 80 px para inimigos e 150 px para o chefe; isso não significa que o personagem visível terá exatamente essa altura, pois há margens nos quadros.

Limitações visuais conhecidas: margens variáveis, possíveis cortes da pá erguida, alinhamento dos pés e escala entre quadros. Esqueleto e mago ainda não têm seleção final de poses nas quatro direções; o esqueleto reutiliza a folha lateral e o mago reutiliza linhas provisórias. É necessário conferir cada folha no editor e definir metadados de frame, duração e pivô. Não usar a imagem inteira como colisor. A ordenação atual separa inimigos antes/depois do jogador pela altura dos pés; não é ordenação completa de todos os objetos.

## Arquitetura desta versão
- clearing.h/c: contém dados de mapa, carregamento, desenho e colisão. InitCemetery reutiliza a interface da clareira; evita reescrever armas e movimento agora. Em uma próxima refatoração, renomear Clearing para WorldMap e separar configurações de cada fase.
- enemy.h/c: estados dos inimigos, projéteis limitados a 32 simultâneos, atributos e apresentação do cemitério. O grupo tem sete posições; as três extras ficam desativadas na clareira.
- game.c: transição, preservação do jogador, entrada de debug e desenho dos efeitos.
- assets/: cópias de execução. design/: originais e referências preservados.
- tests/enemy_audit_test.c: verifica regras anteriores e regressões da nova fase, sem abrir janela.

## Selos e progressão: próxima integração necessária
A narrativa aprovada continua válida: derrotar abre selamento; acerto liberta; erro ressuscita como corrompido. Esta entrega ainda usa o fluxo antigo de morte e cura opcional, sem esse sistema. Não confundir morte do chefe com conclusão narrativa da fase.

Proposta de implementação: adicionar estado AGUARDANDO_SELO após a morte e um componente independente SealChallenge com expressão, resposta correta e alvo. E próximo ao corpo abre o desafio. Recomenda-se pausar combate e projéteis durante a leitura; isso precisa de decisão do grupo. Acerto marca LIBERTO; erro restaura vida e ativa variante corrompida. Definir limite de novas tentativas e bônus da corrupção antes de programar para evitar ciclos infinitos.

O motor lógico deve avaliar uma árvore de operadores NOT, AND e OR a partir de valores booleanos; a interface recebe o resultado, não calcula a regra. Os desafios precisam de testes próprios para precedência e negação. A proteção do anjo deve ser controlada por estado da run, com uso único e até três selamentos protegidos. Ainda faltam fechar limiar de vida baixa e forma de ativação. Ultimate e selo final do necromante continuam fora desta entrega.

## Etapas para o grupo de quatro pessoas
1. Game design + código de integração: validar tamanho do jogador, navegação, alvos e avisos; ajustar bases com F3; registrar parâmetros aprovados. Entrega: passagem clareira → cemitério navegável, sem atravessar objetos ou nascer bloqueado.
2. Programação de inimigos: finalizar orientação e animações, separação entre inimigos, linha de visão dos magos e retorno da pá. Entrega: ataques legíveis, projéteis coerentes e chefe que não prende nos obstáculos.
3. Arte + animação + apoio no código: normalizar quadros e pivôs, corrigir cortes, exportar atlas e definir tabelas de animação junto ao programador. Entrega: sprites estáveis em movimento e ataques, sem saltos de escala.
4. Sistemas de lógica + QA: implementar motor e fluxo de selos, integração com corrupção e proteção, validar regras e fazer testes de regressão. Entrega: vitória dependente de selamento correto, com falha reproduzível e recuperável conforme regra aprovada.

Trabalhar em incrementos integrados: primeiro navegação e escala; depois um esqueleto; depois um mago; depois os três ataques do chefe; por fim selos e progressão. A presente base permite testar os comportamentos juntos, mas cada etapa ainda precisa de aceite visual e de gameplay.

## Critérios de aceite e roteiro de teste manual
- Iniciar nova partida, entrar por E e por F7: jogador aparece no topo do cemitério, arma/vida mantidas, tiros anteriores limpos.
- Testar cada túmulo e a capela com F3; corrigir colisores que não coincidam com as bases; verificar saída direita acessível.
- Testar mago de longe e de perto: aproxima, recua e atira sem projétil atravessar parede.
- Derrotar os seis inimigos: chefe só recebe dano e começa o ciclo depois disso.
- Raízes: ficar no aviso causa dano; sair ou esquivar na ativação evita dano.
- Pá: atinge uma vez, colide com parede e expira; retorno só será aceito quando implementado.
- Investida: direção fixa, aviso antes de avançar, para nas colisões e permite contra-ataque na recuperação.
- Testar as três armas, dano letal, ESC/menu/nova partida e encerramento sem falha de carregamento.
- Validar visual em jogo: sprites, transparência, pés, tamanho e avisos. Esse aceite ainda está pendente.

Validação automática executada: compilação CMake/Ninja e CTest, incluindo formação dos grupos, invulnerabilidade inicial do chefe, sequência inicial de raízes, preparação não interrompida por dano, esquiva e intervalo de imunidade. Não substitui teste interativo.

## Integra��o com as atualiza��es da equipe
As ondas, recompensas, chefe e checkpoint ampliado da clareira foram preservados. O grupo compartilhado suporta 24 posi��es; o cemit�rio usa as sete primeiras. Seus comportamentos ficam em src/cemetery_enemy.c, acessados pela interface de enemy.c. O controlador de encontros da clareira � suspenso durante o cemit�rio para n�o criar ondas ou recompensas da clareira nessa fase. Continuar restaura o checkpoint da clareira; salvar a fase do cemit�rio continua pendente. A descri��o antiga de checkpoint apenas de arma foi superada pelo sistema novo da equipe.

## Correções de fluxo e navegação — 06/11/2026
A passagem agora exige ENCOUNTER_CLEAR e etapa 7: cinco ondas concluídas e Guardião selado. O atalho F7 foi removido. As instruções anteriores de passagem livre/F7 estão superadas.

Game Over agora é tratado pelo fluxo principal para ambos os mapas: pausa a simulação e oferece REPETIR (clique, Enter ou R). Repetir cria uma nova tentativa na primeira onda da clareira, com a arma atual e atributos iniciais, e substitui o checkpoint. Não há botão de saída nesta tela.

Inimigos nascem com folga de sete pixels em relação às colisões. A movimentação compartilhada recupera posições embutidas e procura tangentes nas bordas irregulares quando o avanço direto é bloqueado. É navegação local, sem planejamento global de caminho. Testes cobrem recuperação de posição, movimento junto a limite e desvio de obstáculo; falta validar visualmente nos pontos relatados pelo jogador.

## Apresentação e cinco ondas totais
O ciclo agora tem quatro hordas (6, 9, 12 e 15 inimigos) e uma quinta onda exclusiva do Guardião. A recompensa especial ocorre após a quarta horda. Cada horda começa com aviso ONDA N por 1,6 s. A quinta começa com apresentação de 3 s: fundo escurecido, faixa inclinada, nome e sprite animada do mini-boss, inspirada na referência enviada. Combate fica suspenso durante as apresentações. O módulo presentation.h/c permite reutilizar tempo, nome de mapa e sprite em outros mapas; ainda aplicado apenas na clareira.
Checkpoint atualizado para PIF-CLEARING-4, com migração dos checkpoints da versão anterior. Saída liberada após o selo do mini-boss na quinta onda. Testes verificam o ciclo de quatro hordas, pausa das apresentações, chefe na quinta e conclusão. Preview renderizado: build/audit/boss_intro.png.

## Cemitério com ondas e colisões desenhadas — 06/11/2026
Os 15 retângulos aproximados foram substituídos pelo contorno externo e 20 polígonos de lápides/túmulos seguindo as novas marcações. O corredor superior do portão e a abertura lateral direita permanecem acessíveis. O modo F3 mostra os polígonos efetivos, sem o antigo retângulo de limite que já não representa a arena.

O cemitério usa agora o mesmo controlador de encontros da clareira: ondas 1–4 com 6, 9, 12 e 15 inimigos, avisos, pausa durante apresentação/recompensas/selos e recompensa especial antes da quinta onda. Especiais têm tonalidade violeta, investida e selo obrigatório; os magos disparam bolas verdes. O Coveiro aparece sozinho na onda 5, com os três ataques existentes. Após derrotá-lo, é necessário acertar seu selo para concluir; aparece CEMITERIO LIMPO. A vila continua fora desta versão.

A introdução usa um retrato próprio em pixel art ameaçador do Coveiro, com pá reta na vertical, em assets/coveiro_intro_pixel_pa_reta.png. As sprites de combate da pá não foram redesenhadas por essa alteração de retrato.

O checkpoint PIF-RUN-5 salva o mapa além dos atributos e etapa; formatos anteriores são lidos como clareira. Entrar no cemitério preserva vida, arma e melhorias, inicia onda 1 e salva essa fase. Continuar carrega o mapa correto. Repetir após Game Over inicia uma nova tentativa na clareira.

Verificações: build e os dois testes passaram, incluindo o ciclo completo das duas fases, quantidades, recompensas, selos, mapa do checkpoint, nascimento livre, colisões da capela/lápides, passagem direita e apresentação do Coveiro. Previews renderizados em build/audit/cemetery_collision_updated.png e cemetery_boss_intro.png. Ainda é necessário teste interativo das novas bordas.
