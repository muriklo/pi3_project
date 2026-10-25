import 'dart:convert';
import 'dart:typed_data';

import 'package:dio/dio.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:syscare_app/dados/api.dart';

import 'apoio.dart';

/// Devolve sempre a mesma resposta, ou falha de conexao se [status] for null.
class _Adaptador implements HttpClientAdapter {
  _Adaptador(this.status, this.corpo, {this.cabecalhos = const {}});

  final int? status;
  final Object? corpo;
  final Map<String, List<String>> cabecalhos;

  @override
  Future<ResponseBody> fetch(RequestOptions opcoes, Stream<Uint8List>? _, Future<void>? _) async {
    if (status == null) {
      throw DioException(requestOptions: opcoes, type: DioExceptionType.connectionError);
    }
    return ResponseBody.fromString(jsonEncode(corpo), status!, headers: {
      Headers.contentTypeHeader: ['application/json'],
      ...cabecalhos,
    });
  }

  @override
  void close({bool force = false}) {}
}

({ApiSysCare api, List<int> expirou}) _api(_Adaptador adaptador) {
  final expirou = <int>[];
  final dio = Dio(BaseOptions(baseUrl: 'http://api'))..httpClientAdapter = adaptador;
  final api = ApiSysCare(
    urlBase: 'http://api',
    token: () async => 'token',
    aoExpirarSessao: () => expirou.add(1),
    dio: dio,
  );
  return (api: api, expirou: expirou);
}

Future<ErroApi> _erroDe(Future<Object?> chamada) async {
  try {
    await chamada;
  } on ErroApi catch (e) {
    return e;
  }
  fail('deveria ter lancado ErroApi');
}

void main() {
  // Pendencia P4: a API responde 401 nos dois casos; o cabecalho separa.
  test('401 de sessao vencida (com WWW-Authenticate) encerra a sessao', () async {
    final real = respostasApi['erro_sessao'] as Map<String, dynamic>;
    final (:api, :expirou) = _api(_Adaptador(real['status'] as int, real['corpo'],
        cabecalhos: {'www-authenticate': [real['www_authenticate'] as String]}));

    final erro = await _erroDe(api.pulseiras());
    expect(erro.status, 401);
    expect(erro.sessaoInvalida, isTrue);
    expect(expirou, hasLength(1));
  });

  test('401 de assinatura invalida NAO encerra a sessao', () async {
    final real = respostasApi['erro_assinatura'] as Map<String, dynamic>;
    expect(real['www_authenticate'], isNull, reason: 'e isso que distingue os dois 401');
    final (:api, :expirou) = _api(_Adaptador(real['status'] as int, real['corpo']));

    final erro = await _erroDe(api.pulseiras());
    expect(erro.status, 401);
    expect(erro.sessaoInvalida, isFalse);
    expect(erro.mensagem, 'Assinatura da pulseira invalida.');
    expect(expirou, isEmpty);
  });

  test('sem conexao vira ErroApi sem status', () async {
    final (:api, expirou: _) = _api(_Adaptador(null, null));
    final erro = await _erroDe(api.pulseiras());
    expect(erro.semConexao, isTrue);
  });

  test('mensagem do erro de validacao do FastAPI', () async {
    final (:api, expirou: _) = _api(_Adaptador(422, {
      'detail': [
        {'msg': 'String should have at least 8 characters'},
      ],
    }));
    final erro = await _erroDe(api.criarConta('Ana', 'ana@exemplo.com', '123'));
    expect(erro.mensagem, 'String should have at least 8 characters');
  });

  test('pulseiras reais da API sao lidas', () async {
    final (:api, expirou: _) = _api(_Adaptador(200, respostasApi['pulseiras']));
    final lista = await api.pulseiras();
    expect(lista.single.bleId, 'a1b2c3d4');
  });
}
