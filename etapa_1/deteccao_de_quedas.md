## Estado da arte da detecção de queda

Como ja mencionado anteriormente, a queda é uma das principais causas de lesão e óbito por causas externas entre idosos, e suas consequências se agravam quando o socorro não é acionado rapidamente. Por isso, dispositivos vestíveis capazes de detectar automaticamente uma queda e emitir um alerta em tempo real tornaram-se uma das soluções mais estudadas em saúde digital e sistemas embarcados, com o acelerômetro triaxial ocupando papel central por ser barato, de baixo consumo energético e por dispensar infraestrutura externa, ao contrário de câmeras ou radares. Este texto resume o estado da arte em detecção de queda por acelerômetro, reunindo diferentes artigos e pesquisas e aproveitando das tecnologias mais recentes do mercado.

### Introdução e fundamentos do sinal de queda

Independentemente da técnica empregada, quase todos os métodos de detecção partem da mesma observação sobre o sinal de magnitude do vetor de aceleração (Signal Vector Magnitude, SVM) de um corpo em queda. Tanto Jia (2009)[1] quanto por Tseng, Huang e Kau (2025)[2] descrevem 4 fases características ao analisar a curva do vetor de aceleração no tempo:
1. Repouso: SVM estável em torno de 1g;
2. Perda de equilíbrio: corpo entra em queda parcialmente livre e o SVM cai para perto de 0 g;
3. Impacto: pico de aceleração tipicamente acima de 1,4 g;
4. Imobilidade pós-impacto: sinal se estabiliza numa orientação diferente da inicial, evidenciando a mudança de postura.

<div align="center">
  <img src="img/fall_signals.png" alt="Figura 1 - Sinais de queda" width="75%">
  <p>Figura 1 - Padrão típico do sinal de queda e as quatro fases do evento.</p>
</div>

A principal dificuldade prática não é detectar o pico de impacto isoladamente, já que atividades como sentar rapidamente, saltar ou descer escadas também o geram, mas sim diferenciar a queda real dessas atividades “quase-queda”, onde se concentra a maior parte dos falsos positivos (TSENG; HUANG; KAU, 2025; AZIZ et al., 2017)[3]. Abaixo podem ser visualizados alguns exemplos de sinais de atividades rotineiras, retirados do artigo da Analog Devices, Jia (2009)[4].

<div align="center">
  <img src="img/walking_signals.png" alt="Figura 2 - Sinais de caminhada" width="70%">
  <p>Figura 2 - Sinais de caminhada em acelerometria.</p>
</div>

<div align="center">
  <img src="img/sitting_standing_signals.png" alt="Figura 3 - Sinais de sentar e levantar" width="70%">
  <p>Figura 3 - Sinais de sentar e levantar em acelerometria.</p>
</div>

## Referências (links/datasheets/livros)

- [1] []()

- [2] []()

- [3] []()

- [4] []()