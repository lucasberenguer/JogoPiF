# Jogo PIF — base de design

Este documento descreve o jogo pretendido. O protótipo já implementa o ciclo da clareira: cinco ondas, escolhas de bônus, selos, primeiro chefe e salvamento no início das etapas. Valores e conteúdos marcados como **a definir** serão decididos no protótipo.

## Premissa

Jogo PIF é um roguelite de ação 2D com visão superior. O jogador atravessa uma floresta amaldiçoada, enfrenta criaturas e usa desafios de lógica para romper os selos que as prendem ao necromante. A apresentação `Jogo_PIF_Apresentacao (1).pdf`, versão 0.1, é a referência narrativa. A estrutura de ondas, chefes e progressão abaixo incorpora as decisões posteriores da conversa.

## Fluxo de uma run

1. O menu oferece **Iniciar run**, **Continuar** e **Configurações**. Iniciar uma nova run avisa antes de substituir uma run inacabada.
2. A nova run começa com uma cutscene que pode ser pulada. Dentro do jogo, o jogador escolhe a arma inicial: bíblia, katana ou arma de fogo.
3. O jogador atravessa, nesta ordem, **clareira**, **ruínas da trilha** e **pátio profanado**. Cada área tem cinco ondas e um chefe. A quantidade de inimigos cresce gradualmente.
4. Após as ondas 1 a 4 de cada área, o jogador escolhe um bônus entre três opções. Após a onda 5, a escolha normal é substituída por um bônus mais forte para a arma equipada ou para os status do personagem. Em seguida começa o chefe.
5. Selar o chefe concede o selo da área e abre a passagem seguinte. Os três selos abrem a passagem para o **santuário partido**.
6. Antes do santuário, a **capela** oferece uma pausa narrativa sem combate.
7. No santuário, o jogador enfrenta o necromante e resolve o selo final. Vitória ou derrota encerra a run.

## Ondas, inimigos e selos

- Inimigos surgem fora da área visível da tela e entram na arena. Cada onda contém inimigos comuns e **pelo menos um especial**; a quantidade de especiais é sorteada e aumenta gradualmente ao longo das ondas.
- A fase e a onda determinam a maior parte da quantidade e força dos inimigos. Os status do jogador causam apenas um ajuste leve na força deles, para preservar a sensação de evolução da build.
- Os comuns têm dois papéis básicos: um perseguidor corpo a corpo e um atirador à distância. São as mesmas criaturas ao longo das fases, com aparência progressivamente mais corrompida.
- Os especiais compartilham um avanço rápido anunciado antes do golpe. Sua aparência também evolui com a corrupção de cada fase.
- Inimigos comuns não exigem selo. Quando um especial ou chefe cai, o jogador se aproxima do corpo e pressiona **E** para iniciar o desafio de selo. O combate pausa durante a proposição.
- Erro no selo faz o especial ou chefe voltar mais forte. Ao derrotá-lo novamente, o jogador precisa tentar o selo outra vez. Esse ciclo não tem limite fixo para especiais e chefes de área: termina ao acertar o selo ou perder a run.
- Uma onda só termina quando todos os seus inimigos forem eliminados, incluindo especiais que voltaram após erros de selo.
- Qualquer inimigo pode deixar um item de cura no chão; o jogador precisa coletá-lo. Chance e quantidade de cura: **a definir no protótipo**.
- Cada chefe de área tem sprite maior e padrão de ataque próprio. Identidade visual e ataques específicos: **a definir depois da base jogável**.

## Armas, controles e habilidades

| Ação | Controle | Regra |
| --- | --- | --- |
| Mover | WASD | Movimento livre na arena. |
| Atacar | Botão esquerdo do mouse | Katana e arma de fogo atacam uma vez por clique, respeitando a cadência. |
| Esquivar | Espaço ou botão direito do mouse | Impulso curto com recarga. Com a Bíblia, a esquiva usa Espaço. |
| Ativar ultimate | Q | Disponível quando a barra estiver cheia. |
| Interagir/selar | E | Usado perto de um corpo ou objeto interativo. |

Uma única arma fica equipada por vez:

- **Bíblia:** gira automaticamente ao redor do personagem. Um clique direito, quando a habilidade está pronta, acelera sua rotação por um período.
- **Katana:** golpe em área acionado pelo jogador.
- **Arma de fogo:** tiros em linha reta na direção apontada pelo jogador.

A arma é escolhida no início da run e pode ser trocada em escolhas de bônus entre ondas. O cartão de troca mostra qual arma será recebida. Cada arma conserva seus upgrades enquanto não está equipada, dentro da mesma run. Cada uma pode receber **três upgrades**; depois sai das opções de upgrade.

A ultimate carrega ao causar dano e abre uma proposição de **verdadeiro ou falso** ao ser ativada. O combate pausa enquanto o jogador responde. Acertar produz a versão forte; errar ainda libera uma versão fraca:

- Bíblia: raio de giro muito maior e rotação muito mais rápida.
- Katana: supergolpe em área com salto.
- Arma de fogo: grande rajada em linha reta.

Cadências, duração dos efeitos e comportamento da esquiva sem direção pressionada: **a definir no protótipo**.

## Bônus e builds

- Após cada uma das ondas 1 a 4, aparecem três opções sorteadas. Categorias iniciais: troca de arma, upgrade da arma, dano e vida. A troca tem chance alta, mas não garantida, de aparecer.
- Cada troca de arma concede também um bônus aleatório, cujo efeito fica oculto até a escolha e é mostrado depois. Bônus de troca persistem e podem se acumular durante a run, inclusive quando o mesmo tipo é sorteado mais de uma vez. Não há teto separado de acúmulo; as oportunidades de troca são limitadas pelo número de ondas.
- Após a onda 5, o jogador escolhe entre um bônus forte da arma equipada e um bônus forte de status. Ele é separado dos três upgrades normais da arma e dura pelo restante da run.
- Cada tela de bônus, inclusive a anterior ao chefe, permite **um reroll gratuito** para sortear novas opções.
- O conteúdo e os valores de cada upgrade, bônus de troca e bônus forte: **a definir no protótipo**.

## Lógica e ajuda do anjo

- Ultimate e selos de inimigos especiais usam proposições de **verdadeiro ou falso**.
- Selos de chefes de área usam **simplificação lógica por múltipla escolha**: o jogador seleciona a expressão equivalente.
- O selo do necromante usa simplificações mais complexas.
- Se um golpe mataria o jogador, aparece automaticamente uma proposição como última chance. Acertar restaura **50% da vida máxima** e consome o único uso do anjo naquela run. Errar encerra a run. O uso e a vida restaurada são salvos imediatamente.
- O selo final do necromante permite **três tentativas**. Cada erro faz o necromante voltar mais forte e exige nova luta antes da próxima tentativa. Acertar vence. Após três erros, o anjo completa o ritual se seu poder ainda estiver disponível; caso já tenha sido usado, a run termina em derrota.
- Operadores, geração das questões e escala exata de dificuldade: **a definir no protótipo**.

## Derrota, progressão e continuidade

- Após perder, o jogador escolhe **um** bônus de vida máxima, dano ou velocidade para a próxima tentativa. Bônus escolhidos após derrotas consecutivas se acumulam. Uma vitória zera esse acúmulo.
- Upgrades das armas e bônus obtidos dentro da run recomeçam do zero em uma nova run.
- **Continuar** retoma o começo da última onda alcançada, com a build e a vida salvas naquele ponto. Inimigos derrotados e curas coletadas durante uma onda não concluída são reiniciados.
- A escolha de bônus concluída salva o início da próxima onda. A escolha forte concluída salva o início do chefe. Um selo de chefe errado cria um novo ponto salvo para a revanche mais difícil.
- Fechar o jogo antes de escolher um bônus faz repetir a onda recém-concluída. Fechar durante uma proposição volta ao último ponto salvo.
- O uso do anjo e a vida restaurada por ele são persistidos imediatamente, mesmo que a onda em andamento seja reiniciada por **Continuar**.
- Detalhes de salvamento da cutscene, da capela e das telas finais: **a definir na implementação**.

## Escopo posterior

A apresentação contém material narrativo e visual adicional sobre o guardião, o necromante, as cinco áreas e a atmosfera da floresta. Os ataques de cada chefe, sprites finais, direção de áudio, valores de balanceamento e conteúdo exato das questões serão definidos depois de testar a base jogável. Os exercícios iniciais de raylib (cena e pincel de mouse) ainda precisam ser relacionados ao plano de implementação do PIF.
