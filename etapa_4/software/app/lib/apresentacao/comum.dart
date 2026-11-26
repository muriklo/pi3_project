import 'package:flutter/material.dart';

import '../dados/api.dart';
import '../dominio/modelos.dart';

String doisDigitos(int n) => n.toString().padLeft(2, '0');

String dataHora(DateTime d) {
  final l = d.toLocal();
  return '${doisDigitos(l.day)}/${doisDigitos(l.month)} ${doisDigitos(l.hour)}:${doisDigitos(l.minute)}';
}

String mensagemDeErro(Object erro) => erro is ErroApi ? erro.mensagem : 'Algo deu errado: $erro';

/// Roda uma acao da tela e mostra o erro, se houver, sem derrubar a tela.
Future<bool> executar(BuildContext context, Future<void> Function() acao, {String? sucesso}) async {
  final mensageiro = ScaffoldMessenger.of(context);
  try {
    await acao();
    if (sucesso != null) mensageiro.showSnackBar(SnackBar(content: Text(sucesso)));
    return true;
  } catch (e) {
    mensageiro.showSnackBar(SnackBar(content: Text(mensagemDeErro(e))));
    return false;
  }
}

IconData iconeDoEvento(String tipo) => switch (tipo) {
      'fall' => Icons.personal_injury,
      'panic' => Icons.sos,
      'no_movement' => Icons.airline_seat_flat,
      'low_battery' => Icons.battery_alert,
      'device_offline' => Icons.bluetooth_disabled,
      _ => Icons.notifications,
    };

bool ehEmergencia(String tipo) => tipo == 'fall' || tipo == 'panic' || tipo == 'no_movement';

/// Situacao do alerta com cor, icone e texto: nunca so a cor (Secao 3).
class SeloStatus extends StatelessWidget {
  const SeloStatus(this.status, {super.key});

  final String status;

  @override
  Widget build(BuildContext context) {
    final cores = Theme.of(context).colorScheme;
    final (icone, cor) = switch (status) {
      'open' => (Icons.error, cores.error),
      'acked' => (Icons.directions_run, cores.tertiary),
      'resolved' => (Icons.check_circle, cores.primary),
      _ => (Icons.do_not_disturb_on, cores.outline),
    };
    return Row(mainAxisSize: MainAxisSize.min, children: [
      Icon(icone, color: cor, size: 20),
      const SizedBox(width: 4),
      Text(rotulosStatus[status] ?? status, style: TextStyle(color: cor, fontWeight: FontWeight.w600)),
    ]);
  }
}

class ErroCarregar extends StatelessWidget {
  const ErroCarregar({required this.erro, required this.tentarDeNovo, super.key});

  final Object erro;
  final VoidCallback tentarDeNovo;

  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.all(24),
        child: Column(mainAxisSize: MainAxisSize.min, children: [
          Text(mensagemDeErro(erro), textAlign: TextAlign.center),
          const SizedBox(height: 16),
          OutlinedButton.icon(
            onPressed: tentarDeNovo,
            icon: const Icon(Icons.refresh),
            label: const Text('Tentar de novo'),
          ),
        ]),
      );
}

class Carregando extends StatelessWidget {
  const Carregando({super.key});

  @override
  Widget build(BuildContext context) =>
      const Padding(padding: EdgeInsets.all(32), child: Center(child: CircularProgressIndicator()));
}
