package br.edu.ifsc.syscare_app

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.telephony.SmsManager
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

/**
 * Camada 3 da notificacao (etapa_2/software/app/README.md, Secao 5.7): SMS
 * enviado pelo proprio celular, no plano da operadora. Nao depende de provedor
 * pago nem de internet, so de sinal de celular.
 */
class MainActivity : FlutterActivity() {
    private var permissaoPendente: MethodChannel.Result? = null

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CANAL)
            .setMethodCallHandler { chamada, resultado ->
                when (chamada.method) {
                    "disponivel" -> resultado.success(temLinhaDeSms())
                    "temPermissao" -> resultado.success(temPermissao())
                    "pedirPermissao" -> pedirPermissao(resultado)
                    "enviar" -> enviar(
                        chamada.argument<String>("numero"),
                        chamada.argument<String>("texto"),
                        resultado,
                    )
                    else -> resultado.notImplemented()
                }
            }
    }

    private fun temLinhaDeSms(): Boolean {
        val recurso = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            PackageManager.FEATURE_TELEPHONY_MESSAGING
        } else {
            PackageManager.FEATURE_TELEPHONY
        }
        return packageManager.hasSystemFeature(recurso)
    }

    private fun temPermissao(): Boolean =
        checkSelfPermission(Manifest.permission.SEND_SMS) == PackageManager.PERMISSION_GRANTED

    private fun pedirPermissao(resultado: MethodChannel.Result) {
        if (temPermissao()) {
            resultado.success(true)
            return
        }
        if (permissaoPendente != null) {
            resultado.error("em_andamento", "Ja ha um pedido de permissao aberto.", null)
            return
        }
        permissaoPendente = resultado
        requestPermissions(arrayOf(Manifest.permission.SEND_SMS), CODIGO_PERMISSAO)
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray,
    ) {
        // O super repassa aos plugins (o universal_ble tambem pede permissoes).
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == CODIGO_PERMISSAO) {
            permissaoPendente?.success(grantResults.firstOrNull() == PackageManager.PERMISSION_GRANTED)
            permissaoPendente = null
        }
    }

    private fun enviar(numero: String?, texto: String?, resultado: MethodChannel.Result) {
        if (numero.isNullOrBlank() || texto.isNullOrEmpty()) {
            resultado.error("argumentos", "Numero e texto sao obrigatorios.", null)
            return
        }
        if (!temPermissao()) {
            resultado.error("sem_permissao", "Permissao SEND_SMS negada.", null)
            return
        }
        try {
            val sms: SmsManager? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                getSystemService(SmsManager::class.java)
            } else {
                @Suppress("DEPRECATION")
                SmsManager.getDefault()
            }
            if (sms == null) {
                resultado.error("indisponivel", "Este aparelho nao envia SMS.", null)
                return
            }
            // Texto longo vira varias partes, remontadas no celular de quem recebe.
            val partes = sms.divideMessage(texto)
            sms.sendMultipartTextMessage(numero, null, partes, null, null)
            resultado.success(partes.size)
        } catch (e: Exception) {
            resultado.error("falha", e.message, null)
        }
    }

    companion object {
        private const val CANAL = "br.edu.ifsc.syscare/sms"
        private const val CODIGO_PERMISSAO = 4001
    }
}
