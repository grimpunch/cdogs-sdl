/*
    C-Dogs SDL
    A port of the legendary (and fun) action/arcade cdogs.
    Copyright (c) 2014-2016, 2018 Cong Xu
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/
#include "player.h"

#include "actors.h"
#include "events.h"
#include "log.h"
#include "net_client.h"
#include "player_template.h"


CArray gPlayerDatas;


void PlayerDataInit(CArray *p)
{
	CArrayInit(p, sizeof(PlayerData));
}

void PlayerDataAddOrUpdate(const NPlayerData pd)
{
	PlayerData *p = PlayerDataGetByUID(pd.UID);
	if (p == NULL)
	{
		PlayerData pNew;
		memset(&pNew, 0, sizeof pNew);
		CArrayPushBack(&gPlayerDatas, &pNew);
		p = CArrayGet(&gPlayerDatas, (int)gPlayerDatas.size - 1);

		// Set defaults
		p->ActorUID = -1;
		p->IsLocal =
			(int)pd.UID >= gNetClient.FirstPlayerUID &&
			(int)pd.UID < gNetClient.FirstPlayerUID + MAX_LOCAL_PLAYERS;
		p->inputDevice = INPUT_DEVICE_UNSET;

		p->Char.speed = 1;

		LOG(LM_MAIN, LL_INFO, "add default player UID(%u) local(%s)",
			pd.UID, p->IsLocal ? "true" : "false");
	}

	p->UID = pd.UID;

	strcpy(p->name, pd.Name);
	p->Char.Class = StrCharacterClass(pd.CharacterClass);
	if (p->Char.Class == NULL)
	{
		p->Char.Class = StrCharacterClass("Jones");
	}
	p->Char.Colors = Net2CharColors(pd.Colors);
	for (int i = 0; i < (int)pd.Weapons_count; i++)
	{
		p->guns[i] = NULL;
		if (strlen(pd.Weapons[i]) > 0)
		{
			const WeaponClass *wc = StrWeaponClass(pd.Weapons[i]);
			p->guns[i] = wc;
		}
	}
	p->Lives = pd.Lives;
	p->Stats = pd.Stats;
	p->Totals = pd.Totals;
	p->Char.maxHealth = pd.MaxHealth;
	p->lastMission = pd.LastMission;

	// Ready players as well
	p->Ready = true;

	LOG(LM_MAIN, LL_INFO, "update player UID(%d) maxHealth(%d)",
		p->UID, p->Char.maxHealth);
}

static void PlayerTerminate(PlayerData *p);
void PlayerRemove(const int uid)
{
	// Find the player so we can remove by index
	PlayerData *p = NULL;
	int i;
	for (i = 0; i < (int)gPlayerDatas.size; i++)
	{
		PlayerData *pi = CArrayGet(&gPlayerDatas, i);
		if (pi->UID == uid)
		{
			p = pi;
			break;
		}
	}
	if (p == NULL)
	{
		return;
	}
	if (p->ActorUID >= 0)
	{
		ActorDestroy(ActorGetByUID(p->ActorUID));
	}
	PlayerTerminate(p);
	CArrayDelete(&gPlayerDatas, i);

	LOG(LM_MAIN, LL_INFO, "remove player UID(%d)", uid);
}

static void PlayerTerminate(PlayerData *p)
{
	CFREE(p->Char.bot);
}

NPlayerData PlayerDataDefault(const int idx)
{
	NPlayerData pd = NPlayerData_init_default;

	// load from template if available
	const PlayerTemplate *t = PlayerTemplateGetById(&gPlayerTemplates, idx);
	if (t != NULL)
	{
		strcpy(pd.Name, t->name);
		strcpy(pd.CharacterClass, t->CharClassName);
		pd.Colors = CharColors2Net(t->Colors);
	}
	else
	{
		switch (idx)
		{
		case 0:
			strcpy(pd.Name, "Jones");
			strcpy(pd.CharacterClass, "Jones");
			pd.Colors.Skin = Color2Net(colorSkin);
			pd.Colors.Arms = Color2Net(colorLightBlue);
			pd.Colors.Body = Color2Net(colorLightBlue);
			pd.Colors.Legs = Color2Net(colorLightBlue);
			pd.Colors.Hair = Color2Net(colorLightBlue);
			break;
		case 1:
			strcpy(pd.Name, "Ice");
			strcpy(pd.CharacterClass, "Ice");
			pd.Colors.Skin = Color2Net(colorDarkSkin);
			pd.Colors.Arms = Color2Net(colorRed);
			pd.Colors.Body = Color2Net(colorRed);
			pd.Colors.Legs = Color2Net(colorRed);
			pd.Colors.Hair = Color2Net(colorBlack);
			break;
		case 2:
			strcpy(pd.Name, "Warbaby");
			strcpy(pd.CharacterClass, "WarBaby");
			pd.Colors.Skin = Color2Net(colorSkin);
			pd.Colors.Arms = Color2Net(colorGreen);
			pd.Colors.Body = Color2Net(colorGreen);
			pd.Colors.Legs = Color2Net(colorGreen);
			pd.Colors.Hair = Color2Net(colorRed);
			break;
		case 3:
			strcpy(pd.Name, "Han");
			strcpy(pd.CharacterClass, "Dragon");
			pd.Colors.Skin = Color2Net(colorAsianSkin);
			pd.Colors.Arms = Color2Net(colorYellow);
			pd.Colors.Body = Color2Net(colorYellow);
			pd.Colors.Legs = Color2Net(colorYellow);
			pd.Colors.Hair = Color2Net(colorYellow);
			break;
		default:
			// Set up player N template
			sprintf(pd.Name, "Player %d", idx);
			strcpy(pd.CharacterClass, "Jones");
			pd.Colors.Skin = Color2Net(colorSkin);
			pd.Colors.Arms = Color2Net(colorLightBlue);
			pd.Colors.Body = Color2Net(colorLightBlue);
			pd.Colors.Legs = Color2Net(colorLightBlue);
			pd.Colors.Hair = Color2Net(colorLightBlue);
			break;
		}
	}

	pd.MaxHealth = 200;

	return pd;
}

NPlayerData PlayerDataMissionReset(const PlayerData *p)
{
	NPlayerData pd = NMakePlayerData(p);
	pd.Lives = ModeLives(gCampaign.Entry.Mode);

	memset(&pd.Stats, 0, sizeof pd.Stats);

	pd.LastMission = gCampaign.MissionIndex;
	pd.MaxHealth = ModeMaxHealth(gCampaign.Entry.Mode);
	return pd;
}

void PlayerDataTerminate(CArray *p)
{
	for (int i = 0; i < (int)p->size; i++)
	{
		PlayerTerminate(CArrayGet(p, i));
	}
	CArrayTerminate(p);
}

PlayerData *PlayerDataGetByUID(const int uid)
{
	if (uid == -1)
	{
		return NULL;
	}
	CA_FOREACH(PlayerData, p, gPlayerDatas)
		if (p->UID == uid) return p;
	CA_FOREACH_END()
	return NULL;
}

int GetNumPlayers(
	const PlayerAliveOptions alive, const bool human, const bool local)
{
	int numPlayers = 0;
	CA_FOREACH(const PlayerData, p, gPlayerDatas)
		bool life = false;
		switch (alive)
		{
		case PLAYER_ANY: life = true; break;
		case PLAYER_ALIVE: life = IsPlayerAlive(p); break;
		case PLAYER_ALIVE_OR_DYING: life = IsPlayerAliveOrDying(p); break;
		}
		if (life &&
			(!human || p->inputDevice != INPUT_DEVICE_AI) &&
			(!local || p->IsLocal))
		{
			numPlayers++;
		}
	CA_FOREACH_END()
	return numPlayers;
}

bool AreAllPlayersDeadAndNoLives(void)
{
	CA_FOREACH(const PlayerData, p, gPlayerDatas)
		if (IsPlayerAlive(p) || p->Lives > 0)
		{
			return false;
		}
	CA_FOREACH_END()
	return true;
}

const PlayerData *GetFirstPlayer(
	const bool alive, const bool human, const bool local)
{
	CA_FOREACH(const PlayerData, p, gPlayerDatas)
		if ((!alive || IsPlayerAliveOrDying(p)) &&
			(!human || p->inputDevice != INPUT_DEVICE_AI) &&
			(!local || p->IsLocal))
		{
			return p;
		}
	CA_FOREACH_END()
	return NULL;
}

bool IsPlayerAlive(const PlayerData *player)
{
	if (player == NULL || player->ActorUID == -1)
	{
		return false;
	}
	const TActor *p = ActorGetByUID(player->ActorUID);
	return !p->dead;
}
bool IsPlayerHuman(const PlayerData *player)
{
	return player->inputDevice != INPUT_DEVICE_AI;
}
bool IsPlayerHumanAndAlive(const PlayerData *player)
{
	return IsPlayerAlive(player) && IsPlayerHuman(player);
}
bool IsPlayerAliveOrDying(const PlayerData *player)
{
	if (player->ActorUID == -1)
	{
		return false;
	}
	const TActor *p = ActorGetByUID(player->ActorUID);
	return p->dead <= DEATH_MAX;
}
bool IsPlayerScreen(const PlayerData *p)
{
	const bool humanOnly =
		IsPVP(gCampaign.Entry.Mode) ||
		!ConfigGetBool(&gConfig, "Interface.SplitscreenAI");
	const bool humanOrScreen = !humanOnly || p->inputDevice != INPUT_DEVICE_AI;
	return p->IsLocal && humanOrScreen && IsPlayerAliveOrDying(p);
}

struct vec2 PlayersGetMidpoint(void)
{
	// for all surviving players, find bounding rectangle, and get center
	struct vec2 min, max;
	PlayersGetBoundingRectangle(&min, &max);
	return svec2_scale(svec2_add(min, max), 0.5f);
}

void PlayersGetBoundingRectangle(struct vec2 *min, struct vec2 *max)
{
	bool isFirst = true;
	*min = svec2_zero();
	*max = svec2_zero();
	const bool humansOnly =
		GetNumPlayers(PLAYER_ALIVE_OR_DYING, true, false) > 0;
	CA_FOREACH(const PlayerData, p, gPlayerDatas)
		if (!p->IsLocal)
		{
			continue;
		}
		if (humansOnly ? IsPlayerHumanAndAlive(p) : IsPlayerAlive(p))
		{
			const TActor *player = ActorGetByUID(p->ActorUID);
			const Thing *ti = &player->thing;
			if (isFirst)
			{
				*min = *max = ti->Pos;
			}
			else
			{
				min->x = MIN(ti->Pos.x, min->x);
				min->y = MIN(ti->Pos.y, min->y);
				max->x = MAX(ti->Pos.x, max->x);
				max->y = MAX(ti->Pos.y, max->y);
			}
			isFirst = false;
		}
	CA_FOREACH_END()
}

// Get the number of players that use this ammo
int PlayersNumUseAmmo(const int ammoId)
{
	int numPlayersWithAmmo = 0;
	CA_FOREACH(const PlayerData, p, gPlayerDatas)
		if (!IsPlayerAlive(p))
		{
			continue;
		}
		const TActor *player = ActorGetByUID(p->ActorUID);
		for (int j = 0; j < MAX_WEAPONS; j++)
		{
			const Weapon *w = &player->guns[j];
			if (w->Gun != NULL && w->Gun->AmmoId == ammoId)
			{
				numPlayersWithAmmo++;
			}
		}
	CA_FOREACH_END()
	return numPlayersWithAmmo;
}

bool PlayerIsLocal(const int uid)
{
	const PlayerData *p = PlayerDataGetByUID(uid);
	return p != NULL && p->IsLocal;
}

void PlayerScore(PlayerData *p, const int points)
{
	if (p == NULL)
	{
		return;
	}
	p->Stats.Score += points;
	p->Totals.Score += points;
}

bool PlayerTrySetInputDevice(
	PlayerData *p, const input_device_e d, const int idx)
{
	if (p->inputDevice == d && p->deviceIndex == idx)
	{
		return false;
	}
	p->inputDevice = d;
	p->deviceIndex = idx;
	LOG(LM_MAIN, LL_DEBUG, "playerUID(%d) assigned input device(%d %d)",
		p->UID, (int)d, idx);
	return true;
}

bool PlayerTrySetUnusedInputDevice(
	PlayerData *p, const input_device_e d, const int idx)
{
	// Check that player's input device is unassigned
	if (p->inputDevice != INPUT_DEVICE_UNSET) return false;
	// Check that no players use this input device
	CA_FOREACH(const PlayerData, pOther, gPlayerDatas)
		if (pOther->inputDevice == d && pOther->deviceIndex == idx)
		{
			return false;
		}
	CA_FOREACH_END()
	return PlayerTrySetInputDevice(p, d, idx);
}

int PlayerGetNumWeapons(const PlayerData *p)
{
	int count = 0;
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if (p->guns[i] != NULL)
		{
			count++;
		}
	}
	return count;
}


bool PlayerHasGrenadeButton(const PlayerData *p)
{
	return InputHasGrenadeButton(p->inputDevice, p->deviceIndex);
}
