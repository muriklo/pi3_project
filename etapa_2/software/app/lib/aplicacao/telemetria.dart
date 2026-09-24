import 'package:syscare_protocol/syscare_protocol.dart';

import '../dados/api.dart';

/// Agrega os heartbeats (a cada 2 s) em uma amostra por pulseira por minuto.
///
/// A janela e a mesma da API (SYSCARE_TELEMETRY_BUCKET_SECONDS, alinhada ao
/// relogio): varios celulares ouvindo a mesma pulseira geram uma amostra so.
class AgregadorTelemetria {
  AgregadorTelemetria({this.janela = const Duration(seconds: 60)});

  final Duration janela;

  /// Limite da API por lote.
  static const maximoPorLote = 200;

  final Map<String, List<AmostraTelemetria>> _pendentes = {};
  final Map<String, int> _ultimaJanela = {};

  /// true se o heartbeat virou amostra; false se a janela ja tinha uma.
  bool registrar(Anuncio heartbeat, {required DateTime em, int? rssi}) {
    final indice = em.millisecondsSinceEpoch ~/ janela.inMilliseconds;
    if (_ultimaJanela[heartbeat.bleId] == indice) return false;
    _ultimaJanela[heartbeat.bleId] = indice;
    final lista = _pendentes.putIfAbsent(heartbeat.bleId, () => []);
    lista.add(AmostraTelemetria(
      seq: heartbeat.seq,
      registradaEm: em,
      bateriaPct: heartbeat.bateriaPct,
      rssi: rssi,
    ));
    if (lista.length > maximoPorLote) lista.removeAt(0);
    return true;
  }

  bool get vazio => _pendentes.isEmpty;

  /// Entrega os lotes e esvazia; o que falhar volta por [devolver].
  Map<String, List<AmostraTelemetria>> retirarLotes() {
    final lotes = Map.of(_pendentes);
    _pendentes.clear();
    return lotes;
  }

  void devolver(String bleId, List<AmostraTelemetria> amostras) {
    final lista = _pendentes.putIfAbsent(bleId, () => []);
    lista.insertAll(0, amostras);
    if (lista.length > maximoPorLote) lista.removeRange(0, lista.length - maximoPorLote);
  }
}
