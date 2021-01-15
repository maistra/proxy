use wasm_bindgen::prelude::*;

#[wasm_bindgen]
pub fn hello_world(i: i32) -> i32 {
    i * 2
}

fn main() {
    println!("Hello {}", hello_world(2));
}
