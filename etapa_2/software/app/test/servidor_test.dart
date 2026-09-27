import 'package:flutter_test/flutter_test.dart';
import 'package:syscare_app/config.dart';

void main() {
  test('so o IP vira http com a porta padrao da API', () {
    expect(normalizarServidor('192.168.1.20'), 'http://192.168.1.20:8000');
    expect(normalizarServidor('  192.168.1.20/ '), 'http://192.168.1.20:8000');
  });

  test('porta e esquema informados sao mantidos', () {
    expect(normalizarServidor('http://192.168.1.20:9000'), 'http://192.168.1.20:9000');
    expect(normalizarServidor('https://api.syscare.exemplo:443/'), 'https://api.syscare.exemplo:443');
  });

  test('campo vazio volta ao padrao do build', () {
    expect(normalizarServidor(''), urlApi);
  });
}
