
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
      default:
        if (!Object.is(actual[i], expected[i])) {
          throw new Error("Wasm return value " + expected[i] + " expected, got " + actual[i]);
        };
    }
  }
}

// simd_load16_lane.wast:4
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8c\x80\x80\x80\x00\x02\x60\x02\x7f\x7b\x01\x7b\x60\x01\x7b\x01\x7b\x03\xa1\x80\x80\x80\x00\x20\x00\x00\x00\x00\x00\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\xe9\x86\x80\x80\x00\x20\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x00\x00\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x00\x01\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x00\x02\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x00\x03\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x00\x04\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x00\x05\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x00\x06\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x00\x07\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x5f\x6f\x66\x66\x73\x65\x74\x5f\x30\x00\x08\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x5f\x6f\x66\x66\x73\x65\x74\x5f\x31\x00\x09\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x5f\x6f\x66\x66\x73\x65\x74\x5f\x32\x00\x0a\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x5f\x6f\x66\x66\x73\x65\x74\x5f\x33\x00\x0b\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x5f\x6f\x66\x66\x73\x65\x74\x5f\x34\x00\x0c\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x5f\x6f\x66\x66\x73\x65\x74\x5f\x35\x00\x0d\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x5f\x6f\x66\x66\x73\x65\x74\x5f\x36\x00\x0e\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x5f\x6f\x66\x66\x73\x65\x74\x5f\x37\x00\x0f\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x10\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x11\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x12\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x13\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x14\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x15\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x16\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x17\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x18\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x19\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x1a\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x1b\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x1c\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x1d\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x1e\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x1f\x0a\x81\x84\x80\x80\x00\x20\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x00\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x01\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x02\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x03\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x04\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x05\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x06\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x07\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x00\x00\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x01\x01\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x02\x02\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x03\x03\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x04\x04\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x05\x05\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x06\x06\x0b\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x07\x07\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x00\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x00\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x01\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x01\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x02\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x02\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x03\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x03\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x04\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x04\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x05\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x05\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x06\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x06\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x00\x00\x07\x0b\x8b\x80\x80\x80\x00\x00\x20\x00\x20\x01\xfd\x59\x01\x00\x07\x0b\x0b\x96\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x10\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f");

// simd_load16_lane.wast:105
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x00\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_0", [0, v128("0 0 0 0")]), v128("256 0 0 0 0 0 0 0"))

// simd_load16_lane.wast:108
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x01\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x01\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_1", [1, v128("0 0 0 0")]), v128("0 513 0 0 0 0 0 0"))

// simd_load16_lane.wast:111
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x02\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x02\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_2", [2, v128("0 0 0 0")]), v128("0 0 770 0 0 0 0 0"))

// simd_load16_lane.wast:114
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x03\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x03\x04\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_3", [3, v128("0 0 0 0")]), v128("0 0 0 1_027 0 0 0 0"))

// simd_load16_lane.wast:117
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x04\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x04\x05\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_4", [4, v128("0 0 0 0")]), v128("0 0 0 0 1_284 0 0 0"))

// simd_load16_lane.wast:120
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x05\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x06\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_5", [5, v128("0 0 0 0")]), v128("0 0 0 0 0 1_541 0 0"))

// simd_load16_lane.wast:123
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x06\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x07\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_6", [6, v128("0 0 0 0")]), v128("0 0 0 0 0 0 1_798 0"))

// simd_load16_lane.wast:126
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\x99\x80\x80\x80\x00\x01\x02\x24\x31\x12\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x07\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x07\x08\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_7", [7, v128("0 0 0 0")]), v128("0 0 0 0 0 0 0 2_055"))

// simd_load16_lane.wast:129
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x5f\x6f\x66\x66\x73\x65\x74\x5f\x30\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_0_offset_0", [v128("0 0 0 0")]), v128("256 0 0 0 0 0 0 0"))

// simd_load16_lane.wast:131
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x5f\x6f\x66\x66\x73\x65\x74\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x01\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_1_offset_1", [v128("0 0 0 0")]), v128("0 513 0 0 0 0 0 0"))

// simd_load16_lane.wast:133
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x5f\x6f\x66\x66\x73\x65\x74\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x02\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_2_offset_2", [v128("0 0 0 0")]), v128("0 0 770 0 0 0 0 0"))

// simd_load16_lane.wast:135
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x5f\x6f\x66\x66\x73\x65\x74\x5f\x33\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x03\x04\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_3_offset_3", [v128("0 0 0 0")]), v128("0 0 0 1_027 0 0 0 0"))

// simd_load16_lane.wast:137
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x5f\x6f\x66\x66\x73\x65\x74\x5f\x34\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x04\x05\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_4_offset_4", [v128("0 0 0 0")]), v128("0 0 0 0 1_284 0 0 0"))

// simd_load16_lane.wast:139
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x5f\x6f\x66\x66\x73\x65\x74\x5f\x35\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x06\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_5_offset_5", [v128("0 0 0 0")]), v128("0 0 0 0 0 1_541 0 0"))

// simd_load16_lane.wast:141
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x5f\x6f\x66\x66\x73\x65\x74\x5f\x36\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x07\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_6_offset_6", [v128("0 0 0 0")]), v128("0 0 0 0 0 0 1_798 0"))

// simd_load16_lane.wast:143
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x89\x80\x80\x80\x00\x02\x60\x00\x00\x60\x01\x7b\x01\x7b\x02\xa2\x80\x80\x80\x00\x01\x02\x24\x31\x1b\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x5f\x6f\x66\x66\x73\x65\x74\x5f\x37\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xce\x80\x80\x80\x00\x01\xc8\x80\x80\x80\x00\x00\x02\x40\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x07\x08\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_7_offset_7", [v128("0 0 0 0")]), v128("0 0 0 0 0 0 0 2_055"))

// simd_load16_lane.wast:145
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x00\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_0_align_1", [0, v128("0 0 0 0")]), v128("256 0 0 0 0 0 0 0"))

// simd_load16_lane.wast:148
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x30\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x00\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_0_align_2", [0, v128("0 0 0 0")]), v128("256 0 0 0 0 0 0 0"))

// simd_load16_lane.wast:151
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x01\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x01\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_1_align_1", [1, v128("0 0 0 0")]), v128("0 513 0 0 0 0 0 0"))

// simd_load16_lane.wast:154
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x31\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x01\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x01\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_1_align_2", [1, v128("0 0 0 0")]), v128("0 513 0 0 0 0 0 0"))

// simd_load16_lane.wast:157
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x02\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x02\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_2_align_1", [2, v128("0 0 0 0")]), v128("0 0 770 0 0 0 0 0"))

// simd_load16_lane.wast:160
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x32\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x02\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x02\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_2_align_2", [2, v128("0 0 0 0")]), v128("0 0 770 0 0 0 0 0"))

// simd_load16_lane.wast:163
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x03\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x03\x04\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_3_align_1", [3, v128("0 0 0 0")]), v128("0 0 0 1_027 0 0 0 0"))

// simd_load16_lane.wast:166
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x33\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x03\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x03\x04\x00\x00\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_3_align_2", [3, v128("0 0 0 0")]), v128("0 0 0 1_027 0 0 0 0"))

// simd_load16_lane.wast:169
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x04\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x04\x05\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_4_align_1", [4, v128("0 0 0 0")]), v128("0 0 0 0 1_284 0 0 0"))

// simd_load16_lane.wast:172
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x34\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x04\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x04\x05\x00\x00\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_4_align_2", [4, v128("0 0 0 0")]), v128("0 0 0 0 1_284 0 0 0"))

// simd_load16_lane.wast:175
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x05\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x06\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_5_align_1", [5, v128("0 0 0 0")]), v128("0 0 0 0 0 1_541 0 0"))

// simd_load16_lane.wast:178
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x35\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x05\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x06\x00\x00\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_5_align_2", [5, v128("0 0 0 0")]), v128("0 0 0 0 0 1_541 0 0"))

// simd_load16_lane.wast:181
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x06\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x07\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_6_align_1", [6, v128("0 0 0 0")]), v128("0 0 0 0 0 0 1_798 0"))

// simd_load16_lane.wast:184
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x36\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x06\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x06\x07\x00\x00\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_6_align_2", [6, v128("0 0 0 0")]), v128("0 0 0 0 0 0 1_798 0"))

// simd_load16_lane.wast:187
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x5f\x61\x6c\x69\x67\x6e\x5f\x31\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x07\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x07\x08\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_7_align_1", [7, v128("0 0 0 0")]), v128("0 0 0 0 0 0 0 2_055"))

// simd_load16_lane.wast:190
run(() => call(instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x00\x00\x60\x02\x7f\x7b\x01\x7b\x02\xa1\x80\x80\x80\x00\x01\x02\x24\x31\x1a\x76\x31\x32\x38\x2e\x6c\x6f\x61\x64\x31\x36\x5f\x6c\x61\x6e\x65\x5f\x37\x5f\x61\x6c\x69\x67\x6e\x5f\x32\x00\x01\x03\x82\x80\x80\x80\x00\x01\x00\x07\x87\x80\x80\x80\x00\x01\x03\x72\x75\x6e\x00\x01\x0a\xd0\x80\x80\x80\x00\x01\xca\x80\x80\x80\x00\x00\x02\x40\x41\x07\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\xfd\x0c\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xfd\x4e\xfd\x0c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x07\x08\xfd\x23\xfd\x63\x45\x0d\x00\x0f\x0b\x00\x0b", exports("$1", $1)),  "run", []));  // assert_return(() => call($1, "v128.load16_lane_7_align_2", [7, v128("0 0 0 0")]), v128("0 0 0 0 0 0 0 2_055"))

// simd_load16_lane.wast:195
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7b\x01\x7b\x03\x82\x80\x80\x80\x00\x01\x00\x05\x83\x80\x80\x80\x00\x01\x00\x01\x0a\x91\x80\x80\x80\x00\x01\x8b\x80\x80\x80\x00\x00\x20\x00\x41\x00\xfd\x59\x01\x00\x00\x0b");

// simd_load16_lane.wast:201
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7b\x01\x7b\x03\x82\x80\x80\x80\x00\x01\x00\x05\x83\x80\x80\x80\x00\x01\x00\x01\x0a\x91\x80\x80\x80\x00\x01\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x01\x00\x08\x0b");

// simd_load16_lane.wast:207
assert_invalid("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x86\x80\x80\x80\x00\x01\x60\x01\x7b\x01\x7b\x03\x82\x80\x80\x80\x00\x01\x00\x05\x83\x80\x80\x80\x00\x01\x00\x01\x0a\x91\x80\x80\x80\x00\x01\x8b\x80\x80\x80\x00\x00\x41\x00\x20\x00\xfd\x59\x02\x00\x00\x0b");
