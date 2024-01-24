/*
 determine whether two nodes refer to the same value, keep only one copy of it
*/

#include "common.h"
#include <stack>

/* TODO: array alias */
/* TODO: A = B[idx] */
ENode* Node::isAlias() {
  if (isArray()) {
    /* alias array A to array B iff
      A <= B (if idx = dim[0], A.arrayval[idx] = B, otherwise A.arrayval[idx] = null ) (Done)
      A[i] <= B[i] for every i  (WIP)
    */
    if (std::count(arrayVal.begin(), arrayVal.end(), nullptr) == 1) {
      ExpTree* tree = arrayVal[dimension[0]];
      if (tree && tree->getRoot()->getChildNum() == 0) return tree->getRoot();
    }
    return nullptr;
  }
  if (type != NODE_OTHERS) return nullptr;
  if (!valTree) return nullptr;
  if (!valTree->getRoot()->getNode()) return nullptr;
  if (prev.size() != 1) return nullptr;
  return valTree->getRoot();
}

ENode* ENode::mergeSubTree(ENode* newSubTree) {
  ENode* ret = newSubTree->dup();
  for (ENode* childENode : child) ret->addChild(childENode);
  return ret;
}

/* replace ENode points to oldnode to newSubTree*/
void ExpTree::replace(Node* oldNode, ENode* newSubTree) {
  Assert(!getlval() || getlval()->getNode() != oldNode, "invalid lvalue");

  std::stack<ENode*> s;

  if(getRoot()->getNode() == oldNode) {
    setRoot(getRoot()->mergeSubTree(newSubTree));
  } else {
    s.push(getRoot());
  }
  if (getlval()) s.push(getlval());

  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (!top->getChild(i)) continue;
      if (top->getChild(i)->getNode() == oldNode) {
        top->setChild(i, top->getChild(i)->mergeSubTree(newSubTree));
      } else {
        s.push(top->getChild(i));
      }
    }
  }
}

Node* ENode::getLeafNode() {
  if (!getNode()) return nullptr;
  Node* node = getNode();
  if (node->isArray() && node->arraySplitted()) {
    int idx = getArrayIndex(node);
    return node->arrayMember[idx];
  }
  return node;
}

/*
A -> |         | -> E
     | C  -> D |
B -> |         | -> F
           remove   
*/
void graph::aliasAnalysis() {
  /* counters */
  size_t totalNodes = 0;
  size_t aliasNum = 0;
  size_t totalSuper = sortedSuper.size();

  for (SuperNode* super : sortedSuper) {
    totalNodes += super->member.size();
    for (Node* member : super->member) {
      ENode* enode = member->isAlias();
      if (!enode) continue;
      aliasNum ++;
      Node* origin = enode->getLeafNode();

      Assert(member->prev.size() == 1 && *(member->prev.begin()) == origin, "%s prev %s != %s",
              member->name.c_str(), (*(member->prev.begin()))->name.c_str(), origin->name.c_str());

      /* update connection */
      origin->next.insert(member->next.begin(), member->next.end());
      origin->next.erase(member);
      for (Node* next : member->next) {
        next->prev.erase(member);
        next->prev.insert(origin);
      }
      /* update valTree */
      for (Node* next : member->next) {
        for (ExpTree* tree : next->arrayVal) {
          if (tree)
            tree->replace(member, enode);
        }
        if (next->valTree) next->valTree->replace(member, enode);
      }
      member->status = DEAD_NODE;
    }
  }

  updateSuper();

  printf("remove %ld alias (%ld -> %ld)\n", aliasNum, totalNodes, totalNodes - aliasNum);
  printf("remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());

}