import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'aplicacao/providers.dart';
import 'aplicacao/push.dart';
import 'app.dart';
import 'apresentacao/rotas.dart';
import 'dados/alarme.dart';
import 'dados/armazenamento.dart';
import 'dados/fila_envio.dart';
import 'dados/push.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  final alarme = Alarme();
  final preferencias = await PreferenciasReceptor.abrir();
  final push = PushFirebase();
  await push.iniciar(pushEmSegundoPlano);
  final container = ProviderContainer(
    overrides: [
      sessaoInicialProvider.overrideWithValue(await const CofreSessao().ler()),
      filaProvider.overrideWithValue(await FilaEnvioSqlite.abrir()),
      preferenciasProvider.overrideWithValue(preferencias),
      alarmeProvider.overrideWithValue(alarme),
      pushProvider.overrideWithValue(push),
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

  if (push.ativo) {
    // Com o app aberto o push nao passa pelo isolate de segundo plano: o
    // alarme toca aqui e a tela de alerta abre por cima, como a do radio.
    push.emPrimeiroPlano.listen((dados) async {
      final aviso = await tratarPush(dados, alarme: alarme, ouvidos: preferencias.dedup);
      if (aviso != null && aviso.emergencia) {
        container.read(roteadorProvider).push('/alerta/${aviso.alertaId}');
      }
    });
    unawaited(container.read(registroPushProvider).registrar());
  }
}

void _abrirPelaNotificacao(ProviderContainer container, String? payload) {
  if (payload == null) return;
  final roteador = container.read(roteadorProvider);
  if (payload.startsWith('local:')) roteador.push('/alarme/${payload.substring('local:'.length)}');
  if (payload.startsWith('alerta:')) roteador.push('/alerta/${payload.substring('alerta:'.length)}');
}
