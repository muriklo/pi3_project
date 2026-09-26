import 'package:flutter/material.dart';

/// Cor de emergencia: o vermelho do Material 3, com contraste AA sobre branco.
const corEmergencia = Color(0xFFB3261E);

/// Tema com os criterios da Secao 3: botoes grandes (area de toque bem acima
/// de 48 dp), texto que respeita o tamanho de fonte do sistema.
ThemeData temaSysCare(Brightness brilho) {
  final esquema = ColorScheme.fromSeed(seedColor: const Color(0xFF0E6B5C), brightness: brilho);
  const textoBotao = TextStyle(fontSize: 18, fontWeight: FontWeight.w600);
  return ThemeData(
    colorScheme: esquema,
    // Botoes ocupam a largura da coluna: nunca os coloque dentro de um Row.
    filledButtonTheme: FilledButtonThemeData(
      style: FilledButton.styleFrom(minimumSize: const Size.fromHeight(56), textStyle: textoBotao),
    ),
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(minimumSize: const Size.fromHeight(56), textStyle: textoBotao),
    ),
    inputDecorationTheme: const InputDecorationTheme(border: OutlineInputBorder()),
    listTileTheme: const ListTileThemeData(minVerticalPadding: 12),
  );
}
