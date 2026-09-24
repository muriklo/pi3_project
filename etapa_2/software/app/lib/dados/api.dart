import 'package:dio/dio.dart';

import '../dominio/modelos.dart';
import 'fila_envio.dart';

/// Falha de uma chamada a API.
class ErroApi implements Exception {
  const ErroApi(this.status, this.mensagem, {this.sessaoInvalida = false});

  /// null quando nem chegou a haver resposta (sem conexao, tempo esgotado).
  final int? status;
  final String mensagem;

  /// 401 por token vencido ou invalido. A API tambem responde 401 para
  /// assinatura invalida da pulseira (pendencia P4); so a sessao invalida
  /// traz o cabecalho WWW-Authenticate, e e por ele que os dois sao separados.
  final bool sessaoInvalida;

  bool get semConexao => status == null;

  @override
  String toString() => 'ErroApi($status): $mensagem';
}

class AmostraTelemetria {
  const AmostraTelemetria({
    required this.seq,
    required this.registradaEm,
    required this.bateriaPct,
    this.rssi,
  });

  final int seq;
  final DateTime registradaEm;
  final int bateriaPct;
  final int? rssi;

  Map<String, dynamic> paraJson() => {
        'seq': seq,
        'recorded_at': registradaEm.toUtc().toIso8601String(),
        'battery_pct': bateriaPct,
        'rssi': rssi,
      };
}

/// Cliente HTTP da API do SysCare. Rotas: Secao 5.2 da arquitetura.
class ApiSysCare {
  ApiSysCare({
    required String urlBase,
    required Future<String?> Function() token,
    required this.aoExpirarSessao,
    Dio? dio,
  }) : _dio = dio ??
            Dio(BaseOptions(
              baseUrl: urlBase,
              connectTimeout: const Duration(seconds: 8),
              receiveTimeout: const Duration(seconds: 15),
            )) {
    _dio.interceptors.add(InterceptorsWrapper(onRequest: (opcoes, handler) async {
      final t = await token();
      if (t != null) opcoes.headers['Authorization'] = 'Bearer $t';
      handler.next(opcoes);
    }));
  }

  final Dio _dio;

  /// Chamado quando a API responde que a sessao venceu.
  final void Function() aoExpirarSessao;

  Future<T> _chamar<T>(Future<Response<dynamic>> Function() requisicao, T Function(dynamic) ler) async {
    try {
      final resposta = await requisicao();
      return ler(resposta.data);
    } on DioException catch (e) {
      final resposta = e.response;
      if (resposta == null) {
        throw const ErroApi(null, 'Sem conexão com o servidor.');
      }
      final sessaoInvalida =
          resposta.statusCode == 401 && resposta.headers.value('www-authenticate') != null;
      if (sessaoInvalida) aoExpirarSessao();
      throw ErroApi(resposta.statusCode, _mensagem(resposta.data), sessaoInvalida: sessaoInvalida);
    }
  }

  static String _mensagem(Object? corpo) {
    if (corpo is Map && corpo['detail'] != null) {
      final d = corpo['detail'];
      // 422 de validacao do FastAPI traz uma lista de erros.
      if (d is List && d.isNotEmpty && d.first is Map) return '${(d.first as Map)['msg']}';
      return '$d';
    }
    return 'Erro inesperado do servidor.';
  }

  // ------------------------------------------------------------- conta ----
  Future<({String token, Usuario usuario})> entrar(String email, String senha) => _chamar(
        () => _dio.post('/v1/auth/login', data: {'email': email, 'password': senha}),
        _lerToken,
      );

  Future<({String token, Usuario usuario})> criarConta(String nome, String email, String senha) =>
      _chamar(
        () => _dio.post('/v1/auth/register',
            data: {'name': nome, 'email': email, 'password': senha}),
        _lerToken,
      );

  static ({String token, Usuario usuario}) _lerToken(dynamic j) => (
        token: j['access_token'] as String,
        usuario: Usuario.deJson(j['user'] as Map<String, dynamic>),
      );

  // --------------------------------------------------------- pulseiras ----
  Future<List<Pulseira>> pulseiras() => _chamar(
        () => _dio.get('/v1/devices'),
        (j) => [for (final p in j as List) Pulseira.deJson(p as Map<String, dynamic>)],
      );

  Future<Pulseira> pulseira(String id) =>
      _chamar(() => _dio.get('/v1/devices/$id'), (j) => Pulseira.deJson(j as Map<String, dynamic>));

  /// Devolve a pulseira e a chave do firmware, que a API mostra uma unica vez.
  Future<({Pulseira pulseira, String chave})> cadastrarPulseira({
    required String bleId,
    required String nome,
    String? nomeUsuario,
  }) =>
      _chamar(
        () => _dio.post('/v1/devices',
            data: {'ble_id': bleId, 'name': nome, 'wearer_name': nomeUsuario}),
        (j) => (
          pulseira: Pulseira.deJson(j as Map<String, dynamic>),
          chave: j['shared_secret'] as String,
        ),
      );

  Future<Pulseira> editarPulseira(String id, {String? nome, String? nomeUsuario}) => _chamar(
        () => _dio.patch('/v1/devices/$id', data: {
          'name': ?nome,
          'wearer_name': ?nomeUsuario,
        }),
        (j) => Pulseira.deJson(j as Map<String, dynamic>),
      );

  // ------------------------------------------------------ responsaveis ----
  Future<List<Responsavel>> responsaveis(String pulseiraId) => _chamar(
        () => _dio.get('/v1/devices/$pulseiraId/caregivers'),
        (j) => [for (final r in j as List) Responsavel.deJson(r as Map<String, dynamic>)],
      );

  Future<Responsavel> adicionarResponsavel(
    String pulseiraId, {
    required String nome,
    String? telefone,
    String? email,
    String? usuarioId,
    int prioridade = 1,
  }) =>
      _chamar(
        () => _dio.post('/v1/devices/$pulseiraId/caregivers', data: {
          'name': nome,
          'phone': telefone,
          'email': email,
          // Conta do app que recebe o push e pode atender (pendencia P6: a API
          // nao tem como achar a conta pelo e-mail; o responsavel passa o codigo).
          'user_id': usuarioId,
          'priority': prioridade,
        }),
        (j) => Responsavel.deJson(j as Map<String, dynamic>),
      );

  Future<Responsavel> editarResponsavel(String pulseiraId, String id, {int? prioridade, bool? ativo}) =>
      _chamar(
        () => _dio.patch('/v1/devices/$pulseiraId/caregivers/$id', data: {
          'priority': ?prioridade,
          'active': ?ativo,
        }),
        (j) => Responsavel.deJson(j as Map<String, dynamic>),
      );

  Future<void> removerResponsavel(String pulseiraId, String id) =>
      _chamar(() => _dio.delete('/v1/devices/$pulseiraId/caregivers/$id'), (_) {});

  // ----------------------------------------------------------- alertas ----
  /// Repassa o evento ouvido. Vao os bytes originais em `raw_payload`: e
  /// sobre eles que a pulseira calculou a assinatura.
  Future<ResultadoEnvio> enviarAlerta(ItemFila item) => _chamar(
        () => _dio.post('/v1/alerts', data: {
          'ble_id': item.anuncio.bleId,
          'raw_payload': item.anuncio.hex,
          'occurred_at': item.recebidoEm.toUtc().toIso8601String(),
          'rssi': item.rssi,
          'latitude': item.posicao?.latitude,
          'longitude': item.posicao?.longitude,
          'location_accuracy_m': item.posicao?.precisaoM,
          'gateway_label': item.rotulo,
        }),
        (j) => ResultadoEnvio.deJson(j as Map<String, dynamic>),
      );

  Future<List<Alerta>> alertas({String? pulseiraId}) => _chamar(
        () => _dio.get('/v1/alerts', queryParameters: {'device_id': ?pulseiraId}),
        (j) => [for (final a in j as List) Alerta.deJson(a as Map<String, dynamic>)],
      );

  Future<Alerta> alerta(String id) =>
      _chamar(() => _dio.get('/v1/alerts/$id'), (j) => Alerta.deJson(j as Map<String, dynamic>));

  /// "Estou indo": interrompe o reenvio para os demais responsaveis.
  Future<Alerta> confirmar(String id) =>
      _chamar(() => _dio.post('/v1/alerts/$id/ack'), (j) => Alerta.deJson(j as Map<String, dynamic>));

  Future<Alerta> encerrar(String id, {required bool falsoAlarme, String? notas}) => _chamar(
        () => _dio.post('/v1/alerts/$id/resolve', data: {
          'status': falsoAlarme ? 'false_positive' : 'resolved',
          'notes': notas,
        }),
        (j) => Alerta.deJson(j as Map<String, dynamic>),
      );

  // -------------------------------------------------------- telemetria ----
  Future<void> enviarTelemetria(String bleId, List<AmostraTelemetria> amostras) => _chamar(
        () => _dio.post('/v1/telemetry', data: {
          'ble_id': bleId,
          'samples': [for (final a in amostras) a.paraJson()],
        }),
        (_) {},
      );

  Future<bool> servidorNoAr() async {
    try {
      return await _chamar(() => _dio.get('/health'), (j) => (j as Map)['status'] == 'ok');
    } on ErroApi {
      return false;
    }
  }
}
