"""Configuracao central da API, lida de variaveis de ambiente ou do arquivo .env."""
from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=".env", env_prefix="SYSCARE_", extra="ignore"
    )

    env: str = "dev"
    database_url: str = "sqlite:///./syscare.db"

    jwt_secret: str = "dev-secret-nao-use-em-producao"
    jwt_algorithm: str = "HS256"
    jwt_expire_minutes: int = 60 * 24 * 7

    # Dois celulares podem ouvir o MESMO broadcast BLE e postar o mesmo alerta.
    dedup_window_seconds: int = 180
    max_event_skew_seconds: int = 900

    escalation_seconds: int = 60
    escalation_max_rounds: int = 3
    require_hmac: bool = False

    fcm_credentials_file: str = ""
    fcm_project_id: str = ""

    # SMS: "none" (desligado), "dryrun" (loga sem enviar) ou "twilio" (envio real).
    sms_provider: str = "none"
    twilio_account_sid: str = ""
    twilio_auth_token: str = ""
    twilio_from_number: str = ""
    # A partir de qual rodada de escalonamento o SMS entra. 0 = junto com o push.
    # Cuidador sem token de push recebe SMS ja na rodada 0, independente disto.
    sms_from_escalation_round: int = 1


@lru_cache
def get_settings() -> Settings:
    return Settings()
