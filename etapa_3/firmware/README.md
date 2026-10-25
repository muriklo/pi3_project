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
    ├── acelerometro/
    │   └── src/
    │       └── main.c

```

## 2. Testes individuais

### 2.1 Testes do Bluetooth Low Energy

Esta etapa tem como objetivo validar a inicialização do Bluetooth, a transmissão de dados por advertising e o protocolo de comunicação SysCare, antes da integração com o acelerômetro.

#### Protocolo SysCare BLE

O payload possui 14 bytes, organizados da seguinte forma:

| Bytes | Campo | Descrição |
|---|---|---|
| `[0]` | Versão | Versão do protocolo (`0x01`). |
| `[1..4]` | ID da pulseira | Identificador de 32 bits do dispositivo. |
| `[5]` | Evento | Tipo de evento transmitido. |
| `[6..7]` | Sequência | Contador de mensagens de 16 bits. |
| `[8]` | Bateria | Valor do nível de bateria. |
| `[9]` | Impacto | Valor associado à intensidade do impacto. |
| `[10..11]` | ID da queda | Identificador de 16 bits da ocorrência. |
| `[12..13]` | Reservado | Espaço para futuras extensões. |

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

Esta etapa é dedicada ao desenvolvimento e à validação da comunicação com o acelerômetro **TDK IIM-42351**, componente utilizado para obter informações de movimento da pulseira.




### 3.3. Firmware integrado

A pasta `firmware/` contém a aplicação destinada a reunir os módulos desenvolvidos e validados individualmente.

O objetivo é integrar a aquisição de dados do acelerômetro à comunicação BLE, permitindo que os eventos detectados pela pulseira sejam transmitidos por meio do protocolo SysCare.


