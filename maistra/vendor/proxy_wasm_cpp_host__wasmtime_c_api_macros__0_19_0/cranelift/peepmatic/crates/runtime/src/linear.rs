//! A linear IR for optimizations.
//!
//! This IR is designed such that it should be easy to combine multiple linear
//! optimizations into a single automata.
//!
//! See also `src/linearize.rs` for the AST to linear IR translation pass.

use crate::cc::ConditionCode;
use crate::integer_interner::{IntegerId, IntegerInterner};
use crate::r#type::{BitWidth, Type};
use crate::unquote::UnquoteOperator;
use serde::{Deserialize, Serialize};
use std::fmt::Debug;
use std::hash::Hash;
use std::num::NonZeroU32;

/// A set of linear optimizations.
#[derive(Debug)]
pub struct Optimizations<TOperator>
where
    TOperator: 'static + Copy + Debug + Eq + Hash,
{
    /// The linear optimizations.
    pub optimizations: Vec<Optimization<TOperator>>,

    /// The integer literals referenced by these optimizations.
    pub integers: IntegerInterner,
}

/// A linearized optimization.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Optimization<TOperator>
where
    TOperator: 'static + Copy + Debug + Eq + Hash,
{
    /// The chain of match operations and expected results for this
    /// optimization.
    pub matches: Vec<Match>,

    /// Actions to perform, given that the operation resulted in the expected
    /// value.
    pub actions: Vec<Action<TOperator>>,
}

/// Match any value.
///
/// This can be used to create fallback, wildcard-style transitions between
/// states.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub struct Else;

/// The result of evaluating a `MatchOp`.
///
/// This is either a specific non-zero `u32`, or a fallback that matches
/// everything.
pub type MatchResult = Result<NonZeroU32, Else>;

/// Convert a boolean to a `MatchResult`.
#[inline]
pub fn bool_to_match_result(b: bool) -> MatchResult {
    let b = b as u32;
    unsafe { Ok(NonZeroU32::new_unchecked(b + 1)) }
}

/// A partial match of an optimization's LHS.
///
/// An match is composed of a matching operation and the expected result of that
/// operation. Each match will basically become a state and a transition edge
/// out of that state in the final automata.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Match {
    /// The matching operation to perform.
    pub operation: MatchOp,

    /// The expected result of our matching operation, that enables us to
    /// continue to the next match, or `Else` for "don't care" wildcard-style
    /// matching.
    pub expected: MatchResult,
}

/// A matching operation to be performed on some Cranelift instruction as part
/// of determining whether an optimization is applicable.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Deserialize, Serialize)]
pub enum MatchOp {
    /// Switch on the opcode of an instruction.
    ///
    /// Upon successfully matching an instruction's opcode, bind each of its
    /// operands to a LHS temporary.
    Opcode(LhsId),

    /// Does an instruction have a constant value?
    IsConst(LhsId),

    /// Is the constant value a power of two?
    IsPowerOfTwo(LhsId),

    /// Switch on the bit width of a value.
    BitWidth(LhsId),

    /// Does the value fit in our target architecture's native word size?
    FitsInNativeWord(LhsId),

    /// Are the instructions (or immediates) the same?
    Eq(LhsId, LhsId),

    /// Switch on the constant integer value of an instruction.
    IntegerValue(LhsId),

    /// Switch on the constant boolean value of an instruction.
    BooleanValue(LhsId),

    /// Switch on a condition code.
    ConditionCode(LhsId),

    /// No operation. Always evaluates to `Else`.
    ///
    /// Never appears in real optimizations; nonetheless required to support
    /// corner cases of the DSL, such as a LHS pattern that is nothing but a
    /// variable.
    Nop,
}

/// A canonicalized identifier for a left-hand side value that was bound in a
/// pattern.
///
/// These are defined in a pre-order traversal of the LHS pattern by successful
/// `MatchOp::Opcode` matches.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord, Serialize, Deserialize)]
pub struct LhsId(pub u16);

/// A canonicalized identifier for a right-hand side value.
///
/// These are defined by RHS actions.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct RhsId(pub u16);

/// An action to perform when transitioning between states in the automata.
///
/// When evaluating actions, the `i^th` action implicitly defines the
/// `RhsId(i)`.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum Action<TOperator> {
    /// Reuse something from the left-hand side.
    GetLhs {
        /// The left-hand side instruction or value.
        lhs: LhsId,
    },

    /// Perform compile-time evaluation.
    UnaryUnquote {
        /// The unquote operator.
        operator: UnquoteOperator,
        /// The constant operand to this unquote.
        operand: RhsId,
    },

    /// Perform compile-time evaluation.
    BinaryUnquote {
        /// The unquote operator.
        operator: UnquoteOperator,
        /// The constant operands to this unquote.
        operands: [RhsId; 2],
    },

    /// Create an integer constant.
    MakeIntegerConst {
        /// The constant integer value.
        value: IntegerId,
        /// The bit width of this constant.
        bit_width: BitWidth,
    },

    /// Create a boolean constant.
    MakeBooleanConst {
        /// The constant boolean value.
        value: bool,
        /// The bit width of this constant.
        bit_width: BitWidth,
    },

    /// Create a condition code.
    MakeConditionCode {
        /// The condition code.
        cc: ConditionCode,
    },

    /// Make a unary instruction.
    MakeUnaryInst {
        /// The operand for this instruction.
        operand: RhsId,
        /// The type of this instruction's result.
        r#type: Type,
        /// The operator for this instruction.
        operator: TOperator,
    },

    /// Make a binary instruction.
    MakeBinaryInst {
        /// The opcode for this instruction.
        operator: TOperator,
        /// The type of this instruction's result.
        r#type: Type,
        /// The operands for this instruction.
        operands: [RhsId; 2],
    },

    /// Make a ternary instruction.
    MakeTernaryInst {
        /// The opcode for this instruction.
        operator: TOperator,
        /// The type of this instruction's result.
        r#type: Type,
        /// The operands for this instruction.
        operands: [RhsId; 3],
    },
}

#[cfg(test)]
mod tests {
    use super::*;
    use peepmatic_test_operator::TestOperator;

    // These types all end up in the automaton, so we should take care that they
    // are small and don't fill up the data cache (or take up too much
    // serialized size).

    #[test]
    fn match_result_size() {
        assert_eq!(std::mem::size_of::<MatchResult>(), 4);
    }

    #[test]
    fn match_op_size() {
        assert_eq!(std::mem::size_of::<MatchOp>(), 6);
    }

    #[test]
    fn action_size() {
        assert_eq!(std::mem::size_of::<Action<TestOperator>>(), 16);
    }
}
