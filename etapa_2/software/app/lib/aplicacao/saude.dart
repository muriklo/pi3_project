import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:universal_ble/universal_ble.dart' show AvailabilityState;

import '../config.dart';
import 'providers.dart';
import 'receptor.dart';

/// Uma pre-condicao do receptor. Em emergencia, a falha silenciosa e o pior
/// modo de falha: cada item vira algo que o usuario consegue conferir.
class ItemSaude {
  const ItemSaude(this.titulo, this.ok, this.detalhe, {this.rotuloAcao, this.acao});

  final String titulo;

  /// null quando o sistema nao deixa verificar, so pedir.
  final bool? ok;
  final String detalhe;
  final String? rotuloAcao;
  final Future<void> Function()? acao;
}

String tempoDesde(DateTime quando, [DateTime? agora]) {
  final d = (agora ?? DateTime.now()).difference(quando);
  if (d.inSeconds < 60) return 'agora há pouco';
  if (d.inMinutes < 60) return 'há ${d.inMinutes} min';
  if (d.inHours < 24) return 'há ${d.inHours} h';
  return 'há ${d.inDays} dia${d.inDays > 1 ? 's' : ''}';
}

final saudeProvider = FutureProvider.autoDispose<List<ItemSaude>>((ref) async {
  // Refaz a lista quando a escuta muda de estado, nao a cada anuncio.
  final varredura = ref.watch(receptorProvider.select((s) => s.varredura));
  final receptor = ref.read(receptorProvider.notifier);
  final ble = ref.read(varreduraProvider);
  final alarme = ref.read(alarmeProvider);
  final sessao = ref.read(sessaoProvider);
  void atualizar() => ref.invalidateSelf();

  final bluetooth = await ble.estado().catchError((_) => AvailabilityState.unknown);
  final permissoes = await ble.temPermissoes().catchError((_) => false);
  final gps = await ref.read(localizacaoProvider).disponivel();
  final notificacoes = await alarme.notificacoesPermitidas();
  final servidor = await ref.read(apiProvider).servidorNoAr();
  final fila = await ref.read(filaProvider).total();
  final vence = sessao?.venceEm;
  final escutando = varredura == EstadoVarredura.escutando;

  final itens = <ItemSaude>[
    ItemSaude(
      'Escuta neste celular',
      escutando,
      switch (varredura) {
        EstadoVarredura.escutando => 'Ouvindo as pulseiras.',
        EstadoVarredura.desligado => 'Desligada: este celular não vai alarmar.',
        _ => ref.read(receptorProvider).motivo ?? 'Verificando...',
      },
      rotuloAcao: escutando ? null : 'Ligar',
      acao: escutando ? null : receptor.ligar,
    ),
    ItemSaude(
      'Bluetooth',
      bluetooth == AvailabilityState.poweredOn,
      bluetooth == AvailabilityState.poweredOn ? 'Ligado.' : 'Desligado ou indisponível.',
    ),
    ItemSaude(
      'Permissão de Bluetooth e localização',
      permissoes,
      permissoes ? 'Concedida.' : 'Sem ela o celular não ouve a pulseira.',
      rotuloAcao: permissoes ? null : 'Permitir',
      acao: permissoes
          ? null
          : () async {
              try {
                await ble.pedirPermissoes();
              } catch (_) {}
              atualizar();
            },
    ),
    ItemSaude(
      'Localização (GPS)',
      gps,
      gps ? 'Disponível.' : 'Desligada: os alertas vão sem a posição.',
    ),
    ItemSaude(
      'Notificações',
      notificacoes,
      notificacoes ? 'Permitidas.' : 'Bloqueadas: o alarme não toca.',
      rotuloAcao: notificacoes ? null : 'Permitir',
      acao: notificacoes
          ? null
          : () async {
              await alarme.pedirNotificacoes();
              atualizar();
            },
    ),
    ItemSaude(
      'Alarme em tela cheia',
      null,
      'No Android 14 ou superior precisa estar permitido para o alerta abrir '
          'com o celular bloqueado.',
      rotuloAcao: 'Conferir',
      acao: () async {
        await alarme.pedirTelaCheia();
      },
    ),
    ItemSaude(
      'Servidor',
      servidor,
      servidor ? 'No ar em $urlApi.' : 'Sem resposta de $urlApi. O alarme local continua.',
    ),
    ItemSaude(
      'Sessão',
      sessao != null && (vence == null || vence.difference(DateTime.now()).inDays >= 1),
      sessao == null
          ? 'Vencida: entre de novo para repassar os alertas.'
          : vence == null
              ? 'Ativa.'
              : 'Vence em ${vence.difference(DateTime.now()).inDays} dia(s). '
                  'A API ainda não renova a sessão sozinha.',
    ),
    ItemSaude(
      'Fila de envio',
      fila == 0,
      fila == 0 ? 'Nada pendente.' : '$fila evento(s) aguardando envio à API.',
    ),
  ];

  final vistas = ref.read(receptorProvider).vistas;
  for (final p in ref.read(pulseirasProvider).value ?? const []) {
    final visto = vistas[p.bleId];
    final recente = visto != null && DateTime.now().difference(visto.em).inMinutes < 5;
    itens.add(ItemSaude(
      'Pulseira: ${p.titulo}',
      recente,
      visto == null
          ? 'Não ouvida desde que a escuta foi ligada.'
          : 'Ouvida ${tempoDesde(visto.em)} · bateria ${visto.anuncio.bateriaPct}%',
    ));
  }
  return itens;
});
