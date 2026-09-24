#ifndef POKERI_STATE_H
#define POKERI_STATE_H
#ifdef POKERI_FREESTANDING
namespace pokeri {class State;}
#else
#include <array>
#include <deque>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
namespace pokeri {
// Platform-independent device-state encoding. Host I/O and CPU state stay outside.
class State {
public:
    std::vector<uint8_t> bytes;
    size_t cursor=0;
    bool reading=false;
    State()=default;
    explicit State(std::vector<uint8_t> data):bytes(std::move(data)),reading(true){}
    template<class T> typename std::enable_if<std::is_integral<T>::value && !std::is_same<T,bool>::value>::type value(T &v) {
        using U=typename std::make_unsigned<T>::type;
        if(reading){if(bytes.size()-cursor<sizeof(T))throw std::runtime_error("truncated state");U n=0;for(unsigned i=0;i<sizeof(T);++i)n|=U(bytes[cursor++])<<(8*i);v=T(n);}
        else for(unsigned i=0;i<sizeof(T);++i)bytes.push_back(uint8_t(U(v)>>(8*i)));
    }
    void value(bool &v){uint8_t b=v;value(b);if(b>1)throw std::runtime_error("invalid state boolean");v=b;}
    template<class T,size_t N> void value(std::array<T,N>& a){for(auto &v:a)value(v);}
    template<class T,size_t N> void value(T (&a)[N]){for(auto &v:a)value(v);}
    template<class T> void value(std::vector<T>& a){sequence(a);}
    template<class T> void value(std::deque<T>& a){sequence(a);}
    template<class... T> void fields(T&... v){int unused[]={0,(value(v),0)...};(void)unused;}
private:
    template<class T> void sequence(T &a){uint32_t n=a.size();value(n);if(n>4*1024*1024)throw std::runtime_error("excessive state sequence");if(reading)a.resize(n);for(auto &v:a)value(v);}
};
}
#endif // !POKERI_FREESTANDING
#endif
