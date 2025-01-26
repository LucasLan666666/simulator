/*
  traverse AST / graph
*/

#include "common.h"
#include <tuple>
#include <stack>
#include <map>

const char* pname[] = {
  "P_EMPTY", "P_CIRCUIT", "P_MOD", "P_EXTMOD", "P_INTMOD", "P_PORTS", "P_INPUT",
  "P_OUTPUT", "P_WIRE_DEF", "P_REG_DEF", "P_INST", "P_NODE", "P_CONNECT", 
  "P_PAR_CONNECT", "P_WHEN", "P_MEMORY", "P_READER", "P_WRITER", 
  "P_READWRITER", "P_RUW", "P_RLATENCT", "P_WLATENCT", "P_DATATYPE", "P_DEPTH", 
  "P_REF", "P_REF_DOT", "P_REF_IDX_INT", "P_REF_IDX_EXPR", "P_2EXPR", "P_1EXPR", 
  "P_1EXPR1INT", "P_1EXPR2INT", "P_FIELD", "P_FLIP_FIELD", "P_AG_TYPE", 
  "P_AG_FIELDS", "P_Clock", "P_INT_TYPE", "P_EXPR_INT_NOINIT", "P_EXPR_INT_INIT", 
  "P_EXPR_MUX", "P_STATEMENTS", "P_PRINTF", "P_EXPRS", "P_ASSERT", "P_INDEX", 
  "P_CONS_INDEX", "P_L_CONS_INDEX", "P_L_INDEX",
};
/* Preorder traversal of AST */
void preorder_traversal(PNode* root) {
  std::stack<std::pair<PNode*, int>> s;
  s.push(std::make_pair(root, 0));
  PNode* node;
  int depth;
  while (!s.empty()) {
    std::tie(node, depth) = s.top();
    s.pop();
    printf("%s", std::string(depth * 2, ' ').c_str());
    if(!node) {
      std::cout << "NULL\n";
      continue;
    }
    printf("%s (lineno %d, width %d): %s", pname[node->type], node->lineno, node->width, node->name.c_str());
    for (int i = node->getChildNum() - 1; i >= 0; i--) {
      s.push(std::make_pair(node->getChild(i), depth + 1));
    }
  }
  fflush(stdout);
}

static std::map<OPType, const char*> OP2Name = {
  {OP_INVALID, "invalid"}, {OP_MUX, "mux"}, {OP_ADD, "add"}, {OP_SUB, "sub"}, {OP_MUL, "mul"},
  {OP_DIV, "div"}, {OP_REM, "rem"}, {OP_LT, "lt"}, {OP_LEQ, "leq"}, {OP_GT, "gt"}, {OP_GEQ, "geq"},
  {OP_EQ, "eq"}, {OP_NEQ, "neq"}, {OP_DSHL, "dshl"}, {OP_DSHR, "dshr"}, {OP_AND, "and"},
  {OP_OR, "or"}, {OP_XOR, "xor"}, {OP_CAT, "cat"}, {OP_ASUINT, "asuint"}, {OP_ASSINT, "assint"},
  {OP_ASCLOCK, "asclock"}, {OP_ASASYNCRESET, "asasyncreset"}, {OP_CVT, "cvt"}, {OP_NEG, "neg"},
  {OP_NOT, "not"}, {OP_ANDR, "andr"}, {OP_ORR, "orr"}, {OP_XORR, "xorr"}, {OP_PAD, "pad"}, {OP_SHL, "shl"},
  {OP_SHR, "shr"}, {OP_HEAD, "head"}, {OP_TAIL, "tail"}, {OP_BITS, "bits"}, {OP_INDEX_INT, "index_int"},
  {OP_INDEX, "index"}, {OP_WHEN, "when"}, {OP_PRINTF, "printf"}, {OP_ASSERT, "assert"}, {OP_INT, "int"},
  {OP_READ_MEM, "readMem"}, {OP_RESET, "reset"}, {OP_STMT, "stmts"}, {OP_SEXT, "sext"}, {OP_BITS_NOSHIFT, "bits_noshift"}
};

void ExpTree::display() {
  if (!getRoot()) return;
  std::stack<std::pair<ENode*, int>> enodes;
  enodes.push(std::make_pair(getRoot(), 1));
  if (getlval()) enodes.push(std::make_pair(getlval(), 1));
  while (!enodes.empty()) {
    ENode* top;
    int depth;
    std::tie(top, depth) = enodes.top();
    enodes.pop();
    if (!top) {
      printf("%s(EMPTY)\n",std::string(depth * 2, ' ').c_str());
      continue;
    }
    printf("%s(%d %s %p) %s %s [width=%d, sign=%d, type=%d]", std::string(depth * 2, ' ').c_str(), top->opType, OP2Name[top->opType], top, (top->nodePtr) ? top->nodePtr->name.c_str(): "", top->strVal.c_str(), top->width, top->sign, (top->nodePtr) ? top->nodePtr->type: 0);
    for (int val : top->values) printf(" %d", val);
    printf("\n");
    for (int i = top->child.size() - 1; i >= 0; i --) {
      ENode* childENode = top->child[i];
      enodes.push(std::make_pair(childENode, depth + 1));
    }
  }
}

/* traverse graph */
void graph::traversal() {
  for (SuperNode* super : sortedSuper) {
    super->display();
  }
}

void SuperNode::display() {
   printf("----super %d(type=%d, nodeNum %ld order %d)----:\n", id, superType, member.size(), order);
    for (Node* node : member) {
      node->display();
    }
}

void Node::display() {
  printf("node %s[width %d sign %d status %d type %d super %d]:\n", name.c_str(), width, sign, status, type, super->id);
  for (size_t i = 0; i < assignTree.size(); i ++) {
    printf("[assign] %ld\n", i);
    assignTree[i]->display();
  }
  for (size_t i = 0; i < arrayVal.size(); i ++) {
    if (!arrayVal[i]) continue;
    printf("[array] %ld\n", i);
    arrayVal[i]->display();
  }
  if (updateTree) {
    printf("[updateTree]:\n");
    updateTree->display();
  }
  if (resetTree) {
    printf("[resetTree]:\n");
    resetTree->display();
  }
#if 1
  for (Node* nextNode : next) {
    printf("    next %p %s\n", nextNode, nextNode->name.c_str());
  }
  for (Node* prevNode : prev) {
    printf("    prev %p %s\n", prevNode, prevNode->name.c_str());
  }
#endif
}

void ENode::display() {
  ExpTree* tmp = new ExpTree(this, new ENode());
  tmp->display();
}

void graph::traversalNoTree() {
  int nodeIdx = 0;
  for (size_t idx = 0; idx < sortedSuper.size(); idx ++) {
    SuperNode* super = sortedSuper[idx];
    printf("---%ld super id = %d---\n", idx, super->id);
    for (Node* node : super->member) {
      printf("%d: %s (super %d)\n", nodeIdx ++, node->name.c_str(), super->id);
    }
  }
}
