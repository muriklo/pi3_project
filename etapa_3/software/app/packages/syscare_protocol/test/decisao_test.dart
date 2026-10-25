import 'package:syscare_protocol/syscare_protocol.dart';
import 'package:test/test.dart';

import 'apoio.dart';

void main() {
  const conta = {'a1b2c3d4'};
  const alheia = 'ffee0001';

  Acao decide(Anuncio a, [Deduplicador? d]) => decidir(a, d ?? Deduplicador(), conta);

  group('pulseira da conta', () {
    test('queda, panico e imobilidade alarmam e enviam', () {
      for (final e in [TipoEvento.queda, TipoEvento.panico, TipoEvento.imobilidade]) {
        expect(decide(anuncio(evento: e)), Acao.alarmarEEnviar, reason: e.name);
      }
    });

    test('bateria baixa e teste notificam sem sirene', () {
      expect(decide(anuncio(evento: TipoEvento.bateriaBaixa)), Acao.notificarEEnviar);
      expect(decide(anuncio(evento: TipoEvento.teste)), Acao.notificarEEnviar);
    });

    test('heartbeat nunca alarma, mesmo repetindo o seq de uma queda', () {
      // App recem-aberto, sem estado: o heartbeat traz o seq 42 do ultimo
      // evento. Foi exatamente o furo que motivou o codigo 0x05.
      expect(decide(anuncio(evento: TipoEvento.heartbeat, seq: 42)), Acao.telemetria);
    });

    test('heartbeat nao consome o seq do proximo evento', () {
      final d = Deduplicador();
      decide(anuncio(evento: TipoEvento.heartbeat, seq: 42), d);
      expect(decide(anuncio(seq: 43), d), Acao.alarmarEEnviar);
    });

    test('repeticao do anuncio de emergencia e ignorada', () {
      final d = Deduplicador();
      expect(decide(anuncio(seq: 7), d), Acao.alarmarEEnviar);
      expect(decide(anuncio(seq: 7), d), Acao.ignorar);
    });
  });

  group('pulseira de outra conta', () {
    test('emergencia e repassada a API sem alarmar, uma vez so', () {
      final d = Deduplicador();
      expect(decide(anuncio(bleId: alheia, seq: 3), d), Acao.apenasEnviar);
      expect(decide(anuncio(bleId: alheia, seq: 3), d), Acao.ignorar);
    });

    test('heartbeat, teste e bateria baixa sao ignorados', () {
      for (final e in [TipoEvento.heartbeat, TipoEvento.teste, TipoEvento.bateriaBaixa]) {
        expect(decide(anuncio(bleId: alheia, evento: e)), Acao.ignorar, reason: e.name);
      }
    });
  });
}
