#[test]
fn run() {
    let path = env!("CARGO_BIN_EXE_hello-world");
    let output = std::process::Command::new(path).output().expect("Failed to run process");
    assert_eq!(&b"Hello world\n"[..], output.stdout.as_slice());
}
