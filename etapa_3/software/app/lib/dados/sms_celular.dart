import 'package:flutter/services.dart';

/// SMS pelo plano do proprio celular (Camada 3). Implementado em Kotlin, no
/// MainActivity, com o SmsManager do Android.
class SmsCelular {
  static const _canal = MethodChannel('br.edu.ifsc.syscare/sms');

  /// O aparelho tem linha para SMS (tablets sem chip nao tem).
  Future<bool> disponivel() async => await _canal.invokeMethod<bool>('disponivel') ?? false;

  Future<bool> temPermissao() async => await _canal.invokeMethod<bool>('temPermissao') ?? false;

  Future<bool> pedirPermissao() async {
    try {
      return await _canal.invokeMethod<bool>('pedirPermissao') ?? false;
    } on PlatformException {
      return false;
    }
  }

  /// Entrega o SMS ao sistema do celular. Lanca [PlatformException] se falhar.
  Future<void> enviar(String numero, String texto) =>
      _canal.invokeMethod<int>('enviar', {'numero': numero, 'texto': texto});
}
