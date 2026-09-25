#ifndef POKERI_NATIVE_HOOK_H
#define POKERI_NATIVE_HOOK_H
#include <stdint.h>
namespace pokeri {
// A saved original CPU context. No emulator or platform types cross this API.
struct Registers { uint32_t d[8],a[8],pc; uint16_t sr; };
enum class Ea : uint8_t { none,data,address,indirect,postincrement,predecrement,displacement,indexed,absolute_word,absolute_long,pc_displacement,pc_indexed,immediate };
enum class Operation : uint8_t { move,clear,test,compare,bit_test,bit_test_register,or_bits,and_bits };
struct Operand { Ea kind; int8_t reg,extension; };
struct Hook { uint32_t pc; uint8_t length,size; Operation operation; Operand source,dest; };
struct HookBus {
    virtual ~HookBus() {}
    virtual bool read(uint32_t address,unsigned size,uint32_t &value)=0;
    virtual bool write(uint32_t address,unsigned size,uint32_t value)=0;
};
// Immutable extension words are captured after relocation and byte validation.
// Execution still checks every dynamic data address through the supplied bus.
struct PreparedHook { Hook hook; uint32_t sourceExtension,destExtension; };
bool prepareHook(const Hook &,const uint8_t *instruction,PreparedHook &);
// Instruction extension reads and data transactions go through the checked bus.
// On failure the caller stops; it must not resume a partly executed instruction.
bool executeHook(const Hook &hook,Registers &r,HookBus &bus);
}
#endif
