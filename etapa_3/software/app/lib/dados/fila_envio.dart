import 'package:path/path.dart' as p;
import 'package:sqflite/sqflite.dart';
import 'package:syscare_protocol/syscare_protocol.dart';

import 'localizacao.dart';

/// Um evento esperando para ir a API.
class ItemFila {
  const ItemFila({
    required this.anuncio,
    required this.recebidoEm,
    required this.proximaEm,
    this.id,
    this.rssi,
    this.posicao,
    this.rotulo,
    this.tentativas = 0,
  });

  final int? id;
  final Anuncio anuncio;
  final DateTime recebidoEm;
  final int? rssi;
  final Posicao? posicao;

  /// Qual celular ouviu, ex.: "Celular de Maria" (gateway_label na API).
  final String? rotulo;
  final int tentativas;
  final DateTime proximaEm;

  /// Identifica o evento: o mesmo `seq` da mesma pulseira e o mesmo alerta.
  String get chave => chaveDoEvento(anuncio);

  ItemFila copiar({int? id, int? tentativas, DateTime? proximaEm}) => ItemFila(
        id: id ?? this.id,
        anuncio: anuncio,
        recebidoEm: recebidoEm,
        rssi: rssi,
        posicao: posicao,
        rotulo: rotulo,
        tentativas: tentativas ?? this.tentativas,
        proximaEm: proximaEm ?? this.proximaEm,
      );
}

String chaveDoEvento(Anuncio a) => '${a.bleId}-${a.seq}';

/// Fila persistente: todo evento e gravado ANTES do envio (Secao 5.4), para
/// que uma falha de rede nunca o descarte.
abstract interface class FilaEnvio {
  Future<ItemFila> adicionar(ItemFila item);
  Future<List<ItemFila>> pendentes(DateTime agora);
  Future<void> remover(int id);
  Future<void> adiar(ItemFila item, DateTime proximaEm);
  Future<int> total();
}

class FilaEnvioSqlite implements FilaEnvio {
  FilaEnvioSqlite._(this._db);

  final Database _db;

  static Future<FilaEnvioSqlite> abrir() async {
    final caminho = p.join(await getDatabasesPath(), 'syscare_fila.db');
    final db = await openDatabase(
      caminho,
      version: 1,
      onCreate: (db, _) => db.execute('''
        CREATE TABLE fila (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          bytes_hex TEXT NOT NULL,
          recebido_em TEXT NOT NULL,
          rssi INTEGER,
          latitude REAL,
          longitude REAL,
          precisao_m REAL,
          rotulo TEXT,
          tentativas INTEGER NOT NULL DEFAULT 0,
          proxima_em TEXT NOT NULL
        )'''),
    );
    return FilaEnvioSqlite._(db);
  }

  @override
  Future<ItemFila> adicionar(ItemFila item) async {
    final id = await _db.insert('fila', {
      'bytes_hex': item.anuncio.hex,
      'recebido_em': item.recebidoEm.toUtc().toIso8601String(),
      'rssi': item.rssi,
      'latitude': item.posicao?.latitude,
      'longitude': item.posicao?.longitude,
      'precisao_m': item.posicao?.precisaoM,
      'rotulo': item.rotulo,
      'tentativas': item.tentativas,
      'proxima_em': item.proximaEm.toUtc().toIso8601String(),
    });
    return item.copiar(id: id);
  }

  @override
  Future<List<ItemFila>> pendentes(DateTime agora) async {
    final linhas = await _db.query(
      'fila',
      where: 'proxima_em <= ?',
      whereArgs: [agora.toUtc().toIso8601String()],
      orderBy: 'id',
    );
    final itens = <ItemFila>[];
    for (final l in linhas) {
      try {
        itens.add(_ler(l));
      } on AnuncioInvalido {
        // Nao deveria acontecer: so entram anuncios validados.
        await remover(l['id']! as int);
      }
    }
    return itens;
  }

  static ItemFila _ler(Map<String, Object?> l) {
    final lat = (l['latitude'] as num?)?.toDouble();
    return ItemFila(
      id: l['id']! as int,
      anuncio: Anuncio.deHex(l['bytes_hex']! as String),
      recebidoEm: DateTime.parse(l['recebido_em']! as String),
      rssi: l['rssi'] as int?,
      posicao: lat == null
          ? null
          : Posicao(
              latitude: lat,
              longitude: (l['longitude']! as num).toDouble(),
              precisaoM: (l['precisao_m'] as num?)?.toDouble() ?? 0,
            ),
      rotulo: l['rotulo'] as String?,
      tentativas: l['tentativas']! as int,
      proximaEm: DateTime.parse(l['proxima_em']! as String),
    );
  }

  @override
  Future<void> remover(int id) => _db.delete('fila', where: 'id = ?', whereArgs: [id]);

  @override
  Future<void> adiar(ItemFila item, DateTime proximaEm) => _db.update(
        'fila',
        {'tentativas': item.tentativas + 1, 'proxima_em': proximaEm.toUtc().toIso8601String()},
        where: 'id = ?',
        whereArgs: [item.id],
      );

  @override
  Future<int> total() async =>
      Sqflite.firstIntValue(await _db.rawQuery('SELECT COUNT(*) FROM fila')) ?? 0;
}
