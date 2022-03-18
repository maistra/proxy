
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
  global_i64: 666n,
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

// utf8-custom-section-id.wast:6
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\x80");

// utf8-custom-section-id.wast:16
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\x8f");

// utf8-custom-section-id.wast:26
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\x90");

// utf8-custom-section-id.wast:36
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\x9f");

// utf8-custom-section-id.wast:46
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xa0");

// utf8-custom-section-id.wast:56
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xbf");

// utf8-custom-section-id.wast:68
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xc2\x80\x80");

// utf8-custom-section-id.wast:78
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xc2");

// utf8-custom-section-id.wast:88
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc2\x2e");

// utf8-custom-section-id.wast:100
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc0\x80");

// utf8-custom-section-id.wast:110
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc0\xbf");

// utf8-custom-section-id.wast:120
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc1\x80");

// utf8-custom-section-id.wast:130
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc1\xbf");

// utf8-custom-section-id.wast:140
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc2\x00");

// utf8-custom-section-id.wast:150
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc2\x7f");

// utf8-custom-section-id.wast:160
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc2\xc0");

// utf8-custom-section-id.wast:170
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xc2\xfd");

// utf8-custom-section-id.wast:180
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xdf\x00");

// utf8-custom-section-id.wast:190
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xdf\x7f");

// utf8-custom-section-id.wast:200
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xdf\xc0");

// utf8-custom-section-id.wast:210
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xdf\xfd");

// utf8-custom-section-id.wast:222
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xe1\x80\x80\x80");

// utf8-custom-section-id.wast:232
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xe1\x80");

// utf8-custom-section-id.wast:242
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\x80\x2e");

// utf8-custom-section-id.wast:252
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xe1");

// utf8-custom-section-id.wast:262
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xe1\x2e");

// utf8-custom-section-id.wast:274
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\x00\xa0");

// utf8-custom-section-id.wast:284
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\x7f\xa0");

// utf8-custom-section-id.wast:294
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\x80\x80");

// utf8-custom-section-id.wast:304
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\x80\xa0");

// utf8-custom-section-id.wast:314
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\x9f\xa0");

// utf8-custom-section-id.wast:324
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\x9f\xbf");

// utf8-custom-section-id.wast:334
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\xc0\xa0");

// utf8-custom-section-id.wast:344
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\xfd\xa0");

// utf8-custom-section-id.wast:354
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\x00\x80");

// utf8-custom-section-id.wast:364
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\x7f\x80");

// utf8-custom-section-id.wast:374
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\xc0\x80");

// utf8-custom-section-id.wast:384
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\xfd\x80");

// utf8-custom-section-id.wast:394
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\x00\x80");

// utf8-custom-section-id.wast:404
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\x7f\x80");

// utf8-custom-section-id.wast:414
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\xc0\x80");

// utf8-custom-section-id.wast:424
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\xfd\x80");

// utf8-custom-section-id.wast:434
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\x00\x80");

// utf8-custom-section-id.wast:444
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\x7f\x80");

// utf8-custom-section-id.wast:454
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\xa0\x80");

// utf8-custom-section-id.wast:464
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\xa0\xbf");

// utf8-custom-section-id.wast:474
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\xbf\x80");

// utf8-custom-section-id.wast:484
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\xbf\xbf");

// utf8-custom-section-id.wast:494
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\xc0\x80");

// utf8-custom-section-id.wast:504
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\xfd\x80");

// utf8-custom-section-id.wast:514
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\x00\x80");

// utf8-custom-section-id.wast:524
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\x7f\x80");

// utf8-custom-section-id.wast:534
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\xc0\x80");

// utf8-custom-section-id.wast:544
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\xfd\x80");

// utf8-custom-section-id.wast:554
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\x00\x80");

// utf8-custom-section-id.wast:564
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\x7f\x80");

// utf8-custom-section-id.wast:574
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\xc0\x80");

// utf8-custom-section-id.wast:584
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\xfd\x80");

// utf8-custom-section-id.wast:596
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\xa0\x00");

// utf8-custom-section-id.wast:606
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\xa0\x7f");

// utf8-custom-section-id.wast:616
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\xa0\xc0");

// utf8-custom-section-id.wast:626
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe0\xa0\xfd");

// utf8-custom-section-id.wast:636
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\x80\x00");

// utf8-custom-section-id.wast:646
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\x80\x7f");

// utf8-custom-section-id.wast:656
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\x80\xc0");

// utf8-custom-section-id.wast:666
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xe1\x80\xfd");

// utf8-custom-section-id.wast:676
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\x80\x00");

// utf8-custom-section-id.wast:686
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\x80\x7f");

// utf8-custom-section-id.wast:696
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\x80\xc0");

// utf8-custom-section-id.wast:706
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xec\x80\xfd");

// utf8-custom-section-id.wast:716
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\x80\x00");

// utf8-custom-section-id.wast:726
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\x80\x7f");

// utf8-custom-section-id.wast:736
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\x80\xc0");

// utf8-custom-section-id.wast:746
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xed\x80\xfd");

// utf8-custom-section-id.wast:756
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\x80\x00");

// utf8-custom-section-id.wast:766
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\x80\x7f");

// utf8-custom-section-id.wast:776
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\x80\xc0");

// utf8-custom-section-id.wast:786
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xee\x80\xfd");

// utf8-custom-section-id.wast:796
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\x80\x00");

// utf8-custom-section-id.wast:806
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\x80\x7f");

// utf8-custom-section-id.wast:816
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\x80\xc0");

// utf8-custom-section-id.wast:826
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xef\x80\xfd");

// utf8-custom-section-id.wast:838
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x06\x05\xf1\x80\x80\x80\x80");

// utf8-custom-section-id.wast:848
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xf1\x80\x80");

// utf8-custom-section-id.wast:858
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\x80\x23");

// utf8-custom-section-id.wast:868
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xf1\x80");

// utf8-custom-section-id.wast:878
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xf1\x80\x23");

// utf8-custom-section-id.wast:888
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xf1");

// utf8-custom-section-id.wast:898
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xf1\x23");

// utf8-custom-section-id.wast:910
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x00\x90\x90");

// utf8-custom-section-id.wast:920
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x7f\x90\x90");

// utf8-custom-section-id.wast:930
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x80\x80\x80");

// utf8-custom-section-id.wast:940
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x80\x90\x90");

// utf8-custom-section-id.wast:950
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x8f\x90\x90");

// utf8-custom-section-id.wast:960
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x8f\xbf\xbf");

// utf8-custom-section-id.wast:970
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\xc0\x90\x90");

// utf8-custom-section-id.wast:980
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\xfd\x90\x90");

// utf8-custom-section-id.wast:990
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x00\x80\x80");

// utf8-custom-section-id.wast:1000
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x7f\x80\x80");

// utf8-custom-section-id.wast:1010
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\xc0\x80\x80");

// utf8-custom-section-id.wast:1020
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\xfd\x80\x80");

// utf8-custom-section-id.wast:1030
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x00\x80\x80");

// utf8-custom-section-id.wast:1040
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x7f\x80\x80");

// utf8-custom-section-id.wast:1050
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\xc0\x80\x80");

// utf8-custom-section-id.wast:1060
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\xfd\x80\x80");

// utf8-custom-section-id.wast:1070
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x00\x80\x80");

// utf8-custom-section-id.wast:1080
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x7f\x80\x80");

// utf8-custom-section-id.wast:1090
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x90\x80\x80");

// utf8-custom-section-id.wast:1100
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\xbf\x80\x80");

// utf8-custom-section-id.wast:1110
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\xc0\x80\x80");

// utf8-custom-section-id.wast:1120
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\xfd\x80\x80");

// utf8-custom-section-id.wast:1130
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf5\x80\x80\x80");

// utf8-custom-section-id.wast:1140
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf7\x80\x80\x80");

// utf8-custom-section-id.wast:1150
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf7\xbf\xbf\xbf");

// utf8-custom-section-id.wast:1162
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\x00\x90");

// utf8-custom-section-id.wast:1172
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\x7f\x90");

// utf8-custom-section-id.wast:1182
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\xc0\x90");

// utf8-custom-section-id.wast:1192
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\xfd\x90");

// utf8-custom-section-id.wast:1202
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\x00\x80");

// utf8-custom-section-id.wast:1212
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\x7f\x80");

// utf8-custom-section-id.wast:1222
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\xc0\x80");

// utf8-custom-section-id.wast:1232
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\xfd\x80");

// utf8-custom-section-id.wast:1242
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\x00\x80");

// utf8-custom-section-id.wast:1252
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\x7f\x80");

// utf8-custom-section-id.wast:1262
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\xc0\x80");

// utf8-custom-section-id.wast:1272
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\xfd\x80");

// utf8-custom-section-id.wast:1282
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\x00\x80");

// utf8-custom-section-id.wast:1292
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\x7f\x80");

// utf8-custom-section-id.wast:1302
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\xc0\x80");

// utf8-custom-section-id.wast:1312
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\xfd\x80");

// utf8-custom-section-id.wast:1324
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\x90\x00");

// utf8-custom-section-id.wast:1334
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\x90\x7f");

// utf8-custom-section-id.wast:1344
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\x90\xc0");

// utf8-custom-section-id.wast:1354
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf0\x90\x90\xfd");

// utf8-custom-section-id.wast:1364
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\x80\x00");

// utf8-custom-section-id.wast:1374
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\x80\x7f");

// utf8-custom-section-id.wast:1384
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\x80\xc0");

// utf8-custom-section-id.wast:1394
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf1\x80\x80\xfd");

// utf8-custom-section-id.wast:1404
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\x80\x00");

// utf8-custom-section-id.wast:1414
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\x80\x7f");

// utf8-custom-section-id.wast:1424
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\x80\xc0");

// utf8-custom-section-id.wast:1434
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf3\x80\x80\xfd");

// utf8-custom-section-id.wast:1444
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\x80\x00");

// utf8-custom-section-id.wast:1454
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\x80\x7f");

// utf8-custom-section-id.wast:1464
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\x80\xc0");

// utf8-custom-section-id.wast:1474
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf4\x80\x80\xfd");

// utf8-custom-section-id.wast:1486
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x07\x06\xf8\x80\x80\x80\x80\x80");

// utf8-custom-section-id.wast:1496
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf8\x80\x80\x80");

// utf8-custom-section-id.wast:1506
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x06\x05\xf8\x80\x80\x80\x23");

// utf8-custom-section-id.wast:1516
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xf8\x80\x80");

// utf8-custom-section-id.wast:1526
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xf8\x80\x80\x23");

// utf8-custom-section-id.wast:1536
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xf8\x80");

// utf8-custom-section-id.wast:1546
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xf8\x80\x23");

// utf8-custom-section-id.wast:1556
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xf8");

// utf8-custom-section-id.wast:1566
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xf8\x23");

// utf8-custom-section-id.wast:1578
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x06\x05\xf8\x80\x80\x80\x80");

// utf8-custom-section-id.wast:1588
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x06\x05\xfb\xbf\xbf\xbf\xbf");

// utf8-custom-section-id.wast:1600
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x08\x07\xfc\x80\x80\x80\x80\x80\x80");

// utf8-custom-section-id.wast:1610
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x06\x05\xfc\x80\x80\x80\x80");

// utf8-custom-section-id.wast:1620
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x07\x06\xfc\x80\x80\x80\x80\x23");

// utf8-custom-section-id.wast:1630
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xfc\x80\x80\x80");

// utf8-custom-section-id.wast:1640
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x06\x05\xfc\x80\x80\x80\x23");

// utf8-custom-section-id.wast:1650
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xfc\x80\x80");

// utf8-custom-section-id.wast:1660
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xfc\x80\x80\x23");

// utf8-custom-section-id.wast:1670
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xfc\x80");

// utf8-custom-section-id.wast:1680
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x04\x03\xfc\x80\x23");

// utf8-custom-section-id.wast:1690
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xfc");

// utf8-custom-section-id.wast:1700
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xfc\x23");

// utf8-custom-section-id.wast:1712
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x07\x06\xfc\x80\x80\x80\x80\x80");

// utf8-custom-section-id.wast:1722
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x07\x06\xfd\xbf\xbf\xbf\xbf\xbf");

// utf8-custom-section-id.wast:1734
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xfe");

// utf8-custom-section-id.wast:1744
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x02\x01\xff");

// utf8-custom-section-id.wast:1754
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xfe\xff");

// utf8-custom-section-id.wast:1764
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\x00\x00\xfe\xff");

// utf8-custom-section-id.wast:1774
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x03\x02\xff\xfe");

// utf8-custom-section-id.wast:1784
assert_malformed("\x00\x61\x73\x6d\x01\x00\x00\x00\x00\x05\x04\xff\xfe\x00\x00");
