# Etapa 2

A etapa 2 é destinada ao desenvolvimento e à definição da arquitetura do primeiro protótipo da Pulseira SysCare, abrangendo as áreas de hardware, firmware, mecânica e aplicativo. Nesta etapa, foram selecionados os principais componentes do sistema, definidos os softwares e ferramentas utilizados no desenvolvimento, estabelecida a arquitetura de hardware e firmware, incluindo a comunicação com o acelerômetro e o BLE, e desenvolvido o projeto mecânico da pulseira e de seu encapsulamento. Também foram consideradas as restrições de dimensões, massa, integração dos componentes e montagem do protótipo, buscando uma solução compatível com os requisitos funcionais e físicos definidos para o projeto.

### Definição dos *softwares* necessários para o projeto

Para o desenvolvimento da Pulseira SysCare, foram definidos softwares específicos para cada uma das etapas do projeto, abrangendo o desenvolvimento do firmware (FW), projeto eletrônico (HW), desenvolvimento mecânico (MEC) e aplicativo (APP). A seleção considera as ferramentas utilizadas para programação, simulação, desenvolvimento das placas eletrônicas e modelagem da estrutura física do dispositivo.
Após todas os estudos envolvidos foi decidido os *softwares* a serem usados, podendo ser visto na Tabela 1:

<div align="center">

| **Etapa**            | **Software**                    | **Aplicação no projeto**                                                                              |
| -------------------- | ------------------------------- | ----------------------------------------------------------------------------------------------------- |
| **FW - Firmware**    | Visual Studio Code + nRF Connect NORDIC | Desenvolvimento, compilação e gerenciamento do firmware do XIAO nRF52840.                             |
| **HW - Hardware**    | Altium Designer                 | Desenvolvimento do esquemático e projeto da placa de circuito impresso (PCI).                         |
| **MEC - Mecânica**   | FreeCAD                 | Modelagem tridimensional e desenvolvimento da estrutura mecânica da pulseira e de seu encapsulamento. |
| **APP - Aplicativo** | Flutter (Dart) + Visual Studio Code + Android Studio | Desenvolvimento do aplicativo em Flutter no VS Code; o Android Studio fornece o Android SDK e o emulador usados para compilar e testar o APK. |

</div>

<div align="center">
  <p>Tabela 1 - Definição dos <i>softwares</i> do projeto</p>
</div>

--- 

## Desenvolvimento de *Hardware*

### Componentes selecionados

Após a análise dos requisitos e a comparação entre as alternativas disponíveis, foram definidos os principais componentes para a implementação do primeiro protótipo. A seleção considerou tanto as características elétricas e funcionais dos componentes quanto fatores relacionados à disponibilidade, facilidade de integração e restrições dimensionais da pulseira.

A Tabela 2 apresenta os componentes selecionados para o protótipo e suas respectivas funções no sistema. Todo o estudo e seleção, bem como os benchmarks feitos estão localizados no *README.md* na pasta *Hardware*.

> Nesta seção, foram detalhadas as escolhas dos componentes para o protótipo inicial, bem como o esquemático da PCI. Todas as considerações e comparações podem ser vistas no documento.
>
> 📁 **Documentação de *Hardware*:** Acesse a pasta: [Hardware](./hardware/README.md)


<div align="center">

| **Categoria** | **Componente** | **Figura** | **Função** | **Critério de seleção** |
|:---:|:---:|:---:|:---|:---|
| **Microcontrolador** | Seeed Studio XIAO nRF52840 | <img src="./img/xiaonrf52840.png" width="200"> | Processamento, controle do sistema e comunicação BLE | Plataforma compacta, BLE integrado e facilidade de prototipagem |
| **Acelerômetro** | TDK InvenSense IIM-42351 | <img src="img/iim42351.png" width="120"> | Aquisição de aceleração e detecção de queda | Módulo disponível, conhecimento prévio e Free-fall Detection |
| **Bateria** | Bateria recarregável 541112 | <img src="./img/battery541112.png" width="120"> | Alimentação do sistema | Dimensões reduzidas e disponibilidade |
| **Comunicação** | Bluetooth Low Energy | <img src="./img/ble.png" width="120"> | Comunicação com o smartphone | Baixo consumo e integração ao XIAO nRF52840 |

</div>

<div align="center">
  <p>Tabela 2 - Definição dos componentes do sistema</p>
</div>

A escolha dos componentes apresentada na Tabela 2 corresponde à configuração utilizada para a validação do primeiro protótipo. Após a validação da arquitetura e do funcionamento do sistema, poderão ser avaliadas alternativas com foco na redução do consumo energético e na integração dos componentes em uma placa dedicada.


### Desenvolvimento do esquemático

A partir dos componentes definidos na Tabela 2, foi desenvolvido o esquemático da Pulseira SysCare no Altium Designer, apresentado na Figura 1, com o projeto armazenado no Altium 365. Por se tratar do primeiro protótipo, o circuito prioriza a flexibilidade de montagem e a facilidade de depuração.

O circuito de carregamento integrado ao XIAO nRF52840 permitiu adotar a bateria recarregável 541112, com recarga pela própria porta USB, e entre a bateria e o módulo foi previsto um ponto para medição do consumo. Também foram disponibilizados os sinais da interface SWD para depuração do firmware. O acelerômetro IIM-42351 se comunica via SPI e utiliza os pinos de interrupção para acordar o microcontrolador na detecção de queda, podendo ser soldado diretamente na placa ou conectado como módulo avulso. A interface com o usuário é composta por um LED de indicação e um botão de emergência com *debounce* em hardware.

<div align="center">
  <img src="./img/schematics_all.png" alt="Esquemático completo da Pulseira SysCare" width="90%">
  <p>Figura 1 - Esquemático completo da Pulseira SysCare</p>
</div>

> Nesta seção, foram detalhadas as escolhas dos componentes para o protótipo inicial, bem como o esquemático da PCI. Todas as considerações e comparações podem ser vistas no documento.
>
> 📁 **Documentação do esquemático:** Acesse a pasta: [Esquemático](./hardware/schematics/README.md)

---

## Desenvolvimento de *Software* e *Firmware*

Descrever atividades feitas...

### API
ESCREVERRR


### Firmware

Nesta etapa foram definidas as principais soluções para o firmware da Pulseira SysCare, com foco na detecção de quedas e na arquitetura do software.

A detecção de quedas será realizada utilizando o acelerômetro IIM-42351, aproveitando seu recurso interno de Freefall Detection em modo de baixo consumo. Foi definida inicialmente uma frequência de operação de 25 Hz, permitindo o uso do algoritmo em Low Power, conforme a configuração apresentada na Tabela 3. Quando uma possível queda livre é identificada, o acelerômetro gera uma interrupção para o XIAO nRF52840, que realiza o processamento do evento.


<div align="center">

| Parâmetro | Configuração |
|---|---|
| **Acelerômetro** | IIM-42351 |
| **Accel ODR** | 25 Hz |
| **DMP ODR** | 25 Hz |
| **Freefall Detection** | Low Power |
| **Detecção inicial** | Freefall Detection interno |
| **Saída da detecção** | Interrupção para o MCU |
| **Processamento posterior** | Impacto + imobilidade -> (se necessário) |
| **MCU** | XIAO nRF52840 |

</div>

<div align="center">
  <p>Tabela 3 - Proposta inicial de configuração do acelerômetro</p>
</div>

Dessa forma, o acelerômetro é responsável pela detecção inicial e geração da interrupção, enquanto o microcontrolador é responsável pelo tratamento da interrupção e envio do pedido de ajuda.


Já a arquitetura de *hardware* e *firmware*, apresentada na Figura 2, será desenvolvida utilizando Zephyr RTOS / nRF Connect SDK, sendo organizada em módulos para detecção e confirmação de quedas, tratamento de interrupções, comunicação com o acelerômetro, gerenciamento de energia, armazenamento de dados e comunicação BLE. A comunicação entre o XIAO nRF52840 e o IIM-42351 será realizada por SPI, enquanto a linha INT1 será utilizada para sinalizar os eventos de detecção ao microcontrolador.

<div align="center">
  <img src="./img/diagrama_de_blocos_hardware.png" alt="Diagrama de blocos da arquitetura de hardware e firmware" width="70%">
  <p>Figura 2 - Diagrama de blocos da arquitetura de <i>hardware</i> e <i>firmware</i> da Pulseira SysCare</p>
</div>


---

## Desenvolvimento mecânico

Na etapa 2, foi desenvolvido o projeto mecânico da Pulseira SysCare a partir dos requisitos dimensionais, ergonômicos e de fabricação definidos anteriormente. Foram estabelecidas as dimensões da carcaça, o espaço disponível para os componentes eletrônicos, o sistema de fixação da pulseira, o material de fabricação e uma estimativa inicial da massa do conjunto.

### Dimensões e integração mecânica

Para o primeiro protótipo, foi definida uma carcaça compacta, com dimensões externas de 42 mm de largura e 52 mm de comprimento total, incluindo as regiões de fixação da pulseira. O espaço interno foi definido considerando um diâmetro interno de 37 mm e uma região útil de 35 mm para acomodação dos componentes eletrônicos. As dimensões definidas estão resumidas na Tabela 4.

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
  <p>Tabela 4 - Dimensões mecânicas definidas</p>
</div>

A Figura 3 apresenta a localização das dimensões definidas para os diâmetros externo, interno e útil da carcaça. A região útil de 35 mm de diâmetro corresponde a uma área de aproximadamente 962,11 mm², utilizada como referência para o posicionamento da PCI, bateria, acelerômetro e demais componentes.


<div align="center">
  <img src="./img/cotas_diametros_internos_externos_e_util.png" alt="Dimensões dos diâmetros da carcaça" width="55%">
  <p>Figura 3 - Dimensões dos diâmetros externo, interno e da região útil da carcaça da Pulseira SysCare</p>
</div>

### Pulseira e fixação

Foi definida uma pulseira comercial de silicone de 18 mm, apresentada na Figura 4, escolhida pela disponibilidade e facilidade de integração ao protótipo. A fixação da pulseira à carcaça será realizada por meio de pinos de mola (spring bars), devido à sua simplicidade construtiva, pequeno volume e facilidade de montagem.

Como referência inicial para a geometria das garras, foi definida uma distância de 2,5 mm entre a extremidade da garra e o centro do furo do pino. Essa dimensão poderá ser ajustada após a validação física da pulseira utilizada.

<div align="center"> 
  <img src="./img/pulseira_da_estrutura_mecanica.png" alt="Pulseira comercial de silicone de 18 mm" width="65%"> 
  <p>Figura 4 - Pulseira de silicone Wawe de 18 mm</p> 
</div>

### Material e fabricação

A carcaça será fabricada por impressão 3D, utilizando o filamento disponível no IFSC para o primeiro protótipo. A escolha considera principalmente a disponibilidade do material, a facilidade de fabricação e a possibilidade de realizar alterações dimensionais durante o desenvolvimento. Para versões posteriores, o material poderá ser reavaliado considerando propriedades mecânicas, acabamento e requisitos relacionados ao contato prolongado com a pele.

### Massa do conjunto

Também foi iniciada uma estimativa da massa total da pulseira, apresentada na Tabela 5, considerando a carcaça, bateria, acelerômetro, microcontrolador e PCI.

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
  <p>Tabela 5 - Estimativa da massa total</p>
</div>

As definições apresentadas constituem a referência mecânica para o primeiro protótipo e serão utilizadas nas etapas seguintes de integração dos componentes, detalhamento da carcaça e fabricação. A Figura 5 apresenta o modelo 3D da pulseira com o protótipo da PCI integrado.

<div align="center"> 
  <img src="./img/prototipo_mecanico.png" alt="Modelo 3D da pulseira com a PCI integrada" width="75%"> 
  <p>Figura 5 - Projeto 3D com protótipo da PCI integrada</p> 
</div>

> 📁 **Documentação de Mecânica:** Acesse a pasta: [Mecânica](./mechanics/README.md)



## Referências


- [1] []()


