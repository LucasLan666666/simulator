#include <iostream>
#include <bits/types/FILE.h>
#include <time.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <set>
#include <csignal>
#include <chrono>

#if defined(GSIM)
#include <SimTop.h>
MOD_NAME* mod;
#endif

#if defined(VERILATOR)
#include "verilated.h"
#include HEADER
REF_NAME* ref;
#endif

#if defined(GSIM_DIFF)
#include <top_ref.h>
REF_NAME* ref;
#endif

#define MAX_PROGRAM_SIZE 0x8000000
uint8_t program[MAX_PROGRAM_SIZE];
int program_sz = 0;
bool dut_end = false;

template <typename T>
std::vector<size_t> sort_indexes(const std::vector<T> &v) {

  // initialize original index locations
  std::vector<size_t> idx(v.size());
  iota(idx.begin(), idx.end(), 0);

  // when v contains elements of equal values
  stable_sort(idx.begin(), idx.end(),
       [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

  return idx;
}

void load_program(char* filename){

  memset(&program, 0, sizeof(program));
  if(!filename){
    printf("No input program\n");
    return;
  }

  FILE* fp = fopen(filename, "rb");
  assert(fp);

  fseek(fp, 0, SEEK_END);
  program_sz = ftell(fp);
  assert(program_sz < MAX_PROGRAM_SIZE);

  fseek(fp, 0, SEEK_SET);
  int ret = fread(program, program_sz, 1, fp);
  assert(ret == 1);
  printf("load program size: 0x%x\n", program_sz);
  return;
}

#ifdef VERILATOR
void ref_cycle(int n) {
  while(n --){
    ref->clock = 0;
    ref->eval();
    ref->clock = 1;
    ref->eval();
  }
}
void ref_reset(){
    ref->reset = 1;
    for(int i = 0; i < 10; i++){
        ref_cycle(1);
    }
    ref->reset = 0;
}
#endif
#ifdef GSIM
void mod_reset() {
  mod->set_reset(1);
  for (int i = 0; i < 10; i ++)
    mod->step();
  mod->set_reset(0);
  mod->cycles = 0;
}
#endif
#ifdef GSIM_DIFF
void ref_reset() {
  ref->set_reset(1);
  for (int i = 0; i < 10; i ++)
    ref->step();
  ref->set_reset(0);
}
#endif

#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)
bool checkSig(bool display, REF_NAME* ref, MOD_NAME* mod);
bool checkSignals(bool display) {
  return checkSig(display, ref, mod);
}
#endif

int main(int argc, char** argv) {
  load_program(argv[1]);
#ifdef GSIM
  mod = new MOD_NAME();
  memcpy(&mod->memory$ram$rdata_mem$mem, program, program_sz);
  mod->set_difftest$$perfCtrl$$clean(0);
  mod->set_difftest$$perfCtrl$$dump(0);
  mod->set_difftest$$logCtrl$$begin(0);
  mod->set_difftest$$logCtrl$$end(0);
  mod->set_difftest$$logCtrl$$level(0);
  mod->set_difftest$$uart$$in$$ch(-1);
  mod_reset();
  mod->step();
#endif
#ifdef VERILATOR
  ref = new REF_NAME();
  ref->difftest_perfCtrl_clean = ref->difftest_perfCtrl_dump = 0;
  ref->difftest_uart_in_ch = -1;
  ref->difftest_uart_in_valid = 0;
  ref_reset();
  memcpy(&ref->rootp->SimTop__DOT__memory__DOT__ram__DOT__rdata_mem__DOT__mem, program, program_sz);
#endif
#ifdef GSIM_DIFF
  ref = new REF_NAME();
  memcpy(&ref->memory$ram$rdata_mem$mem, program, program_sz);
  ref->set_difftest$$perfCtrl$$clean(0);
  ref->set_difftest$$perfCtrl$$dump(0);
  ref->set_difftest$$logCtrl$$begin(0);
  ref->set_difftest$$logCtrl$$end(0);
  ref->set_difftest$$logCtrl$$level(0);
  ref->set_difftest$$uart$$in$$ch(-1);
  ref_reset();
  ref->step();
#endif
  std::cout << "start testing.....\n";
  // printf("size = %lx %lx\n", sizeof(*ref->rootp),
  // (uintptr_t)&(ref->rootp->SimTop__DOT__l_soc__DOT__misc__DOT__buffers__DOT__nodeOut_a_q__DOT__ram_opcode) - (uintptr_t)(ref->rootp));
  std::signal(SIGINT, [](int){
    dut_end = true;
  });
  std::signal(SIGTERM, [](int){
    dut_end = true;
  });
  uint64_t cycles = 0;
  size_t nodeNum = 0;
  for (size_t i = 0; i < sizeof(mod->nodeNum) / sizeof(mod->nodeNum[0]); i ++) {
    nodeNum += mod->nodeNum[i];
  }
  auto start = std::chrono::system_clock::now();
  auto prevTime = start;
  // clock_t prevTime = start;
#ifdef PERF
  FILE* activeFp = fopen(ACTIVE_FILE, "w");
#endif
  int max_cycles = 3100000;
  if (strcmp(DUTNAME, "xiangshan") == 0) max_cycles = 6400000;
  while(!dut_end) {
#ifdef VERILATOR
    ref_cycle(1);
  #ifndef GSIM
    if (ref->difftest_uart_out_valid) {
      printf("%c", ref->difftest_uart_out_ch);
      fflush(stdout);
    }
  #endif
#endif
#ifdef GSIM_DIFF
    ref->step();
#endif
    cycles ++;
    if(cycles % 100000 == 0 && cycles <= max_cycles) {
      auto now = std::chrono::system_clock::now();
      auto dur = now - start;
      fprintf(stderr, "cycles %ld (%ld ms, %ld per sec / current %ld ) \n", cycles,
            dur.count(), cycles * 1000 / dur.count(), 100000 * 1000 / (now - prevTime).count());
      prevTime = now;
#ifdef PERF
      fprintf(activeFp, "cycles: %ld\n", cycles);
      size_t totalActives = 0;
      size_t validActives = 0;
      size_t nodeActives = 0;
      for (size_t i = 0; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        totalActives += mod->activeTimes[i];
        validActives += mod->validActive[i];
        nodeActives += mod->nodeNum[i] * mod->activeTimes[i];
      }
      printf("nodeNum %ld totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld nodeActive %ld\n",
          nodeNum, totalActives, totalActives / cycles, validActives, validActives / cycles, nodeActives);
      fprintf(activeFp, "totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld\n",
          totalActives, totalActives / cycles, validActives, validActives / cycles);
      for (size_t i = 1; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        fprintf(activeFp, "%ld : activeTimes %ld validActive %ld\n", i, mod->activeTimes[i], mod->validActive[i]);
      }
      return 0;
      if (cycles == 500000) return 0;
#endif
      if (cycles == max_cycles) return 0;
    }
#if defined(GSIM)
    mod->step();
    if (mod->get_difftest$$uart$$out$$valid()) {
      printf("%c", mod->get_difftest$$uart$$out$$ch());
      fflush(stdout);
    }
    // dut_end = (mod->cpu$writeback$valid_r == 1) && (mod->cpu$writeback$inst_r == 0x6b);
#endif
#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)
    bool isDiff = checkSignals(false);
    if(isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      printf("Failed after %d cycles\nALL diffs: mode -- ref\n", cycles);
      checkSignals(false);
      return 0;
    }
#endif
    if(dut_end) {
    }
  }
}
