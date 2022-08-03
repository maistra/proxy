
'use strict';

let externrefs = {};
let externsym = Symbol("externref");
function externref(s) {
  if (! (s in externrefs)) externrefs[s] = {[externsym]: s};
  return externrefs[s];
}
function is_externref(x) {
  return (x !== null && externsym in x) ? 1 : 0;
}
function is_funcref(x) {
  return typeof x === "function" ? 1 : 0;
}
function eq_externref(x, y) {
  return x === y ? 1 : 0;
}
function eq_funcref(x, y) {
  return x === y ? 1 : 0;
}

let spectest = {
  externref: externref,
  is_externref: is_externref,
  is_funcref: is_funcref,
  eq_externref: eq_externref,
  eq_funcref: eq_funcref,
  print: console.log.bind(console),
  print_i32: console.log.bind(console),
  print_i32_f32: console.log.bind(console),
  print_f64_f64: console.log.bind(console),
  print_f32: console.log.bind(console),
  print_f64: console.log.bind(console),
  global_i32: 666,
  global_f32: 666,
  global_f64: 666,
  table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),
  memory: new WebAssembly.Memory({initial: 1, maximum: 2})
};

let handler = {
  get(target, prop) {
    return (prop in target) ?  target[prop] : {};
  }
};
let registry = new Proxy({spectest}, handler);

function register(name, instance) {
  registry[name] = instance.exports;
}

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  let validated;
  try {
    validated = WebAssembly.validate(buffer);
  } catch (e) {
    throw new Error("Wasm validate throws");
  }
  if (validated !== valid) {
    throw new Error("Wasm validate failure" + (valid ? "" : " expected"));
  }
  return new WebAssembly.Module(buffer);
}

function instance(bytes, imports = registry) {
  return new WebAssembly.Instance(module(bytes), imports);
}

function call(instance, name, args) {
  return instance.exports[name](...args);
}

function get(instance, name) {
  let v = instance.exports[name];
  return (v instanceof WebAssembly.Global) ? v.value : v;
}

function exports(instance) {
  return {module: instance.exports, spectest: spectest};
}

function run(action) {
  action();
}

function assert_malformed(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm decoding failure expected");
}

function assert_invalid(bytes) {
  try { module(bytes, false) } catch (e) {
    if (e instanceof WebAssembly.CompileError) return;
  }
  throw new Error("Wasm validation failure expected");
}

function assert_unlinkable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.LinkError) return;
  }
  throw new Error("Wasm linking failure expected");
}

function assert_uninstantiable(bytes) {
  let mod = module(bytes);
  try { new WebAssembly.Instance(mod, registry) } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

function assert_trap(action) {
  try { action() } catch (e) {
    if (e instanceof WebAssembly.RuntimeError) return;
  }
  throw new Error("Wasm trap expected");
}

let StackOverflow;
try { (function f() { 1 + f() })() } catch (e) { StackOverflow = e.constructor }

function assert_exhaustion(action) {
  try { action() } catch (e) {
    if (e instanceof StackOverflow) return;
  }
  throw new Error("Wasm resource exhaustion expected");
}

function assert_return(action, ...expected) {
  let actual = action();
  if (actual === undefined) {
    actual = [];
  } else if (!Array.isArray(actual)) {
    actual = [actual];
  }
  if (actual.length !== expected.length) {
    throw new Error(expected.length + " value(s) expected, got " + actual.length);
  }
  for (let i = 0; i < actual.length; ++i) {
    switch (expected[i]) {
      case "nan:canonical":
      case "nan:arithmetic":
      case "nan:any":
        // Note that JS can't reliably distinguish different NaN values,
        // so there's no good way to test that it's a canonical NaN.
        if (!Number.isNaN(actual[i])) {
          throw new Error("Wasm return value NaN expected, got " + actual[i]);
        };
        return;
      case "ref.func":
        if (typeof actual[i] !== "function") {
          throw new Error("Wasm function return value expected, got " + actual[i]);
        };
        return;
      case "ref.extern":
        if (actual[i] === null) {
          throw new Error("Wasm reference return value expected, got " + actual[i]);
        };
        return;
      default:
        if (!Object.is(actual[i], expected[i])) {
          throw new Error("Wasm return value " + expected[i] + " expected, got " + actual[i]);
        };
    }
  }
}

// bulk.wast:2
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x05\x83\x80\x80\x80\x00\x01\x00\x01\x0b\x86\x80\x80\x80\x00\x01\x01\x03\x66\x6f\x6f");

// bulk.wast:6
let $2 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x83\x80\x80\x80\x00\x02\x00\x00\x04\x84\x80\x80\x80\x00\x01\x70\x00\x03\x09\x8d\x80\x80\x80\x00\x01\x05\x70\x03\xd2\x00\x0b\xd0\x70\x0b\xd2\x01\x0b\x0a\x8f\x80\x80\x80\x00\x02\x82\x80\x80\x80\x00\x00\x0b\x82\x80\x80\x80\x00\x00\x0b");

// bulk.wast:13
let $3 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\x92\x80\x80\x80\x00\x02\x04\x66\x69\x6c\x6c\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9d\x80\x80\x80\x00\x02\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0b\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b");

// bulk.wast:27
run(() => call($3, "fill", [1, 255, 3]));

// bulk.wast:28
assert_return(() => call($3, "load8_u", [0]), 0);

// bulk.wast:29
assert_return(() => call($3, "load8_u", [1]), 255);

// bulk.wast:30
assert_return(() => call($3, "load8_u", [2]), 255);

// bulk.wast:31
assert_return(() => call($3, "load8_u", [3]), 255);

// bulk.wast:32
assert_return(() => call($3, "load8_u", [4]), 0);

// bulk.wast:35
run(() => call($3, "fill", [0, 48_042, 2]));

// bulk.wast:36
assert_return(() => call($3, "load8_u", [0]), 170);

// bulk.wast:37
assert_return(() => call($3, "load8_u", [1]), 170);

// bulk.wast:40
run(() => call($3, "fill", [0, 0, 65_536]));

// bulk.wast:43
assert_trap(() => call($3, "fill", [65_280, 1, 257]));

// bulk.wast:45
assert_return(() => call($3, "load8_u", [65_280]), 0);

// bulk.wast:46
assert_return(() => call($3, "load8_u", [65_535]), 0);

// bulk.wast:49
run(() => call($3, "fill", [65_536, 0, 0]));

// bulk.wast:52
assert_trap(() => call($3, "fill", [65_537, 0, 0]));

// bulk.wast:57
let $4 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x84\x80\x80\x80\x00\x01\x01\x01\x01\x07\x92\x80\x80\x80\x00\x02\x04\x63\x6f\x70\x79\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0a\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x8a\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x04\xaa\xbb\xcc\xdd");

// bulk.wast:71
run(() => call($4, "copy", [10, 0, 4]));

// bulk.wast:73
assert_return(() => call($4, "load8_u", [9]), 0);

// bulk.wast:74
assert_return(() => call($4, "load8_u", [10]), 170);

// bulk.wast:75
assert_return(() => call($4, "load8_u", [11]), 187);

// bulk.wast:76
assert_return(() => call($4, "load8_u", [12]), 204);

// bulk.wast:77
assert_return(() => call($4, "load8_u", [13]), 221);

// bulk.wast:78
assert_return(() => call($4, "load8_u", [14]), 0);

// bulk.wast:81
run(() => call($4, "copy", [8, 10, 4]));

// bulk.wast:82
assert_return(() => call($4, "load8_u", [8]), 170);

// bulk.wast:83
assert_return(() => call($4, "load8_u", [9]), 187);

// bulk.wast:84
assert_return(() => call($4, "load8_u", [10]), 204);

// bulk.wast:85
assert_return(() => call($4, "load8_u", [11]), 221);

// bulk.wast:86
assert_return(() => call($4, "load8_u", [12]), 204);

// bulk.wast:87
assert_return(() => call($4, "load8_u", [13]), 221);

// bulk.wast:90
run(() => call($4, "copy", [10, 7, 6]));

// bulk.wast:91
assert_return(() => call($4, "load8_u", [10]), 0);

// bulk.wast:92
assert_return(() => call($4, "load8_u", [11]), 170);

// bulk.wast:93
assert_return(() => call($4, "load8_u", [12]), 187);

// bulk.wast:94
assert_return(() => call($4, "load8_u", [13]), 204);

// bulk.wast:95
assert_return(() => call($4, "load8_u", [14]), 221);

// bulk.wast:96
assert_return(() => call($4, "load8_u", [15]), 204);

// bulk.wast:97
assert_return(() => call($4, "load8_u", [16]), 0);

// bulk.wast:100
run(() => call($4, "copy", [65_280, 0, 256]));

// bulk.wast:101
run(() => call($4, "copy", [65_024, 65_280, 256]));

// bulk.wast:104
run(() => call($4, "copy", [65_536, 0, 0]));

// bulk.wast:105
run(() => call($4, "copy", [0, 65_536, 0]));

// bulk.wast:108
assert_trap(() => call($4, "copy", [65_537, 0, 0]));

// bulk.wast:110
assert_trap(() => call($4, "copy", [0, 65_537, 0]));

// bulk.wast:115
let $5 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\x92\x80\x80\x80\x00\x02\x04\x69\x6e\x69\x74\x00\x00\x07\x6c\x6f\x61\x64\x38\x5f\x75\x00\x01\x0c\x81\x80\x80\x80\x00\x01\x0a\x9e\x80\x80\x80\x00\x02\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x08\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x0b\x87\x80\x80\x80\x00\x01\x01\x04\xaa\xbb\xcc\xdd");

// bulk.wast:129
run(() => call($5, "init", [0, 1, 2]));

// bulk.wast:130
assert_return(() => call($5, "load8_u", [0]), 187);

// bulk.wast:131
assert_return(() => call($5, "load8_u", [1]), 204);

// bulk.wast:132
assert_return(() => call($5, "load8_u", [2]), 0);

// bulk.wast:135
run(() => call($5, "init", [65_532, 0, 4]));

// bulk.wast:138
assert_trap(() => call($5, "init", [65_534, 0, 3]));

// bulk.wast:140
assert_return(() => call($5, "load8_u", [65_534]), 204);

// bulk.wast:141
assert_return(() => call($5, "load8_u", [65_535]), 221);

// bulk.wast:144
run(() => call($5, "init", [65_536, 0, 0]));

// bulk.wast:145
run(() => call($5, "init", [0, 4, 0]));

// bulk.wast:148
assert_trap(() => call($5, "init", [65_537, 0, 0]));

// bulk.wast:150
assert_trap(() => call($5, "init", [0, 5, 0]));

// bulk.wast:154
let $6 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x00\x03\x85\x80\x80\x80\x00\x04\x00\x01\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\xbb\x80\x80\x80\x00\x04\x0c\x64\x72\x6f\x70\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x00\x0c\x69\x6e\x69\x74\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x01\x0b\x64\x72\x6f\x70\x5f\x61\x63\x74\x69\x76\x65\x00\x02\x0b\x69\x6e\x69\x74\x5f\x61\x63\x74\x69\x76\x65\x00\x03\x0c\x81\x80\x80\x80\x00\x02\x0a\xb7\x80\x80\x80\x00\x04\x85\x80\x80\x80\x00\x00\xfc\x09\x00\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x20\x00\xfc\x08\x00\x00\x0b\x85\x80\x80\x80\x00\x00\xfc\x09\x01\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x20\x00\xfc\x08\x01\x00\x0b\x0b\x8a\x80\x80\x80\x00\x02\x01\x01\x78\x00\x41\x00\x0b\x01\x78");

// bulk.wast:168
run(() => call($6, "init_passive", [1]));

// bulk.wast:169
run(() => call($6, "drop_passive", []));

// bulk.wast:170
run(() => call($6, "drop_passive", []));

// bulk.wast:171
assert_return(() => call($6, "init_passive", [0]));

// bulk.wast:172
assert_trap(() => call($6, "init_passive", [1]));

// bulk.wast:173
run(() => call($6, "init_passive", [0]));

// bulk.wast:174
run(() => call($6, "drop_active", []));

// bulk.wast:175
assert_return(() => call($6, "init_active", [0]));

// bulk.wast:176
assert_trap(() => call($6, "init_active", [1]));

// bulk.wast:177
run(() => call($6, "init_active", [0]));

// bulk.wast:181
let $7 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0c\x81\x80\x80\x80\x00\x41\x0a\x8b\x80\x80\x80\x00\x01\x85\x80\x80\x80\x00\x00\xfc\x09\x40\x0b\x0b\x83\x81\x80\x80\x00\x41\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00\x01\x00");

// bulk.wast:196
let $8 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x0c\x81\x80\x80\x80\x00\x01\x0a\x8b\x80\x80\x80\x00\x01\x85\x80\x80\x80\x00\x00\xfc\x09\x00\x0b\x0b\x8a\x80\x80\x80\x00\x01\x01\x07\x67\x6f\x6f\x64\x62\x79\x65");

// bulk.wast:199
let $9 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x85\x80\x80\x80\x00\x04\x00\x00\x01\x02\x04\x84\x80\x80\x80\x00\x01\x70\x00\x03\x07\x8f\x80\x80\x80\x00\x02\x04\x69\x6e\x69\x74\x00\x02\x04\x63\x61\x6c\x6c\x00\x03\x09\x88\x80\x80\x80\x00\x01\x01\x00\x04\x00\x01\x00\x01\x0a\xb0\x80\x80\x80\x00\x04\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0c\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// bulk.wast:219
assert_trap(() => call($9, "init", [2, 0, 2]));

// bulk.wast:221
assert_trap(() => call($9, "call", [2]));

// bulk.wast:224
run(() => call($9, "init", [0, 1, 2]));

// bulk.wast:225
assert_return(() => call($9, "call", [0]), 1);

// bulk.wast:226
assert_return(() => call($9, "call", [1]), 0);

// bulk.wast:227
assert_trap(() => call($9, "call", [2]));

// bulk.wast:230
run(() => call($9, "init", [1, 2, 2]));

// bulk.wast:233
run(() => call($9, "init", [3, 0, 0]));

// bulk.wast:234
run(() => call($9, "init", [0, 4, 0]));

// bulk.wast:237
assert_trap(() => call($9, "init", [4, 0, 0]));

// bulk.wast:239
assert_trap(() => call($9, "init", [0, 5, 0]));

// bulk.wast:244
let $10 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x88\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7f\x00\x03\x86\x80\x80\x80\x00\x05\x00\x00\x01\x00\x01\x04\x84\x80\x80\x80\x00\x01\x70\x00\x01\x07\xbb\x80\x80\x80\x00\x04\x0c\x64\x72\x6f\x70\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x01\x0c\x69\x6e\x69\x74\x5f\x70\x61\x73\x73\x69\x76\x65\x00\x02\x0b\x64\x72\x6f\x70\x5f\x61\x63\x74\x69\x76\x65\x00\x03\x0b\x69\x6e\x69\x74\x5f\x61\x63\x74\x69\x76\x65\x00\x04\x09\x8b\x80\x80\x80\x00\x02\x01\x00\x01\x00\x00\x41\x00\x0b\x01\x00\x0a\xbe\x80\x80\x80\x00\x05\x82\x80\x80\x80\x00\x00\x0b\x85\x80\x80\x80\x00\x00\xfc\x0d\x00\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x20\x00\xfc\x0c\x00\x00\x0b\x85\x80\x80\x80\x00\x00\xfc\x0d\x01\x0b\x8c\x80\x80\x80\x00\x00\x41\x00\x41\x00\x20\x00\xfc\x0c\x01\x00\x0b");

// bulk.wast:261
run(() => call($10, "init_passive", [1]));

// bulk.wast:262
run(() => call($10, "drop_passive", []));

// bulk.wast:263
run(() => call($10, "drop_passive", []));

// bulk.wast:264
assert_return(() => call($10, "init_passive", [0]));

// bulk.wast:265
assert_trap(() => call($10, "init_passive", [1]));

// bulk.wast:266
run(() => call($10, "init_passive", [0]));

// bulk.wast:267
run(() => call($10, "drop_active", []));

// bulk.wast:268
assert_return(() => call($10, "init_active", [0]));

// bulk.wast:269
assert_trap(() => call($10, "init_active", [1]));

// bulk.wast:270
run(() => call($10, "init_active", [0]));

// bulk.wast:274
let $11 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x09\xc4\x81\x80\x80\x00\x41\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x01\x00\x00\x0a\x8b\x80\x80\x80\x00\x01\x85\x80\x80\x80\x00\x00\xfc\x0d\x40\x0b");

// bulk.wast:297
let $12 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x84\x80\x80\x80\x00\x01\x60\x00\x00\x03\x82\x80\x80\x80\x00\x01\x00\x09\x85\x80\x80\x80\x00\x01\x01\x00\x01\x00\x0a\x8b\x80\x80\x80\x00\x01\x85\x80\x80\x80\x00\x00\xfc\x0d\x00\x0b");

// bulk.wast:300
let $13 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x90\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x03\x7f\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x86\x80\x80\x80\x00\x05\x00\x00\x00\x01\x02\x04\x84\x80\x80\x80\x00\x01\x70\x00\x0a\x07\x8f\x80\x80\x80\x00\x02\x04\x63\x6f\x70\x79\x00\x03\x04\x63\x61\x6c\x6c\x00\x04\x09\x89\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x03\x00\x01\x02\x0a\xb9\x80\x80\x80\x00\x05\x84\x80\x80\x80\x00\x00\x41\x00\x0b\x84\x80\x80\x80\x00\x00\x41\x01\x0b\x84\x80\x80\x80\x00\x00\x41\x02\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x20\x01\x20\x02\xfc\x0e\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x11\x00\x00\x0b");

// bulk.wast:319
run(() => call($13, "copy", [3, 0, 3]));

// bulk.wast:321
assert_return(() => call($13, "call", [3]), 0);

// bulk.wast:322
assert_return(() => call($13, "call", [4]), 1);

// bulk.wast:323
assert_return(() => call($13, "call", [5]), 2);

// bulk.wast:326
run(() => call($13, "copy", [0, 1, 3]));

// bulk.wast:328
assert_return(() => call($13, "call", [0]), 1);

// bulk.wast:329
assert_return(() => call($13, "call", [1]), 2);

// bulk.wast:330
assert_return(() => call($13, "call", [2]), 0);

// bulk.wast:333
run(() => call($13, "copy", [2, 0, 3]));

// bulk.wast:335
assert_return(() => call($13, "call", [2]), 1);

// bulk.wast:336
assert_return(() => call($13, "call", [3]), 2);

// bulk.wast:337
assert_return(() => call($13, "call", [4]), 0);

// bulk.wast:340
run(() => call($13, "copy", [6, 8, 2]));

// bulk.wast:341
run(() => call($13, "copy", [8, 6, 2]));

// bulk.wast:344
run(() => call($13, "copy", [10, 0, 0]));

// bulk.wast:345
run(() => call($13, "copy", [0, 10, 0]));

// bulk.wast:348
assert_trap(() => call($13, "copy", [11, 0, 0]));

// bulk.wast:350
assert_trap(() => call($13, "copy", [0, 11, 0]));
