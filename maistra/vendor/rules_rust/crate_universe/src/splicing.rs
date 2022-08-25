//! This module is responsible for finding a Cargo workspace

pub(crate) mod cargo_config;
mod splicer;

use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::convert::TryFrom;
use std::fs;
use std::path::{Path, PathBuf};
use std::str::FromStr;

use anyhow::{bail, Context, Result};
use cargo_toml::Manifest;
use hex::ToHex;
use serde::{Deserialize, Serialize};

use crate::config::CrateId;
use crate::metadata::LockGenerator;
use crate::utils::starlark::Label;

use self::cargo_config::CargoConfig;
pub use self::splicer::*;

#[derive(Debug, Default, Serialize, Deserialize)]
pub struct ExtraManifestInfo {
    // The path to a Cargo Manifest
    pub manifest: PathBuf,

    // The URL where the manifest's package can be downloaded
    pub url: String,

    // The Sha256 checksum of the downloaded package located at `url`.
    pub sha256: String,
}

type DirectPackageManifest = BTreeMap<String, cargo_toml::DependencyDetail>;

#[derive(Debug, Default, Serialize, Deserialize, Clone)]
#[serde(deny_unknown_fields)]
pub struct SplicingManifest {
    /// A set of all packages directly written to the rule
    pub direct_packages: DirectPackageManifest,

    /// A mapping of manifest paths to the labels representing them
    pub manifests: BTreeMap<PathBuf, Label>,

    /// The path of a Cargo config file
    pub cargo_config: Option<PathBuf>,

    /// The Cargo resolver version to use for splicing
    pub resolver_version: cargo_toml::Resolver,
}

impl FromStr for SplicingManifest {
    type Err = serde_json::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        serde_json::from_str(s)
    }
}

impl SplicingManifest {
    pub fn try_from_path<T: AsRef<Path>>(path: T) -> Result<Self> {
        let content = fs::read_to_string(path.as_ref())?;
        Self::from_str(&content).context("Failed to load SplicingManifest")
    }

    pub fn absoulutize(self, relative_to: &Path) -> Self {
        let Self {
            manifests,
            cargo_config,
            ..
        } = self;

        // Ensure manifests all have absolute paths
        let manifests = manifests
            .into_iter()
            .map(|(path, label)| {
                if !path.is_absolute() {
                    let path = relative_to.join(path);
                    (path, label)
                } else {
                    (path, label)
                }
            })
            .collect();

        // Ensure the cargo config is located at an absolute path
        let cargo_config = cargo_config.map(|path| {
            if !path.is_absolute() {
                relative_to.join(path)
            } else {
                path
            }
        });

        Self {
            manifests,
            cargo_config,
            ..self
        }
    }
}

#[derive(Debug, Serialize, Default)]
pub struct SplicingMetadata {
    /// A set of all packages directly written to the rule
    pub direct_packages: DirectPackageManifest,

    /// A mapping of manifest paths to the labels representing them
    pub manifests: BTreeMap<Label, cargo_toml::Manifest>,

    /// The path of a Cargo config file
    pub cargo_config: Option<CargoConfig>,
}

impl TryFrom<SplicingManifest> for SplicingMetadata {
    type Error = anyhow::Error;

    fn try_from(value: SplicingManifest) -> Result<Self, Self::Error> {
        let direct_packages = value.direct_packages;

        let manifests = value
            .manifests
            .into_iter()
            .map(|(path, label)| {
                let manifest = cargo_toml::Manifest::from_path(&path)
                    .with_context(|| format!("Failed to load manifest '{}'", path.display()))?;

                Ok((label, manifest))
            })
            .collect::<Result<BTreeMap<Label, Manifest>>>()?;

        let cargo_config = match value.cargo_config {
            Some(path) => Some(
                CargoConfig::try_from_path(&path)
                    .with_context(|| format!("Failed to load cargo config '{}'", path.display()))?,
            ),
            None => None,
        };

        Ok(Self {
            direct_packages,
            manifests,
            cargo_config,
        })
    }
}

/// A collection of information required for reproducible "extra worksspace members".
#[derive(Debug, Default, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ExtraManifestsManifest {
    pub manifests: Vec<ExtraManifestInfo>,
}

impl FromStr for ExtraManifestsManifest {
    type Err = serde_json::Error;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        serde_json::from_str(s)
    }
}

impl ExtraManifestsManifest {
    pub fn try_from_path<T: AsRef<Path>>(path: T) -> Result<Self> {
        let content = fs::read_to_string(path.as_ref())?;
        Self::from_str(&content).context("Failed to load ExtraManifestsManifest")
    }

    pub fn absoulutize(self) -> Self {
        self
    }
}

#[derive(Debug, Default, Serialize, Deserialize, Clone)]
pub struct SourceInfo {
    /// A url where to a `.crate` file.
    pub url: String,

    /// The `.crate` file's sha256 checksum.
    pub sha256: String,
}

/// Information about the Cargo workspace relative to the Bazel workspace
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct WorkspaceMetadata {
    /// A mapping of crates to information about where their source can be downloaded
    #[serde(serialize_with = "toml::ser::tables_last")]
    pub sources: BTreeMap<CrateId, SourceInfo>,

    /// The path from the root of a Bazel workspace to the root of the Cargo workspace
    pub workspace_prefix: Option<String>,

    /// Paths from the root of a Bazel workspace to a Cargo package
    #[serde(serialize_with = "toml::ser::tables_last")]
    pub package_prefixes: BTreeMap<String, String>,
}

impl TryFrom<toml::Value> for WorkspaceMetadata {
    type Error = anyhow::Error;

    fn try_from(value: toml::Value) -> Result<Self, Self::Error> {
        match value.get("cargo-bazel") {
            Some(v) => v
                .to_owned()
                .try_into()
                .context("Failed to deserialize toml value"),
            None => bail!("cargo-bazel workspace metadata not found"),
        }
    }
}

impl TryFrom<serde_json::Value> for WorkspaceMetadata {
    type Error = anyhow::Error;

    fn try_from(value: serde_json::Value) -> Result<Self, Self::Error> {
        match value.get("cargo-bazel") {
            Some(value) => {
                serde_json::from_value(value.to_owned()).context("Faield to deserialize json value")
            }
            None => bail!("cargo-bazel workspace metadata not found"),
        }
    }
}

impl WorkspaceMetadata {
    fn new(
        splicing_manifest: &SplicingManifest,
        extra_manifests_manifest: &ExtraManifestsManifest,
        injected_manifests: HashMap<&PathBuf, String>,
    ) -> Result<Self> {
        let mut sources = BTreeMap::new();

        for config in extra_manifests_manifest.manifests.iter() {
            let package = match read_manifest(&config.manifest) {
                Ok(manifest) => match manifest.package {
                    Some(pkg) => pkg,
                    None => continue,
                },
                Err(e) => return Err(e),
            };

            let id = CrateId::new(package.name, package.version);
            let info = SourceInfo {
                url: config.url.clone(),
                sha256: config.sha256.clone(),
            };

            sources.insert(id, info);
        }

        let mut package_prefixes: BTreeMap<String, String> = injected_manifests
            .iter()
            .filter_map(|(original_manifest, cargo_pkg_name)| {
                let label = match splicing_manifest.manifests.get(*original_manifest) {
                    Some(v) => v,
                    None => return None,
                };

                let package = match &label.package {
                    Some(pkg) => PathBuf::from(pkg),
                    None => return None,
                };

                let prefix = package.to_string_lossy().to_string();

                Some((cargo_pkg_name.clone(), prefix))
            })
            .collect();

        // It is invald for toml maps to use empty strings as keys. In the case
        // the empty key is expected to be the root package. If the root package
        // has a prefix, then all other packages will as well (even if no other
        // manifest represents them). The value is then saved as a separate value
        let workspace_prefix = package_prefixes.remove("");

        let package_prefixes = package_prefixes
            .into_iter()
            .map(|(k, v)| {
                let prefix_path = PathBuf::from(v);
                let prefix = prefix_path.parent().unwrap();
                (k, prefix.to_string_lossy().to_string())
            })
            .collect();

        Ok(Self {
            sources,
            workspace_prefix,
            package_prefixes,
        })
    }

    pub fn write_registry_urls(
        lockfile: &cargo_lock::Lockfile,
        manifest_path: &SplicedManifest,
    ) -> Result<()> {
        let mut manifest = read_manifest(manifest_path.as_path_buf())?;

        let mut workspace_metaata = WorkspaceMetadata::try_from(
            manifest
                .workspace
                .as_ref()
                .unwrap()
                .metadata
                .as_ref()
                .unwrap()
                .clone(),
        )?;

        // Locate all packages soruced from a registry
        let pkg_sources: Vec<&cargo_lock::Package> = lockfile
            .packages
            .iter()
            .filter(|pkg| pkg.source.is_some())
            .filter(|pkg| pkg.source.as_ref().unwrap().is_registry())
            .collect();

        // Collect a unique set of index urls
        let index_urls: BTreeSet<String> = pkg_sources
            .iter()
            .map(|pkg| pkg.source.as_ref().unwrap().url().to_string())
            .collect();

        // Load the cargo config
        let cargo_config = {
            // Note that this path must match the one defined in `splicing::setup_cargo_config`
            let config_path = manifest_path
                .as_path_buf()
                .parent()
                .unwrap()
                .join(".cargo")
                .join("config.toml");

            if config_path.exists() {
                Some(CargoConfig::try_from_path(&config_path)?)
            } else {
                None
            }
        };

        // Load each index for easy access
        let crate_indexes = index_urls
            .into_iter()
            .map(|url| {
                let index = {
                    // Ensure the correct registry is mapped based on the give Cargo config.
                    let index_url = if let Some(config) = &cargo_config {
                        if let Some(source) = config.get_source_from_url(&url) {
                            if let Some(replace_with) = &source.replace_with {
                                if let Some(replacement) = config.get_registry_index_url_by_name(replace_with) {
                                    replacement
                                } else {
                                    bail!("Tried to replace registry {} with registry named {} but didn't have metadata about the replacement", url, replace_with);
                                }
                            } else {
                                &url
                            }
                        } else {
                            &url
                        }
                    } else {
                        &url
                    };

                    // Load the index for the current url
                    let index = crates_index::Index::from_url(index_url)
                        .with_context(|| format!("Failed to load index for url: {}", index_url))?;

                    // Ensure each index has a valid index config
                    index.index_config().with_context(|| {
                        format!("`config.json` not found in index: {}", index_url)
                    })?;

                    index
                };

                Ok((url, index))
            })
            .collect::<Result<BTreeMap<String, crates_index::Index>>>()
            .context("Failed to locate crate indexes")?;

        // Get the download URL of each package based on it's registry url.
        let additional_sources = pkg_sources
            .iter()
            .filter_map(|pkg| {
                let source_id = pkg.source.as_ref().unwrap();
                let index = &crate_indexes[&source_id.url().to_string()];
                let index_config = index.index_config().unwrap();

                index.crate_(pkg.name.as_str()).map(|crate_idx| {
                    crate_idx
                        .versions()
                        .iter()
                        .find(|v| v.version() == pkg.version.to_string())
                        .and_then(|v| {
                            v.download_url(&index_config).map(|url| {
                                let crate_id =
                                    CrateId::new(v.name().to_owned(), v.version().to_owned());
                                let sha256 = pkg
                                    .checksum
                                    .as_ref()
                                    .and_then(|sum| {
                                        sum.as_sha256().map(|sum| sum.encode_hex::<String>())
                                    })
                                    .unwrap_or_else(|| v.checksum().encode_hex::<String>());
                                let source_info = SourceInfo { url, sha256 };
                                (crate_id, source_info)
                            })
                        })
                })
            })
            .flatten();

        workspace_metaata.sources.extend(additional_sources);
        workspace_metaata.inject_into(&mut manifest)?;

        write_root_manifest(manifest_path.as_path_buf(), manifest)?;

        Ok(())
    }

    fn inject_into(&self, manifest: &mut Manifest) -> Result<()> {
        let metadata_value = toml::Value::try_from(self)?;
        let mut workspace = manifest.workspace.as_mut().unwrap();

        match &mut workspace.metadata {
            Some(data) => match data.as_table_mut() {
                Some(map) => {
                    map.insert("cargo-bazel".to_owned(), metadata_value);
                }
                None => bail!("The metadata field is always expected to be a table"),
            },
            None => {
                let mut table = toml::map::Map::new();
                table.insert("cargo-bazel".to_owned(), metadata_value);
                workspace.metadata = Some(toml::Value::Table(table))
            }
        }

        Ok(())
    }
}

#[derive(Debug)]
pub enum SplicedManifest {
    Workspace(PathBuf),
    Package(PathBuf),
    MultiPackage(PathBuf),
}

impl SplicedManifest {
    pub fn as_path_buf(&self) -> &PathBuf {
        match self {
            SplicedManifest::Workspace(p) => p,
            SplicedManifest::Package(p) => p,
            SplicedManifest::MultiPackage(p) => p,
        }
    }
}

pub fn read_manifest(manifest: &Path) -> Result<Manifest> {
    let content = fs::read_to_string(manifest)?;
    cargo_toml::Manifest::from_str(content.as_str()).context("Failed to deserialize manifest")
}

pub fn generate_lockfile(
    manifest_path: &SplicedManifest,
    existing_lock: &Option<PathBuf>,
    cargo_bin: &Path,
    rustc_bin: &Path,
) -> Result<cargo_lock::Lockfile> {
    let manifest_dir = manifest_path
        .as_path_buf()
        .parent()
        .expect("Every manifest should be contained in a parent directory");

    let root_lockfile_path = manifest_dir.join("Cargo.lock");

    // Remove the file so it's not overwitten if it happens to be a symlink.
    if root_lockfile_path.exists() {
        fs::remove_file(&root_lockfile_path)?;
    }

    // Generate the new lockfile
    let lockfile = LockGenerator::new(PathBuf::from(cargo_bin), PathBuf::from(rustc_bin))
        .generate(manifest_path.as_path_buf(), existing_lock)?;

    // Write the lockfile to disk
    if !root_lockfile_path.exists() {
        bail!("Failed to generate Cargo.lock file")
    }

    Ok(lockfile)
}
