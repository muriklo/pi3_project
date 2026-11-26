import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../aplicacao/providers.dart';
import '../../aplicacao/receptor.dart';
import '../../aplicacao/saude.dart';
import '../comum.dart';

class NovaPulseiraTela extends ConsumerStatefulWidget {
  const NovaPulseiraTela({super.key});

  @override
  ConsumerState<NovaPulseiraTela> createState() => _NovaPulseiraTelaState();
}

class _NovaPulseiraTelaState extends ConsumerState<NovaPulseiraTela> {
  final _bleId = TextEditingController();
  final _nome = TextEditingController();
  final _pessoa = TextEditingController();
  bool _enviando = false;

  @override
  void dispose() {
    _bleId.dispose();
    _nome.dispose();
    _pessoa.dispose();
    super.dispose();
  }

  Future<void> _cadastrar() async {
    final bleId = _bleId.text.trim().toLowerCase();
    if (!RegExp(r'^[0-9a-f]{8}$').hasMatch(bleId)) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('O identificador tem 8 caracteres de 0 a 9 e a a f.')),
      );
      return;
    }
    setState(() => _enviando = true);
    try {
      final r = await ref.read(apiProvider).cadastrarPulseira(
            bleId: bleId,
            nome: _nome.text.trim().isEmpty ? 'Pulseira $bleId' : _nome.text.trim(),
            nomeUsuario: _pessoa.text.trim().isEmpty ? null : _pessoa.text.trim(),
          );
      ref.invalidate(pulseirasProvider);
      if (!mounted) return;
      await _mostrarChave(r.chave);
      if (mounted) Navigator.of(context).pop();
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(mensagemDeErro(e))));
      }
    } finally {
      if (mounted) setState(() => _enviando = false);
    }
  }

  /// A API devolve a chave uma unica vez: ela precisa ir para o firmware.
  Future<void> _mostrarChave(String chave) => showDialog<void>(
        context: context,
        barrierDismissible: false,
        builder: (context) => AlertDialog(
          title: const Text('Chave da pulseira'),
          content: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.start, children: [
            const Text('Grave esta chave no firmware da pulseira. Ela assina os alertas '
                'e NÃO será mostrada de novo.'),
            const SizedBox(height: 16),
            SelectableText(chave, style: const TextStyle(fontFamily: 'monospace', fontSize: 16)),
          ]),
          actions: [
            TextButton.icon(
              onPressed: () => Clipboard.setData(ClipboardData(text: chave)),
              icon: const Icon(Icons.copy),
              label: const Text('Copiar'),
            ),
            TextButton(onPressed: () => Navigator.pop(context), child: const Text('Já guardei')),
          ],
        ),
      );

  @override
  Widget build(BuildContext context) {
    final escutando = ref.watch(receptorProvider.select((s) => s.varredura)) == EstadoVarredura.escutando;
    final vistas = ref.watch(receptorProvider.select((s) => s.vistas));
    final daConta = {for (final p in ref.watch(pulseirasProvider).value ?? const []) p.bleId};
    final agora = DateTime.now();
    final proximas = vistas.values
        .where((v) => !daConta.contains(v.anuncio.bleId) && agora.difference(v.em).inMinutes < 2)
        .toList()
      ..sort((a, b) => (b.rssi ?? -127).compareTo(a.rssi ?? -127));

    return Scaffold(
      appBar: AppBar(title: const Text('Nova pulseira')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Text('Pulseiras por perto', style: Theme.of(context).textTheme.titleMedium),
          if (!escutando)
            ListTile(
              leading: const Icon(Icons.bluetooth_disabled),
              title: const Text('Ligue a escuta para encontrar a pulseira'),
              trailing: TextButton(
                onPressed: () => ref.read(receptorProvider.notifier).ligar(),
                child: const Text('Ligar'),
              ),
            )
          else if (proximas.isEmpty)
            const ListTile(
              leading: SizedBox.square(dimension: 24, child: CircularProgressIndicator()),
              title: Text('Procurando... aproxime a pulseira do celular.'),
            ),
          for (final v in proximas)
            ListTile(
              leading: const Icon(Icons.watch),
              title: Text('Pulseira ${v.anuncio.bleId}'),
              subtitle: Text('Sinal ${v.rssi ?? '?'} dBm · bateria ${v.anuncio.bateriaPct}% · '
                  '${tempoDesde(v.em)}'),
              selected: _bleId.text == v.anuncio.bleId,
              onTap: () => setState(() => _bleId.text = v.anuncio.bleId),
            ),
          const Divider(height: 32),
          TextField(
            controller: _bleId,
            decoration: const InputDecoration(
              labelText: 'Identificador da pulseira',
              helperText: '8 caracteres, gravados no firmware (ex.: a1b2c3d4)',
            ),
            maxLength: 8,
            autocorrect: false,
          ),
          const SizedBox(height: 8),
          TextField(
            controller: _pessoa,
            decoration: const InputDecoration(labelText: 'Quem vai usar (ex.: Sr. João)'),
            textCapitalization: TextCapitalization.words,
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _nome,
            decoration: const InputDecoration(labelText: 'Nome da pulseira (opcional)'),
          ),
          const SizedBox(height: 24),
          FilledButton(
            onPressed: _enviando ? null : _cadastrar,
            child: const Text('Cadastrar'),
          ),
        ],
      ),
    );
  }
}
