/*
  classes of expression tree and nodes in the tree
*/
#ifndef EXPTREE_H
#define EXPTREE_H

class Node;
class valInfo;

enum OPType {
  OP_INVALID,
  OP_MUX,
/* 2expr */
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_REM,
  OP_LT,
  OP_LEQ,
  OP_GT,
  OP_GEQ,
  OP_EQ,
  OP_NEQ,
  OP_DSHL,
  OP_DSHR,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_CAT,
/* 1expr */
  OP_ASUINT,
  OP_ASSINT,
  OP_ASCLOCK,
  OP_ASASYNCRESET,
  OP_CVT,
  OP_NEG,
  OP_NOT,
  OP_ANDR,
  OP_ORR,
  OP_XORR,
/* 1expr1int */
  OP_PAD,
  OP_SHL,
  OP_SHR,
  OP_HEAD,
  OP_TAIL,
/* 1expr2int */
  OP_BITS,
/* index */
  OP_INDEX_INT,
  OP_INDEX,
/* when, may be replaced by mux */
  OP_WHEN,
/* special */
  OP_PRINTF,
  OP_ASSERT,
/* leaf non-node enode */
  OP_INT,
/* special nodes for memory */
  OP_READ_MEM,
};

class ENode {

  valInfo* instsMux(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAdd(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsSub(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsMul(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsDIv(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsRem(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsLt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsLeq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsGt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsGeq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsEq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsNeq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsDshl(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsDshr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAnd(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsOr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsXor(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsCat(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsUInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsSInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsClock(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsSyncReset(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsCvt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsNeg(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsNot(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAndr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsOrr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsXorr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsPad(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsShl(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsShr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsHead(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsTail(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsBits(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsWhen(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsIndexInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsIndex(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsReadMem(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsPrintf();
  valInfo* instsAssert();
  /* used in usedBits */
  // int getWidthFromChild();
  // void passWidthToChild();

  // valInfo* instsSimple2expr();
public:
  Node* nodePtr = nullptr;   // leafNodes: point to a real node; internals: nullptr
  std::vector<ENode*> child;
  OPType opType = OP_INVALID;
  int width = 0;
  bool sign = false;
  int usedBit = -1;
  // bool islvalue = false;  // true for root and L_INDEX, otherwise false
  int id; // used to distinguish different whens
  std::vector<int> values;     // used in int_noinit/int_init leaf
  std::string strVal;
  valInfo* computeInfo = nullptr;
// potential: index
  ENode(OPType type = OP_INVALID) {
    opType = type;
  }
  ENode(Node* _node) {
    nodePtr = _node;
  }
  void setNode(Node* node) {
    nodePtr = node;
  }
  void addChild(ENode* node) { // node can be empty (only for when)
    child.push_back(node);
  }
  int getChildNum() {
    return child.size();
  }
  void setChild(int idx, ENode* node) {
    Assert(getChildNum() > idx, "idx %d is out of bound [0, %d)", idx, getChildNum());
    child[idx] = node;
  }
  ENode* getChild(int idx) {
    Assert(getChildNum() > idx, "idx %d is out of bound [0, %d)", idx, getChildNum());
    return child[idx];
  }
  void addVal(int val) {
    values.push_back(val);
  }
  void setOP(OPType type) {
    opType = type;
  }
  Node* getNode() {
    return nodePtr;
  }
  void setWidth(int _width, bool _sign) {
    width = _width;
    sign = _sign;
  }
  void inferWidth();
  valInfo* compute(Node* n, std::string lvalue, bool isRoot);
  void passWidthToChild();
  void updateWidth();
  std::pair<int, int> getIdx(Node* n);
  Node* getConnectNode();
};

/* 
  ExpTree: The expression tree that describe the computing expression of a node
  ** The root represents the lvalue of expression (node that need to be updated)
  ** The internal node is a specific operations
  ** The leaf points to a real node

*/
class ExpTree {
  ENode* root;
  ENode* lvalue;
  void setOrAllocRoot(ENode* node) {
    if (node) root = node;
    else root = new ENode();
  }
public:
    ExpTree(ENode* root) {
      setOrAllocRoot(root);
    }
    ExpTree(ENode* node, ENode* _lvalue) {
      setOrAllocRoot(node);
      lvalue = _lvalue;
    }
    ExpTree(ENode* node, Node* lnode) {
      setOrAllocRoot(node);
      lvalue = new ENode(lnode);
    }
    ENode* getRoot() {
      return root;
    }
    void setRoot(ENode* _root) {
      root = _root;
    }
    ENode* getlval() {
      return lvalue;
    }
    void setlval(ENode* _lvalue) {
      lvalue = _lvalue;
    }
    void display();
    
};

// class ASTENode {
// public:
//   AggrParentNode* nodePtr = nullptr;   // leafNodes: point to a real node; internals: nullptr
//   std::vector<ASTENode*> child;
//   OPType opType = OP_INVALID;
//   bool islvalue = false;  // true for root and L_INDEX, otherwise false
// };

class ASTExpTree { // used in AST2Graph, support aggregate nodes
  ENode* expRoot = nullptr;
  // ASTENode* aggrRoot = nullptr;
  std::vector<ENode*> aggrForest;
  // std::vector<std::string> name; // aggr member name, used for creating new nodes in visitNode
  AggrParentNode* anyParent; // any nodes, used for creating new nodes in visitNode
  
  void validCheck() {
    Assert((expRoot && aggrForest.size() == 0) || (!expRoot && aggrForest.size() != 0), "invalid ASTENode, expRoot %p aggrNum %ld", expRoot, aggrForest.size());
  }
  void requreNormal() {
    Assert(expRoot && aggrForest.size() == 0, "ASTENode is not a normal node");
  }

public:
  ASTExpTree(bool isAggr, int num = 0) {
    if (isAggr) {
      Assert(num != 0, "invalid aggr type\n");
      for (int i = 0; i < num; i ++) aggrForest.push_back(new ENode());
    } //aggrRoot = new ASTENode();
    else expRoot = new ENode();
  }
  void setOp(OPType op) {
    validCheck();
    if (expRoot) expRoot->setOP(op);
    else {
      for (ENode* root : aggrForest) root->setOP(op);
    }
  }
  void addVal(int _value) {
    requreNormal();
    expRoot->addVal(_value);
  }
  void setType(int _width, int _sign) {
    requreNormal();
    expRoot->width = _width;
    expRoot->sign = _sign;
  }
  bool isAggr() {
    validCheck();
    return aggrForest.size() != 0;
  }
  int getAggrNum() {
    return aggrForest.size();
  }
  // void addName(std::string str) {
  //   name.push_back(str);
  // }
  // std::string getName(int idx) {
  //   return name[idx];
  // }
  void setAnyParent(AggrParentNode* parent) {
    anyParent = parent;
  }
  // add children in arguments into all trees
  void addChildTree(int num, ...) {
    validCheck();
    va_list valist;
    va_start(valist, num);
    for (int i = 0; i < num; i ++) {
      ASTExpTree* childTree = va_arg(valist, ASTExpTree*);
      if (isAggr()) {
        for (size_t i = 0; i < aggrForest.size(); i++) {
          aggrForest[i]->addChild(childTree->getAggr(i));
        }
      }
      else expRoot->addChild(childTree->getExpRoot());
    }
    va_end(valist);
  }
  void addChildSameTree(ASTExpTree* child) {
    Assert(!child->isAggr(), "require normal");
    if (isAggr()) {
        for (size_t i = 0; i < aggrForest.size(); i++) {
          aggrForest[i]->addChild(child->getExpRoot());
        }
      }
      else expRoot->addChild(child->getExpRoot());
  }
  void addChild(ENode* child) {
    if (isAggr()) {
      for (ENode* root : aggrForest) root->addChild(child);
    } else {
      expRoot->addChild(child);
    }
  }
  void updateRoot(ENode* root) {
    requreNormal();
    root->addChild(expRoot);
    expRoot = root;
  }
  ENode* getExpRoot() {
    requreNormal();
    return expRoot;
  }
  ENode* getAggr(int idx) {
    Assert((int)aggrForest.size() > idx, "idx %d is out of bound", idx);
    return aggrForest[idx];
  }
  // duplicated new ASTExpTree with the same name and new root
  ASTExpTree* dupEmpty() {
    ASTExpTree* dup = new ASTExpTree(isAggr(), getAggrNum());
    // for (int i = 0; i < getAggrNum(); i++) {
    //   dup->addName(name[i]);
    // }
    dup->anyParent = anyParent;
    return dup;
  }
  AggrParentNode* getParent() {
    return anyParent;
  }

};

class NodeList {
public:
  std::vector<Node*> nodes;
  
  void merge(NodeList* newList) {
    if (!newList) return;
    nodes.insert(nodes.end(), newList->nodes.begin(), newList->nodes.end());
  }
};

#endif