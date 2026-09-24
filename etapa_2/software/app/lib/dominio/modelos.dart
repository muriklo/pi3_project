/// Entidades do app, espelhando os contratos da API (contratos/openapi.json).
library;

DateTime _data(String texto) {
  // O SQLite da API devolve datas sem fuso; todas sao UTC.
  final comFuso = texto.endsWith('Z') || RegExp(r'[+-]\d\d:\d\d$').hasMatch(texto);
  return DateTime.parse(comFuso ? texto : '${texto}Z').toLocal();
}

DateTime? _dataOuNula(Object? texto) => texto == null ? null : _data(texto as String);

double? _real(Object? v) => (v as num?)?.toDouble();

class Usuario {
  const Usuario({required this.id, required this.email, required this.nome});

  factory Usuario.deJson(Map<String, dynamic> j) =>
      Usuario(id: j['id'] as String, email: j['email'] as String, nome: j['name'] as String);

  final String id;
  final String email;
  final String nome;

  Map<String, dynamic> paraJson() => {'id': id, 'email': email, 'name': nome};
}

class Pulseira {
  const Pulseira({
    required this.id,
    required this.bleId,
    required this.nome,
    required this.ownerId,
    required this.ativa,
    this.nomeUsuario,
    this.bateriaPct,
    this.vistaEm,
  });

  factory Pulseira.deJson(Map<String, dynamic> j) => Pulseira(
        id: j['id'] as String,
        bleId: j['ble_id'] as String,
        nome: j['name'] as String,
        nomeUsuario: j['wearer_name'] as String?,
        ownerId: j['owner_id'] as String,
        bateriaPct: j['battery_pct'] as int?,
        vistaEm: _dataOuNula(j['last_seen_at']),
        ativa: j['active'] as bool,
      );

  final String id;
  final String bleId;
  final String nome;

  /// Pessoa que usa a pulseira ("wearer_name" na API).
  final String? nomeUsuario;

  /// Dono da pulseira. Quem nao e o dono so ve e atende alertas.
  final String ownerId;
  final int? bateriaPct;
  final DateTime? vistaEm;
  final bool ativa;

  String get titulo => nomeUsuario ?? nome;

  bool ehDono(String usuarioId) => ownerId == usuarioId;
}

class Responsavel {
  const Responsavel({
    required this.id,
    required this.nome,
    required this.prioridade,
    required this.ativo,
    this.telefone,
    this.email,
    this.usuarioId,
  });

  factory Responsavel.deJson(Map<String, dynamic> j) => Responsavel(
        id: j['id'] as String,
        nome: j['name'] as String,
        telefone: j['phone'] as String?,
        email: j['email'] as String?,
        usuarioId: j['user_id'] as String?,
        prioridade: j['priority'] as int,
        ativo: j['active'] as bool,
      );

  final String id;
  final String nome;
  final String? telefone;
  final String? email;

  /// Conta do app que recebe o push. Sem conta, o responsavel so recebe SMS.
  final String? usuarioId;
  final int prioridade;
  final bool ativo;
}

class Entrega {
  const Entrega({
    required this.canal,
    required this.destino,
    required this.status,
    required this.tentativa,
    this.erro,
  });

  factory Entrega.deJson(Map<String, dynamic> j) => Entrega(
        canal: j['channel'] as String,
        destino: j['target'] as String,
        status: j['status'] as String,
        erro: j['error'] as String?,
        tentativa: j['attempt'] as int,
      );

  final String canal;
  final String destino;
  final String status;
  final String? erro;
  final int tentativa;
}

class Alerta {
  const Alerta({
    required this.id,
    required this.deviceId,
    required this.tipo,
    required this.seq,
    required this.ocorridoEm,
    required this.status,
    required this.rodada,
    this.impactoG,
    this.bateriaPct,
    this.latitude,
    this.longitude,
    this.gateway,
    this.confirmadoPor,
    this.confirmadoEm,
    this.notas,
    this.entregas = const [],
  });

  factory Alerta.deJson(Map<String, dynamic> j) => Alerta(
        id: j['id'] as String,
        deviceId: j['device_id'] as String,
        tipo: j['event_type'] as String,
        seq: j['seq'] as int,
        ocorridoEm: _data(j['occurred_at'] as String),
        impactoG: _real(j['impact_g']),
        bateriaPct: j['battery_pct'] as int?,
        latitude: _real(j['latitude']),
        longitude: _real(j['longitude']),
        gateway: j['gateway_label'] as String?,
        status: j['status'] as String,
        confirmadoPor: j['acked_by_user_id'] as String?,
        confirmadoEm: _dataOuNula(j['acked_at']),
        notas: j['notes'] as String?,
        rodada: j['escalation_round'] as int,
        entregas: [
          for (final e in (j['deliveries'] as List? ?? const []))
            Entrega.deJson(e as Map<String, dynamic>),
        ],
      );

  final String id;
  final String deviceId;

  /// `event_type` da API: fall, panic, no_movement, low_battery, test...
  final String tipo;
  final int seq;
  final DateTime ocorridoEm;
  final double? impactoG;
  final int? bateriaPct;
  final double? latitude;
  final double? longitude;
  final String? gateway;

  /// open, acked, resolved ou false_positive.
  final String status;
  final String? confirmadoPor;
  final DateTime? confirmadoEm;
  final String? notas;
  final int rodada;
  final List<Entrega> entregas;

  bool get aberto => status == 'open';
  bool get encerrado => status == 'resolved' || status == 'false_positive';
}

/// Resposta do POST /v1/alerts.
class ResultadoEnvio {
  const ResultadoEnvio({required this.alerta, required this.duplicado, required this.notificados});

  factory ResultadoEnvio.deJson(Map<String, dynamic> j) => ResultadoEnvio(
        alerta: Alerta.deJson(j['alert'] as Map<String, dynamic>),
        duplicado: j['duplicate'] as bool,
        notificados: j['notified'] as int,
      );

  final Alerta alerta;

  /// Outro celular ja havia reportado o mesmo evento.
  final bool duplicado;
  final int notificados;
}

const Map<String, String> rotulosEvento = {
  'fall': 'Queda detectada',
  'panic': 'Botão de emergência',
  'no_movement': 'Imobilidade prolongada',
  'low_battery': 'Bateria baixa',
  'device_offline': 'Pulseira fora de alcance',
  'test': 'Teste da pulseira',
};

const Map<String, String> rotulosStatus = {
  'open': 'Aguardando alguém',
  'acked': 'Alguém está indo',
  'resolved': 'Atendido',
  'false_positive': 'Falso alarme',
};
