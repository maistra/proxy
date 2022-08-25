///! Gathering dependencies is the largest part of annotating.
use cargo_metadata::{Metadata as CargoMetadata, Node, NodeDep, Package, PackageId};
use serde::{Deserialize, Serialize};

use crate::utils::sanitize_module_name;
use crate::utils::starlark::{Select, SelectList};

/// A representation of a crate dependency
#[derive(Debug, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord, Clone)]
pub struct Dependency {
    /// The PackageId of the target
    pub package_id: PackageId,

    /// The library target name of the dependency.
    pub target_name: String,

    /// The alias for the dependency from the perspective of the current package
    pub alias: Option<String>,
}

/// A collection of [Dependency]s sorted by dependency kind.
#[derive(Debug, Default, Serialize, Deserialize)]
pub struct DependencySet {
    pub normal_deps: SelectList<Dependency>,
    pub normal_dev_deps: SelectList<Dependency>,
    pub proc_macro_deps: SelectList<Dependency>,
    pub proc_macro_dev_deps: SelectList<Dependency>,
    pub build_deps: SelectList<Dependency>,
    pub build_proc_macro_deps: SelectList<Dependency>,
}

impl DependencySet {
    /// Collect all dependencies for a given node in the resolve graph.
    pub fn new_for_node(node: &Node, metadata: &CargoMetadata) -> Self {
        let (normal_dev_deps, normal_deps) = {
            let (dev, normal) = node
                .deps
                .iter()
                // Do not track workspace members as dependencies. Users are expected to maintain those connections
                .filter(|dep| !is_workspace_member(dep, metadata))
                .filter(|dep| is_lib_package(&metadata[&dep.pkg]))
                .filter(|dep| is_normal_dependency(dep) || is_dev_dependency(dep))
                .partition(|dep| is_dev_dependency(dep));

            (
                collect_deps_selectable(dev, metadata),
                collect_deps_selectable(normal, metadata),
            )
        };

        let (proc_macro_dev_deps, proc_macro_deps) = {
            let (dev, normal) = node
                .deps
                .iter()
                // Do not track workspace members as dependencies. Users are expected to maintain those connections
                .filter(|dep| !is_workspace_member(dep, metadata))
                .filter(|dep| is_proc_macro_package(&metadata[&dep.pkg]))
                .filter(|dep| !is_build_dependency(dep))
                .partition(|dep| is_dev_dependency(dep));

            (
                collect_deps_selectable(dev, metadata),
                collect_deps_selectable(normal, metadata),
            )
        };

        let (build_proc_macro_deps, mut build_deps) = {
            let (proc_macro, normal) = node
                .deps
                .iter()
                // Do not track workspace members as dependencies. Users are expected to maintain those connections
                .filter(|dep| !is_workspace_member(dep, metadata))
                .filter(|dep| is_build_dependency(dep))
                .filter(|dep| !is_dev_dependency(dep))
                .partition(|dep| is_proc_macro_package(&metadata[&dep.pkg]));

            (
                collect_deps_selectable(proc_macro, metadata),
                collect_deps_selectable(normal, metadata),
            )
        };

        // `*-sys` packages follow slightly different rules than other dependencies. These
        // packages seem to provide some environment variables required to build the top level
        // package and are expected to be avialable to other build scripts. If a target depends
        // on a `*-sys` crate for itself, so would it's build script. Hopefully this is correct.
        // https://doc.rust-lang.org/cargo/reference/build-scripts.html#the-links-manifest-key
        // https://doc.rust-lang.org/cargo/reference/build-scripts.html#-sys-packages
        let sys_name = format!("{}-sys", &metadata[&node.id].name);
        normal_deps.configurations().into_iter().for_each(|config| {
            normal_deps
                .get_iter(config)
                // Iterating over known key should be safe
                .unwrap()
                // Add any normal dependency to build dependencies that are associated `*-sys` crates
                .for_each(|dep| {
                    let dep_pkg_name = &metadata[&dep.package_id].name;
                    if *dep_pkg_name == sys_name {
                        build_deps.insert(dep.clone(), config.cloned())
                    }
                });
        });

        Self {
            normal_deps,
            normal_dev_deps,
            proc_macro_deps,
            proc_macro_dev_deps,
            build_deps,
            build_proc_macro_deps,
        }
    }
}

fn collect_deps_selectable(
    deps: Vec<&NodeDep>,
    metadata: &cargo_metadata::Metadata,
) -> SelectList<Dependency> {
    let mut selectable = SelectList::default();

    for dep in deps.into_iter() {
        let dep_pkg = &metadata[&dep.pkg];
        let target_name = get_library_target_name(dep_pkg, &dep.name);
        let alias = get_target_alias(&dep.name, dep_pkg);

        for kind_info in &dep.dep_kinds {
            selectable.insert(
                Dependency {
                    package_id: dep.pkg.clone(),
                    target_name: target_name.clone(),
                    alias: alias.clone(),
                },
                kind_info
                    .target
                    .as_ref()
                    .map(|platform| platform.to_string()),
            );
        }
    }

    selectable
}

fn is_lib_package(package: &Package) -> bool {
    package.targets.iter().any(|target| {
        target
            .crate_types
            .iter()
            .any(|t| ["lib", "rlib"].contains(&t.as_str()))
    })
}

fn is_proc_macro_package(package: &Package) -> bool {
    package
        .targets
        .iter()
        .any(|target| target.crate_types.iter().any(|t| t == "proc-macro"))
}

fn is_dev_dependency(node_dep: &NodeDep) -> bool {
    let is_normal_dep = is_normal_dependency(node_dep);
    let is_dev_dep = node_dep
        .dep_kinds
        .iter()
        .any(|k| matches!(k.kind, cargo_metadata::DependencyKind::Development));

    // In the event that a dependency is listed as both a dev and normal dependency,
    // it's only considered a dev dependency if it's __not__ a normal dependency.
    !is_normal_dep && is_dev_dep
}

fn is_build_dependency(node_dep: &NodeDep) -> bool {
    node_dep
        .dep_kinds
        .iter()
        .any(|k| matches!(k.kind, cargo_metadata::DependencyKind::Build))
}

fn is_normal_dependency(node_dep: &NodeDep) -> bool {
    node_dep
        .dep_kinds
        .iter()
        .any(|k| matches!(k.kind, cargo_metadata::DependencyKind::Normal))
}

fn is_workspace_member(node_dep: &NodeDep, metadata: &CargoMetadata) -> bool {
    metadata
        .workspace_members
        .iter()
        .any(|id| id == &node_dep.pkg)
}

fn get_library_target_name(package: &Package, potential_name: &str) -> String {
    // If the potential name is not an alias in a dependent's package, a target's name
    // should match which means we already know what the target library name is.
    if package.targets.iter().any(|t| t.name == potential_name) {
        return potential_name.to_string();
    }

    // Locate any library type targets
    let lib_targets: Vec<&cargo_metadata::Target> = package
        .targets
        .iter()
        .filter(|t| t.kind.iter().any(|k| k == "lib" || k == "proc-macro"))
        .collect();

    // Only one target should be found
    assert_eq!(lib_targets.len(), 1);

    let target = lib_targets.into_iter().last().unwrap();
    target.name.clone()
}

/// The resolve graph (resolve.nodes[#].deps[#].name) of Cargo metadata uses module names
/// for targets where packages (packages[#].targets[#].name) uses crate names. In order to
/// determine whether or not a dependency is aliased, we compare it with all available targets
/// on it's package. Note that target names are not guaranteed to be module names where Node
/// dependnecies are, so we need to do a conversion to check for this
fn get_target_alias(target_name: &str, package: &Package) -> Option<String> {
    match package
        .targets
        .iter()
        .all(|t| sanitize_module_name(&t.name) != target_name)
    {
        true => Some(target_name.to_string()),
        false => None,
    }
}

#[cfg(test)]
mod test {
    use std::collections::BTreeSet;

    use super::*;

    use crate::test::*;

    /// Locate the [cargo_metadata::Node] for the crate matching the given name
    fn find_metadata_node<'a>(
        name: &str,
        metadata: &'a cargo_metadata::Metadata,
    ) -> &'a cargo_metadata::Node {
        metadata
            .resolve
            .as_ref()
            .unwrap()
            .nodes
            .iter()
            .find(|node| {
                let pkg = &metadata[&node.id];
                pkg.name == name
            })
            .unwrap()
    }

    #[test]
    fn sys_dependencies() {
        let metadata = metadata::build_scripts();

        let openssl_node = find_metadata_node("openssl", &metadata);

        let dependencies = DependencySet::new_for_node(openssl_node, &metadata);

        let sys_crate = dependencies
            .normal_deps
            .get_iter(None)
            .unwrap()
            .find(|dep| {
                let pkg = &metadata[&dep.package_id];
                pkg.name == "openssl-sys"
            });

        // sys crates like `openssl-sys` should always be dependencies of any
        // crate which matches it's name minus the `-sys` suffix
        assert!(sys_crate.is_some());
    }

    #[test]
    fn tracked_aliases() {
        let metadata = metadata::alias();

        let aliases_node = find_metadata_node("aliases", &metadata);
        let dependencies = DependencySet::new_for_node(aliases_node, &metadata);

        let aliases: Vec<&Dependency> = dependencies
            .normal_deps
            .get_iter(None)
            .unwrap()
            .filter(|dep| dep.alias.is_some())
            .collect();

        assert_eq!(aliases.len(), 2);

        let expected: BTreeSet<String> = aliases
            .into_iter()
            .map(|dep| dep.alias.as_ref().unwrap().clone())
            .collect();

        assert_eq!(
            expected,
            BTreeSet::from(["pinned_log".to_owned(), "pinned_names".to_owned()])
        );
    }

    #[test]
    fn matched_rlib() {
        let metadata = metadata::crate_types();

        let node = find_metadata_node("crate-types", &metadata);
        let dependencies = DependencySet::new_for_node(node, &metadata);

        let rlib_deps: Vec<&Dependency> = dependencies
            .normal_deps
            .get_iter(None)
            .unwrap()
            .filter(|dep| {
                let pkg = &metadata[&dep.package_id];
                pkg.targets
                    .iter()
                    .any(|t| t.crate_types.contains(&"rlib".to_owned()))
            })
            .collect();

        // Currently the only expected __explicitly__ "rlib" target in this metadata is `sysinfo`.
        assert_eq!(rlib_deps.len(), 1);

        let sysinfo_dep = rlib_deps.iter().last().unwrap();
        assert_eq!(sysinfo_dep.target_name, "sysinfo");
    }

    #[test]
    fn multiple_dep_kinds() {
        let metadata = metadata::multi_cfg_dep();

        let node = find_metadata_node("cpufeatures", &metadata);
        let dependencies = DependencySet::new_for_node(node, &metadata);

        let libc_cfgs: Vec<Option<String>> = dependencies
            .normal_deps
            .configurations()
            .into_iter()
            .flat_map(|conf| {
                dependencies
                    .normal_deps
                    .get_iter(conf)
                    .expect("Iterating over known keys should never panic")
                    .filter(|dep| dep.target_name == "libc")
                    .map(move |_| conf.cloned())
            })
            .collect();

        assert_eq!(libc_cfgs.len(), 2);

        let cfg_strs: BTreeSet<String> = libc_cfgs.into_iter().flatten().collect();
        assert_eq!(
            cfg_strs,
            BTreeSet::from([
                "aarch64-apple-darwin".to_owned(),
                "cfg(all(target_arch = \"aarch64\", target_os = \"linux\"))".to_owned(),
            ])
        );
    }
}
