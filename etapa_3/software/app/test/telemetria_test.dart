import 'package:flutter_test/flutter_test.dart';
import 'package:syscare_app/aplicacao/telemetria.dart';
import 'package:syscare_protocol/syscare_protocol.dart';

import 'apoio.dart';

void main() {
  final base = DateTime.utc(2026, 9, 23, 12, 0, 0);
  final hb = anuncio(evento: TipoEvento.heartbeat, seq: 42);

  test('heartbeat a cada 2 s vira uma amostra por minuto', () {
    final ag = AgregadorTelemetria();
    var amostras = 0;
    for (var s = 0; s < 180; s += 2) {
      if (ag.registrar(hb, em: base.add(Duration(seconds: s)))) amostras++;
    }
    expect(amostras, 3);
    expect(ag.retirarLotes()['a1b2c3d4'], hasLength(3));
    expect(ag.vazio, isTrue);
  });

  test('janela alinhada ao relogio, como a da API', () {
    final ag = AgregadorTelemetria();
    expect(ag.registrar(hb, em: base.add(const Duration(seconds: 59))), isTrue);
    // 1 s depois, mas ja no minuto seguinte: nova amostra.
    expect(ag.registrar(hb, em: base.add(const Duration(seconds: 60))), isTrue);
  });

  test('lote que falhou volta, respeitando o limite da API', () {
    final ag = AgregadorTelemetria();
    for (var m = 0; m < AgregadorTelemetria.maximoPorLote; m++) {
      ag.registrar(hb, em: base.add(Duration(minutes: m)));
    }
    final lote = ag.retirarLotes()['a1b2c3d4']!;
    ag.registrar(hb, em: base.add(const Duration(hours: 5)));
    ag.devolver('a1b2c3d4', lote);
    expect(ag.retirarLotes()['a1b2c3d4'], hasLength(AgregadorTelemetria.maximoPorLote));
  });
}
