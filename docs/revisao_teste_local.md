# Revisão durante teste local — 06/11/2026

O jogo foi aberto antes das correções desta revisão. Sua partida permaneceu intacta; apenas os executáveis de teste foram recompilados. As mudanças abaixo entram na próxima compilação do JogoPiF.

## Problemas encontrados e corrigidos
- Cemitério: dano de raízes, investida, pá, projéteis verdes e ataque comum ignorava o atributo de poder. Agora o dano usa esse atributo e projéteis guardam o dano do momento do disparo, inclusive após corrupção por erro no selo.
- Cemitério: a velocidade calculada para a onda ficava sem uso. Movimento de perseguidores e magos passou a respeitar a velocidade da entidade.
- Ataques próximos: distância sozinha permitia atingir através de lápides/pedras. Agora golpes comuns, investidas, katana e contato da Bíblia verificam se o caminho até o alvo está livre. Ataques mágicos de área mantêm sua regra de área.

## Validação
- Verificação de sintaxe e avisos nos arquivos principais: C99, -Wall, -Wextra, -Wshadow, sem avisos após explicitar inicializadores de recompensas.
- enemy_audit e clearing_encounter passaram.
- Regressões adicionadas para dano aumentado por poder, dano guardado no projétil, velocidade efetiva e bloqueio de ataque através de obstáculo.
- Os testes de encontros percorrem as ondas, recompensas e selamentos nos dois mapas, incluindo o checkpoint com mapa.

## Limitações a observar jogando
- Navegação dos inimigos ainda usa desvio local; não há cálculo de caminho global. Cantos estreitos e contornos côncavos exigem validação interativa.
- A pá do Coveiro ainda não retorna: desaparece ao atingir obstáculo/jogador ou expirar.
- A saída para a vila ainda não carrega outro mapa.
- Esta revisão não equivale a uma garantia de ausência de bugs nem substitui o teste interativo das sprites e colisões.
