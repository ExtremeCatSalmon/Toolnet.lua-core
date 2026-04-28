set EXPORTED_FUNCTIONS=^
    "_lua_vm_init", ^
    "_lua_vm_close", ^
    "_lua_vm_dostring", ^
    "_lua_vm_dostring_free"
REM Val
set EXPORTED_FUNCTIONS=%EXPORTED_FUNCTIONS%, ^
    "_sizeof_val", ^
    "_offsetof_val_type", ^
    "_offsetof_val_as"
REM ValType
set EXPORTED_FUNCTIONS=%EXPORTED_FUNCTIONS%, ^
    "_sizeof_val_type"

REM Table
set EXPORTED_FUNCTIONS=%EXPORTED_FUNCTIONS%, ^
    "_sizeof_table", ^
    "_offsetof_table_ref", ^
    "_offsetof_table_keys", ^
    "_offsetof_table_values"

REM primitive types
set EXPORTED_FUNCTIONS=%EXPORTED_FUNCTIONS%, ^
    "_sizeof_int", ^
    "_sizeof_size_t"

emcc bridge.c ./liblua.a -O2 ^
  -sMODULARIZE=1 ^
  -sEXPORT_ES6=1 ^
  -sENVIRONMENT=web ^
  -sEXPORTED_FUNCTIONS="[%EXPORTED_FUNCTIONS%]" ^
  -sEXPORTED_RUNTIME_METHODS=["ccall","cwrap","HEAPU32","HEAPU8","UTF8ToString","getValue"] ^
  -o lua.vm.js