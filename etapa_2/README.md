# Etapa 2

A etapa 2 é destinada ao desenvolvimento e à definição da arquitetura do primeiro protótipo da Pulseira SysCare, abrangendo as áreas de hardware, firmware, mecânica e aplicativo. Nesta etapa, foram selecionados os principais componentes do sistema, definidos os softwares e ferramentas utilizados no desenvolvimento, estabelecida a arquitetura de hardware e firmware, incluindo a comunicação com o acelerômetro e o BLE, e desenvolvido o projeto mecânico da pulseira e de seu encapsulamento. Também foram consideradas as restrições de dimensões, massa, integração dos componentes e montagem do protótipo, buscando uma solução compatível com os requisitos funcionais e físicos definidos para o projeto.


## Desenvolvimento

### Componentes selecionados

Após a análise dos requisitos e a comparação entre as alternativas disponíveis, foram definidos os principais componentes para a implementação do primeiro protótipo. A seleção considerou tanto as características elétricas e funcionais dos componentes quanto fatores relacionados à disponibilidade, facilidade de integração e restrições dimensionais da pulseira.

A Tabela 1, apresenta os componentes selecionados para o protótipo e suas respectivas funções no sistema. Todo o estudo e seleção, bem como os benchmarks feitos estão localizados no *README.md* na pasta *Hardware*.

> Nesta seção, foram detalhadas as escolhas dos componentes para o protótipo inicial, bem como o esquemático da PCI. Todas as considerações e comparações podem ser vistas no documento.
>
> 📁 **Documentação de *Hardware*:** Acesse a pasta: [Hardware](./hardware/README.md)


<div align="center">

| **Categoria** | **Componente** | **Figura** | **Função** | **Critério de seleção** |
|:---:|:---:|:---:|:---|:---|
| **Microcontrolador** | Seeed Studio XIAO nRF52840 | <img src="./img/xiaonrf52840.png" width="200"> | Processamento, controle do sistema e comunicação BLE | Plataforma compacta, BLE integrado e facilidade de prototipagem |
| **Acelerômetro** | TDK InvenSense IIM-42351 | <img src="images/iim42351.png" width="120"> | Aquisição de aceleração e detecção de queda | Módulo disponível, conhecimento prévio e Free-fall Detection |
| **Bateria** | Bateria recarregável 541112 | <img src="./img/battery541112.png" width="120"> | Alimentação do sistema | Dimensões reduzidas e disponibilidade |
| **Comunicação** | Bluetooth Low Energy | <img src="./img/ble.png" width="120"> | Comunicação com o smartphone | Baixo consumo e integração ao XIAO nRF52840 |

</div>

<div align="center">
  <p>Tabela 1 - Definição dos componentes do sistema</p>
</div>

A escolha dos componentes apresentada na Tabela 1, corresponde à configuração utilizada para a validação do primeiro protótipo. Após a validação da arquitetura e do funcionamento do sistema, poderão ser avaliadas alternativas com foco na redução do consumo energético e na integração dos componentes em uma placa dedicada.

---
## Definição dos softwares necessários

Para o desenvolvimento da Pulseira SysCare, foram definidos softwares específicos para cada uma das etapas do projeto, abrangendo o desenvolvimento do firmware (FW), projeto eletrônico (HW), desenvolvimento mecânico (MEC) e aplicativo (APP). A seleção considera as ferramentas utilizadas para programação, simulação, desenvolvimento das placas eletrônicas e modelagem da estrutura física do dispositivo.

| **Etapa**            | **Software**                    | **Aplicação no projeto**                                                                              |
| -------------------- | ------------------------------- | ----------------------------------------------------------------------------------------------------- |
| **FW - Firmware**    | Visual Studio Code + nRF Connect NORDIC | Desenvolvimento, compilação e gerenciamento do firmware do XIAO nRF52840.                             |
| **HW - Hardware**    | Altium Designer                 | Desenvolvimento do esquemático e projeto da placa de circuito impresso (PCI).                         |
| **MEC - Mecânica**   | FreeCAD                 | Modelagem tridimensional e desenvolvimento da estrutura mecânica da pulseira e de seu encapsulamento. |
| **APP - Aplicativo** | Flutter (Dart) + Visual Studio Code + Android Studio | Desenvolvimento do aplicativo em Flutter no VS Code; o Android Studio fornece o Android SDK e o emulador usados para compilar e testar o APK. |

--- 

## Desenvolvimento mecânico

Na etapa 2, foi desenvolvido o projeto mecânico da Pulseira SysCare a partir dos requisitos dimensionais, ergonômicos e de fabricação definidos anteriormente. Foram estabelecidas as dimensões da carcaça, o espaço disponível para os componentes eletrônicos, o sistema de fixação da pulseira, o material de fabricação e uma estimativa inicial da massa do conjunto.

### Dimensões e integração mecânica

Para o primeiro protótipo, foi definida uma carcaça compacta, com dimensões externas de 42 mm de largura** e 52 mm de comprimento total, incluindo as regiões de fixação da pulseira. O espaço interno foi definido considerando um diâmetro interno de 37 mm e uma região útil de 35 mm para acomodação dos componentes eletrônicos.

<div align="center">

| Parâmetro | Valor |
|---|---:|
| Largura externa da carcaça | **42 mm** |
| Comprimento total da carcaça | **52 mm** |
| Diâmetro interno da carcaça | **37 mm** |
| Diâmetro interno útil | **35 mm** |
| Largura da pulseira | **18 mm** |
| Área interna útil de referência | **962,11 mm²** |
| Sistema de fixação | **Pino de mola** |
</div>

<div align="center">
  <p>Tabela XX - Dimensões mecânicas definidas</p>
</div>

A Figura XX (REFERENCIAR) apresenta a localização das dimensões definidas para os diâmetros externo, interno e útil da carcaça. A região útil de 35 mm de diâmetro, corresponde a uma área de aproximadamente 962,11 mm², utilizada como referência para o posicionamento da PCI, bateria, acelerômetro e demais componentes.


<div align="center">
  <img src="./img/cotas_diametros_internos_externos_e_util.png" alt="Dimensões dos diâmetros da carcaça" width="55%">
  <p>Figura XX - Dimensões dos diâmetros externo, interno e da região útil da carcaça da Pulseira SysCare.</p>
</div>

### Pulseira e fixação

Foi definida uma pulseira comercial de silicone de 18 mm apresentada na Figura XX (REFERENCIAR), escolhida pela disponibilidade e facilidade de integração ao protótipo. A fixação da pulseira à carcaça será realizada por meio de pinos de mola (spring bars), devido à sua simplicidade construtiva, pequeno volume e facilidade de montagem.

Como referência inicial para a geometria das garras, foi definida uma distância de 2,5 mm entre a extremidade da garra e o centro do furo do pino. Essa dimensão poderá ser ajustada após a validação física da pulseira utilizada.

<div align="center"> 
  <img src="./img/pulseira_da_estrutura_mecanica.png" alt="Cotas iniciais do sistema de fixação da pulseira" width="65%"> 
  <p>Figura XX - Pulseira Wawe 18mm de silicone</p> 
</div>

### Material e fabricação

A carcaça será fabricada por impressão 3D, utilizando o filamento disponível no IFSC para o primeiro protótipo. A escolha considera principalmente a disponibilidade do material, a facilidade de fabricação e a possibilidade de realizar alterações dimensionais durante o desenvolvimento. Para versões posteriores, o material poderá ser reavaliado considerando propriedades mecânicas, acabamento e requisitos relacionados ao contato prolongado com a pele.

### Massa do conjunto

Também foi iniciada uma estimativa da massa total da pulseira, possível ser vista na Tabela XX (REFERENCIAR), considerando a carcaça, bateria, acelerômetro, microcontrolador e PCI.

<div align="center">

| Componente                           | Massa estimada (g) |
| ------------------------------------ | -----------------: |
| Estrutura da pulseira impressa em 3D |                  X |
| Bateria                              |                  X |
| Acelerômetro IIM-42351               |                  X |
| Microcontrolador XIAO nRF52840       |                4,7 |
| PCI                                  |                  X |
| **Massa total estimada**             |              **X** |

</div>

<div align="center">
  <p>Tabela XX - Estimativa da massa total</p>
</div>

As definições apresentadas constituem a referência mecânica para o primeiro protótipo e serão utilizadas nas etapas seguintes de integração dos componentes, detalhamento da carcaça e fabricação.

> 📁 **Documentação de Mecânica:** Acesse a pasta: [Mecânica](./mechanics/README.md)



## Referências


- [1] []()


