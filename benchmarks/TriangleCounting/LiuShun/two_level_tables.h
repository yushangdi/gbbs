#pragma once

#include <tuple>
#include "ligra/pbbslib/sparse_table.h"

// a nested hash table where the keys of two tables have the same type

namespace NestHash{

template <class K, class BV, class KeyHash>
class nested_table {
    using BT = std::tuple<K, BV>; // bottom value
    using V = sparse_table<K, BV, KeyHash>; // top value
    using T = std::tuple<K, V>; // top key value pair

    struct insertBottom {
        BT bottom_kv;
        insertBottom(BT _bottom_kv):bottom_kv(_bottom_kv){}
        operator ()(V& v0, T kv){
            v0.insert(bottom_kv);
        }
    };

    struct insertBottomS {
        BT bottom_kv;
        insertBottomS(BT _bottom_kv):bottom_kv(_bottom_kv){}
        operator ()(V& v0, T kv){
            v0.insert_seq(bottom_kv);
        }
    };

    template<class F>
    struct insertBottomF {
        BT bottom_kv;
        F f;
        insertBottomF(BT _bottom_kv, F _f):bottom_kv(_bottom_kv), f(_f){}
        operator ()(V& v0, T kv){
            v0.insert_f(bottom_kv, f);
        }
    };

    public:


        // sparse_table<K, BV, KeyHash>  bottom_table;
        sparse_table<K, V, KeyHash> top_table;
        T* table;
        K empty_key;
        static V empty_val = sparse_table<K, BV, KeyHash>();

        size_t size(){
            return top_table.size();
        }

        static void clearA(T* A, long n, T kv) {
            top_table.clearA(A, n, kv);
        }

        inline size_t hashToRange(size_t h) { return top_table.hashToRange(h); }
        inline size_t firstIndex(K& k) { return top_table.firstIndex(k); }
        inline size_t incrementIndex(size_t h) { return top_table.incrementIndex(h); }

        void del() {
           top_table.del();
        }

        void resize_no_copy(size_t incoming){
            top_table.resize_no_copy(incoming);
        }


        nested_table() {
            top_table = sparse_table();
            empty_key = top_table.empty_key;
            table = top_table.table;
        }

        // Size is the maximum number of values the hash table will hold.
        // Overfilling the table could put it into an infinite loop.
        nested_table(size_t _m, BT _empty, KeyHash _key_hash, long inp_space_mult=-1)
        {
            T top_empty = make_tuple( std::get<0>(_empty), _empty);
            top_table = sparse_table(_m, top_empty, _key_hash, inp_space_mult);
            table = top_table.table;
            empty_key = top_table.empty_key;
        }

        // Size is the maximum number of values the hash table will hold.
        // Overfilling the table could put it into an infinite loop.
        sparse_table(size_t _m, BT _empty, KeyHash _key_hash, T* _tab, bool clear=true)
        {
            T top_empty = make_tuple( std::get<0>(_empty), _empty);
            top_table = sparse_table(_m, top_empty, _key_hash, _tab, inp_space_mult);
            empty_key = top_table.empty_key;
            table = top_table.table;
        }

        // Pre-condition: k must be present in T.
        inline size_t idx(K k) {
           top_table.idx(k);
        }

        bool insert(std::tuple<K, V> kv){
            return top_table.insert(kv);
        }



        bool insert(K k, K k2, BV v) {
            dummy = make_tuple(k, (V)NULL);
            return top_table.insert_f<insertBottom>(dummy, insertBottom(make_tuple(k2,v)) );
        //     size_t h = firstIndex(k);
        //     while (true) {
        //     if (std::get<0>(table[h]) == empty_key) {
        //         if (pbbslib::CAS(&std::get<0>(table[h]), empty_key, k)) {
        // //          std::get<1>(table[h]) = std::get<1>(kv);
        //         std::get<1>(table[h]).insert(make_tuple(k2,v));
        //         return true;
        //         }
        //     }
        //     if (std::get<0>(table[h]) == k) {
        //         f(&std::get<1>(table[h]), kv);
        //         return false;
        //     }
        //     h = incrementIndex(h);
        //     }
        //     return false;
        }

        template <class F>
        bool insert_f(std::tuple<K, V> kv, const F& f) {
            return top_table.insert_f<F>(kv, f);
        }

        template <class F>
        bool insert_f(K k, K k2, BV v, const F& f) {
            dummy = make_tuple(k, (V)NULL);
            return top_table.insert_f<insertBottomF<F>><insertBottomF>(dummy, insertBottomF<F>(make_tuple(k2,v), f));
        //     size_t h = firstIndex(k);
        //     while (true) {
        //     if (std::get<0>(table[h]) == empty_key) {
        //         if (pbbslib::CAS(&std::get<0>(table[h]), empty_key, k)) {
        // //          std::get<1>(table[h]) = std::get<1>(kv);
        //         std::get<1>(table[h]).insert<F>(make_tuple(k2,v), f);
        //         return true;
        //         }
        //     }
        //     if (std::get<0>(table[h]) == k) {
        //         f(&std::get<1>(table[h]), kv);
        //         return false;
        //     }
        //     h = incrementIndex(h);
        //     }
        //     return false;
        }

        bool insert_seq(std::tuple<K, V> kv) {
            return top_table.insert_seq(kv);
        }

        bool insert_seq(K k, K k2, BV v) {
            dummy = make_tuple(k, (V)NULL);
            return top_table.insert_f<insertBottomS>(dummy, insertBottomS(make_tuple(k2,v)) );
        }        

        bool insert_check(std::tuple<K, V> kv, bool* abort) {
            return top_table.insert_check(kv, abort);
        }


        bool contains(K k) {
            return top_table.contains(k);
        }

        bool contains(K k, K k2) {
            top_table.find(k, empty_val).contains(k2);
            // if(top_table.contains(k)){
            //     top_table.find(k, (V)NULL).contains(k2);
            // } else {
            //     return false;
            // }
        }

        V find(K k, V default_value) {
            top_table.find(k, default_value);
        }

        BV find(K k, K k2, BV default_value) {
            top_table.find(k, empty_val).find(k2, default_value);
        }

        sequence<T> entries() {//  keys? just pointers?
            return top_table.entries();
        }

        void clear() {// memory leak?
            top_table.clear();
        }

};

}