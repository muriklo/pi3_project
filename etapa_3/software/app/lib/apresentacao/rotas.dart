import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

import '../aplicacao/providers.dart';
import 'telas/alerta.dart';
import 'telas/conta.dart';
import 'telas/entrar.dart';
import 'telas/historico.dart';
import 'telas/inicio.dart';
import 'telas/nova_pulseira.dart';
import 'telas/pulseira.dart';
import 'telas/saude.dart';

/// Mapa de telas da Figura 2.
final roteadorProvider = Provider<GoRouter>((ref) {
  final mudouSessao = ValueNotifier(0);
  ref.listen(sessaoProvider, (_, _) => mudouSessao.value++);
  ref.onDispose(mudouSessao.dispose);

  return GoRouter(
    refreshListenable: mudouSessao,
    redirect: (context, estado) {
      final logado = ref.read(sessaoProvider) != null;
      final local = estado.matchedLocation;
      // O alarme local abre mesmo sem sessao: ele nao depende da API.
      if (local.startsWith('/alarme/')) return null;
      if (!logado && local != '/entrar') return '/entrar';
      if (logado && local == '/entrar') return '/';
      return null;
    },
    routes: [
      GoRoute(path: '/entrar', builder: (_, _) => const EntrarTela()),
      GoRoute(
        path: '/alarme/:chave',
        builder: (_, e) => AlertaTela.local(chave: e.pathParameters['chave']!),
      ),
      GoRoute(
        path: '/',
        builder: (_, _) => const InicioTela(),
        routes: [
          GoRoute(path: 'nova-pulseira', builder: (_, _) => const NovaPulseiraTela()),
          GoRoute(
            path: 'pulseira/:id',
            builder: (_, e) => PulseiraTela(id: e.pathParameters['id']!),
          ),
          GoRoute(path: 'historico', builder: (_, _) => const HistoricoTela()),
          GoRoute(path: 'saude', builder: (_, _) => const SaudeTela()),
          GoRoute(path: 'conta', builder: (_, _) => const ContaTela()),
          GoRoute(
            path: 'alerta/:id',
            builder: (_, e) => AlertaTela.servidor(id: e.pathParameters['id']!),
          ),
        ],
      ),
    ],
  );
});
