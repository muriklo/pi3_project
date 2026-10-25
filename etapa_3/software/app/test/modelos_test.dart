import 'package:flutter_test/flutter_test.dart';
import 'package:syscare_app/dominio/modelos.dart';

import 'apoio.dart';

/// Contrato: os modelos leem as respostas REAIS da API, nao exemplos escritos a mao.
void main() {
  test('usuario do registro', () {
    final u = Usuario.deJson(respostasApi['registro']['user'] as Map<String, dynamic>);
    expect(u.nome, 'Maria');
    expect(u.email, 'maria@exemplo.com');
  });

  test('pulseira traz o dono, para o app saber se mostra a edicao', () {
    final lista = respostasApi['pulseiras'] as List;
    final p = Pulseira.deJson(lista.first as Map<String, dynamic>);
    final dona = respostasApi['registro']['user']['id'] as String;
    expect(p.bleId, 'a1b2c3d4');
    expect(p.titulo, 'Sr. Joao');
    expect(p.ehDono(dona), isTrue);
    expect(p.ehDono('outra-conta'), isFalse);
    expect(p.bateriaPct, 87);
  });

  test('data sem fuso vinda do SQLite e lida como UTC', () {
    final bruto = respostasApi['envio_alerta']['alert']['occurred_at'] as String;
    expect(bruto.endsWith('Z'), isFalse, reason: 'o caso que o parser precisa cobrir');
    final a = Alerta.deJson(respostasApi['envio_alerta']['alert'] as Map<String, dynamic>);
    expect(a.ocorridoEm.toUtc(), DateTime.parse('${bruto}Z'));
  });

  test('resultado do envio do alerta', () {
    final r = ResultadoEnvio.deJson(respostasApi['envio_alerta'] as Map<String, dynamic>);
    expect(r.duplicado, isFalse);
    expect(r.notificados, 1);
    expect(r.alerta.tipo, 'fall');
    expect(r.alerta.aberto, isTrue);
    expect(r.alerta.impactoG, closeTo(3.2, 1e-9));
    expect(r.alerta.latitude, closeTo(-27.5954, 1e-9));
    expect(r.alerta.entregas, isNotEmpty);
    expect(rotulosEvento[r.alerta.tipo], 'Queda detectada');
  });

  test('alerta confirmado', () {
    final a = Alerta.deJson(respostasApi['alerta_confirmado'] as Map<String, dynamic>);
    expect(a.status, 'acked');
    expect(a.confirmadoEm, isNotNull);
    expect(a.aberto, isFalse);
    expect(a.encerrado, isFalse);
  });

  test('responsavel com conta no app', () {
    final r = Responsavel.deJson(respostasApi['responsavel'] as Map<String, dynamic>);
    expect(r.usuarioId, respostasApi['registro']['user']['id']);
    expect(r.telefone, '+5548999990000');
    expect(r.ativo, isTrue);
  });
}
