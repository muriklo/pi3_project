Módulo CAN para depuração do barco solar (Exemplo do nome do projeto)
#####################################################################

.. contents::
   :local:
   :depth: 2



Requisitos
**********

Este projeto foi implementado com os seguintes módulos/softwares/hardwares...

- `XIAO nRF52840 <https://wiki.seeedstudio.com/XIAO_BLE/>`_


Visão geral
***********

Esse projeto faz parta do barco solar e ...

O desenvolvimento foi divido em quatro etapas:

- Etapa 1 (data da entrega): (breve resumo da etapa)
- Etapa 2 (data da entrega): (breve resumo da etapa)
- Etapa 3 (data da entrega): (breve resumo da etapa)
- Etapa 4 (data da entrega): (breve resumo da etapa)

Configuração
*************

Projeto foi implementado com o nRF OpenConnect SDK versão 2.4.x.
Consulte `Configuring your application <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.4.2/nrf/getting_started/modifying.html#configure-application>`_ para obter informações sobre como alterar a configuração permanente ou temporariamente.

A configuração do perfil do dispositivo é realizada no arquivo de configuração `prj.conf <prj.conf>`_:

- End-Device:

.. code:: C 

  (...)
  CONFIG_ZIGBEE_ROLE_END_DEVICE=y  
  # CONFIG_ZIGBEE_ROLE_ROUTER=y
  // Versão do firmware
  CONFIG_MCUBOOT_IMAGE_VERSION="0.0.3"
  CONFIG_ZIGBEE_FOTA_COMMENT="ruido_zigbee_endpoint"
  # CONFIG_ZIGBEE_FOTA_COMMENT="ruido_zigbee_router"


Interface do usuário
********************

LED 1:
  Pisca enquanto o filtro estiver ativo.

Botão 1:
  Ativa o módulo xyz.


Compilando e executando
***********************

Colocar detalhes na construção da applicação. Exemplo: 

Para compilar o projeto com o Visual Studio Code, siga as etapas listadas na página `How to build an application <https://nrfconnect.github.io/vscode-nrf-connect/get_started/build_app_ncs.html>`_  na documentação da extensão nRF Connect for VS Code.  `Building and programming an application  <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.4.2/nrf/getting_started/programming.html#gs-programming>`_ para outros cenários de construção e programação e `Testing and debugging an application <https://developer.nordicsemi.com/nRF_Connect_SDK/doc/2.4.2/nrf/getting_started/testing.html#gs-testing>`_ para obter informações gerais sobre testes e depuração no nRF Connect SDK.

Recomenda-se o uso do J-Link para gravação e/ou depuração.

Testando
========

Após programar o microcontrolador, conclua as etapas a seguir para testá-lo:

1. ...

Montagem
********

Breve descrição da montagem final do projeto.

