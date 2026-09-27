import 'package:syscare_protocol/syscare_protocol.dart';

/// Monta um anuncio sem assinatura: o dominio nao verifica o HMAC (quem
/// verifica e a API), entao a assinatura nao importa nestes testes.
Anuncio anuncio({
  TipoEvento evento = TipoEvento.queda,
  int seq = 1,
  String bleId = 'a1b2c3d4',
  int bateria = 80,
  int impactoDg = 30,
}) =>
    Anuncio.decodificar([
      versaoProtocolo,
      ...hexParaBytes(bleId),
      evento.codigo,
      seq & 0xFF,
      (seq >> 8) & 0xFF,
      bateria,
      impactoDg,
      0, 0, 0, 0,
    ]);
