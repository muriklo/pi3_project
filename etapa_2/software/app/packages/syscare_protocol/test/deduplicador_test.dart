import 'package:syscare_protocol/syscare_protocol.dart';
import 'package:test/test.dart';

import 'apoio.dart';

void main() {
  group('seqMaisNovo', () {
    test('avanca normalmente', () => expect(seqMaisNovo(44, 43), isTrue));
    test('o mesmo seq nao e novo', () => expect(seqMaisNovo(43, 43), isFalse));
    test('seq menor e antigo', () => expect(seqMaisNovo(42, 43), isFalse));
    test('0 depois de 65535 e novo (volta do contador)',
        () => expect(seqMaisNovo(0, 65535), isTrue));
    test('65535 depois de 0 e antigo', () => expect(seqMaisNovo(65535, 0), isFalse));
  });

  group('Deduplicador', () {
    final queda43 = anuncio(seq: 43);

    test('trata o evento uma vez, por mais que o anuncio se repita', () {
      final d = Deduplicador();
      expect(d.ehNovo(queda43), isTrue);
      // 30 s a cada 100 ms: ~300 recepcoes do mesmo anuncio.
      for (var i = 0; i < 300; i++) {
        expect(d.ehNovo(queda43), isFalse);
      }
      expect(d.ehNovo(anuncio(seq: 44)), isTrue);
    });

    test('estado restaurado nao repete o alarme ao reabrir o app', () {
      final antes = Deduplicador()..ehNovo(queda43);
      final depois = Deduplicador(antes.estado);
      expect(depois.ehNovo(queda43), isFalse);
      expect(depois.ehNovo(anuncio(seq: 44)), isTrue);
    });

    test('pulseiras diferentes nao interferem entre si', () {
      final d = Deduplicador()..ehNovo(queda43);
      expect(d.ehNovo(anuncio(seq: 43, bleId: 'ffee0001')), isTrue);
    });

    test('anuncio antigo retransmitido e ignorado', () {
      final d = Deduplicador()..ehNovo(queda43);
      expect(d.ehNovo(anuncio(seq: 40)), isFalse);
    });
  });
}
