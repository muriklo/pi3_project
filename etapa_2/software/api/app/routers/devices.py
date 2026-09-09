"""Cadastro de pulseiras e dos cuidadores vinculados a cada uma."""
from __future__ import annotations

from fastapi import APIRouter, HTTPException, Response, status
from sqlalchemy import select

from app.deps import CurrentUser, DbSession, OwnedDevice
from app.models import Caregiver, Device, User
from app.schemas import (
    CaregiverCreate,
    CaregiverOut,
    CaregiverUpdate,
    DeviceCreate,
    DeviceCreated,
    DeviceOut,
    DeviceUpdate,
)
from app.security import new_device_secret

router = APIRouter(prefix="/v1/devices", tags=["pulseiras"])


@router.post(
    "",
    response_model=DeviceCreated,
    status_code=status.HTTP_201_CREATED,
    summary="Cadastrar pulseira (retorna a chave para gravar no firmware)",
)
def create_device(body: DeviceCreate, db: DbSession, user: CurrentUser) -> Device:
    if db.scalar(select(Device).where(Device.ble_id == body.ble_id)) is not None:
        raise HTTPException(status.HTTP_409_CONFLICT, "ble_id ja cadastrado.")

    device = Device(
        ble_id=body.ble_id,
        name=body.name,
        wearer_name=body.wearer_name,
        owner_id=user.id,
        shared_secret=new_device_secret(),
    )
    db.add(device)
    db.commit()
    db.refresh(device)
    # shared_secret so aparece aqui. Depois disso, so regravando a pulseira.
    return device


@router.get("", response_model=list[DeviceOut])
def list_devices(db: DbSession, user: CurrentUser) -> list[Device]:
    return list(
        db.scalars(select(Device).where(Device.owner_id == user.id).order_by(Device.name))
    )


@router.get("/{device_id}", response_model=DeviceOut)
def get_device(device: OwnedDevice) -> Device:
    return device


@router.patch("/{device_id}", response_model=DeviceOut)
def update_device(body: DeviceUpdate, device: OwnedDevice, db: DbSession) -> Device:
    for field, value in body.model_dump(exclude_unset=True).items():
        setattr(device, field, value)
    db.commit()
    db.refresh(device)
    return device


# --------------------------------------------------------------------------- #
# Cuidadores
# --------------------------------------------------------------------------- #
@router.post(
    "/{device_id}/caregivers",
    response_model=CaregiverOut,
    status_code=status.HTTP_201_CREATED,
    summary="Vincular um cuidador a pulseira",
)
def add_caregiver(
    body: CaregiverCreate, device: OwnedDevice, db: DbSession
) -> Caregiver:
    if body.user_id and db.get(User, body.user_id) is None:
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, "user_id inexistente.")
    if not body.user_id and not body.phone:
        raise HTTPException(
            status.HTTP_422_UNPROCESSABLE_ENTITY,
            "Informe user_id (push) ou phone: sem um deles o cuidador e inalcancavel.",
        )

    caregiver = Caregiver(device_id=device.id, **body.model_dump())
    db.add(caregiver)
    db.commit()
    db.refresh(caregiver)
    return caregiver


@router.get("/{device_id}/caregivers", response_model=list[CaregiverOut])
def list_caregivers(device: OwnedDevice, db: DbSession) -> list[Caregiver]:
    return list(
        db.scalars(
            select(Caregiver)
            .where(Caregiver.device_id == device.id)
            .order_by(Caregiver.priority, Caregiver.name)
        )
    )


@router.patch("/{device_id}/caregivers/{caregiver_id}", response_model=CaregiverOut)
def update_caregiver(
    caregiver_id: str, body: CaregiverUpdate, device: OwnedDevice, db: DbSession
) -> Caregiver:
    caregiver = db.get(Caregiver, caregiver_id)
    if caregiver is None or caregiver.device_id != device.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Cuidador nao encontrado.")
    for field, value in body.model_dump(exclude_unset=True).items():
        setattr(caregiver, field, value)
    db.commit()
    db.refresh(caregiver)
    return caregiver


@router.delete(
    "/{device_id}/caregivers/{caregiver_id}",
    status_code=status.HTTP_204_NO_CONTENT,
    response_class=Response,
    response_model=None,
)
def delete_caregiver(caregiver_id: str, device: OwnedDevice, db: DbSession) -> None:
    caregiver = db.get(Caregiver, caregiver_id)
    if caregiver is None or caregiver.device_id != device.id:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "Cuidador nao encontrado.")
    db.delete(caregiver)
    db.commit()
