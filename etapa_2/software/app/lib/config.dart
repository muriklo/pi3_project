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
