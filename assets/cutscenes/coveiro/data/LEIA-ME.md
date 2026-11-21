# O Coveiro das Raízes

A cena avança em 43 s. Ela inicia após aceitar a recompensa da quarta onda do Cemitério. Espaço ou Enter avança, P pausa e Esc pula para a arena. No F1, selecione Cemitério, onda 5 e Iniciar onda selecionada. Os atalhos de fase do boss entram direto no combate. Carregar um checkpoint já na quinta onda também entra direto.

A sequência em `sequencia.ini` usa nove PNGs originais e a faixa `take-1-take-1.mp3` fornecida, copiada como `audio/trilha.mp3`. Os onze planos cobrem as dez cenas do roteiro, com despertar e resistência em dois quadros. A trilha segue contínua, com níveis de mixagem graduais entre presente, flashback e retorno. A fala do Coveiro é legendada.

O motor compartilhado em `guardian_cutscene.c/.h` desenha a sequência. `encounter.c` e `game.c` cuidam do gatilho e da retomada do combate, e `dev_tools.c` integra a prévia. `guardian_cutscene_test.c` confere carregamento, duração, avanço automático e pulo.

A abertura reutiliza o plano distante com um enquadramento no protagonista. Os anexos não incluem planos exclusivos dos corpos, da pá erguida nem camadas separadas de capa e raízes. A cartela usa o plano próximo, com pulso roxo e tremor, seguido de fade para a arena. Uma transição de câmera contínua até o top-down depende de arte ou animação dedicada. A faixa é única, com fades de ganho, sem três arranjos independentes.

## Áudio

| Efeito | Autor e fonte | Licença |
| --- | --- | --- |
| Vento, sino e energia | PIF, sons procedurais de `tools/prepare_guardian_aftermath.py` | Acervo do projeto |
| Raízes, ruptura de solo e impacto final | Kenney, [Impact Sounds](https://kenney.nl/assets/impact-sounds) e [Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds), edições do PIF | CC0, atribuição dispensada |
| Silêncio, pá arrastada e escavar | PIF, montagens de vento, sino e do efeito de raízes acima | Componentes do PIF e Kenney CC0 |

Os arquivos específicos de origem e as licenças do Kenney estão em `boss_clareira/audio/CREDITOS-SFX.md` e nos textos CC0 ao lado dele. Foram reaproveitadas as cópias já verificadas do projeto. Gravações dedicadas de tecido, passos, raspagem, terra e sussurros ficam como opções de refinamento.
