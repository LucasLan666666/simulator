#include "common.h"
#include "Node.h"
#include "graph.h"

#define INCLUDE_LIB(f, s) f << "#include <" << s << ">\n";
#define INCLUDE(f, s) f << "#include \"" << s << "\"\n";

const char* cmpOP[][2] = {{"s_mpz_lt", "<"}, {"s_mpz_leq", "<="}, {"s_mpz_gt", ">"}, {"s_mpz_geq", ">="}, {"s_mpz_eq", "=="}, {"s_mpz_neq", "!="}};

void topoSort(graph* g);

void genHeader(graph* g, std::string headerFile) {
  std::ofstream hfile(std::string(OBJ_DIR) + "/" + headerFile + ".h");

  hfile << "#ifndef " << headerFile << "_H\n";
  hfile << "#define " << headerFile << "\n";
  INCLUDE_LIB(hfile, "iostream");
  INCLUDE_LIB(hfile, "vector");
  INCLUDE_LIB(hfile, "gmp.h");
  INCLUDE(hfile, "functions.h");
  hfile << "class S" << g->name << "{\n" << "public:\n";
/*
  // ports
  for(Node* node: g->input) {
    hfile << "int " << node->name << ";\n";
  }
  for(Node* node: g->output) {
    hfile << "int " << node->name << ";\n";
  }
  // variables
  for(Node* node: g->sources) {
    hfile << "int " << node->name << ";\n";
  }
*/
// constructor
  hfile << "S" << g->name << "() {" << std::endl;
  for(Node* node: g->sorted) hfile << "mpz_init2(" << node->name << ", " << node->width << ");\n";
  hfile << "mpz_init(oldVal);\n";
  hfile << "}\n";

// active flags
  hfile << "std::vector<bool> activeFlags = " << "std::vector<bool>(" <<g->sorted.size() << ", true);\n";
// all sigs
  for (Node* node: g->sorted) {
    hfile << "mpz_t " << node->name << ";\n";
  }
// unique oldVal
    hfile << "mpz_t oldVal;\n";
// set functions
  for (Node* node: g->input) {
    hfile << "void set_" << node->name << "(mpz_t val) {\n";
    hfile <<"mpz_set(" << node->name << ", val);\n";
    for (Node* next: node->next)
      hfile << "activeFlags[" << next->id << "] = true;\n";
    hfile << "}\n";
  }
// step functions
  for (int i = 0; i < g->sorted.size(); i++) {
    hfile << "void step" << i << "();\n";
  }

// functions
  hfile << "void step();\n";
  hfile << "};\n";
  hfile << "#endif\n";
  hfile.close();
}

void genSrc(graph* g, std::string headerFile, std::string srcFile) {
  std::ofstream sfile(std::string(OBJ_DIR) + "/" + srcFile + ".cpp");
  INCLUDE(sfile, headerFile + ".h");

  // operations based on libgmp
  // for(int i = 0; i < LENGTH(cmpOP); i++) {
    // sfile << "void " << cmpOP[i][0] << "(mpz_t& dst, mpz_t& op1, mpz_t& op2) {\n";
    // sfile << "  mpz_set_ui(dst, mpz_cmp(op1, op2)" << cmpOP[i][1] << "0);\n}\n";
    // sfile << "void " << cmpOP[i][0] << "(mpz_t& dst, mpz_t& op1, unsigned long int op2) {\n";
    // sfile << "  mpz_set_ui(dst, mpz_cmp_ui(op1, op2)" << cmpOP[i][1] << "0);\n}\n";
  // }

  for(Node* node: g->sorted) {
    if(node->insts.size() == 0) continue;
    // generate function
    sfile << "void S" << g->name << "::step" << node->id << "() {\n";


    sfile << "activeFlags[" << node->id << "] = false;\n";
    sfile << "mpz_set(oldVal, " << node->name << ");\n";
    for(int i = 0; i < node->insts.size(); i ++) sfile << node->insts[i] << ";\n";
    Node* activeNode = node->type == NODE_REG_DST ? node->regNext : node;
    if(activeNode->next.size() > 0){
      sfile << "if(" << "mpz_cmp(oldVal," << node->name << ") != 0){\n";
      for(Node* next: activeNode->next) {
        sfile << "activeFlags[" << next->id << "] = true;\n";
      }
      sfile << "}\n";
    }
    // sfile << "std::cout << \"" << node->id  << ": " << node->name << ": \" << mpz_get_ui(oldVal) << " <<  "\"->\" << mpz_get_ui(" << node->name << ")<< \" \" <<(mpz_cmp(oldVal, " << node->name << ")!=0)<<std::endl;\n";
    sfile << "}\n";
  }

  sfile << "" <<"void S" << g->name << "::step() {\n";
  for(int i = 0; i < g->sorted.size(); i++) {
    if(g->sorted[i]->insts.size() == 0) continue;
    sfile << "if(activeFlags[" << i << "]) " << "step" << i << "();\n";
  }
  // for(Node* node: g->sorted) {
  //   if(node->op.length() == 0) continue;
  //   // generate function

  //   if(!node->defined) sfile << "int ";
  //   sfile << node->name << " = " << node->op << ";\n";
  // }

  // update registers
  for(Node* node: g->sources) {
    sfile << "mpz_set(" << node->name << ", " << node->name << "_next);\n";
  }
  // sfile << "std::cout << \"------\\n\";\n";
  sfile << "}";
}

void generator(graph* g, std::string header, std::string src) {
  topoSort(g);
  genHeader(g, header);
  genSrc(g, header, src);
}
