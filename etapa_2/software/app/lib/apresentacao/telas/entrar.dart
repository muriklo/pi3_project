import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../aplicacao/providers.dart';
import '../../aplicacao/receptor.dart';
import '../comum.dart';

class EntrarTela extends ConsumerStatefulWidget {
  const EntrarTela({super.key});

  @override
  ConsumerState<EntrarTela> createState() => _EntrarTelaState();
}

class _EntrarTelaState extends ConsumerState<EntrarTela> {
  final _nome = TextEditingController();
  final _email = TextEditingController();
  final _senha = TextEditingController();
  bool _criarConta = false;
  bool _enviando = false;
  String? _erro;

  @override
  void dispose() {
    _nome.dispose();
    _email.dispose();
    _senha.dispose();
    super.dispose();
  }

  Future<void> _enviar() async {
    setState(() {
      _enviando = true;
      _erro = null;
    });
    final sessao = ref.read(sessaoProvider.notifier);
    try {
      if (_criarConta) {
        await sessao.criarConta(_nome.text.trim(), _email.text.trim(), _senha.text);
      } else {
        await sessao.entrar(_email.text.trim(), _senha.text);
      }
    } catch (e) {
      if (mounted) setState(() => _erro = mensagemDeErro(e));
    } finally {
      if (mounted) setState(() => _enviando = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final escutando = ref.watch(receptorProvider.select((s) => s.varredura)) != EstadoVarredura.desligado;
    return Scaffold(
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(24),
          children: [
            const SizedBox(height: 24),
            Icon(Icons.health_and_safety, size: 72, color: Theme.of(context).colorScheme.primary),
            const SizedBox(height: 8),
            Text('SysCare', textAlign: TextAlign.center, style: Theme.of(context).textTheme.headlineMedium),
            const SizedBox(height: 24),
            if (escutando)
              const Card(
                child: ListTile(
                  leading: Icon(Icons.bluetooth_searching),
                  title: Text('A escuta continua ligada'),
                  subtitle: Text('Este celular segue alarmando. Entre de novo para '
                      'que os alertas também cheguem aos responsáveis.'),
                ),
              ),
            const SizedBox(height: 8),
            SegmentedButton<bool>(
              segments: const [
                ButtonSegment(value: false, label: Text('Entrar')),
                ButtonSegment(value: true, label: Text('Criar conta')),
              ],
              selected: {_criarConta},
              onSelectionChanged: (s) => setState(() => _criarConta = s.first),
            ),
            const SizedBox(height: 16),
            if (_criarConta) ...[
              TextField(
                controller: _nome,
                decoration: const InputDecoration(labelText: 'Seu nome'),
                textCapitalization: TextCapitalization.words,
              ),
              const SizedBox(height: 12),
            ],
            TextField(
              controller: _email,
              decoration: const InputDecoration(labelText: 'E-mail'),
              keyboardType: TextInputType.emailAddress,
              autocorrect: false,
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _senha,
              decoration: InputDecoration(
                labelText: 'Senha',
                helperText: _criarConta ? 'Pelo menos 8 caracteres' : null,
              ),
              obscureText: true,
              onSubmitted: (_) => _enviar(),
            ),
            if (_erro != null) ...[
              const SizedBox(height: 12),
              Text(_erro!, style: TextStyle(color: Theme.of(context).colorScheme.error)),
            ],
            const SizedBox(height: 24),
            FilledButton(
              onPressed: _enviando ? null : _enviar,
              child: _enviando
                  ? const SizedBox.square(dimension: 24, child: CircularProgressIndicator())
                  : Text(_criarConta ? 'Criar conta' : 'Entrar'),
            ),
          ],
        ),
      ),
    );
  }
}
