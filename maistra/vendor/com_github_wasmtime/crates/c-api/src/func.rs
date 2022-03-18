use crate::{wasm_extern_t, wasm_functype_t, wasm_store_t, wasm_val_t, wasm_val_vec_t};
use crate::{wasm_name_t, wasm_trap_t, wasmtime_error_t};
use anyhow::anyhow;
use std::ffi::c_void;
use std::mem::MaybeUninit;
use std::panic::{self, AssertUnwindSafe};
use std::ptr;
use std::str;
use wasmtime::{Caller, Extern, Func, Trap, Val};

#[derive(Clone)]
#[repr(transparent)]
pub struct wasm_func_t {
    ext: wasm_extern_t,
}

wasmtime_c_api_macros::declare_ref!(wasm_func_t);

#[repr(C)]
pub struct wasmtime_caller_t<'a> {
    caller: Caller<'a>,
}

pub type wasm_func_callback_t = extern "C" fn(
    args: *const wasm_val_vec_t,
    results: *mut wasm_val_vec_t,
) -> Option<Box<wasm_trap_t>>;

pub type wasm_func_callback_with_env_t = extern "C" fn(
    env: *mut std::ffi::c_void,
    args: *const wasm_val_vec_t,
    results: *mut wasm_val_vec_t,
) -> Option<Box<wasm_trap_t>>;

pub type wasmtime_func_callback_t = extern "C" fn(
    caller: *const wasmtime_caller_t,
    args: *const wasm_val_vec_t,
    results: *mut wasm_val_vec_t,
) -> Option<Box<wasm_trap_t>>;

pub type wasmtime_func_callback_with_env_t = extern "C" fn(
    caller: *const wasmtime_caller_t,
    env: *mut std::ffi::c_void,
    args: *const wasm_val_vec_t,
    results: *mut wasm_val_vec_t,
) -> Option<Box<wasm_trap_t>>;

struct Finalizer {
    env: *mut c_void,
    finalizer: Option<extern "C" fn(*mut c_void)>,
}

impl Drop for Finalizer {
    fn drop(&mut self) {
        if let Some(f) = self.finalizer {
            f(self.env);
        }
    }
}

impl wasm_func_t {
    pub(crate) fn try_from(e: &wasm_extern_t) -> Option<&wasm_func_t> {
        match &e.which {
            Extern::Func(_) => Some(unsafe { &*(e as *const _ as *const _) }),
            _ => None,
        }
    }

    pub(crate) fn func(&self) -> &Func {
        match &self.ext.which {
            Extern::Func(f) => f,
            _ => unsafe { std::hint::unreachable_unchecked() },
        }
    }
}

impl From<Func> for wasm_func_t {
    fn from(func: Func) -> wasm_func_t {
        wasm_func_t {
            ext: wasm_extern_t { which: func.into() },
        }
    }
}

fn create_function(
    store: &wasm_store_t,
    ty: &wasm_functype_t,
    func: impl Fn(Caller<'_>, *const wasm_val_vec_t, *mut wasm_val_vec_t) -> Option<Box<wasm_trap_t>>
        + 'static,
) -> Box<wasm_func_t> {
    let store = &store.store;
    let ty = ty.ty().ty.clone();
    let func = Func::new(store, ty, move |caller, params, results| {
        let params: wasm_val_vec_t = params
            .iter()
            .cloned()
            .map(|p| wasm_val_t::from_val(p))
            .collect::<Vec<_>>()
            .into();
        let mut out_results: wasm_val_vec_t = vec![wasm_val_t::default(); results.len()].into();
        let out = func(caller, &params, &mut out_results);
        if let Some(trap) = out {
            return Err(trap.trap.clone());
        }

        let out_results = out_results.as_slice();
        for i in 0..results.len() {
            results[i] = out_results[i].val();
        }
        Ok(())
    });
    Box::new(func.into())
}

#[no_mangle]
pub extern "C" fn wasm_func_new(
    store: &wasm_store_t,
    ty: &wasm_functype_t,
    callback: wasm_func_callback_t,
) -> Box<wasm_func_t> {
    create_function(store, ty, move |_caller, params, results| {
        callback(params, results)
    })
}

#[no_mangle]
pub unsafe extern "C" fn wasmtime_func_new(
    store: &wasm_store_t,
    ty: &wasm_functype_t,
    callback: wasmtime_func_callback_t,
) -> Box<wasm_func_t> {
    create_function(store, ty, move |caller, params, results| {
        callback(&wasmtime_caller_t { caller }, params, results)
    })
}

#[no_mangle]
pub extern "C" fn wasm_func_new_with_env(
    store: &wasm_store_t,
    ty: &wasm_functype_t,
    callback: wasm_func_callback_with_env_t,
    env: *mut c_void,
    finalizer: Option<extern "C" fn(arg1: *mut std::ffi::c_void)>,
) -> Box<wasm_func_t> {
    let finalizer = Finalizer { env, finalizer };
    create_function(store, ty, move |_caller, params, results| {
        callback(finalizer.env, params, results)
    })
}

#[no_mangle]
pub extern "C" fn wasmtime_func_new_with_env(
    store: &wasm_store_t,
    ty: &wasm_functype_t,
    callback: wasmtime_func_callback_with_env_t,
    env: *mut c_void,
    finalizer: Option<extern "C" fn(*mut c_void)>,
) -> Box<wasm_func_t> {
    let finalizer = Finalizer { env, finalizer };
    create_function(store, ty, move |caller, params, results| {
        callback(
            &wasmtime_caller_t { caller },
            finalizer.env,
            params,
            results,
        )
    })
}

#[no_mangle]
pub unsafe extern "C" fn wasm_func_call(
    wasm_func: &wasm_func_t,
    args: *const wasm_val_vec_t,
    results: *mut wasm_val_vec_t,
) -> *mut wasm_trap_t {
    let mut trap = ptr::null_mut();
    let error = _wasmtime_func_call(
        wasm_func,
        (*args).as_slice(),
        (*results).as_uninit_slice(),
        &mut trap,
    );
    match error {
        Some(err) => Box::into_raw(err.to_trap()),
        None => trap,
    }
}

#[no_mangle]
pub unsafe extern "C" fn wasmtime_func_call(
    func: &wasm_func_t,
    args: *const wasm_val_vec_t,
    results: *mut wasm_val_vec_t,
    trap_ptr: &mut *mut wasm_trap_t,
) -> Option<Box<wasmtime_error_t>> {
    _wasmtime_func_call(
        func,
        (*args).as_slice(),
        (*results).as_uninit_slice(),
        trap_ptr,
    )
}

fn _wasmtime_func_call(
    func: &wasm_func_t,
    args: &[wasm_val_t],
    results: &mut [MaybeUninit<wasm_val_t>],
    trap_ptr: &mut *mut wasm_trap_t,
) -> Option<Box<wasmtime_error_t>> {
    let func = func.func();
    if results.len() != func.result_arity() {
        return Some(Box::new(anyhow!("wrong number of results provided").into()));
    }
    let params = args.iter().map(|i| i.val()).collect::<Vec<_>>();

    // We're calling arbitrary code here most of the time, and we in general
    // want to try to insulate callers against bugs in wasmtime/wasi/etc if we
    // can. As a result we catch panics here and transform them to traps to
    // allow the caller to have any insulation possible against Rust panics.
    let result = panic::catch_unwind(AssertUnwindSafe(|| func.call(&params)));
    match result {
        Ok(Ok(out)) => {
            for (slot, val) in results.iter_mut().zip(out.into_vec().into_iter()) {
                crate::initialize(slot, wasm_val_t::from_val(val));
            }
            None
        }
        Ok(Err(trap)) => match trap.downcast::<Trap>() {
            Ok(trap) => {
                *trap_ptr = Box::into_raw(Box::new(wasm_trap_t::new(trap)));
                None
            }
            Err(err) => Some(Box::new(err.into())),
        },
        Err(panic) => {
            let trap = if let Some(msg) = panic.downcast_ref::<String>() {
                Trap::new(msg)
            } else if let Some(msg) = panic.downcast_ref::<&'static str>() {
                Trap::new(*msg)
            } else {
                Trap::new("rust panic happened")
            };
            let trap = Box::new(wasm_trap_t::new(trap));
            *trap_ptr = Box::into_raw(trap);
            None
        }
    }
}

#[no_mangle]
pub extern "C" fn wasm_func_type(f: &wasm_func_t) -> Box<wasm_functype_t> {
    Box::new(wasm_functype_t::new(f.func().ty()))
}

#[no_mangle]
pub extern "C" fn wasm_func_param_arity(f: &wasm_func_t) -> usize {
    f.func().param_arity()
}

#[no_mangle]
pub extern "C" fn wasm_func_result_arity(f: &wasm_func_t) -> usize {
    f.func().result_arity()
}

#[no_mangle]
pub extern "C" fn wasm_func_as_extern(f: &mut wasm_func_t) -> &mut wasm_extern_t {
    &mut (*f).ext
}

#[no_mangle]
pub extern "C" fn wasmtime_caller_export_get(
    caller: &wasmtime_caller_t,
    name: &wasm_name_t,
) -> Option<Box<wasm_extern_t>> {
    let name = str::from_utf8(name.as_slice()).ok()?;
    let which = caller.caller.get_export(name)?;
    Some(Box::new(wasm_extern_t { which }))
}

#[no_mangle]
pub extern "C" fn wasmtime_func_as_funcref(
    func: &wasm_func_t,
    funcrefp: &mut MaybeUninit<wasm_val_t>,
) {
    let funcref = wasm_val_t::from_val(Val::FuncRef(Some(func.func().clone())));
    crate::initialize(funcrefp, funcref);
}

#[no_mangle]
pub extern "C" fn wasmtime_funcref_as_func(val: &wasm_val_t) -> Option<Box<wasm_func_t>> {
    if let Val::FuncRef(Some(f)) = val.val() {
        Some(Box::new(f.into()))
    } else {
        None
    }
}
