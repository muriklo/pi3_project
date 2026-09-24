import 'dart:convert';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../dominio/modelos.dart';

class SessaoSalva {
  const SessaoSalva({required this.token, required this.usuario});

  final String token;
  final Usuario usuario;

  /// Vencimento lido do proprio JWT. A API nao tem renovacao (pendencia P2),
  /// entao a tela Saude do sistema mostra quantos dias faltam.
  DateTime? get venceEm {
    final partes = token.split('.');
    if (partes.length != 3) return null;
    try {
      final corpo = jsonDecode(utf8.decode(base64Url.decode(base64Url.normalize(partes[1]))));
      final exp = (corpo as Map)['exp'];
      return exp is int ? DateTime.fromMillisecondsSinceEpoch(exp * 1000) : null;
    } catch (_) {
      return null;
    }
  }
}

/// Token e usuario no armazenamento seguro do sistema (Keystore no Android).
class CofreSessao {
  const CofreSessao();

  static const _cofre = FlutterSecureStorage();
  static const _chaveToken = 'token';
  static const _chaveUsuario = 'usuario';

  Future<SessaoSalva?> ler() async {
    final token = await _cofre.read(key: _chaveToken);
    final usuario = await _cofre.read(key: _chaveUsuario);
    if (token == null || usuario == null) return null;
    return SessaoSalva(
      token: token,
      usuario: Usuario.deJson(jsonDecode(usuario) as Map<String, dynamic>),
    );
  }

  Future<void> salvar(SessaoSalva sessao) async {
    await _cofre.write(key: _chaveToken, value: sessao.token);
    await _cofre.write(key: _chaveUsuario, value: jsonEncode(sessao.usuario.paraJson()));
  }

  Future<void> apagar() async {
    await _cofre.delete(key: _chaveToken);
    await _cofre.delete(key: _chaveUsuario);
  }
}

/// Estado do receptor que precisa sobreviver a reabertura do app.
class PreferenciasReceptor {
  PreferenciasReceptor._(this._prefs);

  final SharedPreferences _prefs;

  static Future<PreferenciasReceptor> abrir() async =>
      PreferenciasReceptor._(await SharedPreferences.getInstance());

  static const _escutar = 'escutar';
  static const _dedup = 'dedup';
  static const _pulseiras = 'pulseiras_da_conta';

  /// O usuario deixou a escuta ligada neste celular.
  bool get escutar => _prefs.getBool(_escutar) ?? false;
  Future<void> salvarEscutar(bool valor) => _prefs.setBool(_escutar, valor);

  /// Ultimo seq tratado por pulseira: reabrir o app durante o anuncio de
  /// emergencia sustentado nao repete a sirene.
  Map<String, int> get dedup {
    final texto = _prefs.getString(_dedup);
    if (texto == null) return {};
    return (jsonDecode(texto) as Map<String, dynamic>).map((k, v) => MapEntry(k, v as int));
  }

  Future<void> salvarDedup(Map<String, int> estado) => _prefs.setString(_dedup, jsonEncode(estado));

  /// `ble_id` das pulseiras da conta, para alarmar mesmo sem conexao.
  Set<String> get pulseirasDaConta => (_prefs.getStringList(_pulseiras) ?? const []).toSet();

  Future<void> salvarPulseirasDaConta(Set<String> ids) => _prefs.setStringList(_pulseiras, ids.toList());

  Future<void> limpar() async {
    await _prefs.remove(_escutar);
    await _prefs.remove(_pulseiras);
  }
}
