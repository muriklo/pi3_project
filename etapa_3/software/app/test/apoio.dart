import 'dart:convert';
import 'dart:io';

import 'package:syscare_app/dados/fila_envio.dart';
import 'package:syscare_protocol/syscare_protocol.dart';

/// Respostas reais da API, capturadas com o TestClient do FastAPI.
final Map<String, dynamic> respostasApi =
    jsonDecode(File('test/respostas_api.json').readAsStringSync()) as Map<String, dynamic>;

Anuncio anuncio({TipoEvento evento = TipoEvento.queda, int seq = 43, String bleId = 'a1b2c3d4'}) =>
    Anuncio.decodificar([
      versaoProtocolo,
      ...hexParaBytes(bleId),
      evento.codigo,
      seq & 0xFF,
      (seq >> 8) & 0xFF,
      87,
      32,
      0, 0, 0, 0,
    ]);

/// Fila em memoria com a mesma semantica da de SQLite.
class FilaMemoria implements FilaEnvio {
  final Map<int, ItemFila> itens = {};
  int _proximoId = 1;

  @override
  Future<ItemFila> adicionar(ItemFila item) async {
    final comId = item.copiar(id: _proximoId++);
    itens[comId.id!] = comId;
    return comId;
  }

  @override
  Future<List<ItemFila>> pendentes(DateTime agora) async =>
      (itens.values.where((i) => !i.proximaEm.isAfter(agora)).toList()..sort((a, b) => a.id!.compareTo(b.id!)));

  @override
  Future<void> remover(int id) async => itens.remove(id);

  @override
  Future<void> adiar(ItemFila item, DateTime proximaEm) async =>
      itens[item.id!] = item.copiar(tentativas: item.tentativas + 1, proximaEm: proximaEm);

  @override
  Future<int> total() async => itens.length;
}
