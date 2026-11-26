import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:dio/dio.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:syscare_app/aplicacao/providers.dart';
import 'package:syscare_app/aplicacao/push.dart';
import 'package:syscare_app/dados/alarme.dart';
import 'package:syscare_app/dados/api.dart';
import 'package:syscare_app/dados/armazenamento.dart';
import 'package:syscare_app/dados/push.dart';
import 'package:syscare_app/dominio/modelos.dart';

/// O `data` que a API manda (app/notifications/dispatcher.py + fcm.py).
Map<String, dynamic> dadosDaApi({String tipo = 'fall', String seq = '43'}) => {
      'alert_id': 'f3a9c2d4e5b6478890aabbccddeeff00',
      'device_id': 'd1',
      'event_type': tipo,
      'occurred_at': '2026-10-15T13:00:00+00:00',
      'wearer_name': 'Sr. Jose',
      'ble_id': 'a1b2c3d4',
      'seq': seq,
      'escalation_round': '0',
      'title': 'QUEDA DETECTADA',
      'body': 'Sr. Jose - impacto de 3.2 g - bateria 87%',
    };

class _AlarmeGravador extends Alarme {
  final chamadas = <String>[];

  @override
  Future<void> emergencia({
    required int id,
    required String titulo,
    required String texto,
    required String payload,
  }) async =>
      chamadas.add('emergencia $id $titulo $payload');

  @override
  Future<void> aviso({required int id, required String titulo, required String texto, String? payload}) async =>
      chamadas.add('aviso $id $titulo $payload');
}

class _PushFalso extends PushFirebase {
  final renovacoes = StreamController<String>.broadcast();
  int apagados = 0;

  @override
  bool get ativo => true;

  @override
  Future<void> pedirPermissao() async {}

  @override
  Future<String?> token() async => 'token-fcm-do-celular';

  @override
  Stream<String> get tokenRenovado => renovacoes.stream;

  @override
  Future<void> apagarToken() async => apagados++;
}

/// Guarda as requisicoes e responde como a API.
class _ApiFalsa implements HttpClientAdapter {
  final pedidos = <String>[];

  @override
  Future<ResponseBody> fetch(RequestOptions opcoes, Stream<Uint8List>? corpo, Future<void>? _) async {
    final dados = opcoes.data is Map ? ' ${jsonEncode(opcoes.data)}' : '';
    pedidos.add('${opcoes.method} ${opcoes.path}$dados');
    if (opcoes.method == 'DELETE') return ResponseBody.fromString('', 204);
    return ResponseBody.fromString(
      jsonEncode({'id': 'pt-${pedidos.length}', 'platform': 'android', 'created_at': '2026-10-15T13:00:00Z'}),
      201,
      headers: {Headers.contentTypeHeader: ['application/json']},
    );
  }

  @override
  void close({bool force = false}) {}
}

void main() {
  group('mensagem do push', () {
    test('le o alerta que a API manda', () {
      final aviso = AvisoPush.deDados(dadosDaApi())!;
      expect(aviso.alertaId, 'f3a9c2d4e5b6478890aabbccddeeff00');
      expect(aviso.bleId, 'a1b2c3d4');
      expect(aviso.seq, 43);
      expect(aviso.emergencia, isTrue);
      expect(aviso.titulo, 'Queda detectada');
      expect(aviso.texto, startsWith('Sr. Jose'));
    });

    test('mensagem que nao e alerta do SysCare e ignorada', () {
      expect(AvisoPush.deDados({'qualquer': 'coisa'}), isNull);
    });
  });

  group('tocar ou nao', () {
    test('queda de longe toca a sirene', () {
      expect(decidirPush(AvisoPush.deDados(dadosDaApi())!, {}), ReacaoPush.alarmar);
    });

    test('o celular que ouviu o mesmo evento pelo radio nao toca de novo', () {
      final aviso = AvisoPush.deDados(dadosDaApi())!;
      expect(decidirPush(aviso, {'a1b2c3d4': 43}), ReacaoPush.jaTocando);
      // Evento seguinte da mesma pulseira: e outra queda, toca.
      expect(decidirPush(aviso, {'a1b2c3d4': 42}), ReacaoPush.alarmar);
    });

    test('bateria baixa vira aviso, sem sirene', () {
      expect(decidirPush(AvisoPush.deDados(dadosDaApi(tipo: 'low_battery'))!, {}), ReacaoPush.avisar);
    });

    test('a sirene do push abre a tela do alerta no servidor', () async {
      final alarme = _AlarmeGravador();
      final aviso = await tratarPush(dadosDaApi(), alarme: alarme, ouvidos: {});
      expect(aviso, isNotNull);
      expect(alarme.chamadas, [
        'emergencia ${idDoAlerta('f3a9c2d4e5b6478890aabbccddeeff00')} QUEDA DETECTADA '
            'alerta:f3a9c2d4e5b6478890aabbccddeeff00',
      ]);
    });

    test('ja tocando: nao chama o alarme', () async {
      final alarme = _AlarmeGravador();
      expect(await tratarPush(dadosDaApi(), alarme: alarme, ouvidos: {'a1b2c3d4': 43}), isNull);
      expect(alarme.chamadas, isEmpty);
    });
  });

  test('id da notificacao: estavel, positivo e diferente por alerta', () {
    final a = idDoAlerta('f3a9c2d4e5b6478890aabbccddeeff00');
    expect(idDoAlerta('f3a9c2d4e5b6478890aabbccddeeff00'), a);
    expect(a, inInclusiveRange(0, 0x7fffffff));
    expect(idDoAlerta('f3a9c2d4e5b6478890aabbccddeeff01'), isNot(a));
  });

  group('registro do token', () {
    late _ApiFalsa servidor;
    late _PushFalso push;
    late PreferenciasReceptor preferencias;
    late ProviderContainer container;

    setUp(() async {
      SharedPreferences.setMockInitialValues({});
      preferencias = await PreferenciasReceptor.abrir();
      servidor = _ApiFalsa();
      push = _PushFalso();
      final dio = Dio(BaseOptions(baseUrl: 'http://api'))..httpClientAdapter = servidor;
      container = ProviderContainer(overrides: [
        preferenciasProvider.overrideWithValue(preferencias),
        pushProvider.overrideWithValue(push),
        sessaoInicialProvider.overrideWithValue(const SessaoSalva(
          token: 'x',
          usuario: Usuario(id: 'u1', email: 'a@b.c', nome: 'Ana'),
        )),
        apiProvider.overrideWithValue(
          ApiSysCare(urlBase: 'http://api', token: () async => 'x', aoExpirarSessao: () {}, dio: dio),
        ),
      ]);
      addTearDown(container.dispose);
    });

    test('registra depois do login e guarda o id para o logout', () async {
      await container.read(registroPushProvider).registrar();
      expect(servidor.pedidos, ['POST /v1/auth/push-tokens {"token":"token-fcm-do-celular","platform":"android"}']);
      expect(preferencias.pushId, 'pt-1');
    });

    test('token renovado pelo Firebase vai de novo para a API', () async {
      await container.read(registroPushProvider).registrar();
      push.renovacoes.add('token-novo');
      await pumpEventQueue();
      expect(servidor.pedidos.last, 'POST /v1/auth/push-tokens {"token":"token-novo","platform":"android"}');
    });

    test('no logout remove o token da API e do Firebase', () async {
      await container.read(registroPushProvider).registrar();
      await container.read(registroPushProvider).remover();
      expect(servidor.pedidos.last, 'DELETE /v1/auth/push-tokens/pt-1');
      expect(preferencias.pushId, isNull);
      expect(push.apagados, 1);
    });

    test('build sem Firebase nao chama a API', () async {
      final semFirebase = ProviderContainer(overrides: [
        preferenciasProvider.overrideWithValue(preferencias),
        apiProvider.overrideWithValue(ApiSysCare(
          urlBase: 'http://api',
          token: () async => 'x',
          aoExpirarSessao: () {},
          dio: Dio()..httpClientAdapter = servidor,
        )),
      ]);
      addTearDown(semFirebase.dispose);
      await semFirebase.read(registroPushProvider).registrar();
      await semFirebase.read(registroPushProvider).remover();
      expect(servidor.pedidos, isEmpty);
    });
  });
}
