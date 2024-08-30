// DyLua: SimpleGraphic
// (c) David Gowor, 2014
//
// UI Local Header
//

#include "common.h"
#include "system.h"
#include "render.h"
#include "core.h"

#include "ui.h"

extern "C" {
#ifdef _WIN32
#include <luajit/lua.h>
#include <luajit/lauxlib.h>
#include <luajit/lualib.h>
#include <luajit/luajit.h>
#else
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif
}

#include "ui_console.h"
#include "ui_debug.h"
#include "ui_subscript.h"

#include "ui_main.h"

// to keep track of end and begins and clean up the imgui stack
// if lua errors

#include <deque>
// Stack for imgui begin and end
extern std::deque<int> imGuiStack;
static void AddToImGuiStack(int type);
static void PopEndImGuiStack(int type);
void ImEndStack(int type);
// ImGui Binding Function definition
void LoadImguiBindings(lua_State* L);