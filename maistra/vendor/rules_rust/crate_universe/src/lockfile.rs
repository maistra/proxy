//! Utility module for interracting with different kinds of lock files

use std::convert::TryFrom;
use std::ffi::OsStr;
use std::fs;
use std::path::Path;
use std::process::Command;
use std::str::FromStr;

use anyhow::{bail, Context as AnyhowContext, Result};
use hex::ToHex;
use serde::{Deserialize, Serialize};
use sha2::{Digest as Sha2Digest, Sha256};

use crate::config::Config;
use crate::context::Context;
use crate::splicing::{SplicingManifest, SplicingMetadata};

#[derive(Debug)]
pub enum LockfileKind {
    Auto,
    Bazel,
    Cargo,
}

impl LockfileKind {
    pub fn detect(path: &Path) -> Result<Self> {
        let content = fs::read_to_string(path)?;

        if serde_json::from_str::<Context>(&content).is_ok() {
            return Ok(Self::Bazel);
        }

        if cargo_lock::Lockfile::from_str(&content).is_ok() {
            return Ok(Self::Cargo);
        }

        bail!("Unknown Lockfile kind for {}", path.display())
    }
}

impl FromStr for LockfileKind {
    type Err = anyhow::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let lower = s.to_lowercase();
        if lower == "auto" {
            return Ok(Self::Auto);
        }

        if lower == "bazel" {
            return Ok(Self::Bazel);
        }

        if lower == "cargo" {
            return Ok(Self::Cargo);
        }

        bail!("Unknown LockfileKind: '{}'", s)
    }
}

pub fn is_cargo_lockfile(path: &Path, kind: &LockfileKind) -> bool {
    match kind {
        LockfileKind::Auto => match LockfileKind::detect(path) {
            Ok(kind) => matches!(kind, LockfileKind::Cargo),
            Err(_) => false,
        },
        LockfileKind::Bazel => false,
        LockfileKind::Cargo => true,
    }
}

pub fn lock_context(
    mut context: Context,
    config: &Config,
    splicing_manifest: &SplicingManifest,
    cargo_bin: &Path,
    rustc_bin: &Path,
) -> Result<Context> {
    // Ensure there is no existing checksum which could impact the lockfile results
    context.checksum = None;

    let checksum = Digest::new(&context, config, splicing_manifest, cargo_bin, rustc_bin)
        .context("Failed to generate context digest")?;

    Ok(Context {
        checksum: Some(checksum),
        ..context
    })
}

/// Write a [crate::planning::PlannedContext] to disk
pub fn write_lockfile(lockfile: Context, path: &Path, dry_run: bool) -> Result<()> {
    let content = serde_json::to_string_pretty(&lockfile)?;

    if dry_run {
        println!("{:#?}", content);
    } else {
        // Ensure the parent directory exists
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::write(path, content + "\n")
            .context(format!("Failed to write file to disk: {}", path.display()))?;
    }

    Ok(())
}

#[derive(Debug, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub struct Digest(String);

impl Digest {
    pub fn new(
        context: &Context,
        config: &Config,
        splicing_manifest: &SplicingManifest,
        cargo_bin: &Path,
        rustc_bin: &Path,
    ) -> Result<Self> {
        let splicing_metadata = SplicingMetadata::try_from((*splicing_manifest).clone())?;
        let cargo_version = Self::bin_version(cargo_bin)?;
        let rustc_version = Self::bin_version(rustc_bin)?;
        let cargo_bazel_version = env!("CARGO_PKG_VERSION");

        // Ensure the checksum of a digest is not present before computing one
        Ok(match context.checksum {
            Some(_) => Self::compute(
                &Context {
                    checksum: None,
                    ..context.clone()
                },
                config,
                &splicing_metadata,
                cargo_bazel_version,
                &cargo_version,
                &rustc_version,
            ),
            None => Self::compute(
                context,
                config,
                &splicing_metadata,
                cargo_bazel_version,
                &cargo_version,
                &rustc_version,
            ),
        })
    }

    fn compute(
        context: &Context,
        config: &Config,
        splicing_metadata: &SplicingMetadata,
        cargo_bazel_version: &str,
        cargo_version: &str,
        rustc_version: &str,
    ) -> Self {
        // Since this method is private, it should be expected that context is
        // always None. This then allows us to have this method not return a
        // Result.
        debug_assert!(context.checksum.is_none());

        let mut hasher = Sha256::new();

        hasher.update(cargo_bazel_version.as_bytes());
        hasher.update(b"\0");

        hasher.update(serde_json::to_string(context).unwrap().as_bytes());
        hasher.update(b"\0");

        hasher.update(serde_json::to_string(config).unwrap().as_bytes());
        hasher.update(b"\0");

        hasher.update(serde_json::to_string(splicing_metadata).unwrap().as_bytes());
        hasher.update(b"\0");

        hasher.update(cargo_version.as_bytes());
        hasher.update(b"\0");

        hasher.update(rustc_version.as_bytes());
        hasher.update(b"\0");

        Self(hasher.finalize().encode_hex::<String>())
    }

    fn bin_version(binary: &Path) -> Result<String> {
        let safe_vars = [OsStr::new("HOMEDRIVE"), OsStr::new("PATHEXT")];
        let env = std::env::vars_os().filter(|(var, _)| safe_vars.contains(&var.as_os_str()));

        let output = Command::new(binary)
            .arg("--version")
            .env_clear()
            .envs(env)
            .output()?;

        if !output.status.success() {
            bail!("Failed to query cargo version")
        }

        let version = String::from_utf8(output.stdout)?;
        Ok(version)
    }
}

impl PartialEq<str> for Digest {
    fn eq(&self, other: &str) -> bool {
        self.0 == other
    }
}

impl PartialEq<String> for Digest {
    fn eq(&self, other: &String) -> bool {
        &self.0 == other
    }
}

#[cfg(test)]
mod test {
    use crate::config::{CrateAnnotations, CrateId};
    use crate::splicing::cargo_config::{AdditionalRegistry, CargoConfig, Registry};

    use super::*;

    use std::collections::{BTreeMap, BTreeSet};
    use std::fs;

    #[test]
    fn simple_digest() {
        let context = Context::default();
        let config = Config::default();
        let splicing_metadata = SplicingMetadata::default();

        let digest = Digest::compute(
            &context,
            &config,
            &splicing_metadata,
            "0.1.0",
            "cargo 1.57.0 (b2e52d7ca 2021-10-21)",
            "rustc 1.57.0 (f1edd0429 2021-11-29)",
        );

        assert_eq!(
            digest,
            Digest("4c8bc5de2d6d7acc7997ae9870e52bc0f0fcbc2b94076e61162078be6a69cc3b".to_owned())
        );
    }

    #[test]
    fn digest_with_config() {
        let context = Context::default();
        let config = Config {
            generate_build_scripts: false,
            annotations: BTreeMap::from([(
                CrateId::new("rustonomicon".to_owned(), "1.0.0".to_owned()),
                CrateAnnotations {
                    compile_data_glob: Some(BTreeSet::from(["arts/**".to_owned()])),
                    ..CrateAnnotations::default()
                },
            )]),
            cargo_config: None,
            supported_platform_triples: BTreeSet::from([
                "aarch64-apple-darwin".to_owned(),
                "aarch64-unknown-linux-gnu".to_owned(),
                "wasm32-unknown-unknown".to_owned(),
                "wasm32-wasi".to_owned(),
                "x86_64-apple-darwin".to_owned(),
                "x86_64-pc-windows-msvc".to_owned(),
                "x86_64-unknown-freebsd".to_owned(),
                "x86_64-unknown-linux-gnu".to_owned(),
            ]),
            ..Config::default()
        };

        let splicing_metadata = SplicingMetadata::default();

        let digest = Digest::compute(
            &context,
            &config,
            &splicing_metadata,
            "0.1.0",
            "cargo 1.57.0 (b2e52d7ca 2021-10-21)",
            "rustc 1.57.0 (f1edd0429 2021-11-29)",
        );

        assert_eq!(
            digest,
            Digest("7a0d2f5fce05c4d433826b5c4748bec7b125b79182de598dc700e893e09077e9".to_owned())
        );
    }

    #[test]
    fn digest_with_splicing_metadata() {
        let context = Context::default();
        let config = Config::default();
        let splicing_metadata = SplicingMetadata {
            direct_packages: BTreeMap::from([(
                "rustonomicon".to_owned(),
                cargo_toml::DependencyDetail {
                    version: Some("1.0.0".to_owned()),
                    ..cargo_toml::DependencyDetail::default()
                },
            )]),
            manifests: BTreeMap::new(),
            cargo_config: None,
        };

        let digest = Digest::compute(
            &context,
            &config,
            &splicing_metadata,
            "0.1.0",
            "cargo 1.57.0 (b2e52d7ca 2021-10-21)",
            "rustc 1.57.0 (f1edd0429 2021-11-29)",
        );

        assert_eq!(
            digest,
            Digest("fb5d7854dae366d4a9ff135208c28f08c14c2608dd6c5aa1b35b6e677dd53c06".to_owned())
        );
    }

    #[test]
    fn digest_with_cargo_config() {
        let context = Context::default();
        let config = Config::default();
        let cargo_config = CargoConfig {
            registries: BTreeMap::from([
                (
                    "art-crates-remote".to_owned(),
                    AdditionalRegistry {
                        index: "https://artprod.mycompany/artifactory/git/cargo-remote.git"
                            .to_owned(),
                        token: None,
                    },
                ),
                (
                    "crates-io".to_owned(),
                    AdditionalRegistry {
                        index: "https://github.com/rust-lang/crates.io-index".to_owned(),
                        token: None,
                    },
                ),
            ]),
            registry: Registry {
                default: "art-crates-remote".to_owned(),
                token: None,
            },
            source: BTreeMap::new(),
        };

        let splicing_metadata = SplicingMetadata {
            cargo_config: Some(cargo_config),
            ..SplicingMetadata::default()
        };

        let digest = Digest::compute(
            &context,
            &config,
            &splicing_metadata,
            "0.1.0",
            "cargo 1.57.0 (b2e52d7ca 2021-10-21)",
            "rustc 1.57.0 (f1edd0429 2021-11-29)",
        );

        assert_eq!(
            digest,
            Digest("2b32833e4265bce03df70dbb9c2b32a78879cc02fbe88a481e3fe4a17812aca9".to_owned())
        );
    }

    #[test]
    fn detect_bazel_lockfile() {
        let temp_dir = tempfile::tempdir().unwrap();
        let lockfile = temp_dir.as_ref().join("lockfile");
        fs::write(
            &lockfile,
            serde_json::to_string(&crate::context::Context::default()).unwrap(),
        )
        .unwrap();

        let kind = LockfileKind::detect(&lockfile).unwrap();
        assert!(matches!(kind, LockfileKind::Bazel));
    }

    #[test]
    fn detect_cargo_lockfile() {
        let temp_dir = tempfile::tempdir().unwrap();
        let lockfile = temp_dir.as_ref().join("lockfile");
        fs::write(
            &lockfile,
            textwrap::dedent(
                r#"
                version = 3

                [[package]]
                name = "detect"
                version = "0.1.0"
                "#,
            ),
        )
        .unwrap();

        let kind = LockfileKind::detect(&lockfile).unwrap();
        assert!(matches!(kind, LockfileKind::Cargo));
    }

    #[test]
    fn detect_invalid_lockfile() {
        let temp_dir = tempfile::tempdir().unwrap();
        let lockfile = temp_dir.as_ref().join("lockfile");
        fs::write(&lockfile, "]} invalid {[").unwrap();

        assert!(LockfileKind::detect(&lockfile).is_err());
    }

    #[test]
    fn detect_missing_lockfile() {
        let temp_dir = tempfile::tempdir().unwrap();
        let lockfile = temp_dir.as_ref().join("lockfile");
        assert!(LockfileKind::detect(&lockfile).is_err());
    }
}
