// Program entry owns constructor/destructor order and preserves main's result.
// The shared gcc support _start is void and loses D0 across finalizers; keep
// that shared installation untouched and select this entry for Pokeri only.
using Initializer = void (*)();
extern "C" {
extern Initializer __preinit_array_start[] __attribute__((weak));
extern Initializer __preinit_array_end[] __attribute__((weak));
extern Initializer __init_array_start[] __attribute__((weak));
extern Initializer __init_array_end[] __attribute__((weak));
extern Initializer __fini_array_start[] __attribute__((weak));
extern Initializer __fini_array_end[] __attribute__((weak));
int main(int, char **);
__attribute__((used, section(".text.unlikely"))) int _start() {
    for (auto p=__preinit_array_start; p!=__preinit_array_end; ++p) (*p)();
    for (auto p=__init_array_start; p!=__init_array_end; ++p) (*p)();
    const int result=main(0, nullptr);
    for (auto p=__fini_array_end; p!=__fini_array_start;) (*--p)();
    return result;
}
}
