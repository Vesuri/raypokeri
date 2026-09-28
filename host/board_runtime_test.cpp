// Build without the host C++ standard library headers to exercise the exact
// containers compiled by the Amiga toolchain. No game data.
#include "../src/platform/amiga/board-runtime/Support.h"
#include <stdio.h>
#include <stdlib.h>
extern "C" void pokeriRuntimeFault(const char *reason){fprintf(stderr,"runtime fault: %s\n",reason);abort();}
static void check(bool b){if(!b)pokeriRuntimeFault("test assertion");}
struct Tracked {
    static int alive;
    Tracked(){++alive;}Tracked(const Tracked&){++alive;}
    Tracked&operator=(const Tracked&)=default;
    ~Tracked(){--alive;}
};
int Tracked::alive=0;
int main(){
    std::array<unsigned,16> a{};a.fill(17);for(auto v:a)check(v==17);
    std::vector<unsigned> v;for(unsigned i=0;i<8192;++i)v.push_back(i);
    v.push_back(v[0]);check(v.back()==0);v.pop_back();auto copy=v;v.clear();check(copy.size()==8192);
    for(unsigned i=0;i<copy.size();++i)check(copy[i]==i);
    std::deque<std::vector<unsigned>> queue;queue.push_back({1,2,3});queue.push_back(copy);queue.pop_front();check(queue.front().back()==8191);queue.clear();check(queue.empty());
    std::deque<unsigned> ring;
    for(unsigned i=0;i<4096;++i)ring.push_back(i);
    for(unsigned i=0;i<4000;++i){check(ring.front()==i);ring.pop_front();}
    for(unsigned i=4096;i<12000;++i)ring.push_back(i);
    auto ringCopy=ring;ring.clear();unsigned serial=4000;
    for(auto value:ringCopy)check(value==serial++);
    check(serial==12000);ring=ringCopy;
    while(!ring.empty()){check(ring.front()==ringCopy.front());ring.pop_front();ringCopy.pop_front();}
    for(unsigned i=0;i<8;++i)ringCopy.push_back(i);
    ringCopy.push_back(ringCopy.front());check(ringCopy.back()==0);
    {
        std::deque<Tracked> retained;retained.push_back(Tracked());retained.pop_front();
        check(retained.empty() && Tracked::alive>0); // clear/pop retain storage
        std::deque<Tracked>().swap(retained);
        check(retained.empty() && Tracked::alive==0);
        retained.push_back(Tracked()); // released queue remains reusable
    }
    check(Tracked::alive==0); // later/static destruction is harmless
    std::deque<unsigned> wrapped,other;
    for(unsigned i=0;i<8;++i)wrapped.push_back(i);
    for(unsigned i=0;i<5;++i)wrapped.pop_front();
    for(unsigned i=8;i<12;++i)wrapped.push_back(i);
    other.push_back(99);wrapped.swap(other);
    check(wrapped.size()==1 && wrapped.front()==99 && other.size()==7);
    for(unsigned i=5;i<12;++i){check(other.front()==i);other.pop_front();}
    std::set<std::pair<int,int>> tree;
    for(int i=0;i<12000;++i)tree.insert(std::make_pair(i,i+1));
    for(int i=11999;i>=0;--i){check(tree.count(std::make_pair(i,i+1))==1);tree.insert(std::make_pair(i,i+1));}
    int expected=0;for(auto &p:tree){check(p.first==expected++);check(p.second==expected);}check(expected==12000);
    check(!tree.count(std::make_pair(-1,0)));
    std::vector<std::pair<int,int>> order;for(int i=12000;i>=0;--i)order.push_back(std::make_pair(i%137,i));std::sort(order.begin(),order.end());
    for(unsigned i=1;i<order.size();++i)check(!(order[i]<order[i-1]));
    puts("PASS freestanding arrays, vector growth/alias/copy, nested queues, wrapped swaps and storage release, AVL balancing/order and heap sort");
}
