#include "common.h"
#include <stack>
#include <set>
#include <map>

static std::set<Node*> fullyVisited;
static std::set<Node*> partialVisited;

void partialDfs(std::set<Node*>&loop) {
  enum nodeState{NOT_VISIT = 0, EXPANDED, VISITED};
  std::map<Node*, nodeState> state;
  for (Node* node : partialVisited) {
    state[node] = NOT_VISIT;
  }
  std::stack<Node*>s;
  for (Node* node : partialVisited) {
    if (state[node] == VISITED) continue;
    s.push(node);
    while (!s.empty()) {
      Node* top = s.top();
      if (state[top] == NOT_VISIT) {
        state[top] = EXPANDED;
        for (Node* next : top->next) {
          if (partialVisited.find(next) == partialVisited.end()) continue;
          if (next == top) Panic();
          if (state[next] == NOT_VISIT) s.push(next);
          else if (state[next] == EXPANDED) {
            while(!s.empty()) {
              Node* nodeInLoop = s.top();
              s.pop();
              if (state[nodeInLoop] == EXPANDED) loop.insert(nodeInLoop);
              if (nodeInLoop == next) break;
            }
            return;
          }
        }
      } else if (state[top] == EXPANDED) {
        state[top] = VISITED;
        s.pop();
      }

    }
  }
}

ExpTree* dupTreeWithIdx(ExpTree* tree, std::vector<int>& index) {
  ENode* lvalue = tree->getlval()->dup();
  for (int idx : index) {
    ENode* idxNode = new ENode(OP_INDEX_INT);
    idxNode->addVal(idx);
    lvalue->addChild(idxNode);
  }
  ENode* rvalue = tree->getRoot()->dup();
  std::stack<ENode*> s;
  s.push(rvalue);
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (!top->getChild(i)) continue;
    }
    if (top->getNode()) {
      Assert(top->getNode()->isArray(), "%s is not array", top->getNode()->name.c_str());
      for (int idx : index) {
        ENode* idxNode = new ENode(OP_INDEX_INT);
        idxNode->addVal(idx);
        top->addChild(idxNode);
      }
    }
  }
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  return ret;
}

bool point2self(Node* node) {
  std::set<Node*> nextNodes;
  std::stack<Node*> s;
  std::set<Node*> visited;
  for (Node* next : node->next) {
    s.push(next);
    nextNodes.insert(next);
    if (next == node) return true;
  }
  while(!s.empty()) {
    Node* top = s.top();
    s.pop();
    if (visited.find(top) != visited.end()) continue;
    visited.insert(top);
    for (Node* next : top->next) {
      if (next == node) return true;
      if (nextNodes.find(next) != nextNodes.end()) continue;
      s.push(next);
    }
  }
  return false;
}

Node* getSplitArray() {
  /* array points to itself */
  for (Node* node : partialVisited) {
    if (node->isArray() && node->next.find(node) != node->next.end()) return node;
  }

  for (Node* node : partialVisited) {
    if (node->isArray() && point2self(node)) return node;
  }
  Panic();
}

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode) {
  std::stack<ENode*> s;
  s.push(newTree->getRoot());
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
    if (top->opType == OP_WHEN) {
      if (!top->getChild(1)) top->setChild(1, oldNode);
      if (!top->getChild(2)) top->setChild(2, oldNode);
    }
  }
}

void graph::splitArray() {
  std::map<Node*, int> times;
  std::stack<Node*> s;

  for (SuperNode* super : supersrc) {
    for (Node* node : super->member) s.push(node);
  }

  while(!s.empty()) {
    Node* top = s.top();
    s.pop();
    fullyVisited.insert(top);

    for (Node* next : top->next) {
      if (times.find(next) == times.end()) {
        times[next] = 0;
        partialVisited.insert(next);
      }
      times[next] ++;
      if (times[next] == (int)next->prev.size()) {
        s.push(next);
        Assert(partialVisited.find(next) != partialVisited.end(), "%s not found in partialVisited", next->name.c_str());
        partialVisited.erase(next);
      }
    }

    while (s.size() == 0 && partialVisited.size() != 0) {
      /* split arrays in partialVisited until s.size() != 0 */
      Node* node = getSplitArray();
      Assert(!node->arraySplitted(), "%s is already splitted", node->name.c_str());
      printf("split array %s\n", node->name.c_str());
      /* remove prev connection */
      for (Node* prev : node->prev) prev->next.erase(node);
      for (SuperNode* super : node->super->prev) super->next.erase(node->super);
      for (Node* next : node->next) next->prev.erase(node);
      for (SuperNode* super : node->super->next) super->prev.erase(node->super);
      node->super->prev.clear();
      node->super->next.clear();

      for (Node* memberInSuper : node->super->member) {
        if (memberInSuper != node)
          memberInSuper->constructSuperConnect();
      }
      /* create new node */
      int entryNum = 1;
      for (int num : node->dimension) entryNum *= num;

      for (int i = 0; i < entryNum; i ++) {
        Node* member = node->arrayMemberNode(i);
        Assert(!member->isArray() || !member->valTree, "%s implement me!", member->name.c_str());
      }
      /* distribute arrayVal */
      for (ExpTree* tree : node->arrayVal) {
        ArrayMemberList* list = tree->getlval()->getArrayMember(node);

        if (list->member.size() == 1) {
          int idx = list->idx[0];
          if (node->arrayMember[idx]->valTree) {
            /* fill empty when body in tree with old valTree*/
            ExpTree* oldTree = node->arrayMember[idx]->valTree;
            fillEmptyWhen(tree, oldTree->getRoot());
          }
          node->arrayMember[idx]->valTree = tree;
        } else {
          for (size_t i = 0; i < list->member.size(); i ++) {
            int idx = list->idx[i];
            /* compute index for all array operands in tree */
            std::vector<int> subIdx;
            int tmp = idx;
            for (size_t i = tree->getlval()->getChildNum(); i < node->dimension.size(); i ++) {
              subIdx.push_back(tmp / node->dimension[i]);
              tmp %= node->dimension[i];
            }
            tree = dupTreeWithIdx(tree, subIdx);
            /* duplicate tree with idx */
            if (node->arrayMember[idx]->valTree) {
              /* fill empty when body in tree with old valTree*/
              ExpTree* oldTree = node->arrayMember[idx]->valTree;
              fillEmptyWhen(tree, oldTree->getRoot());
            }
            node->arrayMember[idx]->valTree = tree;
          }
        }
      }
      /* construct connections */
      for (Node* member : node->arrayMember) {
        member->updateConnect();
      }
      for (Node* next : node->next) {
        if (next != node)
          next->updateConnect();
      }

      /* clear node connection */
      node->prev.clear();
      node->next.clear();

      for (Node* member : node->arrayMember) member->constructSuperConnect();
      for (Node* member : node->arrayMember) {
        if (member->super->prev.size() == 0) supersrc.insert(member->super);
      }
      /* add into s and visitedSet */
      for (Node* member : node->arrayMember) {
        if (!member->valTree) continue;
        times[member] = 0;
        for (Node* prev : member->prev) {
          if (fullyVisited.find(prev) != fullyVisited.end()) {
            times[member] ++;
          }
        }
        if (times[member] == (int)member->prev.size()) {
          s.push(member);
        } else {
          partialVisited.insert(member);
        }
      }
      /* erase array */
      partialVisited.erase(node);
      break;
    }
  }
}

Node* Node::arrayMemberNode(int idx) {
  Assert(isArray(), "%s is not array", name.c_str());
  std::vector<int>index(dimension);
  int dividend = dimension.back();
  int divisor = idx;

  for (int i = (int)dimension.size() - 1; i >= 0; i --) {
    dividend = dimension[i];
    index[i] = divisor % dividend;
    divisor = divisor / dividend;
  }
  std::string memberName = name;
  size_t i;
  for (i = 0; i < index.size(); i ++) {
    memberName += "[" + std::to_string(index[i]) + "]";
  }

  Node* member = new Node(NODE_ARRAY_MEMBER);
  member->name = memberName;
  arrayMember.push_back(member);
  member->arrayParent = this;
  member->setType(width, sign);
  member->constructSuperNode();

  for ( ; i < dimension.size(); i ++) member->dimension.push_back(dimension[i]);

  return member;
}