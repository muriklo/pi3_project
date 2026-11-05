# Software

Na Etapa 3, o *software* da Pulseira SysCare saiu dos testes com dados simulados e passou a funcionar com a pulseira real. As frentes desta etapa foram o aviso remoto por *push*, para o responsável que está longe da pulseira; os ajustes revelados ao ligar o aplicativo à pulseira; a publicação da API na nuvem; e os testes de ponta a ponta. O código do aplicativo e da API foi trazido da Etapa 2 para esta pasta, de onde passa a evoluir.

> 📁 **Código e documentação técnica:** [Aplicativo](./app/README.md) · [API](./api/README.md) · **Protocolo BLE:** [ble_payload.md](./api/docs/ble_payload.md)

## Aviso remoto por *push*

Na Etapa 2, o alerta chegava aos responsáveis pela sirene do celular que ouviu a pulseira e pelo SMS enviado por ele. Faltava atender o responsável que tem o aplicativo, mas está fora do alcance do Bluetooth. Para ele foi implementado o *push* pelo Firebase Cloud Messaging (FCM) [1], que não cobra por mensagem e já estava previsto na arquitetura da API.

O celular que ouve a pulseira repassa o evento à API, que confere a assinatura do anúncio e envia o *push* aos celulares dos responsáveis daquela pulseira, que tocam a mesma sirene do alarme local. As decisões estão na Tabela 1.

<div align="center">

| Aspecto | Decisão |
|---|---|
| **Quem recebe** | Os responsáveis vinculados à pulseira pelo código exibido na tela Conta; quem é cadastrado só com telefone recebe apenas o SMS |
| **Registro** | Depois do *login*, o celular registra na API o seu identificador do Firebase e o remove ao sair da conta |
| **Formato** | No Android, a mensagem leva apenas dados, para que o próprio aplicativo, mesmo fechado, dispare a sirene insistente no canal de alarme [2] |
| **Repetição** | A mensagem traz a identificação da pulseira e o número de sequência do anúncio; se o celular já ouviu o mesmo evento pelo rádio, o *push* é ignorado |
| **Configuração** | As chaves do Firebase ficam fora do repositório; sem elas, o sistema funciona sem *push*, e a tela Saúde do sistema avisa |

</div>

<div align="center">
  <p>Tabela 1 - Decisões do aviso por <i>push</i></p>
</div>

## Ajustes encontrados na integração

No aplicativo, a biblioteca de Bluetooth do Flutter [3] configura o Android, por padrão, para procurar o anúncio estendido do BLE 5, mas a pulseira usa o anúncio legado de 31 bytes. A varredura passou a pedir o formato legado [4], e um novo teste automatizado reproduz, com os bytes reais da pulseira, o caminho até as telas Nova pulseira e Início.

No *firmware* de referência do anúncio, o pacote era remontado a cada ciclo, mas não era reenviado ao rádio; o intervalo de repouso era de cerca de 1 s, e não os 2 s do protocolo; e a função de HMAC usada deixou de ser pública na versão 3 do nRF Connect SDK [5]. O *firmware* foi corrigido, passou a calcular a assinatura pela API PSA Crypto [6] e foi gravado na XIAO nRF52840. A mesma assinatura foi levada ao *firmware* integrado, que passou a assinar as quedas detectadas pelo acelerômetro.

> 📁 **Firmware de referência do anúncio:** [teste_ble](../../etapa_2/software/firmware/teste_ble/src/main.c)

## Servidor na nuvem

A API, que até a Etapa 2 rodava no computador de desenvolvimento, foi publicada em uma máquina virtual do Azure, obtida pelo programa para estudantes [8], com HTTPS e endereço fixo. Uma máquina sempre ligada foi preferida aos planos gratuitos que desligam o serviço por inatividade, porque o escalonamento dos alertas roda dentro da API.

> 📁 **Servidor na nuvem:** requisitos, opções avaliadas, implantação e testes em [servidor_nuvem.md](./api/docs/servidor_nuvem.md)

## Testes

Além dos testes automatizados, o sistema foi testado com duas versões do *firmware*: em 22/10, com quedas simuladas e sem assinatura; em 04/11, com o *firmware* integrado, que detecta a queda pelo acelerômetro e assina o anúncio. Os resultados estão na Tabela 2.

<div align="center">

| Teste | Como foi feito | Resultado |
|---|---|---|
| **Automatizados** | 47 testes do aplicativo e 27 da API, incluindo o *push* e a recepção com os bytes reais da pulseira | Todos aprovados |
| **Pulseira → aplicativo** | Celular Android com a escuta ligada, próximo da pulseira | O aplicativo reconheceu a pulseira, exibiu a bateria e o impacto transmitidos e disparou a sirene na queda |
| **SMS** | Responsável cadastrado com telefone | O SMS com o link do mapa chegou ao celular do responsável |
| **Sem assinatura (22/10)** | Quedas simuladas, sem HMAC | A API recusou o evento, e o aplicativo desligou a sirene e informou a recusa |
| **Cadeia completa (04/11)** | Queda real com o anúncio assinado e o servidor na nuvem | Queda aceita; um responsável em outra rede recebeu o *push* e confirmou o atendimento em menos de 30 s |

</div>

<div align="center">
  <p>Tabela 2 - Testes do <i>software</i> na Etapa 3</p>
</div>

Na Figura 1, à direita, o celular que ouviu o anúncio exibe a queda, a recusa do servidor e o SMS enviado; à esquerda, o celular do responsável mostra o SMS recebido.

<div align="center">
  <img src="../img/teste_ble_e_aplicativo.png" alt="Teste da pulseira com o aplicativo e o SMS recebido pelo responsável" width="60%">
  <p>Figura 1 - Teste de 22/10/2026: queda simulada pela pulseira, alarme no celular que a ouviu e SMS recebido pelo responsável</p>
</div>

A recusa de 22/10 era o comportamento esperado: sem o HMAC [7], a API não distingue a pulseira de um rádio que imite o anúncio, e a sirene e o SMS saem antes da resposta do servidor, por projeto. Em 04/11, com o anúncio assinado, o servidor aceitou a queda e o *push* completou a cadeia (Figura 2).

<div align="center">
  <img src="../img/teste_pulseira_push.jpg" alt="Tela de alerta da queda detectada pela pulseira" width="35%">
  <p>Figura 2 - Teste de 04/11/2026: alerta da queda detectada pelo acelerômetro (<a href="../img/teste_pulseira_push.mp4">vídeo</a>)</p>
</div>

## Próximos passos

- Agrupar os alertas de uma mesma queda: os quiques da pulseira no chão geraram três.
- Apagar os registros de *push* que o Firebase não reconhece mais, deixados por instalações antigas.
- Manter a escuta com a tela apagada, por meio de um serviço em primeiro plano.
- Parar a sirene dos demais responsáveis quando um deles confirma.

## Referências (links/datasheets/livros)

- [1] [GOOGLE. Firebase Cloud Messaging - Documentation](https://firebase.google.com/docs/cloud-messaging)

- [2] [GOOGLE. Receive messages in Flutter apps - Firebase Cloud Messaging](https://firebase.google.com/docs/cloud-messaging/flutter/receive)

- [3] [NAVIDECK. universal_ble - Flutter plugin for Bluetooth Low Energy](https://pub.dev/packages/universal_ble)

- [4] [ANDROID DEVELOPERS. ScanSettings.Builder - setLegacy](https://developer.android.com/reference/android/bluetooth/le/ScanSettings.Builder)

- [5] [NORDIC SEMICONDUCTOR. nRF Connect SDK - Documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)

- [6] [ARM. PSA Certified Crypto API](https://arm-software.github.io/psa-api/crypto/)

- [7] [KRAWCZYK, H.; BELLARE, M.; CANETTI, R. RFC 2104 - HMAC: Keyed-Hashing for Message Authentication](https://datatracker.ietf.org/doc/html/rfc2104)

- [8] [MICROSOFT. Azure for Students](https://azure.microsoft.com/en-us/free/students)
