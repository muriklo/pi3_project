# Software

## Aplicativo

Arquitetura do aplicativo móvel: escolha da plataforma, divisão em camadas, telas, tratamento do anúncio BLE e integração com a API, incluindo a revisão do protocolo que criou o código de *heartbeat* e as pendências encontradas na API.

> 📁 **Documentação do Aplicativo:** Acesse a pasta: [Aplicativo](./app/README.md)

## Firmware

Nessa etapa, foi realizada a definição de como será implementado o algoritmo de detecção de quedas, utilizando o acelerômetro para identificar o evento de queda e gerar uma interrupção para o microcontrolador. Também foram definidos o funcionamento da detecção, o processamento da interrupção e os critérios utilizados para a confirmação de uma queda.

> 📁 **Documentação do Firmware:** Acesse a pasta: [Firmware](./firmware/README.md)
