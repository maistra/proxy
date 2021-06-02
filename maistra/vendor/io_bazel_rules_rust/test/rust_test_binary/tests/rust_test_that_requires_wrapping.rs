use std::env;

#[test]
pub fn rust_test_that_requires_wrapping() {
    let actual = format!(
        "This test requires {} at runtime.",
        env::var("USER_DEFINED_KEY").unwrap()
    );
    let expected = "This test requires USER_DEFINED_VALUE at runtime.";
    assert_eq!(actual, expected);
}
