// This code is part of the project "Theoretically Efficient Parallel Graph
// Algorithms Can Be Fast and Scalable", presented at Symposium on Parallelism
// in Algorithms and Architectures, 2018.
// Copyright (c) 2018 Laxman Dhulipala, Guy Blelloch, and Julian Shun
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all  copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once

#include <algorithm>
#include <cmath>
#include "pbbslib/sample_sort.h"
#include "pbbslib/monoid.h"
#include "ligra/pbbslib/sparse_table.h"
#include "ligra/ligra.h"

#include "benchmarks/DegeneracyOrder/BarenboimElkin08/DegeneracyOrder.h"
#include "benchmarks/DegeneracyOrder/GoodrichPszona11/DegeneracyOrder.h"
#include "benchmarks/KCore/JulienneDBS17/KCore.h"

using namespace std;

#define OLD_EDGE 0
#define NEW_INSERTION 1
#define NEW_DELETION 2

inline std::tuple<std::pair<uintE, uintE>, int8_t> newKV(uintE s, uintE d, int8_t v){
  return make_tuple(make_pair(s,d), v);
}

inline std::tuple<std::pair<uintE, uintE>, size_t> newKV(uintE s, uintE d, size_t v){
  return make_tuple(make_pair(s,d), v);
}

struct hash_pair {
  inline size_t operator () (const std::tuple<uintE, uintE>& t) {
    size_t l = std::min(std::get<0>(t), std::get<1>(t));
    size_t r = std::max(std::get<0>(t), std::get<1>(t));
    size_t key = (l << 32) + r;
    return pbbslib::hash64_2(key);
  }
};


template <class Graph, class E, class F>
struct BPDTriangleCountState {
  using K = std::pair<uintE, uintE>;
  using V = int8_t;
  using KV = std::tuple<K, V>;

  // initialize tables assuming state.D is already initialized
  // use D to determine the low/high of vertices
  struct updateTablesF {
    updateTablesF(() {}

    inline bool update(uintE s, uintE d) {
    // can add condition s < d and add both edges in one call
      add_to_tables(s,d,OLD_EDGE);
      return 1;
    }// when to return false?

    inline bool updateAtomic(uintE s, uintE d) {
      update(s,d);
      return 1;
    }

    inline bool cond(uintE d) { return cond_true(d); }
  };

  struct updateTF {
    operator ()(size_t& v0, std::tuple<K, size_t> kv){
      pbbslib::write_add(v0, std::get<1>(kv));
    }

  }

  Graph& G;
  size_t M;
  size_t t1;
  size_t t2;
  pbbs::sequence<uintE> D;
  E& HH; 
  E& HL; 
  E& LH; 
  E& LL; 
  F& T;

  BPDTriangleCountState(Graph& _G, E _HH, E _HL, E _LH, E _LL, F _T): 
    G(_G),
    // D(_D),
    HH(_HH),
    HL(_HL),
    LH(_LH),
    LL(_LL),
    T(_T){

    size_t n = G.n;
    M  = 2 * G.m + 1;
    t1 = sqrt(M) / 2;
    t2 = 3 * t1;


    // auto init_D_f = [&](const uintE& v) {
    //   D[v] = G.get_vertex(v).getOutDegree();
    // };
    auto frontier = pbbs::sequence<bool>(n, true);
    vertexSubset Frontier(n,n,frontier.to_array());
    // vertexMap(activeAndCts, init_D_f);

    D = pbbs::sequence<uintE>(n, [&] (size_t i) { return G.get_vertex(i).getOutDegree(); });
    edgeMap(G, Frontier, updateTablesF());
    // for(std::tuple<K, size_t> kv : HH.entries()){
    //   vhigh = std::get<0>(kv).first;
    //   vlow  = std::get<0>(kv).second;
    //   size_t deg = G.get_vertex(v_low).getOutDegree();
    //   // auto neigh_vlow = vertexSubset(deg, deg, G.get_vertex(v_low).getOutNeighbors());

    // }
  }


  ~BPDTriangleCountState(){
    // free(D); ??
    // HH.del();
    // HL.del();
    // LH.del();
    // LL.del();
    // T.del();
  }

  inline bool is_high(uintE s){
    return D[s] >= t2;
  }

  inline void add_to_tables(uintE s, uintE d, int8_t V){
    bool highS = is_high(s);
    bool highD = is_high(d);
    if( highS && highD){ //HH
      HH.insert(newKV(s,d,V));
    }else if(highS){ //HL
      HL.insert(newKV(s,d,V));
    }else if(highD){ //LH
      LH.insert(newKV(s,d,V));
    }else{ //LL
      LL.insert(newKV(s,d,V));
    }
  }

  inline void add_to_T(uintE s, uintE d, size_t V){
    T.insert_f<updateTF>(newKV(s,d,V), updateTF());
  }



};


// // initialize tables assuming state.D is already initialized
// // use D to determine the low/high of vertices
// template <class Graph, class E, class F>
// struct updateTablesF {
//   Graph& G;
//   BPDTriangleCountState<Graph, E, F>& state;
//   updateTablesF(Graph& G, BPDTriangleCountState<Graph, E, F>& _state) : G(G), state(_state) {}

//   inline bool update(uintE s, uintE d) {// (1,2) (2,1) will be called twice?
//   // can add condition s < d and add both edges in one call
//     bool highS = state.is_high(s);
//     bool highD = state.is_high(d);
//     if( highS && highD){ //HH
//       state.HH.insert(newKV(s,d,OLD_EDGE));
//     }else if(highS){ //HL
//       state.HL.insert(newKV(s,d,OLD_EDGE));
//     }else if(highD){ //LH
//       state.LH.insert(newKV(s,d,OLD_EDGE));
//     }else{ //LL
//       state.LL.insert(newKV(s,d,OLD_EDGE));
//     }
//     return 1;
//   }// when to return false?

//   inline bool updateAtomic(uintE s, uintE d) {
//     update(s,d);
//     return 1;
//   }

//   inline bool cond(uintE d) { return cond_true(d); }
// };


template <class Graph>
inline auto Initialize(Graph& G){
  using K = std::pair<uintE, uintE>;
  using V = int8_t;
  using KV = std::tuple<K, V>;
  // using W = typename Graph::weight_type;
  size_t n = G.n;
  KV empty = std::make_tuple(std::make_pair(UINT_E_MAX, UINT_E_MAX), -1);

  auto HH = sparse_table<K, V, hash_pair>(n, empty, hash_pair());
  auto HL = sparse_table<K, V, hash_pair>(n, empty, hash_pair());
  auto LH = sparse_table<K, V, hash_pair>(n, empty, hash_pair());
  auto LL = sparse_table<K, V, hash_pair>(n, empty, hash_pair());
  auto T  = sparse_table<K, size_t, hash_pair>(n, empty, hash_pair());
  // auto D  = sequence<size_t>(n);

  using E = decltype(HH);
  using F = decltype(T);
  
  auto state = BPDTriangleCountState<Graph, E, F>(G, HH, HL, LH, LL, T);

  return state;
}


template <class Graph, class F>
inline size_t Triangle(Graph& G, const F& f, commandLine& P) {
  auto C0 = P.getOptionIntValue("-c", 0);

  auto state = Initialize<Graph>(G);

  return C0;
}
