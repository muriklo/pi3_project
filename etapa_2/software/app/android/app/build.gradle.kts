import groovy.json.JsonSlurper

plugins {
    id("com.android.application")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

/**
 * Push (FCM) sem o plugin google-services: le o google-services.json, se
 * existir, e gera os mesmos quatro recursos que o plugin geraria; o Firebase
 * se inicializa a partir deles. Sem o arquivo o app compila e roda igual, so
 * sem push. O arquivo nao vai para o git (.gitignore).
 */
fun recursosDoFirebase(arquivo: File, pacote: String): Map<String, String> {
    if (!arquivo.exists()) return emptyMap()
    val json = JsonSlurper().parse(arquivo) as Map<*, *>
    val projeto = json["project_info"] as Map<*, *>
    val cliente = (json["client"] as List<*>).map { it as Map<*, *> }.firstOrNull {
        val info = it["client_info"] as Map<*, *>
        (info["android_client_info"] as Map<*, *>)["package_name"] == pacote
    } ?: throw GradleException(
        "google-services.json nao tem o app $pacote. No console do Firebase, " +
            "registre um app Android com esse nome de pacote e baixe o arquivo de novo."
    )
    return mapOf(
        "google_app_id" to (cliente["client_info"] as Map<*, *>)["mobilesdk_app_id"] as String,
        "google_api_key" to ((cliente["api_key"] as List<*>)[0] as Map<*, *>)["current_key"] as String,
        "gcm_defaultSenderId" to projeto["project_number"] as String,
        "project_id" to projeto["project_id"] as String,
    )
}

android {
    namespace = "br.edu.ifsc.syscare_app"
    compileSdk = flutter.compileSdkVersion
    // O Flutter 3.47 fixa o NDK 28.2.13676358. Usamos o 30, que e o que o
    // SDK Manager do Android Studio instala por padrao: nenhum plugin do app
    // tem codigo C/C++, e o NDK so e usado para enxugar as .so do APK.
    ndkVersion = "30.0.16248370"

    compileOptions {
        // Exigido pelo flutter_local_notifications (alarme local).
        isCoreLibraryDesugaringEnabled = true
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "br.edu.ifsc.syscare_app"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        // Uses the version code from pubspec.yaml. When using split APKs, 1000 * ABI_VERSION
        // is added automatically by Flutter. (https://developer.android.com/studio/build/configure-apk-splits#configure-APK-versions)
        // You can force using the value of versionCode by specifying the `-P force-version-code-ignoring-abi=true`
        // flag during build.
        versionCode = flutter.versionCode
        versionName = flutter.versionName

        for ((nome, valor) in recursosDoFirebase(file("google-services.json"), applicationId!!)) {
            resValue("string", nome, valor)
        }
    }

    buildFeatures {
        resValues = true
    }

    buildTypes {
        release {
            // TODO: Add your own signing config for the release build.
            // Signing with the debug keys for now, so `flutter run --release` works.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

flutter {
    source = "../.."
}

dependencies {
    coreLibraryDesugaring("com.android.tools:desugar_jdk_libs:2.1.4")
}
