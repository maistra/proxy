//! Interfacing with actual instructions.

use crate::part::{Constant, Part};
use crate::r#type::Type;
use std::fmt::Debug;
use std::hash::Hash;
use std::num::NonZeroU32;

/// A trait for interfacing with actual instruction sequences.
///
/// This trait enables both:
///
/// * `peepmatic-runtime` to be used by `cranelift-codegen` without a circular
///   dependency from `peepmatic-runtime` to `cranelift-codegen` to get access
///   to Cranelift's IR types, and
///
/// * enables us to write local tests that exercise peephole optimizers on a
///  simple, testing-only instruction set without pulling in all of Cranelift.
///
/// Finally, this should also make the task of adding support for Cranelift's
/// new `MachInst` and vcode backend easier, since all that needs to be done is
/// "just" implementing this trait. (And probably add/modify some
/// `peepmatic_runtime::operation::Operation`s as well).
///
/// ## Safety
///
/// See doc comment for `instruction_result_bit_width`.
pub unsafe trait InstructionSet<'a> {
    /// Mutable context passed into all trait methods. Can be whatever you want!
    ///
    /// In practice, this is a `FuncCursor` for `cranelift-codegen`'s trait
    /// implementation.
    type Context;

    /// An operator.
    type Operator: 'static + Copy + Debug + Eq + Hash + Into<NonZeroU32>;

    /// An instruction (or identifier for an instruction).
    type Instruction: Copy + Debug + Eq;

    /// Replace the `old` instruction with `new`.
    ///
    /// `new` is either a `Part::Instruction` or a constant `Part::Boolean` or
    /// `Part::Integer`. In the former case, it can directly replace `old`. In
    /// the latter case, implementations of this trait should transparently
    /// create an `iconst` or `bconst` instruction to wrap the given constant.
    ///
    /// `new` will never be `Part::ConditionCode`.
    fn replace_instruction(
        &self,
        context: &mut Self::Context,
        old: Self::Instruction,
        new: Part<Self::Instruction>,
    ) -> Self::Instruction;

    /// Get the given instruction's operator.
    ///
    /// If the instruction isn't supported, then `None` should be returned.
    ///
    /// Additionally, if `Some` is returned, then the instruction's operands
    /// must be pushed in order into `operands`. E.g. calling this method on
    /// `(iadd $x $y)` would return `Some(iadd)` and extend `operands` with
    /// `[$x, $y]`.
    fn operator<E>(
        &self,
        context: &mut Self::Context,
        instr: Self::Instruction,
        operands: &mut E,
    ) -> Option<Self::Operator>
    where
        E: Extend<Part<Self::Instruction>>;

    /// Make a unary instruction.
    ///
    /// If the type is not given, then it should be inferred.
    fn make_inst_1(
        &self,
        context: &mut Self::Context,
        root: Self::Instruction,
        operator: Self::Operator,
        r#type: Type,
        a: Part<Self::Instruction>,
    ) -> Self::Instruction;

    /// Make a binary instruction.
    ///
    /// Operands are given as immediates first and arguments following
    /// them. Condition codes are treated as immediates. So if we are creating
    /// an `iadd_imm` instruction, then `a` will be the constant integer
    /// immediate and `b` will be the instruction whose result is the dynamic
    /// argument.
    fn make_inst_2(
        &self,
        context: &mut Self::Context,
        root: Self::Instruction,
        operator: Self::Operator,
        r#type: Type,
        a: Part<Self::Instruction>,
        b: Part<Self::Instruction>,
    ) -> Self::Instruction;

    /// Make a ternary instruction.
    ///
    /// Operands are given as immediates first and arguments following
    /// them. Condition codes are treated as immediates. So if we are creating
    /// an `icmp` instruction, then `a` will be the condition code, and `b` and
    /// `c` will be instructions whose results are the dynamic arguments.
    fn make_inst_3(
        &self,
        context: &mut Self::Context,
        root: Self::Instruction,
        operator: Self::Operator,
        r#type: Type,
        a: Part<Self::Instruction>,
        b: Part<Self::Instruction>,
        c: Part<Self::Instruction>,
    ) -> Self::Instruction;

    /// Try to resolve the given instruction into a constant value.
    ///
    /// If we can tell that the instruction returns a constant value, then
    /// return that constant value as either a `Part::Boolean` or
    /// `Part::Integer`. Otherwise, return `None`.
    fn instruction_to_constant(
        &self,
        context: &mut Self::Context,
        inst: Self::Instruction,
    ) -> Option<Constant>;

    /// Get the bit width of the given instruction's result.
    ///
    /// ## Safety
    ///
    /// There is code that makes memory-safety assumptions that the result is
    /// always one of 1, 8, 16, 32, 64, or 128. Implementors must uphold this.
    fn instruction_result_bit_width(
        &self,
        context: &mut Self::Context,
        inst: Self::Instruction,
    ) -> u8;

    /// Get the size of a native word in bits.
    fn native_word_size_in_bits(&self, context: &mut Self::Context) -> u8;
}
