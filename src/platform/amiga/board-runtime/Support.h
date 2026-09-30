#ifndef POKERI_BOARD_RUNTIME_SUPPORT
#define POKERI_BOARD_RUNTIME_SUPPORT
#include <stddef.h>
#include <stdint.h>
#include <initializer_list>
// Small freestanding subset used by the shared board, not a general STL.
extern "C" void pokeriRuntimeFault(const char *reason);
namespace std {
template<class T> const T &min(const T&a,const T&b){return b<a?b:a;}
template<class T> const T &max(const T&a,const T&b){return a<b?b:a;}
template<class T> T abs(T a){return a<0?-a:a;}
template<class T> void swap(T&a,T&b){T t=a;a=b;b=t;}
template<class T,size_t N> struct array {
    T values[N];
    T& operator[](size_t i){return values[i];}const T&operator[](size_t i)const{return values[i];}
    T* data(){return values;}const T* data()const{return values;}
    T*begin(){return values;}T*end(){return values+N;}const T*begin()const{return values;}const T*end()const{return values+N;}
    constexpr size_t size()const{return N;}void fill(const T&v){for(auto &x:*this)x=v;}
};
template<class T> class vector {
    T*p=nullptr;size_t n=0,cap=0;
public:
    vector(){}explicit vector(size_t count,const T&v=T()){resize(count,v);}
    vector(initializer_list<T> a){for(const auto &v:a)push_back(v);}
    vector(const vector&a){for(const auto &v:a)push_back(v);}
    ~vector(){delete[] p;}
    vector&operator=(const vector&a){if(this!=&a){clear();reserve(a.n);for(const auto &v:a)push_back(v);}return *this;}
    void reserve(size_t count){if(count<=cap)return;T*q=new T[count];if(!q)pokeriRuntimeFault("board allocation failed");for(size_t i=0;i<n;++i)q[i]=p[i];delete[] p;p=q;cap=count;}
    void resize(size_t count,const T&v=T()){reserve(count);while(n<count)p[n++]=v;n=count;}
    void push_back(const T&v){if(n==cap){T copy=v;reserve(cap?cap*2:8);p[n++]=copy;}else p[n++]=v;}
    void swap(vector &other){std::swap(p,other.p);std::swap(n,other.n);std::swap(cap,other.cap);}
    void pop_back(){if(!n)pokeriRuntimeFault("empty vector");p[--n]=T();}
    void clear(){while(n)pop_back();}bool empty()const{return !n;}size_t size()const{return n;}
    T*data(){return p;}const T*data()const{return p;}T*begin(){return p;}T*end(){return p+n;}const T*begin()const{return p;}const T*end()const{return p+n;}
    T&operator[](size_t i){return p[i];}const T&operator[](size_t i)const{return p[i];}
    T&front(){return p[0];}const T&front()const{return p[0];}T&back(){return p[n-1];}const T&back()const{return p[n-1];}
};
template<class T> class deque {
    vector<T> storage;size_t head=0,n=0;
    void grow(){
        vector<T> grown(storage.empty()?8:storage.size()*2);
        for(size_t i=0;i<n;++i)grown[i]=(*this)[i];
        storage.swap(grown);head=0;
    }
public:
    deque(){}deque(const deque &other){for(const auto &v:other)push_back(v);}
    deque&operator=(const deque &other){if(this!=&other){clear();for(const auto &v:other)push_back(v);}return *this;}
    void swap(deque &other){storage.swap(other.storage);std::swap(head,other.head);std::swap(n,other.n);}
    bool empty()const{return !n;}size_t size()const{return n;}
    T&operator[](size_t i){return storage[(head+i)&(storage.size()-1)];}
    const T&operator[](size_t i)const{return storage[(head+i)&(storage.size()-1)];}
    T&front(){return (*this)[0];}const T&front()const{return (*this)[0];}
    T&back(){return (*this)[n-1];}const T&back()const{return (*this)[n-1];}
    void push_back(const T &v){if(n==storage.size()){T copy=v;grow();(*this)[n++]=copy;}else (*this)[n++]=v;}
    void push_front(const T &v){T copy=v;if(n==storage.size())grow();head=(head-1)&(storage.size()-1);++n;storage[head]=copy;}
    void pop_front(){if(!n)pokeriRuntimeFault("empty deque");storage[head]=T();head=(head+1)&(storage.size()-1);--n;}
    void clear(){while(n)pop_front();head=0;}
    struct iterator {
        const deque *owner;size_t index;
        const T&operator*()const{return (*owner)[index];}
        iterator&operator++(){++index;return *this;}
        bool operator!=(const iterator &other)const{return owner!=other.owner || index!=other.index;}
    };
    iterator begin()const{return {this,0};}iterator end()const{return {this,n};}
};
template<class A,class B> struct pair {A first;B second;bool operator<(const pair&o)const{return first<o.first || (!(o.first<first) && second<o.second);} };
template<class A,class B> pair<A,B> make_pair(A a,B b){return {a,b};}
template<class It,class Less> void sort(It first,It last,Less less){
    // In-place heap sort; deterministic bounds, no stack proportional to input.
    auto down=[&](size_t root,size_t count){while(root<count/2){size_t child=2*root+1;if(child+1<count && less(first[child],first[child+1]))++child;if(!(less(first[root],first[child])))break;swap(first[root],first[child]);root=child;}};
    if(first==last)return;
    size_t n=last-first;for(size_t i=n/2;i;i--)down(i-1,n);while(n>1){swap(first[0],first[--n]);down(0,n);}
}
template<class It> void sort(It first,It last){sort(first,last,[](const auto &a,const auto &b){return a<b;});}
// AVL tree keeps PAINT visitation and curve outlines bounded by log(size).
template<class T> class set {
    struct Node {T value;Node*l=nullptr,*r=nullptr;unsigned h=1;explicit Node(const T&v):value(v){}};Node*root=nullptr;
    static unsigned height(Node*n){return n?n->h:0;}static void update(Node*n){n->h=1+max(height(n->l),height(n->r));}
    static Node*right(Node*n){Node*p=n->l;n->l=p->r;p->r=n;update(n);update(p);return p;}
    static Node*left(Node*n){Node*p=n->r;n->r=p->l;p->l=n;update(n);update(p);return p;}
    static Node*add(Node*n,const T&v){if(!n){n=new Node(v);if(!n)pokeriRuntimeFault("set allocation failed");return n;}if(v<n->value)n->l=add(n->l,v);else if(n->value<v)n->r=add(n->r,v);else return n;update(n);
        if(height(n->l)>height(n->r)+1){if(n->l->value<v)n->l=left(n->l);return right(n);}if(height(n->r)>height(n->l)+1){if(v<n->r->value)n->r=right(n->r);return left(n);}return n;}
    static void destroy(Node*n){if(n){destroy(n->l);destroy(n->r);delete n;}}
public:
    set(){}set(const set&)=delete;set&operator=(const set&)=delete;
    ~set(){destroy(root);}void insert(const T&v){root=add(root,v);}unsigned count(const T&v)const{Node*n=root;while(n){if(v<n->value)n=n->l;else if(n->value<v)n=n->r;else return 1;}return 0;}
    struct iterator {Node*path[64];unsigned depth=0;void push(Node*n){while(n){if(depth==64)pokeriRuntimeFault("set depth");path[depth++]=n;n=n->l;}}const T&operator*()const{return path[depth-1]->value;}iterator&operator++(){Node*n=path[--depth];push(n->r);return *this;}bool operator!=(const iterator&o)const{return depth!=o.depth || (depth && path[depth-1]!=o.path[o.depth-1]);}};
    iterator begin()const{iterator i;i.push(root);return i;}iterator end()const{return iterator();}
};
}
#endif
