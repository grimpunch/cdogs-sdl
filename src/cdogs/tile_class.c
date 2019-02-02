/*
    C-Dogs SDL
    A port of the legendary (and fun) action/arcade cdogs.
    Copyright (c) 2018-2019 Cong Xu
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
#include "tile_class.h"

#include "log.h"
#include "sys_config.h"


TileClasses gTileClasses;
TileClass gTileFloor = {
	"tile", NULL, NULL, NULL, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
	true, false, false, false, TILE_CLASS_FLOOR,
};
TileClass gTileRoom = {
	"tile", NULL, NULL, NULL, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
	true, false, false, true, TILE_CLASS_FLOOR,
};
TileClass gTileWall = {
	"wall", NULL, NULL, NULL, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
	false, true, true, false, TILE_CLASS_WALL,
};
TileClass gTileNothing = {
	NULL, NULL, NULL, NULL, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
	false, false, false, false, TILE_CLASS_NOTHING,
};
TileClass gTileExit = {
	"exits", NULL, NULL, NULL, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
	true, false, false, false, TILE_CLASS_FLOOR,
};
TileClass gTileDoor = {
	"door", NULL, NULL, NULL, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
	false, true, true, true, TILE_CLASS_DOOR,
};

void TileClassesInit(TileClasses *c)
{
	c->classes = hashmap_new();
	c->customClasses = hashmap_new();
}
void TileClassesClearCustom(TileClasses *c)
{
	TileClassesTerminate(c);
	TileClassesInit(c);
}
static void TileClassDestroy(any_t data);
void TileClassesTerminate(TileClasses *c)
{
	hashmap_destroy(c->classes, TileClassDestroy);
	hashmap_destroy(c->customClasses, TileClassDestroy);
}
static void TileClassDestroy(any_t data)
{
	TileClass *c = data;
	CFREE(c->Name);
	CFREE(c->Style);
	CFREE(c->StyleType);
	CFREE(c);
}

const TileClass *StrTileClass(const char *name)
{
	if (name == NULL || strlen(name) == 0)
	{
		return &gTileNothing;
	}
	TileClass *t;
	int error = hashmap_get(gTileClasses.customClasses, name, (any_t *)&t);
	if (error == MAP_OK)
	{
		return t;
	}
	error = hashmap_get(gTileClasses.classes, name, (any_t *)&t);
	if (error == MAP_OK)
	{
		return t;
	}
	LOG(LM_MAIN, LL_ERROR, "failed to get tile class %s: %d",
		name, error);
	return &gTileNothing;
}

const TileClass *TileClassesGetMaskedTile(
	const TileClass *baseClass, const char *style, const char *type,
	const color_t mask, const color_t maskAlt)
{
	char buf[256];
	TileClassGetName(buf, baseClass->Name, style, type, mask, maskAlt);
	return StrTileClass(buf);
}
TileClass *TileClassesAdd(
	TileClasses *c, const PicManager *pm, const TileClass *baseClass,
	const char *style, const char *type,
	const color_t mask, const color_t maskAlt)
{
	TileClass *t;
	CCALLOC(t, sizeof *t);
	memcpy(t, baseClass, sizeof *t);
	CSTRDUP(t->Name, baseClass->Name);
	CSTRDUP(t->Style, style);
	CSTRDUP(t->StyleType, type);
	char buf[CDOGS_PATH_MAX];
	TileClassGetName(buf, baseClass->Name, style, type, mask, maskAlt);
	t->Pic = PicManagerGetPic(pm, buf);

	const int error = hashmap_put(c->customClasses, buf, t);
	if (error != MAP_OK)
	{
		LOG(LM_MAIN, LL_ERROR, "failed to add tile class %s: %d", buf, error);
		return NULL;
	}
	return t;
}
void TileClassGetName(
	char *buf, const char *name, const char *style, const char *type,
	const color_t mask, const color_t maskAlt)
{
	char maskName[16];
	ColorStr(maskName, mask);
	char maskAltName[16];
	ColorStr(maskAltName, maskAlt);
	sprintf(buf, "%s/%s/%s/%s/%s", name, style, type, maskName, maskAltName);
}

const TileClass *TileClassesGetExit(
	TileClasses *c, PicManager *pm, const char *style, const bool isShadow)
{
	char buf[256];
	const char *type = isShadow ? "shadow" : "normal";
	TileClassGetName(buf, "exits", style, type, colorWhite, colorWhite);
	const TileClass *t = StrTileClass(buf);
	if (t != &gTileNothing)
	{
		return t;
	}

	// tile class not found; create it
	PicManagerGenerateMaskedStylePic(
		pm, "exits", style, type, colorWhite, colorWhite);
	return TileClassesAdd(
		c, pm, &gTileExit, style, type, colorWhite, colorWhite);
}
