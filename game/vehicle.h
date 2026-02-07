#pragma once

#include "game.h"
#include "entity.h"

enum eLandingGearState 
{
    LGS_CHANGING = 0,
    LGS_UP       = 1,
    LGS_DOWN     = 2,
};

class CVehicle : public CEntity
{
public:
    CVehicle(int iType, float fPosX, float fPosY, float fPosZ, float fRotation = 0.0f, PCHAR szNumberPlate = nullptr);
    virtual ~CVehicle();

    virtual void Add();
    virtual void Remove();

    void ResetPointers();
    void ProcessMarkers();

    void SetLockedState(int iLocked);
    UINT GetVehicleSubtype();

    float GetHealth() const;
    void SetHealth(float fHealth);
    void SetColor(int iColor1, int iColor2);

    BOOL HasSunk() const;
    BOOL IsWrecked() const;
    BOOL IsDriverLocalPlayer() const;
    bool IsVehicleMatchesPedVehicle() const;
    BOOL IsATrainPart() const;
    BOOL HasTurret() const;

    void SetHydraThrusters(DWORD dwDirection);
    DWORD GetHydraThrusters() const;

    void SetTankRot(float X, float Y);
    float GetTankRotX() const;
    float GetTankRotY() const;

    float GetTrainSpeed() const;
    void SetTrainSpeed(float fSpeed);

    void Explode();
    void Fix();
    void ClearLastWeaponDamage();
    UINT GetPassengersMax() const;

    void SetSirenOn(BOOL state);
    BOOL IsSirenOn() const;

    void SetLandingGearState(eLandingGearState state);
    eLandingGearState GetLandingGearState() const;
    bool IsLandingGearNotUp() const;
    void SetLandingGearState(bool bUpState);

    void SetInvulnerable(BOOL bInv);
    inline BOOL IsInvulnerable() const { return m_bIsInvulnerable; }

    void SetEngineState(BOOL bState);
    void SetDoorState(int iState);
    void LinkToInterior(int iInterior);
    bool IsPrimaryPedInVehicle() const;

    void SetWheelPopped(DWORD wheelid, DWORD popped);
    BYTE GetWheelPopped(DWORD wheelid) const;

    void AttachTrailer();
    void DetachTrailer();
    void SetTrailer(CVehicle* pTrailer);
    CVehicle* GetTrailer();

    BOOL IsRCVehicle() const;

    void UpdateDamage(int iPanels, int iDoors, unsigned char ucLights);
    int GetCarPanelsDamageStatus() const;
    int GetCarDoorsDamageStatus() const;
    unsigned char GetCarLightsDamageStatus() const;

    void SetCarOrBikeWheelStatus(unsigned char ucStatus);
    unsigned char GetCarOrBikeWheelStatus() const;

    BOOL IsOccupied() const;
    void Recreate();
    BOOL UpdateLastDrivenTime();
    void ProcessEngineAudio(BYTE byteDriverID);
    void RemoveEveryoneFromVehicle();
    void SetHornState(BYTE byteState);
    BOOL HasADriver() const;
    BOOL VerifyInstance() const;

    void ToggleWindow(unsigned char ucDoorId, bool bClosed);
    void ToggleTaxiLight(bool bToggle);
    void ToggleEngine(bool bToggle);
    bool IsUpsideDown() const;
    bool IsOnItsSide() const;
    void SetLightState(BOOL bState);
    void ToggleComponent(DWORD dwComp, float fAngle);
    void SetFeature(bool bToggle);
    void SetVisibility(bool bVisible);
    void ToggleDoor(int iDoor, int iNodeIndex, float fAngle);

    unsigned char GetNumOfPassengerSeats() const;

    VEHICLE_TYPE* m_pVehicle = nullptr;
    BOOL m_bIsInvulnerable = FALSE;
    BOOL m_bDoorsLocked = FALSE;
    BYTE m_byteObjectiveVehicle = 0;
    BOOL m_bSpecialMarkerEnabled = FALSE;
    DWORD m_dwTimeSinceLastDriven = 0;
    BOOL m_bHasBeenDriven = FALSE;
    CVehicle* m_pTrailer = nullptr;
    int m_dwMarkerID = 0;
};
