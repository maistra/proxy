use std::cell::RefCell;
use std::future::Future;
use std::pin::Pin;
use std::rc::Rc;
use std::task::{Context, Poll, RawWaker, RawWakerVTable, Waker};

wasmtime_wiggle::from_witx!({
    witx: ["$CARGO_MANIFEST_DIR/tests/atoms.witx"],
    async: {
        atoms::{double_int_return_float}
    }
});

wasmtime_wiggle::wasmtime_integration!({
    target: crate,
    witx: ["$CARGO_MANIFEST_DIR/tests/atoms.witx"],
    ctx: Ctx,
    modules: { atoms => { name: Atoms } },
    async: {
        atoms::double_int_return_float
    }
});

pub struct Ctx;
impl wiggle::GuestErrorType for types::Errno {
    fn success() -> Self {
        types::Errno::Ok
    }
}

#[wasmtime_wiggle::async_trait]
impl atoms::Atoms for Ctx {
    fn int_float_args(&self, an_int: u32, an_float: f32) -> Result<(), types::Errno> {
        println!("INT FLOAT ARGS: {} {}", an_int, an_float);
        Ok(())
    }
    async fn double_int_return_float(
        &self,
        an_int: u32,
    ) -> Result<types::AliasToFloat, types::Errno> {
        Ok((an_int as f32) * 2.0)
    }
}

fn run_sync_func(linker: &wasmtime::Linker) {
    let shim_mod = shim_module(linker.store());
    let shim_inst = run(linker.instantiate_async(&shim_mod)).unwrap();

    let results = run(shim_inst
        .get_func("int_float_args_shim")
        .unwrap()
        .call_async(&[0i32.into(), 123.45f32.into()]))
    .unwrap();

    assert_eq!(results.len(), 1, "one return value");
    assert_eq!(
        results[0].unwrap_i32(),
        types::Errno::Ok as i32,
        "int_float_args errno"
    );
}

fn run_async_func(linker: &wasmtime::Linker) {
    let shim_mod = shim_module(linker.store());
    let shim_inst = run(linker.instantiate_async(&shim_mod)).unwrap();

    let input: i32 = 123;
    let result_location: i32 = 0;

    let results = run(shim_inst
        .get_func("double_int_return_float_shim")
        .unwrap()
        .call_async(&[input.into(), result_location.into()]))
    .unwrap();

    assert_eq!(results.len(), 1, "one return value");
    assert_eq!(
        results[0].unwrap_i32(),
        types::Errno::Ok as i32,
        "double_int_return_float errno"
    );

    // The actual result is in memory:
    let mem = shim_inst.get_memory("memory").unwrap();
    let mut result_bytes: [u8; 4] = [0, 0, 0, 0];
    mem.read(result_location as usize, &mut result_bytes)
        .unwrap();
    let result = f32::from_le_bytes(result_bytes);
    assert_eq!((input * 2) as f32, result);
}

#[test]
fn test_sync_host_func() {
    let store = async_store();

    let ctx = Rc::new(RefCell::new(Ctx));
    let atoms = Atoms::new(&store, ctx.clone());

    let mut linker = wasmtime::Linker::new(&store);
    atoms.add_to_linker(&mut linker).unwrap();

    run_sync_func(&linker);
}

#[test]
fn test_async_host_func() {
    let store = async_store();

    let ctx = Rc::new(RefCell::new(Ctx));
    let atoms = Atoms::new(&store, ctx.clone());

    let mut linker = wasmtime::Linker::new(&store);
    atoms.add_to_linker(&mut linker).unwrap();

    run_async_func(&linker);
}

#[test]
fn test_sync_config_host_func() {
    let mut config = wasmtime::Config::new();
    config.async_support(true);
    Atoms::add_to_config(&mut config);

    let engine = wasmtime::Engine::new(&config).unwrap();
    let store = wasmtime::Store::new(&engine);

    assert!(Atoms::set_context(&store, Ctx).is_ok());

    let linker = wasmtime::Linker::new(&store);
    run_sync_func(&linker);
}

#[test]
fn test_async_config_host_func() {
    let mut config = wasmtime::Config::new();
    config.async_support(true);
    Atoms::add_to_config(&mut config);

    let engine = wasmtime::Engine::new(&config).unwrap();
    let store = wasmtime::Store::new(&engine);

    assert!(Atoms::set_context(&store, Ctx).is_ok());

    let linker = wasmtime::Linker::new(&store);
    run_async_func(&linker);
}

fn run<F: Future>(future: F) -> F::Output {
    let mut f = Pin::from(Box::new(future));
    let waker = dummy_waker();
    let mut cx = Context::from_waker(&waker);
    loop {
        match f.as_mut().poll(&mut cx) {
            Poll::Ready(val) => break val,
            Poll::Pending => {}
        }
    }
}

fn dummy_waker() -> Waker {
    return unsafe { Waker::from_raw(clone(5 as *const _)) };

    unsafe fn clone(ptr: *const ()) -> RawWaker {
        assert_eq!(ptr as usize, 5);
        const VTABLE: RawWakerVTable = RawWakerVTable::new(clone, wake, wake_by_ref, drop);
        RawWaker::new(ptr, &VTABLE)
    }

    unsafe fn wake(ptr: *const ()) {
        assert_eq!(ptr as usize, 5);
    }

    unsafe fn wake_by_ref(ptr: *const ()) {
        assert_eq!(ptr as usize, 5);
    }

    unsafe fn drop(ptr: *const ()) {
        assert_eq!(ptr as usize, 5);
    }
}

fn async_store() -> wasmtime::Store {
    wasmtime::Store::new(
        &wasmtime::Engine::new(wasmtime::Config::new().async_support(true)).unwrap(),
    )
}

// Wiggle expects the caller to have an exported memory. Wasmtime can only
// provide this if the caller is a WebAssembly module, so we need to write
// a shim module:
fn shim_module(store: &wasmtime::Store) -> wasmtime::Module {
    wasmtime::Module::new(
        store.engine(),
        r#"
        (module
            (memory 1)
            (export "memory" (memory 0))
            (import "atoms" "int_float_args" (func $int_float_args (param i32 f32) (result i32)))
            (import "atoms" "double_int_return_float" (func $double_int_return_float (param i32 i32) (result i32)))

            (func $int_float_args_shim (param i32 f32) (result i32)
                local.get 0
                local.get 1
                call $int_float_args
            )
            (func $double_int_return_float_shim (param i32 i32) (result i32)
                local.get 0
                local.get 1
                call $double_int_return_float
            )
            (export "int_float_args_shim" (func $int_float_args_shim))
            (export "double_int_return_float_shim" (func $double_int_return_float_shim))
        )
    "#,
    )
    .unwrap()
}
