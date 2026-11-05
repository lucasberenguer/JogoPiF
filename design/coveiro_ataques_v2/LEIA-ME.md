# Coveiro das Raízes — ataques v2

Folhas de referência visual geradas com IA; não integradas ao jogo.

## Personagem: 9 colunas x 4 linhas
Leitura de cada linha da esquerda para a direita. Direções: frente, costas, esquerda e direita.
1. ataque_1_raizes.png: sequência anterior preservada; preparação, pá contra o chão, raízes e recuperação.
2. ataque_2_pa_giratoria.png: preparação, arremesso, espera sem pá, captura e recuperação.
3. ataque_3_investida.png: preparação agachada, arrancada, avanço, frenagem e recuperação.

## Efeitos separados: 4 colunas x 2 linhas
Oito quadros, lendo a primeira linha e depois a segunda.
- efeito_raizes_area.png: nascimento, expansão e retração das raízes em área.
- efeito_raizes_linha.png: surgimento de espinhos direcionais.
- efeito_pa_giratoria.png: poses para o giro completo da pá; a trajetória é controlada pelo código.
- efeito_investida.png: poeira, rastro, impacto e dissipação.

## Preparação para implementação
Estas folhas não constituem animações finais validadas. Revisar consistência entre quadros, recortar e normalizar células, escala e pivôs. Corrigir margens e possíveis cortes nas poses com a pá erguida. Ajustar o giro da pá para evitar oscilação de tamanho. Conferir transparência e franjas no editor de sprites. Definir duração de cada quadro, momento de dano e colisores separadamente da imagem. Não houve teste dentro do jogo.

Direção visual: coveiro zumbificado, raízes mortas, pá enferrujada e possessão violeta. Referências fornecidas pelo usuário: especial do chefe da clareira e efeitos de raízes em linha e área. Os novos efeitos usam violeta para combinar com o Coveiro.
