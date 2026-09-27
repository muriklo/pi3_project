/// Endereco da API do SysCare.
///
/// Troque na execucao, apontando para o computador que roda a API:
///   flutter run --dart-define=SYSCARE_API=http://192.168.0.10:8000
///
/// O padrao 10.0.2.2 e o proprio computador visto de dentro do emulador Android.
const String urlApi = String.fromEnvironment(
  'SYSCARE_API',
  defaultValue: 'http://10.0.2.2:8000',
);

/// Aceita o que a pessoa digitar no campo Servidor: "192.168.1.20" vira
/// "http://192.168.1.20:8000" (a porta padrao da API).
String normalizarServidor(String texto) {
  var url = texto.trim();
  if (url.isEmpty) return urlApi;
  if (!url.startsWith(RegExp(r'https?://'))) url = 'http://$url';
  while (url.endsWith('/')) {
    url = url.substring(0, url.length - 1);
  }
  // Olha o texto, nao o Uri: o Uri esconde a porta quando ela e a padrao do
  // esquema (443 no https) e o codigo acharia que nao ha porta nenhuma.
  final autoridade = url.replaceFirst(RegExp(r'^https?://'), '').split('/').first;
  return RegExp(r':\d+$').hasMatch(autoridade) ? url : '$url:8000';
}
