extern "C" {
/* LSAN compile-time suppression.
 * https://github.com/llvm/llvm-project/blob/6009708b4367171ccdbf4b5905cb6a803753fe18/compiler-rt/include/sanitizer/lsan_interface.h#L74-L76
 */
const char* __lsan_default_suppressions() {
    // Known issue of PAPI.
    return "leak:libpfm.so";
}
}
