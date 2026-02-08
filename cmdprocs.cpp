#include "main.h"
#include "game/util.h"
#include "game/task.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cerrno>

extern BYTE* pbyteCameraMode;

static CRemotePlayer* pTestPlayer;
static int iCurrentPlayerTest = 1;

extern float fFarClip;

extern unsigned long _sendtoUncompressedTotal;
extern unsigned long _sendtoCompressedTotal;
extern float fOnFootCorrectionMultiplier;
extern float fInCarCorrectionMultiplier;
extern float fInaccuracyFactorMultiplier;

static __forceinline bool HasChat() { return (pChatWindow != NULL); }
static __forceinline bool HasConfig() { return (pConfigFile != NULL); }
static __forceinline bool HasNetGame() { return (pNetGame != NULL); }
static __forceinline bool HasGame() { return (pGame != NULL); }

static void ChatInfo(const char* fmt, ...)
{
	if (!HasChat() || !fmt) return;
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	pChatWindow->AddInfoMessage(buf);
}

static void ChatDebug(const char* fmt, ...)
{
	if (!HasChat() || !fmt) return;
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	pChatWindow->AddDebugMessage("%s", buf);
}

struct FileGuard
{
	FILE* f;
	explicit FileGuard(FILE* fp = NULL) : f(fp) {}
	~FileGuard() { if (f) fclose(f); }
	FileGuard(const FileGuard&) = delete;
	FileGuard& operator=(const FileGuard&) = delete;
	FileGuard(FileGuard&& o) noexcept : f(o.f) { o.f = NULL; }
	FileGuard& operator=(FileGuard&& o) noexcept
	{
		if (this != &o)
		{
			if (f) fclose(f);
			f = o.f;
			o.f = NULL;
		}
		return *this;
	}
	operator bool() const { return f != NULL; }
};

static bool OpenAppendInUserDir(const char* fileName, FileGuard& outFile, char outPath[MAX_PATH])
{
	if (!fileName || !outPath) return false;
	outPath[0] = '\0';
	if (!szUserDocPath[0]) return false;

	sprintf_s(outPath, MAX_PATH, "%s\\%s", szUserDocPath, fileName);

	FILE* fp = NULL;
	fopen_s(&fp, outPath, "a");
	outFile = FileGuard(fp);
	return (bool)outFile;
}

static bool ParseFloat(const char* s, float& out)
{
	if (!s || !s[0]) return false;
	char* end = NULL;
	errno = 0;
	double v = strtod(s, &end);
	if (end == s || errno != 0) return false;
	out = (float)v;
	return true;
}

static bool ParseInt(const char* s, int& out)
{
	if (!s || !s[0]) return false;
	char* end = NULL;
	errno = 0;
	long v = strtol(s, &end, 10);
	if (end == s || errno != 0) return false;
	out = (int)v;
	return true;
}

void cmdDefaultCmdProc(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd || !szCmd[0]) return;

	CPlayerPool* pool = pNetGame->GetPlayerPool();
	if (!pool) return;

	CLocalPlayer* lp = pool->GetLocalPlayer();
	if (!lp) return;

	lp->Say(szCmd);
}

void cmdAudioMessages(PCHAR szCmd)
{
	(void)szCmd;

	if (!HasConfig())
	{
		ChatInfo("-> Config is not available.");
		return;
	}

	const int cur = pConfigFile->GetInt("audiomsgoff");
	const int next = (cur == 1) ? 0 : 1;

	pConfigFile->SetInt("audiomsgoff", next);
	ChatInfo(next ? "-> You have disabled audio messages." : "-> You have enabled audio messages.");
}

void cmdOnfootCorrection(PCHAR szCmd)
{
	float v;
	if (ParseFloat(szCmd, v)) fOnFootCorrectionMultiplier = v;
	ChatDebug("OnFootCorrection: %.4f", fOnFootCorrectionMultiplier);
}

void cmdInCarCorrection(PCHAR szCmd)
{
	float v;
	if (ParseFloat(szCmd, v)) fInCarCorrectionMultiplier = v;
	ChatDebug("InCarCorrection: %.4f", fInCarCorrectionMultiplier);
}

void cmdInAccMultiplier(PCHAR szCmd)
{
	float v;
	if (ParseFloat(szCmd, v)) fInaccuracyFactorMultiplier = v;
	ChatDebug("InAccMultiplier: %.4f", fInaccuracyFactorMultiplier);
}

void cmdSetFrameLimit(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: fpslimit [0-100]");
		return;
	}

	int limit = 0;
	if (!ParseInt(szCmd, limit) || limit < 0 || limit > 100)
	{
		ChatDebug("Invalid frame limit. Use a value between 0 and 100.");
		return;
	}

	*(PDWORD)0xC1704C = (DWORD)((float)limit * 1.375f);
	ChatDebug("-> Frame limit set to %d.", limit);
}

void cmdQuit(PCHAR szCmd)
{
	(void)szCmd;
	QuitGame();
}

void cmdKill(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;

	ped->ResetDamageEntity();
	ped->SetDead();
}

void cmdDance(PCHAR szCmd)
{
	if (!HasGame()) return;
	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;

	int style = 0;
	ParseInt(szCmd, style);
	ped->StartDancing(style);
}

void cmdHandsUp(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;

	ped->HandsUp();
}

void cmdCigar(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;

	ped->HoldItem(0x5CD);
}

void cmdRcon(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: rcon <command>");
		return;
	}

	if (!HasNetGame()) return;

	RakClientInterface* rc = pNetGame->GetRakClient();
	if (!rc)
	{
		ChatDebug("-> Not connected.");
		return;
	}

	const DWORD len = (DWORD)strlen(szCmd);

	BYTE packetId = ID_RCON_COMMAND;
	RakNet::BitStream bs;
	bs.Write(packetId);
	bs.Write(len);
	bs.Write(szCmd, len);

	rc->Send(&bs, HIGH_PRIORITY, RELIABLE, 0);
	ChatDebug("-> RCON command sent.");
}

void cmdCmpStat(PCHAR szCmd)
{
	(void)szCmd;

	float upRatio = 0.0f;
	if (_sendtoUncompressedTotal > 0)
		upRatio = (float)_sendtoCompressedTotal / (float)_sendtoUncompressedTotal;

	const float downRatio = 0.0f;

	ChatDebug("Compression: up %.2f%%, down %.2f%%", upRatio * 100.0f, downRatio * 100.0f);
}

void cmdSavePos(PCHAR szCmd)
{
	if (!HasGame()) return;

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	char path[MAX_PATH];
	FileGuard file;
	if (!OpenAppendInUserDir("savedpositions.txt", file, path))
	{
		ChatDebug("-> Failed to open savedpositions.txt for append.");
		return;
	}

	const char* comment = (szCmd && szCmd[0]) ? szCmd : "";

	if (player->IsInVehicle())
	{
		VEHICLE_TYPE* veh = player->GetGtaVehicle();
		if (!veh)
		{
			ChatDebug("Failed to read current vehicle.");
			return;
		}

		DWORD vehicleId = GamePool_Vehicle_GetIndex(veh);
		float zAngle = 0.0f;
		ScriptCommand(&get_car_z_angle, vehicleId, &zAngle);

		fprintf(file.f,
			"AddStaticVehicle(%u,%.4f,%.4f,%.4f,%.4f,%u,%u); // %s\n",
			veh->entity.nModelIndex,
			veh->entity.mat->pos.X, veh->entity.mat->pos.Y, veh->entity.mat->pos.Z,
			zAngle, veh->byteColor1, veh->byteColor2,
			comment);

		ChatDebug("-> In-car position saved.");
		return;
	}

	PED_TYPE* ped = player->GetGtaActor();
	if (!ped)
	{
		ChatDebug("Failed to read player ped.");
		return;
	}

	float zAngle = 0.0f;
	ScriptCommand(&get_actor_z_angle, player->m_dwGTAId, &zAngle);

	fprintf(file.f,
		"AddPlayerClass(%u,%.4f,%.4f,%.4f,%.4f,0,0,0,0,0,0); // %s\n",
		player->GetModelIndex(),
		ped->entity.mat->pos.X, ped->entity.mat->pos.Y, ped->entity.mat->pos.Z,
		zAngle,
		comment);

	ChatDebug("-> On-foot position saved.");
}

void cmdRawSave(PCHAR szCmd)
{
	if (!HasGame()) return;

	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;

	const char* comment = (szCmd && szCmd[0]) ? szCmd : "";

	if (ped->IsInVehicle())
	{
		char path[MAX_PATH];
		FileGuard f;
		if (!OpenAppendInUserDir("rawvehicles.txt", f, path))
		{
			ChatDebug("Failed to open rawvehicles.txt for append.");
			return;
		}

		VEHICLE_TYPE* v = ped->GetGtaVehicle();
		if (!v)
		{
			ChatDebug("Failed to read current vehicle.");
			return;
		}

		DWORD vehicleId = GamePool_Vehicle_GetIndex(v);
		float zAngle = 0.0f;
		ScriptCommand(&get_car_z_angle, vehicleId, &zAngle);

		fprintf_s(f.f, "%u,%.4f,%.4f,%.4f,%.4f,%u,%u ; %s\n",
			v->entity.nModelIndex,
			v->entity.mat->pos.X, v->entity.mat->pos.Y, v->entity.mat->pos.Z,
			zAngle,
			v->byteColor1, v->byteColor2,
			comment);

		ChatDebug("-> Raw in-car position saved.");
		return;
	}

	char path[MAX_PATH];
	FileGuard f;
	if (!OpenAppendInUserDir("rawpositions.txt", f, path))
	{
		ChatDebug("-> Failed to open rawpositions.txt for append.");
		return;
	}

	PED_TYPE* a = ped->GetGtaActor();
	if (!a)
	{
		ChatDebug("Failed to read player ped.");
		return;
	}

	float zAngle = 0.0f;
	ScriptCommand(&get_actor_z_angle, ped->m_dwGTAId, &zAngle);

	fprintf_s(f.f, "%.4f,%.4f,%.4f,%.4f ; %s\n",
		a->entity.mat->pos.X, a->entity.mat->pos.Y, a->entity.mat->pos.Z,
		zAngle,
		comment);

	ChatDebug("-> Raw on-foot position saved.");
}

void cmdPlayerSkin(PCHAR szCmd)
{
#ifndef _DEBUG
	if (!tSettings.bDebug) return;
#endif

	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: player_skin <skinId>");
		return;
	}

	int skin = 0;
	if (!ParseInt(szCmd, skin))
	{
		ChatDebug("Invalid skin id.");
		return;
	}

	if (!HasGame() || !pGame->IsGameLoaded()) return;

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	player->SetModelIndex(skin);
}

void cmdCreateVehicle(PCHAR szCmd)
{
	if (!tSettings.bDebug) return;

	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: v <vehicleId>");
		return;
	}

	int model = 0;
	if (!ParseInt(szCmd, model))
	{
		ChatDebug("Invalid vehicle id.");
		return;
	}

	if (!HasGame() || !pGame->IsGameLoaded())
	{
		ChatDebug("Game is not loaded.");
		return;
	}

	CGame::RequestModel(model);
	CGame::LoadRequestedModels();

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player)
	{
		ChatDebug("-> Failed to obtain the player ped.");
		return;
	}

	MATRIX4X4 mat;
	player->GetMatrix(&mat);

	CHAR name[16];
	sprintf_s(name, "TYPE_%d", model);

	CVehicle* veh = pGame->NewVehicle(model, mat.pos.X - 5.0f, mat.pos.Y - 5.0f, mat.pos.Z + 1.0f, 0.0f, name);
	if (!veh)
	{
		ChatDebug("-> Failed to create vehicle.");
		return;
	}

	veh->Add();
}

void cmdSelectVehicle(PCHAR szCmd)
{
	(void)szCmd;
	if (!tSettings.bDebug) return;
	GameDebugEntity(0, 0, 10);
}

void cmdSetWeather(PCHAR szCmd)
{
	int id = 0;
	if (!ParseInt(szCmd, id))
	{
		ChatDebug("Usage: set_weather <weatherId>");
		return;
	}
	if (HasGame()) pGame->SetWorldWeather(id);
}

void cmdSetTime(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: set_time <hour 0-23> <minute 0-59>");
		return;
	}

	int hour = 0, minute = 0;
	if (sscanf_s(szCmd, "%d %d", &hour, &minute) != 2)
	{
		ChatDebug("Usage: set_time <hour 0-23> <minute 0-59>");
		return;
	}

	if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
	{
		ChatDebug("-> Invalid time. Use hour 0-23 and minute 0-59.");
		return;
	}

	if (HasGame()) pGame->SetWorldTime(hour, minute);
}

void cmdShowInterior(PCHAR szCmd)
{
	(void)szCmd;
	DWORD dwRet = 0;
	ScriptCommand(&get_active_interior, &dwRet);
	ChatDebug("Current interior: %u", dwRet);
}

#ifdef _DEBUG

void cmdSetInterior(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: setinterior <interiorId>");
		return;
	}
	int iInterior = 0;
	if (!ParseInt(szCmd, iInterior))
	{
		ChatDebug("-> Invalid interior id.");
		return;
	}
	ScriptCommand(&select_interior, iInterior);
}

void cmdPlayerToVehicle(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd) return;

	int iPlayer = 0, iVehicle = 0;
	if (sscanf_s(szCmd, "%u %u", &iPlayer, &iVehicle) != 2) return;

	CRemotePlayer* rp = pNetGame->GetPlayerPool() ? pNetGame->GetPlayerPool()->GetAt(iPlayer) : NULL;
	CVehicle* v = pNetGame->GetVehiclePool() ? pNetGame->GetVehiclePool()->GetAt(iVehicle) : NULL;

	if (rp && v && rp->GetPlayerPed())
		rp->GetPlayerPed()->PutDirectlyInVehicle(v->m_dwGTAId, 0);
}

void cmdCamMode(PCHAR szCmd)
{
	if (!szCmd) return;
	int iMode = 0;
	if (sscanf_s(szCmd, "%d", &iMode) != 1) return;
	if (pbyteCameraMode) *pbyteCameraMode = (BYTE)iMode;
}

void cmdCreatePlayer(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: p <playerId> <skinId>");
		return;
	}

	int skin = 0, playerNum = 0;
	if (sscanf_s(szCmd, "%d %d", &playerNum, &skin) != 2)
	{
		ChatDebug("Usage: p <playerId> <skinId>");
		return;
	}

	if (!HasGame() || !pGame->IsGameLoaded())
	{
		ChatDebug("-> Game is not loaded.");
		return;
	}

	CGame::RequestModel(skin);
	CGame::LoadRequestedModels();

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player)
	{
		ChatDebug("-> Failed to obtain the player ped.");
		return;
	}

	MATRIX4X4 mat;
	player->GetMatrix(&mat);

	CPlayerPed* test = pGame->NewPlayer(playerNum, 0, mat.pos.X - 5.0f, mat.pos.Y - 5.0f, mat.pos.Z, 0.0f);
	(void)test;
}

void cmdActorDebug(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: actor_debug <actorId>");
		return;
	}
	int iActor = 0;
	if (!ParseInt(szCmd, iActor)) return;
	GameDebugEntity(iActor, 0, 1);
}

void cmdActorTaskDebug(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: actor_task_debug <actorId>");
		return;
	}
	int iActor = 0;
	if (!ParseInt(szCmd, iActor)) return;
	GameDebugEntity(iActor, 0, 8);
}

void cmdVehicleDebug(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: vehicle_debug <vehicleId>");
		return;
	}

	int iVehicle = 0;
	if (!ParseInt(szCmd, iVehicle)) return;

	if (iVehicle < 3) ChatDebug("-> Warning: vehicle ids below 3 are usually reserved.");
	GameDebugEntity(iVehicle, 0, 2);
}

void cmdShowGameInterfaceDebug(PCHAR szCmd)
{
	(void)szCmd;
	GameDebugEntity(0, 0, 6);
}

void cmdPlayerVehicleDebug(PCHAR szCmd)
{
	(void)szCmd;
	GameDebugEntity(0, 0, 3);
}

void cmdDebugStop(PCHAR szCmd)
{
	(void)szCmd;
	GameDebugScreensOff();
}

void cmdSetCameraPos(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: set_cam_pos <x> <y> <z> <lookx> <looky> <lookz>");
		return;
	}

	float fX = 0, fY = 0, fZ = 0, fLookX = 0, fLookY = 0, fLookZ = 0;
	if (sscanf_s(szCmd, "%f %f %f %f %f %f", &fX, &fY, &fZ, &fLookX, &fLookY, &fLookZ) != 6) return;

	if (!HasGame() || !pGame->GetCamera()) return;

	pGame->GetCamera()->SetPosition(fX, fY, fZ, 0.0f, 0.0f, 0.0f);
	pGame->GetCamera()->LookAtPoint(fLookX, fLookY, fLookZ, 0);
}

void cmdGiveActorWeapon(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: give_weapon <weaponId>");
		return;
	}

	int w = 0;
	if (sscanf_s(szCmd, "%d", &w) != 1) return;

	if (!HasGame()) return;
	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;

	ped->GiveWeapon(w, 1000);
}

void cmdDisplayMemory(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: disp_mem <hexAddress> <decimalOffset>");
		return;
	}

	DWORD addr = 0, off = 0;
	if (sscanf_s(szCmd, "%X %u", &addr, &off) != 2 || !addr)
	{
		ChatDebug("Usage: disp_mem <hexAddress> <decimalOffset>");
		return;
	}

	GameDebugEntity(addr, off, 5);
}

void cmdDisplayMemoryAsc(PCHAR szCmd)
{
	if (!szCmd || !szCmd[0])
	{
		ChatDebug("Usage: disp_mem_asc <hexAddress> <decimalOffset>");
		return;
	}

	DWORD addr = 0, off = 0;
	if (sscanf_s(szCmd, "%X %u", &addr, &off) != 2 || !addr)
	{
		ChatDebug("Usage: disp_mem_asc <hexAddress> <decimalOffset>");
		return;
	}

	GameDebugEntity(addr, off, 15);
}

void cmdRemotePlayer(PCHAR szCmd)
{
	(void)szCmd;

	if (!HasNetGame()) return;

	CPlayerPool* pool = pNetGame->GetPlayerPool();
	if (!pool) return;

	if (!HasGame()) return;
	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	MATRIX4X4 mat;
	player->GetMatrix(&mat);

	VECTOR vecPos;
	vecPos.X = mat.pos.X - 5.0f;
	vecPos.Y = mat.pos.Y - 5.0f;
	vecPos.Z = mat.pos.Z;

	while (iCurrentPlayerTest != 10)
	{
		pool->New(iCurrentPlayerTest, "TestPlayer");
		CRemotePlayer* rp = pool->GetAt((BYTE)iCurrentPlayerTest);
		if (rp)
		{
			rp->Spawn(255, 50, &vecPos, 0.0f, 0);
			vecPos.X += 1.0f;
			if (rp->GetPlayerPed()) rp->GetPlayerPed()->Remove();
			pTestPlayer = rp;
			ChatDebug("-> Created player %u.", iCurrentPlayerTest);
		}
		iCurrentPlayerTest++;
	}
}

void cmdRemotePlayerRespawn(PCHAR szCmd)
{
	(void)szCmd;
}

void cmdPlayerAddr(PCHAR szCmd)
{
	int iActor = 0;
	if (!szCmd) return;
	sscanf_s(szCmd, "%d", &iActor);

	if (iActor == 0) ChatDebug("0x%X", GamePool_FindPlayerPed());
	else ChatDebug("0x%X", GamePool_Ped_GetAt(iActor));
}

void cmdVehicleSyncTest(PCHAR szCmd)
{
	(void)szCmd;
	GameDebugEntity(0, 0, 7);
}

void cmdSay(PCHAR szCmd)
{
	if (!szCmd) return;
	int iSay = atoi(szCmd);
	BYTE* dwPlayerPed1 = (BYTE*)GamePool_FindPlayerPed();
	if (!dwPlayerPed1) return;

	_asm push 0
	_asm push 0
	_asm push 0
	_asm push 0x3F800000
	_asm push 0
	_asm push iSay
	_asm mov ecx, dwPlayerPed1
	_asm mov ebx, 0x5EFFE0
	_asm call ebx
}

DWORD MyCreateActor(int iModelID, float X, float Y, float Z, float fRotation)
{
	DWORD dwRetID = 0;
	CGame::RequestModel(iModelID);
	CGame::LoadRequestedModels();
	while (!CGame::IsModelLoaded(iModelID)) Sleep(5);

	ScriptCommand(&create_actor, 5, iModelID, X, Y, Z, &dwRetID);
	ScriptCommand(&set_actor_z_angle, dwRetID, fRotation);

	ChatDebug("New actor: 0x%X", GamePool_Ped_GetAt(dwRetID));
	return dwRetID;
}

void cmdCreateActor(PCHAR szCmd)
{
	if (!HasGame()) return;

	int iActor = 0;
	if (!ParseInt(szCmd, iActor)) return;

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	MATRIX4X4 mat;
	player->GetMatrix(&mat);

	VECTOR vecPos;
	vecPos.X = mat.pos.X - 5.0f;
	vecPos.Y = mat.pos.Y - 5.0f;
	vecPos.Z = mat.pos.Z;

	MyCreateActor(iActor, vecPos.X, vecPos.Y, vecPos.Z, 0.0f);
}

void cmdFreeAim(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	player->GiveWeapon(28, 200);
	player->StartPassengerDriveByMode();
}

void cmdWorldVehicleRemove(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd) return;

	CVehiclePool* vp = pNetGame->GetVehiclePool();
	if (!vp) return;

	int id = 0;
	sscanf_s(szCmd, "%d", &id);

	CVehicle* v = vp->GetAt(id);
	if (v) v->Remove();
}

void cmdWorldVehicleAdd(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd) return;

	CVehiclePool* vp = pNetGame->GetVehiclePool();
	if (!vp) return;

	int id = 0;
	sscanf_s(szCmd, "%d", &id);

	CVehicle* v = vp->GetAt(id);
	if (v) v->Add();
}

void cmdWorldPlayerRemove(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd) return;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	if (!pp) return;

	int id = 0;
	sscanf_s(szCmd, "%d", &id);

	CRemotePlayer* rp = pp->GetAt(id);
	if (rp && rp->GetPlayerPed()) rp->GetPlayerPed()->Remove();
}

void cmdWorldPlayerAdd(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd) return;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	if (!pp) return;

	int id = 0;
	sscanf_s(szCmd, "%d", &id);

	CRemotePlayer* rp = pp->GetAt(id);
	if (rp && rp->GetPlayerPed()) rp->GetPlayerPed()->Add();
}

void cmdAnimSet(PCHAR szCmd)
{
	if (!HasGame() || !szCmd) return;
	CPlayerPed* player = pGame->FindPlayerPed();
	if (player) player->SetAnimationSet(szCmd);
}

void cmdKillRemotePlayer(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd) return;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	if (!pp) return;

	int id = 0;
	sscanf_s(szCmd, "%d", &id);

	CRemotePlayer* rp = pp->GetAt(id);
	if (rp && rp->GetPlayerPed()) rp->GetPlayerPed()->SetDead();
}

void cmdApplyAnimation(PCHAR szCmd)
{
	if (!szCmd) return;
	char szAnimFile[256] = { 0 };
	char szAnimName[256] = { 0 };
	sscanf_s(szCmd, "%s %s", szAnimName, (unsigned)_countof(szAnimName), szAnimFile, (unsigned)_countof(szAnimFile));
	ChatDebug("Applied animation: %s (%s)", szAnimName, szAnimFile);
}

void cmdCheckpointTest(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	MATRIX4X4 mat;
	player->GetMatrix(&mat);

	VECTOR vecPos;
	vecPos.X = mat.pos.X - 5.0f;
	vecPos.Y = mat.pos.Y - 5.0f;
	vecPos.Z = mat.pos.Z;

	VECTOR vecExtent = { 2.0f, 2.0f, 2.0f };

	pGame->SetCheckpointInformation(&vecPos, &vecExtent);
	pGame->ToggleCheckpoints(TRUE);
}

void cmdCreateRadarMarkers(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	MATRIX4X4 mat;
	player->GetMatrix(&mat);

	VECTOR vecPos;
	vecPos.X = mat.pos.X - 10.0f;
	vecPos.Y = mat.pos.Y;
	vecPos.Z = mat.pos.Z;

	for (int i = 0; i <= 100; i++)
	{
		pGame->CreateRadarMarkerIcon(i, vecPos.X, vecPos.Y, vecPos.Z);
		vecPos.X -= 40.0f;
	}
}

void cmdPlaySound(PCHAR szCmd)
{
	if (!HasGame()) return;

	CPlayerPed* player = pGame->FindPlayerPed();
	if (!player) return;

	MATRIX4X4 mat;
	player->GetMatrix(&mat);

	int iSound = 0;
	if (!ParseInt(szCmd, iSound)) return;

	ScriptCommand(&play_sound, mat.pos.X, mat.pos.Y, mat.pos.Z, iSound);
}

void cmdSaveLocation(PCHAR szCmd)
{
	FileGuard f;
	fopen_s(&f.f, "savedlocations.txt", "a");
	if (!f) return;

	float fZAngle = 0.0f;
	ScriptCommand(&get_actor_z_angle, 0x1, &fZAngle);

	if (!HasGame()) return;
	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;

	VECTOR* pVec = &ped->GetGtaActor()->entity.mat->pos;
	fprintf(f.f, "%.4f, %.4f, %.4f, %.4f   // %s\n", pVec->X, pVec->Y, pVec->Z, fZAngle, (szCmd ? szCmd : ""));
}

void cmdResetVehiclePool(PCHAR szCmd)
{
	(void)szCmd;
	if (HasNetGame()) pNetGame->ResetVehiclePool();
}

void cmdDeactivatePlayers(PCHAR szCmd)
{
	(void)szCmd;
	if (HasNetGame() && pNetGame->GetPlayerPool()) pNetGame->GetPlayerPool()->DeactivateAll();
}

void cmdSetFarClip(PCHAR szCmd)
{
	float f = 0.0f;
	if (ParseFloat(szCmd, f)) fFarClip = f;
}

void cmdStartJetpack(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;
	CPlayerPed* ped = pGame->FindPlayerPed();
	if (ped) ped->StartJetpack();
}

void cmdStopJetpack(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;
	CPlayerPed* ped = pGame->FindPlayerPed();
	if (ped) ped->StopJetpack();
}

void cmdLotsOfV(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasNetGame()) return;

	int i = 210;
	VECTOR vecSpawnPos;
	vecSpawnPos.X = 1482.7546f;
	vecSpawnPos.Y = 3236.4551f;
	vecSpawnPos.Z = 0.1f;
	CHAR plate[2] = "";

	while (i != MAX_VEHICLES)
	{
		pNetGame->GetVehiclePool()->New(i, 473, &vecSpawnPos, 0.0f, 0, 0, &vecSpawnPos, 0.0f, 0, plate);
		vecSpawnPos.X -= 7.0f;
		i++;
	}

	ChatDebug("Done");
}

void TestExit(PCHAR szCmd)
{
	(void)szCmd;
	if (pD3DDevice) pD3DDevice->ShowCursor(TRUE);
	ShowCursor(TRUE);
	SetCursor(LoadCursor(NULL, IDC_ARROW));
	ShowCursor(TRUE);
	ChatDebug("Done");
}

void DriveByTest(PCHAR szCmd)
{
	(void)szCmd;
	cmdRemotePlayer("");

	if (!HasNetGame()) return;

	CRemotePlayer* pRemote[4] = { NULL, NULL, NULL, NULL };
	pRemote[0] = pNetGame->GetPlayerPool()->GetAt(1);
	if (pRemote[0] && pRemote[0]->GetPlayerPed()) pRemote[0]->GetPlayerPed()->GiveWeapon(31, 200);

	int iVehicleID = pNetGame->GetVehiclePool()->FindGtaIDFromID(3);

	if (HasGame())
	{
		CPlayerPed* ped = pGame->FindPlayerPed();
		if (ped) ped->GiveWeapon(31, 200);
	}

	(void)iVehicleID;
}

void cmdSpectatev(PCHAR szCmd)
{
	if (!HasNetGame()) return;

	CLocalPlayer* pPlayer = pNetGame->GetPlayerPool() ? pNetGame->GetPlayerPool()->GetLocalPlayer() : NULL;
	if (!pPlayer) return;

	if (szCmd && szCmd[0])
	{
		int iVehicleID = 0;
		int iSpectateMode = 0;
		if (sscanf_s(szCmd, "%d %d", &iVehicleID, &iSpectateMode) >= 1)
		{
			pPlayer->ToggleSpectating(TRUE);
			pPlayer->m_byteSpectateMode = iSpectateMode;
			pPlayer->SpectateVehicle(iVehicleID);
		}
	}
	else
	{
		pPlayer->ToggleSpectating(FALSE);
	}
}

void cmdSpectatep(PCHAR szCmd)
{
	if (!HasNetGame()) return;

	CLocalPlayer* pPlayer = pNetGame->GetPlayerPool() ? pNetGame->GetPlayerPool()->GetLocalPlayer() : NULL;
	if (!pPlayer) return;

	if (szCmd && szCmd[0])
	{
		int iPlayerID = 0;
		int iSpectateMode = 0;
		if (sscanf_s(szCmd, "%d %d", &iPlayerID, &iSpectateMode) >= 1)
		{
			pPlayer->ToggleSpectating(TRUE);
			pPlayer->m_byteSpectateMode = iSpectateMode;
			pPlayer->SpectatePlayer(iPlayerID);
		}
	}
	else
	{
		pPlayer->ToggleSpectating(FALSE);
	}
}

void cmdClothes(PCHAR szCmd)
{
	if (!szCmd) return;

	char szTexture[64] = { 0 };
	char szModel[64] = { 0 };
	int iType = 0;

	sscanf_s(szCmd, "%s %s %d",
		szTexture, (unsigned)_countof(szTexture),
		szModel, (unsigned)_countof(szModel),
		&iType);

	if (strcmp(szTexture, "-") == 0) strcpy_s(szTexture, "");
	if (strcmp(szModel, "-") == 0) strcpy_s(szModel, "");

	__asm
	{
		push 0xFFFFFFFF;
		mov eax, 0x56E210;
		call eax;
		add esp, 4;

		push 0;
		push eax;

		mov ecx, [eax + 1152];
		mov ecx, [ecx + 4];

		push iType;
		lea eax, szModel;
		push eax;
		lea eax, szTexture;
		push eax;
		mov eax, 0x5A8080;
		call eax;

		mov eax, 0x5A82C0;
		call eax;
		add esp, 8;
	}
}

void cmdNoFire(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;
	CPlayerPed* ped = pGame->FindPlayerPed();
	if (ped) ped->ExtinguishFire();
}

void cmdSit(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;
	CPlayerPed* ped = pGame->FindPlayerPed();
	if (!ped) return;
	ScriptCommand(&actor_task_sit, ped->m_dwGTAId, -1);
}

void cmdAttractAddr(PCHAR szCmd)
{
	(void)szCmd;
	ChatDebug("0x%X", *(DWORD*)0xB744C4);
}

void cmdPlayerVtbl(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* pPed = pGame->FindPlayerPed();
	if (!pPed || !pPed->m_pPed) return;

	BYTE* b = (BYTE*)pPed->m_pPed + 1156;
	BYTE sv = *b;

	pPed->m_pPed->dwPedType = 3;
	*b = 2;

	pPed->TeleportTo(
		pPed->m_pPed->entity.mat->pos.X,
		pPed->m_pPed->entity.mat->pos.Y,
		pPed->m_pPed->entity.mat->pos.Z + 10.0f);

	*b = sv;
	pPed->m_pPed->dwPedType = 0;
}

void cmdDumpFreqTable(PCHAR szCmd)
{
	(void)szCmd;

	if (!HasNetGame()) return;

	unsigned int freqTable[256];
	RakClientInterface* rc = pNetGame->GetRakClient();
	if (!rc) return;

	rc->GetSendFrequencyTable(freqTable);

	FILE* fout = NULL;
	fopen_s(&fout, "freqclient.txt", "w");
	if (!fout) return;

	for (int i = 0; i < 256; i++)
		fprintf(fout, "%u,", freqTable[i]);

	fclose(fout);
	ChatDebug("Done.");
}

void cmdPopWheels(PCHAR szCmd)
{
	if (!HasNetGame()) return;

	int toggleInt = 0;
	if (!ParseInt(szCmd, toggleInt)) toggleInt = 0;
	BYTE byteToggle = (BYTE)toggleInt;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	CVehiclePool* vp = pNetGame->GetVehiclePool();
	if (!pp || !vp) return;

	VEHICLEID vehicleid = pp->GetLocalPlayer()->m_CurrentVehicle;
	if (vehicleid == 0xFFFF)
	{
		ChatDebug("-> Get in a vehicle first.");
		return;
	}

	CVehicle* v = vp->GetAt(vehicleid);
	if (!v) return;

	v->SetWheelPopped(0, byteToggle);
	v->SetWheelPopped(1, byteToggle);
	v->SetWheelPopped(2, byteToggle);
	v->SetWheelPopped(3, byteToggle);

	ChatDebug(byteToggle ? "-> Wheels popped." : "-> Wheels restored.");
}

void cmdVModels(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasNetGame()) return;

	CVehiclePool* vp = pNetGame->GetVehiclePool();
	if (!vp) return;

	int iCount[211];
	memset(&iCount[0], 0, sizeof(iCount));

	int totalVehicles = 0;
	for (int x = 0; x != MAX_VEHICLES; x++)
	{
		if (vp->GetSlotState(x))
		{
			int type = vp->m_SpawnInfo[x].iVehicleType;
			int idx = type - 400;
			if (idx >= 0 && idx < 211) iCount[idx]++;
			totalVehicles++;
		}
	}

	int totalTypes = 0;
	for (int x = 0; x != 211; x++)
	{
		if (iCount[x]) totalTypes++;
	}

	ChatDebug("Vehicle types: %u/%u", totalTypes, totalVehicles);
}

void cmdFollowPedSA(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	VECTOR LookAt;
	CAMERA_AIM* Aim = GameGetInternalAim();
	if (!Aim) return;

	LookAt.X = Aim->pos1x + (Aim->f1x * 10.0f);
	LookAt.Y = Aim->pos1y + (Aim->f1y * 10.0f);
	LookAt.Z = Aim->pos1z + (Aim->f1z * 10.0f);

	CPlayerPed* ped = pGame->FindPlayerPed();
	if (ped)
		ped->ApplyCommandTask("FollowPedSA", 0, 1500, -1, &LookAt, 0, 1.0f, 500, 3, 0);
}

void cmdTaskSitIdle(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;

	CPlayerPed* ped = pGame->FindPlayerPed();
	if (ped)
		ped->ApplyCommandTask("TaskSitIdle", 0, 99999, -1, 0, 0, 0.25f, 750, 3, 0);
}

extern int iVehiclesBench;
extern int iPlayersBench;
extern int iPicksupsBench;
extern int iMenuBench;
extern int iObjectBench;

void cmdBench(PCHAR szCmd)
{
	(void)szCmd;
	ChatDebug("Pl: %u V: %u Pk: %u Mn: %u O: %u T: %u", iPlayersBench, iVehiclesBench, iPicksupsBench, iMenuBench, iObjectBench, 0);
}

void cmdDisableEnEx(PCHAR szCmd)
{
	(void)szCmd;
	if (HasGame()) pGame->DisableEnterExits();
}

void cmdStartPiss(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasNetGame()) return;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	if (!pp) return;

	CLocalPlayer* lp = pp->GetLocalPlayer();
	if (lp) lp->ApplySpecialAction(SPECIAL_ACTION_URINATE);
}

void cmdShowMyVirtualWorld(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasNetGame()) return;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	if (!pp) return;

	CLocalPlayer* lp = pp->GetLocalPlayer();
	if (lp) ChatDebug("MyVW: %u", lp->GetVirtualWorld());
}

void cmdShowPlayerVirtualWorld(PCHAR szCmd)
{
	if (!HasNetGame() || !szCmd || !szCmd[0]) return;

	int id = 0;
	if (!ParseInt(szCmd, id)) return;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	if (!pp) return;

	if (pp->GetSlotState((BYTE)id))
	{
		CRemotePlayer* rp = pp->GetAt((BYTE)id);
		if (rp) ChatDebug("VW: %u", rp->m_iVirtualWorld);
	}
}

static void cmdTeamTest(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasNetGame()) return;

	CPlayerPool* pp = pNetGame->GetPlayerPool();
	if (!pp) return;

	CLocalPlayer* lp = pp->GetLocalPlayer();
	if (!lp) return;

	ChatDebug("Your team: %d", lp->GetTeam());

	for (unsigned short i = 0; i < MAX_PLAYERS; i++)
	{
		CRemotePlayer* rp = pp->GetAt(i);
		if (rp)
			ChatDebug("ID: %d Name: %s Team: %d", rp->GetID(), rp->GetName(), rp->GetTeam());
	}
}

#endif

static void cmdSetChatPageSize(PCHAR szCmd)
{
	DWORD dwSize = 0;

	if (szCmd && szCmd[0] != 0)
	{
		dwSize = atol(szCmd);
		if (dwSize >= 10 && dwSize <= 20)
		{
			if (pChatWindow) pChatWindow->SetPageSize(dwSize);
			if (pConfigFile) pConfigFile->SetInt("pagesize", dwSize);
			ChatInfo("-> Chat page size set to %u lines.", dwSize);
			return;
		}
	}

	ChatDebug("Usage: pagesize [10-20]");
}

static void cmdToggleChatTimeStamp(PCHAR szCmd)
{
	(void)szCmd;

	if (!pChatWindow)
	{
		ChatInfo("-> Chat window is not available.");
		return;
	}

	const bool visible = pChatWindow->IsTimeStampVisible();
	pChatWindow->SetTimeStampVisisble(!visible);

	if (pConfigFile) pConfigFile->SetInt("timestamp", visible ? 0 : 1);

	ChatInfo(!visible ? "-> Chat timestamp enabled." : "-> Chat timestamp disabled.");
}

void cmdDisableVehMapIcon(PCHAR szCmd)
{
	(void)szCmd;
	if (!HasGame()) return;
	pGame->m_bDisableVehMapIcons = !pGame->m_bDisableVehMapIcons;
	ChatInfo(pGame->m_bDisableVehMapIcons ? "-> Vehicle map icons disabled." : "-> Vehicle map icons enabled.");
}

static void cmdShowMem(PCHAR szCmd)
{
	(void)szCmd;
	ChatDebug("Memory: %u", *(DWORD*)0x8A5A80);
}

static void cmdHudScaleFix(PCHAR szCmd)
{
	(void)szCmd;

	if (!HasConfig()) return;

	bWantHudScaling = !bWantHudScaling;
	pConfigFile->SetInt("nohudscalefix", bWantHudScaling ? 0 : 1);

	ChatInfo(bWantHudScaling ? "-> HUD scale fix enabled." : "-> HUD scale fix disabled.");
}

static void cmdHeadMove(PCHAR szCmd)
{
	(void)szCmd;

	bHeadMove = !bHeadMove;
	ChatInfo(bHeadMove ? "-> Head movements enabled." : "-> Head movements disabled.");
}

void cmdDebugLabels(PCHAR szCmd)
{
	(void)szCmd;
	bShowDebugLabels = !bShowDebugLabels;
	ChatInfo(bShowDebugLabels ? "-> Debug labels enabled." : "-> Debug labels disabled.");
}

void cmdTestDeathWindow(PCHAR szCmd)
{
	(void)szCmd;

	if (pDeathWindow)
	{
		pDeathWindow->AddMessage("Pooper", "Pooper333", -1, -1, 1);
		pDeathWindow->AddMessage("Pooper", "Pooper", -1, -1, 5);
		pDeathWindow->AddMessage("Pooper", "Pooper", -1, -1, 15);
		pDeathWindow->AddMessage("Pooper", "Pooper", -1, -1, 14);
		pDeathWindow->AddMessage("Pooper", "Pooper", -1, -1, 2);
		pDeathWindow->AddMessage(NULL, "PooperPooperPooper0001", -1, -1, 5);
		pDeathWindow->AddMessage(NULL, "Pooper", -1, -1, -1);
		pDeathWindow->AddMessage("Pooper", "PooperPooperPooper0001", -1, -1, 0);
	}
}

void SetupCommands()
{
	pCmdWindow->AddDefaultCmdProc(cmdDefaultCmdProc);
	pCmdWindow->AddCmdProc("quit", cmdQuit);
	pCmdWindow->AddCmdProc("q", cmdQuit);
	pCmdWindow->AddCmdProc("save", cmdSavePos);
	pCmdWindow->AddCmdProc("rs", cmdRawSave);
	pCmdWindow->AddCmdProc("rcon", cmdRcon);
	pCmdWindow->AddCmdProc("mem", cmdShowMem);

	pCmdWindow->AddCmdProc("pagesize", cmdSetChatPageSize);
	pCmdWindow->AddCmdProc("timestamp", cmdToggleChatTimeStamp);
	pCmdWindow->AddCmdProc("hudscalefix", cmdHudScaleFix);
	pCmdWindow->AddCmdProc("headmove", cmdHeadMove);

	pCmdWindow->AddCmdProc("disvehico", cmdDisableVehMapIcon);
	pCmdWindow->AddCmdProc("testdw", cmdTestDeathWindow);

#ifndef _DEBUG
	if (tSettings.bDebug)
	{
#endif
		pCmdWindow->AddCmdProc("vsel", cmdSelectVehicle);
		pCmdWindow->AddCmdProc("v", cmdCreateVehicle);
		pCmdWindow->AddCmdProc("vehicle", cmdCreateVehicle);
		pCmdWindow->AddCmdProc("player_skin", cmdPlayerSkin);
		pCmdWindow->AddCmdProc("set_weather", cmdSetWeather);
		pCmdWindow->AddCmdProc("set_time", cmdSetTime);
#ifndef _DEBUG
	}
#endif

	pCmdWindow->AddCmdProc("interior", cmdShowInterior);
	pCmdWindow->AddCmdProc("cmpstat", cmdCmpStat);
	pCmdWindow->AddCmdProc("dl", cmdDebugLabels);
	pCmdWindow->AddCmdProc("onfoot_correct", cmdOnfootCorrection);
	pCmdWindow->AddCmdProc("incar_correct", cmdInCarCorrection);
	pCmdWindow->AddCmdProc("inacc_multiplier", cmdInAccMultiplier);
	pCmdWindow->AddCmdProc("fpslimit", cmdSetFrameLimit);
	pCmdWindow->AddCmdProc("audiomsg", cmdAudioMessages);

#ifdef _DEBUG
	pCmdWindow->AddCmdProc("setinterior", cmdSetInterior);
	pCmdWindow->AddCmdProc("markers", cmdCreateRadarMarkers);
	pCmdWindow->AddCmdProc("vstest", cmdVehicleSyncTest);
	pCmdWindow->AddCmdProc("paddr", cmdPlayerAddr);
	pCmdWindow->AddCmdProc("cammode", cmdCamMode);
	pCmdWindow->AddCmdProc("ptov", cmdPlayerToVehicle);
	pCmdWindow->AddCmdProc("p", cmdCreatePlayer);
	pCmdWindow->AddCmdProc("player", cmdCreatePlayer);
	pCmdWindow->AddCmdProc("a", cmdCreateActor);
	pCmdWindow->AddCmdProc("actor", cmdCreateActor);
	pCmdWindow->AddCmdProc("actor_debug", cmdActorDebug);
	pCmdWindow->AddCmdProc("actor_task_debug", cmdActorTaskDebug);
	pCmdWindow->AddCmdProc("vehicle_debug", cmdVehicleDebug);
	pCmdWindow->AddCmdProc("pv_debug", cmdPlayerVehicleDebug);
	pCmdWindow->AddCmdProc("debug_stop", cmdDebugStop);
	pCmdWindow->AddCmdProc("s", cmdDebugStop);
	pCmdWindow->AddCmdProc("set_cam_pos", cmdSetCameraPos);
	pCmdWindow->AddCmdProc("give_weapon", cmdGiveActorWeapon);
	pCmdWindow->AddCmdProc("disp_mem", cmdDisplayMemory);
	pCmdWindow->AddCmdProc("disp_mem_asc", cmdDisplayMemoryAsc);
	pCmdWindow->AddCmdProc("rp", cmdRemotePlayer);
	pCmdWindow->AddCmdProc("rpr", cmdRemotePlayerRespawn);
	pCmdWindow->AddCmdProc("say", cmdSay);
	pCmdWindow->AddCmdProc("freeaim", cmdFreeAim);
	pCmdWindow->AddCmdProc("vehicle_remove", cmdWorldVehicleRemove);
	pCmdWindow->AddCmdProc("vehicle_add", cmdWorldVehicleAdd);
	pCmdWindow->AddCmdProc("player_remove", cmdWorldPlayerRemove);
	pCmdWindow->AddCmdProc("player_add", cmdWorldPlayerAdd);
	pCmdWindow->AddCmdProc("animset", cmdAnimSet);
	pCmdWindow->AddCmdProc("kill_player", cmdKillRemotePlayer);
	pCmdWindow->AddCmdProc("anim", cmdApplyAnimation);
	pCmdWindow->AddCmdProc("checktest", cmdCheckpointTest);
	pCmdWindow->AddCmdProc("ps", cmdPlaySound);
	pCmdWindow->AddCmdProc("sl", cmdSaveLocation);
	pCmdWindow->AddCmdProc("resetvp", cmdResetVehiclePool);
	pCmdWindow->AddCmdProc("deactivate", cmdDeactivatePlayers);
	pCmdWindow->AddCmdProc("setfarclip", cmdSetFarClip);
	pCmdWindow->AddCmdProc("start_jetpack", cmdStartJetpack);
	pCmdWindow->AddCmdProc("stop_jetpack", cmdStopJetpack);
	pCmdWindow->AddCmdProc("lotsofv", cmdLotsOfV);
	pCmdWindow->AddCmdProc("cursor", TestExit);
	pCmdWindow->AddCmdProc("drivebytest", DriveByTest);
	pCmdWindow->AddCmdProc("specp", cmdSpectatep);
	pCmdWindow->AddCmdProc("specv", cmdSpectatev);
	pCmdWindow->AddCmdProc("clothes", cmdClothes);
	pCmdWindow->AddCmdProc("nofire", cmdNoFire);
	pCmdWindow->AddCmdProc("sit", cmdSit);
	pCmdWindow->AddCmdProc("dumpfreq", cmdDumpFreqTable);
	pCmdWindow->AddCmdProc("attract", cmdAttractAddr);
	pCmdWindow->AddCmdProc("pop", cmdPopWheels);
	pCmdWindow->AddCmdProc("pvtbl", cmdPlayerVtbl);
	pCmdWindow->AddCmdProc("danc", cmdDance);
	pCmdWindow->AddCmdProc("vmodels", cmdVModels);
	pCmdWindow->AddCmdProc("followped", cmdFollowPedSA);
	pCmdWindow->AddCmdProc("sitidle", cmdTaskSitIdle);
	pCmdWindow->AddCmdProc("bench", cmdBench);
	pCmdWindow->AddCmdProc("disenex", cmdDisableEnEx);
	pCmdWindow->AddCmdProc("dbgpiss", cmdStartPiss);
	pCmdWindow->AddCmdProc("teamtest", cmdTeamTest);
	pCmdWindow->AddCmdProc("showmyvw", cmdShowMyVirtualWorld);
	pCmdWindow->AddCmdProc("showplayervw", cmdShowPlayerVirtualWorld);
#endif
}
