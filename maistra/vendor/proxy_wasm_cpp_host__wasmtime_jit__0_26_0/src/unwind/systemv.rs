//! Module for System V ABI unwind registry.

use anyhow::{bail, Result};
use cranelift_codegen::isa::{unwind::UnwindInfo, TargetIsa};
use gimli::{
    write::{Address, EhFrame, EndianVec, FrameTable, Writer},
    RunTimeEndian,
};

/// Represents a registry of function unwind information for System V ABI.
pub struct UnwindRegistry {
    base_address: usize,
    functions: Vec<gimli::write::FrameDescriptionEntry>,
    frame_table: Vec<u8>,
    registrations: Vec<usize>,
    published: bool,
}

extern "C" {
    // libunwind import
    fn __register_frame(fde: *const u8);
    fn __deregister_frame(fde: *const u8);
}

impl UnwindRegistry {
    /// Creates a new unwind registry with the given base address.
    pub fn new(base_address: usize) -> Self {
        Self {
            base_address,
            functions: Vec::new(),
            frame_table: Vec::new(),
            registrations: Vec::new(),
            published: false,
        }
    }

    /// Registers a function given the start offset, length, and unwind information.
    pub fn register(&mut self, func_start: u32, _func_len: u32, info: &UnwindInfo) -> Result<()> {
        if self.published {
            bail!("unwind registry has already been published");
        }

        match info {
            UnwindInfo::SystemV(info) => {
                self.functions.push(info.to_fde(Address::Constant(
                    self.base_address as u64 + func_start as u64,
                )));
            }
            _ => bail!("unsupported unwind information"),
        }

        Ok(())
    }

    /// Publishes all registered functions.
    pub fn publish(&mut self, isa: &dyn TargetIsa) -> Result<()> {
        if self.published {
            bail!("unwind registry has already been published");
        }

        if self.functions.is_empty() {
            self.published = true;
            return Ok(());
        }

        self.set_frame_table(isa)?;

        unsafe {
            self.register_frames();
        }

        self.published = true;

        Ok(())
    }

    fn set_frame_table(&mut self, isa: &dyn TargetIsa) -> Result<()> {
        let mut table = FrameTable::default();
        let cie_id = table.add_cie(match isa.create_systemv_cie() {
            Some(cie) => cie,
            None => bail!("ISA does not support System V unwind information"),
        });

        let functions = std::mem::replace(&mut self.functions, Vec::new());

        for func in functions {
            table.add_fde(cie_id, func);
        }

        let mut eh_frame = EhFrame(EndianVec::new(RunTimeEndian::default()));
        table.write_eh_frame(&mut eh_frame).unwrap();

        if cfg!(any(
            all(target_os = "linux", target_env = "gnu"),
            target_os = "freebsd"
        )) {
            // libgcc expects a terminating "empty" length, so write a 0 length at the end of the table.
            eh_frame.0.write_u32(0).unwrap();
        }

        self.frame_table = eh_frame.0.into_vec();

        Ok(())
    }

    unsafe fn register_frames(&mut self) {
        if cfg!(any(
            all(target_os = "linux", target_env = "gnu"),
            target_os = "freebsd"
        )) {
            // On gnu (libgcc), `__register_frame` will walk the FDEs until an entry of length 0
            let ptr = self.frame_table.as_ptr();
            __register_frame(ptr);
            self.registrations.push(ptr as usize);
        } else {
            // For libunwind, `__register_frame` takes a pointer to a single FDE
            let start = self.frame_table.as_ptr();
            let end = start.add(self.frame_table.len());
            let mut current = start;

            // Walk all of the entries in the frame table and register them
            while current < end {
                let len = std::ptr::read::<u32>(current as *const u32) as usize;

                // Skip over the CIE
                if current != start {
                    __register_frame(current);
                    self.registrations.push(current as usize);
                }

                // Move to the next table entry (+4 because the length itself is not inclusive)
                current = current.add(len + 4);
            }
        }
    }
}

impl Drop for UnwindRegistry {
    fn drop(&mut self) {
        if self.published {
            unsafe {
                // libgcc stores the frame entries as a linked list in decreasing sort order
                // based on the PC value of the registered entry.
                //
                // As we store the registrations in increasing order, it would be O(N^2) to
                // deregister in that order.
                //
                // To ensure that we just pop off the first element in the list upon every
                // deregistration, walk our list of registrations backwards.
                for fde in self.registrations.iter().rev() {
                    __deregister_frame(*fde as *const _);
                }
            }
        }
    }
}
