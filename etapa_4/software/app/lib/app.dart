import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'aplicacao/providers.dart';
import 'apresentacao/rotas.dart';
import 'apresentacao/tema.dart';

class AppSysCare extends ConsumerStatefulWidget {
  const AppSysCare({super.key});

  @override
  ConsumerState<AppSysCare> createState() => _AppSysCareState();
}

class _AppSysCareState extends ConsumerState<AppSysCare> {
  late final AppLifecycleListener _ciclo;

  @override
  void initState() {
    super.initState();
    ref.read(rotinasProvider).iniciar();
    // Voltou ao app: tenta de novo o que ficou na fila.
    _ciclo = AppLifecycleListener(onResume: () => ref.read(rotinasProvider).drenarFila());
  }

  @override
  void dispose() {
    _ciclo.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final roteador = ref.watch(roteadorProvider);
    // Alarme novo do radio: abre a tela de alerta por cima do que estiver aberto.
    ref.listen(receptorProvider.select((s) => s.alarmes.keys.toSet()), (antes, depois) {
      for (final chave in depois.difference(antes ?? const {})) {
        roteador.push('/alarme/$chave');
      }
    });
    return MaterialApp.router(
      title: 'SysCare',
      theme: temaSysCare(Brightness.light),
      darkTheme: temaSysCare(Brightness.dark),
      routerConfig: roteador,
    );
  }
}
