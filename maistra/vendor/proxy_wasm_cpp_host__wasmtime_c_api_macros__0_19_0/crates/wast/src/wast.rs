use crate::spectest::link_spectest;
use anyhow::{anyhow, bail, Context as _, Result};
use core::fmt;
use std::str;
use std::{mem::size_of_val, path::Path};
use wasmtime::*;
use wast::Wat;
use wast::{
    parser::{self, ParseBuffer},
    HeapType,
};

/// Translate from a `script::Value` to a `RuntimeValue`.
fn runtime_value(v: &wast::Expression<'_>) -> Result<Val> {
    use wast::Instruction::*;

    if v.instrs.len() != 1 {
        bail!("too many instructions in {:?}", v);
    }
    Ok(match &v.instrs[0] {
        I32Const(x) => Val::I32(*x),
        I64Const(x) => Val::I64(*x),
        F32Const(x) => Val::F32(x.bits),
        F64Const(x) => Val::F64(x.bits),
        V128Const(x) => Val::V128(u128::from_le_bytes(x.to_le_bytes())),
        RefNull(HeapType::Extern) => Val::ExternRef(None),
        RefNull(HeapType::Func) => Val::FuncRef(None),
        RefExtern(x) => Val::ExternRef(Some(ExternRef::new(*x))),
        other => bail!("couldn't convert {:?} to a runtime value", other),
    })
}

/// The wast test script language allows modules to be defined and actions
/// to be performed on them.
pub struct WastContext {
    /// Wast files have a concept of a "current" module, which is the most
    /// recently defined.
    current: Option<Instance>,
    // FIXME(#1479) this is only needed to retain correct trap information after
    // we've dropped previous `Instance` values.
    modules: Vec<Module>,
    linker: Linker,
    store: Store,
}

enum Outcome<T = Vec<Val>> {
    Ok(T),
    Trap(Trap),
}

impl<T> Outcome<T> {
    fn into_result(self) -> Result<T, Trap> {
        match self {
            Outcome::Ok(t) => Ok(t),
            Outcome::Trap(t) => Err(t),
        }
    }
}

impl WastContext {
    /// Construct a new instance of `WastContext`.
    pub fn new(store: Store) -> Self {
        // Spec tests will redefine the same module/name sometimes, so we need
        // to allow shadowing in the linker which picks the most recent
        // definition as what to link when linking.
        let mut linker = Linker::new(&store);
        linker.allow_shadowing(true);
        Self {
            current: None,
            linker,
            store,
            modules: Vec::new(),
        }
    }

    fn get_export(&self, module: Option<&str>, name: &str) -> Result<Extern> {
        match module {
            Some(module) => self.linker.get_one_by_name(module, Some(name)),
            None => self
                .current
                .as_ref()
                .ok_or_else(|| anyhow!("no previous instance found"))?
                .get_export(name)
                .ok_or_else(|| anyhow!("no item named `{}` found", name)),
        }
    }

    fn instantiate(&mut self, module: &[u8]) -> Result<Outcome<Instance>> {
        let module = Module::new(self.store.engine(), module)?;
        self.modules.push(module.clone());
        let instance = match self.linker.instantiate(&module) {
            Ok(i) => i,
            Err(e) => return e.downcast::<Trap>().map(Outcome::Trap),
        };
        Ok(Outcome::Ok(instance))
    }

    /// Register "spectest" which is used by the spec testsuite.
    pub fn register_spectest(&mut self) -> Result<()> {
        link_spectest(&mut self.linker)?;
        Ok(())
    }

    /// Perform the action portion of a command.
    fn perform_execute(&mut self, exec: wast::WastExecute<'_>) -> Result<Outcome> {
        match exec {
            wast::WastExecute::Invoke(invoke) => self.perform_invoke(invoke),
            wast::WastExecute::Module(mut module) => {
                let binary = module.encode()?;
                let result = self.instantiate(&binary)?;
                Ok(match result {
                    Outcome::Ok(_) => Outcome::Ok(Vec::new()),
                    Outcome::Trap(e) => Outcome::Trap(e),
                })
            }
            wast::WastExecute::Get { module, global } => self.get(module.map(|s| s.name()), global),
        }
    }

    fn perform_invoke(&mut self, exec: wast::WastInvoke<'_>) -> Result<Outcome> {
        let values = exec
            .args
            .iter()
            .map(|v| runtime_value(v))
            .collect::<Result<Vec<_>>>()?;
        self.invoke(exec.module.map(|i| i.name()), exec.name, &values)
    }

    /// Define a module and register it.
    fn module(&mut self, instance_name: Option<&str>, module: &[u8]) -> Result<()> {
        let instance = match self.instantiate(module)? {
            Outcome::Ok(i) => i,
            Outcome::Trap(e) => return Err(e).context("instantiation failed"),
        };
        if let Some(name) = instance_name {
            self.linker.instance(name, &instance)?;
        }
        self.current = Some(instance);
        Ok(())
    }

    /// Register an instance to make it available for performing actions.
    fn register(&mut self, name: Option<&str>, as_name: &str) -> Result<()> {
        match name {
            Some(name) => self.linker.alias(name, as_name),
            None => {
                let current = self
                    .current
                    .as_ref()
                    .ok_or(anyhow!("no previous instance"))?;
                self.linker.instance(as_name, current)?;
                Ok(())
            }
        }
    }

    /// Invoke an exported function from an instance.
    fn invoke(
        &mut self,
        instance_name: Option<&str>,
        field: &str,
        args: &[Val],
    ) -> Result<Outcome> {
        let func = self
            .get_export(instance_name, field)?
            .into_func()
            .ok_or_else(|| anyhow!("no function named `{}`", field))?;
        Ok(match func.call(args) {
            Ok(result) => Outcome::Ok(result.into()),
            Err(e) => Outcome::Trap(e.downcast()?),
        })
    }

    /// Get the value of an exported global from an instance.
    fn get(&mut self, instance_name: Option<&str>, field: &str) -> Result<Outcome> {
        let global = self
            .get_export(instance_name, field)?
            .into_global()
            .ok_or_else(|| anyhow!("no global named `{}`", field))?;
        Ok(Outcome::Ok(vec![global.get()]))
    }

    fn assert_return(&self, result: Outcome, results: &[wast::AssertExpression]) -> Result<()> {
        let values = result.into_result()?;
        for (v, e) in values.iter().zip(results) {
            if val_matches(v, e)? {
                continue;
            }
            bail!(
                "expected {:?} ({}), got {:?} ({})",
                e,
                e.as_hex_pattern(),
                v,
                v.as_hex_pattern()
            )
        }
        Ok(())
    }

    fn assert_trap(&self, result: Outcome, expected: &str) -> Result<()> {
        let trap = match result {
            Outcome::Ok(values) => bail!("expected trap, got {:?}", values),
            Outcome::Trap(t) => t,
        };
        let actual = trap.to_string();
        if actual.contains(expected)
            // `bulk-memory-operations/bulk.wast` checks for a message that
            // specifies which element is uninitialized, but our traps don't
            // shepherd that information out.
            || (expected.contains("uninitialized element 2") && actual.contains("uninitialized element"))
        {
            return Ok(());
        }
        if cfg!(feature = "lightbeam") {
            println!("TODO: Check the assert_trap message: {}", expected);
            return Ok(());
        }
        bail!("expected '{}', got '{}'", expected, actual)
    }

    /// Run a wast script from a byte buffer.
    pub fn run_buffer(&mut self, filename: &str, wast: &[u8]) -> Result<()> {
        let wast = str::from_utf8(wast)?;

        let adjust_wast = |mut err: wast::Error| {
            err.set_path(filename.as_ref());
            err.set_text(wast);
            err
        };

        let buf = wast::parser::ParseBuffer::new(wast).map_err(adjust_wast)?;
        let ast = wast::parser::parse::<wast::Wast>(&buf).map_err(adjust_wast)?;

        for directive in ast.directives {
            let sp = directive.span();
            self.run_directive(directive, &adjust_wast)
                .with_context(|| {
                    let (line, col) = sp.linecol_in(wast);
                    format!("failed directive on {}:{}:{}", filename, line + 1, col)
                })?;
        }
        Ok(())
    }

    fn run_directive(
        &mut self,
        directive: wast::WastDirective,
        adjust: impl Fn(wast::Error) -> wast::Error,
    ) -> Result<()> {
        use wast::WastDirective::*;

        match directive {
            Module(mut module) => {
                let binary = module.encode().map_err(adjust)?;
                self.module(module.id.map(|s| s.name()), &binary)?;
            }
            QuoteModule { span: _, source } => {
                let mut module = String::new();
                for src in source {
                    module.push_str(str::from_utf8(src)?);
                    module.push_str(" ");
                }
                let buf = ParseBuffer::new(&module)?;
                let mut wat = parser::parse::<Wat>(&buf).map_err(|mut e| {
                    e.set_text(&module);
                    e
                })?;
                let binary = wat.module.encode()?;
                self.module(wat.module.id.map(|s| s.name()), &binary)?;
            }
            Register {
                span: _,
                name,
                module,
            } => {
                self.register(module.map(|s| s.name()), name)?;
            }
            Invoke(i) => {
                self.perform_invoke(i)?;
            }
            AssertReturn {
                span: _,
                exec,
                results,
            } => {
                let result = self.perform_execute(exec)?;
                self.assert_return(result, &results)?;
            }
            AssertTrap {
                span: _,
                exec,
                message,
            } => {
                let result = self.perform_execute(exec)?;
                self.assert_trap(result, message)?;
            }
            AssertExhaustion {
                span: _,
                call,
                message,
            } => {
                let result = self.perform_invoke(call)?;
                self.assert_trap(result, message)?;
            }
            AssertInvalid {
                span: _,
                mut module,
                message,
            } => {
                let bytes = module.encode()?;
                let err = match self.module(None, &bytes) {
                    Ok(()) => bail!("expected module to fail to build"),
                    Err(e) => e,
                };
                let error_message = format!("{:?}", err);
                if !is_matching_assert_invalid_error_message(&message, &error_message) {
                    bail!(
                        "assert_invalid: expected \"{}\", got \"{}\"",
                        message,
                        error_message
                    )
                }
            }
            AssertMalformed {
                module,
                span: _,
                message: _,
            } => {
                let mut module = match module {
                    wast::QuoteModule::Module(m) => m,
                    // This is a `*.wat` parser test which we're not
                    // interested in.
                    wast::QuoteModule::Quote(_) => return Ok(()),
                };
                let bytes = module.encode().map_err(adjust)?;
                if let Ok(_) = self.module(None, &bytes) {
                    bail!("expected malformed module to fail to instantiate");
                }
            }
            AssertUnlinkable {
                span: _,
                mut module,
                message,
            } => {
                let bytes = module.encode().map_err(adjust)?;
                let err = match self.module(None, &bytes) {
                    Ok(()) => bail!("expected module to fail to link"),
                    Err(e) => e,
                };
                let error_message = format!("{:?}", err);
                if !error_message.contains(&message) {
                    bail!(
                        "assert_unlinkable: expected {}, got {}",
                        message,
                        error_message
                    )
                }
            }
        }

        Ok(())
    }

    /// Run a wast script from a file.
    pub fn run_file(&mut self, path: &Path) -> Result<()> {
        let bytes =
            std::fs::read(path).with_context(|| format!("failed to read `{}`", path.display()))?;
        self.run_buffer(path.to_str().unwrap(), &bytes)
    }
}

fn is_matching_assert_invalid_error_message(expected: &str, actual: &str) -> bool {
    actual.contains(expected)
        // `elem.wast` and `proposals/bulk-memory-operations/elem.wast` disagree
        // on the expected error message for the same error.
        || (expected.contains("out of bounds") && actual.contains("does not fit"))
        // slight difference in error messages
        || (expected.contains("unknown elem segment") && actual.contains("unknown element segment"))
}

fn extract_lane_as_i8(bytes: u128, lane: usize) -> i8 {
    (bytes >> (lane * 8)) as i8
}

fn extract_lane_as_i16(bytes: u128, lane: usize) -> i16 {
    (bytes >> (lane * 16)) as i16
}

fn extract_lane_as_i32(bytes: u128, lane: usize) -> i32 {
    (bytes >> (lane * 32)) as i32
}

fn extract_lane_as_i64(bytes: u128, lane: usize) -> i64 {
    (bytes >> (lane * 64)) as i64
}

/// Check if an f32 (as u32 bits to avoid possible quieting when moving values in registers, e.g.
/// https://developer.arm.com/documentation/ddi0344/i/neon-and-vfp-programmers-model/modes-of-operation/default-nan-mode?lang=en)
/// is a canonical NaN:
///  - the sign bit is unspecified,
///  - the 8-bit exponent is set to all 1s
///  - the MSB of the payload is set to 1 (a quieted NaN) and all others to 0.
/// See https://webassembly.github.io/spec/core/syntax/values.html#floating-point.
fn is_canonical_f32_nan(bits: u32) -> bool {
    (bits & 0x7fff_ffff) == 0x7fc0_0000
}

/// Check if an f64 (as u64 bits to avoid possible quieting when moving values in registers, e.g.
/// https://developer.arm.com/documentation/ddi0344/i/neon-and-vfp-programmers-model/modes-of-operation/default-nan-mode?lang=en)
/// is a canonical NaN:
///  - the sign bit is unspecified,
///  - the 11-bit exponent is set to all 1s
///  - the MSB of the payload is set to 1 (a quieted NaN) and all others to 0.
/// See https://webassembly.github.io/spec/core/syntax/values.html#floating-point.
fn is_canonical_f64_nan(bits: u64) -> bool {
    (bits & 0x7fff_ffff_ffff_ffff) == 0x7ff8_0000_0000_0000
}

/// Check if an f32 (as u32, see comments above) is an arithmetic NaN. This is the same as a
/// canonical NaN including that the payload MSB is set to 1, but one or more of the remaining
/// payload bits MAY BE set to 1 (a canonical NaN specifies all 0s). See
/// https://webassembly.github.io/spec/core/syntax/values.html#floating-point.
fn is_arithmetic_f32_nan(bits: u32) -> bool {
    const AF32_NAN: u32 = 0x7f80_0000;
    let is_nan = bits & AF32_NAN == AF32_NAN;
    const AF32_PAYLOAD_MSB: u32 = 0x0040_0000;
    let is_msb_set = bits & AF32_PAYLOAD_MSB == AF32_PAYLOAD_MSB;
    is_nan && is_msb_set
}

/// Check if an f64 (as u64, see comments above) is an arithmetic NaN. This is the same as a
/// canonical NaN including that the payload MSB is set to 1, but one or more of the remaining
/// payload bits MAY BE set to 1 (a canonical NaN specifies all 0s). See
/// https://webassembly.github.io/spec/core/syntax/values.html#floating-point.
fn is_arithmetic_f64_nan(bits: u64) -> bool {
    const AF64_NAN: u64 = 0x7ff0_0000_0000_0000;
    let is_nan = bits & AF64_NAN == AF64_NAN;
    const AF64_PAYLOAD_MSB: u64 = 0x0008_0000_0000_0000;
    let is_msb_set = bits & AF64_PAYLOAD_MSB == AF64_PAYLOAD_MSB;
    is_nan && is_msb_set
}

fn val_matches(actual: &Val, expected: &wast::AssertExpression) -> Result<bool> {
    Ok(match (actual, expected) {
        (Val::I32(a), wast::AssertExpression::I32(b)) => a == b,
        (Val::I64(a), wast::AssertExpression::I64(b)) => a == b,
        // Note that these float comparisons are comparing bits, not float
        // values, so we're testing for bit-for-bit equivalence
        (Val::F32(a), wast::AssertExpression::F32(b)) => f32_matches(*a, b),
        (Val::F64(a), wast::AssertExpression::F64(b)) => f64_matches(*a, b),
        (Val::V128(a), wast::AssertExpression::V128(b)) => v128_matches(*a, b),
        (Val::ExternRef(x), wast::AssertExpression::RefNull(Some(HeapType::Extern))) => x.is_none(),
        (Val::ExternRef(x), wast::AssertExpression::RefExtern(y)) => {
            if let Some(x) = x {
                let x = x
                    .data()
                    .downcast_ref::<u32>()
                    .expect("only u32 externrefs created in wast test suites");
                x == y
            } else {
                false
            }
        }
        (Val::FuncRef(x), wast::AssertExpression::RefNull(_)) => x.is_none(),
        _ => bail!(
            "don't know how to compare {:?} and {:?} yet",
            actual,
            expected
        ),
    })
}

fn f32_matches(actual: u32, expected: &wast::NanPattern<wast::Float32>) -> bool {
    match expected {
        wast::NanPattern::CanonicalNan => is_canonical_f32_nan(actual),
        wast::NanPattern::ArithmeticNan => is_arithmetic_f32_nan(actual),
        wast::NanPattern::Value(expected_value) => actual == expected_value.bits,
    }
}

fn f64_matches(actual: u64, expected: &wast::NanPattern<wast::Float64>) -> bool {
    match expected {
        wast::NanPattern::CanonicalNan => is_canonical_f64_nan(actual),
        wast::NanPattern::ArithmeticNan => is_arithmetic_f64_nan(actual),
        wast::NanPattern::Value(expected_value) => actual == expected_value.bits,
    }
}

fn v128_matches(actual: u128, expected: &wast::V128Pattern) -> bool {
    match expected {
        wast::V128Pattern::I8x16(b) => b
            .iter()
            .enumerate()
            .all(|(i, b)| *b == extract_lane_as_i8(actual, i)),
        wast::V128Pattern::I16x8(b) => b
            .iter()
            .enumerate()
            .all(|(i, b)| *b == extract_lane_as_i16(actual, i)),
        wast::V128Pattern::I32x4(b) => b
            .iter()
            .enumerate()
            .all(|(i, b)| *b == extract_lane_as_i32(actual, i)),
        wast::V128Pattern::I64x2(b) => b
            .iter()
            .enumerate()
            .all(|(i, b)| *b == extract_lane_as_i64(actual, i)),
        wast::V128Pattern::F32x4(b) => b.iter().enumerate().all(|(i, b)| {
            let a = extract_lane_as_i32(actual, i) as u32;
            f32_matches(a, b)
        }),
        wast::V128Pattern::F64x2(b) => b.iter().enumerate().all(|(i, b)| {
            let a = extract_lane_as_i64(actual, i) as u64;
            f64_matches(a, b)
        }),
    }
}

/// When troubleshooting a failure in a spec test, it is valuable to understand the bit-by-bit
/// difference. To do this, we print a hex-encoded version of Wasm values and assertion expressions
/// using this helper.
fn as_hex_pattern<T>(bits: T) -> String
where
    T: fmt::LowerHex,
{
    format!("{1:#00$x}", size_of_val(&bits) * 2 + 2, bits)
}

/// The [AsHexPattern] allows us to extend `as_hex_pattern` to various structures.
trait AsHexPattern {
    fn as_hex_pattern(&self) -> String;
}

impl AsHexPattern for wast::AssertExpression<'_> {
    fn as_hex_pattern(&self) -> String {
        match self {
            wast::AssertExpression::I32(i) => as_hex_pattern(*i),
            wast::AssertExpression::I64(i) => as_hex_pattern(*i),
            wast::AssertExpression::F32(f) => f.as_hex_pattern(),
            wast::AssertExpression::F64(f) => f.as_hex_pattern(),
            wast::AssertExpression::V128(v) => v.as_hex_pattern(),
            wast::AssertExpression::RefNull(_)
            | wast::AssertExpression::RefExtern(_)
            | wast::AssertExpression::RefFunc(_)
            | wast::AssertExpression::LegacyArithmeticNaN
            | wast::AssertExpression::LegacyCanonicalNaN => "no hex representation".to_string(),
        }
    }
}

impl AsHexPattern for wast::NanPattern<wast::Float32> {
    fn as_hex_pattern(&self) -> String {
        match self {
            wast::NanPattern::CanonicalNan => "0x7fc00000".to_string(),
            // Note that NaN patterns can have varying sign bits and payloads. Technically the first
            // bit should be a `*` but it is impossible to show that in hex.
            wast::NanPattern::ArithmeticNan => "0x7fc*****".to_string(),
            wast::NanPattern::Value(wast::Float32 { bits }) => as_hex_pattern(*bits),
        }
    }
}

impl AsHexPattern for wast::NanPattern<wast::Float64> {
    fn as_hex_pattern(&self) -> String {
        match self {
            wast::NanPattern::CanonicalNan => "0x7ff8000000000000".to_string(),
            // Note that NaN patterns can have varying sign bits and payloads. Technically the first
            // bit should be a `*` but it is impossible to show that in hex.
            wast::NanPattern::ArithmeticNan => "0x7ff8************".to_string(),
            wast::NanPattern::Value(wast::Float64 { bits }) => as_hex_pattern(*bits),
        }
    }
}

// This implementation reverses both the lanes and the lane bytes in order to match the Wasm SIMD
// little-endian order. This implementation must include special behavior for this reversal; other
// implementations do not because they deal with raw values (`u128`) or use big-endian order for
// display (scalars).
impl AsHexPattern for wast::V128Pattern {
    fn as_hex_pattern(&self) -> String {
        fn reverse_pattern(pattern: String) -> String {
            let chars: Vec<char> = pattern[2..].chars().collect();
            let reversed: Vec<&[char]> = chars.chunks(2).rev().collect();
            reversed.concat().iter().collect()
        }

        fn as_hex_pattern(bits: &[u8]) -> String {
            bits.iter()
                .map(|b| format!("{:02x}", b))
                .collect::<Vec<_>>()
                .concat()
        }

        fn reverse_lanes<T, F>(
            lanes: impl DoubleEndedIterator<Item = T>,
            as_hex_pattern: F,
        ) -> String
        where
            F: Fn(T) -> String,
        {
            lanes
                .rev()
                .map(|f| as_hex_pattern(f))
                .collect::<Vec<_>>()
                .concat()
        }

        let lanes_as_hex = match self {
            wast::V128Pattern::I8x16(v) => {
                reverse_lanes(v.iter(), |b| as_hex_pattern(&b.to_le_bytes()))
            }
            wast::V128Pattern::I16x8(v) => {
                reverse_lanes(v.iter(), |b| as_hex_pattern(&b.to_le_bytes()))
            }
            wast::V128Pattern::I32x4(v) => {
                reverse_lanes(v.iter(), |b| as_hex_pattern(&b.to_le_bytes()))
            }
            wast::V128Pattern::I64x2(v) => {
                reverse_lanes(v.iter(), |b| as_hex_pattern(&b.to_le_bytes()))
            }
            wast::V128Pattern::F32x4(v) => {
                reverse_lanes(v.iter(), |b| reverse_pattern(b.as_hex_pattern()))
            }
            wast::V128Pattern::F64x2(v) => {
                reverse_lanes(v.iter(), |b| reverse_pattern(b.as_hex_pattern()))
            }
        };

        String::from("0x") + &lanes_as_hex
    }
}

impl AsHexPattern for Val {
    fn as_hex_pattern(&self) -> String {
        match self {
            Val::I32(i) => as_hex_pattern(*i),
            Val::I64(i) => as_hex_pattern(*i),
            Val::F32(f) => as_hex_pattern(*f),
            Val::F64(f) => as_hex_pattern(*f),
            Val::V128(v) => as_hex_pattern(*v),
            Val::ExternRef(_) | Val::FuncRef(_) => "no hex representation".to_string(),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    #[test]
    fn val_to_hex() {
        assert_eq!(Val::I32(0x42).as_hex_pattern(), "0x00000042");
        assert_eq!(Val::F64(0x0).as_hex_pattern(), "0x0000000000000000");
        assert_eq!(
            Val::V128(u128::from_le_bytes([
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf
            ]))
            .as_hex_pattern(),
            "0x0f0e0d0c0b0a09080706050403020100"
        );
    }

    #[test]
    fn assert_expression_to_hex() {
        assert_eq!(
            wast::AssertExpression::F32(wast::NanPattern::ArithmeticNan).as_hex_pattern(),
            "0x7fc*****"
        );
        assert_eq!(
            wast::AssertExpression::F64(wast::NanPattern::Value(wast::Float64 { bits: 0x42 }))
                .as_hex_pattern(),
            "0x0000000000000042"
        );
        assert_eq!(
            wast::AssertExpression::V128(wast::V128Pattern::I32x4([0, 1, 2, 3])).as_hex_pattern(),
            "0x03000000020000000100000000000000"
        );
        assert_eq!(
            wast::AssertExpression::V128(wast::V128Pattern::F64x2([
                wast::NanPattern::CanonicalNan,
                wast::NanPattern::ArithmeticNan
            ]))
            .as_hex_pattern(),
            "0x************f87f000000000000f87f"
        );
    }
}
