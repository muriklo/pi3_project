# SysCare - Firmware da Pulseira

## 1. Visão geral

O firmware da pulseira SysCare está sendo desenvolvido para a placa **Seeed Studio XIAO nRF52840**, utilizando o nRF Connect SDK (NCS) 3.4.0 e o sistema operacional Zephyr RTOS.

O desenvolvimento está dividido em três partes:

- **advertising_ble**: testes e desenvolvimento da comunicação Bluetooth Low Energy (BLE), incluindo advertising e protocolo de dados.
- **acelerometro**: testes e desenvolvimento da comunicação com o acelerômetro IIM-42351 e avaliação dos recursos de detecção de queda.
- **firmware**: implementação integrada, reunindo a comunicação BLE e o acelerômetro em uma única aplicação.

A estratégia é validar cada componente individualmente antes de realizar a integração, facilitando a depuração e a identificação de problemas.

## 2. Estrutura do repositório

```text
etapa_3/
└── firmware/
    ├── README.md
    ├── CMakeLists.txt
    ├── prj.conf
    ├── src/
    │   └── main.c
    └── ...
    ├── advertising_ble/
    |   └── src/
    │       └── main.c
    │
    ├── acc_validation/
    │   └── src/
    │       └── main.c

```

## 2. Testes individuais

### 2.1 Testes do Bluetooth Low Energy

Esta etapa tem como objetivo validar a inicialização do Bluetooth, a transmissão de dados por advertising e o protocolo de comunicação SysCare, antes da integração com o acelerômetro.

#### Protocolo SysCare BLE

O payload possui 14 bytes, organizados da seguinte forma:

| Campo | Bytes | Descrição |
|---|---|---|
| `Versão `| [0] | Identificação da versão do protocolo |
| `ID da pulseira` | [1..4] | Identificador do dispositivo |
| `Evento` | [5] | Tipo de evento transmitido |
| `Sequência` | [6..7] | Identificador sequencial da queda |
| `Bateria` | [8] | Informação da bateria |
| `Impacto` | [9] | Informação de impacto |
| `Assinatura HMAC` | [10..13] | Assinatura de autenticação |

Os campos de 16 bits são transmitidos em little-endian.

#### Tipos de evento

| Código | Evento |
|---|---|
| `0x00` | Teste |
| `0x01` | Queda |
| `0x02` | Botão de pânico |
| `0x03` | Imobilidade |
| `0x04` | Bateria baixa |
| `0x05` | Heartbeat |

Os eventos de queda utilizados nesta etapa são simulados, não provenientes de uma detecção real pelo acelerômetro.

#### Funcionamento do advertising

O advertising é iniciado uma única vez com `bt_le_adv_start()`. Após a inicialização, os dados são atualizados por meio de `bt_le_adv_update_data()`.

O identificador `fall_id` permite que uma futura aplicação móvel diferencie uma nova queda das transmissões repetidas de uma ocorrência já sinalizada.

Após feito o código, foi feito um teste em conjunto com o aplicativo, que identificava a mensagem enviado via BLE e com isso conseguia fazer as atualizações necessárias, bem como avisar ao celular da vitima a queda ao servidor, enviando o SMS para a pessoa de suporte, é possível ver este teste feito na Figura 1.

<div align="center">
  <img src="../img/teste_ble_e_aplicativo.png" alt="" width="55%">
  <p>Figura 1 - Teste com código do BLE e integração com o Aplicativo</p>
</div>


### 3.2. Testes do acelerômetro

Esta etapa é dedicada ao desenvolvimento e à validação da comunicação com o acelerômetro **TDK IIM-42351**, componente utilizado para obter informações de movimento da pulseira. A validação foi feita de forma incremental, em cinco passos, antes da integração com o BLE:

1. **Identificação (WHO_AM_I):** leitura do registrador de identificação para confirmar que a comunicação com o sensor **[I2C / SPI]** está funcionando.
2. **Leitura em mg:** leitura das acelerações nos três eixos, convertidas para mg, conferindo os valores com a placa em repouso (módulo próximo de 1000 mg).
3. **Leitura em FIFO:** leitura das amostras pela FIFO do sensor, em vez de leitura direta dos registradores, reduzindo o trabalho do microcontrolador.
4. **Interrupções INT1/INT2:** configuração dos pinos de interrupção do sensor, permitindo que o nRF52840 seja avisado de um evento sem precisar ler o sensor continuamente.
5. **Queda livre (APEX):** uso do detector de queda livre do próprio sensor para identificar quedas e medir sua duração.

#### Detecção de queda livre

A detecção de queda é feita pelo recurso APEX do IIM-42351. Durante uma queda livre, a aceleração medida pelo sensor cai para perto de zero; ao final, o impacto causa um pico de aceleração. O sensor usa dois limiares para reconhecer essas duas fases e, ao concluir a detecção, sinaliza o evento por interrupção. Os parâmetros aplicados são mostrados na Tabela 1.

<div align="center">

| Parâmetro | Valor |
|---|---|
| Início (low-g) | 500 mg |
| Impacto (high-g) | 3000 mg |
| Altura | 13 a 201 cm |
| Debounce | 1000 ms |
| Saída | Duração e altura |

<p>Tabela 1 - Parâmetros da queda livre</p>
</div>

- **Início (low-g):** a queda livre começa quando a aceleração cai abaixo de 500 mg.
- **Impacto (high-g):** a queda termina quando o sensor registra uma aceleração acima de 3000 mg.
- **Altura:** faixa de alturas de queda considerada válida, de 13 a 201 cm. Quedas fora dessa faixa são descartadas.
- **Debounce:** após uma detecção, novos eventos são ignorados por 1000 ms, evitando contar a mesma queda mais de uma vez.

A saída do firmware é a **duração** da queda livre e a **altura estimada**, calculada pela relação da queda livre sem resistência do ar:


<div align="center">
h = (g · t²) / 2
</div>

#### Metodologia de teste

O teste consistiu em soltar a placa de alturas conhecidas sobre uma almofada, para proteger o hardware, e observar o resultado no terminal serial. Também foi verificado que movimentos que não são quedas, como sacudir a placa ou bater na mesa, **[não geraram eventos]**.

#### Resultados

A Figura XX mostra o registro do terminal. Foram detectadas 5 quedas livres, e o contador de quedas detectadas foi atualizado a cada evento.


<div align="center">

| Queda | Duração (ms) | Altura estimada (cm) |
|---|---|---|
| 1 | 256 | ~32 |
| 2 | 274 | ~37 |
| 3 | 330 | ~53 |
| 4 | 162 | ~13 |
| 5 | 192 | ~18 |

<p>Tabela 1 - Resultados dos dados de queda</p>

</div>

<div align="center">
  <img src="../img/registro_quedas_terminal.png" alt="Registro de quedas no terminal" width="70%">
  <img src="../img/teste_terminal_acelerometro_dados.png" alt="Dados do acelerômetro durante os testes" width="70%">

  <p>Figura 2 - Testes de detecção de queda livre e leitura dos dados do acelerômetro</p>
</div>

As alturas estimadas foram comparadas com as alturas reais de **[X, Y, Z cm]**, com erro de **[X]**. O módulo mostrou-se capaz de detectar a queda e medir sua duração, sendo o próximo passo integrá-lo ao advertising BLE, de modo que cada detecção gere um evento de queda no protocolo SysCare em vez de um evento simulado.


### 3.3. Firmware integrado

A pasta `firmware/` contém a aplicação que reúne os módulos desenvolvidos e validados individualmente: a comunicação BLE (seção 3.1) e a detecção de queda pelo acelerômetro (seção 3.2). Nesta etapa, a integração foi realizada, e os eventos de queda passaram a ser gerados por detecção real do sensor, e não mais simulados.

#### Funcionamento

A integração é baseada em interrupção, o que dispensa a leitura contínua do sensor (polling) e reduz o consumo, requisito importante para um dispositivo vestível:

1. O IIM-42351 detecta a queda livre pelo recurso APEX, com os parâmetros da Tabela 1, e sinaliza o evento no pino de interrupção **[INT1/INT2]**.
2. O nRF52840 recebe a interrupção por um callback de GPIO do Zephyr. Para manter a rotina de interrupção curta, ela apenas **[sinaliza uma thread / agenda um trabalho na work queue]**.
3. Fora do contexto da interrupção, o firmware lê o status do sensor, confirma a queda e obtém a duração e a altura estimada.
4. A aplicação monta o payload do protocolo SysCare BLE com o evento **Queda (`0x01`)**, incrementa o contador de sequência e o `fall_id`, e preenche o campo *Impacto* com **[o valor obtido do sensor]**.
5. O payload é publicado com `bt_le_adv_update_data()`, sem reiniciar o advertising, que continua ativo desde a inicialização.

Dessa forma, o `fall_id` identifica cada ocorrência: o aplicativo consegue distinguir uma nova queda das retransmissões da mesma ocorrência, e dispara o alerta ao servidor uma única vez por queda.

#### Resultados

Foi realizado um teste ponta a ponta com a pulseira, o aplicativo e o servidor: a queda detectada pelo acelerômetro gerou o advertising BLE com o evento de queda, o aplicativo identificou a mensagem e o alerta foi enviado à pessoa de suporte **[por SMS]**. A Figura XX mostra este teste.

<div align="center">
  <img src="../img/teste_integrado.png" alt="" width="55%">
  <p>Figura XX - Teste do firmware integrado: queda detectada e alerta recebido</p>
</div>

**[Descrever: quantas quedas foram testadas, tempo entre a queda e o alerta, se houve falha ou falso positivo.]**

#### Próximos passos

Estão previstos para as próximas etapas **[a definir: botão de pânico, evento de bateria baixa, heartbeat, otimização de consumo, ajuste dos limiares com mais ensaios]**, que já possuem códigos reservados no protocolo.

