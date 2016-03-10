/* COPYRIGHT (c) 2015 Sam Westrick, Laxman Dhulipala, Umut Acar,
 * Arthur Chargueraud, and Michael Rainey.
 * All rights reserved.
 *
 * \file pwsa.cpp
 */
#include "benchmark.hpp"
#include "pwsa.hpp"
#include "bin_heap.hpp"
#include "weighted_graph.hpp"

int main(int argc, char** argv) {
  int split_cutoff; // (K)
  int poll_cutoff; // (D)
  std::string fname;
  int srow;
  int scol;
  int drow;
  int dcol;
  double w;
  double exptime;
  bool pathcorrect;
  double opt;
  std::string visualize;

  Graph G;
  int src;
  int dst;

  std::atomic<int>* res_normal;
  std::atomic<vertpack>* res_pathcorrect;
  std::atomic<bool>* expanded_pathcorrect;

  state* res_pathcorrect_states;
  state_simple* res_states;

  int* pebbles = nullptr;
  int* predecessors = nullptr;

  int important = 0;

  auto init = [&] {
    split_cutoff = (int)pasl::util::cmdline::parse_or_default_int("K", 8);
    poll_cutoff = (int)pasl::util::cmdline::parse_or_default_int("D", 1);
    fname = pasl::util::cmdline::parse_or_default_string("map", "maps/simple_map.map");
    srow = (int)pasl::util::cmdline::parse_or_default_int("sr", 1);
    scol = (int)pasl::util::cmdline::parse_or_default_int("sc", 1);
    drow = (int)pasl::util::cmdline::parse_or_default_int("dr", 1);
    dcol = (int)pasl::util::cmdline::parse_or_default_int("dc", 1);
    w = (double)pasl::util::cmdline::parse_or_default_float("w", 1.0);
    exptime = (double)pasl::util::cmdline::parse_or_default_float("exptime", 0.0);
    pathcorrect = pasl::util::cmdline::parse_or_default_bool("pathcorrect", false);
    opt = (double)pasl::util::cmdline::parse_or_default_float("opt", 1.0);
//    checkdev = pasl::util::cmdline::parse_or_default_bool("checkdev", false);
//    debug = pasl::util::cmdline::parse_or_default_bool("debug", false);
    visualize = pasl::util::cmdline::parse_or_default_string("visualize", "");

    G = Graph(fname.c_str());

    src = G.vertex_at(srow, scol);
    dst = G.vertex_at(drow, dcol);

    if (!visualize.empty()) {
      pebbles = pasl::data::mynew_array<int>(G.number_vertices());
      for (int i = 0; i < G.number_vertices(); i++) {
        pebbles[i] = -1;
      }
    }

    if (!visualize.empty() || pathcorrect) {
      predecessors = pasl::data::mynew_array<int>(G.number_vertices());
      for (int i = 0; i < G.number_vertices(); i++) {
        predecessors[i] = -1;
      }
    }
  };

  auto run = [&] (bool sequential) {
    auto heuristic = [&] (int v) {
      return G.weighted_euclidean(w, v, dst);
    };

    if (pathcorrect) {
      // auto pair = pwsa_pathcorrect<Graph, Heap<int>, decltype(heuristic)>(G, heuristic, src, dst, split_cutoff, poll_cutoff, exptime, pebbles);
      // res_pathcorrect = pair.first;
      // expanded_pathcorrect = pair.second;
      res_pathcorrect_states = pwsa_pathcorrect_locality<Graph, Heap<int>, decltype(heuristic)>(G, heuristic, src, dst, split_cutoff, poll_cutoff, exptime, important, pebbles);
    }
    else {
      //res_normal = pwsa<Graph, Heap<std::tuple<int,int,int>>, decltype(heuristic)>(G, heuristic, src, dst, split_cutoff, poll_cutoff, exptime, pebbles, predecessors);
      //res_normal = pwsa<Graph, Heap<std::tuple<int,int/*,int*/>>, decltype(heuristic)>(G, heuristic, src, dst, split_cutoff, poll_cutoff, exptime, pebbles, predecessors);
      res_states = pwsa_locality<Graph, Heap<int>, decltype(heuristic)>(G, heuristic, src, dst, split_cutoff, poll_cutoff, exptime, important, pebbles);
    }
  };

  auto output = [&] {
    int* res = pasl::data::mynew_array<int>(G.number_vertices());
    int num_expanded = 0;
    for (int i = 0; i < G.number_vertices(); i++) {
      if (!pathcorrect) {
        res[i] = res_states[i].g;
        if (res_states[i].is_expanded.load()) num_expanded++;
        // res[i] = res_normal[i].load();
        // if (res[i] != -1) num_expanded++;
      }
      else {
        res[i] = res_pathcorrect_states[i].gpred.load().distance;
        if (predecessors) {
          predecessors[i] = res_pathcorrect_states[i].gpred.load().pred;
        }
        if (res_pathcorrect_states[i].is_expanded.load()) {
          num_expanded++;
        }
        // res[i] = res_pathcorrect[i].load().distance;
        // if (predecessors) {
        //   predecessors[i] = res_pathcorrect[i].load().pred;
        // }
        // if (expanded_pathcorrect[i].load()) {
        //   num_expanded++;
        // }
      }
    }
    // if (predecessors) {
    //   int i = dst;
    //   std::cout << "source is " << src << std::endl;
    //   while (i != predecessors[i] && 0 <= i && i < G.number_vertices()) {
    //     std::cout << i << "," << std::flush;
    //     i = predecessors[i];
    //   }
    //   std::cout << i << std::endl;
    // }
    double pathlen = double(res[dst])/10000.0;
    if (pathcorrect) {
      pathlen = double(G.pathlen(predecessors, src, dst)) / 10000.0;
    }
    // else {
    //   double bypath = double(G.pathlen(predecessors, src, dst)) / 10000.0;
    //   if (pathlen != bypath) {
    //     std::cout << pathlen << ", " << bypath << std::endl;
    //   }
    // }
    std::cout << "expanded " << num_expanded << std::endl;
    std::cout << "pathlen " << pathlen << std::endl;
    std::cout << "deviation " << pathlen / opt << std::endl;
    std::cout << "important " << important << std::endl;

    if (pebbles && predecessors) {
      G.pebble_dump(pebbles, predecessors, src, dst, visualize.c_str());
    }

  };

  auto destroy = [&] {;};

  pasl::sched::launch(argc, argv, init, run, output, destroy);
  return 0;
}