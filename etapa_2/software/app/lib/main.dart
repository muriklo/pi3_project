import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'aplicacao/providers.dart';
import 'app.dart';
import 'apresentacao/rotas.dart';
import 'dados/alarme.dart';
import 'dados/armazenamento.dart';
import 'dados/fila_envio.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  final alarme = Alarme();
  final preferencias = await PreferenciasReceptor.abrir();
  final container = ProviderContainer(
    overrides: [
      sessaoInicialProvider.overrideWithValue(await const CofreSessao().ler()),
      filaProvider.overrideWithValue(await FilaEnvioSqlite.abrir()),
      preferenciasProvider.overrideWithValue(preferencias),
      alarmeProvider.overrideWithValue(alarme),
    ],
    // Falhas de rede sao mostradas na tela com "Tentar de novo".
    retry: (_, _) => null,
  );

  await alarme.inicializar((payload) => _abrirPelaNotificacao(container, payload));
  runApp(UncontrolledProviderScope(container: container, child: const AppSysCare()));

  _abrirPelaNotificacao(container, await alarme.payloadDeAbertura());
  // A escuta volta sozinha se estava ligada: o alarme nao pode depender de
  // alguem lembrar de abrir o app e religar.
  if (preferencias.escutar) unawaited(container.read(receptorProvider.notifier).ligar());
}

void _abrirPelaNotificacao(ProviderContainer container, String? payload) {
  if (payload == null || !payload.startsWith('local:')) return;
  container.read(roteadorProvider).push('/alarme/${payload.substring('local:'.length)}');
}
