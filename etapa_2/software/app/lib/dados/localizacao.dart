import 'package:geolocator/geolocator.dart';

class Posicao {
  const Posicao({required this.latitude, required this.longitude, required this.precisaoM});

  final double latitude;
  final double longitude;
  final double precisaoM;
}

/// Localizacao do evento, em dois tempos (Secao 5.4 da arquitetura): a
/// posicao recente vai no primeiro envio, sem esperar o GPS; a atual, se
/// precisar, vai num segundo envio que a API funde ao primeiro.
class Localizacao {
  /// Ultima posicao conhecida, se tiver menos de [idadeMaxima]. Instantanea.
  Future<Posicao?> recente({Duration idadeMaxima = const Duration(minutes: 2)}) async {
    try {
      final p = await Geolocator.getLastKnownPosition();
      if (p == null || DateTime.now().difference(p.timestamp) > idadeMaxima) return null;
      return _converter(p);
    } catch (_) {
      return null;
    }
  }

  /// Posicao do GPS agora. Pode levar segundos; nunca atrasa o alarme.
  Future<Posicao?> atual() async {
    try {
      if (!await Geolocator.isLocationServiceEnabled()) return null;
      final permissao = await Geolocator.checkPermission();
      if (permissao == LocationPermission.denied || permissao == LocationPermission.deniedForever) {
        return null;
      }
      final p = await Geolocator.getCurrentPosition(
        locationSettings: const LocationSettings(
          accuracy: LocationAccuracy.high,
          timeLimit: Duration(seconds: 20),
        ),
      );
      return _converter(p);
    } catch (_) {
      return null;
    }
  }

  Future<bool> disponivel() async {
    try {
      if (!await Geolocator.isLocationServiceEnabled()) return false;
      final permissao = await Geolocator.checkPermission();
      return permissao == LocationPermission.always || permissao == LocationPermission.whileInUse;
    } catch (_) {
      return false;
    }
  }

  static Posicao _converter(Position p) =>
      Posicao(latitude: p.latitude, longitude: p.longitude, precisaoM: p.accuracy);
}
