# Software — Definição dos sistemas a serem implementados (FW, BLE, App e API)

Nesta seção é apresentada a definição funcional dos quatro subsistemas de software do SysCare — o firmware embarcado (FW), o enlace de comunicação sem fio (BLE), o aplicativo móvel (App) e o serviço de retaguarda (API) — a partir do diagrama de blocos e do diagrama de casos de uso apresentados na [Etapa 1](../README.md). O objetivo é estabelecer, ainda na fase de concepção, **qual subsistema é responsável por cada requisito**, quais são os contratos de interface entre eles e como se dá o caminho completo do alerta, desde o impacto detectado no punho até a notificação recebida pelo familiar ou responsável.

A divisão foi conduzida por um princípio de projeto: **manter na pulseira apenas o que precisa estar na pulseira**. Cada função mantida no dispositivo vestível consome bateria e ocupa espaço em uma carcaça já dimensionada para 42 × 52 mm (ver [Mecânica](../mechanics/README.md)). Tudo o que puder ser resolvido no smartphone — que tem tela, alto-falante, GPS, conexão à Internet e uma bateria recarregada diariamente — deve ser resolvido no smartphone.

## 1. Visão geral dos subsistemas

| Subsistema | Onde executa | Tecnologia definida | Responsabilidade principal |
|---|---|---|---|
| **FW** — Firmware | Microcontrolador da pulseira (SoC nRF52, ARM Cortex-M4) | nRF Connect SDK / Zephyr RTOS [1] | Aquisição inercial, algoritmo de detecção de queda, botão de emergência, sinalização local e montagem do anúncio BLE |
| **BLE** — Enlace | Rádio 2,4 GHz entre pulseira e smartphone | Bluetooth Low Energy, *advertising* não-conectável [2] | Transporte do evento de emergência para qualquer celular ao alcance |
| **App** — Aplicativo | Smartphone do usuário e dos responsáveis | Flutter (Android como plataforma-alvo) | Escuta do anúncio BLE, alarme local imediato, geolocalização, repasse à API e interface de gestão |
| **API** — Serviço | Servidor Linux com acesso à Internet | Python + FastAPI [3] | Cadastro, validação e deduplicação dos eventos, histórico e notificação dos responsáveis |

Os subsistemas correspondem, respectivamente, às Camadas 2, 3 e 4 do diagrama de blocos da Figura 3 da Etapa 1, tendo os responsáveis como Camada 5.

## 2. Alocação de requisitos entre os subsistemas

A tabela a seguir explicita em qual subsistema cada requisito funcional é implementado. Ela evita a duplicação de responsabilidades — em especial no ponto mais sensível do projeto, a emissão do alarme — e serve de base para o planejamento das etapas seguintes.

| Requisito | FW | BLE | App | API |
|---|:---:|:---:|:---:|:---:|
| Monitorar movimentos continuamente | ● | | | |
| Detectar queda automaticamente | ● | | | |
| Permitir acionamento manual (botão de emergência) | ● | | | |
| Permitir cancelar o alerta antes do envio | ● | | | |
| Sinalizar localmente ao usuário (LED, som, vibração) | ● | | ○ | |
| Transmitir o evento ao smartphone | | ● | | |
| Autenticar a origem do evento | ● | ● | | ● |
| Emitir alarme sonoro imediato no celular | | | ● | |
| Obter a localização do ocorrido | | | ● | |
| Registrar o histórico de eventos | | | | ● |
| Notificar familiares e responsáveis remotos | | | ○ | ● |
| Reenviar o alerta enquanto ninguém confirmar | | | | ● |
| Cadastrar pulseira, usuário e responsáveis | | | ○ | ● |

● responsabilidade principal · ○ participação secundária

## 3. Firmware (FW)

O firmware executa sobre o nRF Connect SDK, distribuição da Nordic Semiconductor baseada no Zephyr RTOS, que já integra a pilha Bluetooth certificada, o gerenciamento de energia e o subsistema de armazenamento não volátil necessários ao projeto [1]. A escolha se apoia na integração nativa entre rádio e aplicação em um mesmo SoC, o que dispensa um transceptor externo e favorece as restrições de volume interno da carcaça.

O comportamento do firmware é definido como uma **máquina de estados finitos**, coerente com a estratégia de detecção levantada no [Estado da Arte](../EstadoDaArte.md) — IMU de seis eixos associada a uma FSM que combina aceleração resultante, velocidade angular e inclinação pós-impacto:

1. **Repouso / monitoramento.** A IMU opera em modo de detecção de movimento (*wake-on-motion*), acordando o microcontrolador apenas quando o limiar de aceleração é ultrapassado. Este é o estado predominante e o que determina a autonomia do dispositivo.
2. **Análise.** Ao ultrapassar o limiar, o firmware avalia a sequência característica da queda: queda livre (redução da aceleração resultante), impacto (pico), reorientação do tronco e imobilidade subsequente.
3. **Pré-alarme.** Confirmado o padrão, a pulseira sinaliza localmente (buzzer, vibração e LED) e abre uma janela de aproximadamente 10 segundos para cancelamento pelo usuário. Esse intervalo elimina o falso positivo na origem, antes de incomodar qualquer responsável — decisão que atende diretamente ao caso de uso *Cancelar sinalização* da Figura 2 da Etapa 1.
4. **Emergência.** Encerrada a janela sem cancelamento, o firmware monta o pacote de 14 bytes, assina-o e passa a anunciá-lo em intervalo curto.
5. **Pós-alerta.** O anúncio agressivo é sustentado por alguns minutos e depois retorna ao intervalo de repouso.

Duas exigências adicionais recaem sobre o firmware. A primeira é o **contador de eventos persistente**: o campo `seq` do pacote precisa ser gravado em memória não volátil a cada evento, pois um reinício que o zere faria os eventos seguintes serem recusados pela regra de anti-replay da API. A segunda é a **guarda da chave simétrica** gerada no cadastro da pulseira, usada para assinar cada anúncio.

O firmware também emite periodicamente um anúncio de presença (*heartbeat*) contendo o nível de bateria, o que permite ao sistema distinguir "nada aconteceu" de "a pulseira parou de funcionar".

## 4. Comunicação BLE

O enlace entre a pulseira e o smartphone é o ponto do sistema em que uma decisão de protocolo tem efeito direto sobre a autonomia. Definiu-se o uso de **advertising não-conectável** em vez de conexão BLE mantida.

Uma conexão exige varredura, estabelecimento, descoberta de serviços e pareamento, além de eventos periódicos de manutenção mesmo quando nada ocorre. O anúncio não-conectável, ao contrário, liga o rádio por poucos milissegundos, transmite nos três canais de anúncio e desliga. Para um evento único e urgente, é a alternativa mais econômica — e tem a vantagem de alcançar **simultaneamente todos os celulares próximos**, e não apenas um aparelho previamente pareado.

O pacote é transportado no campo *Manufacturer Specific Data* do anúncio, com 14 bytes úteis, folgados dentro dos 31 bytes disponíveis no anúncio legado, o que preserva a compatibilidade com aparelhos mais antigos:

| Campo | Bytes | Descrição |
|---|:---:|---|
| `version` | 1 | Versão do protocolo |
| `ble_id` | 4 | Identificador único da pulseira |
| `event_type` | 1 | Teste, queda, botão de pânico, imobilidade ou bateria baixa |
| `seq` | 2 | Contador monotônico do evento |
| `battery_pct` | 1 | Nível de bateria |
| `impact_dg` | 1 | Pico de aceleração em décimos de *g* |
| `signature` | 4 | Assinatura HMAC-SHA256 truncada sobre os 10 primeiros bytes |

Como a transmissão é aberta, o protocolo precisa resolver dois problemas que a conexão resolveria sozinha:

- **Autenticidade.** Sem assinatura, qualquer aparelho com um aplicativo genérico de BLE poderia anunciar uma queda em nome de uma pulseira alheia. O alerta falso repetido é uma falha grave em um sistema de emergência, pois treina o responsável a ignorar a notificação. A pulseira assina cada anúncio com uma chave simétrica compartilhada com o servidor [4]; o celular apenas repassa os bytes, sem conhecer a chave.
- **Unicidade.** O anúncio é repetido por dezenas de segundos e pode ser ouvido por vários celulares. Todos irão reportá-lo, e cabe à API consolidar os relatos em um único alerta (Seção 6).

Os intervalos de anúncio são definidos por estado: cerca de 2 s em repouso, 100 ms durante os primeiros 30 s da emergência — maximizando a chance de o celular capturar o anúncio já na primeira varredura — e 500 ms nos minutos seguintes, cobrindo o caso do responsável que chega depois. Os valores de consumo associados deverão ser medidos com instrumentação apropriada nas etapas de prototipagem.

Para a etapa de desenvolvimento do produto está prevista a evolução para o padrão **broadcast + conexão**: durante a emergência a pulseira também se anuncia como conectável, permitindo que o aplicativo, após tratar o alerta, confirme o recebimento (para a pulseira interromper o anúncio agressivo e poupar bateria) e baixe a janela de aceleração em torno do impacto, material necessário para calibrar o detector.

## 5. Aplicativo (App)

O aplicativo será desenvolvido em Flutter, o que permite uma base de código única para Android e iOS, com o Android como plataforma-alvo do protótipo. Ele atende a dois papéis distintos, ambos previstos no diagrama de casos de uso:

**Como receptor do alerta**, o aplicativo do celular que ouviu o anúncio BLE dispara o alarme local **antes de qualquer chamada de rede**: notificação em tela cheia, som de sirene em canal de alta prioridade e vibração. Em seguida obtém a localização por GPS e repassa o evento à API. Este é o alerta mais importante do sistema, por ser o único que funciona sem conexão à Internet.

**Como interface de gestão**, o aplicativo permite criar a conta do responsável, cadastrar a pulseira, vincular a lista de responsáveis com ordem de prioridade, consultar o histórico de eventos, confirmar o atendimento de um alerta ("estou indo") e encerrá-lo, classificando-o como atendido ou como falso positivo.

Duas exigências não funcionais recaem sobre o aplicativo: a **execução em segundo plano**, para que a varredura BLE continue com a tela apagada, e a **fila de reenvio offline**, para que a falha de conexão com o servidor não interrompa o alerta local nem descarte o evento.

## 6. API e a notificação aos responsáveis

A API é um serviço em Python com o framework FastAPI, executado em um servidor com acesso à Internet — **nenhuma parte dela roda na pulseira**. O FastAPI foi escolhido por gerar automaticamente a especificação OpenAPI, o que fornece documentação verificável das interfaces para o relatório do projeto.

São responsabilidades da API:

- **Cadastro e autenticação** de usuários, pulseiras e responsáveis, com geração da chave simétrica gravada no firmware.
- **Validação** da assinatura e do contador do evento recebido, rejeitando pacotes forjados ou retransmitidos.
- **Deduplicação** dos relatos: como vários celulares podem reportar o mesmo anúncio, os relatos são consolidados por pulseira e número de evento dentro de uma janela de tempo, produzindo **um alerta e uma notificação** — aproveitando, ainda assim, a localização de qualquer um dos relatos que a tenha trazido.
- **Notificação dos responsáveis** por push, em ordem de prioridade.
- **Escalonamento**: notificação entregue não significa socorro a caminho. Enquanto nenhum responsável confirmar o atendimento, o alerta é reenviado em intervalos regulares, por um número limitado de rodadas.
- **Histórico e registro de entregas**, guardando quem foi avisado, por qual canal e com qual desfecho.

O canal de notificação remota definido é o **push via Firebase Cloud Messaging (FCM)** [5], por três razões: custo zero e sem limite prático de mensagens, entrega em segundos mesmo com o aplicativo fechado — mensagens de alta prioridade atravessam o modo de economia de bateria do Android [6] — e ausência de qualquer processo de habilitação comercial.

A notificação aos responsáveis foi organizada em camadas, de modo que a falha de uma não comprometa o alerta:

| Camada | Canal | Custo | Funciona sem Internet? | Papel |
|---|---|---|---|---|
| 1 | Alarme local no celular que ouviu o BLE | Zero | **Sim** | Alerta primário, imediato |
| 2 | Push FCM aos demais responsáveis | Zero | Não | Alcança quem está distante |
| 3 | SMS e ligação nativos, pelo plano do usuário | Zero | **Sim** | Reserva quando o servidor está inacessível |
| 4 | Canais externos adicionais | Variável | Não | Extensão opcional |

As camadas 1 e 2 são as definidas como obrigatórias para o produto. A camada 3 depende de permissões sensíveis na loja de aplicativos, concedidas mediante justificativa para aplicativos de emergência [7]. A camada 4 permanece como extensão: a arquitetura do serviço de notificação prevê a inclusão de novos canais como implementações de uma mesma interface, sem alteração da lógica de alerta.

## 7. Fluxo completo do alerta

```
  Pulseira                Smartphone                  Servidor            Responsáveis
     |                         |                          |                     |
  [queda]                      |                          |                     |
     |-- pré-alarme local      |                          |                     |
     |   (janela p/ cancelar)  |                          |                     |
     |-- anúncio BLE assinado ->|                         |                     |
     |                         |-- ALARME LOCAL           |                     |
     |                         |   (som + tela + vibra)   |                     |
     |                         |-- obtém GPS              |                     |
     |                         |------ evento + local --->|                     |
     |                         |                          |-- valida assinatura |
     |                         |                          |-- deduplica relatos |
     |                         |                          |------- push ------->|
     |                         |                          |                [notificação]
     |                         |<---------- "estou indo" -----------------------|
     |                         |                          |-- encerra reenvio   |
```

Caso a etapa de envio ao servidor falhe por ausência de conexão, o alarme local já ocorreu, o evento é enfileirado para reenvio e a camada 3 pode ser acionada pelo próprio aparelho.

## 8. Definições consolidadas e desdobramentos

| Item | Definição da Etapa 1 | Desdobramento previsto |
|---|---|---|
| Ambiente de firmware | nRF Connect SDK / Zephyr | Implementação e medição de consumo |
| Estratégia de detecção | IMU de 6 eixos com FSM | Calibração de limiares com dados do protótipo |
| Enlace | Advertising BLE não-conectável, pacote de 14 bytes assinado | Evolução para *broadcast + conexão* |
| Aplicativo | Flutter, alarme local prioritário | Publicação e testes de campo |
| Serviço | Python + FastAPI | Migração do banco e implantação em servidor |
| Notificação | Push FCM em camadas | Avaliação de canais adicionais |

## Referências (links/datasheets/livros)

- [1] [NORDIC SEMICONDUCTOR. nRF Connect SDK — Documentation](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.4.2/nrf/getting_started/modifying.html#configure-application)

- [2] [BLUETOOTH SIG. Core Specification Supplement — Advertising and Scan Response Data Format](https://www.bluetooth.com/specifications/specs/core-specification-supplement/)

- [3] [RAMÍREZ, Sebastián. FastAPI — Documentation](https://fastapi.tiangolo.com/)

- [4] [KRAWCZYK, H.; BELLARE, M.; CANETTI, R. RFC 2104 — HMAC: Keyed-Hashing for Message Authentication](https://datatracker.ietf.org/doc/html/rfc2104)

- [5] [GOOGLE. Firebase Cloud Messaging — Documentation](https://firebase.google.com/docs/cloud-messaging)

- [6] [ANDROID DEVELOPERS. Optimize for Doze and App Standby](https://developer.android.com/training/monitoring-device-state/doze-standby)

- [7] [GOOGLE PLAY. Use of SMS or Call Log permission groups](https://support.google.com/googleplay/android-developer/answer/10208820)

<div align="center">

[⬅️ Etapa 1](../README.md) | [Mecânica ➡️](../mechanics/README.md)

</div>
