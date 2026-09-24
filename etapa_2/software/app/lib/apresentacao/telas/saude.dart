import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../aplicacao/providers.dart';
import '../../aplicacao/saude.dart';
import '../comum.dart';

class SaudeTela extends ConsumerWidget {
  const SaudeTela({super.key});

  static const _idAlarmeTeste = 1;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final itens = ref.watch(saudeProvider);
    final cores = Theme.of(context).colorScheme;
    return Scaffold(
      appBar: AppBar(title: const Text('Saúde do sistema')),
      body: RefreshIndicator(
        onRefresh: () => ref.refresh(saudeProvider.future),
        child: ListView(padding: const EdgeInsets.all(16), children: [
          FilledButton.icon(
            onPressed: () => _testarAlarme(context, ref),
            icon: const Icon(Icons.campaign),
            label: const Text('Tocar alarme de teste'),
          ),
          const SizedBox(height: 16),
          ...itens.when(
            loading: () => const [Carregando()],
            error: (e, _) => [ErroCarregar(erro: e, tentarDeNovo: () => ref.invalidate(saudeProvider))],
            data: (lista) => [
              for (final item in lista)
                Card(
                  child: ListTile(
                    leading: Icon(
                      switch (item.ok) {
                        true => Icons.check_circle,
                        false => Icons.error,
                        null => Icons.help,
                      },
                      color: switch (item.ok) {
                        true => cores.primary,
                        false => cores.error,
                        null => cores.tertiary,
                      },
                      size: 32,
                    ),
                    title: Text(item.titulo),
                    subtitle: Text(item.detalhe),
                    trailing: item.acao == null
                        ? null
                        : TextButton(
                            onPressed: () async {
                              await executar(context, item.acao!);
                              ref.invalidate(saudeProvider);
                            },
                            child: Text(item.rotuloAcao ?? 'Corrigir'),
                          ),
                  ),
                ),
            ],
          ),
        ]),
      ),
    );
  }

  Future<void> _testarAlarme(BuildContext context, WidgetRef ref) async {
    final alarme = ref.read(alarmeProvider);
    final mensageiro = ScaffoldMessenger.of(context);
    if (!await alarme.notificacoesPermitidas() && !await alarme.pedirNotificacoes()) {
      mensageiro.showSnackBar(const SnackBar(
        content: Text('Notificações bloqueadas: o alarme não pode tocar. Permita nas configurações.'),
      ));
      return;
    }
    await alarme.emergencia(
      id: _idAlarmeTeste,
      titulo: 'TESTE DO ALARME',
      texto: 'Confira se a sirene tocou alto o bastante.',
      payload: 'teste',
    );
    mensageiro.showSnackBar(SnackBar(
      duration: const Duration(seconds: 30),
      content: const Text('Alarme de teste tocando.'),
      action: SnackBarAction(label: 'Silenciar', onPressed: () => alarme.silenciar(_idAlarmeTeste)),
    ));
  }
}
