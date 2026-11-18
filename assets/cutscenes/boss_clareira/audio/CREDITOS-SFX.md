# Efeitos da cutscene do Guardião

Fontes de Kenney, obtidas em 18/11/2026. Ambos os pacotes declaram licença Creative Commons Zero, CC0 1.0. As licenças distribuídas nos pacotes estão preservadas em `Kenney-Impact-CC0.txt` e `Kenney-SciFi-CC0.txt`.

- [Impact Sounds, de Kenney](https://kenney.nl/assets/impact-sounds)
- [Sci-fi Sounds, de Kenney](https://kenney.nl/assets/sci-fi-sounds)

Os WAVs usados no jogo são combinações dos efeitos originais com redução de altura, filtragem, pequenos atrasos, eco, fades e ajuste de pico. Formato PCM de 16 bits, estéreo, 44.100 Hz. As versões editadas preservam folga de pico antes do ganho de reprodução e usam o volume de efeitos das configurações.

| Arquivo no jogo | Duração | Fontes originais |
| --- | --- | --- |
| `ruptura_solo.wav` | 2,6 s | `lowFrequency_explosion_000.ogg`, `explosionCrunch_003.ogg`, `impactWood_heavy_000.ogg`, `impactMining_000.ogg` |
| `raizes.wav` | 4,0 s | `impactWood_heavy_000.ogg`, `impactWood_heavy_001.ogg`, `impactWood_heavy_002.ogg`, `impactMining_001.ogg`, `lowFrequency_explosion_000.ogg` |
| `selo_furia.wav` | 2,0 s | `lowFrequency_explosion_001.ogg`, `explosionCrunch_001.ogg` |
| `impacto_raizes.wav` | 2,5 s | `explosionCrunch_004.ogg`, `lowFrequency_explosion_001.ogg`, `impactMining_002.ogg`, `impactWood_heavy_002.ogg` |

Os nomes iniciados por `impactWood` e `impactMining` pertencem a Impact Sounds. Os nomes iniciados por `explosionCrunch` e `lowFrequency` pertencem a Sci-fi Sounds.

`audio_at` sincroniza o início em segundos a partir da entrada do enquadramento. `audio_gain` define a intensidade relativa de cada efeito, de 0 a 1. O impacto final começa junto ao corte de imagem. O efeito das raízes recebe um pequeno atraso para acompanhar a transição para o Guardião emergindo.
