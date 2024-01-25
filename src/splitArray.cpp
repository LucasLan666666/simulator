#include "common.h"
#include <stack>
#include <set>
#include <map>

static std::set<Node*> fullyVisited;
static std::set<Node*> partialVisited;

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
      for (Node* node : partialVisited) {
        if (node->isArray()) { // split the first array
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
          for (size_t i = 0; i < node->arrayVal.size(); i ++) {
            Node* member = node->arrayMemberNode(i);
            Assert(!member->isArray() || !member->valTree, "%s implement me!", member->name.c_str());
          }
          /* construct connections */
          for (Node* member : node->arrayMember) {
            member->updateConnect();
          }
          for (Node* next : node->next) next->updateConnect();

          /* clear node connection */
          node->prev.clear();
          node->next.clear();

          for (Node* member : node->arrayMember) member->constructSuperConnect();
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
  }
}

Node* Node::arrayMemberNode(int idx) {
  Assert(isArray(), "%s is not array", name.c_str());
  std::vector<int>index(dimension);
  int dividend = dimension.back();
  int divisor = idx;

  for (int i = (int)dimension.size() - 1; i >= 0; i --) {
    dividend = dimension[i] + 1;
    index[i] = divisor % dividend;
    divisor = divisor / dividend;
  }
  std::string memberName = name;
  size_t i;
  for (i = 0; i < index.size(); i ++) {
    if (index[i] == dimension[i]) break;
    memberName += "__" + std::to_string(index[i]);
  }

  Node* member = new Node(NODE_ARRAY_MEMBER);
  member->name = memberName;
  arrayMember.push_back(member);
  member->arrayParent = this;
  member->setType(width, sign);
  if (arrayVal[idx] && !arrayVal[idx]->isInvalid()) member->valTree = arrayVal[idx];
  member->constructSuperNode();

  for ( ; i < dimension.size(); i ++) member->dimension.push_back(dimension[i]);

  return member;
}