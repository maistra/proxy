use crate::{FrameInfo, Store};
use backtrace::Backtrace;
use std::fmt;
use std::sync::Arc;
use wasmtime_environ::ir;

/// A struct representing an aborted instruction execution, with a message
/// indicating the cause.
#[derive(Clone)]
pub struct Trap {
    inner: Arc<TrapInner>,
}

/// State describing the occasion which evoked a trap.
#[derive(Debug)]
enum TrapReason {
    /// An error message describing a trap.
    Message(String),

    /// An `i32` exit status describing an explicit program exit.
    I32Exit(i32),

    /// A structured error describing a trap.
    Error(Box<dyn std::error::Error + Send + Sync>),

    /// A specific code for a trap triggered while executing WASM.
    InstructionTrap(TrapCode),
}

impl fmt::Display for TrapReason {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TrapReason::Message(s) => write!(f, "{}", s),
            TrapReason::I32Exit(status) => write!(f, "Exited with i32 exit status {}", status),
            TrapReason::Error(e) => write!(f, "{}", e),
            TrapReason::InstructionTrap(code) => write!(f, "wasm trap: {}", code),
        }
    }
}

/// A trap code describing the reason for a trap.
///
/// All trap instructions have an explicit trap code.
#[non_exhaustive]
#[derive(Clone, Copy, PartialEq, Eq, Debug, Hash)]
pub enum TrapCode {
    /// The current stack space was exhausted.
    StackOverflow,

    /// An out-of-bounds memory access.
    MemoryOutOfBounds,

    /// A wasm atomic operation was presented with a not-naturally-aligned linear-memory address.
    HeapMisaligned,

    /// An out-of-bounds access to a table.
    TableOutOfBounds,

    /// Indirect call to a null table entry.
    IndirectCallToNull,

    /// Signature mismatch on indirect call.
    BadSignature,

    /// An integer arithmetic operation caused an overflow.
    IntegerOverflow,

    /// An integer division by zero.
    IntegerDivisionByZero,

    /// Failed float-to-int conversion.
    BadConversionToInteger,

    /// Code that was supposed to have been unreachable was reached.
    UnreachableCodeReached,

    /// Execution has potentially run too long and may be interrupted.
    Interrupt,
}

impl TrapCode {
    /// Panics if `code` is `ir::TrapCode::User`.
    fn from_non_user(code: ir::TrapCode) -> Self {
        match code {
            ir::TrapCode::StackOverflow => TrapCode::StackOverflow,
            ir::TrapCode::HeapOutOfBounds => TrapCode::MemoryOutOfBounds,
            ir::TrapCode::HeapMisaligned => TrapCode::HeapMisaligned,
            ir::TrapCode::TableOutOfBounds => TrapCode::TableOutOfBounds,
            ir::TrapCode::IndirectCallToNull => TrapCode::IndirectCallToNull,
            ir::TrapCode::BadSignature => TrapCode::BadSignature,
            ir::TrapCode::IntegerOverflow => TrapCode::IntegerOverflow,
            ir::TrapCode::IntegerDivisionByZero => TrapCode::IntegerDivisionByZero,
            ir::TrapCode::BadConversionToInteger => TrapCode::BadConversionToInteger,
            ir::TrapCode::UnreachableCodeReached => TrapCode::UnreachableCodeReached,
            ir::TrapCode::Interrupt => TrapCode::Interrupt,
            ir::TrapCode::User(_) => panic!("Called `TrapCode::from_non_user` with user code"),
        }
    }
}

impl fmt::Display for TrapCode {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        use TrapCode::*;
        let desc = match self {
            StackOverflow => "call stack exhausted",
            MemoryOutOfBounds => "out of bounds memory access",
            HeapMisaligned => "misaligned memory access",
            TableOutOfBounds => "undefined element: out of bounds table access",
            IndirectCallToNull => "uninitialized element",
            BadSignature => "indirect call type mismatch",
            IntegerOverflow => "integer overflow",
            IntegerDivisionByZero => "integer divide by zero",
            BadConversionToInteger => "invalid conversion to integer",
            UnreachableCodeReached => "unreachable",
            Interrupt => "interrupt",
        };
        write!(f, "{}", desc)
    }
}

struct TrapInner {
    reason: TrapReason,
    wasm_trace: Vec<FrameInfo>,
    native_trace: Backtrace,
    hint_wasm_backtrace_details_env: bool,
}

fn _assert_trap_is_sync_and_send(t: &Trap) -> (&dyn Sync, &dyn Send) {
    (t, t)
}

impl Trap {
    /// Creates a new `Trap` with `message`.
    /// # Example
    /// ```
    /// let trap = wasmtime::Trap::new("unexpected error");
    /// assert!(trap.to_string().contains("unexpected error"));
    /// ```
    pub fn new<I: Into<String>>(message: I) -> Self {
        let reason = TrapReason::Message(message.into());
        Trap::new_with_trace(None, None, reason, Backtrace::new_unresolved())
    }

    /// Creates a new `Trap` representing an explicit program exit with a classic `i32`
    /// exit status value.
    pub fn i32_exit(status: i32) -> Self {
        Trap::new_with_trace(
            None,
            None,
            TrapReason::I32Exit(status),
            Backtrace::new_unresolved(),
        )
    }

    pub(crate) fn from_runtime(store: &Store, runtime_trap: wasmtime_runtime::Trap) -> Self {
        match runtime_trap {
            wasmtime_runtime::Trap::User(error) => Trap::from(error),
            wasmtime_runtime::Trap::Jit {
                pc,
                backtrace,
                maybe_interrupted,
            } => {
                let mut code = store
                    .frame_info()
                    .borrow()
                    .lookup_trap_info(pc)
                    .map(|info| info.trap_code)
                    .unwrap_or(ir::TrapCode::StackOverflow);
                if maybe_interrupted && code == ir::TrapCode::StackOverflow {
                    code = ir::TrapCode::Interrupt;
                }
                Trap::new_wasm(Some(store), Some(pc), code, backtrace)
            }
            wasmtime_runtime::Trap::Wasm {
                trap_code,
                backtrace,
            } => Trap::new_wasm(Some(store), None, trap_code, backtrace),
            wasmtime_runtime::Trap::OOM { backtrace } => {
                let reason = TrapReason::Message("out of memory".to_string());
                Trap::new_with_trace(Some(store), None, reason, backtrace)
            }
        }
    }

    pub(crate) fn new_wasm(
        store: Option<&Store>,
        trap_pc: Option<usize>,
        code: ir::TrapCode,
        backtrace: Backtrace,
    ) -> Self {
        let code = TrapCode::from_non_user(code);
        Trap::new_with_trace(store, trap_pc, TrapReason::InstructionTrap(code), backtrace)
    }

    /// Creates a new `Trap`.
    ///
    /// * `store` - this is optionally provided, if available. If `None` we'll
    ///   look up the last store, if available, used to call wasm code on the
    ///   stack.
    ///
    /// * `trap_pc` - this is the precise program counter, if available, that
    ///   wasm trapped at. This is used when learning about the wasm stack trace
    ///   to ensure we assign the correct source to every frame.
    ///
    /// * `reason` - this is the wasmtime-internal reason for why this trap is
    ///   being created.
    ///
    /// * `native_trace` - this is a captured backtrace from when the trap
    ///   occurred, and this will iterate over the frames to find frames that
    ///   lie in wasm jit code.
    fn new_with_trace(
        store: Option<&Store>,
        trap_pc: Option<usize>,
        reason: TrapReason,
        native_trace: Backtrace,
    ) -> Self {
        let mut wasm_trace = Vec::new();
        let mut hint_wasm_backtrace_details_env = false;
        wasmtime_runtime::with_last_info(|last| {
            // If the `store` passed in is `None` then we look at the `last`
            // store configured to call wasm, and if that's a `Store` we use
            // that. If that all fails then we just don't generate any
            // `wasm_trace` information.
            if let Some(store) = store.or_else(|| last?.downcast_ref()) {
                for frame in native_trace.frames() {
                    let pc = frame.ip() as usize;
                    if pc == 0 {
                        continue;
                    }
                    // Note that we need to be careful about the pc we pass in
                    // here to lookup frame information. This program counter is
                    // used to translate back to an original source location in
                    // the origin wasm module. If this pc is the exact pc that
                    // the trap happened at, then we look up that pc precisely.
                    // Otherwise backtrace information typically points at the
                    // pc *after* the call instruction (because otherwise it's
                    // likely a call instruction on the stack). In that case we
                    // want to lookup information for the previous instruction
                    // (the call instruction) so we subtract one as the lookup.
                    let pc_to_lookup = if Some(pc) == trap_pc { pc } else { pc - 1 };
                    if let Some((info, has_unparsed_debuginfo)) =
                        store.frame_info().borrow().lookup_frame_info(pc_to_lookup)
                    {
                        wasm_trace.push(info);

                        // If this frame has unparsed debug information and the
                        // store's configuration indicates that we were
                        // respecting the environment variable of whether to
                        // do this then we will print out a helpful note in
                        // `Display` to indicate that more detailed information
                        // in a trap may be available.
                        if has_unparsed_debuginfo
                            && store.engine().config().wasm_backtrace_details_env_used
                        {
                            hint_wasm_backtrace_details_env = true;
                        }
                    }
                }
            }
        });
        Trap {
            inner: Arc::new(TrapInner {
                reason,
                wasm_trace,
                native_trace,
                hint_wasm_backtrace_details_env,
            }),
        }
    }

    /// If the trap was the result of an explicit program exit with a classic
    /// `i32` exit status value, return the value, otherwise return `None`.
    pub fn i32_exit_status(&self) -> Option<i32> {
        match self.inner.reason {
            TrapReason::I32Exit(status) => Some(status),
            _ => None,
        }
    }

    /// Returns a list of function frames in WebAssembly code that led to this
    /// trap happening.
    pub fn trace(&self) -> &[FrameInfo] {
        &self.inner.wasm_trace
    }

    /// Code of a trap that happened while executing a WASM instruction.
    /// If the trap was triggered by a host export this will be `None`.
    pub fn trap_code(&self) -> Option<TrapCode> {
        match self.inner.reason {
            TrapReason::InstructionTrap(code) => Some(code),
            _ => None,
        }
    }
}

impl fmt::Debug for Trap {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Trap")
            .field("reason", &self.inner.reason)
            .field("wasm_trace", &self.inner.wasm_trace)
            .field("native_trace", &self.inner.native_trace)
            .finish()
    }
}

impl fmt::Display for Trap {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.inner.reason)?;
        let trace = self.trace();
        if trace.is_empty() {
            return Ok(());
        }
        writeln!(f, "\nwasm backtrace:")?;
        for (i, frame) in self.trace().iter().enumerate() {
            let name = frame.module_name().unwrap_or("<unknown>");
            write!(f, "  {:>3}: {:#6x} - ", i, frame.module_offset())?;

            let demangle =
                |f: &mut fmt::Formatter<'_>, name: &str| match rustc_demangle::try_demangle(name) {
                    Ok(name) => write!(f, "{}", name),
                    Err(_) => match cpp_demangle::Symbol::new(name) {
                        Ok(name) => write!(f, "{}", name),
                        Err(_) => write!(f, "{}", name),
                    },
                };
            let write_raw_func_name = |f: &mut fmt::Formatter<'_>| match frame.func_name() {
                Some(name) => demangle(f, name),
                None => write!(f, "<wasm function {}>", frame.func_index()),
            };
            if frame.symbols().is_empty() {
                write!(f, "{}!", name)?;
                write_raw_func_name(f)?;
                writeln!(f, "")?;
            } else {
                for (i, symbol) in frame.symbols().iter().enumerate() {
                    if i > 0 {
                        write!(f, "              - ")?;
                    } else {
                        // ...
                    }
                    match symbol.name() {
                        Some(name) => demangle(f, name)?,
                        None if i == 0 => write_raw_func_name(f)?,
                        None => write!(f, "<inlined function>")?,
                    }
                    writeln!(f, "")?;
                    if let Some(file) = symbol.file() {
                        write!(f, "                    at {}", file)?;
                        if let Some(line) = symbol.line() {
                            write!(f, ":{}", line)?;
                            if let Some(col) = symbol.column() {
                                write!(f, ":{}", col)?;
                            }
                        }
                    }
                    writeln!(f, "")?;
                }
            }
        }
        if self.inner.hint_wasm_backtrace_details_env {
            writeln!(f, "note: run with `WASMTIME_BACKTRACE_DETAILS=1` environment variable to display more information")?;
        }
        Ok(())
    }
}

impl std::error::Error for Trap {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match &self.inner.reason {
            TrapReason::Error(e) => e.source(),
            TrapReason::I32Exit(_) | TrapReason::Message(_) | TrapReason::InstructionTrap(_) => {
                None
            }
        }
    }
}

impl From<anyhow::Error> for Trap {
    fn from(e: anyhow::Error) -> Trap {
        Box::<dyn std::error::Error + Send + Sync>::from(e).into()
    }
}

impl From<Box<dyn std::error::Error + Send + Sync>> for Trap {
    fn from(e: Box<dyn std::error::Error + Send + Sync>) -> Trap {
        // If the top-level error is already a trap, don't be redundant and just return it.
        if let Some(trap) = e.downcast_ref::<Trap>() {
            trap.clone()
        } else {
            let reason = TrapReason::Error(e.into());
            Trap::new_with_trace(None, None, reason, Backtrace::new_unresolved())
        }
    }
}
