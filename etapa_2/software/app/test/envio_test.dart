import 'package:flutter_test/flutter_test.dart';
import 'package:syscare_app/aplicacao/envio.dart';
import 'package:syscare_app/dados/api.dart';
import 'package:syscare_app/dados/fila_envio.dart';
import 'package:syscare_app/dominio/modelos.dart';

import 'apoio.dart';

/// Tabela de respostas da Secao 5.4 da arquitetura.
void main() {
  final agora = DateTime.utc(2026, 9, 23, 12);
  final resultadoReal = ResultadoEnvio.deJson(respostasApi['envio_alerta'] as Map<String, dynamic>);

  late FilaMemoria fila;
  late List<(String, Desfecho)> vistos;

  EnvioAlertas envio(Future<ResultadoEnvio> Function(ItemFila) enviar) => EnvioAlertas(
        fila: fila,
        enviar: enviar,
        agora: () => agora,
        aoResultado: (item, r) => vistos.add((item.chave, r.desfecho)),
      );

  Future<void> enfileirar({int seq = 43}) =>
      fila.adicionar(ItemFila(anuncio: anuncio(seq: seq), recebidoEm: agora, proximaEm: agora));

  setUp(() {
    fila = FilaMemoria();
    vistos = [];
  });

  test('201: sai da fila', () async {
    await enfileirar();
    await envio((_) async => resultadoReal).drenar();
    expect(vistos, [('a1b2c3d4-43', Desfecho.enviado)]);
    expect(await fila.total(), 0);
  });

  test('sem conexao: fica na fila com espera crescente e para de tentar os outros', () async {
    await enfileirar(seq: 43);
    await enfileirar(seq: 44);
    var chamadas = 0;
    await envio((_) async {
      chamadas++;
      throw const ErroApi(null, 'sem rede');
    }).drenar();
    expect(chamadas, 1, reason: 'sem rede, o segundo falharia igual');
    expect(await fila.total(), 2);
    final adiado = fila.itens.values.first;
    expect(adiado.tentativas, 1);
    expect(adiado.proximaEm, agora.add(EnvioAlertas.espera(1)));
  });

  test('5xx conta como falha passageira', () async {
    await enfileirar();
    await envio((_) async => throw const ErroApi(503, 'fora do ar')).drenar();
    expect(vistos.single.$2, Desfecho.semConexao);
    expect(await fila.total(), 1);
  });

  test('401 por assinatura: descarta e rebaixa o alarme', () async {
    await enfileirar();
    await envio((_) async => throw const ErroApi(401, 'Assinatura da pulseira invalida.')).drenar();
    expect(vistos.single.$2, Desfecho.rejeitadoAssinatura);
    expect(const ResultadoItem(Desfecho.rejeitadoAssinatura).rebaixaAlarme, isTrue);
    expect(await fila.total(), 0);
  });

  test('401 por sessao: fica na fila, sem gastar tentativa, ate o novo login', () async {
    await enfileirar();
    await envio((_) async => throw const ErroApi(401, 'Token invalido.', sessaoInvalida: true)).drenar();
    expect(vistos.single.$2, Desfecho.sessaoExpirada);
    expect(fila.itens.values.single.tentativas, 0);
  });

  test('409 (seq antigo) rebaixa; 404 e 422 so descartam', () async {
    for (final (status, esperado) in [
      (409, Desfecho.retransmissao),
      (404, Desfecho.descartado),
      (422, Desfecho.descartado),
    ]) {
      fila = FilaMemoria();
      vistos = [];
      await enfileirar();
      await envio((_) async => throw ErroApi(status, 'x')).drenar();
      expect(vistos.single.$2, esperado, reason: 'status $status');
      expect(await fila.total(), 0);
    }
  });

  test('espera cresce ate o teto de 5 min', () {
    expect([for (var t = 1; t <= 6; t++) EnvioAlertas.espera(t).inSeconds], [5, 15, 60, 300, 300, 300]);
  });
}
