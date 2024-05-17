
#include <cstdio>
#include "common.h"
#define MAX_NODES_PER_SUPER 7000
/*
  merge nodes with out-degree=1 to their successors
*/
void graph::mergeOut1() {
  for (int i = sortedSuper.size() - 1; i >= 0; i --) {
    SuperNode* super = sortedSuper[i];
    /* do not merge superNodes that contains reg src */
    if (inSrc(super) || super->superType != SUPER_VALID) continue;
    if (super->next.size() == 1) {
      SuperNode* nextSuper = *(super->next.begin());
      if (nextSuper->superType != SUPER_VALID) continue;
      if (nextSuper->member.size() > MAX_NODES_PER_SUPER) continue;
      for (Node* member : super->member) member->super = nextSuper;
      /* move members in super to next super*/
      for (Node* member : super->member) {
        member->super = nextSuper;
      }

      nextSuper->member.insert(nextSuper->member.begin(), super->member.begin(), super->member.end());
      /* update connection */
      nextSuper->prev.erase(super);
      nextSuper->prev.insert(super->prev.begin(), super->prev.end());
      for (SuperNode* prevSuper : super->prev) {
        prevSuper->next.erase(super);
        prevSuper->next.insert(nextSuper);
      }
      super->member.clear();
    }
  }
  removeEmptySuper();
}

/*
  merge nodes with in-degree=1 to their preceding nodes
*/
void graph::mergeIn1() {
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    SuperNode* super = sortedSuper[i];
    if (super->superType != SUPER_VALID) continue;
    if (super->prev.size() == 1) {
      SuperNode* prevSuper = *(super->prev.begin());
      if (inSrc(prevSuper) || prevSuper->superType != SUPER_VALID) continue;
      if (prevSuper->member.size() > MAX_NODES_PER_SUPER) continue;
      /* move members in super to prev super */
      for (Node* member : super->member) member->super = prevSuper;
      Assert(prevSuper->member.size() != 0, "empty prevSuper %d", prevSuper->id);
      prevSuper->member.insert(prevSuper->member.end(), super->member.begin(), super->member.end());
      /* update connection */
      prevSuper->next.erase(super);
      prevSuper->next.insert(super->next.begin(), super->next.end());
      for (SuperNode* nextSuper : super->next) {
        nextSuper->prev.erase(super);
        nextSuper->prev.insert(prevSuper);
      }
      super->member.clear();
    }
  }

  removeEmptySuper();
  // reconnectSuper();
}

void graph::mergeNodes() {
  size_t totalSuper = sortedSuper.size();

  mergeOut1();
  mergeIn1();

  size_t optimizeSuper = sortedSuper.size();
  printf("[mergeNodes] remove %ld superNodes (%ld -> %ld)\n", totalSuper - optimizeSuper, totalSuper, optimizeSuper);

}