//! A template engine backed by [Tera] for rendering Files.

use std::collections::HashMap;

use anyhow::{Context as AnyhowContext, Result};
use serde_json::{from_value, to_value, Value};
use tera::{self, Tera};

use crate::config::RenderConfig;
use crate::context::Context;
use crate::rendering::{
    render_crate_bazel_label, render_crate_bazel_repository, render_crate_build_file,
    render_module_label,
};
use crate::utils::sanitize_repository_name;
use crate::utils::starlark::SelectStringList;

pub struct TemplateEngine {
    engine: Tera,
    context: tera::Context,
}

impl TemplateEngine {
    pub fn new(render_config: &RenderConfig) -> Self {
        let mut tera = Tera::default();
        tera.add_raw_templates(vec![
            (
                "partials/module/aliases_map.j2",
                include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/src/rendering/templates/partials/module/aliases_map.j2"
                )),
            ),
            (
                "partials/module/deps_map.j2",
                include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/src/rendering/templates/partials/module/deps_map.j2"
                )),
            ),
            (
                "partials/module/repo_git.j2",
                include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/src/rendering/templates/partials/module/repo_git.j2"
                )),
            ),
            (
                "partials/module/repo_http.j2",
                include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/src/rendering/templates/partials/module/repo_http.j2"
                )),
            ),
            (
                "partials/header.j2",
                include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/src/rendering/templates/partials/header.j2"
                )),
            ),
            (
                "module_bzl.j2",
                include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/src/rendering/templates/module_bzl.j2"
                )),
            ),
            (
                "vendor_module.j2",
                include_str!(concat!(
                    env!("CARGO_MANIFEST_DIR"),
                    "/src/rendering/templates/vendor_module.j2"
                )),
            ),
        ])
        .unwrap();

        tera.register_function(
            "crate_build_file",
            crate_build_file_fn_generator(render_config.build_file_template.clone()),
        );
        tera.register_function(
            "crate_label",
            crate_label_fn_generator(
                render_config.crate_label_template.clone(),
                render_config.repository_name.clone(),
            ),
        );
        tera.register_function(
            "crate_repository",
            crate_repository_fn_generator(
                render_config.crate_repository_template.clone(),
                render_config.repository_name.clone(),
            ),
        );
        tera.register_function(
            "crates_module_label",
            module_label_fn_generator(render_config.crates_module_template.clone()),
        );

        let mut context = tera::Context::new();
        context.insert("default_select_list", &SelectStringList::default());
        context.insert("repository_name", &render_config.repository_name);
        context.insert("vendor_mode", &render_config.vendor_mode);
        context.insert("regen_command", &render_config.regen_command);
        context.insert("Null", &tera::Value::Null);
        context.insert(
            "default_package_name",
            &match render_config.default_package_name.as_ref() {
                Some(pkg_name) => format!("\"{pkg_name}\""),
                None => "None".to_owned(),
            },
        );

        Self {
            engine: tera,
            context,
        }
    }

    fn new_tera_ctx(&self) -> tera::Context {
        self.context.clone()
    }

    pub fn render_header(&self) -> Result<String> {
        let context = self.new_tera_ctx();
        let mut header = self
            .engine
            .render("partials/header.j2", &context)
            .context("Failed to render header comment")?;
        header.push('\n');
        Ok(header)
    }

    pub fn render_module_bzl(&self, data: &Context) -> Result<String> {
        let mut context = self.new_tera_ctx();
        context.insert("context", data);

        self.engine
            .render("module_bzl.j2", &context)
            .context("Failed to render crates module")
    }

    pub fn render_vendor_module_file(&self, data: &Context) -> Result<String> {
        let mut context = self.new_tera_ctx();
        context.insert("context", data);

        self.engine
            .render("vendor_module.j2", &context)
            .context("Failed to render vendor module")
    }
}

/// A convienience wrapper for parsing parameters to tera functions
macro_rules! parse_tera_param {
    ($param:literal, $param_type:ty, $args:ident) => {
        match $args.get($param) {
            Some(val) => match from_value::<$param_type>(val.clone()) {
                Ok(v) => v,
                Err(_) => {
                    return Err(tera::Error::msg(format!(
                        "The `{}` paramater could not be parsed as a String.",
                        $param
                    )))
                }
            },
            None => {
                return Err(tera::Error::msg(format!(
                    "No `{}` parameter was passed.",
                    $param
                )))
            }
        }
    };
}

/// Convert a crate name into a module name by applying transforms to invalid characters.
fn crate_build_file_fn_generator(template: String) -> impl tera::Function {
    Box::new(
        move |args: &HashMap<String, Value>| -> tera::Result<Value> {
            let name = parse_tera_param!("name", String, args);
            let version = parse_tera_param!("version", String, args);

            match to_value(render_crate_build_file(&template, &name, &version)) {
                Ok(v) => Ok(v),
                Err(_) => Err(tera::Error::msg("Failed to generate crate's BUILD file")),
            }
        },
    )
}

/// Convert a file name to a Bazel label
fn module_label_fn_generator(template: String) -> impl tera::Function {
    Box::new(
        move |args: &HashMap<String, Value>| -> tera::Result<Value> {
            let file = parse_tera_param!("file", String, args);

            let label = match render_module_label(&template, &file) {
                Ok(v) => v,
                Err(e) => return Err(tera::Error::msg(e)),
            };

            match to_value(label.to_string()) {
                Ok(v) => Ok(v),
                Err(_) => Err(tera::Error::msg("Failed to generate crate's BUILD file")),
            }
        },
    )
}

/// Convert a crate name into a module name by applying transforms to invalid characters.
fn crate_label_fn_generator(template: String, repository_name: String) -> impl tera::Function {
    Box::new(
        move |args: &HashMap<String, Value>| -> tera::Result<Value> {
            let name = parse_tera_param!("name", String, args);
            let version = parse_tera_param!("version", String, args);
            let target = parse_tera_param!("target", String, args);

            match to_value(sanitize_repository_name(&render_crate_bazel_label(
                &template,
                &repository_name,
                &name,
                &version,
                &target,
            ))) {
                Ok(v) => Ok(v),
                Err(_) => Err(tera::Error::msg("Failed to generate crate's label")),
            }
        },
    )
}

/// Convert a crate name into a module name by applying transforms to invalid characters.
fn crate_repository_fn_generator(template: String, repository_name: String) -> impl tera::Function {
    Box::new(
        move |args: &HashMap<String, Value>| -> tera::Result<Value> {
            let name = parse_tera_param!("name", String, args);
            let version = parse_tera_param!("version", String, args);

            match to_value(sanitize_repository_name(&render_crate_bazel_repository(
                &template,
                &repository_name,
                &name,
                &version,
            ))) {
                Ok(v) => Ok(v),
                Err(_) => Err(tera::Error::msg("Failed to generate crate repository name")),
            }
        },
    )
}
