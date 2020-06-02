'use strict';

let spectest = {
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

function exports(name, instance) {
  return {[name]: instance.exports};
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

function assert_return(action, expected) {
  let actual = action();
  if (!Object.is(actual, expected)) {
    throw new Error("Wasm return value " + expected + " expected, got " + actual);
  };
}

function assert_return_canonical_nan(action) {
  let actual = action();
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test that it's a canonical NaN.
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}

function assert_return_arithmetic_nan(action) {
  // Note that JS can't reliably distinguish different NaN values,
  // so there's no good way to test for specific bitpatterns here.
  let actual = action();
  if (!Number.isNaN(actual)) {
    throw new Error("Wasm return value NaN expected, got " + actual);
  };
}


// memory_grow.wast:1
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8d\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x00\x00\x60\x01\x7f\x01\x7f\x03\x87\x80\x80\x80\x00\x06\x00\x01\x00\x01\x02\x00\x05\x83\x80\x80\x80\x00\x01\x00\x00\x07\xd7\x80\x80\x80\x00\x06\x0c\x6c\x6f\x61\x64\x5f\x61\x74\x5f\x7a\x65\x72\x6f\x00\x00\x0d\x73\x74\x6f\x72\x65\x5f\x61\x74\x5f\x7a\x65\x72\x6f\x00\x01\x11\x6c\x6f\x61\x64\x5f\x61\x74\x5f\x70\x61\x67\x65\x5f\x73\x69\x7a\x65\x00\x02\x12\x73\x74\x6f\x72\x65\x5f\x61\x74\x5f\x70\x61\x67\x65\x5f\x73\x69\x7a\x65\x00\x03\x04\x67\x72\x6f\x77\x00\x04\x04\x73\x69\x7a\x65\x00\x05\x0a\xcd\x80\x80\x80\x00\x06\x87\x80\x80\x80\x00\x00\x41\x00\x28\x02\x00\x0b\x89\x80\x80\x80\x00\x00\x41\x00\x41\x02\x36\x02\x00\x0b\x89\x80\x80\x80\x00\x00\x41\x80\x80\x04\x28\x02\x00\x0b\x8b\x80\x80\x80\x00\x00\x41\x80\x80\x04\x41\x03\x36\x02\x00\x0b\x86\x80\x80\x80\x00\x00\x20\x00\x40\x00\x0b\x84\x80\x80\x80\x00\x00\x3f\x00\x0b");

// memory_grow.wast:14
assert_return(() => call($1, "size", []), 0);

// memory_grow.wast:15
assert_trap(() => call($1, "store_at_zero", []));

// memory_grow.wast:16
assert_trap(() => call($1, "load_at_zero", []));

// memory_grow.wast:17
assert_trap(() => call($1, "store_at_page_size", []));

// memory_grow.wast:18
assert_trap(() => call($1, "load_at_page_size", []));

// memory_grow.wast:19
assert_return(() => call($1, "grow", [1]), 0);

// memory_grow.wast:20
assert_return(() => call($1, "size", []), 1);

// memory_grow.wast:21
assert_return(() => call($1, "load_at_zero", []), 0);

// memory_grow.wast:22
assert_return(() => call($1, "store_at_zero", []));

// memory_grow.wast:23
assert_return(() => call($1, "load_at_zero", []), 2);

// memory_grow.wast:24
assert_trap(() => call($1, "store_at_page_size", []));

// memory_grow.wast:25
assert_trap(() => call($1, "load_at_page_size", []));

// memory_grow.wast:26
assert_return(() => call($1, "grow", [4]), 1);

// memory_grow.wast:27
assert_return(() => call($1, "size", []), 5);

// memory_grow.wast:28
assert_return(() => call($1, "load_at_zero", []), 2);

// memory_grow.wast:29
assert_return(() => call($1, "store_at_zero", []));

// memory_grow.wast:30
assert_return(() => call($1, "load_at_zero", []), 2);

// memory_grow.wast:31
assert_return(() => call($1, "load_at_page_size", []), 0);

// memory_grow.wast:32
assert_return(() => call($1, "store_at_page_size", []));

// memory_grow.wast:33
assert_return(() => call($1, "load_at_page_size", []), 3);

// memory_grow.wast:36
let $2 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7f\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x00\x05\x83\x80\x80\x80\x00\x01\x00\x00\x07\x88\x80\x80\x80\x00\x01\x04\x67\x72\x6f\x77\x00\x00\x0a\x8c\x80\x80\x80\x00\x01\x86\x80\x80\x80\x00\x00\x20\x00\x40\x00\x0b");

// memory_grow.wast:41
assert_return(() => call($2, "grow", [0]), 0);

// memory_grow.wast:42
assert_return(() => call($2, "grow", [1]), 0);

// memory_grow.wast:43
assert_return(() => call($2, "grow", [0]), 1);

// memory_grow.wast:44
assert_return(() => call($2, "grow", [2]), 1);

// memory_grow.wast:45
assert_return(() => call($2, "grow", [800]), 3);

// memory_grow.wast:46
assert_return(() => call($2, "grow", [65536]), -1);

// memory_grow.wast:47
assert_return(() => call($2, "grow", [64736]), -1);

// memory_grow.wast:48
assert_return(() => call($2, "grow", [1]), 803);

// memory_grow.wast:50
let $3 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7f\x01\x7f\x03\x82\x80\x80\x80\x00\x01\x00\x05\x84\x80\x80\x80\x00\x01\x01\x00\x0a\x07\x88\x80\x80\x80\x00\x01\x04\x67\x72\x6f\x77\x00\x00\x0a\x8c\x80\x80\x80\x00\x01\x86\x80\x80\x80\x00\x00\x20\x00\x40\x00\x0b");

// memory_grow.wast:55
assert_return(() => call($3, "grow", [0]), 0);

// memory_grow.wast:56
assert_return(() => call($3, "grow", [1]), 0);

// memory_grow.wast:57
assert_return(() => call($3, "grow", [1]), 1);

// memory_grow.wast:58
assert_return(() => call($3, "grow", [2]), 2);

// memory_grow.wast:59
assert_return(() => call($3, "grow", [6]), 4);

// memory_grow.wast:60
assert_return(() => call($3, "grow", [0]), 10);

// memory_grow.wast:61
assert_return(() => call($3, "grow", [1]), -1);

// memory_grow.wast:62
assert_return(() => call($3, "grow", [65536]), -1);

// memory_grow.wast:66
let $4 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x01\x7f\x01\x7f\x60\x02\x7f\x7f\x01\x7f\x03\x83\x80\x80\x80\x00\x02\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\x9c\x80\x80\x80\x00\x02\x04\x67\x72\x6f\x77\x00\x00\x11\x63\x68\x65\x63\x6b\x2d\x6d\x65\x6d\x6f\x72\x79\x2d\x7a\x65\x72\x6f\x00\x01\x0a\xc4\x80\x80\x80\x00\x02\x86\x80\x80\x80\x00\x00\x20\x00\x40\x00\x0b\xb3\x80\x80\x80\x00\x01\x01\x7f\x41\x01\x21\x02\x02\x40\x03\x40\x20\x00\x2d\x00\x00\x21\x02\x20\x02\x41\x00\x47\x0d\x01\x20\x00\x20\x01\x4f\x0d\x01\x20\x00\x41\x01\x6a\x21\x00\x20\x00\x20\x01\x4d\x0d\x00\x0b\x0b\x20\x02\x0b");

// memory_grow.wast:87
assert_return(() => call($4, "check-memory-zero", [0, 65535]), 0);

// memory_grow.wast:88
assert_return(() => call($4, "grow", [1]), 1);

// memory_grow.wast:89
assert_return(() => call($4, "check-memory-zero", [65536, 131071]), 0);

// memory_grow.wast:90
assert_return(() => call($4, "grow", [1]), 2);

// memory_grow.wast:91
assert_return(() => call($4, "check-memory-zero", [131072, 196607]), 0);

// memory_grow.wast:92
assert_return(() => call($4, "grow", [1]), 3);

// memory_grow.wast:93
assert_return(() => call($4, "check-memory-zero", [196608, 262143]), 0);

// memory_grow.wast:94
assert_return(() => call($4, "grow", [1]), 4);

// memory_grow.wast:95
assert_return(() => call($4, "check-memory-zero", [262144, 327679]), 0);

// memory_grow.wast:96
assert_return(() => call($4, "grow", [1]), 5);

// memory_grow.wast:97
assert_return(() => call($4, "check-memory-zero", [327680, 393215]), 0);
