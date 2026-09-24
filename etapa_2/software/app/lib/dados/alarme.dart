import 'dart:typed_data';

import 'package:flutter_local_notifications/flutter_local_notifications.dart';

/// Alarme local: sirene, vibracao e tela cheia, sem depender de rede.
///
/// O canal `syscare_emergencia` e o som `sirene` sao os mesmos identificadores
/// que a API manda no push (app/notifications/fcm.py): o alarme soa igual
/// venha do radio ou do servidor.
class Alarme {
  final _plugin = FlutterLocalNotificationsPlugin();

  static const canalEmergencia = AndroidNotificationChannel(
    'syscare_emergencia',
    'Emergências',
    description: 'Quedas e botão de emergência. Toca como um despertador.',
    importance: Importance.max,
    sound: RawResourceAndroidNotificationSound('sirene'),
    // Fluxo de alarme: o mesmo do despertador, ativo no modo silencioso.
    audioAttributesUsage: AudioAttributesUsage.alarm,
  );

  static const canalAvisos = AndroidNotificationChannel(
    'syscare_avisos',
    'Avisos',
    description: 'Bateria baixa e testes da pulseira.',
    importance: Importance.high,
  );

  /// Repete o som ate alguem silenciar (Notification.FLAG_INSISTENT).
  static const _flagInsistente = 4;

  AndroidFlutterLocalNotificationsPlugin? get _android =>
      _plugin.resolvePlatformSpecificImplementation<AndroidFlutterLocalNotificationsPlugin>();

  Future<void> inicializar(void Function(String? payload) aoTocar) async {
    await _plugin.initialize(
      settings: const InitializationSettings(
        android: AndroidInitializationSettings('@mipmap/ic_launcher'),
        iOS: DarwinInitializationSettings(),
      ),
      onDidReceiveNotificationResponse: (r) => aoTocar(r.payload),
    );
    await _android?.createNotificationChannel(canalEmergencia);
    await _android?.createNotificationChannel(canalAvisos);
  }

  /// Payload da notificacao que abriu o app, se foi assim que ele abriu.
  Future<String?> payloadDeAbertura() async {
    final detalhes = await _plugin.getNotificationAppLaunchDetails();
    if (detalhes == null || !detalhes.didNotificationLaunchApp) return null;
    return detalhes.notificationResponse?.payload;
  }

  Future<bool> notificacoesPermitidas() async => await _android?.areNotificationsEnabled() ?? true;

  Future<bool> pedirNotificacoes() async => await _android?.requestNotificationsPermission() ?? true;

  /// Android 14+: so apps de chamada e alarme tem a tela cheia por padrao.
  Future<bool> pedirTelaCheia() async => await _android?.requestFullScreenIntentPermission() ?? true;

  Future<void> emergencia({
    required int id,
    required String titulo,
    required String texto,
    required String payload,
  }) =>
      _plugin.show(
        id: id,
        title: titulo,
        body: texto,
        payload: payload,
        notificationDetails: NotificationDetails(
          android: AndroidNotificationDetails(
            canalEmergencia.id,
            canalEmergencia.name,
            channelDescription: canalEmergencia.description,
            importance: Importance.max,
            priority: Priority.max,
            sound: const RawResourceAndroidNotificationSound('sirene'),
            audioAttributesUsage: AudioAttributesUsage.alarm,
            category: AndroidNotificationCategory.alarm,
            visibility: NotificationVisibility.public,
            fullScreenIntent: true,
            ongoing: true,
            autoCancel: false,
            additionalFlags: Int32List.fromList([_flagInsistente]),
          ),
        ),
      );

  Future<void> aviso({required int id, required String titulo, required String texto, String? payload}) =>
      _plugin.show(
        id: id,
        title: titulo,
        body: texto,
        payload: payload,
        notificationDetails: NotificationDetails(
          android: AndroidNotificationDetails(
            canalAvisos.id,
            canalAvisos.name,
            channelDescription: canalAvisos.description,
            importance: Importance.high,
            priority: Priority.high,
          ),
        ),
      );

  Future<void> silenciar(int id) => _plugin.cancel(id: id);
}
