# Etapa 2

A etapa 2 ...

**(Adicionar aqui UM parágrafo com visão geral da etapa. Resumo dos itens da planilha.)**

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
| **Bateria** | CR2032 | <img src="./img" width="120"> | Alimentação do sistema | Dimensões reduzidas e disponibilidade |
| **Comunicação** | Bluetooth Low Energy | <img src="./img/ble.png" width="120"> | Comunicação com o smartphone | Baixo consumo e integração ao XIAO nRF52840 |

</div>

<div align="center">
  <p>Tabela 1 - Definição dos componentes do sistema</p>
</div>

A escolha dos componentes apresentada na Tabela 1, corresponde à configuração utilizada para a validação do primeiro protótipo. Após a validação da arquitetura e do funcionamento do sistema, poderão ser avaliadas alternativas com foco na redução do consumo energético e na integração dos componentes em uma placa dedicada.

---
## Definição dos softwares necessários (FW, HW, MEC, APP)

Nessa parte será detalhado quais softwares será usado para firmware, hardware, mecanica e aplicativo.

(tabela)



## Referências (links/datasheets/livros)


- [1] []()


