import 'anuncio.dart';

/// `novo` e mais recente que `ultimo` no contador circular de 16 bits.
///
/// Mesma ideia da aritmetica de numeros de serie da RFC 1982: depois de 65535
/// vem 0, e uma distancia de ate meio circulo conta como "para frente".
bool seqMaisNovo(int novo, int ultimo) {
  final distancia = (novo - ultimo) & 0xFFFF;
  return distancia != 0 && distancia < 0x8000;
}

/// Lembra o ultimo `seq` tratado de cada pulseira.
///
/// A pulseira repete o anuncio de emergencia a cada 100 ms por 30 s e a cada
/// 500 ms pelos 5 min seguintes; sem isto a sirene tocaria a cada recepcao.
/// O app persiste [estado] para que reabrir o aplicativo durante o anuncio
/// sustentado tambem nao repita o alarme.
class Deduplicador {
  Deduplicador([Map<String, int>? estado]) : _ultimo = {...?estado};

  final Map<String, int> _ultimo;

  Map<String, int> get estado => Map.unmodifiable(_ultimo);

  /// true na primeira vez que o evento aparece; false nas repeticoes.
  bool ehNovo(Anuncio anuncio) {
    final anterior = _ultimo[anuncio.bleId];
    if (anterior != null && !seqMaisNovo(anuncio.seq, anterior)) return false;
    _ultimo[anuncio.bleId] = anuncio.seq;
    return true;
  }
}
