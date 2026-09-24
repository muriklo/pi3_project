import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../../aplicacao/providers.dart';
import '../../aplicacao/saude.dart';
import '../../dominio/modelos.dart';
import '../comum.dart';

class PulseiraTela extends ConsumerWidget {
  const PulseiraTela({required this.id, super.key});

  final String id;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final lista = ref.watch(pulseirasProvider);
    final usuarioId = ref.watch(sessaoProvider)?.usuario.id ?? '';
    Pulseira? pulseira;
    for (final p in lista.value ?? const <Pulseira>[]) {
      if (p.id == id) pulseira = p;
    }
    if (pulseira == null) {
      return Scaffold(
        appBar: AppBar(),
        body: lista.isLoading ? const Carregando() : const Center(child: Text('Pulseira não encontrada.')),
      );
    }
    final dono = pulseira.ehDono(usuarioId);
    return DefaultTabController(
      length: 3,
      child: Scaffold(
        appBar: AppBar(
          title: Text(pulseira.titulo),
          bottom: const TabBar(tabs: [
            Tab(text: 'Dados'),
            Tab(text: 'Responsáveis'),
            Tab(text: 'Histórico'),
          ]),
        ),
        body: TabBarView(children: [
          _Dados(pulseira, dono: dono),
          dono
              ? _Responsaveis(pulseira.id)
              : const Padding(
                  padding: EdgeInsets.all(24),
                  child: Text('Só o dono da pulseira gerencia os responsáveis: a lista tem '
                      'telefones de outras pessoas.'),
                ),
          _Historico(pulseira.id),
        ]),
      ),
    );
  }
}

class _Dados extends ConsumerWidget {
  const _Dados(this.pulseira, {required this.dono});

  final Pulseira pulseira;
  final bool dono;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final vistaEm = pulseira.vistaEm;
    return ListView(padding: const EdgeInsets.all(16), children: [
      ListTile(title: const Text('Quem usa'), subtitle: Text(pulseira.nomeUsuario ?? '—')),
      ListTile(title: const Text('Nome da pulseira'), subtitle: Text(pulseira.nome)),
      ListTile(title: const Text('Identificador BLE'), subtitle: Text(pulseira.bleId)),
      ListTile(
        title: const Text('Bateria'),
        subtitle: Text(pulseira.bateriaPct == null ? '—' : '${pulseira.bateriaPct}%'),
      ),
      ListTile(
        title: const Text('Última comunicação com o servidor'),
        subtitle: Text(vistaEm == null ? 'Nunca' : '${dataHora(vistaEm)} (${tempoDesde(vistaEm)})'),
      ),
      ListTile(
        title: const Text('Seu papel'),
        subtitle: Text(dono ? 'Dono' : 'Responsável: vê e atende os alertas'),
      ),
      if (dono) ...[
        const SizedBox(height: 16),
        OutlinedButton.icon(
          onPressed: () => _editar(context, ref),
          icon: const Icon(Icons.edit),
          label: const Text('Editar'),
        ),
      ],
    ]);
  }

  Future<void> _editar(BuildContext context, WidgetRef ref) async {
    final nome = TextEditingController(text: pulseira.nome);
    final pessoa = TextEditingController(text: pulseira.nomeUsuario);
    final salvar = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Editar pulseira'),
        content: Column(mainAxisSize: MainAxisSize.min, children: [
          TextField(controller: pessoa, decoration: const InputDecoration(labelText: 'Quem usa')),
          const SizedBox(height: 12),
          TextField(controller: nome, decoration: const InputDecoration(labelText: 'Nome da pulseira')),
        ]),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
          TextButton(onPressed: () => Navigator.pop(context, true), child: const Text('Salvar')),
        ],
      ),
    );
    if (salvar != true || !context.mounted) return;
    await executar(context, () async {
      await ref.read(apiProvider).editarPulseira(pulseira.id, nome: nome.text.trim(), nomeUsuario: pessoa.text.trim());
      ref.invalidate(pulseirasProvider);
    });
  }
}

class _Responsaveis extends ConsumerWidget {
  const _Responsaveis(this.pulseiraId);

  final String pulseiraId;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final responsaveis = ref.watch(responsaveisProvider(pulseiraId));
    final api = ref.read(apiProvider);
    void recarregar() => ref.invalidate(responsaveisProvider(pulseiraId));
    return responsaveis.when(
      loading: () => const Carregando(),
      error: (e, _) => ErroCarregar(erro: e, tentarDeNovo: recarregar),
      data: (lista) {
        final ordenada = [...lista]..sort((a, b) => a.prioridade.compareTo(b.prioridade));
        return ListView(padding: const EdgeInsets.all(16), children: [
          const Text('Todos os responsáveis ativos são avisados ao mesmo tempo; a prioridade '
              'define a ordem de exibição e de quem recebe SMS primeiro.'),
          const SizedBox(height: 8),
          for (final r in ordenada)
            Card(
              child: ListTile(
                leading: CircleAvatar(child: Text('${r.prioridade}')),
                title: Text(r.nome),
                subtitle: Text([
                  if (r.telefone != null) r.telefone!,
                  if (r.email != null) r.email!,
                  r.usuarioId == null ? 'Sem app: só recebe SMS' : 'Recebe o push no app',
                ].join(' · ')),
                trailing: Row(mainAxisSize: MainAxisSize.min, children: [
                  Switch(
                    value: r.ativo,
                    onChanged: (ativo) => executar(context, () async {
                      await api.editarResponsavel(pulseiraId, r.id, ativo: ativo);
                      recarregar();
                    }),
                  ),
                  IconButton(
                    tooltip: 'Remover',
                    icon: const Icon(Icons.delete_outline),
                    onPressed: () => executar(context, () async {
                      await api.removerResponsavel(pulseiraId, r.id);
                      recarregar();
                    }),
                  ),
                ]),
              ),
            ),
          const SizedBox(height: 16),
          FilledButton.icon(
            onPressed: () => _adicionar(context, ref, proximaPrioridade: lista.length + 1),
            icon: const Icon(Icons.person_add),
            label: const Text('Adicionar responsável'),
          ),
        ]);
      },
    );
  }

  Future<void> _adicionar(BuildContext context, WidgetRef ref, {required int proximaPrioridade}) async {
    final nome = TextEditingController();
    final telefone = TextEditingController();
    final codigo = TextEditingController();
    final salvar = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Novo responsável'),
        content: SingleChildScrollView(
          child: Column(mainAxisSize: MainAxisSize.min, children: [
            TextField(controller: nome, decoration: const InputDecoration(labelText: 'Nome')),
            const SizedBox(height: 12),
            TextField(
              controller: telefone,
              decoration: const InputDecoration(labelText: 'Telefone (opcional)'),
              keyboardType: TextInputType.phone,
            ),
            const SizedBox(height: 12),
            TextField(
              controller: codigo,
              decoration: const InputDecoration(
                labelText: 'Código do app (opcional)',
                helperText: 'Está na tela Conta do app do responsável.\nSem ele, só SMS.',
                helperMaxLines: 2,
              ),
              autocorrect: false,
            ),
          ]),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(context, false), child: const Text('Cancelar')),
          TextButton(onPressed: () => Navigator.pop(context, true), child: const Text('Adicionar')),
        ],
      ),
    );
    if (salvar != true || !context.mounted) return;
    await executar(context, () async {
      await ref.read(apiProvider).adicionarResponsavel(
            pulseiraId,
            nome: nome.text.trim(),
            telefone: telefone.text.trim().isEmpty ? null : telefone.text.trim(),
            usuarioId: codigo.text.trim().isEmpty ? null : codigo.text.trim(),
            prioridade: proximaPrioridade.clamp(1, 10),
          );
      ref.invalidate(responsaveisProvider(pulseiraId));
    });
  }
}

class _Historico extends ConsumerWidget {
  const _Historico(this.pulseiraId);

  final String pulseiraId;

  @override
  Widget build(BuildContext context, WidgetRef ref) => ref.watch(alertasProvider(pulseiraId)).when(
        loading: () => const Carregando(),
        error: (e, _) => ErroCarregar(erro: e, tentarDeNovo: () => ref.invalidate(alertasProvider(pulseiraId))),
        data: (alertas) => alertas.isEmpty
            ? const Center(child: Text('Nenhum evento registrado.'))
            : ListView(children: [
                for (final a in alertas)
                  ListTile(
                    leading: Icon(iconeDoEvento(a.tipo)),
                    title: Text(rotulosEvento[a.tipo] ?? a.tipo),
                    subtitle: Text(dataHora(a.ocorridoEm)),
                    trailing: SeloStatus(a.status),
                    onTap: () => context.push('/alerta/${a.id}'),
                  ),
              ]),
      );
}
