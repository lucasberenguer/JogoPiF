# Auditoria após o push — 06/11/2026

Versão de gameplay auditada: e6b2322. Base remota anterior: fd74ec9. Antes do commit, HEAD e origin/main apontavam para a mesma base, sem divergência. Não foi necessário mesclar outro commit. Não foram encontrados marcadores de conflito ou dependências de imagens locais não versionadas.

## Resultado
Nenhum bug crítico ou quase crítico foi identificado no escopo das verificações executadas. Isso não garante ausência de falhas em execução interativa.

## Evidências
- Compilação completa Debug a partir de uma pasta de build nova, out/review, com raylib 5.5.
- Ciclos completos da clareira e do cemitério nos testes: quatro hordas, mini-boss na quinta, recompensas, selos, conclusão e checkpoint com mapa.
- Checkpoints vazios, truncados, arma/mapa/etapa inválidos, vida inconsistente, atributos não finitos e upgrade fora do limite foram rejeitados sem modificar parcialmente o estado de saída.
- Leitura e migração dos formatos antigos PIF-CLEARING-2, 3 e 4 passaram.
- Análise estática GCC -fanalyzer com -Wall, -Wextra e -Wshadow em menu, encounter, cemetery_enemy, clearing e game: sem diagnósticos.
- Conferência das imagens carregadas por caminhos literais: todas presentes no índice Git.
- Conectividade dos dois mapas em malha de 8 pixels, usando a colisão efetiva dos pés e checagem de meio de passo: uma região navegável em cada mapa, nenhuma célula livre isolada. Clareira: 13493 células; cemitério: 12631. Isso avalia geometria, não prova que a IA local encontrará toda rota.

## Correções já incluídas no push principal
Dano do cemitério passou a respeitar o poder de corrupção; velocidade dos inimigos passou a usar os atributos de onda; ataques próximos verificam obstáculos entre atacante e alvo. Regressões dessas regras passaram.

## Limitações conhecidas
- IA usa desvio local, sem pathfinding global: validar jogando os cantos estreitos e perseguição ao redor dos túmulos.
- A pá do Coveiro ainda desaparece no impacto/fim do tempo; não retorna ao chefe.
- Vila ainda não implementada.
- Salvamento atual escreve diretamente o checkpoint; não foi simulado desligamento do computador durante a escrita. Não constitui erro reproduzido nesta auditoria.
- Não foi realizado um teste humano completo de todas as armas e de todas as posições nas bordas.

Os builds de auditoria e seus checkpoints ficam em out/review; a partida já aberta e o checkpoint do jogador em build não foram substituídos.
