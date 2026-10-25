import 'dart:typed_data';

/// Company ID do anuncio: 0xFFFF e reservado pela Bluetooth SIG para testes.
const int companyId = 0xFFFF;

/// Versao do protocolo que este pacote entende.
const int versaoProtocolo = 0x01;

/// Tamanho do manufacturer data, sem o Company ID.
const int tamanhoPayload = 14;

/// Codigos que trafegam no ar. Espelha EVENT_CODES em app/ble.py da API.
enum TipoEvento {
  teste(0x00, 'test'),
  queda(0x01, 'fall'),
  panico(0x02, 'panic'),
  imobilidade(0x03, 'no_movement'),
  bateriaBaixa(0x04, 'low_battery'),
  heartbeat(0x05, 'heartbeat');

  const TipoEvento(this.codigo, this.nomeApi);

  final int codigo;

  /// Nome do evento na API (campo `event_type`).
  final String nomeApi;

  static TipoEvento? doCodigo(int codigo) {
    for (final tipo in values) {
      if (tipo.codigo == codigo) return tipo;
    }
    return null;
  }

  bool get ehEmergencia => this == queda || this == panico || this == imobilidade;
}

class AnuncioInvalido implements Exception {
  const AnuncioInvalido(this.motivo);

  /// Mesmo texto do PayloadError da API, para os testes compararem os dois.
  final String motivo;

  @override
  String toString() => 'AnuncioInvalido: $motivo';
}

/// Os 14 bytes do anuncio, ja validados.
///
/// Layout (little-endian):
///   [0] versao · [1..4] ble_id · [5] evento · [6..7] seq · [8] bateria %
///   [9] impacto em decimos de g · [10..13] HMAC-SHA256 truncado
class Anuncio {
  Anuncio._(this.bytes);

  factory Anuncio.decodificar(List<int> dados) {
    if (dados.length != tamanhoPayload) {
      throw AnuncioInvalido(
          'esperado $tamanhoPayload bytes, recebido ${dados.length}');
    }
    if (dados[0] != versaoProtocolo) {
      throw AnuncioInvalido('versao de protocolo nao suportada: ${dados[0]}');
    }
    if (TipoEvento.doCodigo(dados[5]) == null) {
      throw AnuncioInvalido('event_type desconhecido: 0x${_hex2(dados[5])}');
    }
    return Anuncio._(Uint8List.fromList(dados));
  }

  factory Anuncio.deHex(String hex) => Anuncio.decodificar(hexParaBytes(hex));

  /// Bytes originais. Vao para a API em `raw_payload` sem reinterpretacao:
  /// e sobre eles que a pulseira calculou a assinatura.
  final Uint8List bytes;

  int get versao => bytes[0];

  /// Identificador da pulseira em hex minusculo, como a API guarda.
  String get bleId => bytesParaHex(bytes.sublist(1, 5));

  TipoEvento get evento => TipoEvento.doCodigo(bytes[5])!;

  int get seq => bytes[6] | (bytes[7] << 8);

  int get bateriaPct => bytes[8];

  int get impactoDg => bytes[9];

  double get impactoG => impactoDg / 10;

  String get assinatura => bytesParaHex(bytes.sublist(10, 14));

  String get hex => bytesParaHex(bytes);
}

String bytesParaHex(List<int> bytes) => bytes.map(_hex2).join();

Uint8List hexParaBytes(String hex) {
  final limpo = hex.replaceAll(' ', '');
  if (limpo.length.isOdd) {
    throw AnuncioInvalido('hex com numero impar de digitos: $hex');
  }
  final saida = Uint8List(limpo.length ~/ 2);
  for (var i = 0; i < saida.length; i++) {
    final par = limpo.substring(2 * i, 2 * i + 2);
    final valor = int.tryParse(par, radix: 16);
    if (valor == null || par.startsWith('-') || par.startsWith('+')) {
      throw AnuncioInvalido('hex invalido: $hex');
    }
    saida[i] = valor;
  }
  return saida;
}

String _hex2(int b) => b.toRadixString(16).padLeft(2, '0');
