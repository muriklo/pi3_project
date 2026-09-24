import 'dart:async';
import 'dart:math';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:syscare_protocol/syscare_protocol.dart';
import 'package:universal_ble/universal_ble.dart' show AvailabilityState;

import '../dados/fila_envio.dart';
import '../dados/varredura_ble.dart';
import '../dominio/modelos.dart';
import 'envio.dart';
import 'providers.dart';

/// Estados da Figura 3 da arquitetura.
enum EstadoVarredura { desligado, verificando, escutando, suspenso, aguardandoReinicio }

enum SituacaoEnvio { naFila, enviado, jaReportado, rejeitado, descartado, semConexao, sessaoExpirada }

/// Um alarme disparado por este celular a partir do radio.
class AlarmeLocal {
  const AlarmeLocal({
    required this.chave,
    required this.anuncio,
    required this.recebidoEm,
    this.alertaId,
    this.situacao = SituacaoEnvio.naFila,
    this.notificados,
    this.silenciado = false,
    this.detalhe,
  });

  final String chave;
  final Anuncio anuncio;
  final DateTime recebidoEm;

  /// Id do alerta na API, conhecido depois do primeiro envio aceito.
  final String? alertaId;
  final SituacaoEnvio situacao;
  final int? notificados;
  final bool silenciado;
  final String? detalhe;

  /// A API respondeu que o anuncio nao e legitimo (Secao 4.7).
  bool get rebaixado => situacao == SituacaoEnvio.rejeitado;

  AlarmeLocal copiar({
    String? alertaId,
    SituacaoEnvio? situacao,
    int? notificados,
    bool? silenciado,
    String? detalhe,
  }) =>
      AlarmeLocal(
        chave: chave,
        anuncio: anuncio,
        recebidoEm: recebidoEm,
        alertaId: alertaId ?? this.alertaId,
        situacao: situacao ?? this.situacao,
        notificados: notificados ?? this.notificados,
        silenciado: silenciado ?? this.silenciado,
        detalhe: detalhe ?? this.detalhe,
      );
}

/// Ultimo anuncio ouvido de uma pulseira, de qualquer tipo.
class Visto {
  const Visto(this.anuncio, this.em, this.rssi);

  final Anuncio anuncio;
  final DateTime em;
  final int? rssi;
}

class EstadoReceptor {
  const EstadoReceptor({
    this.varredura = EstadoVarredura.desligado,
    this.motivo,
    this.vistas = const {},
    this.alarmes = const {},
  });

  final EstadoVarredura varredura;

  /// Por que esta suspenso ou aguardando reinicio.
  final String? motivo;
  final Map<String, Visto> vistas;
  final Map<String, AlarmeLocal> alarmes;

  EstadoReceptor comVarredura(EstadoVarredura v, [String? motivo]) =>
      EstadoReceptor(varredura: v, motivo: motivo, vistas: vistas, alarmes: alarmes);

  EstadoReceptor copiar({Map<String, Visto>? vistas, Map<String, AlarmeLocal>? alarmes}) =>
      EstadoReceptor(
        varredura: varredura,
        motivo: motivo,
        vistas: vistas ?? this.vistas,
        alarmes: alarmes ?? this.alarmes,
      );
}

/// Id de notificacao estavel entre execucoes do app, para silenciar depois.
int idNotificacao(String chave) {
  final [bleId, seq] = chave.split('-');
  return (int.parse(bleId, radix: 16) ^ (int.parse(seq) << 7)) & 0x7fffffff;
}

/// O celular receptor: escuta a pulseira, alarma e repassa a API.
class Receptor extends Notifier<EstadoReceptor> {
  late Deduplicador _dedup;
  Set<String> _daConta = {};
  StreamSubscription<Recepcao>? _recepcoes;
  StreamSubscription<AvailabilityState>? _bluetooth;
  Timer? _reinicio;
  int _falhas = 0;
  final Map<String, DateTime> _ultimaAtualizacao = {};

  @override
  EstadoReceptor build() {
    final prefs = ref.read(preferenciasProvider);
    _dedup = Deduplicador(prefs.dedup);
    _daConta = prefs.pulseirasDaConta;
    ref.listen(pulseirasProvider, (_, proximo) {
      final lista = proximo.value;
      if (lista != null) _daConta = {for (final p in lista) p.bleId};
    });
    ref.onDispose(_liberar);
    return const EstadoReceptor();
  }

  VarreduraBle get _ble => ref.read(varreduraProvider);

  // ------------------------------------------------------- maquina ----
  Future<void> ligar() async {
    await ref.read(preferenciasProvider).salvarEscutar(true);
    // No Android 13+ as notificacoes comecam bloqueadas: sem elas, o alarme
    // local nao toca. Pedido aqui, no primeiro uso da escuta (Secao 4.4).
    await ref.read(alarmeProvider).pedirNotificacoes();
    await _verificar();
  }

  Future<void> desligar() async {
    await ref.read(preferenciasProvider).salvarEscutar(false);
    await _liberar();
    try {
      await _ble.parar();
    } catch (_) {}
    state = state.comVarredura(EstadoVarredura.desligado);
  }

  Future<void> _verificar() async {
    _reinicio?.cancel();
    state = state.comVarredura(EstadoVarredura.verificando);
    try {
      if (!await _ble.temPermissoes()) await _ble.pedirPermissoes();
    } catch (_) {
      state = state.comVarredura(
          EstadoVarredura.suspenso, 'Permissão de Bluetooth ou de localização negada.');
      return;
    }
    _bluetooth ??= _ble.disponibilidade.listen(_aoMudarBluetooth);
    final bluetooth = await _ble.estado();
    if (bluetooth != AvailabilityState.poweredOn) {
      state = state.comVarredura(EstadoVarredura.suspenso, _motivoBluetooth(bluetooth));
      return;
    }
    _recepcoes ??= _ble.recepcoes.listen(_aoReceber);
    try {
      await _ble.iniciar();
      _falhas = 0;
      state = state.comVarredura(EstadoVarredura.escutando);
    } catch (e) {
      _agendarReinicio('A varredura falhou ao iniciar.');
    }
  }

  /// Reinicia so em mudanca de estado e com espera crescente (4, 8, 16... s):
  /// o Android 7+ ignora mais de 5 inicios de varredura em 30 s.
  void _agendarReinicio(String motivo) {
    _falhas++;
    final espera = Duration(seconds: min(60, 2 << _falhas));
    state = state.comVarredura(EstadoVarredura.aguardandoReinicio, motivo);
    _reinicio = Timer(espera, _verificar);
  }

  void _aoMudarBluetooth(AvailabilityState bluetooth) {
    final atual = state.varredura;
    if (atual == EstadoVarredura.desligado) return;
    if (bluetooth == AvailabilityState.poweredOn) {
      if (atual == EstadoVarredura.suspenso) unawaited(_verificar());
    } else if (atual != EstadoVarredura.suspenso) {
      state = state.comVarredura(EstadoVarredura.suspenso, _motivoBluetooth(bluetooth));
    }
  }

  static String _motivoBluetooth(AvailabilityState s) => switch (s) {
        AvailabilityState.poweredOff => 'O Bluetooth está desligado.',
        AvailabilityState.unauthorized => 'O app não tem permissão para usar o Bluetooth.',
        AvailabilityState.unsupported => 'Este celular não tem Bluetooth Low Energy.',
        _ => 'O Bluetooth não está disponível.',
      };

  Future<void> _liberar() async {
    _reinicio?.cancel();
    await _recepcoes?.cancel();
    _recepcoes = null;
    await _bluetooth?.cancel();
    _bluetooth = null;
  }

  // ------------------------------------------------ anuncio recebido ----
  void _aoReceber(Recepcao r) {
    final Anuncio anuncio;
    try {
      anuncio = Anuncio.decodificar(r.bytes);
    } on AnuncioInvalido {
      return; // outro dispositivo usando o Company ID de testes
    }
    _registrarVisto(anuncio, r);

    switch (decidir(anuncio, _dedup, _daConta)) {
      case Acao.ignorar:
        return;
      case Acao.telemetria:
        ref.read(agregadorProvider).registrar(anuncio, em: r.em, rssi: r.rssi);
        return;
      case Acao.alarmarEEnviar:
        // PRIMEIRO o alarme: ele nao depende de rede nem de GPS (Figura 4).
        _alarmar(anuncio, r);
      case Acao.notificarEEnviar:
        _avisar(anuncio);
      case Acao.apenasEnviar:
        break;
    }
    unawaited(ref.read(preferenciasProvider).salvarDedup(_dedup.estado));
    unawaited(_enfileirar(anuncio, r));
  }

  void _registrarVisto(Anuncio a, Recepcao r) {
    // O anuncio de emergencia chega a cada 100 ms: atualiza a tela 1x/s.
    final ultimo = _ultimaAtualizacao[a.bleId];
    if (ultimo != null && r.em.difference(ultimo) < const Duration(seconds: 1)) return;
    _ultimaAtualizacao[a.bleId] = r.em;
    state = state.copiar(vistas: {...state.vistas, a.bleId: Visto(a, r.em, r.rssi)});
  }

  String _quem(String bleId) {
    final lista = ref.read(pulseirasProvider).value ?? const <Pulseira>[];
    for (final p in lista) {
      if (p.bleId == bleId) return p.titulo;
    }
    return 'Pulseira $bleId';
  }

  void _alarmar(Anuncio a, Recepcao r) {
    final chave = chaveDoEvento(a);
    final impacto = a.impactoDg > 0 ? ' · impacto de ${a.impactoG.toStringAsFixed(1)} g' : '';
    unawaited(ref.read(alarmeProvider).emergencia(
          id: idNotificacao(chave),
          titulo: (rotulosEvento[a.evento.nomeApi] ?? 'Emergência').toUpperCase(),
          texto: '${_quem(a.bleId)}$impacto',
          payload: 'local:$chave',
        ));
    state = state.copiar(alarmes: {
      ...state.alarmes,
      chave: AlarmeLocal(chave: chave, anuncio: a, recebidoEm: r.em),
    });
  }

  void _avisar(Anuncio a) {
    unawaited(ref.read(alarmeProvider).aviso(
          id: idNotificacao(chaveDoEvento(a)),
          titulo: rotulosEvento[a.evento.nomeApi] ?? 'Aviso da pulseira',
          texto: '${_quem(a.bleId)} · bateria ${a.bateriaPct}%',
        ));
  }

  /// Grava na fila antes de enviar. Localizacao em dois tempos (Secao 5.4).
  Future<void> _enfileirar(Anuncio a, Recepcao r) async {
    final fila = ref.read(filaProvider);
    final localizacao = ref.read(localizacaoProvider);
    final rotinas = ref.read(rotinasProvider);
    final nome = ref.read(sessaoProvider)?.usuario.nome;
    final rotulo = nome == null ? 'Celular receptor' : 'Celular de $nome';

    final recente = await localizacao.recente();
    await fila.adicionar(ItemFila(
      anuncio: a,
      recebidoEm: r.em,
      rssi: r.rssi,
      posicao: recente,
      rotulo: rotulo,
      proximaEm: r.em,
    ));
    unawaited(rotinas.drenarFila());
    if (recente != null) return;

    // Sem posicao recente: o alerta ja foi sem ela; a posicao vai num segundo
    // envio que a API funde ao primeiro (duplicado dentro de 180 s).
    final atual = await localizacao.atual();
    if (atual == null) return;
    await fila.adicionar(ItemFila(
      anuncio: a,
      recebidoEm: r.em,
      rssi: r.rssi,
      posicao: atual,
      rotulo: rotulo,
      proximaEm: DateTime.now(),
    ));
    unawaited(rotinas.drenarFila());
  }

  // -------------------------------------------------- volta da API ----
  void aoResultadoDoEnvio(ItemFila item, ResultadoItem r) {
    final atual = state.alarmes[item.chave];
    if (atual == null) return; // evento sem alarme: bateria, teste, pulseira alheia
    final novo = switch (r.desfecho) {
      Desfecho.enviado => atual.copiar(
          situacao: SituacaoEnvio.enviado,
          alertaId: r.envio!.alerta.id,
          notificados: r.envio!.notificados,
        ),
      // O segundo envio (com a posicao) volta como duplicado do proprio primeiro.
      Desfecho.duplicado => atual.copiar(
          situacao: atual.situacao == SituacaoEnvio.enviado
              ? SituacaoEnvio.enviado
              : SituacaoEnvio.jaReportado,
          alertaId: r.envio!.alerta.id,
        ),
      Desfecho.rejeitadoAssinatura ||
      Desfecho.retransmissao =>
        atual.copiar(situacao: SituacaoEnvio.rejeitado, detalhe: r.detalhe, silenciado: true),
      Desfecho.descartado => atual.copiar(situacao: SituacaoEnvio.descartado, detalhe: r.detalhe),
      Desfecho.sessaoExpirada => atual.copiar(situacao: SituacaoEnvio.sessaoExpirada),
      Desfecho.semConexao => atual.copiar(situacao: SituacaoEnvio.semConexao),
    };
    if (r.rebaixaAlarme) unawaited(ref.read(alarmeProvider).silenciar(idNotificacao(item.chave)));
    state = state.copiar(alarmes: {...state.alarmes, item.chave: novo});
  }

  // ------------------------------------------------- acoes da tela ----
  Future<void> silenciar(String chave) async {
    await ref.read(alarmeProvider).silenciar(idNotificacao(chave));
    final atual = state.alarmes[chave];
    if (atual != null) {
      state = state.copiar(alarmes: {...state.alarmes, chave: atual.copiar(silenciado: true)});
    }
  }

  Future<void> dispensar(String chave) async {
    await ref.read(alarmeProvider).silenciar(idNotificacao(chave));
    state = state.copiar(alarmes: {...state.alarmes}..remove(chave));
  }
}
