# Canais de alerta — custo, alcance e o que recomendo

Levantamento feito em **setembro de 2026**. Preços de API mudam; confira as
páginas oficiais antes de citar no relatório.

## Comparativo

| Canal | Custo | Chega com o app fechado? | Funciona sem internet? | Esforço |
|---|---|---|---|---|
| **Push FCM** | **Grátis, ilimitado** | Sim (prioridade `high` fura o Doze) | Não | Baixo — já implementado |
| **Alerta local no celular** | **Grátis** | Sim (o app já está acordado pelo BLE) | **Sim** | Baixo — é no Flutter |
| **SMS nativo do celular** | Grátis (usa o plano do usuário) | — | **Sim** | Baixo — `SmsManager` no Android |
| **Ligação nativa** | Grátis (usa o plano) | — | **Sim** | Baixo — `ACTION_CALL` |
| **Telegram Bot** | **Grátis, ilimitado** | Sim | Não | Baixo — 1 requisição HTTP |
| WhatsApp Cloud API | Grátis p/ 5 números de teste; em produção ~US$ 0,008/msg *utility* no Brasil | Sim | Não | **Alto** — verificação de negócio, templates aprovados |
| SMS via Twilio | ~US$ 0,06/msg + US$ 1,15/mês do número | — | Não | Médio |
| Ligação via Twilio | Por minuto, além do número | — | Não | Médio |

## Recomendação para o SysCare

**Camada 1 — alerta local, no próprio celular que ouviu o BLE.** Grátis,
instantâneo e o único que funciona sem internet. Como o app já está acordado
processando o anúncio, ele mesmo dispara pop-up em tela cheia, som de sirene
(ignorando o modo silencioso, via canal de notificação de alta prioridade) e
vibração. **Este é o alerta mais importante do sistema** — os outros são
redundância.

**Camada 2 — push FCM para os demais cuidadores.** É o que a API já faz. Custo
zero, entrega em segundos, funciona com o app fechado. Cobre o cuidador que não
está no alcance do BLE.

**Camada 3 — SMS pelo próprio app.** O app dispara SMS direto pelo celular,
pelo plano do usuário. Sai de graça e não depende de servidor nenhum. No
Android: `SmsManager` (permissão `SEND_SMS`); a ligação automática, com
`Intent(ACTION_CALL)` (permissão `CALL_PHONE`), segue só avaliada.

> **Implementada na etapa 2** (`etapa_2/software/app`, Seção 5.7 da
> arquitetura do app), com uma mudança: o SMS sai em **toda** emergência, para
> todos os responsáveis com telefone, e não só quando a API está inacessível.
> Virou o canal de SMS do sistema porque a conta de teste do Twilio só envia
> modelos prontos (erro `572006`, testado em 24/09/2026).

> Atenção na publicação: a Play Store restringe `SEND_SMS` e `CALL_PHONE`, mas
> abre exceção justamente para apps de emergência/segurança pessoal. É preciso
> preencher a declaração de permissão sensível. Para a banca e para uso interno
> não muda nada.

**Camada 4 (opcional) — Telegram Bot.** Se quiserem um canal externo grátis com
custo de implementação de ~30 linhas, é o melhor negócio. `BotFather` gera o
token, uma requisição `POST` em `api.telegram.org/bot<token>/sendMessage` entrega
a mensagem, e dá para incluir botão inline de "Estou indo" que chama o
`/v1/alerts/{id}/ack` da API por webhook. Sem limite prático, sem verificação de
negócio, sem template aprovado.

## Sobre WhatsApp

É o canal que a família mais quer, e o mais caro de operar.

- Desde **1º de julho de 2025** a Meta cobra **por mensagem entregue**, não mais
  por "conversa" de 24 h. Categoria *utility* no Brasil sai por cerca de
  **US$ 0,008**; *authentication* ~US$ 0,0225; *marketing* ~US$ 0,0625.
- Mensagens de serviço e templates *utility* dentro da janela de 24 h abertas
  pelo usuário são gratuitas hoje — mas a Meta anunciou que **a partir de
  1º de outubro de 2026** passará a cobrá-las também. Não construam o projeto
  contando com essa gratuidade.
- Um alerta de queda é **business-initiated**: exige template aprovado e é pago
  desde a primeira mensagem.

**Para a apresentação, porém, sai de graça:** o *número de teste* do WhatsApp
Cloud API envia mensagens sem custo para até **5 números verificados** no painel
de desenvolvedor da Meta. É suficiente para demonstrar o canal na banca sem
abrir conta comercial, sem verificação de empresa e sem gastar nada. As
limitações: só esses 5 números, eles não podem iniciar conversa, e os templates
criados no número de teste não migram para um número de produção.

Se decidirem implementar, o encaixe na arquitetura já está pronto: basta uma
classe `WhatsAppChannel(NotificationChannel)` em `app/notifications/`, sem tocar
em nenhuma regra de alerta.

## Sobre SMS pago

Só faz sentido como último recurso para um cuidador **sem smartphone**. O Brasil
é uma das rotas mais caras: Twilio cobra **US$ 0,0599 por SMS** para números
brasileiros, mais **US$ 1,15/mês** pelo aluguel do número internacional.
Provedores nacionais (Zenvia, GTI SMS, Comtele) ficam na faixa de
**R$ 0,05 a R$ 0,065** por SMS, sem mensalidade em alguns planos — mais baratos
que o Twilio para volume brasileiro, e com atendimento em português.

Para o protótipo, o SMS nativo do celular (camada 3) entrega o mesmo resultado
por R$ 0,00.

## Fontes

- [WhatsApp Business Platform — pricing](https://whatsappbusiness.com/products/platform-pricing/)
- [WhatsApp API pricing 2026 — taxas por país](https://chatmaxima.com/whatsapp-api-pricing/)
- [Limitações do número de teste do Cloud API](https://help.wanotifier.com/en/article/test-phone-number-limitations-in-direct-setup-kt0ly2/)
- [Twilio — SMS pricing Brazil](https://www.twilio.com/en-us/sms/pricing/br)
- [GTI SMS vs Zenvia — comparativo de preços no Brasil](https://gtisms.com/gti-sms-vs-zenvia/)
