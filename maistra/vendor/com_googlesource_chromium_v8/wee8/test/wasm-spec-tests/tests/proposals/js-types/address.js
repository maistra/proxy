'use strict';

let spectest = {
  print: print || ((...xs) => console.log(...xs)),
  global: 666,
  table: new WebAssembly.Table({initial: 10, maximum: 20, element: 'anyfunc'}),  memory: new WebAssembly.Memory({initial: 1, maximum: 2}),};

let registry = {spectest};

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
  return instance.exports[name];
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


// address.wast:1
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8a\x80\x80\x80\x00\x02\x60\x01\x7f\x01\x7f\x60\x01\x7f\x00\x03\x8f\x80\x80\x80\x00\x0e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\xf3\x80\x80\x80\x00\x0e\x05\x67\x6f\x6f\x64\x31\x00\x00\x05\x67\x6f\x6f\x64\x32\x00\x01\x05\x67\x6f\x6f\x64\x33\x00\x02\x05\x67\x6f\x6f\x64\x34\x00\x03\x05\x67\x6f\x6f\x64\x35\x00\x04\x05\x67\x6f\x6f\x64\x36\x00\x05\x05\x67\x6f\x6f\x64\x37\x00\x06\x05\x67\x6f\x6f\x64\x38\x00\x07\x05\x67\x6f\x6f\x64\x39\x00\x08\x06\x67\x6f\x6f\x64\x31\x30\x00\x09\x06\x67\x6f\x6f\x64\x31\x31\x00\x0a\x06\x67\x6f\x6f\x64\x31\x32\x00\x0b\x06\x67\x6f\x6f\x64\x31\x33\x00\x0c\x03\x62\x61\x64\x00\x0d\x0a\xae\x81\x80\x80\x00\x0e\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x01\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x02\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2d\x00\x19\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2f\x01\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2f\x00\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2f\x00\x01\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2f\x01\x02\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x2f\x00\x19\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x28\x02\x00\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x28\x00\x01\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x28\x01\x02\x0b\x87\x80\x80\x80\x00\x00\x20\x00\x28\x00\x19\x0b\x8c\x80\x80\x80\x00\x00\x20\x00\x28\x02\xff\xff\xff\xff\x0f\x1a\x0b\x0b\xa0\x80\x80\x80\x00\x01\x00\x41\x00\x0b\x1a\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a");

// address.wast:52
assert_return(() => call($1, "good1", [0]), 97);

// address.wast:53
assert_return(() => call($1, "good2", [0]), 98);

// address.wast:54
assert_return(() => call($1, "good3", [0]), 99);

// address.wast:55
assert_return(() => call($1, "good4", [0]), 122);

// address.wast:56
assert_return(() => call($1, "good5", [0]), 25185);

// address.wast:57
assert_return(() => call($1, "good6", [0]), 25185);

// address.wast:58
assert_return(() => call($1, "good7", [0]), 25442);

// address.wast:59
assert_return(() => call($1, "good8", [0]), 25699);

// address.wast:60
assert_return(() => call($1, "good9", [0]), 122);

// address.wast:61
assert_return(() => call($1, "good10", [0]), 1684234849);

// address.wast:62
assert_return(() => call($1, "good11", [0]), 1701077858);

// address.wast:63
assert_return(() => call($1, "good12", [0]), 1717920867);

// address.wast:64
assert_return(() => call($1, "good13", [0]), 122);

// address.wast:66
assert_return(() => call($1, "good1", [65507]), 0);

// address.wast:67
assert_return(() => call($1, "good2", [65507]), 0);

// address.wast:68
assert_return(() => call($1, "good3", [65507]), 0);

// address.wast:69
assert_return(() => call($1, "good4", [65507]), 0);

// address.wast:70
assert_return(() => call($1, "good5", [65507]), 0);

// address.wast:71
assert_return(() => call($1, "good6", [65507]), 0);

// address.wast:72
assert_return(() => call($1, "good7", [65507]), 0);

// address.wast:73
assert_return(() => call($1, "good8", [65507]), 0);

// address.wast:74
assert_return(() => call($1, "good9", [65507]), 0);

// address.wast:75
assert_return(() => call($1, "good10", [65507]), 0);

// address.wast:76
assert_return(() => call($1, "good11", [65507]), 0);

// address.wast:77
assert_return(() => call($1, "good12", [65507]), 0);

// address.wast:78
assert_return(() => call($1, "good13", [65507]), 0);

// address.wast:80
assert_return(() => call($1, "good1", [65508]), 0);

// address.wast:81
assert_return(() => call($1, "good2", [65508]), 0);

// address.wast:82
assert_return(() => call($1, "good3", [65508]), 0);

// address.wast:83
assert_return(() => call($1, "good4", [65508]), 0);

// address.wast:84
assert_return(() => call($1, "good5", [65508]), 0);

// address.wast:85
assert_return(() => call($1, "good6", [65508]), 0);

// address.wast:86
assert_return(() => call($1, "good7", [65508]), 0);

// address.wast:87
assert_return(() => call($1, "good8", [65508]), 0);

// address.wast:88
assert_return(() => call($1, "good9", [65508]), 0);

// address.wast:89
assert_return(() => call($1, "good10", [65508]), 0);

// address.wast:90
assert_return(() => call($1, "good11", [65508]), 0);

// address.wast:91
assert_return(() => call($1, "good12", [65508]), 0);

// address.wast:92
assert_trap(() => call($1, "good13", [65508]));

// address.wast:94
assert_trap(() => call($1, "bad", [0]));

// address.wast:95
assert_trap(() => call($1, "bad", [1]));

// address.wast:97
assert_malformed("\x3c\x6d\x61\x6c\x66\x6f\x72\x6d\x65\x64\x20\x71\x75\x6f\x74\x65\x3e");
