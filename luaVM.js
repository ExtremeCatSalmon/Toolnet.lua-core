import createLuaModule from "./lua.vm.js";

globalThis.Module = await createLuaModule();
const Module = globalThis.Module;

class Ref {
  constructor(type,ref) {
    this.type = type;
    this.ref = ref;
  }
}

// Val
const sizeof_val = Module.getValue(Module._sizeof_val, "i32");
const offsetof_val_type = Module.getValue(Module._offsetof_val_type, "i32");
const offsetof_val_as = Module.getValue(Module._offsetof_val_as, "i32");
// ValType
const sizeof_val_type = Module.getValue(Module._sizeof_val_type, "i32");
// Table
const sizeof_table = Module.getValue(Module._sizeof_table, "i32");
const offsetof_table_ref = Module.getValue(Module._offsetof_table_ref, "i32");
const offsetof_table_keys = Module.getValue(Module._offsetof_table_keys, "i32");
const offsetof_table_values = Module.getValue(
  Module._offsetof_table_values,
  "i32",
);
// primitive types
const sizeof_int = Module.getValue(Module._sizeof_int, "i32");
const sizeof_size_t = Module.getValue(Module._sizeof_size_t, "i32");

const lua_vm_init = Module.cwrap("lua_vm_init", "number", []);
const lua_vm_close = Module.cwrap("lua_vm_close", "number", []);
const lua_vm_dostring_free = Module.cwrap("lua_vm_dostring_free", "", [
  "number",
]);
const TYPES = {
  NIL: 0,
  BOOLEAN: 1,
  NUMBER: 2,
  STRING: 3,
  TABLE: 4,
  FUNCTION: 5,
  UNSUPPORTED: -1,
};
const vals_ptr_to_array = function (rawptr) {
  let ptr = rawptr >>> 2;
  let [vals_ptr, vals_cnt] = Module.HEAPU32.subarray(ptr, ptr + 2);
  const vals = [];
  for (let i = 0; i < vals_cnt; ++i) {
    let pt = vals_ptr + sizeof_val * i;
    vals.push(val_to_jsval(pt));
  }
  return vals;
};
const val_to_jsval = function (ptr) {
  let bytes = Module.HEAPU8.subarray(ptr, ptr + sizeof_val);
  let view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let type = view.getInt32(0, 1);
  switch (type) {
    case TYPES.NIL:
      return 0;
      break;
    case TYPES.BOOLEAN:
      return view.getInt32(offsetof_val_as, 1);
      break;
    case TYPES.NUMBER:
      return view.getFloat64(offsetof_val_as, 1);
      break;
    case TYPES.STRING:
      return Module.UTF8ToString(Module.HEAPU32[(ptr + offsetof_val_as) >>> 2]);
      break;
    case TYPES.TABLE:
      let keys = vals_ptr_to_array(ptr + offsetof_val_as + offsetof_table_keys);
      let values = vals_ptr_to_array(ptr + offsetof_val_as + offsetof_table_values);
      let result = Object.fromEntries(keys.map((key, i) => [key, values[i]]));
      Object.defineProperty(result, "__ref__", {
        value: new Ref(TYPES.TABLE,view.getInt32(offsetof_table_ref, 1)),
        enumerable: false,
      });
      return result;
      break;
    case TYPES.FUNCTION:
      return new Ref(TYPES.FUNCTION, view.getInt32(offsetof_val_as, 1));
    case TYPES.UNSUPPORTED:
      return "UNSUPPORTED";
      break;
  }
};
const lua_vm_dostring = function (code) {
  const fn = Module.cwrap("lua_vm_dostring", "number", ["string"]);
  const rawptr = fn(code);
  if (!rawptr) return [];
  const result = val_to_jsval(rawptr);
  lua_vm_dostring_free(rawptr);
  if (result.ok) {
    return Object.values(result.ret);
  } else {
    console.error(result.ret[0]);
    return null;
  }
};
export default {
    init: lua_vm_init,
    close: lua_vm_close,
    dostring: lua_vm_dostring
};