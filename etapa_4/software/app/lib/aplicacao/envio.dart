import 'dart:math';

import '../dados/api.dart';
import '../dados/fila_envio.dart';
import '../dominio/modelos.dart';

/// O que aconteceu com um item da fila. Tabela da Secao 5.4 da arquitetura.
enum Desfecho {
  /// 201: alerta criado e responsaveis notificados.
  enviado,

  /// 201 duplicado: outro celular ja tinha reportado.
  duplicado,

  /// 401 sem WWW-Authenticate: assinatura da pulseira invalida. Rebaixa o alarme.
  rejeitadoAssinatura,

  /// 409: seq antigo, retransmissao de um anuncio gravado. Rebaixa o alarme.
  retransmissao,

  /// 404 ou 422: pulseira nao cadastrada ou pacote invalido. So descarta.
  descartado,

  /// 401 com WWW-Authenticate: sessao vencida. Fica na fila ate novo login.
  sessaoExpirada,

  /// Sem resposta ou 5xx. Fica na fila; o alarme local continua.
  semConexao,
}

class ResultadoItem {
  const ResultadoItem(this.desfecho, {this.envio, this.detalhe});

  final Desfecho desfecho;
  final ResultadoEnvio? envio;
  final String? detalhe;

  /// Resposta definitiva de que o anuncio nao era legitimo.
  bool get rebaixaAlarme =>
      desfecho == Desfecho.rejeitadoAssinatura || desfecho == Desfecho.retransmissao;
}

/// Esvazia a fila de envio, um item por vez, na ordem de chegada.
class EnvioAlertas {
  EnvioAlertas({
    required this.fila,
    required this.enviar,
    this.aoResultado,
    DateTime Function()? agora,
  }) : _agora = agora ?? DateTime.now;

  final FilaEnvio fila;
  final Future<ResultadoEnvio> Function(ItemFila) enviar;
  final void Function(ItemFila item, ResultadoItem resultado)? aoResultado;
  final DateTime Function() _agora;
  bool _drenando = false;

  /// Espera antes de tentar de novo, crescente e com teto de 5 min.
  static Duration espera(int tentativas) {
    const passos = [5, 15, 60, 300];
    return Duration(seconds: passos[min(max(tentativas, 1), passos.length) - 1]);
  }

  Future<void> drenar() async {
    if (_drenando) return;
    _drenando = true;
    try {
      for (final item in await fila.pendentes(_agora())) {
        final resultado = await _tentar(item);
        aoResultado?.call(item, resultado);
        // Sem rede ou sem sessao, os proximos itens falhariam do mesmo jeito.
        if (resultado.desfecho == Desfecho.semConexao ||
            resultado.desfecho == Desfecho.sessaoExpirada) {
          break;
        }
      }
    } finally {
      _drenando = false;
    }
  }

  Future<ResultadoItem> _tentar(ItemFila item) async {
    try {
      final r = await enviar(item);
      await fila.remover(item.id!);
      return ResultadoItem(r.duplicado ? Desfecho.duplicado : Desfecho.enviado, envio: r);
    } on ErroApi catch (e) {
      if (e.semConexao || (e.status ?? 0) >= 500) {
        await fila.adiar(item, _agora().add(espera(item.tentativas + 1)));
        return ResultadoItem(Desfecho.semConexao, detalhe: e.mensagem);
      }
      if (e.sessaoInvalida) {
        // Sem contar tentativa: o problema e a sessao, nao o evento.
        await fila.adiar(item.copiar(tentativas: item.tentativas - 1), _agora());
        return ResultadoItem(Desfecho.sessaoExpirada, detalhe: e.mensagem);
      }
      await fila.remover(item.id!);
      final desfecho = switch (e.status) {
        401 => Desfecho.rejeitadoAssinatura,
        409 => Desfecho.retransmissao,
        _ => Desfecho.descartado,
      };
      return ResultadoItem(desfecho, detalhe: e.mensagem);
    }
  }
}
