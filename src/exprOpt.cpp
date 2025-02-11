#include "common.h"
#include <stack>
#include <tuple>

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);

void Node::fillArrayInvalid(ExpTree* tree) {
  int beg, end;
  std::tie(beg, end) = tree->getlval()->getIdx(this);
    bool canBeInvalid = true;
    for (int i = beg; i <= end; i ++) {
      if (invalidIdx.find(i) == invalidIdx.end()) canBeInvalid = false;
    }
    if (canBeInvalid) fillEmptyWhen(tree, new ENode(OP_INVALID));
}

void Node::invalidArrayOptimize() {
  if (invalidIdx.size() == 0) return;
  if (!isArray()) return;
  if (type != NODE_OTHERS) return;
  /* can compute in AST2Graph and pass using member variable */
  std::set<int> allIdx;
  int beg, end;
  for (ExpTree* tree : assignTree) {
    std::tie(beg, end) = tree->getlval()->getIdx(this);
    if (beg < 0) return;  // varialble index
    for (int i = beg; i <= end; i ++) {
      if (allIdx.find(i) != allIdx.end()) return; // multiple assignment
      allIdx.insert(i);
    }
  }
  for (ExpTree* tree : arrayVal) {
    std::tie(beg, end) = tree->getlval()->getIdx(this);
    if (beg < 0) return;  // varialble index
    for (int i = beg; i <= end; i ++) {
      if (allIdx.find(i) != allIdx.end()) return; // multiple assignment
      allIdx.insert(i);
    }
  }
  for (ExpTree* tree : assignTree) fillArrayInvalid(tree);
  for (ExpTree* tree : arrayVal) fillArrayInvalid(tree);
}

bool checkENodeEq(ENode* enode1, ENode* enode2);
bool subTreeEq(ENode* enode1, ENode* enode2) {
  if (enode1 == NULL && enode2 == NULL) {
    // TODO: opt this case
    printf("%s,%d: opt this case\n", __FILE__, __LINE__);
    return false;
  }
  if (!checkENodeEq(enode1, enode2)) return false;
  for (size_t i = 0; i < enode1->getChildNum(); i ++) {
    if (!subTreeEq(enode1->getChild(i), enode2->getChild(i))) return false;
  }
  return true;
}

bool ExpTree::treeOpt() {
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(getRoot(), nullptr, -1));
  bool change = false;

  while(!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    if ((top->opType == OP_WHEN || top->opType == OP_MUX)) {
      if (subTreeEq(top->getChild(1), top->getChild(2))) {
        change = true;
        if (parent) {
          parent->setChild(idx, top->getChild(1));
        } else {
          setRoot(top->getChild(1));
        }
        s.push(std::make_tuple(top->getChild(1), parent, idx));
        continue;
      }
    }
    if (top->opType == OP_ASASYNCRESET) {
      change = true;
      if (parent) {
        parent->setChild(idx, top->getChild(0));
      } else {
        setRoot(top->getChild(0));
      }
      s.push(std::make_tuple(top->getChild(0), parent, idx));
      continue;
    }
    if (top->opType == OP_ASSINT) {
      if (top->getChild(0)->sign == top->sign && top->getChild(0)->width == top->width) {
        change = true;
        if (parent) {
          parent->setChild(idx, top->getChild(0));
        } else {
          setRoot(top->getChild(0));
        }
        s.push(std::make_tuple(top->getChild(0), parent, idx));
      }
      continue;
    }
    /* width = 1 xor 0*/
    for (size_t i = 0; i < top->child.size(); i ++) {
      if (top->child[i]) s.push(std::make_tuple(top->child[i], top, i));
    }
  }
  return change;
}

bool graph::exprOpt() {
  bool change = false;
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID) continue;
    for (Node* node : super->member) {
      if (node->width == 0) {
        change = true;
        node->assignTree.clear();
        node->arrayVal.clear();
        ENode* enodeInt = new ENode(OP_INT);
        enodeInt->width = 0;
        enodeInt->strVal = "h0";
        if (node->isArray()) node->arrayVal.push_back(new ExpTree(enodeInt, node));
        else node->assignTree.push_back(new ExpTree(enodeInt, node));
        continue;
      }
      for (ExpTree* tree : node->assignTree) change |= tree->treeOpt();
      for (ExpTree* tree : node->arrayVal) change |= tree->treeOpt();
      if (node->updateTree) change |= node->updateTree->treeOpt();
      if (node->resetTree) change |= node->resetTree->treeOpt();
    }
  }

  reconnectAll();
  return change;
}
