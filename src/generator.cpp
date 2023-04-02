#include "common.h"
#include "Node.h"
#include "graph.h"

#define INCLUDE_LIB(f, s) f << "#include <" << s << ">\n";
#define INCLUDE(f, s) f << "#include \"" << s << "\"\n";

void topoSort(graph* g);

void genHeader(graph* g, std::string headerFile) {
  std::ofstream hfile(std::string(OBJ_DIR) + "/" + headerFile + ".h");

  hfile << "#ifndef " << headerFile << "_H\n";
  hfile << "#define " << headerFile << "\n";
  INCLUDE_LIB(hfile, "vector");
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
  hfile << "}\n";

// active flags
  hfile << "std::vector<bool> activeFlags = " << "std::vector<bool>(" <<g->sorted.size() << ", false);\n";
  for (Node* node: g->sorted) {
    hfile << "int " << node->name << ";\n";
  }
  for (int i = 0; i < g->sorted.size(); i++) {
    hfile << "void step" << i << "();\n";
  }
  // functions
  hfile << "void step();\n";
  hfile << "}\n";
  hfile << "#endif\n";
  hfile.close();
}

void genSrc(graph* g, std::string headerFile, std::string srcFile) {
  std::ofstream sfile(std::string(OBJ_DIR) + "/" + srcFile + ".cpp");
  INCLUDE(sfile, headerFile + ".h");
// func step
  for(Node* node: g->sorted) {
    if(node->op.length() == 0) continue;
    // generate function
    sfile << "void S" << g->name << "::step" << node->id << "() {\n";
    sfile << "activeFlags[" << node->id << "] = false;\n";
    sfile << "int oldVal = " << node->name << ";\n";
    sfile << node->name << " = " << node->op << ";\n";
    for(Node* next: node->next) {
      sfile << "if(" << "oldVal != " << node->name << ") activeFlags[" << next->id << "] = true;\n";
    }
    sfile << "}\n";
  }

  sfile << "" <<"void S" << g->name << "::step() {\n";
  for(int i = 0; i < g->sorted.size(); i++) {
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
    sfile << node->name << " = " << node->name << "_next;\n";
  }
  sfile << "}";
}

void generator(graph* g, std::string header, std::string src) {
  topoSort(g);
  genHeader(g, header);
  genSrc(g, header, src);
}
