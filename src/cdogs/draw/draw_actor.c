/*
    C-Dogs SDL
    A port of the legendary (and fun) action/arcade cdogs.
    Copyright (C) 1995 Ronny Wester
    Copyright (C) 2003 Jeremy Chin 
    Copyright (C) 2003-2007 Lucas Martin-King 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    This file incorporates work covered by the following copyright and
    permission notice:

    Copyright (c) 2013-2019 Cong Xu
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
#include "draw/draw_actor.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "actors.h"
#include "algorithms.h"
#include "config.h"
#include "draw/drawtools.h"
#include "font.h"
#include "game_events.h"
#include "net_util.h"
#include "objs.h"
#include "pics.h"
#include "draw/draw.h"
#include "blit.h"
#include "pic_manager.h"

#define TRANSPARENT_ACTOR_ALPHA 64


static struct vec2i GetActorDrawOffset(
	const Pic *pic, const BodyPart part, const CharSprites *cs,
	const ActorAnimation anim, const int frame, const direction_e d)
{
	struct vec2i offset = svec2i_scale_divide(pic->size, -2);
	offset = svec2i_subtract(offset, CharSpritesGetOffset(
		cs->Offsets.Frame[part],
		anim == ACTORANIMATION_WALKING ? "run" : "idle",
		frame));
	offset = svec2i_add(offset, svec2i_assign_vec2(cs->Offsets.Dir[part][d]));
	return offset;
}

static Character *ActorGetCharacterMutable(TActor *a);
static direction_e GetLegDirAndFrame(
	const TActor *a, const direction_e bodyDir, int *frame);
ActorPics GetCharacterPicsFromActor(TActor *a)
{
	Character *c = ActorGetCharacterMutable(a);
	const Weapon *gun = ACTOR_GET_WEAPON(a);

	color_t mask;
	bool hasStatus = false;
	if (a->flamed)
	{
		mask = colorRed;
		hasStatus = true;
	}
	else if (a->poisoned)
	{
		mask = colorPoison;
		hasStatus = true;
	}
	else if (a->petrified)
	{
		mask = colorGray;
		hasStatus = true;
	}
	else if (a->confused)
	{
		mask = colorPurple;
		hasStatus = true;
	}

	const CharColors allBlack = CharColorsFromOneColor(colorBlack);
	const CharColors allWhite = CharColorsFromOneColor(colorWhite);
	const bool isTransparent = !!(a->flags & FLAGS_SEETHROUGH);
	const CharColors *colors = NULL;
	const color_t *maskP = NULL;
	if (isTransparent)
	{
		colors = &allBlack;
		maskP = &mask;
		mask.a = TRANSPARENT_ACTOR_ALPHA;
	}
	else if (hasStatus)
	{
		maskP = &mask;
		colors = &allWhite;
	}

	const direction_e dir = RadiansToDirection(a->DrawRadians);
	int frame;
	const direction_e legDir = GetLegDirAndFrame(a, dir, &frame);
	return GetCharacterPics(
		c, dir, legDir, a->anim.Type, frame,
		gun->Gun != NULL ? gun->Gun->Sprites : NULL, gun->state,
		!isTransparent, maskP, colors,
		a->dead);
}
static const Pic *GetBodyPic(
	PicManager *pm, const CharSprites *cs, const direction_e dir,
	const ActorAnimation anim, const int frame, const bool isArmed,
	const CharColors *colors);
static const Pic *GetLegsPic(
	PicManager *pm, const CharSprites *cs, const direction_e dir,
	const ActorAnimation anim, const int frame,
	const CharColors *colors);
static const Pic *GetGunPic(
	PicManager *pm, const char *gunSprites, const direction_e dir,
	const int gunState, const CharColors *colors);
static const Pic *GetDeathPic(PicManager *pm, const int frame);
ActorPics GetCharacterPics(
	const Character *c, const direction_e dir, const direction_e legDir,
	const ActorAnimation anim, const int frame,
	const char *gunSprites, const gunstate_e gunState,
	const bool hasShadow, const color_t *mask, const CharColors *colors,
	const int deadPic)
{
	ActorPics pics;
	memset(&pics, 0, sizeof pics);

	// Dummy return to handle invalid character class
	if (c->Class == NULL)
	{
		pics.IsDead = true;
		pics.IsDying = true;
		pics.Body = GetDeathPic(&gPicManager, 0);
		pics.OrderedPics[0] = pics.Body;
		return pics;
	}

	pics.HasShadow = hasShadow;

	// If the actor is dead, simply draw a dying animation
	pics.IsDead = deadPic > 0;
	if (pics.IsDead)
	{
		if (deadPic < DEATH_MAX)
		{
			pics.IsDying = true;
			pics.Body = GetDeathPic(&gPicManager, deadPic - 1);
			pics.OrderedPics[0] = pics.Body;
		}
		return pics;
	}

	if (mask != NULL)
	{
		pics.Mask = *mask;
	}
	else
	{
		pics.Mask = colorWhite;
	}
	if (colors == NULL)
	{
		colors = &c->Colors;
	}

	// Head
	direction_e headDir = dir;
	// If idle, turn head left/right on occasion
	if (anim == ACTORANIMATION_IDLE)
	{
		if (frame == IDLEHEAD_LEFT) headDir = (dir + 7) % 8;
		else if (frame == IDLEHEAD_RIGHT) headDir = (dir + 1) % 8;
	}
	pics.Head = GetHeadPic(c->Class, headDir, gunState, colors);
	pics.HeadOffset = GetActorDrawOffset(
		pics.Head, BODY_PART_HEAD, c->Class->Sprites, anim, frame, dir);

	// Gun
	pics.Gun = NULL;
	if (gunSprites != NULL)
	{
		pics.Gun = GetGunPic(&gPicManager, gunSprites, dir, gunState, colors);
		if (pics.Gun != NULL)
		{
			pics.GunOffset = GetActorDrawOffset(
				pics.Gun, BODY_PART_GUN, c->Class->Sprites, anim, frame, dir);
		}
	}
	const bool isArmed = pics.Gun != NULL;

	// Body
	pics.Body = GetBodyPic(
		&gPicManager, c->Class->Sprites, dir, anim, frame, isArmed,
		colors);
	pics.BodyOffset = GetActorDrawOffset(
		pics.Body, BODY_PART_BODY, c->Class->Sprites, anim, frame, dir);

	// Legs
	pics.Legs = GetLegsPic(
		&gPicManager, c->Class->Sprites, legDir, anim, frame, colors);
	pics.LegsOffset = GetActorDrawOffset(
		pics.Legs, BODY_PART_LEGS, c->Class->Sprites, anim, frame, legDir);

	// Determine draw order based on the direction the player is facing
	for (BodyPart bp = BODY_PART_HEAD; bp < BODY_PART_COUNT; bp++)
	{
		const BodyPart drawOrder = c->Class->Sprites->Order[dir][bp];
		switch (drawOrder)
		{
		case BODY_PART_HEAD:
			pics.OrderedPics[bp] = pics.Head;
			pics.OrderedOffsets[bp] = pics.HeadOffset;
			break;
		case BODY_PART_BODY:
			pics.OrderedPics[bp] = pics.Body;
			pics.OrderedOffsets[bp] = pics.BodyOffset;
			break;
		case BODY_PART_LEGS:
			pics.OrderedPics[bp] = pics.Legs;
			pics.OrderedOffsets[bp] = pics.LegsOffset;
			break;
		case BODY_PART_GUN:
			pics.OrderedPics[bp] = pics.Gun;
			pics.OrderedOffsets[bp] = pics.GunOffset;
			break;
		default:
			break;
		}
	}

	pics.Sprites = c->Class->Sprites;

	return pics;
}
static Character *ActorGetCharacterMutable(TActor *a)
{
	if (a->PlayerUID >= 0)
	{
		return &PlayerDataGetByUID(a->PlayerUID)->Char;
	}
	return CArrayGet(&gCampaign.Setting.characters.OtherChars, a->charId);
}
static direction_e GetLegDirAndFrame(
	const TActor *a, const direction_e bodyDir, int *frame)
{
	*frame = AnimationGetFrame(&a->anim);
	const struct vec2 vel = svec2_add(a->MoveVel, a->thing.Vel);
	if (svec2_is_zero(vel))
	{
		return bodyDir;
	}
	const direction_e legDir = RadiansToDirection(svec2_angle(vel) + MPI_2);
	// Walk backwards if the leg dir is >90 degrees from body dir
	const int dirDiff = abs((int)bodyDir - (int)legDir);
	const bool reversed = dirDiff > 2 && dirDiff < 6;
	if (reversed)
	{
		*frame = ANIMATION_MAX_FRAMES - *frame;
		return DirectionOpposite(legDir);
	}
	return legDir;
}

static void DrawDyingBody(
	GraphicsDevice *g, const ActorPics *pics, const struct vec2i pos);
void DrawActorPics(
	const ActorPics *pics, const struct vec2i pos, const bool blit)
{
	if (pics->IsDead)
	{
		if (pics->IsDying)
		{
			DrawDyingBody(&gGraphicsDevice, pics, pos);
		}
	}
	else
	{
		// Draw shadow
		if (pics->HasShadow)
		{
			DrawShadow(&gGraphicsDevice, pos, svec2i(8, 6));
		}
		for (int i = 0; i < BODY_PART_COUNT; i++)
		{
			const Pic *pic = pics->OrderedPics[i];
			if (pic == NULL)
			{
				continue;
			}
			const struct vec2i drawPos = svec2i_add(pos, pics->OrderedOffsets[i]);
			if (blit)
			{
				// TODO: deprecated
				Blit(&gGraphicsDevice, pic, drawPos);
			}
			else
			{
				PicRender(
					pic, gGraphicsDevice.gameWindow.renderer, drawPos,
					pics->Mask, 0, svec2_one());
			}
		}
	}
}
static void DrawLaserSightSingle(
	const struct vec2i from, const float radians, const int range,
	const color_t color);
void DrawLaserSight(
	const ActorPics *pics, const TActor *a, const struct vec2i picPos)
{
	// Don't draw if dead or transparent
	if (pics->IsDead || !pics->HasShadow) return;
	// Check config
	const LaserSight ls = ConfigGetEnum(&gConfig, "Game.LaserSight");
	if (ls != LASER_SIGHT_ALL &&
		!(ls == LASER_SIGHT_PLAYERS && a->PlayerUID >= 0))
	{
		return;
	}
	// Draw weapon indicators
	const WeaponClass *wc = ACTOR_GET_WEAPON(a)->Gun;
	struct vec2i muzzlePos = svec2i_add(picPos, svec2i_assign_vec2(ActorGetMuzzleOffset(a, wc)));
	muzzlePos.y -= wc->MuzzleHeight / Z_FACTOR;
	const float radians = dir2radians[a->direction] + wc->AngleOffset;
	const int range = (int)WeaponClassGetRange(wc);
	color_t color = colorCyan;
	color.a = 64;
	const float spreadHalf =
		(wc->Spread.Count - 1) * wc->Spread.Width / 2 + wc->Recoil / 2;
	if (spreadHalf > 0)
	{
		DrawLaserSightSingle(muzzlePos, radians - spreadHalf, range, color);
		DrawLaserSightSingle(muzzlePos, radians + spreadHalf, range, color);
	}
	else
	{
		DrawLaserSightSingle(muzzlePos, radians, range, color);
	}
}
static void DrawLaserSightSingle(
	const struct vec2i from, const float radians, const int range,
	const color_t color)
{
	const struct vec2 v = svec2_scale(
		Vec2FromRadiansScaled(radians), (float)range);
	const struct vec2i to = svec2i_add(from, svec2i_assign_vec2(v));
	DrawLine(from, to, color);
}

void DrawActorHighlight(
	const ActorPics *pics, const struct vec2i pos, const color_t color)
{
	// Do not highlight dead, dying or transparent characters
	if (pics->IsDead || !pics->HasShadow)
	{
		return;
	}
	BlitPicHighlight(
		&gGraphicsDevice, pics->Head, svec2i_add(pos, pics->HeadOffset), color);
	if (pics->Body != NULL)
	{
		BlitPicHighlight(
			&gGraphicsDevice, pics->Body, svec2i_add(pos, pics->BodyOffset),
			color);
	}
	if (pics->Legs != NULL)
	{
		BlitPicHighlight(
			&gGraphicsDevice, pics->Legs, svec2i_add(pos, pics->LegsOffset),
			color);
	}
	if (pics->Gun != NULL)
	{
		BlitPicHighlight(
			&gGraphicsDevice, pics->Gun, svec2i_add(pos, pics->GunOffset), color);
	}
}

static void DrawChatter(
	const Thing *ti, DrawBuffer *b, const struct vec2i offset);
void DrawChatters(DrawBuffer *b, const struct vec2i offset)
{
	const Tile *tile = &b->tiles[0][0];
	for (int y = 0; y < Y_TILES; y++)
	{
		for (int x = 0; x < b->Size.x; x++, tile++)
		{
			CA_FOREACH(ThingId, tid, tile->things)
				const Thing *ti = ThingIdGetThing(tid);
				if (ti->kind != KIND_CHARACTER)
				{
					continue;
				}
				DrawChatter(ti, b, offset);
			CA_FOREACH_END()
		}
		tile += X_TILES - b->Size.x;
	}
}
#define ACTOR_HEIGHT 25
static void DrawChatter(
	const Thing *ti, DrawBuffer *b, const struct vec2i offset)
{
	if (!ConfigGetBool(&gConfig, "Graphics.ShowHUD"))
	{
		return;
	}

	const TActor *a = CArrayGet(&gActors, ti->id);
	// Draw character text
	if (strlen(a->Chatter) > 0)
	{
		const struct vec2i textPos = svec2i(
			(int)a->thing.Pos.x - b->xTop + offset.x -
			FontStrW(a->Chatter) / 2,
			(int)a->thing.Pos.y - b->yTop + offset.y - ACTOR_HEIGHT);
		FontStr(a->Chatter, textPos);
	}
}

const Pic *GetHeadPic(
	const CharacterClass *c, const direction_e dir, const gunstate_e gunState,
	const CharColors *colors)
{
	// If firing, draw the firing head pic
	const int row =
		(gunState == GUNSTATE_FIRING || gunState == GUNSTATE_RECOIL) ? 1 : 0;
	const int idx = (int)dir + row * 8;
	// Get or generate masked sprites
	const NamedSprites *ns = PicManagerGetCharSprites(
		&gPicManager, c->HeadSprites, colors);
	return CArrayGet(&ns->pics, idx);
}
static const Pic *GetBodyPic(
	PicManager *pm, const CharSprites *cs, const direction_e dir,
	const ActorAnimation anim, const int frame, const bool isArmed,
	const CharColors *colors)
{
	const int stride = anim == ACTORANIMATION_IDLE ? 1 : 8;
	const int col = frame % stride;
	const int row = (int)dir;
	const int idx = col + row * stride;
	char buf[CDOGS_PATH_MAX];
	sprintf(
		buf, "chars/bodies/%s/upper_%s%s",
		cs->Name,
		anim == ACTORANIMATION_IDLE ? "idle" : "run",
		isArmed ? "_handgun" : "");	// TODO: other gun holding poses
	// Get or generate masked sprites
	const NamedSprites *ns = PicManagerGetCharSprites(pm, buf, colors);
	return CArrayGet(&ns->pics, idx);
}
static const Pic *GetLegsPic(
	PicManager *pm, const CharSprites *cs, const direction_e dir,
	const ActorAnimation anim, const int frame,
	const CharColors *colors)
{
	const int stride = anim == ACTORANIMATION_IDLE ? 1 : 8;
	const int col = frame % stride;
	const int row = (int)dir;
	const int idx = col + row * stride;
	char buf[CDOGS_PATH_MAX];
	sprintf(
		buf, "chars/bodies/%s/legs_%s",
		cs->Name, anim == ACTORANIMATION_IDLE ? "idle" : "run");
	// Get or generate masked sprites
	const NamedSprites *ns = PicManagerGetCharSprites(pm, buf, colors);
	return CArrayGet(&ns->pics, idx);
}
static const Pic *GetGunPic(
	PicManager *pm, const char *gunSprites, const direction_e dir,
	const int gunState, const CharColors *colors)
{
	const int idx = (gunState == GUNSTATE_READY ? 8 : 0) + dir;
	// Get or generate masked sprites
	const NamedSprites *ns = PicManagerGetCharSprites(
		pm, gunSprites, colors);
	if (ns == NULL)
	{
		return NULL;
	}
	return CArrayGet(&ns->pics, idx);
}
static const Pic *GetDeathPic(PicManager *pm, const int frame)
{
	return CArrayGet(&PicManagerGetSprites(pm, "chars/death")->pics, frame);
}

void DrawCharacterSimple(
	const Character *c, const struct vec2i pos, const direction_e d,
	const bool hilite, const bool showGun, const bool blit)
{
	ActorPics pics = GetCharacterPics(
		c, d, d, ACTORANIMATION_IDLE, 0, NULL, GUNSTATE_READY,
		true, NULL, NULL, 0);
	DrawActorPics(&pics, pos, blit);
	if (hilite)
	{
		FontCh('>', svec2i_add(pos, svec2i(-8, -16)));
		if (showGun)
		{
			FontStr(c->Gun->name, svec2i_add(pos, svec2i(-8, 8)));
		}
	}
}

void DrawHead(
	SDL_Renderer *renderer, const Character *c, const direction_e dir,
	const struct vec2i pos)
{
	const Pic *head = GetHeadPic(c->Class, dir, GUNSTATE_READY, &c->Colors);
	const struct vec2i drawPos = svec2i_subtract(pos, svec2i(
		head->size.x / 2, head->size.y / 2));
	PicRender(head, renderer, drawPos, colorWhite, 0, svec2_one());
}
#define DYING_BODY_OFFSET 3
static void DrawDyingBody(
	GraphicsDevice *g, const ActorPics *pics, const struct vec2i pos)
{
	const Pic *body = pics->Body;
	const struct vec2i drawPos = svec2i_subtract(pos, svec2i(
		body->size.x / 2, body->size.y / 2 + DYING_BODY_OFFSET));
	PicRender(
		body, g->gameWindow.renderer, drawPos, pics->Mask, 0, svec2_one());
}
