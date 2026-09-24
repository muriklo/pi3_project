import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../aplicacao/providers.dart';
import '../../aplicacao/receptor.dart';
import '../../dominio/modelos.dart';
import '../comum.dart';
import '../tema.dart';

/// Tela de alerta (Figura 2). Dois modos:
///  - local: disparado pelo radio deste celular, antes de qualquer rede;
///  - servidor: aberto pelo historico ou por um push.
class AlertaTela extends ConsumerWidget {
  const AlertaTela.local({required String this.chave, super.key}) : id = null;

  const AlertaTela.servidor({required String this.id, super.key}) : chave = null;

  final String? chave;
  final String? id;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final local = chave == null ? null : ref.watch(receptorProvider.select((s) => s.alarmes[chave]));
    final alertaId = id ?? local?.alertaId;
    final remoto = alertaId == null ? null : ref.watch(alertaProvider(alertaId));
    final alerta = remoto?.value;

    // App reaberto pela notificacao depois de ter sido encerrado: o alarme
    // continua tocando, mas o estado dele se perdeu com o processo.
    if (chave != null && local == null) return _AlarmeOrfao(chave: chave!);

    final tipo = alerta?.tipo ?? local!.anuncio.evento.nomeApi;
    final emergenciaAtiva = ehEmergencia(tipo) &&
        !(local?.rebaixado ?? false) &&
        (alerta == null || alerta.aberto);
    final cores = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(
        backgroundColor: emergenciaAtiva ? corEmergencia : null,
        foregroundColor: emergenciaAtiva ? Colors.white : null,
        title: Text(rotulosEvento[tipo] ?? 'Alerta'),
      ),
      body: RefreshIndicator(
        onRefresh: () async {
          if (alertaId != null) ref.invalidate(alertaProvider(alertaId));
        },
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            _Cabecalho(tipo: tipo, local: local, alerta: alerta),
            const SizedBox(height: 16),
            if (local != null) _SituacaoLocal(local),
            if (remoto != null && remoto.hasError && alerta == null)
              ErroCarregar(erro: remoto.error!, tentarDeNovo: () => ref.invalidate(alertaProvider(alertaId!))),
            if (alerta != null) _SituacaoServidor(alerta),
            const SizedBox(height: 24),
            ..._acoes(context, ref, local, alerta, alertaId, cores),
          ],
        ),
      ),
    );
  }

  List<Widget> _acoes(
    BuildContext context,
    WidgetRef ref,
    AlarmeLocal? local,
    Alerta? alerta,
    String? alertaId,
    ColorScheme cores,
  ) {
    final receptor = ref.read(receptorProvider.notifier);
    final api = ref.read(apiProvider);
    void recarregar() {
      if (alertaId != null) ref.invalidate(alertaProvider(alertaId));
      ref.invalidate(alertasProvider);
    }

    return [
      if (local != null && !local.silenciado) ...[
        FilledButton.icon(
          style: FilledButton.styleFrom(backgroundColor: corEmergencia),
          onPressed: () => receptor.silenciar(local.chave),
          icon: const Icon(Icons.volume_off),
          label: const Text('Silenciar sirene'),
        ),
        const SizedBox(height: 12),
      ],
      if (alertaId == null && local != null && !local.rebaixado)
        const Padding(
          padding: EdgeInsets.only(bottom: 12),
          child: Text('"Estou indo" e "Encerrar" ficam disponíveis quando o servidor '
              'receber o alerta. A sirene não depende disso.'),
        ),
      if (alerta != null && alerta.aberto) ...[
        FilledButton.icon(
          onPressed: () => executar(context, () async {
            await api.confirmar(alerta.id);
            if (local != null) await receptor.silenciar(local.chave);
            recarregar();
          }, sucesso: 'Os outros responsáveis foram avisados de que você está indo.'),
          icon: const Icon(Icons.directions_run),
          label: const Text('Estou indo'),
        ),
        const SizedBox(height: 12),
      ],
      if (alerta != null && !alerta.encerrado) ...[
        OutlinedButton.icon(
          onPressed: () async {
            final falsoAlarme = await _perguntarDesfecho(context);
            if (falsoAlarme == null || !context.mounted) return;
            final ok = await executar(context, () => api.encerrar(alerta.id, falsoAlarme: falsoAlarme));
            if (!ok) return;
            if (local != null) await receptor.dispensar(local.chave);
            recarregar();
            if (context.mounted && local != null) Navigator.of(context).maybePop();
          },
          icon: const Icon(Icons.task_alt),
          label: const Text('Encerrar'),
        ),
        const SizedBox(height: 12),
      ],
      if (alerta?.latitude != null && alerta?.longitude != null) ...[
        OutlinedButton.icon(
          onPressed: () => launchUrl(
            Uri.parse('https://www.google.com/maps/search/?api=1'
                '&query=${alerta!.latitude},${alerta.longitude}'),
            mode: LaunchMode.externalApplication,
          ),
          icon: const Icon(Icons.map),
          label: const Text('Abrir mapa'),
        ),
        const SizedBox(height: 12),
      ],
      if (local != null && (local.rebaixado || (alerta?.encerrado ?? false)))
        OutlinedButton(
          onPressed: () async {
            await receptor.dispensar(local.chave);
            if (context.mounted) Navigator.of(context).maybePop();
          },
          child: const Text('Dispensar'),
        ),
    ];
  }

  /// true = falso alarme, false = atendido, null = cancelou.
  static Future<bool?> _perguntarDesfecho(BuildContext context) => showDialog<bool>(
        context: context,
        builder: (context) => AlertDialog(
          title: const Text('Como terminou?'),
          content: const Text('Isso ajuda a calibrar o detector de quedas.'),
          actions: [
            TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancelar')),
            TextButton(onPressed: () => Navigator.pop(context, true), child: const Text('Falso alarme')),
            FilledButton(
              style: FilledButton.styleFrom(minimumSize: const Size(0, 48)),
              onPressed: () => Navigator.pop(context, false),
              child: const Text('Atendido'),
            ),
          ],
        ),
      );
}

class _Cabecalho extends ConsumerWidget {
  const _Cabecalho({required this.tipo, this.local, this.alerta});

  final String tipo;
  final AlarmeLocal? local;
  final Alerta? alerta;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final pulseiras = ref.watch(pulseirasProvider).value ?? const <Pulseira>[];
    Pulseira? pulseira;
    for (final p in pulseiras) {
      if (p.id == alerta?.deviceId || p.bleId == local?.anuncio.bleId) pulseira = p;
    }
    final quem = pulseira?.titulo ?? (local != null ? 'Pulseira ${local!.anuncio.bleId}' : 'Pulseira');
    final quando = alerta?.ocorridoEm ?? local!.recebidoEm;
    final impacto = alerta?.impactoG ?? local?.anuncio.impactoG;
    final bateria = alerta?.bateriaPct ?? local?.anuncio.bateriaPct;
    final texto = Theme.of(context).textTheme;
    return Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
      Icon(iconeDoEvento(tipo), size: 56, color: ehEmergencia(tipo) ? corEmergencia : null),
      const SizedBox(width: 16),
      Expanded(
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text(quem, style: texto.headlineSmall),
          Text('Às ${dataHora(quando)}', style: texto.titleMedium),
          if (impacto != null && impacto > 0) Text('Impacto de ${impacto.toStringAsFixed(1)} g'),
          if (bateria != null) Text('Bateria da pulseira: $bateria%'),
        ]),
      ),
    ]);
  }
}

class _SituacaoLocal extends StatelessWidget {
  const _SituacaoLocal(this.local);

  final AlarmeLocal local;

  @override
  Widget build(BuildContext context) {
    final (icone, texto) = switch (local.situacao) {
      SituacaoEnvio.naFila => (Icons.upload, 'Avisando os responsáveis...'),
      SituacaoEnvio.enviado => (
          Icons.check_circle,
          'Responsáveis avisados: ${local.notificados ?? 0}.',
        ),
      SituacaoEnvio.jaReportado => (Icons.check_circle, 'Outro celular já avisou os responsáveis.'),
      SituacaoEnvio.rejeitado => (
          Icons.gpp_bad,
          'O servidor rejeitou este anúncio (assinatura inválida ou repetição de um '
              'anúncio antigo). A sirene foi desligada.',
        ),
      SituacaoEnvio.descartado => (
          Icons.info,
          'O servidor não aceitou o evento: ${local.detalhe ?? 'motivo não informado'}.',
        ),
      SituacaoEnvio.semConexao => (
          Icons.cloud_off,
          'Sem conexão. O evento está guardado e vai assim que a conexão voltar. '
              'Se precisar, ligue para os responsáveis.',
        ),
      SituacaoEnvio.sessaoExpirada => (
          Icons.lock_clock,
          'Sessão vencida. Entre de novo para avisar os responsáveis.',
        ),
    };
    return Card(child: ListTile(leading: Icon(icone), title: Text(texto)));
  }
}

class _SituacaoServidor extends ConsumerWidget {
  const _SituacaoServidor(this.alerta);

  final Alerta alerta;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final eu = ref.watch(sessaoProvider)?.usuario.id;
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          SeloStatus(alerta.status),
          if (alerta.confirmadoEm != null)
            Text(alerta.confirmadoPor == eu
                ? 'Você confirmou às ${dataHora(alerta.confirmadoEm!)}.'
                : 'Um responsável confirmou às ${dataHora(alerta.confirmadoEm!)}.'),
          if (alerta.aberto && alerta.rodada > 0)
            Text('Ninguém confirmou ainda: reenvio nº ${alerta.rodada}.'),
          if (alerta.gateway != null) Text('Ouvido por: ${alerta.gateway}'),
          if (alerta.entregas.isNotEmpty) ...[
            const SizedBox(height: 8),
            Text('Notificações', style: Theme.of(context).textTheme.titleSmall),
            for (final e in alerta.entregas)
              Text('${e.canal} · ${e.status}${e.tentativa > 1 ? ' · tentativa ${e.tentativa}' : ''}'),
          ],
        ]),
      ),
    );
  }
}

class _AlarmeOrfao extends ConsumerWidget {
  const _AlarmeOrfao({required this.chave});

  final String chave;

  @override
  Widget build(BuildContext context, WidgetRef ref) => Scaffold(
        appBar: AppBar(title: const Text('Alarme')),
        body: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(children: [
            const Text('Este alarme foi disparado antes de o app ser reaberto. '
                'O evento continua na fila de envio; veja a situação no Histórico.'),
            const SizedBox(height: 24),
            FilledButton.icon(
              style: FilledButton.styleFrom(backgroundColor: corEmergencia),
              onPressed: () async {
                await ref.read(alarmeProvider).silenciar(idNotificacao(chave));
                if (context.mounted) Navigator.of(context).maybePop();
              },
              icon: const Icon(Icons.volume_off),
              label: const Text('Silenciar sirene'),
            ),
          ]),
        ),
      );
}
