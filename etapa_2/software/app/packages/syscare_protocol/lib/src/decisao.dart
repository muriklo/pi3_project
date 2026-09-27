import 'anuncio.dart';
import 'deduplicador.dart';

enum Acao {
  /// Sirene e tela cheia, depois envio a API.
  alarmarEEnviar,

  /// Notificacao comum, sem sirene, e envio a API.
  notificarEEnviar,

  /// Pulseira de outra conta: repassa a API sem alarmar ninguem aqui.
  apenasEnviar,

  /// Heartbeat: alimenta a telemetria, nunca alarma.
  telemetria,

  /// Repeticao de um evento ja tratado, ou nada a fazer.
  ignorar,
}

/// O que o receptor faz com um anuncio valido (Secao 4.2 da arquitetura).
///
/// [pulseirasDaConta] tem os `ble_id` em hex minusculo, como [Anuncio.bleId].
Acao decidir(
  Anuncio anuncio,
  Deduplicador deduplicador,
  Set<String> pulseirasDaConta,
) {
  final daConta = pulseirasDaConta.contains(anuncio.bleId);
  final evento = anuncio.evento;

  // O heartbeat repete o seq do ultimo evento: nao passa pelo deduplicador e
  // nunca alarma, senao um app recem-aberto tocaria por uma queda antiga.
  if (evento == TipoEvento.heartbeat) {
    return daConta ? Acao.telemetria : Acao.ignorar;
  }
  // De pulseira alheia so interessa emergencia, e so para repassar.
  if (!daConta && !evento.ehEmergencia) return Acao.ignorar;
  if (!deduplicador.ehNovo(anuncio)) return Acao.ignorar;

  if (!daConta) return Acao.apenasEnviar;
  return evento.ehEmergencia ? Acao.alarmarEEnviar : Acao.notificarEEnviar;
}
