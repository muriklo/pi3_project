import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:syscare_app/aplicacao/providers.dart';
import 'package:syscare_app/apresentacao/telas/entrar.dart';
import 'package:syscare_app/dados/armazenamento.dart';

void main() {
  testWidgets('tela de entrada alterna entre entrar e criar conta', (tester) async {
    SharedPreferences.setMockInitialValues({});
    final preferencias = await PreferenciasReceptor.abrir();
    await tester.pumpWidget(ProviderScope(
      overrides: [preferenciasProvider.overrideWithValue(preferencias)],
      child: const MaterialApp(home: EntrarTela()),
    ));

    expect(find.text('SysCare'), findsOneWidget);
    expect(find.text('Seu nome'), findsNothing);
    // Escuta desligada: sem o aviso de que ela continua.
    expect(find.text('A escuta continua ligada'), findsNothing);

    await tester.tap(find.text('Criar conta').first);
    await tester.pump();
    expect(find.text('Seu nome'), findsOneWidget);
  });
}
