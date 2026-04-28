#include "../lua.h"
#include "../lauxlib.h"
#include "../lualib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

lua_State *L;

static void stackDump(lua_State *L)
{
    int i;
    int top = lua_gettop(L); // 스택 최상단 인덱스
    printf("--- Stack Top: %d ---\n", top);
    for (i = 1; i <= top; i++) { // 하단에서 상단으로 스캔
        int t = lua_type(L, i);
        switch (t)
        {
        case LUA_TSTRING:
            printf("'%s'", lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;
        case LUA_TNUMBER:
            printf("%g", lua_tonumber(L, i));
            break;
        default:
            printf("%s", lua_typename(L, t));
            break;
        }
        printf("  ");
    }
    printf("\n---------------------\n");
}

#define TNIL         0
#define TBOOLEAN     1
#define TNUMBER      2
#define TSTRING      3
#define TTABLE       4
#define TFUNCTION    5
#define TUNSUPPORTED -1
typedef int ValType;

typedef struct Val Val;

typedef struct Vals {
    Val *data;
    size_t cnt;
    size_t cap;
} Vals;

typedef struct {
    int ref;
    Vals keys;
    Vals values;
} Table;

struct Val {
    ValType type;
    union {
        int boolean;
        lua_Number number;
        char *string;
        Table table;
        int function;
    } as;
};

// Val
const int32_t sizeof_val = sizeof(Val);
const int32_t offsetof_val_type = offsetof(Val, type);
const int32_t offsetof_val_as = offsetof(Val, as);
// ValType
const int32_t sizeof_val_type = sizeof(ValType);
// Table
const int32_t sizeof_table = sizeof(Table);
const int32_t offsetof_table_ref = offsetof(Table, ref);
const int32_t offsetof_table_keys = offsetof(Table, keys);
const int32_t offsetof_table_values = offsetof(Table, values);
// primitive types
const int32_t sizeof_int = sizeof(int);
const int32_t sizeof_size_t = sizeof(size_t);

static int push_val(Vals *vals, Val *v) {
    if (vals->cap <= vals->cnt) {
        size_t new_cap = vals->cap ? vals->cap*2 : 16;
        if (new_cap < vals->cap) {
            perror("cap overflow");
            return 1;
        }
        if (new_cap > SIZE_MAX / sizeof(Val)) {
            perror("allocation size overflow");
            return 1;
        }
        Val* new_data = realloc(vals->data,sizeof(Val)*new_cap);
        if (new_data) {
            vals->data=new_data;
            vals->cap=new_cap;
        } else {
            perror("OOM error");
            return 1;
        }
    }
    vals->data[vals->cnt++]=*v;
    return 0;
}

static int push_table_val(Vals *vals, Table *t) {
    Val val = {0};
    val.type = TTABLE;
    val.as.table = *t;
    return push_val(vals,&val);
}

static int push_string_val(Vals *vals, const char* s) {
    Val val = {0};
    val.type = TSTRING;
    val.as.string = strdup(s);
    return push_val(vals,&val);
}

static int push_int_val(Vals *vals, int a) {
    Val val = {0};
    val.type = TNUMBER;
    val.as.number = a;
    return push_val(vals,&val);
}

static int l_get_ref(lua_State *L) {
    int ref = luaL_checkinteger(L, 1);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

    if (lua_isnil(L, -1)) {
        return luaL_error(L, "invalid registry ref: %d", ref);
    }

    return 1;
}

int lua_vm_init()
{
    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dostring(L, "print('LuaVM ready')");
    lua_pushcfunction(L, l_get_ref);
    lua_setglobal(L, "get_ref");
    return 0;
}

Val lua_to_val(lua_State *L, int _i) {
    int i = lua_absindex(L, _i);
    int t = lua_type(L, i);
    Val val = {0};
    switch (t) {
    case LUA_TSTRING: {
        const char *s = lua_tolstring(L, i, NULL);
        val.type=TSTRING;
        val.as.string = strdup(s);
        break;
    }
    case LUA_TBOOLEAN:
        val.type=TBOOLEAN;
        val.as.boolean = lua_toboolean(L, i);
        break;
    case LUA_TNUMBER:
        val.type=TNUMBER;
        val.as.number = lua_tonumber(L, i);
        break;
    case LUA_TTABLE:
        val.type=TTABLE;
        val.as.table = (Table){0};
        val.as.table.keys = (Vals){0};
        val.as.table.values = (Vals){0};

        lua_pushvalue(L, i);
        val.as.table.ref = luaL_ref(L, LUA_REGISTRYINDEX);
        if (lua_getmetatable(L, i)) {
            int mti = lua_gettop(L);
            lua_pushnil(L);
            while (lua_next(L, mti) != 0) {
                Val key = lua_to_val(L,-2);
                Val value = lua_to_val(L, -1);
                push_val(&val.as.table.keys,&key);
                push_val(&val.as.table.values,&value);
                lua_pop(L, 1);
            }
            lua_pop(L,1);
        }
        lua_pushnil(L);
        while (lua_next(L, i) != 0) {
            Val key = lua_to_val(L,-2);
            Val value = lua_to_val(L, -1);
            push_val(&val.as.table.keys,&key);
            push_val(&val.as.table.values,&value);
            lua_pop(L, 1);
        }
        break;
    case LUA_TFUNCTION:
        val.type = TFUNCTION;
        lua_pushvalue(L, i);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);
        val.as.function = ref;
        break;
    default: ;
        val.type=TUNSUPPORTED;
        break;
    }
    return val;
}

size_t lua_vm_sizeof_val() {
    return sizeof(Val);
}

Val *lua_vm_dostring(const char *code)
{
    int base = lua_gettop(L);
    
    Val *ret = malloc(sizeof(Val));
    if (!ret) {
        return NULL;
    }
    ret->type = TTABLE;
    ret->as.table = (Table) {
        .keys={0},
        .values={0}
    };
    Table ret_table = {
        .keys={0},
        .values={0}
    };

    // TODO: 오류 처리
    bool ok = !luaL_dostring(L, code);
    int top = lua_gettop(L);

    if (top > base) {
        int cnt = 0;
        for (int i = base + 1; i <= top; ++i) {
            Val key = {0};
            key.type = TNUMBER;
            key.as.number = cnt++;
            Val val = lua_to_val(L, i);
            push_val(&ret_table.keys,&key);
            push_val(&ret_table.values,&val);
        }
    }

    push_string_val(&ret->as.table.keys,"ok");
    push_int_val(&ret->as.table.values,ok);
    push_string_val(&ret->as.table.keys,"ret");
    push_table_val(&ret->as.table.values, &ret_table);
    lua_settop(L, base);
    return ret;
}
static void free_vals(Vals *val) {
    if (val == NULL) return;

    free(val->data);
    val->data=NULL;
    val->cap=0;
    val->cnt=0;
}
static void free_table(Table *table) {
    if (table == NULL) return;

    free_vals(&table->keys);
    free_vals(&table->values);
    free(table);
}
static void free_val_contents(Val *v) {
    if (!v) return;

    switch (v->type) {
    case TSTRING:
        free(v->as.string);
        v->as.string = NULL;
        break;

    case TTABLE:
        free_table(&v->as.table);
        break;

    case TFUNCTION:
        luaL_unref(L, LUA_REGISTRYINDEX, v->as.function);
        v->as.function = LUA_NOREF;
        break;

    default:
        break;
    }
}
void lua_vm_dostring_free(Val *val) {
    if (val == NULL) {
        return;
    }

    free_val_contents(val);
    free(val);
}

int lua_vm_close()
{
    luaL_dostring(L, "print('LuaVM close')");
    stackDump(L);
    lua_close(L);
    return 0;
}