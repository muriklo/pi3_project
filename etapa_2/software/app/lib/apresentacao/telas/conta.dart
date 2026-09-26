import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../aplicacao/providers.dart';
import '../../config.dart';

class ContaTela extends ConsumerWidget {
  const ContaTela({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final usuario = ref.watch(sessaoProvider)?.usuario;
    if (usuario == null) return const Scaffold();
    return Scaffold(
      appBar: AppBar(title: const Text('Conta')),
      body: ListView(padding: const EdgeInsets.all(16), children: [
        ListTile(leading: const Icon(Icons.person), title: Text(usuario.nome), subtitle: Text(usuario.email)),
        ListTile(
          leading: const Icon(Icons.badge),
          title: const Text('Seu código de responsável'),
          subtitle: SelectableText(usuario.id),
          trailing: IconButton(
            tooltip: 'Copiar',
            icon: const Icon(Icons.copy),
            onPressed: () {
              Clipboard.setData(ClipboardData(text: usuario.id));
              ScaffoldMessenger.of(context)
                  .showSnackBar(const SnackBar(content: Text('Código copiado.')));
            },
          ),
        ),
        const Padding(
          padding: EdgeInsets.symmetric(horizontal: 16),
          child: Text('Passe este código ao dono de uma pulseira para receber os alertas '
              'dela no app e poder responder "Estou indo".'),
        ),
        const SizedBox(height: 8),
        const ListTile(leading: Icon(Icons.dns), title: Text('Servidor'), subtitle: Text(urlApi)),
        const SizedBox(height: 24),
        OutlinedButton.icon(
          onPressed: () async {
            final sair = await showDialog<bool>(
              context: context,
              builder: (context) => AlertDialog(
                title: const Text('Sair?'),
                content: const Text('Este celular para de escutar as pulseiras e de alarmar.'),
                actions: [
                  TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
                  TextButton(onPressed: () => Navigator.pop(context, true), child: const Text('Sair')),
                ],
              ),
            );
            if (sair == true) await ref.read(sessaoProvider.notifier).sair();
          },
          icon: const Icon(Icons.logout),
          label: const Text('Sair'),
        ),
      ]),
    );
  }
}
