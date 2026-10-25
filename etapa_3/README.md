# Etapa 3

A etapa 3 corresponde ao desenvolvimento do produto: a fabricação das primeiras peças físicas da Pulseira SysCare e a primeira integração entre a pulseira, o aplicativo e o servidor. Nesta etapa, a placa de circuito impresso foi fabricada, a carcaça foi impressa em 3D, o *firmware* passou a transmitir o protocolo SysCare a partir da pulseira real e o sistema ganhou o aviso remoto por *push*. Este documento resume cada frente; o detalhamento está nos documentos indicados ao final de cada seção.

---

## Desenvolvimento de *Hardware*

A placa de circuito impresso (PCI) foi produzida a partir do esquemático da Etapa 2, pelo processo de transferência do desenho e corrosão do cobre. O *layout* foi impresso e transferido para a placa; a corrosão removeu o cobre das regiões não protegidas, formando as trilhas; e o lixamento ajustou o tamanho e os cortes laterais da placa para o encaixe na carcaça, como mostra a Figura 1. As próximas atividades são a inspeção das trilhas, o teste de continuidade elétrica e a montagem dos componentes.

<div align="center">
  <img src="./img/corroido_pcb.png" alt="Placa de circuito impresso após a corrosão" width="40%">
  <img src="./img/lixando_pcb.png" alt="Adequação da placa por lixamento" width="40%">
  <p>Figura 1 - Placa de circuito impresso após a corrosão e adequação por lixamento</p>
</div>

> 📁 **Documentação de *Hardware*:** layout e etapas de fabricação em [Hardware](./hardware/README.md)

---

## Desenvolvimento de *Firmware*

O *firmware* da pulseira é desenvolvido para a XIAO nRF52840 [1], sobre o Zephyr RTOS e o nRF Connect SDK [2]. A estratégia adotada é validar cada módulo isoladamente antes de integrá-los: primeiro o anúncio BLE, depois a comunicação com o acelerômetro IIM-42351 [3] e, por fim, a aplicação integrada.

O módulo de anúncio BLE já transmite o pacote de 14 bytes do protocolo SysCare [4]. O anúncio é iniciado uma única vez e atualizado a cada evento, sem interromper a transmissão. Como o acelerômetro ainda está em teste, os eventos são simulados: um *heartbeat* a cada 10 s e uma queda a cada 60 s, com a bateria decrescendo para exercitar o aplicativo. Nessa versão de testes, os quatro bytes finais, que o protocolo reserva à assinatura HMAC [5], foram ocupados por um identificador de queda. Como o servidor confere essa assinatura, as quedas são recusadas, ponto a ser resolvido na integração. O *firmware* de referência do anúncio, revisado nesta etapa, já calcula a assinatura e serve de base para isso.

> 📁 **Documentação do *Firmware*:** [Firmware](./firmware/README.md) · **Protocolo BLE:** [ble_payload.md](./software/api/docs/ble_payload.md)

---

## Desenvolvimento de *Software*

O código do aplicativo e da API foi trazido da Etapa 2 para esta etapa, e o sistema ganhou o aviso remoto por *push*, pelo Firebase Cloud Messaging [6]. Até então, o alerta chegava aos responsáveis pela sirene do celular que ouviu a pulseira e pelo SMS enviado por esse celular. Com o *push*, a API notifica os celulares de todos os responsáveis vinculados à pulseira, mesmo longe dela, e o aplicativo toca a mesma sirene do alarme local. A mensagem identifica o anúncio de origem, para que o celular que já ouviu a pulseira não alarme duas vezes.

A integração com a pulseira real também exigiu ajustes. O aplicativo passou a procurar explicitamente o anúncio BLE legado, formato usado pela pulseira [7], e o *firmware* de referência do anúncio foi corrigido para atualizar os dados transmitidos e calcular a assinatura com a interface criptográfica da versão atual do nRF Connect SDK.

> 📁 **Documentação de *Software*:** [Software](./software/README.md) · [Aplicativo](./software/app/README.md) · [API](./software/api/README.md)

---

## Desenvolvimento mecânico

A carcaça projetada na Etapa 2 foi fabricada por impressão 3D em ABS, com o filamento e o processo disponíveis no IFSC, como mostra a Figura 2. A peça permitiu verificar fisicamente as dimensões externas, a região reservada à placa, à bateria e ao acelerômetro, e os elementos de fixação da pulseira de 18 mm. A impressão 3D mantém a possibilidade de ajustar a geometria rapidamente durante a montagem dos componentes.

<div align="center">
  <img src="./img/mecanica_estrutura_3D.png" alt="Carcaça da Pulseira SysCare impressa em 3D" width="40%">
  <img src="./img/mecanica_perfil_pulseira.png" alt="Carcaça montada com a pulseira de 18 mm" width="40%">
  <p>Figura 2 - Carcaça impressa em 3D e montagem com a pulseira</p>
</div>

> 📁 **Documentação de Mecânica:** fabricação e montagem do *case* em [Mecânica](./mechanics/README.md)

---

## Testes

As validações desta etapa estão resumidas na Tabela 1.

<div align="center">

| Área | Teste | Resultado |
|---|---|---|
| **Mecânica** | Impressão da carcaça e montagem com a pulseira | Geometria e fixação verificadas fisicamente |
| ***Hardware*** | Fabricação da PCI | Trilhas formadas; continuidade e montagem pendentes |
| ***Firmware*** | Anúncio com eventos simulados, conferido no console da placa | Pacote transmitido e atualizado a cada evento |
| **Integração** | Pulseira, celular Android e responsável, em 22/10/2026 | Alarme no celular e SMS ao responsável; queda recusada pela API por falta de assinatura |
| ***Push*** | Alerta assinado gerado pelo *script* de simulação da API | Alerta recebido no celular do responsável, com a sirene |
| **Automatizados** | 47 testes do aplicativo e 27 da API | Todos aprovados |

</div>

<div align="center">
  <p>Tabela 1 - Testes realizados na Etapa 3</p>
</div>

No teste de integração, apresentado na Figura 3, o celular que ouviu a pulseira exibiu a queda simulada com o impacto e a bateria transmitidos, tocou a sirene e enviou o SMS, recebido pelo responsável com o link do mapa. Em seguida, a API recusou o evento, porque a versão de testes do *firmware* não assina o anúncio, e o aplicativo desligou a sirene e informou a recusa. É o comportamento esperado: sem a assinatura, o servidor não distingue a pulseira de um rádio que imite o anúncio. A cadeia completa, da pulseira ao *push*, será validada quando o *firmware* integrado passar a assinar os anúncios.

<div align="center">
  <img src="./img/teste_ble_e_aplicativo.png" alt="Teste de integração entre a pulseira, o aplicativo e o responsável" width="60%">
  <p>Figura 3 - Teste de integração: à direita, o alarme no celular que ouviu a pulseira; à esquerda, o SMS recebido pelo responsável</p>
</div>

## Referências (links/datasheets/livros)

**Firmware e protocolo BLE**

- [1] [SEEED STUDIO. Getting Started with Seeed Studio XIAO nRF52840 Series](https://wiki.seeedstudio.com/XIAO_BLE/)

- [2] [NORDIC SEMICONDUCTOR. nRF Connect SDK - Documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)

- [3] [TDK INVENSENSE. High-performance 3-Axis SmartIndustrial™ Accelerometer MEMS Device for Industrial Applications (IIM-42351)](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/accelero/data_sheet/ds-000441-iim-42351-typ-v1.2.pdf)

- [4] [BLUETOOTH SIG. Core Specification Supplement - Advertising and Scan Response Data Format](https://www.bluetooth.com/specifications/specs/core-specification-supplement/)

- [5] [KRAWCZYK, H.; BELLARE, M.; CANETTI, R. RFC 2104 - HMAC: Keyed-Hashing for Message Authentication](https://datatracker.ietf.org/doc/html/rfc2104)

**Software**

- [6] [GOOGLE. Firebase Cloud Messaging - Documentation](https://firebase.google.com/docs/cloud-messaging)

- [7] [ANDROID DEVELOPERS. ScanSettings.Builder - setLegacy](https://developer.android.com/reference/android/bluetooth/le/ScanSettings.Builder)
