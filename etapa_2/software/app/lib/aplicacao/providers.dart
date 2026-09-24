import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../config.dart';
import '../dados/alarme.dart';
import '../dados/api.dart';
import '../dados/armazenamento.dart';
import '../dados/fila_envio.dart';
import '../dados/localizacao.dart';
import '../dados/sms_celular.dart';
import '../dados/varredura_ble.dart';
import '../dominio/modelos.dart';
import 'envio.dart';
import 'receptor.dart';
import 'telemetria.dart';

// ---------------------------------------------------------------------------
// Sobrescritos em main.dart, depois de abrir o armazenamento.
// ---------------------------------------------------------------------------
final sessaoInicialProvider = Provider<SessaoSalva?>((ref) => null);
final filaProvider = Provider<FilaEnvio>((ref) => throw UnimplementedError('main.dart'));
final preferenciasProvider =
    Provider<PreferenciasReceptor>((ref) => throw UnimplementedError('main.dart'));
final alarmeProvider = Provider<Alarme>((ref) => throw UnimplementedError('main.dart'));

// ---------------------------------------------------------------------------
// Adaptadores da camada de dados.
// ---------------------------------------------------------------------------
final cofreProvider = Provider((ref) => const CofreSessao());
final varreduraProvider = Provider((ref) => VarreduraBle());
final localizacaoProvider = Provider((ref) => Localizacao());
final smsCelularProvider = Provider((ref) => SmsCelular());
final agregadorProvider = Provider((ref) => AgregadorTelemetria());

final apiProvider = Provider<ApiSysCare>(
  (ref) => ApiSysCare(
    urlBase: urlApi,
    token: () async => ref.read(sessaoProvider)?.token,
    aoExpirarSessao: () => ref.read(sessaoProvider.notifier).expirou(),
  ),
);

// ---------------------------------------------------------------------------
// Sessao.
// ---------------------------------------------------------------------------
final sessaoProvider = NotifierProvider<SessaoControle, SessaoSalva?>(SessaoControle.new);

class SessaoControle extends Notifier<SessaoSalva?> {
  @override
  SessaoSalva? build() => ref.read(sessaoInicialProvider);

  Future<void> entrar(String email, String senha) async =>
      _guardar(await ref.read(apiProvider).entrar(email, senha));

  Future<void> criarConta(String nome, String email, String senha) async =>
      _guardar(await ref.read(apiProvider).criarConta(nome, email, senha));

  Future<void> _guardar(({String token, Usuario usuario}) r) async {
    final sessao = SessaoSalva(token: r.token, usuario: r.usuario);
    await ref.read(cofreProvider).salvar(sessao);
    state = sessao;
    unawaited(ref.read(rotinasProvider).drenarFila());
  }

  /// Saida pedida pelo usuario: para a escuta e esquece as pulseiras.
  Future<void> sair() async {
    await ref.read(receptorProvider.notifier).desligar();
    await ref.read(preferenciasProvider).limpar();
    await ref.read(cofreProvider).apagar();
    state = null;
  }

  /// Sessao vencida (a API nao renova: pendencia P2). A escuta e o alarme
  /// local CONTINUAM: eles nao dependem da rede. So o repasse espera o login.
  void expirou() {
    if (state == null) return;
    unawaited(ref.read(cofreProvider).apagar());
    state = null;
  }
}

// ---------------------------------------------------------------------------
// Dados da API.
// ---------------------------------------------------------------------------
final pulseirasProvider = FutureProvider<List<Pulseira>>((ref) async {
  if (ref.watch(sessaoProvider) == null) return const [];
  final lista = await ref.read(apiProvider).pulseiras();
  // Guardadas para o receptor alarmar mesmo sem conexao.
  await ref.read(preferenciasProvider).salvarPulseirasDaConta({for (final p in lista) p.bleId});
  return lista;
});

final alertasProvider = FutureProvider.family<List<Alerta>, String?>((ref, pulseiraId) {
  ref.watch(sessaoProvider);
  return ref.read(apiProvider).alertas(pulseiraId: pulseiraId);
});

final alertaProvider = FutureProvider.family<Alerta, String>(
  (ref, id) => ref.read(apiProvider).alerta(id),
);

final responsaveisProvider = FutureProvider.family<List<Responsavel>, String>(
  (ref, pulseiraId) => ref.read(apiProvider).responsaveis(pulseiraId),
);

final tamanhoFilaProvider = FutureProvider<int>((ref) => ref.read(filaProvider).total());

// ---------------------------------------------------------------------------
// Receptor, envio e rotinas.
// ---------------------------------------------------------------------------
final receptorProvider = NotifierProvider<Receptor, EstadoReceptor>(Receptor.new);

final envioProvider = Provider<EnvioAlertas>(
  (ref) => EnvioAlertas(
    fila: ref.read(filaProvider),
    enviar: (item) => ref.read(apiProvider).enviarAlerta(item),
    aoResultado: (item, r) => ref.read(receptorProvider.notifier).aoResultadoDoEnvio(item, r),
  ),
);

final rotinasProvider = Provider<Rotinas>((ref) {
  final rotinas = Rotinas(ref);
  ref.onDispose(rotinas.parar);
  return rotinas;
});

/// Tarefas periodicas: esvaziar a fila e enviar a telemetria agregada.
class Rotinas {
  Rotinas(this._ref);

  final Ref _ref;
  Timer? _fila;
  Timer? _telemetria;

  void iniciar() {
    _fila ??= Timer.periodic(const Duration(seconds: 30), (_) => drenarFila());
    _telemetria ??= Timer.periodic(const Duration(minutes: 5), (_) => enviarTelemetria());
    unawaited(drenarFila());
  }

  void parar() {
    _fila?.cancel();
    _telemetria?.cancel();
  }

  Future<void> drenarFila() async {
    if (_ref.read(sessaoProvider) == null) return;
    await _ref.read(envioProvider).drenar();
    _ref.invalidate(tamanhoFilaProvider);
  }

  Future<void> enviarTelemetria() async {
    if (_ref.read(sessaoProvider) == null) return;
    final agregador = _ref.read(agregadorProvider);
    for (final MapEntry(key: bleId, value: amostras) in agregador.retirarLotes().entries) {
      try {
        await _ref.read(apiProvider).enviarTelemetria(bleId, amostras);
      } on ErroApi catch (e) {
        // Pulseira alheia (404) e descartada; falha passageira volta para a fila.
        if (e.semConexao || e.sessaoInvalida || (e.status ?? 0) >= 500) {
          agregador.devolver(bleId, amostras);
        }
      }
    }
  }
}
