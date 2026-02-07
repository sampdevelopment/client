#include <windows.h>
#include <math.h>
#include <stdio.h>

#include "../main.h"
#include "util.h"
#include "keystuff.h"

static DWORD dwLastCreatedVehicleID = 0;

CVehicle::CVehicle(int iType, float fPosX, float fPosY,
				   float fPosZ, float fRotation, PCHAR szNumberPlate)
{
	DWORD dwRetID = 0;
	m_pVehicle = nullptr;
	m_dwGTAId = 0;
	m_pTrailer = nullptr;

	if (iType != TRAIN_PASSENGER_LOCO && iType != TRAIN_FREIGHT_LOCO &&
		iType != TRAIN_PASSENGER && iType != TRAIN_FREIGHT &&
		iType != TRAIN_TRAM)
	{
		if (!CGame::IsModelLoaded(iType))
		{
			CGame::RequestModel(iType);
			CGame::LoadRequestedModels();
			while (!CGame::IsModelLoaded(iType)) Sleep(5);
		}

		if (szNumberPlate && szNumberPlate[0])
			ScriptCommand(&set_car_numberplate, iType, szNumberPlate);

		ScriptCommand(&create_car, iType, fPosX, fPosY, fPosZ, &dwRetID);
		ScriptCommand(&set_car_z_angle, dwRetID, fRotation);
		ScriptCommand(&car_gas_tank_explosion, dwRetID, 0);
		ScriptCommand(&set_car_hydraulics, dwRetID, 0);
		ScriptCommand(&toggle_car_tires_vulnerable, dwRetID, 0);

		m_pVehicle = GamePool_Vehicle_GetAt(dwRetID);
		m_pEntity = (ENTITY_TYPE*)m_pVehicle;
		m_dwGTAId = dwRetID;
		dwLastCreatedVehicleID = dwRetID;
		m_pVehicle->dwDoorsLocked = 0;

		Remove();
	}
	else if (iType == TRAIN_PASSENGER_LOCO || iType == TRAIN_FREIGHT_LOCO || iType == TRAIN_TRAM)
	{
		CGame::RequestModel(iType);
		CGame::LoadRequestedModels();
		while (!CGame::IsModelLoaded(iType)) Sleep(1);

		if (iType == TRAIN_PASSENGER_LOCO) iType = 10;
		else if (iType == TRAIN_FREIGHT_LOCO) iType = 15;
		else if (iType == TRAIN_TRAM) iType = 9;

		ScriptCommand(&create_train, iType, fPosX, fPosY, fPosZ, fRotation != 0.0f, &dwRetID);

		m_pVehicle = GamePool_Vehicle_GetAt(dwRetID);
		m_pEntity = (ENTITY_TYPE*)m_pVehicle;
		m_dwGTAId = dwRetID;
		dwLastCreatedVehicleID = dwRetID;

		GamePrepareTrain(m_pVehicle);
	}
	else if (iType == TRAIN_PASSENGER || iType == TRAIN_FREIGHT)
	{
		dwRetID = (((dwLastCreatedVehicleID >> 8) + 1) << 8) + 1;
		m_pVehicle = GamePool_Vehicle_GetAt(dwRetID);
		m_pEntity = (ENTITY_TYPE*)m_pVehicle;
		m_dwGTAId = dwRetID;
		dwLastCreatedVehicleID = dwRetID;
	}

	m_bIsInvulnerable = FALSE;
	m_byteObjectiveVehicle = 0;
	m_bSpecialMarkerEnabled = FALSE;
	m_dwMarkerID = 0;
	m_bHasBeenDriven = FALSE;
	m_dwTimeSinceLastDriven = GetTickCount();
	m_bDoorsLocked = FALSE;
}

CVehicle::~CVehicle()
{
	m_pVehicle = GamePool_Vehicle_GetAt(m_dwGTAId);
	if (m_pVehicle)
	{
		if (m_dwMarkerID)
		{
			ScriptCommand(&disable_marker, m_dwMarkerID);
			m_dwMarkerID = 0;
		}
		RemoveEveryoneFromVehicle();
		ScriptCommand(&destroy_car, m_dwGTAId);
	}
}

void CVehicle::Add()
{
	if (!IsAdded())
	{
		CEntity::Add();
		CVehicle* pTrailer = GetTrailer();
		if (pTrailer) pTrailer->Add();
	}
}

void CVehicle::Remove()
{
	if (IsAdded())
	{
		CVehicle* pTrailer = GetTrailer();
		if (pTrailer) pTrailer->Remove();
		CEntity::Remove();
	}
}

void CVehicle::ProcessEngineAudio(BYTE byteDriverID)
{
	DWORD dwVehicle = (DWORD)m_pVehicle;

	if (byteDriverID)
	{
		GameStoreLocalPlayerKeys();
		GameSetRemotePlayerKeys(byteDriverID);
	}

	if (m_pVehicle && IsAdded())
	{
		_asm mov esi, dwVehicle
		_asm lea ecx, [esi + 312]
		_asm mov edx, 0x502280
		_asm call edx
	}

	if (byteDriverID)
		GameSetLocalPlayerKeys();
}

void CVehicle::LinkToInterior(int iInterior)
{
	if (GamePool_Vehicle_GetAt(m_dwGTAId))
		ScriptCommand(&link_vehicle_to_interior, m_dwGTAId, iInterior);
}

bool CVehicle::IsPrimaryPedInVehicle()
{
	PED_TYPE* pPed;
	if (!m_pVehicle || !GamePool_Vehicle_GetAt(m_dwGTAId)) return false;
	pPed = m_pVehicle->pDriver;
	return pPed && IN_VEHICLE(pPed) && !pPed->dwPedType;
}

void CVehicle::ResetPointers()
{
	m_pVehicle = GamePool_Vehicle_GetAt(m_dwGTAId);
	m_pEntity = (ENTITY_TYPE*)m_pVehicle;
}

void CVehicle::Recreate()
{
	if (!m_pVehicle) return;

	UINT uiType = GetModelIndex();
	MATRIX4X4 mat;
	BYTE byteColor1 = m_pVehicle->byteColor1;
	BYTE byteColor2 = m_pVehicle->byteColor2;
	GetMatrix(&mat);

	ScriptCommand(&destroy_car, m_dwGTAId);

	if (!CGame::IsModelLoaded(uiType))
	{
		CGame::RequestModel(uiType);
		CGame::LoadRequestedModels();
		while (!CGame::IsModelLoaded(uiType)) Sleep(5);
	}

	DWORD dwRetID = 0;
	ScriptCommand(&create_car, uiType, mat.pos.X, mat.pos.Y, mat.pos.Z, &dwRetID);
	ScriptCommand(&car_gas_tank_explosion, dwRetID, 0);

	m_pVehicle = GamePool_Vehicle_GetAt(dwRetID);
	m_pEntity = (ENTITY_TYPE*)m_pVehicle;
	m_dwGTAId = dwRetID;
	dwLastCreatedVehicleID = dwRetID;
	m_pVehicle->dwDoorsLocked = 0;

	SetMatrix(mat);
	SetColor(byteColor1, byteColor2);
}

BOOL CVehicle::IsOccupied()
{
	if (!m_pVehicle) return FALSE;
	if (m_pVehicle->pDriver) return TRUE;
	for (int i = 0; i < 7; i++)
		if (m_pVehicle->pPassengers[i]) return TRUE;
	return FALSE;
}

void CVehicle::SetInvulnerable(BOOL bInv)
{
	if (!m_pVehicle || !GamePool_Vehicle_GetAt(m_dwGTAId)) return;
	if (bInv)
	{
		ScriptCommand(&set_car_immunities, m_dwGTAId, 1, 1, 1, 1, 1);
		ScriptCommand(&toggle_car_tires_vulnerable, m_dwGTAId, 0);
		m_bIsInvulnerable = TRUE;
	}
	else
	{
		ScriptCommand(&set_car_immunities, m_dwGTAId, 0, 0, 0, 0, 0);
		if (pNetGame && pNetGame->m_bTirePopping)
			ScriptCommand(&toggle_car_tires_vulnerable, m_dwGTAId, 1);
		m_bIsInvulnerable = FALSE;
	}
}

void CVehicle::SetLockedState(int iLocked)
{
	if (!m_pVehicle) return;
	ScriptCommand(&lock_car, m_dwGTAId, iLocked ? 1 : 0);
}

void CVehicle::SetEngineState(BOOL bState)
{
	if (!m_pVehicle) return;
	if (bState) m_pVehicle->byteFlags |= 0x10;
	else m_pVehicle->byteFlags &= 0xEF;
}

float CVehicle::GetHealth()
{
	return m_pVehicle ? m_pVehicle->fHealth : 0.0f;
}

void CVehicle::SetHealth(float fHealth)
{
	if (m_pVehicle) m_pVehicle->fHealth = fHealth;
}

void CVehicle::SetColor(int iColor1, int iColor2)
{
	if (m_pVehicle) ScriptCommand(&set_car_color, m_dwGTAId, iColor1, iColor2);
}

UINT CVehicle::GetVehicleSubtype()
{
	if (!m_pVehicle) return 0;
	switch (m_pVehicle->entity.vtable)
	{
	case 0x871120: return VEHICLE_SUBTYPE_CAR;
	case 0x8721A0: return VEHICLE_SUBTYPE_BOAT;
	case 0x871360: return VEHICLE_SUBTYPE_BIKE;
	case 0x871948: return VEHICLE_SUBTYPE_PLANE;
	case 0x871680: return VEHICLE_SUBTYPE_HELI;
	case 0x871528: return VEHICLE_SUBTYPE_PUSHBIKE;
	case 0x872370: return VEHICLE_SUBTYPE_TRAIN;
	}
	return 0;
}

BOOL CVehicle::HasSunk()
{
	if (!m_pVehicle) return FALSE;
	return ScriptCommand(&has_car_sunk, m_dwGTAId);
}

BOOL CVehicle::IsWrecked()
{
	if (!m_pVehicle) return FALSE;
	return ScriptCommand(&is_car_wrecked, m_dwGTAId);
}

BOOL CVehicle::IsDriverLocalPlayer()
{
	return m_pVehicle && (PED_TYPE*)m_pVehicle->pDriver == GamePool_FindPlayerPed();
}

bool CVehicle::IsVehicleMatchesPedVehicle()
{
	PED_TYPE* pPed = GamePool_FindPlayerPed();
	return m_pVehicle && pPed && IN_VEHICLE(pPed) && (DWORD)m_pVehicle == pPed->pVehicle;
}

BOOL CVehicle::IsATrainPart()
{
	if (!m_pVehicle) return FALSE;
	int nModel = m_pVehicle->entity.nModelIndex;
	return nModel == TRAIN_PASSENGER_LOCO || nModel == TRAIN_PASSENGER ||
		   nModel == TRAIN_FREIGHT_LOCO || nModel == TRAIN_FREIGHT ||
		   nModel == TRAIN_TRAM;
}

BOOL CVehicle::HasTurret()
{
	int nModel = GetModelIndex();
	return nModel == 432 || nModel == 564 || nModel == 407 || nModel == 601;
}

void CVehicle::SetSirenOn(BOOL state)
{
	if (m_pVehicle) m_pVehicle->bSirenOn = state;
}

BOOL CVehicle::IsSirenOn()
{
	return m_pVehicle && m_pVehicle->bSirenOn == 1;
}

void CVehicle::SetLandingGearState(eLandingGearState state)
{
	if (!m_pVehicle) return;
	if (state == LGS_UP) m_pVehicle->fPlaneLandingGear = 0.0f;
	else if (state == LGS_DOWN) m_pVehicle->fPlaneLandingGear = 1.0f;
}

eLandingGearState CVehicle::GetLandingGearState()
{
	if (!m_pVehicle) return LGS_CHANGING;
	if (m_pVehicle->fPlaneLandingGear == 0.0f) return LGS_UP;
	if (m_pVehicle->fPlaneLandingGear == 1.0f) return LGS_DOWN;
	return LGS_CHANGING;
}

bool CVehicle::IsLandingGearNotUp()
{
	return m_pVehicle && GetVehicleSubtype() == VEHICLE_SUBTYPE_PLANE && m_pVehicle->fPlaneLandingGear != 0.0f;
}

void CVehicle::SetLandingGearState(bool bUpState)
{
	if (!m_pVehicle || GetVehicleSubtype() != VEHICLE_SUBTYPE_PLANE) return;
	DWORD dwThis = (DWORD)m_pVehicle;
	if (bUpState && m_pVehicle->fPlaneLandingGear == 0.0f) _asm { mov ecx, dwThis; mov edx, 0x6CAC20; call edx; }
	else if (!bUpState && m_pVehicle->fPlaneLandingGear == 1.0f) _asm { mov ecx, dwThis; mov edx, 0x6CAC70; call edx; }
}

UINT CVehicle::GetPassengersMax() { return 0; }

void CVehicle::SetHydraThrusters(DWORD dwDirection) { if (m_pVehicle) m_pVehicle->dwHydraThrusters = dwDirection; }
DWORD CVehicle::GetHydraThrusters() { return m_pVehicle ? m_pVehicle->dwHydraThrusters : 0; }

void CVehicle::SetTrailer(CVehicle* pTrailer)
{
	m_pTrailer = pTrailer;
	if (m_pTrailer)
		m_pTrailer->m_pVehicle->pTrailer = m_pVehicle;
}

CVehicle* CVehicle::GetTrailer()
{
	return m_pTrailer;
}

void CVehicle::RemoveEveryoneFromVehicle()
{
	if (!m_pVehicle) return;
	PED_TYPE* pDriver = m_pVehicle->pDriver;
	if (pDriver) ScriptCommand(&task_leave_car, (DWORD)pDriver, m_dwGTAId, 0);
	for (int i = 0; i < 7; i++)
	{
		PED_TYPE* pPassenger = m_pVehicle->pPassengers[i];
		if (pPassenger) ScriptCommand(&task_leave_car, (DWORD)pPassenger, m_dwGTAId, 0);
	}
}

void CVehicle::SetSpecialMarker(BOOL bEnabled)
{
	if (!m_pVehicle) return;
	if (bEnabled && !m_bSpecialMarkerEnabled)
	{
		ScriptCommand(&create_marker_on_car, m_dwGTAId, &m_dwMarkerID);
		m_bSpecialMarkerEnabled = TRUE;
	}
	else if (!bEnabled && m_bSpecialMarkerEnabled)
	{
		ScriptCommand(&disable_marker, m_dwMarkerID);
		m_dwMarkerID = 0;
		m_bSpecialMarkerEnabled = FALSE;
	}
}

BOOL CVehicle::IsSpecialMarkerEnabled()
{
	return m_bSpecialMarkerEnabled;
}

void CVehicle::UpdateDoorsLockStatus()
{
	if (!m_pVehicle) return;
	m_bDoorsLocked = m_pVehicle->dwDoorsLocked ? TRUE : FALSE;
}

BOOL CVehicle::AreDoorsLocked()
{
	UpdateDoorsLockStatus();
	return m_bDoorsLocked;
}

void CVehicle::SetObjectiveVehicle(BYTE byteObjective)
{
	m_byteObjectiveVehicle = byteObjective;
}

BYTE CVehicle::GetObjectiveVehicle()
{
	return m_byteObjectiveVehicle;
}

void CVehicle::EnableTirePopping(BOOL bEnabled)
{
	if (!m_pVehicle) return;
	ScriptCommand(&toggle_car_tires_vulnerable, m_dwGTAId, bEnabled ? 1 : 0);
}

void CVehicle::SetLandingGear(bool bUp)
{
	if (!m_pVehicle || GetVehicleSubtype() != VEHICLE_SUBTYPE_PLANE) return;
	m_pVehicle->fPlaneLandingGear = bUp ? 0.0f : 1.0f;
}

BOOL CVehicle::IsLandingGearUp()
{
	if (!m_pVehicle || GetVehicleSubtype() != VEHICLE_SUBTYPE_PLANE) return FALSE;
	return m_pVehicle->fPlaneLandingGear == 0.0f;
}

BOOL CVehicle::IsLandingGearDown()
{
	if (!m_pVehicle || GetVehicleSubtype() != VEHICLE_SUBTYPE_PLANE) return FALSE;
	return m_pVehicle->fPlaneLandingGear == 1.0f;
}

void CVehicle::UpdateTimeSinceLastDriven()
{
	if (!m_pVehicle) return;
	if (IsOccupied()) m_dwTimeSinceLastDriven = GetTickCount();
}

DWORD CVehicle::GetTimeSinceLastDriven()
{
	UpdateTimeSinceLastDriven();
	return GetTickCount() - m_dwTimeSinceLastDriven;
}

void CVehicle::SetVehicleInvulnerable(BOOL bInv)
{
	SetInvulnerable(bInv);
}

BOOL CVehicle::IsVehicleInvulnerable()
{
	return m_bIsInvulnerable;
}

UINT CVehicle::GetModelIndex()
{
	return m_pVehicle ? m_pVehicle->entity.nModelIndex : 0;
}

void CVehicle::SetColorRGB(int iColor1, int iColor2)
{
	SetColor(iColor1, iColor2);
}

void CVehicle::ForceEngineOn()
{
	if (!m_pVehicle) return;
	m_pVehicle->byteFlags |= 0x10;
}

void CVehicle::ForceEngineOff()
{
	if (!m_pVehicle) return;
	m_pVehicle->byteFlags &= 0xEF;
}

BOOL CVehicle::IsEngineOn()
{
	return m_pVehicle ? (m_pVehicle->byteFlags & 0x10) != 0 : FALSE;
}

BOOL CVehicle::IsOccupiedByAnyPlayer()
{
	if (!m_pVehicle) return FALSE;
	PED_TYPE* pDriver = m_pVehicle->pDriver;
	if (pDriver && pDriver->pPlayerData) return TRUE;
	for (int i = 0; i < 7; i++)
	{
		PED_TYPE* pPassenger = m_pVehicle->pPassengers[i];
		if (pPassenger && pPassenger->pPlayerData) return TRUE;
	}
	return FALSE;
}

void CVehicle::SetHydraThrustersDirection(DWORD dwDir)
{
	SetHydraThrusters(dwDir);
}

DWORD CVehicle::GetHydraThrustersDirection()
{
	return GetHydraThrusters();
}

BOOL CVehicle::IsPlane()
{
	UINT type = GetVehicleSubtype();
	return type == VEHICLE_SUBTYPE_PLANE || type == VEHICLE_SUBTYPE_HELI;
}

BOOL CVehicle::IsBoat()
{
	return GetVehicleSubtype() == VEHICLE_SUBTYPE_BOAT;
}

BOOL CVehicle::IsCar()
{
	return GetVehicleSubtype() == VEHICLE_SUBTYPE_CAR;
}

BOOL CVehicle::IsBike()
{
	UINT type = GetVehicleSubtype();
	return type == VEHICLE_SUBTYPE_BIKE || type == VEHICLE_SUBTYPE_PUSHBIKE;
}

BOOL CVehicle::IsTrain()
{
	return GetVehicleSubtype() == VEHICLE_SUBTYPE_TRAIN;
}
