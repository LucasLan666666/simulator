#include <iostream>
#include <top.h>
#include "verilated.h"
#include HEADER

MOD_NAME* mod;
REF_NAME* ref;

#define MAX_PROGRAM_SIZE 0x8000000
uint8_t program[MAX_PROGRAM_SIZE];
int program_sz = 0;

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
void mod_reset() {
  mpz_t val;
  mpz_init(val);
  mpz_set_ui(val, 1);
  mod->set_reset(val);
  mod->step();
  mpz_set_ui(val, 0);
  mod->set_reset(val);
}

bool checkSignals(bool display) {
  #include "../obj/checkSig.h"
}

int main(int argc, char** argv) {
  mod = new MOD_NAME();
  ref = new REF_NAME();
  ref_reset();
  mod_reset();
  load_program(argv[1]);
  memcpy(&ref->rootp->top__DOT__mem__DOT__ram, program, program_sz);
  memcpy(&mod->mem$ram, program, program_sz);
  mod->step();
  if(checkSignals(false)) {
    std::cout << "Diff after reset!\n";
    return 0;
  }
  std::cout << "start testing.....\n";
  for(int i = 1; i <= 10; i++) {
    ref_cycle(1);
    mod->step();

    bool isDiff = checkSignals(false);
    if(isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      std::cout << "Failed after " << i << " cycles\n";
      return 0;
    }
  }
}