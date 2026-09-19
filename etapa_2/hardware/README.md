# Hardware

Na etapa 2, a definição do hardware tem como objetivo detalhar a implementação da solução proposta, contemplando o desenvolvimento do esquemático, a elaboração da lista de componentes e a definição dos principais componentes eletrônicos do sistema. Entre os elementos considerados nesta etapa estão o microcontrolador (MCU), o acelerômetro e a bateria.

## Seleção do microcontrolador

Para a implementação do hardware, foram avaliadas diferentes plataformas de microcontroladores considerando os requisitos da aplicação, principalmente baixo consumo energético, conectividade Bluetooth Low Energy (BLE), capacidade de processamento, interfaces de comunicação com o acelerômetro e dimensões físicas compatíveis com a proposta da pulseira.

A comparação entre as principais alternativas consideradas é apresentada na Tabela abaixo.

| **Característica** | **XIAO nRF52840** |   **STM32WB55**  | **STM32WBA52** |     **ESP32-C3**    | **STM32G031** |
| :----------------- | :----------: | :--------------: | :------------: | :-----------------: | :-----------: |
| **Fabricante**     |    Nordic    |        ST        |       ST       |      Espressif      |       ST      |
| **CPU**            |  Cortex-M4F  | Cortex-M4F + M0+ |   Cortex-M33   |        RISC-V       |   Cortex-M0+  |
| **Frequência**     |    64 MHz    |      64 MHz      |     100 MHz    |       160 MHz       |     64 MHz    |
| **RAM**            |    256 KB    |      256 KB      |     128 KB     |        400 KB       |      8 KB     |
| **Flash**          |     1 MB     |     até 1 MB     |    até 1 MB    | depende da variante |   até 64 KB   |
| **Bluetooth LE**   |       ✓      |         ✓        |        ✓       |          ✓          |       ✗       |
| **Wi-Fi**          |       ✗      |         ✗        |        ✗       |          ✓          |       ✗       |
| **IEEE 802.15.4**  |       ✓      |         ✓        |       ✓*       |          ✗          |       ✗       |
| **USB**            |       ✓      |         ✓        |        ✗       |   USB Serial/JTAG   |       ✗       |
| **I²C**            |       ✓      |         ✓        |        ✓       |          ✓          |       ✓       |
| **SPI**            |       ✓      |         ✓        |        ✓       |          ✓          |       ✓       |
| **ADC**            |    12 bits   |      12 bits     |     12 bits    |       12 bits       |    12 bits    |
| **FPU**            |       ✓      |         ✓        |        ✓       |          ✗          |       ✗       |
| **Baixo consumo**  |       ✓      |         ✓        |        ✓       |          ✓          |       ✓       |


Entre as alternativas avaliadas, foi selecionado o **XIAO nRF52840** como plataforma para o desenvolvimento do primeiro protótipo. A escolha está relacionada não somente às características do microcontrolador nRF52840, mas principalmente à integração proporcionada pela placa XIAO, que reúne o microcontrolador, os recursos necessários para programação e interfaces de comunicação em uma plataforma de pequenas dimensões [1].

O XIAO nRF52840 apresenta dimensões reduzidas, conectividade Bluetooth Low Energy (BLE), interfaces de comunicação compatíveis com o acelerômetro e capacidade de processamento suficiente para executar o firmware responsável pelo monitoramento dos sensores e pela detecção de quedas. Essas características são compatíveis com os requisitos estabelecidos para o dispositivo vestível.

Outra vantagem da utilização do XIAO nRF52840 está relacionada ao desenvolvimento do primeiro protótipo. A utilização de uma placa já integrada permite validar a arquitetura eletrônica, o funcionamento do acelerômetro, o algoritmo de detecção de quedas e a comunicação BLE sem que seja necessário desenvolver inicialmente uma placa de circuito impresso dedicada para o microcontrolador.

O STM32WB55 e STM32WBA52 também apresentam características compatíveis com os requisitos da aplicação, incluindo processamento de baixo consumo, conectividade Bluetooth Low Energy e interfaces de comunicação. Além disso, a familiaridade da equipe com a plataforma STM32 poderia reduzir a curva de aprendizado durante o desenvolvimento do firmware. Entretanto, as alternativas disponíveis para desenvolvimento apresentam dimensões superiores às do XIAO nRF52840. A utilização direta do circuito integrado exigiria ainda o desenvolvimento de uma placa própria, incluindo os circuitos de alimentação, programação e comunicação necessários.

Dessa forma, o XIAO nRF52840 foi escolhido para o primeiro protótipo por combinar os recursos necessários à aplicação com uma plataforma compacta e já integrada. Essa escolha reduz a complexidade inicial do desenvolvimento e permite concentrar os esforços na validação da funcionalidade do sistema. Posteriormente, após a validação do protótipo, os circuitos necessários poderão ser incorporados a uma placa de circuito impresso própria, caso seja necessário desenvolver uma versão final mais compacta da pulseira.

## Seleção do acelerômetro

Para a seleção do acelerômetro, foram considerados principalmente o consumo de corrente, a presença de recursos de*detecção de queda livre (Free-fall Detection), a disponibilidade de interrupções para comunicação com o microcontrolador, as interfaces de comunicação e a disponibilidade do componente para desenvolvimento do protótipo.

A Tabela abaixo apresenta a comparação entre alguns dos acelerômetros avaliados durante a seleção.

| **Característica**                 |   **ADXL362**   |    **LIS2DW12**    |    **BMA400**   |        **IIM-42351**       |
| :--------------------------------- | :-------------: | :----------------: | :-------------: | :------------------------: |
| **Fabricante**                     |  Analog Devices | STMicroelectronics | Bosch Sensortec |       TDK InvenSense       |
| **Eixos**                          |        3        |          3         |        3        |              3             |
| **Resolução**                      |     12 bits     |       16 bits      |     12 bits     |           16 bits          |
| **Faixa**                          |    ±2/±4/±8 g   |   ±2/±4/±8/±16 g   |  ±2/±4/±8/±16 g |       ±2/±4/±8/±16 g       |
| **ODR máximo**                     |      400 Hz     |       1600 Hz      |      800 Hz     |           8000 Hz          |
| **Corrente ativa**                 | 1,8 µA @ 100 Hz |     < 1 µA (LP)    |   3,5–14,5 µA   | ~300 µA (full performance) |
| **Standby**                        |      10 nA      |        50 nA       |      160 nA     |           7,5 µA           |
| **Wake-up**                        |      270 nA     |       ~50 nA       |      800 nA     |       Wake-on-Motion       |
| **FIFO**                           |   512 amostras  |      32 níveis     |       1 KB      |            2 KB            |
| **Interface**                      |       SPI       |      SPI + I²C     |    SPI + I²C    |       I3C + I²C + SPI      |
| **Tensão**                         |    1,6–3,5 V    |     1,62–3,6 V     |    1,71–3,6 V   |         1,71–3,6 V         |
| **Detecção de queda livre**        |        —        |          —         |        —        |            **✓**           |
| **Interrupção**                    |        ✓        |          ✓         |        ✓        |              ✓             |
| **Disponibilidade para protótipo** |        —        |          —         |        —        |            **✓**           |

*Os valores apresentados são referentes às condições de operação indicadas na documentação de cada componente e devem ser considerados como referência para comparação, uma vez que o consumo depende do modo de operação e das configurações utilizadas.*

Apesar de existirem alternativas com consumo significativamente inferior, como o ADXL362, LIS2DW12 e BMA400, foi selecionado o IIM-42351 [2] para o primeiro protótipo. Essa escolha ocorre principalmente devido à disponibilidade de um módulo do componente já disponível para a equipe, permitindo iniciar o desenvolvimento sem a necessidade de projetar inicialmente uma placa específica para o acelerômetro.

Outro fator determinante é a familiaridade da equipe com o funcionamento do IIM-42351 e, principalmente, a disponibilidade do recurso de detecção de queda livre (Free-fall Detection). Esse recurso é particularmente relevante para a aplicação, pois a detecção de uma condição de queda pode ser realizada pelo próprio acelerômetro, gerando uma interrupção para que o microcontrolador realize o processamento posterior. Dessa forma, parte do processamento necessário para identificar uma possível queda pode ser realizada pelo próprio sensor, o que aumenta muito a economia de bateria.

Embora o consumo do IIM-42351 seja superior ao de algumas das alternativas analisadas, essa característica foi considerada aceitável para o primeiro protótipo, uma vez que a prioridade nesta etapa é validar o funcionamento da arquitetura, a detecção de quedas, a geração das interrupções e a comunicação entre o acelerômetro e o microcontrolador.

Após a validação do primeiro protótipo, poderá ser realizada uma nova avaliação do acelerômetro com foco na redução do consumo energético. Nesse estágio, sensores com menor consumo poderão ser considerados como alternativas para uma futura versão do dispositivo, caso apresentem os recursos de detecção de queda e integração necessários à aplicação.

Portanto, a seleção do IIM-42351 representa uma decisão de desenvolvimento voltada à validação inicial do sistema, considerando simultaneamente a disponibilidade do hardware, o conhecimento prévio da equipe e a presença de uma funcionalidade considerada fundamental para a aplicação, mesmo que existam alternativas com menor consumo energético.

 

## Referências (links/datasheets/livros)


- [1] [ZEPHYR PROJECT. Seeed XIAO BLE. Zephyr Project Documentation, 2026](https://docs.zephyrproject.org/latest/boards/seeed/xiao_ble/doc/index.html)

- [2] [High-performance 3-Axis SmartIndustrial™ Accelerometer MEMS Device for Industrial Applications](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/accelero/data_sheet/ds-000441-iim-42351-typ-v1.2.pdf)



