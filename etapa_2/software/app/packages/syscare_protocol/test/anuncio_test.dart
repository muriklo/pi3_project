import 'dart:convert';
import 'dart:io';

import 'package:syscare_protocol/syscare_protocol.dart';
import 'package:test/test.dart';

/// Gerados por etapa_2/software/api/scripts/gerar_vetores_ble.py a partir do
/// codigo da API. Se estes testes falham, o app e o servidor divergem.
final Map<String, dynamic> vetores =
    jsonDecode(File('test/vetores_api.json').readAsStringSync())
        as Map<String, dynamic>;

void main() {
  group('le os bytes exatamente como a API', () {
    for (final caso in (vetores['validos'] as List).cast<Map<String, dynamic>>()) {
      final esperado = caso['esperado'] as Map<String, dynamic>;
      test(caso['nome'] as String, () {
        final a = Anuncio.deHex(caso['hex'] as String);
        expect(a.versao, esperado['version']);
        expect(a.bleId, esperado['ble_id']);
        expect(a.evento.nomeApi, esperado['event_type']);
        expect(a.seq, esperado['seq']);
        expect(a.bateriaPct, esperado['battery_pct']);
        expect(a.impactoDg, esperado['impact_dg']);
        expect(a.impactoG, closeTo((esperado['impact_g'] as num).toDouble(), 1e-9));
        expect(a.assinatura, esperado['signature']);
        expect(a.hex, caso['hex'], reason: 'os bytes vao intactos para a API');
      });
    }
  });

  group('recusa o que a API recusa, com a mesma mensagem', () {
    for (final caso in (vetores['invalidos'] as List).cast<Map<String, dynamic>>()) {
      test(caso['nome'] as String, () {
        expect(
          () => Anuncio.deHex(caso['hex'] as String),
          throwsA(isA<AnuncioInvalido>()
              .having((e) => e.motivo, 'motivo', caso['erro'])),
        );
      });
    }
  });

  test('app e API conhecem os mesmos codigos de evento', () {
    final daApi = (vetores['validos'] as List)
        .map((c) => (c['esperado'] as Map)['event_type'] as String)
        .toSet();
    expect(TipoEvento.values.map((t) => t.nomeApi).toSet(), equals(daApi));
  });

  test('hex invalido e recusado', () {
    expect(() => Anuncio.deHex('01zz'), throwsA(isA<AnuncioInvalido>()));
    expect(() => Anuncio.deHex('012'), throwsA(isA<AnuncioInvalido>()));
  });
}
