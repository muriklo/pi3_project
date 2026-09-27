import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../aplicacao/providers.dart';
import '../../dominio/modelos.dart';
import '../comum.dart';

class HistoricoTela extends ConsumerWidget {
  const HistoricoTela({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final alertas = ref.watch(alertasProvider(null));
    final nomes = {
      for (final p in ref.watch(pulseirasProvider).value ?? const <Pulseira>[]) p.id: p.titulo,
    };
    return Scaffold(
      appBar: AppBar(title: const Text('Histórico')),
      body: RefreshIndicator(
        onRefresh: () => ref.refresh(alertasProvider(null).future),
        child: alertas.when(
          loading: () => const Carregando(),
          error: (e, _) => ListView(children: [
            ErroCarregar(erro: e, tentarDeNovo: () => ref.invalidate(alertasProvider(null))),
          ]),
          data: (lista) => lista.isEmpty
              ? ListView(children: const [
                  Padding(padding: EdgeInsets.all(24), child: Text('Nenhum evento registrado.')),
                ])
              : ListView.separated(
                  itemCount: lista.length,
                  separatorBuilder: (_, _) => const Divider(height: 1),
                  itemBuilder: (context, i) {
                    final a = lista[i];
                    return ListTile(
                      leading: Icon(iconeDoEvento(a.tipo), size: 32),
                      title: Text(rotulosEvento[a.tipo] ?? a.tipo),
                      subtitle: Text('${nomes[a.deviceId] ?? 'Pulseira'} · ${dataHora(a.ocorridoEm)}'),
                      trailing: SeloStatus(a.status),
                      onTap: () => context.push('/alerta/${a.id}'),
                    );
                  },
                ),
        ),
      ),
    );
  }
}
