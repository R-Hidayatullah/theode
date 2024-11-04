fn main() {
    cc::Build::new()
        .file("dat_decompress.c")
        .compile("dat_decompress");
}
