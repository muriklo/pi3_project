import '../dados/localizacao.dart';
import '../dominio/modelos.dart';

/// Regras do SMS enviado pelo celular (Camada 3). Mesmo formato do SMS da API
/// (app/notifications/sms.py): curto, sem acento e com link de mapa.

/// Um SMS cabe 160 caracteres no alfabeto GSM-7. Um unico "ã" ou "ç" joga a
/// mensagem para UCS-2 e o limite cai para 70: por isso tudo vira ASCII.
const limiteSms = 160;

const _semAcento = {
  'á': 'a', 'à': 'a', 'â': 'a', 'ã': 'a', 'ä': 'a',
  'é': 'e', 'è': 'e', 'ê': 'e', 'ë': 'e',
  'í': 'i', 'ì': 'i', 'î': 'i', 'ï': 'i',
  'ó': 'o', 'ò': 'o', 'ô': 'o', 'õ': 'o', 'ö': 'o',
  'ú': 'u', 'ù': 'u', 'û': 'u', 'ü': 'u',
  'ç': 'c', 'ñ': 'n',
  'Á': 'A', 'À': 'A', 'Â': 'A', 'Ã': 'A', 'Ä': 'A',
  'É': 'E', 'È': 'E', 'Ê': 'E', 'Ë': 'E',
  'Í': 'I', 'Ì': 'I', 'Î': 'I', 'Ï': 'I',
  'Ó': 'O', 'Ò': 'O', 'Ô': 'O', 'Õ': 'O', 'Ö': 'O',
  'Ú': 'U', 'Ù': 'U', 'Û': 'U', 'Ü': 'U',
  'Ç': 'C', 'Ñ': 'N',
};

String paraGsm7(String texto) {
  final ascii = StringBuffer();
  for (final c in texto.split('')) {
    final trocado = _semAcento[c] ?? c;
    // Mantem so ASCII imprimivel; o resto (emoji, simbolos) sai.
    if (trocado.codeUnits.every((u) => u >= 0x20 && u < 0x7f)) ascii.write(trocado);
  }
  return ascii.toString().split(RegExp(r'\s+')).where((p) => p.isNotEmpty).join(' ');
}

const _titulos = {
  'fall': 'QUEDA DETECTADA',
  'panic': 'BOTAO DE EMERGENCIA ACIONADO',
  'no_movement': 'IMOBILIDADE PROLONGADA',
};

String textoSms({
  required String tipo,
  required String quem,
  required DateTime quando,
  Posicao? posicao,
  bool teste = false,
}) {
  final hora = '${quando.hour.toString().padLeft(2, '0')}:${quando.minute.toString().padLeft(2, '0')}';
  final cabeca = paraGsm7([
    '${teste ? 'SysCare TESTE' : 'SysCare'}: ${_titulos[tipo] ?? 'EMERGENCIA'}',
    quem,
    'as $hora',
  ].join(' - '));
  final link = posicao == null
      ? ''
      : ' - https://maps.google.com/?q=${posicao.latitude.toStringAsFixed(5)},'
          '${posicao.longitude.toStringAsFixed(5)}';
  // Se passar de 160, encurta o comeco: o link do mapa precisa chegar inteiro.
  final espaco = limiteSms - link.length;
  final inicio = cabeca.length <= espaco ? cabeca : '${cabeca.substring(0, espaco - 3).trimRight()}...';
  return '$inicio$link';
}

/// Responsaveis ativos com telefone, por prioridade, sem numero repetido.
List<ContatoSms> destinatariosSms(List<Responsavel> responsaveis) {
  final ordenados = [...responsaveis]..sort((a, b) => a.prioridade.compareTo(b.prioridade));
  final vistos = <String>{};
  return [
    for (final r in ordenados)
      if (r.ativo && (r.telefone?.trim().isNotEmpty ?? false) && vistos.add(_soDigitos(r.telefone!)))
        ContatoSms(nome: r.nome, telefone: r.telefone!.trim()),
  ];
}

String _soDigitos(String telefone) => telefone.replaceAll(RegExp(r'[^0-9]'), '');
