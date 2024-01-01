/*
  graph class which describe the whole design graph
*/

#ifndef GRAPH_H
#define GRAPH_H

class graph {
  FILE* genHeaderStart(std::string headerFile);
  FILE* genSrcStart(std::string name);
  void genNodeDef(FILE* fp, Node* node);
  void genNodeInsts(FILE* fp, Node* node);
  void genNodeStep(FILE* fp, SuperNode* node, std::string& insts);
  void genInterfaceInput(FILE* fp, Node* input);
  void genInterfaceOutput(FILE* fp, Node* output);
  void genStep(FILE* fp);
  void genHeaderEnd(FILE* fp);
  void genSrcEnd(FILE* fp);
  void genNodeStepStart(FILE* fp, SuperNode* node);
  void genNodeStepEnd(FILE* fp, SuperNode* node);
 public:
  std::vector<Node*> allNodes;
  std::vector<Node*> input;
  std::vector<Node*> output;
  std::vector<Node*> regsrc;
  std::vector<Node*> sorted;
  std::vector<Node*> memory;
  std::vector<Node*> external;
  std::set<SuperNode*> supersrc;
  std::vector<SuperNode*> sortedSuper;
  std::string name;
  int maxTmp = 0;
  int nodeNum = 0;
  void addReg(Node* reg) {
    regsrc.push_back(reg);
  }
  void detectLoop();
  void topoSort();
  void instsGenerator();
  void cppEmitter();
  void usedBits();
  void traversal();
};

#endif
