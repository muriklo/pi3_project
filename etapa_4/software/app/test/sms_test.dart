import 'package:flutter_test/flutter_test.dart';
import 'package:syscare_app/aplicacao/sms_emergencia.dart';
import 'package:syscare_app/dados/localizacao.dart';
import 'package:syscare_app/dominio/modelos.dart';

void main() {
  final quando = DateTime(2026, 9, 24, 14, 5);
  const posicao = Posicao(latitude: -27.5954321, longitude: -48.548012, precisaoM: 8);

  group('texto do SMS', () {
    test('mesmo formato do SMS da API, com hora e link do mapa', () {
      expect(
        textoSms(tipo: 'fall', quem: 'Sr. João', quando: quando, posicao: posicao),
        'SysCare: QUEDA DETECTADA - Sr. Joao - as 14:05 - '
        'https://maps.google.com/?q=-27.59543,-48.54801',
      );
    });

    test('sem acento nem emoji: um so caractere fora do GSM-7 derruba o limite para 70', () {
      final t = textoSms(tipo: 'panic', quem: 'Dona Conceição 👵', quando: quando);
      expect(t, 'SysCare: BOTAO DE EMERGENCIA ACIONADO - Dona Conceicao - as 14:05');
      expect(t.codeUnits.every((u) => u < 0x80), isTrue);
    });

    test('simulacao sai marcada como TESTE', () {
      expect(textoSms(tipo: 'fall', quem: 'Ana', quando: quando, teste: true), startsWith('SysCare TESTE:'));
    });

    test('nome enorme: encurta o comeco e o link chega inteiro em 160 caracteres', () {
      final t = textoSms(tipo: 'no_movement', quem: 'Nome ' * 60, quando: quando, posicao: posicao);
      expect(t.length, lessThanOrEqualTo(limiteSms));
      expect(t, endsWith('https://maps.google.com/?q=-27.59543,-48.54801'));
      expect(t, contains('...'));
    });
  });

  group('destinatarios', () {
    Responsavel r(String nome, String? telefone, {int prioridade = 1, bool ativo = true}) =>
        Responsavel(id: nome, nome: nome, telefone: telefone, prioridade: prioridade, ativo: ativo);

    test('so ativos com telefone, por prioridade e sem numero repetido', () {
      final lista = destinatariosSms([
        r('Carla', '+55 48 99999-0003', prioridade: 3),
        r('Ana', '+5548999990001', prioridade: 1),
        r('Sem telefone', null),
        r('Inativo', '+5548999990009', ativo: false),
        r('Ana de novo', '+55 (48) 99999-0001', prioridade: 2),
        r('Bruno', '+5548999990002', prioridade: 2),
      ]);
      expect([for (final c in lista) c.nome], ['Ana', 'Bruno', 'Carla']);
    });
  });
}
