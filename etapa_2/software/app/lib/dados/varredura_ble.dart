import 'package:syscare_protocol/syscare_protocol.dart';
import 'package:universal_ble/universal_ble.dart';

/// Um manufacturer data com o Company ID do SysCare, como chegou do radio.
/// Ainda nao validado: o Company ID 0xFFFF e compartilhado com outros
/// dispositivos em desenvolvimento.
class Recepcao {
  const Recepcao({required this.bytes, required this.em, this.rssi});

  final List<int> bytes;
  final int? rssi;
  final DateTime em;
}

/// Adaptador do `universal_ble` (Secao 4 da arquitetura).
class VarreduraBle {
  /// Recepcoes filtradas pelo Company ID. Fluxo broadcast: a tela de nova
  /// pulseira e o receptor podem ouvir ao mesmo tempo.
  Stream<Recepcao> get recepcoes => UniversalBle.scanStream.expand(_extrair);

  Stream<AvailabilityState> get disponibilidade => UniversalBle.availabilityStream;

  Future<AvailabilityState> estado() => UniversalBle.getBluetoothAvailabilityState();

  /// Inclui localizacao: sem `neverForLocation`, e o GPS do alerta usa ela.
  Future<bool> temPermissoes() => UniversalBle.hasPermissions(withAndroidFineLocation: true);

  /// Lanca excecao se o usuario negar.
  Future<void> pedirPermissoes() => UniversalBle.requestPermissions(withAndroidFineLocation: true);

  /// Varredura com filtro no radio: sem filtro, o Android 8.1+ pausa a
  /// varredura com a tela apagada.
  Future<void> iniciar() => UniversalBle.startScan(
        scanFilter: ScanFilter(
          withManufacturerData: [ManufacturerDataFilter(companyIdentifier: companyId)],
        ),
        platformConfig: PlatformConfig(
          android: AndroidOptions(scanMode: AndroidScanMode.lowLatency),
        ),
      );

  Future<void> parar() => UniversalBle.stopScan();

  static Iterable<Recepcao> _extrair(BleDevice d) {
    final em = DateTime.now();
    return d.manufacturerDataList
        .where((m) => m.companyId == companyId)
        .map((m) => Recepcao(bytes: m.payload, rssi: d.rssi, em: em));
  }
}
