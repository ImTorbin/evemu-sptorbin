/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2026 The EVEmu Team

    Central registration point for DevTools API route handlers.  Each
    Register<Group> function adds its routes to the supplied Router.
    ------------------------------------------------------------------------------------
*/

#ifndef __DEVTOOLS__HANDLERS__ALL_HANDLERS_H__INCL__
#define __DEVTOOLS__HANDLERS__ALL_HANDLERS_H__INCL__

namespace EvE { namespace Devtools { class Router; } }

namespace EvE {
namespace Devtools {
namespace Handlers {

void RegisterAll(Router& r);

void RegisterCore(Router& r);        // /healthz, /status, /auth/*
void RegisterOpenApi(Router& r);     // /api/v1/openapi.json
void RegisterDungeons(Router& r);    // /api/v1/dungeons/*
void RegisterMissions(Router& r);    // /api/v1/missions/*, /agents/*
void RegisterNpcs(Router& r);        // /api/v1/npc-classes/*, /npc-spawn-classes/*, /spawns/*
void RegisterControl(Router& r);     // /api/v1/control/*

} // namespace Handlers
} // namespace Devtools
} // namespace EvE

#endif // __DEVTOOLS__HANDLERS__ALL_HANDLERS_H__INCL__
