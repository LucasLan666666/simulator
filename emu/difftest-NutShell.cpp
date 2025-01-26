#include <iostream>
#include <time.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <csignal>

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
  mod->step();
  mod->set_reset(0);
}
#endif
#ifdef GSIM_DIFF
void ref_reset() {
  ref->set_reset(1);
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
  memcpy(&mod->mem$rdata_mem$mem, program, program_sz);
  mod->set_io_logCtrl_log_begin(0);
  mod->set_io_logCtrl_log_end(0);
  mod->set_io_uart_in_ch(-1);
  mod_reset();
  mod->step();
#endif
#ifdef VERILATOR
  ref = new REF_NAME();
  memcpy(&ref->rootp->SimTop__DOT__mem__DOT__rdata_mem__DOT__mem, program, program_sz);
  ref->io_logCtrl_log_begin = ref->io_logCtrl_log_begin = 0;
  ref->io_uart_in_ch = -1;
  ref_reset();
#endif
#ifdef GSIM_DIFF
  ref = new REF_NAME();
  memcpy(&ref->mem$rdata_mem$mem, program, program_sz);
  ref->set_io_uart_in_ch(-1);
  ref_reset();
  ref->step();
#endif
  std::cout << "start testing.....\n";
  // printf("size = %lx %lx\n", sizeof(*ref->rootp),
  // (uintptr_t)&(ref->rootp->SimTop__DOT__soc__DOT__nutcore__DOT__frontend__DOT__ifu__DOT__bp1__DOT__pht) - (uintptr_t)(ref->rootp));
#ifdef PERF
  FILE* activeFp = fopen(ACTIVE_FILE, "w");
#endif
  std::signal(SIGINT, [](int){
    dut_end = true;
  });
  std::signal(SIGTERM, [](int){
    dut_end = true;
  });
  uint64_t cycles = 0;
  clock_t start = clock();
  while(!dut_end) {
#ifdef VERILATOR
    ref_cycle(1);
  #ifndef GSIM
    if (ref->io_uart_out_valid) {
      printf("%c", ref->io_uart_out_ch);
      fflush(stdout);
    }
  #endif
#endif
#ifdef GSIM_DIFF
    ref->step();
#endif
    cycles ++;
    if(cycles % 10000000 == 0 && cycles <= 250000000) {
      clock_t dur = clock() - start;
      fprintf(stderr, "cycles %d (%d ms, %d per sec) \n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
#ifdef PERF
      size_t totalActives = 0;
      size_t validActives = 0;
      for (size_t i = 1; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        totalActives += mod->activeTimes[i];
        validActives += mod->validActive[i];
      }
      printf("totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld\n",
          totalActives, totalActives / cycles, validActives, validActives / cycles);
      fprintf(activeFp, "totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld\n",
          totalActives, totalActives / cycles, validActives, validActives / cycles);
      for (size_t i = 1; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        fprintf(activeFp, "%ld: activeTimes %ld validActive %ld\n", i, mod->activeTimes[i], mod->validActive[i]);
      }
      if (cycles == 50000000) return 0;
#endif
      if (cycles == 250000000) return 0;
    }
#if defined(GSIM)
    mod->step();
    if (mod->get_io_uart_out_valid()) {
      printf("%c", mod->get_io_uart_out_ch());
      fflush(stdout);
    }
    // dut_end = mod->soc$nutcore$_dataBuffer_T_ctrl_isNutCoreTrap;
#endif
#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)
    bool isDiff = checkSignals(false);
    if(isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      printf("Failed after %ld cucles\nALL diffs: mode -- ref\n", cycles);
      checkSignals(false);
      return 0;
    }
#endif
#if (defined (VERILATOR))
  // dut_end = ref->rootp->SimTop__DOT__soc__DOT__nutcore__DOT__dataBuffer_0_ctrl_isNutCoreTrap;
#endif
    if(dut_end) {
      clock_t dur = clock() - start;
    }
  }
  clock_t dur = clock() - start;
  printf("end cycles %d (%d ms, %d per sec) \n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
}