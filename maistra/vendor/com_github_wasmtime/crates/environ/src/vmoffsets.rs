//! Offsets and sizes of various structs in wasmtime-runtime's vmcontext
//! module.

// Currently the `VMContext` allocation by field looks like this:
//
// struct VMContext {
//      interrupts: *const VMInterrupts,
//      externref_activations_table: *mut VMExternRefActivationsTable,
//      stack_map_registry: *mut StackMapRegistry,
//      signature_ids: [VMSharedSignatureIndex; module.num_signature_ids],
//      imported_functions: [VMFunctionImport; module.num_imported_functions],
//      imported_tables: [VMTableImport; module.num_imported_tables],
//      imported_memories: [VMMemoryImport; module.num_imported_memories],
//      imported_globals: [VMGlobalImport; module.num_imported_globals],
//      tables: [VMTableDefinition; module.num_defined_tables],
//      memories: [VMMemoryDefinition; module.num_defined_memories],
//      globals: [VMGlobalDefinition; module.num_defined_globals],
//      anyfuncs: [VMCallerCheckedAnyfunc; module.num_imported_functions + module.num_defined_functions],
//      builtins: VMBuiltinFunctionsArray,
// }

use crate::module::Module;
use crate::BuiltinFunctionIndex;
use cranelift_codegen::ir;
use cranelift_wasm::{
    DefinedGlobalIndex, DefinedMemoryIndex, DefinedTableIndex, FuncIndex, GlobalIndex, MemoryIndex,
    TableIndex, TypeIndex,
};
use more_asserts::assert_lt;
use std::convert::TryFrom;

/// Sentinel value indicating that wasm has been interrupted.
// Note that this has a bit of an odd definition. See the `insert_stack_check`
// function in `cranelift/codegen/src/isa/x86/abi.rs` for more information
pub const INTERRUPTED: usize = usize::max_value() - 32 * 1024;

#[cfg(target_pointer_width = "32")]
fn cast_to_u32(sz: usize) -> u32 {
    u32::try_from(sz).unwrap()
}
#[cfg(target_pointer_width = "64")]
fn cast_to_u32(sz: usize) -> u32 {
    u32::try_from(sz).expect("overflow in cast from usize to u32")
}

/// Align an offset used in this module to a specific byte-width by rounding up
fn align(offset: u32, width: u32) -> u32 {
    (offset + (width - 1)) / width * width
}

/// This class computes offsets to fields within `VMContext` and other
/// related structs that JIT code accesses directly.
#[derive(Debug, Clone, Copy)]
pub struct VMOffsets {
    /// The size in bytes of a pointer on the target.
    pub pointer_size: u8,
    /// The number of signature declarations in the module.
    pub num_signature_ids: u32,
    /// The number of imported functions in the module.
    pub num_imported_functions: u32,
    /// The number of imported tables in the module.
    pub num_imported_tables: u32,
    /// The number of imported memories in the module.
    pub num_imported_memories: u32,
    /// The number of imported globals in the module.
    pub num_imported_globals: u32,
    /// The number of defined functions in the module.
    pub num_defined_functions: u32,
    /// The number of defined tables in the module.
    pub num_defined_tables: u32,
    /// The number of defined memories in the module.
    pub num_defined_memories: u32,
    /// The number of defined globals in the module.
    pub num_defined_globals: u32,
}

impl VMOffsets {
    /// Return a new `VMOffsets` instance, for a given pointer size.
    pub fn new(pointer_size: u8, module: &Module) -> Self {
        Self {
            pointer_size,
            num_signature_ids: cast_to_u32(module.types.len()),
            num_imported_functions: cast_to_u32(module.num_imported_funcs),
            num_imported_tables: cast_to_u32(module.num_imported_tables),
            num_imported_memories: cast_to_u32(module.num_imported_memories),
            num_imported_globals: cast_to_u32(module.num_imported_globals),
            num_defined_functions: cast_to_u32(module.functions.len()),
            num_defined_tables: cast_to_u32(module.table_plans.len()),
            num_defined_memories: cast_to_u32(module.memory_plans.len()),
            num_defined_globals: cast_to_u32(module.globals.len()),
        }
    }
}

/// Offsets for `VMFunctionImport`.
impl VMOffsets {
    /// The offset of the `body` field.
    #[allow(clippy::erasing_op)]
    pub fn vmfunction_import_body(&self) -> u8 {
        0 * self.pointer_size
    }

    /// The offset of the `vmctx` field.
    #[allow(clippy::identity_op)]
    pub fn vmfunction_import_vmctx(&self) -> u8 {
        1 * self.pointer_size
    }

    /// Return the size of `VMFunctionImport`.
    pub fn size_of_vmfunction_import(&self) -> u8 {
        2 * self.pointer_size
    }
}

/// Offsets for `*const VMFunctionBody`.
impl VMOffsets {
    /// The size of the `current_elements` field.
    #[allow(clippy::identity_op)]
    pub fn size_of_vmfunction_body_ptr(&self) -> u8 {
        1 * self.pointer_size
    }
}

/// Offsets for `VMTableImport`.
impl VMOffsets {
    /// The offset of the `from` field.
    #[allow(clippy::erasing_op)]
    pub fn vmtable_import_from(&self) -> u8 {
        0 * self.pointer_size
    }

    /// The offset of the `vmctx` field.
    #[allow(clippy::identity_op)]
    pub fn vmtable_import_vmctx(&self) -> u8 {
        1 * self.pointer_size
    }

    /// Return the size of `VMTableImport`.
    pub fn size_of_vmtable_import(&self) -> u8 {
        2 * self.pointer_size
    }
}

/// Offsets for `VMTableDefinition`.
impl VMOffsets {
    /// The offset of the `base` field.
    #[allow(clippy::erasing_op)]
    pub fn vmtable_definition_base(&self) -> u8 {
        0 * self.pointer_size
    }

    /// The offset of the `current_elements` field.
    #[allow(clippy::identity_op)]
    pub fn vmtable_definition_current_elements(&self) -> u8 {
        1 * self.pointer_size
    }

    /// The size of the `current_elements` field.
    pub fn size_of_vmtable_definition_current_elements(&self) -> u8 {
        4
    }

    /// Return the size of `VMTableDefinition`.
    pub fn size_of_vmtable_definition(&self) -> u8 {
        2 * self.pointer_size
    }

    /// The type of the `current_elements` field.
    pub fn type_of_vmtable_definition_current_elements(&self) -> ir::Type {
        ir::Type::int(u16::from(self.size_of_vmtable_definition_current_elements()) * 8).unwrap()
    }
}

/// Offsets for `VMMemoryImport`.
impl VMOffsets {
    /// The offset of the `from` field.
    #[allow(clippy::erasing_op)]
    pub fn vmmemory_import_from(&self) -> u8 {
        0 * self.pointer_size
    }

    /// The offset of the `vmctx` field.
    #[allow(clippy::identity_op)]
    pub fn vmmemory_import_vmctx(&self) -> u8 {
        1 * self.pointer_size
    }

    /// Return the size of `VMMemoryImport`.
    pub fn size_of_vmmemory_import(&self) -> u8 {
        2 * self.pointer_size
    }
}

/// Offsets for `VMMemoryDefinition`.
impl VMOffsets {
    /// The offset of the `base` field.
    #[allow(clippy::erasing_op)]
    pub fn vmmemory_definition_base(&self) -> u8 {
        0 * self.pointer_size
    }

    /// The offset of the `current_length` field.
    #[allow(clippy::identity_op)]
    pub fn vmmemory_definition_current_length(&self) -> u8 {
        1 * self.pointer_size
    }

    /// The size of the `current_length` field.
    pub fn size_of_vmmemory_definition_current_length(&self) -> u8 {
        4
    }

    /// Return the size of `VMMemoryDefinition`.
    pub fn size_of_vmmemory_definition(&self) -> u8 {
        2 * self.pointer_size
    }

    /// The type of the `current_length` field.
    pub fn type_of_vmmemory_definition_current_length(&self) -> ir::Type {
        ir::Type::int(u16::from(self.size_of_vmmemory_definition_current_length()) * 8).unwrap()
    }
}

/// Offsets for `VMGlobalImport`.
impl VMOffsets {
    /// The offset of the `from` field.
    #[allow(clippy::erasing_op)]
    pub fn vmglobal_import_from(&self) -> u8 {
        0 * self.pointer_size
    }

    /// Return the size of `VMGlobalImport`.
    #[allow(clippy::identity_op)]
    pub fn size_of_vmglobal_import(&self) -> u8 {
        1 * self.pointer_size
    }
}

/// Offsets for `VMGlobalDefinition`.
impl VMOffsets {
    /// Return the size of `VMGlobalDefinition`; this is the size of the largest value type (i.e. a
    /// V128).
    pub fn size_of_vmglobal_definition(&self) -> u8 {
        16
    }
}

/// Offsets for `VMSharedSignatureIndex`.
impl VMOffsets {
    /// Return the size of `VMSharedSignatureIndex`.
    pub fn size_of_vmshared_signature_index(&self) -> u8 {
        4
    }
}

/// Offsets for `VMInterrupts`.
impl VMOffsets {
    /// Return the offset of the `stack_limit` field of `VMInterrupts`
    pub fn vminterrupts_stack_limit(&self) -> u8 {
        0
    }

    /// Return the offset of the `fuel_consumed` field of `VMInterrupts`
    pub fn vminterrupts_fuel_consumed(&self) -> u8 {
        self.pointer_size
    }
}

/// Offsets for `VMCallerCheckedAnyfunc`.
impl VMOffsets {
    /// The offset of the `func_ptr` field.
    #[allow(clippy::erasing_op)]
    pub fn vmcaller_checked_anyfunc_func_ptr(&self) -> u8 {
        0 * self.pointer_size
    }

    /// The offset of the `type_index` field.
    #[allow(clippy::identity_op)]
    pub fn vmcaller_checked_anyfunc_type_index(&self) -> u8 {
        1 * self.pointer_size
    }

    /// The offset of the `vmctx` field.
    pub fn vmcaller_checked_anyfunc_vmctx(&self) -> u8 {
        2 * self.pointer_size
    }

    /// Return the size of `VMCallerCheckedAnyfunc`.
    pub fn size_of_vmcaller_checked_anyfunc(&self) -> u8 {
        3 * self.pointer_size
    }
}

/// Offsets for `VMContext`.
impl VMOffsets {
    /// Return the offset to the `VMInterrupts` structure
    pub fn vmctx_interrupts(&self) -> u32 {
        0
    }

    /// The offset of the `VMExternRefActivationsTable` member.
    pub fn vmctx_externref_activations_table(&self) -> u32 {
        self.vmctx_interrupts()
            .checked_add(u32::from(self.pointer_size))
            .unwrap()
    }

    /// The offset of the `*mut StackMapRegistry` member.
    pub fn vmctx_stack_map_registry(&self) -> u32 {
        self.vmctx_externref_activations_table()
            .checked_add(u32::from(self.pointer_size))
            .unwrap()
    }

    /// The offset of the `signature_ids` array.
    pub fn vmctx_signature_ids_begin(&self) -> u32 {
        self.vmctx_stack_map_registry()
            .checked_add(u32::from(self.pointer_size))
            .unwrap()
    }

    /// The offset of the `tables` array.
    #[allow(clippy::erasing_op)]
    pub fn vmctx_imported_functions_begin(&self) -> u32 {
        self.vmctx_signature_ids_begin()
            .checked_add(
                self.num_signature_ids
                    .checked_mul(u32::from(self.size_of_vmshared_signature_index()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// The offset of the `tables` array.
    #[allow(clippy::identity_op)]
    pub fn vmctx_imported_tables_begin(&self) -> u32 {
        self.vmctx_imported_functions_begin()
            .checked_add(
                self.num_imported_functions
                    .checked_mul(u32::from(self.size_of_vmfunction_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// The offset of the `memories` array.
    pub fn vmctx_imported_memories_begin(&self) -> u32 {
        self.vmctx_imported_tables_begin()
            .checked_add(
                self.num_imported_tables
                    .checked_mul(u32::from(self.size_of_vmtable_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// The offset of the `globals` array.
    pub fn vmctx_imported_globals_begin(&self) -> u32 {
        self.vmctx_imported_memories_begin()
            .checked_add(
                self.num_imported_memories
                    .checked_mul(u32::from(self.size_of_vmmemory_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// The offset of the `tables` array.
    pub fn vmctx_tables_begin(&self) -> u32 {
        self.vmctx_imported_globals_begin()
            .checked_add(
                self.num_imported_globals
                    .checked_mul(u32::from(self.size_of_vmglobal_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// The offset of the `memories` array.
    pub fn vmctx_memories_begin(&self) -> u32 {
        self.vmctx_tables_begin()
            .checked_add(
                self.num_defined_tables
                    .checked_mul(u32::from(self.size_of_vmtable_definition()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// The offset of the `globals` array.
    pub fn vmctx_globals_begin(&self) -> u32 {
        let offset = self
            .vmctx_memories_begin()
            .checked_add(
                self.num_defined_memories
                    .checked_mul(u32::from(self.size_of_vmmemory_definition()))
                    .unwrap(),
            )
            .unwrap();
        align(offset, 16)
    }

    /// The offset of the `anyfuncs` array.
    pub fn vmctx_anyfuncs_begin(&self) -> u32 {
        self.vmctx_globals_begin()
            .checked_add(
                self.num_defined_globals
                    .checked_mul(u32::from(self.size_of_vmglobal_definition()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// The offset of the builtin functions array.
    pub fn vmctx_builtin_functions_begin(&self) -> u32 {
        self.vmctx_anyfuncs_begin()
            .checked_add(
                self.num_imported_functions
                    .checked_add(self.num_defined_functions)
                    .unwrap()
                    .checked_mul(u32::from(self.size_of_vmcaller_checked_anyfunc()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the size of the `VMContext` allocation.
    pub fn size_of_vmctx(&self) -> u32 {
        self.vmctx_builtin_functions_begin()
            .checked_add(
                BuiltinFunctionIndex::builtin_functions_total_number()
                    .checked_mul(u32::from(self.pointer_size))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to `VMSharedSignatureId` index `index`.
    pub fn vmctx_vmshared_signature_id(&self, index: TypeIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_signature_ids);
        self.vmctx_signature_ids_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmshared_signature_index()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to `VMFunctionImport` index `index`.
    pub fn vmctx_vmfunction_import(&self, index: FuncIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_imported_functions);
        self.vmctx_imported_functions_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmfunction_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to `VMTableImport` index `index`.
    pub fn vmctx_vmtable_import(&self, index: TableIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_imported_tables);
        self.vmctx_imported_tables_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmtable_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to `VMMemoryImport` index `index`.
    pub fn vmctx_vmmemory_import(&self, index: MemoryIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_imported_memories);
        self.vmctx_imported_memories_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmmemory_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to `VMGlobalImport` index `index`.
    pub fn vmctx_vmglobal_import(&self, index: GlobalIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_imported_globals);
        self.vmctx_imported_globals_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmglobal_import()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to `VMTableDefinition` index `index`.
    pub fn vmctx_vmtable_definition(&self, index: DefinedTableIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_defined_tables);
        self.vmctx_tables_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmtable_definition()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to `VMMemoryDefinition` index `index`.
    pub fn vmctx_vmmemory_definition(&self, index: DefinedMemoryIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_defined_memories);
        self.vmctx_memories_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmmemory_definition()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to the `VMGlobalDefinition` index `index`.
    pub fn vmctx_vmglobal_definition(&self, index: DefinedGlobalIndex) -> u32 {
        assert_lt!(index.as_u32(), self.num_defined_globals);
        self.vmctx_globals_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmglobal_definition()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to the `VMCallerCheckedAnyfunc` for the given function
    /// index (either imported or defined).
    pub fn vmctx_anyfunc(&self, index: FuncIndex) -> u32 {
        self.vmctx_anyfuncs_begin()
            .checked_add(
                index
                    .as_u32()
                    .checked_mul(u32::from(self.size_of_vmcaller_checked_anyfunc()))
                    .unwrap(),
            )
            .unwrap()
    }

    /// Return the offset to the `body` field in `*const VMFunctionBody` index `index`.
    pub fn vmctx_vmfunction_import_body(&self, index: FuncIndex) -> u32 {
        self.vmctx_vmfunction_import(index)
            .checked_add(u32::from(self.vmfunction_import_body()))
            .unwrap()
    }

    /// Return the offset to the `vmctx` field in `*const VMFunctionBody` index `index`.
    pub fn vmctx_vmfunction_import_vmctx(&self, index: FuncIndex) -> u32 {
        self.vmctx_vmfunction_import(index)
            .checked_add(u32::from(self.vmfunction_import_vmctx()))
            .unwrap()
    }

    /// Return the offset to the `from` field in `VMTableImport` index `index`.
    pub fn vmctx_vmtable_import_from(&self, index: TableIndex) -> u32 {
        self.vmctx_vmtable_import(index)
            .checked_add(u32::from(self.vmtable_import_from()))
            .unwrap()
    }

    /// Return the offset to the `base` field in `VMTableDefinition` index `index`.
    pub fn vmctx_vmtable_definition_base(&self, index: DefinedTableIndex) -> u32 {
        self.vmctx_vmtable_definition(index)
            .checked_add(u32::from(self.vmtable_definition_base()))
            .unwrap()
    }

    /// Return the offset to the `current_elements` field in `VMTableDefinition` index `index`.
    pub fn vmctx_vmtable_definition_current_elements(&self, index: DefinedTableIndex) -> u32 {
        self.vmctx_vmtable_definition(index)
            .checked_add(u32::from(self.vmtable_definition_current_elements()))
            .unwrap()
    }

    /// Return the offset to the `from` field in `VMMemoryImport` index `index`.
    pub fn vmctx_vmmemory_import_from(&self, index: MemoryIndex) -> u32 {
        self.vmctx_vmmemory_import(index)
            .checked_add(u32::from(self.vmmemory_import_from()))
            .unwrap()
    }

    /// Return the offset to the `vmctx` field in `VMMemoryImport` index `index`.
    pub fn vmctx_vmmemory_import_vmctx(&self, index: MemoryIndex) -> u32 {
        self.vmctx_vmmemory_import(index)
            .checked_add(u32::from(self.vmmemory_import_vmctx()))
            .unwrap()
    }

    /// Return the offset to the `base` field in `VMMemoryDefinition` index `index`.
    pub fn vmctx_vmmemory_definition_base(&self, index: DefinedMemoryIndex) -> u32 {
        self.vmctx_vmmemory_definition(index)
            .checked_add(u32::from(self.vmmemory_definition_base()))
            .unwrap()
    }

    /// Return the offset to the `current_length` field in `VMMemoryDefinition` index `index`.
    pub fn vmctx_vmmemory_definition_current_length(&self, index: DefinedMemoryIndex) -> u32 {
        self.vmctx_vmmemory_definition(index)
            .checked_add(u32::from(self.vmmemory_definition_current_length()))
            .unwrap()
    }

    /// Return the offset to the `from` field in `VMGlobalImport` index `index`.
    pub fn vmctx_vmglobal_import_from(&self, index: GlobalIndex) -> u32 {
        self.vmctx_vmglobal_import(index)
            .checked_add(u32::from(self.vmglobal_import_from()))
            .unwrap()
    }

    /// Return the offset to builtin function in `VMBuiltinFunctionsArray` index `index`.
    pub fn vmctx_builtin_function(&self, index: BuiltinFunctionIndex) -> u32 {
        self.vmctx_builtin_functions_begin()
            .checked_add(
                index
                    .index()
                    .checked_mul(u32::from(self.pointer_size))
                    .unwrap(),
            )
            .unwrap()
    }
}

/// Offsets for `VMExternData`.
impl VMOffsets {
    /// Return the offset for `VMExternData::ref_count`.
    pub fn vm_extern_data_ref_count() -> u32 {
        0
    }
}

/// Offsets for `VMExternRefActivationsTable`.
impl VMOffsets {
    /// Return the offset for `VMExternRefActivationsTable::next`.
    pub fn vm_extern_ref_activation_table_next(&self) -> u32 {
        0
    }

    /// Return the offset for `VMExternRefActivationsTable::end`.
    pub fn vm_extern_ref_activation_table_end(&self) -> u32 {
        self.pointer_size.into()
    }
}

/// Target specific type for shared signature index.
#[derive(Debug, Copy, Clone)]
pub struct TargetSharedSignatureIndex(u32);

impl TargetSharedSignatureIndex {
    /// Constructs `TargetSharedSignatureIndex`.
    pub fn new(value: u32) -> Self {
        Self(value)
    }

    /// Returns index value.
    pub fn index(self) -> u32 {
        self.0
    }
}

#[cfg(test)]
mod tests {
    use crate::vmoffsets::align;

    #[test]
    fn alignment() {
        fn is_aligned(x: u32) -> bool {
            x % 16 == 0
        }
        assert!(is_aligned(align(0, 16)));
        assert!(is_aligned(align(32, 16)));
        assert!(is_aligned(align(33, 16)));
        assert!(is_aligned(align(31, 16)));
    }
}
