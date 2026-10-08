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

/// Aceita o que a pessoa digitar ou colar no campo Servidor e devolve so
/// esquema, maquina e porta: "192.168.1.20" e
/// "http://192.168.1.20:8000/docs#/" viram "http://192.168.1.20:8000".
String normalizarServidor(String texto) {
  final limpo = texto.trim();
  if (limpo.isEmpty) return urlApi;
  final esquema = RegExp(r'^https?://').firstMatch(limpo)?.group(0) ?? 'http://';
  // Caminho, consulta e fragmento (/docs, ?x=1, #/) ficam de fora: as rotas da
  // API sao montadas a partir da raiz do servidor.
  final autoridade = limpo.substring(limpo.startsWith(esquema) ? esquema.length : 0).split(RegExp(r'[/?#]')).first;
  // Olha o texto, nao o Uri: o Uri esconde a porta quando ela e a padrao do
  // esquema (443 no https) e o codigo acharia que nao ha porta nenhuma.
  final comPorta = RegExp(r':\d+$').hasMatch(autoridade) ? autoridade : '$autoridade:8000';
  return '$esquema$comPorta';
}
