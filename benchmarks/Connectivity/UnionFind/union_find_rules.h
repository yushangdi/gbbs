#pragma once

#include "ligra/bridge.h"
#include "pbbslib/seq.h"

#include <mutex>

namespace find_variants {
  inline uintE find_naive(uintE i, pbbs::sequence<uintE>& parent) {
    while(i != parent[i])
      i = parent[i];
    return i;
  }

  inline uintE find_compress(uintE i, pbbs::sequence<uintE>& parent) {
    uintE j = i;
    if (parent[j] == j) return j;
    do {
      j = parent[j];
    } while (parent[j] != j);
    uintE tmp;
    while ((tmp=parent[i])<j) {
      parent[i]=j; i=tmp;
    }
    return j;
  }

  inline uintE find_split(uintE i, pbbs::sequence<uintE>& parent) {
    while(1) {
      uintE v = parent[i];
      uintE w = parent[v];
      if(v == w) return v;
      else {
        pbbs::atomic_compare_and_swap(&parent[i],v,w);
        i = v;
      }
    }
  }

  inline uintE find_halve(uintE i, pbbs::sequence<uintE>& parent) {
    while(1) {
      uintE v = parent[i];
      uintE w = parent[v];
      if(v == w) return v;
      else {
        pbbs::atomic_compare_and_swap(&parent[i],v,w);
        //i = w;
        i = parent[i];
      }
    }
  }

  inline uintE find_a_halve(uintE i, pbbs::sequence<uintE>& parent) {
    while(1) {
      uintE v = parent[i];
      uintE w = parent[v];
      if(v == w) return v;
      else {
        pbbs::atomic_compare_and_swap(&parent[i],v,w);
        //i = w;
        i = parent[i];
      }
    }
  }


} // namespace find_variants

namespace unite_variants {

  template <class Find>
  struct Unite {
    Find& find;
    Unite(Find& find) : find(find) {}

    inline void operator()(uintE u_orig, uintE v_orig, pbbs::sequence<uintE>& parents, pbbs::sequence<uintE>& hooks) {
      uintE u = u_orig;
      uintE v = v_orig;
      while(1) {
        u = find(u,parents);
        v = find(v,parents);
        if(u == v) break;
        else if (u < v && parents[u] == u && pbbs::atomic_compare_and_swap(&parents[u],u,v)) {
          hooks[u] = v;
          return;
        }
        else if (v < u && parents[v] == v && pbbs::atomic_compare_and_swap(&parents[v],v,u)) {
          hooks[v] = u;
          return;
        }
      }
    }

    inline void operator()(uintE u_orig, uintE v_orig, pbbs::sequence<uintE>& parents) {
      uintE u = u_orig;
      uintE v = v_orig;
      while(1) {
        u = find(u,parents);
        v = find(v,parents);
        if(u == v) break;
        else if (u < v && parents[u] == u && pbbs::atomic_compare_and_swap(&parents[u],u,v)) {
          return;
        }
        else if (v < u && parents[v] == v && pbbs::atomic_compare_and_swap(&parents[v],v,u)) {
          return;
        }
      }
    }
  };

  template <class Find>
  struct UniteRem {
    Find& find;
    UniteRem(Find& find) : find(find) {}

    inline void operator()(uintE u_orig, uintE v_orig, pbbs::sequence<uintE>& parent, pbbs::sequence<uintE>& hooks, pbbs::sequence<std::mutex>& locks) {
      uintE rx = u_orig;
      uintE ry = v_orig;
      uintE z;
      while (parent[rx] != parent[ry]) {
        if (parent[rx] < parent[ry]) std::swap(rx,ry);
        if (rx == parent[rx]) {
          locks[rx].lock();
          bool success = false;
          if (rx == parent[rx]) {
    	parent[rx] = parent[ry];
    	success = true;
          }
          locks[rx].unlock();
          if (success) {
    	hooks[rx] = u_orig; // todo
    	return;
          }
        } else {
          z = parent[rx];
          parent[rx] = parent[ry];
          rx = z;
        }
      }
      return;
    }

    inline void operator()(uintE u_orig, uintE v_orig, pbbs::sequence<uintE>& parent, pbbs::sequence<std::mutex>& locks) {
      uintE rx = u_orig;
      uintE ry = v_orig;
      uintE z;
      while (parent[rx] != parent[ry]) {
        if (parent[rx] < parent[ry]) std::swap(rx,ry);
        if (rx == parent[rx]) {
          locks[rx].lock();
          if (rx == parent[rx]) {
    	parent[rx] = parent[ry];
          }
          locks[rx].unlock();
        } else {
          z = parent[rx];
          parent[rx] = parent[ry];
          rx = z;
        }
      }
      return;
    }
  };

  struct UniteEarly {
    inline void operator()(uintE u, uintE v, pbbs::sequence<uintE>& parents, pbbs::sequence<uintE>& hooks) {
      while(1) {
        if(u == v) return;
        if(v < u) std::swap(u,v);
        if(pbbs::atomic_compare_and_swap(&parents[u],u,v)) { hooks[u] = v; return;}
        uintE z = parents[u];
        uintE w = parents[z];
        pbbs::atomic_compare_and_swap(&parents[u],z,w);
        u = z;
      }
    }
  };
} // namespace unite_variants