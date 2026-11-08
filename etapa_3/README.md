# Etapa 3

A Etapa 3 corresponde ao desenvolvimento e à implementação do produto: a fabricação das primeiras peças físicas da Pulseira SysCare e a integração entre a pulseira, o aplicativo e o servidor. Nesta etapa, a placa de circuito impresso (PCI) foi fabricada, a carcaça foi produzida por impressão 3D e o *firmware* passou a detectar quedas com o acelerômetro IIM-42351 e a transmiti-las pelo protocolo SysCare. O sistema ganhou notificações remotas por *push*, e a API foi publicada na nuvem, levando os alertas aos responsáveis independentemente da rede local. O detalhamento está nos documentos indicados ao final de cada seção.

---

## Desenvolvimento de *Hardware*

A PCI foi produzida a partir do esquemático da Etapa 2: o *layout* foi transferido para a placa, a corrosão removeu o cobre excedente, formando as trilhas, e o lixamento ajustou as dimensões para o encaixe na carcaça. Em seguida, foram soldados os componentes necessários aos testes iniciais: o XIAO nRF52840, o acelerômetro IIM-42351 e a bateria. Essa montagem permitiu validar os componentes principais antes da integração (Figura 1).

<div align="center">
  <img src="./img/corroido_pcb.png" alt="Placa de circuito impresso após a corrosão" width="30%">
  <img src="./img/lixando_pcb.png" alt="Adequação da placa por lixamento" width="30%">
  <img src="./img/soldagem.png" alt="Soldagem dos componentes na placa" width="35%">
  <p>Figura 1 - Etapas de fabricação da placa de circuito impresso</p>
</div>

> 📁 **Documentação de *Hardware*:** *layout*, fabricação e montagem em [Hardware](./hardware/README.md)

---

## Desenvolvimento de *Firmware*

O *firmware* é desenvolvido para a XIAO nRF52840 [1], sobre o Zephyr RTOS e o nRF Connect SDK [2], e organizado em camadas (HAL, *drivers*, serviços e aplicações), que separam o acesso aos periféricos da lógica da pulseira. Os módulos foram validados individualmente antes da integração: primeiro a comunicação BLE e depois o acelerômetro IIM-42351 [3].

Na validação do acelerômetro, foram verificados a identificação do dispositivo, a leitura da aceleração, a FIFO, as interrupções e o recurso APEX de detecção de queda livre (Figura 2).

<div align="center">
  <img src="./img/registro_quedas_terminal.png" alt="Registro de quedas no terminal" width="70%">
  <img src="./img/teste_terminal_acelerometro_dados.png" alt="Dados do acelerômetro durante os testes" width="70%">
  <p>Figura 2 - Testes de detecção de queda livre e leitura dos dados do acelerômetro</p>
</div>

No *firmware* integrado, a queda livre detectada pelo acelerômetro é transmitida pelo protocolo SysCare [4]; sem queda, a pulseira envia um *heartbeat* a cada 10 s. O pacote de 14 bytes leva a versão do protocolo, o identificador da pulseira, o tipo de evento, o número de sequência, a bateria, o impacto e, nos quatro últimos bytes, a assinatura HMAC-SHA256 [5], calculada com a chave gerada no cadastro da pulseira. A cada queda, a sequência avança e fica gravada na memória flash, mesmo após um reinício. O campo de impacto ainda usa um valor fixo, pois o acelerômetro informa a duração e a altura da queda livre.

> 📁 **Documentação do *Firmware*:** detalhes em [Firmware](./firmware/README.md)

---

## Desenvolvimento de *Software*

O código do aplicativo e da API foi trazido da Etapa 2, e o sistema ganhou o aviso remoto por *push*, pelo Firebase Cloud Messaging [6]. Até então, o alerta chegava pela sirene e pelo SMS do celular que ouviu a pulseira; com o *push*, a API notifica os celulares de todos os responsáveis vinculados à pulseira, mesmo longe dela, e o aplicativo toca a mesma sirene do alarme local.

A integração com a pulseira real exigiu ajustes: o aplicativo passou a procurar explicitamente o anúncio BLE legado, formato usado pela pulseira [7], e o *firmware* de referência passou a calcular a assinatura com a interface criptográfica atual do nRF Connect SDK. Por fim, a API foi publicada em uma máquina virtual do Azure, obtida pelo programa para estudantes [8], com HTTPS e endereço fixo, como mostra a Figura 3.

<div align="center">
  <img src="./img/servidor_nuvem.svg" alt="Organização do servidor na nuvem" width="90%">
  <p>Figura 3 - Organização do servidor na nuvem e caminho do alerta até o responsável</p>
</div>

> 📁 **Documentação de *Software*:** [Software](./software/README.md) · [Aplicativo](./software/app/README.md) · [API](./software/api/README.md) · [Servidor na nuvem](./software/api/docs/servidor_nuvem.md)

---

## Desenvolvimento mecânico

A carcaça projetada na Etapa 2 foi fabricada por impressão 3D em ABS, no IFSC (Figura 4). A peça permitiu verificar fisicamente as dimensões externas, o espaço destinado à PCI, à bateria e ao acelerômetro e os elementos de fixação da pulseira de 18 mm, além de identificar ajustes para as próximas etapas.

<div align="center">
  <img src="./img/mecanica_estrutura_3D.png" alt="Carcaça da Pulseira SysCare impressa em 3D" width="40%">
  <img src="./img/mecanica_perfil_pulseira.png" alt="Carcaça montada com a pulseira de 18 mm" width="53%">
  <p>Figura 4 - Carcaça impressa em 3D e montagem com a pulseira</p>
</div>

> 📁 **Documentação de Mecânica:** fabricação e montagem da estrutura em [Mecânica](./mechanics/README.md)

---

## Integração do sistema

Com as áreas desenvolvidas, a PCI, o acelerômetro, a bateria, a estrutura mecânica e o *software* foram reunidos no primeiro protótipo funcional da Pulseira SysCare (Figura 5), usado nos testes a seguir.

<div align="center">
  <img src="./img/prototipo.png" alt="Primeiro protótipo da Pulseira SysCare" width="45%">
  <p>Figura 5 - Primeiro protótipo da Pulseira SysCare</p>
</div>

## Testes do protótipo

No primeiro teste de integração, em 22/10, o celular que ouviu a pulseira tocou a sirene e enviou o SMS ao responsável, mas a API recusou a queda, porque aquela versão do *firmware* não assinava o anúncio; sem a assinatura, o servidor não distingue a pulseira de um rádio que a imite.

Em 04/11, com o *firmware* integrado assinando os anúncios, a cadeia completa funcionou (Figura 6): a queda detectada pelo acelerômetro foi aceita pelo servidor na nuvem, o *push* chegou aos responsáveis e um deles, em outra rede, confirmou o atendimento em menos de 30 s. Os quiques da pulseira no chão geraram três alertas para a mesma queda, ponto a ser tratado na próxima etapa.

<div align="center">
  <img src="./img/teste_pulseira_push.jpg" alt="Tela de alerta da queda detectada pela pulseira" width="35%">
  <p>Figura 6 - Teste de 04/11/2026: alerta da queda detectada pelo acelerômetro (<a href="./img/teste_pulseira_push.mp4">vídeo</a>)</p>
</div>

Em 05/11, com o protótipo montado, o teste foi repetido (Gif 1): a queda foi detectada pelo acelerômetro, a pulseira transmitiu o alerta por BLE, o aplicativo recebeu a mensagem e o responsável foi notificado por SMS, confirmando o funcionamento do protótipo completo.

<div align="center">
  <img src="./img/teste_queda.gif" alt="Teste de funcionamento do protótipo completo" width="80%">
  <p>Gif 1 - Teste de funcionamento do protótipo completo</p>
</div>

## Próximos passos - Etapa 4

Na Etapa 4, serão feitos os ajustes finais do projeto:

- ***Hardware*:** adicionar os componentes pendentes, validar o sistema completo e fabricar a placa na China.
- ***Firmware*:** implementar o botão de emergência e o LED e refinar a detecção de quedas.
- **Aplicativo:** otimizar a interface, melhorando a experiência e a usabilidade.
- **Mecânica:** fabricar a tampa, desenvolver a entrada de carregamento e fazer o acabamento da estrutura.

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

- [8] [MICROSOFT. Azure for Students](https://azure.microsoft.com/en-us/free/students)
