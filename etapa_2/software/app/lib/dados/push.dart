// O firebase_messaging 17 marcou getToken/onTokenRefresh/deleteToken como
// obsoletos em favor do registro por FID (register/onRegistered). A API envia
// pelo campo `token` do HTTP v1 (app/notifications/fcm.py); trocar para FID
// muda o destino no servidor tambem, entao os dois lados migram juntos.
// ignore_for_file: deprecated_member_use

import 'dart:io' show Platform;

import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_messaging/firebase_messaging.dart';

/// Push do FCM (Camada 2): o alerta chega ao responsavel que esta longe da
/// pulseira.
///
/// So funciona em um build com o `android/app/google-services.json` (lido em
/// `android/app/build.gradle.kts`). Sem ele [iniciar] devolve false e o app
/// segue sem push, com o alarme local e o SMS funcionando igual.
class PushFirebase {
  bool _ativo = false;

  /// Firebase configurado neste build.
  bool get ativo => _ativo;

  /// [emSegundoPlano] precisa ser uma funcao de topo marcada com
  /// `@pragma('vm:entry-point')`: o Android a executa num isolate proprio,
  /// com o app fechado.
  Future<bool> iniciar(Future<void> Function(RemoteMessage) emSegundoPlano) async {
    try {
      await Firebase.initializeApp();
    } catch (_) {
      return _ativo = false;
    }
    FirebaseMessaging.onBackgroundMessage(emSegundoPlano);
    return _ativo = true;
  }

  String get plataforma => Platform.isIOS ? 'ios' : 'android';

  /// Android 13+ e iOS: sem a permissao de notificacao o push chega mudo.
  Future<void> pedirPermissao() => FirebaseMessaging.instance.requestPermission();

  Future<String?> token() => FirebaseMessaging.instance.getToken();

  /// O FCM troca o token de tempos em tempos (reinstalacao, limpeza de dados).
  Stream<String> get tokenRenovado => FirebaseMessaging.instance.onTokenRefresh;

  /// No logout: o token antigo deixa de valer no proprio Firebase.
  Future<void> apagarToken() => FirebaseMessaging.instance.deleteToken();

  /// Mensagens que chegam com o app aberto (o isolate de segundo plano nao roda).
  Stream<Map<String, dynamic>> get emPrimeiroPlano => FirebaseMessaging.onMessage.map((m) => m.data);
}
