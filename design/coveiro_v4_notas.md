# Coveiro: movimento e pá, revisão local v4

Assets criados com imagegen integrado, com fundo transparente e referência de identidade em `assets/coveiro_throw_v3.png`:

- `assets/coveiro_walk_v4.png`: 12 colunas × 4 direções; ciclo de caminhada com passos, braços, roupa e raízes em movimento. Reproduzido a 18 quadros/s.
- `assets/coveiro_throw_v4.png`: 12 colunas × 4 direções; seis poses de preparação antes de 0,8 s e seis de lançamento/recuperação até 1,65 s.
- `assets/coveiro_pa_vento_v4.png`: oito colunas × duas linhas; pá enferrujada girando com vento, seguida por uma linha de poeira. Reproduzido a 24 quadros/s.

Prompts de produção: manter o coveiro zumbificado, olhos violetas, barba castanha, raízes, camisa marfim rasgada, colete e pá de cabo reto com ponta arredondada; criar atlas uniformes sem texto, grade ou sobreposição. Caminhada: doze fases contínuas de passada nas quatro direções. Arremesso: doze fases de transferência de peso, antecipação, lançamento e recuperação com mão vazia. Projétil: oito rotações em incrementos de 45 graus da mesma pá, com rastros de vento e partículas violetas discretas; segunda linha com oito fases de poeira do chão.

Velocidades da pá: ida 620/740 unidades por segundo nas fases 1/2 (antes 430/500); retorno 660/760. Área de contato: raio de 43 (antes 29). Contato limitado a um acerto por trajeto de ida ou volta, respeitando esquiva e invulnerabilidade. Movimento subdividido em passos de até quatro unidades para evitar atravessar obstáculos/personagem em alta velocidade.

Aviso de 0,8 s preservado. Três direções de aviso na fase 2. Recuperação encurtada, pequeno avanço no golpe e balanço contínuo do corpo durante as ações. Não corresponde a uma medição do FPS do motor: trata da cadência e quantidade de poses da animação.

Validar jogando: leitura do aviso, dificuldade das três pás, sensação de peso, transição entre caminhada e ataques. Alterações locais, sem commit/push.
