#ifndef POKERI_MUSASHI_HOOKS_H
#define POKERI_MUSASHI_HOOKS_H
#ifdef __cplusplus
extern "C" {
#endif
void pokeri_exception(unsigned vector);
#ifdef __cplusplus
}
#endif
#define POKERI_EXCEPTION_CALLBACK(vector) pokeri_exception(vector)
#endif
