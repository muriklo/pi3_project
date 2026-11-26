import 'dart:async';

import 'package:firebase_messaging/firebase_messaging.dart' show RemoteMessage;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:syscare_protocol/syscare_protocol.dart';

import '../dados/alarme.dart';
import '../dados/api.dart';
import '../dados/armazenamento.dart';
import '../dominio/modelos.dart';
import 'providers.dart';

/// Alerta que chegou pelo push, lido do `data` montado pela API
/// (app/notifications/dispatcher.py).
class AvisoPush {
  const AvisoPush({
    required this.alertaId,
    required this.tipo,
    required this.titulo,
    required this.texto,
    this.bleId,
    this.seq,
  });

  /// null quando a mensagem nao e um alerta do SysCare.
  static AvisoPush? deDados(Map<String, dynamic> dados) {
    final id = dados['alert_id'];
    final tipo = dados['event_type'];
    if (id is! String || id.isEmpty || tipo is! String) return null;
    return AvisoPush(
      alertaId: id,
      tipo: tipo,
      titulo: rotulosEvento[tipo] ?? '${dados['title'] ?? 'Alerta do SysCare'}',
      texto: '${dados['body'] ?? dados['wearer_name'] ?? ''}',
      bleId: dados['ble_id'] as String?,
      seq: int.tryParse('${dados['seq']}'),
    );
  }

  final String alertaId;

  /// `event_type` da API.
  final String tipo;
  final String titulo;
  final String texto;

  /// Identificam o anuncio de origem: e por eles que o celular que ouviu a
  /// pulseira reconhece o proprio evento.
  final String? bleId;
  final int? seq;

  bool get emergencia => TipoEvento.values.any((t) => t.nomeApi == tipo && t.ehEmergencia);
}

enum ReacaoPush { alarmar, avisar, jaTocando }

/// [ouvidos] e o estado do deduplicador do receptor (ultimo seq por pulseira):
/// se este celular ouviu o mesmo evento pelo radio, a sirene local ja tocou.
ReacaoPush decidirPush(AvisoPush aviso, Map<String, int> ouvidos) {
  if (aviso.bleId != null && aviso.seq != null && ouvidos[aviso.bleId] == aviso.seq) {
    return ReacaoPush.jaTocando;
  }
  return aviso.emergencia ? ReacaoPush.alarmar : ReacaoPush.avisar;
}

/// Id da notificacao do push, igual em qualquer isolate: a tela de alerta o
/// recalcula a partir do id do alerta para silenciar a sirene.
int idDoAlerta(String alertaId) {
  var h = 0x811c9dc5; // FNV-1a de 32 bits
  for (final c in alertaId.codeUnits) {
    h = ((h ^ c) * 0x01000193) & 0xffffffff;
  }
  return h & 0x7fffffff;
}

/// Toca o alarme do push. Devolve o aviso quando houve alarme ou aviso novo.
Future<AvisoPush?> tratarPush(
  Map<String, dynamic> dados, {
  required Alarme alarme,
  required Map<String, int> ouvidos,
}) async {
  final aviso = AvisoPush.deDados(dados);
  if (aviso == null) return null;
  switch (decidirPush(aviso, ouvidos)) {
    case ReacaoPush.jaTocando:
      return null;
    case ReacaoPush.alarmar:
      await alarme.emergencia(
        id: idDoAlerta(aviso.alertaId),
        titulo: aviso.titulo.toUpperCase(),
        texto: aviso.texto,
        payload: 'alerta:${aviso.alertaId}',
      );
    case ReacaoPush.avisar:
      await alarme.aviso(
        id: idDoAlerta(aviso.alertaId),
        titulo: aviso.titulo,
        texto: aviso.texto,
        payload: 'alerta:${aviso.alertaId}',
      );
  }
  return aviso;
}

/// Push com o app fechado ou em segundo plano: roda num isolate a parte, sem
/// o ProviderContainer do app, entao abre o que precisa por conta propria.
@pragma('vm:entry-point')
Future<void> pushEmSegundoPlano(RemoteMessage mensagem) async {
  final alarme = Alarme();
  await alarme.inicializar((_) {});
  final preferencias = await PreferenciasReceptor.abrir();
  await tratarPush(mensagem.data, alarme: alarme, ouvidos: preferencias.dedup);
}

final registroPushProvider = Provider<RegistroPush>((ref) {
  final registro = RegistroPush(ref);
  ref.onDispose(registro._parar);
  return registro;
});

/// Mantem o token deste celular registrado na API enquanto ha sessao
/// (`POST /v1/auth/push-tokens`), e o remove no logout.
class RegistroPush {
  RegistroPush(this._ref);

  final Ref _ref;
  StreamSubscription<String>? _renovacao;

  /// Depois do login e a cada abertura do app com sessao: o token pode ter
  /// mudado com o app fechado.
  Future<void> registrar() async {
    final push = _ref.read(pushProvider);
    if (!push.ativo || _ref.read(sessaoProvider) == null) return;
    try {
      await push.pedirPermissao();
      final token = await push.token();
      if (token != null) await _enviar(token);
      _renovacao ??= push.tokenRenovado.listen(_enviar);
    } catch (_) {
      // Celular sem Google Play Services: segue sem push.
    }
  }

  Future<void> _enviar(String token) async {
    try {
      final id = await _ref.read(apiProvider).registrarPush(token, _ref.read(pushProvider).plataforma);
      await _ref.read(preferenciasProvider).salvarPushId(id);
    } on ErroApi {
      // Sem conexao: tenta de novo na proxima abertura do app.
    }
  }

  /// No logout: este celular deixa de receber os alertas da conta.
  Future<void> remover() async {
    await _parar();
    final preferencias = _ref.read(preferenciasProvider);
    final id = preferencias.pushId;
    if (id != null) {
      try {
        await _ref.read(apiProvider).removerPush(id);
      } on ErroApi {
        // Sem conexao: o apagarToken abaixo invalida o token no Firebase.
      }
      await preferencias.salvarPushId(null);
    }
    final push = _ref.read(pushProvider);
    if (!push.ativo) return;
    try {
      await push.apagarToken();
    } catch (_) {}
  }

  Future<void> _parar() async {
    await _renovacao?.cancel();
    _renovacao = null;
  }
}

/// A sirene do push deste alerta ainda toca neste celular.
final sirenePushProvider = FutureProvider.autoDispose.family<bool, String>(
  (ref, alertaId) => ref.read(alarmeProvider).tocando(idDoAlerta(alertaId)),
);
