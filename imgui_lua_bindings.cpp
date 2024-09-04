#include "ui_local.h"
#include <imgui.h>

struct editbuf {
	char* buf;
	size_t len;
	lua_State* L;
};

static int editbuf_gc(lua_State* L) {
	editbuf* e = (editbuf*)lua_touserdata(L, 1);
	free(e->buf);
	e->buf = NULL;
	e->len = 0;
	return 0;
}

static int editbuf_tostring(lua_State* L) {
	editbuf* e = (editbuf*)lua_touserdata(L, 1);
	lua_pushstring(L, e->buf);
	return 1;
}

static void create_new_editbuf(lua_State* L, int argpos) {
	size_t len;
	const char* buf = luaL_checklstring(L, argpos, &len);
	if (buf == NULL) {
		len = 64;
	}
	else {
		len = len + 1;
	}

	editbuf* e = (editbuf*)lua_newuserdata(L, sizeof(struct editbuf));
	new (e) editbuf();
	e->buf = (char*)malloc(len);
	if (e->buf == NULL) {
		luaL_error(L, "Failed to allocate memory for editbuf");
		return;
	}
	e->len = len;
	if (buf) {
		memcpy(e->buf, buf, len);
	}
	else {
		e->buf[0] = NULL;
	}
	e->L = L;

	lua_newtable(L);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, editbuf_gc);
	lua_settable(L, -3);

	lua_pushstring(L, "__tostring");
	lua_pushcfunction(L, editbuf_tostring);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);
}

static int edit_callback(ImGuiInputTextCallbackData* data) {
	struct editbuf* e = (struct editbuf*)data->UserData;
	lua_State* L = e->L;

	switch (data->EventFlag)
	{
	case ImGuiInputTextFlags_CallbackResize:
	{
		size_t newsize = e->len;
		while (newsize < (size_t)data->BufSize) {
			newsize *= 2;
		}
		data->Buf = (char*)realloc(e->buf, newsize);
		if (data->Buf == NULL) {
			data->Buf = e->buf;
			data->BufTextLen = 0;
		}
		else {
			e->buf = data->Buf;
			e->len = newsize;
		}
		break;
	}
	default:
		break;
	}

	return 0;
}

std::deque<int> imGuiStack;
static void AddToImGuiStack(int type) {
	imGuiStack.push_back(type);
}

static void PopEndImGuiStack(int type) {
	if (!imGuiStack.empty()) {
		imGuiStack.pop_back(); // hopefully the type matches
	}
}

#define IMGUI_FUNCTION_DRAW_LIST(name) \
static int impl_draw_list_##name(lua_State *L) { \
  int max_args = lua_gettop(L); \
  int arg = 1; \
  int stackval = 0;

#define IMGUI_FUNCTION(name) \
static int impl_##name(lua_State *L) { \
  int max_args = lua_gettop(L); \
  int arg = 1; \
  int stackval = 0;

// I use OpenGL so this is a GLuint
// Using unsigned int cause im lazy don't copy me
#define IM_TEXTURE_ID_ARG(name) \
  const ImTextureID name = (ImTextureID)luaL_checkinteger(L, arg++);

#define OPTIONAL_LABEL_ARG(name) \
  const char* name; \
  if (arg <= max_args) { \
    name = lua_tostring(L, arg++); \
  } else { \
    name = NULL; \
  }

#define LABEL_ARG(name) \
  size_t i_##name##_size; \
  const char * name = luaL_checklstring(L, arg++, &(i_##name##_size));

#define IM_VEC_2_ARG(name)\
  const lua_Number i_##name##_x = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_y = luaL_checknumber(L, arg++); \
  const ImVec2 name((double)i_##name##_x, (double)i_##name##_y);

#define OPTIONAL_IM_VEC_2_ARG(name, x, y) \
  lua_Number i_##name##_x = x; \
  lua_Number i_##name##_y = y; \
  if (arg <= max_args - 1) { \
    i_##name##_x = luaL_checknumber(L, arg++); \
    i_##name##_y = luaL_checknumber(L, arg++); \
  } \
  const ImVec2 name((double)i_##name##_x, (double)i_##name##_y);

#define IM_VEC_4_ARG(name) \
  const lua_Number i_##name##_x = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_y = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_z = luaL_checknumber(L, arg++); \
  const lua_Number i_##name##_w = luaL_checknumber(L, arg++); \
  const ImVec4 name((double)i_##name##_x, (double)i_##name##_y, (double)i_##name##_z, (double)i_##name##_w);

#define OPTIONAL_IM_VEC_4_ARG(name, x, y, z, w) \
  lua_Number i_##name##_x = x; \
  lua_Number i_##name##_y = y; \
  lua_Number i_##name##_z = z; \
  lua_Number i_##name##_w = w; \
  if (arg <= max_args - 1) { \
    i_##name##_x = luaL_checknumber(L, arg++); \
    i_##name##_y = luaL_checknumber(L, arg++); \
    i_##name##_z = luaL_checknumber(L, arg++); \
    i_##name##_w = luaL_checknumber(L, arg++); \
  } \
  const ImVec4 name((double)i_##name##_x, (double)i_##name##_y, (double)i_##name##_z, (double)i_##name##_w);

#define NUMBER_ARG(name)\
  lua_Number name = luaL_checknumber(L, arg++);

#define OPTIONAL_NUMBER_ARG(name, otherwise)\
  lua_Number name = otherwise; \
  if (arg <= max_args) { \
    name = lua_tonumber(L, arg++); \
  }

#define FLOAT_POINTER_ARG(name) \
  float i_##name##_value = luaL_checknumber(L, arg++); \
  float* name = &(i_##name##_value);

#define END_FLOAT_POINTER(name) \
  if (name != NULL) { \
    lua_pushnumber(L, i_##name##_value); \
    stackval++; \
  }

#define OPTIONAL_INT_ARG(name, otherwise)\
  int name = otherwise; \
  if (arg <= max_args) { \
    name = (int)lua_tonumber(L, arg++); \
  }

#define INT_ARG(name) \
  const int name = (int)luaL_checknumber(L, arg++);

#define GUIKEY_ARG(name) \
  const ImGuiKey name = (ImGuiKey)luaL_checkinteger(L, arg++);

#define OPTIONAL_GUIKEY_ARG(name, otherwise)\
  ImGuiKey name = static_cast<ImGuiKey>(otherwise); \
  if (arg <= max_args) { \
    name = (ImGuiKey)lua_tonumber(L, arg++); \
  }

#define OPTIONAL_UINT_ARG(name, otherwise)\
  unsigned int name = otherwise; \
  if (arg <= max_args) { \
    name = (unsigned int)luaL_checkinteger(L, arg++); \
  }

#define UINT_ARG(name) \
  const unsigned int name = (unsigned int)luaL_checkinteger(L, arg++);

#define INT_POINTER_ARG(name) \
  int i_##name##_value = (int)luaL_checkinteger(L, arg++); \
  int* name = &(i_##name##_value);

#define END_INT_POINTER(name) \
  if (name != NULL) { \
    lua_pushnumber(L, i_##name##_value); \
    stackval++; \
  }

#define UINT_POINTER_ARG(name) \
  unsigned int i_##name##_value = (unsigned int)luaL_checkinteger(L, arg++); \
  unsigned int* name = &(i_##name##_value);

#define END_UINT_POINTER(name) \
  if (name != NULL) { \
    lua_pushnumber(L, i_##name##_value); \
    stackval++; \
  }

#define BOOL_POINTER_ARG(name) \
  bool i_##name##_value = lua_toboolean(L, arg++); \
  bool* name = &(i_##name##_value);

#define OPTIONAL_BOOL_POINTER_ARG(name) \
  bool i_##name##_value; \
  bool* name = NULL; \
  if (arg <= max_args) { \
    i_##name##_value = lua_toboolean(L, arg++); \
    name = &(i_##name##_value); \
  }

#define OPTIONAL_BOOL_ARG(name, otherwise) \
  bool name = otherwise; \
  if (arg <= max_args) { \
    name = lua_toboolean(L, arg++); \
  }

#define BOOL_ARG(name) \
  bool name = lua_toboolean(L, arg++);

#define CALL_FUNCTION(name, retType,...) \
  retType ret = ImGui::name(__VA_ARGS__);

#define DRAW_LIST_CALL_FUNCTION(name, retType,...) \
  retType ret = ImGui::GetWindowDrawList()->name(__VA_ARGS__);

#define CALL_FUNCTION_NO_RET(name, ...) \
  ImGui::name(__VA_ARGS__);

#define DRAW_LIST_CALL_FUNCTION_NO_RET(name, ...) \
  ImGui::GetWindowDrawList()->name(__VA_ARGS__);

#define PUSH_STRING(name) \
  lua_pushstring(L, name); \
  stackval++;

#define PUSH_NUMBER(name) \
  lua_pushnumber(L, name); \
  stackval++;

#define PUSH_BOOL(name) \
  lua_pushboolean(L, (int) name); \
  stackval++;

#define END_BOOL_POINTER(name) \
  if (name != NULL) { \
    lua_pushboolean(L, (int)i_##name##_value); \
    stackval++; \
  }

#define END_IMGUI_FUNC \
  return stackval; \
}

#define IF_RET_ADD_END_STACK(type) \
  if (ret) { \
    AddToImGuiStack(type); \
  }

#define ADD_END_STACK(type) \
  AddToImGuiStack(type);

#define POP_END_STACK(type) \
  PopEndImGuiStack(type);

#define END_STACK_START \
void ImEndStack(int type) { \
  switch(type) {

#define END_STACK_OPTION(type, function) \
    case type: \
      ImGui::function(); \
      break;

#define END_STACK_END \
  } \
}

#define START_ENUM(name)
#define MAKE_ENUM(c_name,lua_name)
#define END_ENUM(name)

#define EDITOR_ARG(name) \
	int typeArg_##name## = lua_type(L, arg); \
	int pos_##name## = arg; \
	if (typeArg_##name## == LUA_TSTRING || typeArg_##name## == LUA_TNIL) { \
		create_new_editbuf(L, arg); \
		lua_replace(L, arg); \
	} \
	else if (typeArg_##name## != LUA_TUSERDATA) { \
		lua_error(L); \
	} \
	struct editbuf* ##name## = (struct editbuf*)lua_touserdata(L, arg++);

#define END_EDITOR(name) \
	lua_pushvalue(L, pos_##name##);stackval++;

#include "imgui_iterator.inl"

IMGUI_FUNCTION(InputText)
LABEL_ARG(label)
EDITOR_ARG(e)
OPTIONAL_INT_ARG(flags, 0)
	flags |= ImGuiInputTextFlags_CallbackResize;
CALL_FUNCTION(InputText, bool, label, e->buf, e->len, flags, edit_callback, e)
PUSH_BOOL(ret)
END_EDITOR(e)
END_IMGUI_FUNC

IMGUI_FUNCTION(InputTextWithHint)
LABEL_ARG(label)
LABEL_ARG(hint)
EDITOR_ARG(e)
OPTIONAL_INT_ARG(flags, 0)
	flags |= ImGuiInputTextFlags_CallbackResize;
CALL_FUNCTION(InputTextWithHint, bool, label, hint, e->buf, e->len, flags, edit_callback, e)
PUSH_BOOL(ret)
END_EDITOR(e)
END_IMGUI_FUNC



static void PushImguiEnums(lua_State* lState, const char* tableName) {
	lua_pushstring(lState, tableName);
	lua_newtable(lState);

#undef START_ENUM
#undef MAKE_ENUM
#undef END_ENUM
#define START_ENUM(name) \
  lua_pushstring(lState, #name); \
  lua_newtable(lState); \
  { \
    int i = 1;
#define MAKE_ENUM(c_name,lua_name) \
  lua_pushstring(lState, #lua_name); \
  lua_pushnumber(lState, c_name); \
  lua_rawset(lState, -3);
#define END_ENUM(name) \
  } \
  lua_rawset(lState, -3);
	// These defines are just redefining everything to nothing so
	// we get only the enums.
#undef IMGUI_FUNCTION
#define IMGUI_FUNCTION(name)
#undef IMGUI_FUNCTION_DRAW_LIST
#define IMGUI_FUNCTION_DRAW_LIST(name)
#undef IM_TEXTURE_ID_ARG
#define IM_TEXTURE_ID_ARG(name)
#undef OPTIONAL_LABEL_ARG
#define OPTIONAL_LABEL_ARG(name)
#undef LABEL_ARG
#define LABEL_ARG(name)
#undef IM_VEC_2_ARG
#define IM_VEC_2_ARG(name)
#undef OPTIONAL_IM_VEC_2_ARG
#define OPTIONAL_IM_VEC_2_ARG(name, x, y)
#undef IM_VEC_4_ARG
#define IM_VEC_4_ARG(name)
#undef OPTIONAL_IM_VEC_4_ARG
#define OPTIONAL_IM_VEC_4_ARG(name, x, y, z, w)
#undef NUMBER_ARG
#define NUMBER_ARG(name)
#undef OPTIONAL_NUMBER_ARG
#define OPTIONAL_NUMBER_ARG(name, otherwise)
#undef FLOAT_POINTER_ARG
#define FLOAT_POINTER_ARG(name)
#undef END_FLOAT_POINTER
#define END_FLOAT_POINTER(name)
#undef OPTIONAL_INT_ARG
#define OPTIONAL_INT_ARG(name, otherwise)
#undef INT_ARG
#define INT_ARG(name)
#undef GUIKEY_ARG
#define GUIKEY_ARG(name)
#undef OPTIONAL_UINT_ARG
#define OPTIONAL_UINT_ARG(name, otherwise)
#undef UINT_ARG
#define UINT_ARG(name)
#undef INT_POINTER_ARG
#define INT_POINTER_ARG(name)
#undef END_INT_POINTER
#define END_INT_POINTER(name)
#undef UINT_POINTER_ARG
#define UINT_POINTER_ARG(name)
#undef END_UINT_POINTER
#define END_UINT_POINTER(name)
#undef BOOL_POINTER_ARG
#define BOOL_POINTER_ARG(name)
#undef OPTIONAL_BOOL_POINTER_ARG
#define OPTIONAL_BOOL_POINTER_ARG(name)
#undef OPTIONAL_BOOL_ARG
#define OPTIONAL_BOOL_ARG(name, otherwise)
#undef BOOL_ARG
#define BOOL_ARG(name)
#undef CALL_FUNCTION
#define CALL_FUNCTION(name, retType, ...)
#undef DRAW_LIST_CALL_FUNCTION
#define DRAW_LIST_CALL_FUNCTION(name, retType, ...)
#undef CALL_FUNCTION_NO_RET
#define CALL_FUNCTION_NO_RET(name, ...)
#undef DRAW_LIST_CALL_FUNCTION_NO_RET
#define DRAW_LIST_CALL_FUNCTION_NO_RET(name, ...)
#undef PUSH_STRING
#define PUSH_STRING(name)
#undef PUSH_NUMBER
#define PUSH_NUMBER(name)
#undef PUSH_BOOL
#define PUSH_BOOL(name)
#undef END_BOOL_POINTER
#define END_BOOL_POINTER(name)
#undef END_IMGUI_FUNC
#define END_IMGUI_FUNC
#undef END_STACK_START
#define END_STACK_START
#undef END_STACK_OPTION
#define END_STACK_OPTION(type, function)
#undef END_STACK_END
#define END_STACK_END
#undef IF_RET_ADD_END_STACK
#define IF_RET_ADD_END_STACK(type)
#undef ADD_END_STACK
#define ADD_END_STACK(type)
#undef POP_END_STACK
#define POP_END_STACK(type)

#include "imgui_iterator.inl"

    lua_pushstring(lState, "FLT_MIN");
	lua_pushnumber(lState, FLT_MIN);
    lua_rawset(lState, -3);

    lua_pushstring(lState, "FLT_MAX");
    lua_pushnumber(lState, FLT_MAX);
    lua_rawset(lState, -3);

	lua_rawset(lState, -3);
};

void PushFunctions(lua_State* L) {
#undef IMGUI_FUNCTION
#define IMGUI_FUNCTION(name) \
    lua_pushcclosure(L, impl_##name, 0);lua_setfield(L, -2, #name);
#undef IMGUI_FUNCTION_DRAW_LIST
#define IMGUI_FUNCTION_DRAW_LIST(name)  \
    lua_pushcclosure(L, impl_draw_list_##name, 0); lua_setfield(L, -2, "DrawList_" #name);
    // These defines are just redefining everything to nothing so
    // we can get the function names
#undef IM_TEXTURE_ID_ARG
#define IM_TEXTURE_ID_ARG(name)
#undef OPTIONAL_LABEL_ARG
#define OPTIONAL_LABEL_ARG(name)
#undef LABEL_ARG
#define LABEL_ARG(name)
#undef IM_VEC_2_ARG
#define IM_VEC_2_ARG(name)
#undef OPTIONAL_IM_VEC_2_ARG
#define OPTIONAL_IM_VEC_2_ARG(name, x, y)
#undef IM_VEC_4_ARG
#define IM_VEC_4_ARG(name)
#undef OPTIONAL_IM_VEC_4_ARG
#define OPTIONAL_IM_VEC_4_ARG(name, x, y, z, w)
#undef NUMBER_ARG
#define NUMBER_ARG(name)
#undef OPTIONAL_NUMBER_ARG
#define OPTIONAL_NUMBER_ARG(name, otherwise)
#undef FLOAT_POINTER_ARG
#define FLOAT_POINTER_ARG(name)
#undef END_FLOAT_POINTER
#define END_FLOAT_POINTER(name)
#undef OPTIONAL_INT_ARG
#define OPTIONAL_INT_ARG(name, otherwise)
#undef INT_ARG
#define INT_ARG(name)
#undef GUIKEY_ARG
#define GUIKEY_ARG(name)
#undef OPTIONAL_UINT_ARG
#define OPTIONAL_UINT_ARG(name, otherwise)
#undef UINT_ARG
#define UINT_ARG(name)
#undef INT_POINTER_ARG
#define INT_POINTER_ARG(name)
#undef END_INT_POINTER
#define END_INT_POINTER(name)
#undef UINT_POINTER_ARG
#define UINT_POINTER_ARG(name)
#undef END_UINT_POINTER
#define END_UINT_POINTER(name)
#undef BOOL_POINTER_ARG
#define BOOL_POINTER_ARG(name)
#undef OPTIONAL_BOOL_POINTER_ARG
#define OPTIONAL_BOOL_POINTER_ARG(name)
#undef OPTIONAL_BOOL_ARG
#define OPTIONAL_BOOL_ARG(name, otherwise)
#undef BOOL_ARG
#define BOOL_ARG(name)
#undef CALL_FUNCTION
#define CALL_FUNCTION(name, retType, ...)
#undef DRAW_LIST_CALL_FUNCTION
#define DRAW_LIST_CALL_FUNCTION(name, retType, ...)
#undef CALL_FUNCTION_NO_RET
#define CALL_FUNCTION_NO_RET(name, ...)
#undef DRAW_LIST_CALL_FUNCTION_NO_RET
#define DRAW_LIST_CALL_FUNCTION_NO_RET(name, ...)
#undef PUSH_STRING
#define PUSH_STRING(name)
#undef PUSH_NUMBER
#define PUSH_NUMBER(name)
#undef PUSH_BOOL
#define PUSH_BOOL(name)
#undef END_BOOL_POINTER
#define END_BOOL_POINTER(name)
#undef END_IMGUI_FUNC
#define END_IMGUI_FUNC
#undef END_STACK_START
#define END_STACK_START
#undef END_STACK_OPTION
#define END_STACK_OPTION(type, function)
#undef END_STACK_END
#define END_STACK_END
#undef IF_RET_ADD_END_STACK
#define IF_RET_ADD_END_STACK(type)
#undef ADD_END_STACK
#define ADD_END_STACK(type)
#undef POP_END_STACK
#define POP_END_STACK(type)
#undef START_ENUM
#define START_ENUM(name)
#undef MAKE_ENUM
#define MAKE_ENUM(c_name,lua_name)
#undef END_ENUM
#define END_ENUM(name)

#include "imgui_iterator.inl"
}


static int impl_clipper_begin(lua_State* L) {
	ImGuiListClipper* clipper = (ImGuiListClipper*)lua_touserdata(L, 1);

	IM_ASSERT(clipper != NULL);
	clipper->Begin(luaL_checkinteger(L, 2));
    return 0;
}

static int impl_clipper_step(lua_State* L) {
	ImGuiListClipper* clipper = (ImGuiListClipper*)lua_touserdata(L, 1);
	
    IM_ASSERT(clipper != NULL);
	bool step = clipper->Step();
	lua_pushboolean(L, step);

    return 1;
}

static int impl_clipper_index(lua_State* L) {
	ImGuiListClipper* clipper = (ImGuiListClipper*)lua_touserdata(L, 1);

	IM_ASSERT(clipper != NULL);
	const char* key = luaL_checkstring(L, 2);

	if (strcmp(key, "DisplayStart") == 0) {
		lua_pushinteger(L, clipper->DisplayStart);
	}
	else if (strcmp(key, "DisplayEnd") == 0) {
		lua_pushinteger(L, clipper->DisplayEnd);
	}
	else if (strcmp(key, "Begin") == 0) {
		lua_pushcfunction(L, impl_clipper_begin);
	}
	else if (strcmp(key, "Step") == 0) {
		lua_pushcfunction(L, impl_clipper_step);
	}
	else {
		lua_pushnil(L);
	}

	return 1;
}

static int impl_clipper(lua_State* L) {
	ImGuiListClipper* clipper = (ImGuiListClipper*)lua_newuserdata(L, sizeof(ImGuiListClipper));
	new (clipper) ImGuiListClipper();

	lua_newtable(L);
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, [](lua_State* L) {
		ImGuiListClipper* clipper = (ImGuiListClipper*)lua_touserdata(L, 1);
		clipper->~ImGuiListClipper();
		return 0;
	});
	lua_settable(L, -3);

	lua_pushstring(L, "__index");
	lua_pushcfunction(L, impl_clipper_index);
	lua_settable(L, -3);

	lua_setmetatable(L, -2);

	return 1;
}

void LoadImguiBindings(lua_State* L)
{
	if (!L) {
		fprintf(stderr, "You didn't assign the global lState, either assign that or refactor LoadImguiBindings and RunString\n");
	}
	lua_newtable(L);
	// luaL_setfuncs(L, imguilib, 0);
	PushFunctions(L);
	PushImguiEnums(L, "constant");

	lua_pushcclosure(L, impl_clipper, 0);
	lua_setfield(L, -2, "ListClipper");

	lua_pushcclosure(L, impl_InputText, 0);
	lua_setfield(L, -2, "InputText");

	lua_pushcclosure(L, impl_InputTextWithHint, 0);
	lua_setfield(L, -2, "InputTextWithHint");	

	lua_setglobal(L, "imgui");
}