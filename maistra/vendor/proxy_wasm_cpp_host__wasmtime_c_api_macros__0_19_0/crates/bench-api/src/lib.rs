//! A C API for benchmarking Wasmtime's WebAssembly compilation, instantiation,
//! and execution.
//!
//! The API expects sequential calls to:
//!
//!  - `wasm_bench_create`
//!  - `wasm_bench_compile`
//!  - `wasm_bench_instantiate`
//!  - `wasm_bench_execute`
//!  - `wasm_bench_free`
//!
//! You may repeat this sequence of calls multiple times to take multiple
//! measurements of compilation, instantiation, and execution time within a
//! single process.
//!
//! All API calls must happen on the same thread.
//!
//! Functions which return pointers use null as an error value. Function which
//! return `int` use `0` as OK and non-zero as an error value.
//!
//! # Example
//!
//! ```
//! use std::ptr;
//! use wasmtime_bench_api::*;
//!
//! let working_dir = std::env::current_dir().unwrap().display().to_string();
//! let mut bench_api = ptr::null_mut();
//! unsafe {
//!     let code = wasm_bench_create(working_dir.as_ptr(), working_dir.len(), &mut bench_api);
//!     assert_eq!(code, OK);
//!     assert!(!bench_api.is_null());
//! };
//!
//! let wasm = wat::parse_bytes(br#"
//!     (module
//!         (func $bench_start (import "bench" "start"))
//!         (func $bench_end (import "bench" "end"))
//!         (func $start (export "_start")
//!             call $bench_start
//!             i32.const 1
//!             i32.const 2
//!             i32.add
//!             drop
//!             call $bench_end
//!         )
//!     )
//! "#).unwrap();
//!
//! // Start your compilation timer here.
//! let code = unsafe { wasm_bench_compile(bench_api, wasm.as_ptr(), wasm.len()) };
//! // End your compilation timer here.
//! assert_eq!(code, OK);
//!
//! // The Wasm benchmark will expect us to provide functions to start ("bench"
//! // "start") and stop ("bench" "stop") the measurement counters/timers during
//! // execution.
//! extern "C" fn bench_start() {
//!     // Start your execution timer here.
//! }
//! extern "C" fn bench_stop() {
//!     // End your execution timer here.
//! }
//!
//! // Start your instantiation timer here.
//! let code = unsafe { wasm_bench_instantiate(bench_api, bench_start, bench_stop) };
//! // End your instantiation timer here.
//! assert_eq!(code, OK);
//!
//! // No need to start timers for the execution since, by convention, the timer
//! // functions we passed during instantiation will be called by the benchmark
//! // at the appropriate time (before and after the benchmarked section).
//! let code = unsafe { wasm_bench_execute(bench_api) };
//! assert_eq!(code, OK);
//!
//! unsafe {
//!     wasm_bench_free(bench_api);
//! }
//! ```

use anyhow::{anyhow, Context, Result};
use std::env;
use std::os::raw::{c_int, c_void};
use std::path::Path;
use std::slice;
use wasi_cap_std_sync::WasiCtxBuilder;
use wasmtime::{Config, Engine, Instance, Linker, Module, Store};
use wasmtime_wasi::Wasi;

pub type ExitCode = c_int;
pub const OK: ExitCode = 0;
pub const ERR: ExitCode = -1;

// Randomize the location of heap objects to avoid accidental locality being an
// uncontrolled variable that obscures performance evaluation in our
// experiments.
#[cfg(feature = "shuffling-allocator")]
#[global_allocator]
static ALLOC: shuffling_allocator::ShufflingAllocator<std::alloc::System> =
    shuffling_allocator::wrap!(&std::alloc::System);

/// Exposes a C-compatible way of creating the engine from the bytes of a single
/// Wasm module.
///
/// On success, the `out_bench_ptr` is initialized to a pointer to a structure
/// that contains the engine's initialized state, and `0` is returned. On
/// failure, a non-zero status code is returned and `out_bench_ptr` is left
/// untouched.
#[no_mangle]
pub extern "C" fn wasm_bench_create(
    working_dir_ptr: *const u8,
    working_dir_len: usize,
    out_bench_ptr: *mut *mut c_void,
) -> ExitCode {
    let result = (|| -> Result<_> {
        let working_dir = unsafe { std::slice::from_raw_parts(working_dir_ptr, working_dir_len) };
        let working_dir = std::str::from_utf8(working_dir)
            .context("given working directory is not valid UTF-8")?;
        let state = Box::new(BenchState::new(working_dir)?);
        Ok(Box::into_raw(state) as _)
    })();

    if let Ok(bench_ptr) = result {
        unsafe {
            assert!(!out_bench_ptr.is_null());
            *out_bench_ptr = bench_ptr;
        }
    }

    to_exit_code(result.map(|_| ()))
}

/// Free the engine state allocated by this library.
#[no_mangle]
pub extern "C" fn wasm_bench_free(state: *mut c_void) {
    assert!(!state.is_null());
    unsafe {
        Box::from_raw(state as *mut BenchState);
    }
}

/// Compile the Wasm benchmark module.
#[no_mangle]
pub extern "C" fn wasm_bench_compile(
    state: *mut c_void,
    wasm_bytes: *const u8,
    wasm_bytes_length: usize,
) -> ExitCode {
    let state = unsafe { (state as *mut BenchState).as_mut().unwrap() };
    let wasm_bytes = unsafe { slice::from_raw_parts(wasm_bytes, wasm_bytes_length) };
    let result = state.compile(wasm_bytes).context("failed to compile");
    to_exit_code(result)
}

/// Instantiate the Wasm benchmark module.
#[no_mangle]
pub extern "C" fn wasm_bench_instantiate(
    state: *mut c_void,
    bench_start: extern "C" fn(),
    bench_end: extern "C" fn(),
) -> ExitCode {
    let state = unsafe { (state as *mut BenchState).as_mut().unwrap() };
    let result = state
        .instantiate(bench_start, bench_end)
        .context("failed to instantiate");
    to_exit_code(result)
}

/// Execute the Wasm benchmark module.
#[no_mangle]
pub extern "C" fn wasm_bench_execute(state: *mut c_void) -> ExitCode {
    let state = unsafe { (state as *mut BenchState).as_mut().unwrap() };
    let result = state.execute().context("failed to execute");
    to_exit_code(result)
}

/// Helper function for converting a Rust result to a C error code.
///
/// This will print an error indicating some information regarding the failure.
fn to_exit_code<T>(result: impl Into<Result<T>>) -> ExitCode {
    match result.into() {
        Ok(_) => OK,
        Err(error) => {
            eprintln!("{:?}", error);
            ERR
        }
    }
}

/// This structure contains the actual Rust implementation of the state required
/// to manage the Wasmtime engine between calls.
struct BenchState {
    engine: Engine,
    linker: Linker,
    module: Option<Module>,
    instance: Option<Instance>,
    did_execute: bool,
}

impl BenchState {
    fn new(working_dir: impl AsRef<Path>) -> Result<Self> {
        let mut config = Config::new();
        config.wasm_simd(true);
        // NB: do not configure a code cache.

        let engine = Engine::new(&config)?;
        let store = Store::new(&engine);

        let mut linker = Linker::new(&store);

        // Create a WASI environment.

        let mut cx = WasiCtxBuilder::new();
        cx = cx.inherit_stdio();
        // Allow access to the working directory so that the benchmark can read
        // its input workload(s).
        let working_dir = unsafe { cap_std::fs::Dir::open_ambient_dir(working_dir) }
            .context("failed to preopen the working directory")?;
        cx = cx.preopened_dir(working_dir, ".")?;
        // Pass this env var along so that the benchmark program can use smaller
        // input workload(s) if it has them and that has been requested.
        if let Ok(val) = env::var("WASM_BENCH_USE_SMALL_WORKLOAD") {
            cx = cx.env("WASM_BENCH_USE_SMALL_WORKLOAD", &val)?;
        }

        Wasi::new(linker.store(), cx.build()?).add_to_linker(&mut linker)?;

        #[cfg(feature = "wasi-nn")]
        {
            use std::cell::RefCell;
            use std::rc::Rc;
            use wasmtime_wasi_nn::{WasiNn, WasiNnCtx};

            let wasi_nn = WasiNn::new(linker.store(), Rc::new(RefCell::new(WasiNnCtx::new()?)));
            wasi_nn.add_to_linker(&mut linker)?;
        }

        #[cfg(feature = "wasi-crypto")]
        {
            use std::cell::RefCell;
            use std::rc::Rc;
            use wasmtime_wasi_crypto::{
                WasiCryptoAsymmetricCommon, WasiCryptoCommon, WasiCryptoCtx, WasiCryptoSignatures,
                WasiCryptoSymmetric,
            };

            let cx_crypto = Rc::new(RefCell::new(WasiCryptoCtx::new()));
            WasiCryptoCommon::new(linker.store(), cx_crypto.clone()).add_to_linker(linker)?;
            WasiCryptoAsymmetricCommon::new(linker.store(), cx_crypto.clone())
                .add_to_linker(linker)?;
            WasiCryptoSignatures::new(linker.store(), cx_crypto.clone()).add_to_linker(linker)?;
            WasiCryptoSymmetric::new(linker.store(), cx_crypto).add_to_linker(linker)?;
        }

        Ok(Self {
            engine,
            linker,
            module: None,
            instance: None,
            did_execute: false,
        })
    }

    fn compile(&mut self, bytes: &[u8]) -> Result<()> {
        assert!(
            self.module.is_none(),
            "create a new engine to repeat compilation"
        );
        self.module = Some(Module::from_binary(&self.engine, bytes)?);
        Ok(())
    }

    fn instantiate(
        &mut self,
        bench_start: extern "C" fn(),
        bench_end: extern "C" fn(),
    ) -> Result<()> {
        assert!(
            self.instance.is_none(),
            "create a new engine to repeat instantiation"
        );
        let module = self
            .module
            .as_mut()
            .expect("compile the module before instantiating it");

        // Import the specialized benchmarking functions.
        self.linker.func("bench", "start", move || bench_start())?;
        self.linker.func("bench", "end", move || bench_end())?;

        self.instance = Some(self.linker.instantiate(&module)?);
        Ok(())
    }

    fn execute(&mut self) -> Result<()> {
        assert!(!self.did_execute, "create a new engine to repeat execution");
        self.did_execute = true;

        let instance = self
            .instance
            .as_ref()
            .expect("instantiate the module before executing it");

        let start_func = instance.get_typed_func::<(), ()>("_start")?;
        match start_func.call(()) {
            Ok(_) => Ok(()),
            Err(trap) => {
                // Since _start will likely return by using the system `exit` call, we must
                // check the trap code to see if it actually represents a successful exit.
                match trap.i32_exit_status() {
                    Some(0) => Ok(()),
                    Some(n) => Err(anyhow!("_start exited with a non-zero code: {}", n)),
                    None => Err(anyhow!(
                        "executing the benchmark resulted in a trap: {}",
                        trap
                    )),
                }
            }
        }
    }
}
