#ifndef NODE_H
#define NODE_H
#include "common.h"
enum {NODE_REG_SRC, NODE_REG_DST, NODE_ACTIVE, NODE_INP, NODE_OUT, NODE_MEMORY, NODE_READER, NODE_WRITER, NODE_MEMBER, NODE_OTHERS};
enum {NO_OP, OP_ADD, OP_SUB, OP_MUL, OP_DIV};
class Node {
public:
  Node() {
    defined = 0;
    type = NODE_OTHERS;
    visited = 0;
  }
  Node(int _type) {
    defined = 0;
    type = _type;
    visited = 0;
  }
  std::string name; // concat the module name in order (member in structure / temp variable)
  int id;   // unused
  int type;
  int width;
  int sign;
  std::vector<Node*> next;
  std::vector<std::string> insts;
  // std::vector<Node*> prev; // keep the order in sources file
  int inEdge; // for topo sort
  int defined;
  int initVal;
  Node* regNext;
  std::vector<Node*>member;
  int latency[2];
  int visited;
};
#endif