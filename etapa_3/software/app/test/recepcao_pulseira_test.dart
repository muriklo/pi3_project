import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:syscare_app/aplicacao/providers.dart';
import 'package:syscare_app/aplicacao/receptor.dart';
import 'package:syscare_app/apresentacao/telas/inicio.dart';
import 'package:syscare_app/apresentacao/telas/nova_pulseira.dart';
import 'package:syscare_app/dados/alarme.dart';
import 'package:syscare_app/dados/armazenamento.dart';
import 'package:syscare_app/dados/sms_celular.dart';
import 'package:syscare_app/dominio/modelos.dart';
import 'package:syscare_protocol/syscare_protocol.dart';
import 'package:universal_ble/universal_ble.dart';

/// Os 14 bytes que o firmware de teste (firmware/teste_ble) anuncia:
/// ble_id 01020304, heartbeat, seq 0, bateria 100%, chave 00..0F.
final heartbeatDoFirmware = hexParaBytes('01' '01020304' '05' '0000' '64' '00' '2fccb01d');

/// Radio falso no lugar do plugin: o resto do caminho e o do app.
class _RadioFalso extends UniversalBlePlatform {
  ScanFilter? filtro;
  PlatformConfig? configuracao;

  void ouvir(List<int> payload, {int rssi = -58}) => updateScanResult(BleDevice(
        deviceId: 'C0:FF:EE:00:00:01',
        name: null,
        rssi: rssi,
        manufacturerDataList: [ManufacturerData(companyId, Uint8List.fromList(payload))],
      ));

  @override
  Future<AvailabilityState> getBluetoothAvailabilityState() async => AvailabilityState.poweredOn;

  @override
  Future<void> startScan({ScanFilter? scanFilter, PlatformConfig? platformConfig}) async {
    filtro = scanFilter;
    configuracao = platformConfig;
  }

  @override
  Future<void> stopScan() async {}

  @override
  dynamic noSuchMethod(Invocation invocation) => throw UnimplementedError('$invocation');
}

class _AlarmeMudo extends Alarme {
  @override
  Future<bool> pedirNotificacoes() async => true;
}

class _SemSms extends SmsCelular {
  @override
  Future<bool> disponivel() async => false;
}

void main() {
  late _RadioFalso radio;
  late PreferenciasReceptor preferencias;

  setUp(() async {
    radio = _RadioFalso();
    UniversalBle.setInstance(radio);
    SharedPreferences.setMockInitialValues({});
    preferencias = await PreferenciasReceptor.abrir();
  });

  Future<ProviderContainer> abrir(WidgetTester tester, Widget tela, List<Pulseira> pulseiras) async {
    await tester.pumpWidget(ProviderScope(
      overrides: [
        preferenciasProvider.overrideWithValue(preferencias),
        alarmeProvider.overrideWithValue(_AlarmeMudo()),
        smsCelularProvider.overrideWithValue(_SemSms()),
        sessaoInicialProvider.overrideWithValue(const SessaoSalva(
          token: 'x',
          usuario: Usuario(id: 'u1', email: 'a@b.c', nome: 'Ana'),
        )),
        pulseirasProvider.overrideWith((ref) async => pulseiras),
      ],
      child: MaterialApp(home: tela),
    ));
    final container = ProviderScope.containerOf(tester.element(find.byWidget(tela)));
    await tester.runAsync(() => container.read(receptorProvider.notifier).ligar());
    await tester.pump();
    expect(container.read(receptorProvider).varredura, EstadoVarredura.escutando);
    return container;
  }

  testWidgets('a varredura procura o Company ID 0xFFFF no advertising legado', (tester) async {
    await abrir(tester, const NovaPulseiraTela(), const []);

    expect(radio.filtro!.withManufacturerData.single.companyIdentifier, 0xFFFF);
    expect(radio.configuracao!.android!.legacy, isTrue);
  });

  testWidgets('pulseira nao cadastrada aparece em Nova pulseira', (tester) async {
    await abrir(tester, const NovaPulseiraTela(), const []);
    expect(find.text('Pulseira 01020304'), findsNothing);

    radio.ouvir(heartbeatDoFirmware);
    await tester.pump();

    expect(find.text('Pulseira 01020304'), findsOneWidget);
    expect(find.textContaining('Sinal -58 dBm · bateria 100%'), findsOneWidget);

    // Tocar na pulseira preenche o identificador para o cadastro.
    await tester.tap(find.text('Pulseira 01020304'));
    await tester.pump();
    expect(find.widgetWithText(TextField, '01020304'), findsOneWidget);
  });

  testWidgets('pulseira cadastrada: o heartbeat atualiza o Inicio e vira telemetria', (tester) async {
    const pulseira = Pulseira(
      id: 'p1',
      bleId: '01020304',
      nome: 'Pulseira de teste',
      ownerId: 'outra-pessoa',
      ativa: true,
    );
    final container = await abrir(tester, const InicioTela(), const [pulseira]);
    await tester.pump();
    expect(find.textContaining('Ainda não ouvida'), findsOneWidget);

    radio.ouvir(heartbeatDoFirmware);
    await tester.pump();

    expect(find.textContaining('Bateria 100% · Vista agora há pouco'), findsOneWidget);
    final lotes = container.read(agregadorProvider).retirarLotes();
    expect(lotes['01020304']!.single.bateriaPct, 100);
    // Heartbeat nunca alarma.
    expect(container.read(receptorProvider).alarmes, isEmpty);
  });

  testWidgets('anuncio de outro aparelho com o mesmo Company ID e ignorado', (tester) async {
    await abrir(tester, const NovaPulseiraTela(), const []);

    radio.ouvir([0x4c, 0x00, 0x02, 0x15]);
    await tester.pump();

    expect(find.textContaining('Pulseira '), findsNothing);
  });
}
