# Firmware

A partir desta etapa, será definida as soluções escolidas para o firmware. A primeira delas será a definição do algoritmo de detecção de quedas, a partir do acelerômetro IIM-42351.

## Definição do algoritmo de detecção de quedas

A detecção de quedas da Pulseira SysCare será baseada no acelerômetro IIM-42351 [1], utilizando o recurso interno de *Freefall Detection* para identificar o início de uma possível queda e gerar uma interrupção para o microcontrolador. Essa abordagem permite que o processamento principal permaneça em um estado de baixo consumo durante a maior parte do tempo, sendo acionado quando o acelerômetro identificar um evento compatível com queda.

O IIM-42351 possui um modo de operação de baixo consumo para os algoritmos APEX (Advanced Pedometer and Event eXecution). Conforme a configuração apresentada no datasheet, para uma frequência de saída do acelerômetro igual ou superior a 25 Hz, o algoritmo de *freefall detection* pode operar em *low power*. Dessa forma, será adotada inicialmente uma configuração de 25 Hz, reduzindo o consumo energético do sistema sem desabilitar a detecção de queda. Nessa condição, o DMP opera a 25 Hz, conforme a tabela de configuração apresentada pelo fabricante.

A detecção interna de *freefall* utiliza detectores de aceleração para identificar o início e o término de um período de queda livre. O evento é definido a partir de parâmetros de limiar e duração configuráveis no acelerômetro. Entre os parâmetros disponíveis estão o limiar de início da queda, o tempo mínimo de queda, o limiar de término e as durações mínima e máxima do evento. O próprio datasheet também relaciona a duração da queda livre à distância percorrida durante o evento:

$$
FF_{DISTANCE}=0,5\cdot9,81\cdot(FF_{DUR}\cdot DMP\_ODR_S)^2
$$

onde $FF_{DUR}$ representa a duração da queda em número de amostras e $DMP\_ODR_S$ corresponde ao período de amostragem do DMP.

### Funcionamento da detecção

O funcionamento proposto para o protótipo pode ser representado pela seguinte sequência:

**Acelerômetro em baixo consumo → detecção de queda livre → interrupção → processamento pelo MCU → verificação do impacto → verificação de imobilidade → confirmação da queda → envio do alerta**

Durante a operação normal, o IIM-42351 permanece monitorando continuamente a aceleração, utilizando o algoritmo interno de *Freefall Detection*. Quando os parâmetros configurados indicarem a ocorrência de uma queda livre, o acelerômetro gera uma interrupção, que é recebida pelo XIAO nRF52840.

A partir dessa interrupção, o microcontrolador passa a executar o processamento necessário para confirmar o evento. Como a detecção de queda livre isoladamente não é suficiente para diferenciar uma queda real de outros movimentos rápidos, será utilizada uma sequência de validação baseada no comportamento da aceleração:

1. **Estado normal:** aceleração próxima à condição de repouso, aproximadamente $1g$ considerando a magnitude do vetor de aceleração.
2. **Queda livre:** redução significativa da magnitude da aceleração, identificada pelo detector interno do IIM-42351.
3. **Impacto:** ocorrência de uma variação elevada de aceleração após o período de queda livre, utilizando um limiar definido no projeto.
4. **Imobilidade:** permanência do usuário com baixa variação de aceleração após o impacto.
5. **Confirmação:** caso as condições sejam satisfeitas, o evento será considerado uma queda e o sistema poderá iniciar o procedimento de alerta aos responsáveis.

Dessa forma, o acelerômetro é responsável pela detecção inicial e geração da interrupção, enquanto o microcontrolador é responsável pelo tratamento da interrupção e pela confirmação do evento, reduzindo a possibilidade de que movimentos cotidianos sejam interpretados como uma queda.

### Configuração inicial proposta

| Parâmetro | Configuração |
|---|---|
| **Acelerômetro** | IIM-42351 |
| **Accel ODR** | 25 Hz |
| **DMP ODR** | 25 Hz |
| **Freefall Detection** | Low Power |
| **Detecção inicial** | Freefall Detection interno |
| **Saída da detecção** | Interrupção para o MCU |
| **Processamento posterior** | Impacto + imobilidade |
| **MCU** | XIAO nRF52840 |

Essa configuração é particularmente interessante para o primeiro protótipo porque permite utilizar a funcionalidade de detecção de queda do próprio acelerômetro em Low Power a partir de 25 Hz, contribuindo para reduzir o consumo energético da pulseira. O XIAO nRF52840 permanece responsável pelo processamento do evento apenas quando uma interrupção é gerada.

## Arquitetura do firmware

Para detalhar a implementação do firmware da Pulseira SysCare, foi elaborado um diagrama de blocos relacionando os principais módulos de software, os periféricos internos do XIAO nRF52840 e os componentes externos do sistema. O diagrama apresenta também as principais interconexões utilizadas, incluindo a comunicação SPI entre o microcontrolador e o acelerômetro IIM-42351 e a linha de interrupção utilizada para sinalizar eventos de detecção.

A arquitetura foi organizada sobre o Zephyr RTOS / nRF Connect SDK, sendo dividida em módulos responsáveis pela detecção e confirmação de quedas, tratamento de interrupções, comunicação com o acelerômetro, gerenciamento de energia, armazenamento de dados, montagem dos pacotes e comunicação BLE.

Na detecção de quedas, o acelerômetro IIM-42351 realiza a detecção inicial de *freefall* e gera uma interrupção por meio da linha INT1. O XIAO nRF52840 recebe essa interrupção e executa a máquina de estados responsável pelo envio das mensagens via BLE.

<div align="center">
    <img src="../../img/diagrama_de_blocos_hardware.png" alt="Diagrama de blocos da arquitetura de hardware e firmware da Pulseira SysCare" width="65%">
    <p>Figura 1 - Diagrama de blocos da arquitetura de hardware e firmware da Pulseira SysCare.</p>
</div>


## Referências

- [1] [High-performance 3-Axis SmartIndustrial™ Accelerometer MEMS Device for Industrial Applications](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/accelero/data_sheet/ds-000441-iim-42351-typ-v1.2.pdf)