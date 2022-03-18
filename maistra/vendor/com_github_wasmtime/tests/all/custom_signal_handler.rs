#[cfg(target_os = "linux")]
mod tests {
    use anyhow::Result;
    use std::rc::Rc;
    use std::sync::atomic::{AtomicBool, Ordering};
    use wasmtime::unix::StoreExt;
    use wasmtime::*;

    const WAT1: &str = r#"
(module
  (func $hostcall_read (import "" "hostcall_read") (result i32))
  (func $read (export "read") (result i32)
    (i32.load (i32.const 0))
  )
  (func $read_out_of_bounds (export "read_out_of_bounds") (result i32)
    (i32.load
      (i32.mul
        ;; memory size in Wasm pages
        (memory.size)
        ;; Wasm page size
        (i32.const 65536)
      )
    )
  )
  (func (export "hostcall_read") (result i32)
    call $hostcall_read
  )
  (func $start
    (i32.store (i32.const 0) (i32.const 123))
  )
  (start $start)
  (memory (export "memory") 1 4)
)
"#;

    const WAT2: &str = r#"
(module
  (import "other_module" "read" (func $other_module.read (result i32)))
  (func $run (export "run") (result i32)
      call $other_module.read)
)
"#;

    fn invoke_export(instance: &Instance, func_name: &str) -> Result<i32> {
        let ret = instance.get_typed_func::<(), i32>(func_name)?.call(())?;
        Ok(ret)
    }

    // Locate "memory" export, get base address and size and set memory protection to PROT_NONE
    fn set_up_memory(instance: &Instance) -> (*mut u8, usize) {
        let mem_export = instance.get_memory("memory").unwrap();
        let base = mem_export.data_ptr();
        let length = mem_export.data_size();

        // So we can later trigger SIGSEGV by performing a read
        unsafe {
            libc::mprotect(base as *mut libc::c_void, length, libc::PROT_NONE);
        }

        println!("memory: base={:?}, length={}", base, length);

        (base, length)
    }

    fn handle_sigsegv(
        base: *mut u8,
        length: usize,
        signum: libc::c_int,
        siginfo: *const libc::siginfo_t,
    ) -> bool {
        println!("Hello from instance signal handler!");
        // SIGSEGV on Linux, SIGBUS on Mac
        if libc::SIGSEGV == signum || libc::SIGBUS == signum {
            let si_addr: *mut libc::c_void = unsafe { (*siginfo).si_addr() };
            // Any signal from within module's memory we handle ourselves
            let result = (si_addr as u64) < (base as u64) + (length as u64);
            // Remove protections so the execution may resume
            unsafe {
                libc::mprotect(
                    base as *mut libc::c_void,
                    length,
                    libc::PROT_READ | libc::PROT_WRITE,
                );
            }
            println!("signal handled: {}", result);
            result
        } else {
            // Otherwise, we forward to wasmtime's signal handler.
            false
        }
    }

    fn make_externs(store: &Store, module: &Module) -> Vec<Extern> {
        module
            .imports()
            .map(|import| {
                assert_eq!(Some("hostcall_read"), import.name());
                let func = Func::wrap(&store, {
                    move |caller: Caller<'_>| {
                        let mem = caller.get_export("memory").unwrap().into_memory().unwrap();
                        let memory = unsafe { mem.data_unchecked_mut() };
                        use std::convert::TryInto;
                        i32::from_le_bytes(memory[0..4].try_into().unwrap())
                    }
                });
                wasmtime::Extern::Func(func)
            })
            .collect::<Vec<_>>()
    }

    // This test will only succeed if the SIGSEGV signal originating from the
    // hostcall can be handled.
    #[test]
    fn test_custom_signal_handler_single_instance_hostcall() -> Result<()> {
        let engine = Engine::new(&Config::default())?;
        let store = Store::new(&engine);
        let module = Module::new(&engine, WAT1)?;

        let instance = Instance::new(&store, &module, &make_externs(&store, &module))?;

        let (base, length) = set_up_memory(&instance);
        unsafe {
            store.set_signal_handler(move |signum, siginfo, _| {
                handle_sigsegv(base, length, signum, siginfo)
            });
        }
        println!("calling hostcall_read...");
        let result = invoke_export(&instance, "hostcall_read").unwrap();
        assert_eq!(123, result);
        Ok(())
    }

    #[test]
    fn test_custom_signal_handler_single_instance() -> Result<()> {
        let engine = Engine::new(&Config::default())?;
        let store = Store::new(&engine);
        let module = Module::new(&engine, WAT1)?;

        let instance = Instance::new(&store, &module, &make_externs(&store, &module))?;

        let (base, length) = set_up_memory(&instance);
        unsafe {
            store.set_signal_handler(move |signum, siginfo, _| {
                handle_sigsegv(base, length, signum, siginfo)
            });
        }

        // these invoke wasmtime_call_trampoline from action.rs
        {
            println!("calling read...");
            let result = invoke_export(&instance, "read").expect("read succeeded");
            assert_eq!(123, result);
        }

        {
            println!("calling read_out_of_bounds...");
            let trap = invoke_export(&instance, "read_out_of_bounds")
                .unwrap_err()
                .downcast::<Trap>()?;
            assert!(
                trap.to_string()
                    .contains("wasm trap: out of bounds memory access"),
                "bad trap message: {:?}",
                trap.to_string()
            );
        }

        // these invoke wasmtime_call_trampoline from callable.rs
        {
            let read_func = instance.get_typed_func::<(), i32>("read")?;
            println!("calling read...");
            let result = read_func.call(()).expect("expected function not to trap");
            assert_eq!(123i32, result);
        }

        {
            let read_out_of_bounds_func =
                instance.get_typed_func::<(), i32>("read_out_of_bounds")?;
            println!("calling read_out_of_bounds...");
            let trap = read_out_of_bounds_func.call(()).unwrap_err();
            assert!(trap
                .to_string()
                .contains("wasm trap: out of bounds memory access"));
        }
        Ok(())
    }

    #[test]
    fn test_custom_signal_handler_multiple_instances() -> Result<()> {
        let engine = Engine::new(&Config::default())?;
        let store = Store::new(&engine);
        let module = Module::new(&engine, WAT1)?;

        // Set up multiple instances

        let instance1 = Instance::new(&store, &module, &make_externs(&store, &module))?;
        let instance1_handler_triggered = Rc::new(AtomicBool::new(false));

        unsafe {
            let (base1, length1) = set_up_memory(&instance1);

            store.set_signal_handler({
                let instance1_handler_triggered = instance1_handler_triggered.clone();
                move |_signum, _siginfo, _context| {
                    // Remove protections so the execution may resume
                    libc::mprotect(
                        base1 as *mut libc::c_void,
                        length1,
                        libc::PROT_READ | libc::PROT_WRITE,
                    );
                    instance1_handler_triggered.store(true, Ordering::SeqCst);
                    println!(
                        "Hello from instance1 signal handler! {}",
                        instance1_handler_triggered.load(Ordering::SeqCst)
                    );
                    true
                }
            });
        }

        // Invoke both instances and trigger both signal handlers

        // First instance1
        {
            let mut exports1 = instance1.exports();
            assert!(exports1.next().is_some());

            println!("calling instance1.read...");
            let result = invoke_export(&instance1, "read").expect("read succeeded");
            assert_eq!(123, result);
            assert_eq!(
                instance1_handler_triggered.load(Ordering::SeqCst),
                true,
                "instance1 signal handler has been triggered"
            );
        }

        let instance2 = Instance::new(&store, &module, &make_externs(&store, &module))
            .expect("failed to instantiate module");
        let instance2_handler_triggered = Rc::new(AtomicBool::new(false));

        unsafe {
            let (base2, length2) = set_up_memory(&instance2);

            store.set_signal_handler({
                let instance2_handler_triggered = instance2_handler_triggered.clone();
                move |_signum, _siginfo, _context| {
                    // Remove protections so the execution may resume
                    libc::mprotect(
                        base2 as *mut libc::c_void,
                        length2,
                        libc::PROT_READ | libc::PROT_WRITE,
                    );
                    instance2_handler_triggered.store(true, Ordering::SeqCst);
                    println!(
                        "Hello from instance2 signal handler! {}",
                        instance2_handler_triggered.load(Ordering::SeqCst)
                    );
                    true
                }
            });
        }

        // And then instance2
        {
            let mut exports2 = instance2.exports();
            assert!(exports2.next().is_some());

            println!("calling instance2.read...");
            let result = invoke_export(&instance2, "read").expect("read succeeded");
            assert_eq!(123, result);
            assert_eq!(
                instance2_handler_triggered.load(Ordering::SeqCst),
                true,
                "instance1 signal handler has been triggered"
            );
        }
        Ok(())
    }

    #[test]
    fn test_custom_signal_handler_instance_calling_another_instance() -> Result<()> {
        let engine = Engine::new(&Config::default())?;
        let store = Store::new(&engine);

        // instance1 which defines 'read'
        let module1 = Module::new(&engine, WAT1)?;
        let instance1 = Instance::new(&store, &module1, &make_externs(&store, &module1))?;
        let (base1, length1) = set_up_memory(&instance1);
        unsafe {
            store.set_signal_handler(move |signum, siginfo, _| {
                println!("instance1");
                handle_sigsegv(base1, length1, signum, siginfo)
            });
        }

        let mut instance1_exports = instance1.exports();
        let instance1_read = instance1_exports.next().unwrap();

        // instance2 which calls 'instance1.read'
        let module2 = Module::new(&engine, WAT2)?;
        let instance2 = Instance::new(&store, &module2, &[instance1_read.into_extern()])?;
        // since 'instance2.run' calls 'instance1.read' we need to set up the signal handler to handle
        // SIGSEGV originating from within the memory of instance1
        unsafe {
            store.set_signal_handler(move |signum, siginfo, _| {
                handle_sigsegv(base1, length1, signum, siginfo)
            });
        }

        println!("calling instance2.run");
        let result = invoke_export(&instance2, "run")?;
        assert_eq!(123, result);
        Ok(())
    }
}
