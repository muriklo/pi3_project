import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../aplicacao/providers.dart';
import '../../aplicacao/receptor.dart';
import '../../aplicacao/saude.dart';
import '../../dominio/modelos.dart';
import '../comum.dart';
import '../tema.dart';

class InicioTela extends ConsumerWidget {
  const InicioTela({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final alarmes = ref.watch(receptorProvider.select((s) => s.alarmes));
    final pulseiras = ref.watch(pulseirasProvider);
    return Scaffold(
      appBar: AppBar(
        title: const Text('SysCare'),
        actions: [
          IconButton(
            tooltip: 'Histórico',
            icon: const Icon(Icons.history),
            onPressed: () => context.push('/historico'),
          ),
          IconButton(
            tooltip: 'Saúde do sistema',
            icon: const Icon(Icons.monitor_heart),
            onPressed: () => context.push('/saude'),
          ),
          IconButton(
            tooltip: 'Conta',
            icon: const Icon(Icons.account_circle),
            onPressed: () => context.push('/conta'),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => context.push('/nova-pulseira'),
        icon: const Icon(Icons.add),
        label: const Text('Nova pulseira'),
      ),
      body: RefreshIndicator(
        onRefresh: () => ref.refresh(pulseirasProvider.future),
        child: ListView(
          padding: const EdgeInsets.fromLTRB(16, 8, 16, 96),
          children: [
            for (final alarme in alarmes.values) _CartaoAlarme(alarme),
            const _CartaoEscuta(),
            const SizedBox(height: 16),
            Text('Pulseiras', style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(height: 8),
            pulseiras.when(
              data: (lista) => lista.isEmpty
                  ? const Padding(
                      padding: EdgeInsets.all(16),
                      child: Text('Nenhuma pulseira ainda. Toque em "Nova pulseira" '
                          'ou peça ao dono de uma pulseira para incluir você como responsável.'),
                    )
                  : Column(children: [for (final p in lista) _CartaoPulseira(p)]),
              loading: () => const Carregando(),
              error: (e, _) => ErroCarregar(erro: e, tentarDeNovo: () => ref.invalidate(pulseirasProvider)),
            ),
          ],
        ),
      ),
    );
  }
}

class _CartaoAlarme extends StatelessWidget {
  const _CartaoAlarme(this.alarme);

  final AlarmeLocal alarme;

  @override
  Widget build(BuildContext context) => Card(
        color: alarme.rebaixado ? null : corEmergencia,
        child: ListTile(
          leading: Icon(iconeDoEvento(alarme.anuncio.evento.nomeApi),
              color: alarme.rebaixado ? null : Colors.white, size: 36),
          title: Text(
            rotulosEvento[alarme.anuncio.evento.nomeApi] ?? 'Emergência',
            style: TextStyle(color: alarme.rebaixado ? null : Colors.white, fontWeight: FontWeight.bold),
          ),
          subtitle: Text(
            alarme.rebaixado ? 'Rejeitado pelo servidor' : 'Recebido às ${dataHora(alarme.recebidoEm)}',
            style: TextStyle(color: alarme.rebaixado ? null : Colors.white),
          ),
          trailing: Icon(Icons.chevron_right, color: alarme.rebaixado ? null : Colors.white),
          onTap: () => context.push('/alarme/${alarme.chave}'),
        ),
      );
}

class _CartaoEscuta extends ConsumerWidget {
  const _CartaoEscuta();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final estado = ref.watch(receptorProvider.select((s) => (s.varredura, s.motivo)));
    final (varredura, motivo) = estado;
    final ligado = varredura != EstadoVarredura.desligado;
    final (icone, texto) = switch (varredura) {
      EstadoVarredura.escutando => (Icons.bluetooth_searching, 'Ouvindo as pulseiras'),
      EstadoVarredura.verificando => (Icons.hourglass_top, 'Verificando Bluetooth e permissões'),
      EstadoVarredura.suspenso => (Icons.warning_amber, motivo ?? 'Escuta suspensa'),
      EstadoVarredura.aguardandoReinicio => (Icons.restart_alt, motivo ?? 'Tentando de novo'),
      EstadoVarredura.desligado => (Icons.bluetooth_disabled, 'Este celular não está alarmando'),
    };
    final problema = varredura == EstadoVarredura.suspenso || varredura == EstadoVarredura.aguardandoReinicio;
    return Card(
      child: Column(children: [
        SwitchListTile(
          secondary: Icon(icone, color: problema ? Theme.of(context).colorScheme.error : null),
          title: const Text('Escutar neste celular'),
          subtitle: Text(texto),
          value: ligado,
          onChanged: (valor) {
            final receptor = ref.read(receptorProvider.notifier);
            valor ? receptor.ligar() : receptor.desligar();
          },
        ),
        if (problema)
          ListTile(
            leading: const Icon(Icons.monitor_heart),
            title: const Text('Ver o que falta'),
            onTap: () => context.push('/saude'),
          ),
      ]),
    );
  }
}

class _CartaoPulseira extends ConsumerWidget {
  const _CartaoPulseira(this.pulseira);

  final Pulseira pulseira;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final visto = ref.watch(receptorProvider.select((s) => s.vistas[pulseira.bleId]));
    final usuarioId = ref.watch(sessaoProvider)?.usuario.id;
    // O que este celular ouviu agora vale mais que o ultimo dado do servidor.
    final bateria = visto?.anuncio.bateriaPct ?? pulseira.bateriaPct;
    final vistaEm = visto?.em ?? pulseira.vistaEm;
    final partes = [
      if (bateria != null) 'Bateria $bateria%',
      vistaEm == null ? 'Ainda não ouvida' : 'Vista ${tempoDesde(vistaEm)}',
      if (usuarioId != null && !pulseira.ehDono(usuarioId)) 'Você é responsável',
    ];
    return Card(
      child: ListTile(
        leading: Icon(bateria != null && bateria <= 20 ? Icons.battery_alert : Icons.watch, size: 36),
        title: Text(pulseira.titulo, style: const TextStyle(fontWeight: FontWeight.w600)),
        subtitle: Text(partes.join(' · ')),
        trailing: const Icon(Icons.chevron_right),
        onTap: () => context.push('/pulseira/${pulseira.id}'),
      ),
    );
  }
}
