#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H

#include "clearing.h"

void InitAtmosphere(void);
void UnloadAtmosphere(void);
bool UpdateAtmosphere(const Clearing *map, Camera2D camera, float dt);
bool MapHasAtmosphere(const Clearing *map);
void SetAtmosphereLighting(bool enabled);
void DrawAtmosphereBackground(const Clearing *map, int bossPhase);
void BeginAtmosphereActors(const Clearing *map);
void EndAtmosphereActors(void);
void DrawAtmosphereBelow(const Clearing *map);
void DrawAtmosphereShadow(const Clearing *map, Vector2 feet, float radius);
void DrawAtmosphereAbove(const Clearing *map);
void DrawAtmosphereGuides(const Clearing *map);
void DrawAtmospherePanel(const Clearing *map);

#endif
