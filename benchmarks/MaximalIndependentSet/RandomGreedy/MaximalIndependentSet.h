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

#include "pbbslib/random_shuffle.h"

#include "ligra/speculative_for.h"
#include "ligra/ligra.h"
#include "ligra/pbbslib/sparse_table.h"

namespace MaximalIndependentSet_rootset {

template <class Graph, class Fl>
inline void verify_mis(Graph& G, Fl& in_mis) {
  using W = typename Graph::weight_type;
  auto d = sequence<uintE>(G.n, (uintE)0);
  auto map_f = [&](const uintE& src, const uintE& ngh, const W& wgh) {
    if (!d[ngh]) {
      d[ngh] = 1;
    }
  };
  par_for(0, G.n, [&] (size_t i) {
    if (in_mis[i]) {
      G.get_vertex(i).mapOutNgh(i, map_f);
    }
  });
  par_for(0, G.n, [&] (size_t i) {
    if (in_mis[i]) {
      assert(!d[i]);
    }
  });
  auto mis_f = [&](size_t i) { return (size_t)in_mis[i]; };
  auto mis_int =
      pbbslib::make_sequence<size_t>(G.n, mis_f);
  size_t mis_size = pbbslib::reduce_add(mis_int);
  if (pbbslib::reduce_add(d) != (G.n - mis_size)) {
    std::cout << "MaximalIndependentSet incorrect"
              << "\n";
    assert(false);
  }
  std::cout << "MaximalIndependentSet Ok"
            << "\n";
}

template <class Graph, class VS, class P>
inline vertexSubset get_nghs(Graph& G, VS& vs, P p) {
  using W = typename Graph::weight_type;
  vs.toSparse();
  assert(!vs.isDense);
  auto deg_f =  [&](size_t i) { return G.get_vertex(vs.vtx(i)).getOutDegree(); };
  auto deg_im = pbbslib::make_sequence<size_t>(
      vs.size(), deg_f);
  size_t sum_d = pbbslib::reduce_add(deg_im);

  if (sum_d > G.m / 100) {  // dense forward case
    auto dense = sequence<bool>(G.n, false);
    auto map_f = [&](const uintE& src, const uintE& ngh, const W& wgh) {
      if (p(ngh) && !dense[ngh]) {
        dense[ngh] = 1;
      }
    };
    par_for(0, vs.size(), [&] (size_t i) {
      uintE v = vs.vtx(i);
      G.get_vertex(v).mapOutNgh(v, map_f);
    });
    return vertexSubset(G.n, dense.to_array());
  } else {  // sparse --- iterate, and add nghs satisfying P to a hashtable
    debug(std::cout << "sum_d = " << sum_d << std::endl;);
    auto ht = make_sparse_table<uintE, pbbslib::empty>(
        sum_d, std::make_tuple(UINT_E_MAX, pbbslib::empty()),
        [&](const uintE& k) { return pbbslib::hash64(k); });
    vs.toSparse();
    par_for(0, vs.size(), [&] (size_t i) {
      auto map_f = [&](const uintE& src, const uintE& ngh, const W& wgh) {
        if (p(ngh)) {
          ht.insert(std::make_tuple(ngh, pbbslib::empty()));
        }
      };
      uintE v = vs.vtx(i);
      G.get_vertex(v).mapOutNgh(v, map_f);
    });
    auto nghs = ht.entries();
    ht.del();
    size_t nghs_size = nghs.size();
    return vertexSubset(G.n, nghs_size, (uintE*)nghs.to_array());
  }
}

inline bool hash_lt(const uintE& src, const uintE& ngh) {
  uint32_t src_h = pbbslib::hash32(src);
  uint32_t ngh_h = pbbslib::hash32(ngh);
  return (src_h < ngh_h) || ((src_h == ngh_h) && src < ngh);
};

template <class W>
struct mis_f {
  intE* p;
  uintE* perm;
  mis_f(intE* _p, uintE* _perm) : p(_p), perm(_perm) {}
  inline bool update(const uintE& s, const uintE& d, const W& w) {
    if (perm[s] < perm[d]) {
      p[d]--;
      return p[d] == 0;
    }
    return false;
  }
  inline bool updateAtomic(const uintE& s, const uintE& d, const W& wgh) {
    if (perm[s] < perm[d]) {
      return (pbbslib::fetch_and_add(&p[d], -1) == 1);
    }
    return false;
  }
  inline bool cond(uintE d) { return (p[d] > 0); }
};

template <class W>
struct mis_f_2 {
  intE* p;
  mis_f_2(intE* _p) : p(_p) {}
  inline bool update(const uintE& s, const uintE& d, const W& w) {
    if (hash_lt(s, d)) {
      p[d]--;
      return p[d] == 0;
    }
    return false;
  }
  inline bool updateAtomic(const uintE& s, const uintE& d, const W& wgh) {
    if (hash_lt(s, d)) {
      return (pbbslib::fetch_and_add(&p[d], -1) == 0);
    }
    return false;
  }
  inline bool cond(uintE d) { return (p[d] > 0); }  // still live
};

template <class Graph>
inline sequence<bool> MaximalIndependentSet(Graph& G) {
  using W = typename Graph::weight_type;
  timer init_t;
  init_t.start();
  size_t n = G.n;

  // compute the priority DAG
  auto priorities = sequence<intE>(n);
  auto perm = pbbslib::random_permutation<uintE>(n);
  par_for(0, n, 1, [&] (size_t i) {
    uintE our_pri = perm[i];
    auto count_f = [&](uintE src, uintE ngh, const W& wgh) {
      uintE ngh_pri = perm[ngh];
      return ngh_pri < our_pri;
    };
    priorities[i] = G.get_vertex(i).countOutNgh(i, count_f);
  });
  init_t.stop();
  debug(init_t.reportTotal("init"););


  // compute the initial rootset
  auto zero_f = [&](size_t i) { return priorities[i] == 0; };
  auto zero_map =
      pbbslib::make_sequence<bool>(n, zero_f);
  auto init = pbbslib::pack_index<uintE>(zero_map);
  size_t init_size = init.size();
  auto roots = vertexSubset(n, init_size, init.to_array());

  auto in_mis = sequence<bool>(n, false);
  size_t finished = 0;
  size_t rounds = 0;
  while (finished != n) {
    assert(roots.size() > 0);
    debug(std::cout << "round = " << rounds << " size = " << roots.size()
              << " remaining = " << (n - finished) << "\n";);

    // set the roots in the MaximalIndependentSet
    vertexMap(roots, [&](uintE v) { in_mis[v] = true; });

    // compute neighbors of roots that are still live
    auto removed = get_nghs(
        G, roots, [&](const uintE& ngh) { return priorities[ngh] > 0; });
    vertexMap(removed, [&](uintE v) { priorities[v] = 0; });

    // compute the new roots: neighbors of removed that have their priorities
    // set to 0 after eliminating all nodes in removed
    intE* pri = priorities.begin();
    timer nr;
    nr.start();
    auto new_roots =
        edgeMap(G, removed, mis_f<W>(pri, perm.begin()), -1, sparse_blocked);
    nr.stop();
    nr.reportTotal("new roots time");

    // update finished with roots and removed. update roots.
    finished += roots.size();
    finished += removed.size();
    removed.del();
    roots.del();

    roots = new_roots;
    rounds++;
  }
  return in_mis;
}
}  // namespace MaximalIndependentSet_rootset

namespace MaximalIndependentSet_spec_for {
// For each vertex:
//   Flags = 0 indicates undecided
//   Flags = 1 indicates chosen
//   Flags = 2 indicates a neighbor is chosen
template <class Graph>
struct MaximalIndependentSetstep {
  using W = typename Graph::weight_type;
  char* FlagsNext;
  char* Flags;
  Graph& G;

  MaximalIndependentSetstep(char* _PF, char* _F, Graph& _G)
      : FlagsNext(_PF), Flags(_F), G(_G) {}

  bool reserve(intT i) {
    // decode neighbor
    FlagsNext[i] = 1;

    auto map_f = [&](const uintE& src, const uintE& ngh,
                     const W& wgh) -> std::tuple<int, int> {
      if (ngh < src) {
        auto fl = Flags[ngh];
        return std::make_tuple(fl == 1, fl == 0);
      }
      return std::make_tuple(0, 0);
    };
    auto red_f = [&](const std::tuple<int, int>& l,
                     const std::tuple<int, int>& r) {
      return std::make_tuple(std::get<0>(l) + std::get<0>(r),
                             std::get<1>(l) + std::get<1>(r));
    };
    using E = std::tuple<int, int>;
    auto id = std::make_tuple(0, 0);
    auto monoid = pbbslib::make_monoid(red_f, id);
    auto res = G.get_vertex(i).template reduceOutNgh<E>(i, map_f, monoid);
    if (std::get<0>(res) > 0) {
      FlagsNext[i] = 2;
    } else if (std::get<1>(res) > 0) {
      FlagsNext[i] = 0;
    }
    return 1;
  }

  bool commit(intT i) { return (Flags[i] = FlagsNext[i]) > 0; }
};

template <class Graph>
inline sequence<char> MaximalIndependentSet(Graph& G) {
  size_t n = G.n;
  auto Flags = sequence<char>(n, [&](size_t i) { return 0; });
  auto FlagsNext = sequence<char>(n);
  auto mis = MaximalIndependentSetstep<Graph>(FlagsNext.begin(), Flags.begin(), G);
  eff_for<uintE>(mis, 0, n, 50);
  return Flags;
}
};  // namespace MaximalIndependentSet_spec_for

template <class Graph, class Seq>
inline void verify_MaximalIndependentSet(Graph& G, Seq& mis) {
  using W = typename Graph::weight_type;
  size_t n = G.n;
  auto ok = sequence<bool>(n, [&](size_t i) { return 1; });
  par_for(0, n, [&] (size_t i) {
    auto pred = [&](const uintE& src, const uintE& ngh, const W& wgh) {
      return mis[ngh];
    };
    size_t ct = G.get_vertex(i).countOutNgh(i, pred);
    ok[i] = (mis[i]) ? (ct == 0) : (ct > 0);
  });
  auto ok_f = [&](size_t i) { return ok[i]; };
  auto ok_imap = pbbslib::make_sequence<size_t>(n, ok_f);
  size_t n_ok = pbbslib::reduce_add(ok_imap);
  if (n_ok == n) {
    std::cout << "valid MaximalIndependentSet"
              << "\n";
  } else {
    std::cout << "invalid MaximalIndependentSet, " << (n - n_ok)
              << " vertices saw bad neighborhoods"
              << "\n";
  }
}
