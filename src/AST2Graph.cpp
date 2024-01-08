/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/


#include "common.h"
#include <stack>
#include <map>

/* check the node type and children num */
#define TYPE_CHECK(node, min, max,...) typeCheck(node, (const int[]){__VA_ARGS__}, sizeof((const int[]){__VA_ARGS__}) / sizeof(int), min, max)
#define SEP_MODULE '$' // seperator for module
#define SEP_AGGR '_'

int p_stoi(const char* str);
TypeInfo* visitType(graph* g, PNode* ptype, NodeType parentType);
ASTExpTree* visitExpr(graph* g, PNode* expr);
void visitStmts(graph* g, PNode* stmts);
void visitWhen(graph* g, PNode* when);

/* map between module name and module pnode*/
static std::map<std::string, PNode*> moduleMap;
/* prefix trace. module1, module1$module2 module1$a_b_c...*/
static std::stack<std::string> prefixTrace;
static std::map<std::string, Node*> allSignals;
static std::map<std::string, AggrParentNode*> allDummy; // CHECK: any other dummy nodes ?
static std::vector<std::pair<bool, Node*>> whenTrace;
static std::set<std::string> moduleInstances;

static inline void typeCheck(PNode* node, const int expect[], int size, int minChildNum, int maxChildNum) {
  const int* expectEnd = expect + size;
  Assert((size == 0) || (std::find(expect, expectEnd, node->type) != expectEnd),
    "The type of node %s should in {%d...}, got %d\n", node->name.c_str(), expect[0], node->type);
  Assert(node->getChildNum() >= minChildNum && node->getChildNum() <= maxChildNum,
    "the childNum %d of node %s is out of bound [%d, %d]", node->getChildNum(), node->name.c_str(), minChildNum, maxChildNum);
}

static inline Node* allocNode(NodeType type = NODE_OTHERS, std::string name = "") {
  Node* node = new Node(type);
  node->name = name;
  return node;
}

static inline void addSignal(std::string s, Node* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  Assert(allDummy.find(s) == allDummy.end(), "Signal %s is already in allDummy\n", s.c_str());
  allSignals[s] = n;
  // printf("add signal %s\n", s.c_str());
}

static inline Node* getSignal(std::string s) {
  Assert(allSignals.find(s) != allSignals.end(), "Signal %s is not in allSignals\n", s.c_str());
  return allSignals[s];  
}

static inline void addDummy(std::string s, AggrParentNode* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  Assert(allDummy.find(s) == allDummy.end(), "Node %s is already in allDummy\n", s.c_str());
  allDummy[s] = n;
  // printf("add dummy %s\n", s.c_str());
}

static inline AggrParentNode* getDummy(std::string s) {
  Assert(allDummy.find(s) != allDummy.end(), "Node %s is not in allDummy\n", s.c_str());
  return allDummy[s];
}

static inline bool isAggr(std::string s) {
  if (allDummy.find(s) != allDummy.end()) return true;
  if (allSignals.find(s) != allSignals.end()) return false;
  Assert(0, "%s is not added\n", s.c_str());
}

static inline std::string prefix(char ch) {
  return prefixTrace.empty() ? "" : prefixTrace.top() + ch;
}

static inline std::string topPrefix() {
  Assert(!prefixTrace.empty(), "prefix is empty");
  return prefixTrace.top();
}

static inline std::string prefixName(char ch, std::string name) {
  return prefix(ch) + name;
}

static inline void prefix_append(char ch, std::string str) {
  prefixTrace.push(prefix(ch) + str);
}

static inline void prefix_pop() {
  prefixTrace.pop();
}

/*
field: ALLID ':' type { $$ = newNode(P_FIELD, synlineno(), $1, 1, $3); }
    | Flip ALLID ':' type  { $$ = newNode(P_FLIP_FIELD, synlineno(), $2, 1, $4); }
@return: node list in this field
*/
TypeInfo* visitField(graph* g, PNode* field, NodeType parentType) {
  NodeType fieldType = parentType;
  if (field->type == P_FLIP_FIELD) fieldType = parentType == NODE_INP ? NODE_OUT :
                                              (parentType == NODE_OUT ? NODE_INP : fieldType);
  prefix_append(SEP_AGGR, field->name);
  TypeInfo* info = visitType(g, field->getChild(0), fieldType);
  prefix_pop();
  return info;
  
}

/*
fields:                 { $$ = new PList(); }
    | fields ',' field  { $$ = $1; $$->append($3); }
    | field             { $$ = new PList($1); }
*/
TypeInfo* visitFields(graph* g, PNode* fields, NodeType parentType) {
  TypeInfo* info = new TypeInfo();
  for (int i = 0; i < fields->getChildNum(); i ++) {
    PNode* field = fields->getChild(i);
    TypeInfo* fieldInfo = visitField(g, field, parentType);
    if (!fieldInfo->isAggr()) { // The type of field is ground
      Node* fieldNode = allocNode(parentType, prefixName(SEP_AGGR, field->name));
      fieldNode->updateInfo(fieldInfo);
      info->add(fieldNode);
    } else { // The type of field is aggregate
      info->mergeInto(fieldInfo);
    }
    delete fieldInfo;
  }
  return info;
}

/*
type_aggregate: '{' fields '}'  { $$ = new PNode(P_AG_FIELDS, synlineno()); $$->appendChildList($2); }
    | type '[' INT ']'          { $$ = newNode(P_AG_TYPE, synlineno(), emptyStr, 1, $1); $$->appendExtraInfo($3); }
type_ground: Clock    { $$ = new PNode(P_Clock, synlineno()); }
    | IntType width   { $$ = newNode(P_INT_TYPE, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); }
    | anaType width   { TODO(); }
    | FixedType width binary_point  { TODO(); }
update width/sign/dimension/aggrtype
*/
TypeInfo* visitType(graph* g, PNode* ptype, NodeType parentType) {
  TYPE_CHECK(ptype, 0, INT32_MAX, P_AG_FIELDS, P_AG_TYPE, P_Clock, P_INT_TYPE);
  TypeInfo* info = NULL;
  switch (ptype->type) {
    case P_Clock:
      info = new TypeInfo();
      info->set_sign(false); info->set_width(1);
      break;
    case P_INT_TYPE:
      info = new TypeInfo();
      info->set_sign(ptype->sign); info->set_width(ptype->width);
      break;
    case P_AG_FIELDS:
      info = visitFields(g, ptype, parentType);
      info->newParent(topPrefix());
      break;
    case P_AG_TYPE:
      TYPE_CHECK(ptype, 1, 1, P_AG_TYPE);
      info = visitType(g, ptype->getChild(0), parentType);
      info->addDim(p_stoi(ptype->getExtra(0).c_str()));
      break;
    default:
      Panic();
  }
  Assert(info, "info should not be empty");
  return info;
}

/*
port: Input ALLID ':' type info    { $$ = newNode(P_INPUT, synlineno(), $5, $2, 1, $4); }
    | Output ALLID ':' type info   { $$ = newNode(P_OUTPUT, synlineno(), $5, $2, 1, $4); }
all nodes are stored in aggrMember
*/
TypeInfo* visitPort(graph* g, PNode* port, bool isTop) {
  TYPE_CHECK(port, 1, 1, P_INPUT, P_OUTPUT);

  NodeType type = isTop ? (port->type == P_INPUT ? NODE_INP : NODE_OUT) : NODE_OTHERS;
  prefix_append(SEP_MODULE, port->name);
  TypeInfo* info = visitType(g, port->getChild(0), type);

  if (!info->isAggr()) {
    Node* node = allocNode(type, topPrefix());
    node->updateInfo(info);
    info->add(node);
  }
  prefix_pop();
  return info;
}

/*
  ports:  { $$ = new PNode(P_PORTS); }
      | ports port    { $$ = $1; $$->appendChild($2); }
*/
void visitTopPorts(graph* g, PNode* ports) {
  TYPE_CHECK(ports, 0, INT32_MAX, P_PORTS);
  // visit all ports
  for (int i = 0; i < ports->getChildNum(); i ++) {
    PNode* port = ports->getChild(i);
    TypeInfo* info = visitPort(g, port, true);
    for (Node* node : info->aggrMember) {
      addSignal(node->name, node);
      if (node->type == NODE_INP) g->input.push_back(node);
      else if (node->type == NODE_OUT) g->output.push_back(node);
    }
    for (AggrParentNode* dummy : info->aggrParent) {
      addDummy(dummy->name, dummy);
    }
  }
}
/*
expr: IntType width '(' ')'     { $$ = newNode(P_EXPR_INT_NOINIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S');}
*/
ASTExpTree* visitIntNoInit(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 0, 0, P_EXPR_INT_NOINIT);
  ASTExpTree* ret = new ASTExpTree(false);
  ret->addVal(0);
  ret->setType(expr->width, expr->sign);
  ret->setOp(OP_INT);
  return ret;
}
/*
| IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); $$->appendExtraInfo($4);}
*/
ASTExpTree* visitIntInit(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 0, 0, P_EXPR_INT_INIT);
  ASTExpTree* ret = new ASTExpTree(false);
  ret->getExpRoot()->strVal= expr->getExtra(0);
  ret->setType(expr->width, expr->sign);
  ret->setOp(OP_INT);
  return ret;
}

ENode* allocIntIndex(std::string intStr) {
  ENode* index = new ENode(OP_INDEX_INT);
  index->addVal(p_stoi(intStr.c_str()));
  return index;
}
/*
  OP_INDEX
      \
      exprTree
*/
ASTExpTree* allocIndex(graph* g, PNode* expr) {
  ASTExpTree* exprTree = visitExpr(g, expr);
  ENode* index = new ENode(OP_INDEX);
  exprTree->updateRoot(index);
  return exprTree;
}
/*
    reference: ALLID  { $$ = newNode(P_REF, synlineno(), $1, 0); }
    | reference '.' ALLID  { $$ = $1; $1->appendChild(newNode(P_REF_DOT, synlineno(), $3, 0)); }
    | reference '[' INT ']' { $$ = $1; $1->appendChild(newNode(P_REF_IDX_INT, synlineno(), $3, 0)); }
    | reference '[' expr ']' { $$ = $1; $1->appendChild(newNode(P_REF_IDX_EXPR, synlineno(), NULL, 1, $3)); }
*/
ASTExpTree* visitReference(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 0, INT32_MAX, P_REF);
  std::string name = prefixName(SEP_MODULE, expr->name);

  for (int i = 0; i < expr->getChildNum(); i ++) {
    if (expr->getChild(i)->type == P_REF_DOT) {
      name += (moduleInstances.find(name) != moduleInstances.end() ? SEP_MODULE : SEP_AGGR) + expr->getChild(i)->name;
    }
  }
  ASTExpTree* ret = nullptr;
  if (isAggr(name)) { // add all signals and their names into aggr
    AggrParentNode* parent = getDummy(name);
    ret = new ASTExpTree(true, parent->size());
    ret->setAnyParent(parent);
    // point the root of aggrTree to parent member
    // the width of node may not be determined yet
    for (int i = 0; i < parent->size(); i++) {
      ret->getAggr(i)->setNode(parent->member[i]);
    }
  } else {
    ret = new ASTExpTree(false);
    ret->getExpRoot()->setNode(getSignal(name));
  }
  // update dimensions
  for (int i = 0; i < expr->getChildNum(); i ++) {
    PNode* child = expr->getChild(i);
    switch(child->type) {
      case P_REF_IDX_EXPR: ret->addChildSameTree(allocIndex(g, child->getChild(0))); break;
      case P_REF_IDX_INT: ret->addChild(allocIntIndex(child->name.c_str())); break;
      case P_REF_DOT: break;
      default: Panic();
    }
  }
  return ret;
}
/*
| Mux '(' expr ',' expr ',' expr ')' { $$ = newNode(P_EXPR_MUX, synlineno(), NULL, 3, $3, $5, $7); }
*/
ASTExpTree* visitMux(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 3, 3, P_EXPR_MUX);

  ASTExpTree* cond = visitExpr(g, expr->getChild(0));
  ASTExpTree* left = visitExpr(g, expr->getChild(1));
  ASTExpTree* right = visitExpr(g, expr->getChild(2));

  ASTExpTree* ret = left->dupEmpty();
  ret->setOp(OP_MUX);
  ret->addChildSameTree(cond);
  ret->addChildTree(2, left, right);

  delete cond;
  delete left;
  delete right;

  return ret;
}
/*
primop_2expr: E2OP expr ',' expr ')' { $$ = newNode(P_2EXPR, synlineno(), $1, 2, $2, $4); }
*/
ASTExpTree* visit2Expr(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 2, 2, P_2EXPR);

  ASTExpTree* left = visitExpr(g, expr->getChild(0));
  ASTExpTree* right = visitExpr(g, expr->getChild(1));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(str2op_expr2(expr->name));
  ret->addChildTree(2, left, right);

  delete left;
  delete right;

  return ret;
}
/*
primop_1expr: E1OP expr ')' { $$ = newNode(P_1EXPR, synlineno(), $1, 1, $2); }
*/
ASTExpTree* visit1Expr(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR);

  ASTExpTree* child = visitExpr(g, expr->getChild(0));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(str2op_expr1(expr->name));
  ret->addChildTree(1, child);

  delete child;

  return ret;
}
/*
primop_1expr1int: E1I1OP expr ',' INT ')' { $$ = newNode(P_1EXPR1INT, synlineno(), $1, 1, $2); $$->appendExtraInfo($4); }
*/
ASTExpTree* visit1Expr1Int(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR1INT);

  ASTExpTree* child = visitExpr(g, expr->getChild(0));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(str2op_expr1int1(expr->name));
  ret->addChildTree(1, child);
  ret->addVal(p_stoi(expr->getExtra(0).c_str()));

  delete child;

  return ret;
}
/*
primop_1expr2int: E1I2OP expr ',' INT ',' INT ')' { $$ = newNode(P_1EXPR2INT, synlineno(), $1, 1, $2); $$->appendExtraInfo($4); $$->appendExtraInfo($6); }
*/
ASTExpTree* visit1Expr2Int(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR2INT);

  ASTExpTree* child = visitExpr(g, expr->getChild(0));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(OP_BITS);
  ret->addChildTree(1, child);
  ret->addVal(p_stoi(expr->getExtra(0).c_str()));
  ret->addVal(p_stoi(expr->getExtra(1).c_str()));

  delete child;
  return ret;
}


/*
expr: IntType width '(' ')'     { $$ = newNode(P_EXPR_INT_NOINIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S');}
    | IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); $$->appendExtraInfo($4);}
    | reference { $$ = $1; }
    | Mux '(' expr ',' expr ',' expr ')' { $$ = newNode(P_EXPR_MUX, synlineno(), NULL, 3, $3, $5, $7); }
    | Validif '(' expr ',' expr ')' { $$ = $5; }
    | primop_2expr  { $$ = $1; }
    | primop_1expr  { $$ = $1; }
    | primop_1expr1int  { $$ = $1; }
    | primop_1expr2int  { $$ = $1; }
    ;
*/
ASTExpTree* visitExpr(graph* g, PNode* expr) {
  ASTExpTree* ret;
  switch (expr->type) {
    case P_EXPR_INT_NOINIT: ret = visitIntNoInit(g, expr); break;
    case P_EXPR_INT_INIT: ret = visitIntInit(g, expr); break;
    case P_REF: ret = visitReference(g, expr); break;
    case P_EXPR_MUX: ret = visitMux(g, expr); break;
    case P_2EXPR: ret = visit2Expr(g, expr); break;
    case P_1EXPR: ret = visit1Expr(g, expr); break;
    case P_1EXPR1INT: ret = visit1Expr1Int(g, expr); break;
    case P_1EXPR2INT: ret = visit1Expr2Int(g, expr); break;
    default: printf("Invalid type %d\n", expr->type); Panic();
  }
  return ret;
}
/*
module: Module ALLID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, synlineno(), $4, $2, 2, $6, $7); }
*/
void visitModule(graph* g, PNode* module) {
  TYPE_CHECK(module, 2, 2, P_MOD);
  // printf("visit module %s\n", module->name.c_str());

  PNode* ports = module->getChild(0);
  for (int i = 0; i < ports->getChildNum(); i ++) {
    TypeInfo* portInfo = visitPort(g, ports->getChild(i), false);

    for (Node* node : portInfo->aggrMember) {
      addSignal(node->name, node);
    }
    for (AggrParentNode* dummy : portInfo->aggrParent) {
      addDummy(dummy->name, dummy);
    }
  }

  visitStmts(g, module->getChild(1));
  // printf("leave module %s\n", module->name.c_str());
}
/*
extmodule: Extmodule ALLID ':' info INDENT ports ext_defname params DEDENT  { $$ = newNode(P_EXTMOD, synlineno(), $4, $2, 1, $6); $$->appendChildList($8);}
*/
void visitExtModule(graph* g, PNode* module) {
  TYPE_CHECK(module, 1, 1, P_EXTMOD);
  /* the same process of visitModule */
  PNode* ports = module->getChild(0);
  for (int i = 0; i < ports->getChildNum(); i ++) {
    TypeInfo* portInfo = visitPort(g, ports->getChild(i), false);

    for (Node* node : portInfo->aggrMember) {
        addSignal(node->name, node);
      }
    for (AggrParentNode* dummy : portInfo->aggrParent) {
      addDummy(dummy->name, dummy);
    }
  }
}

/*
statement: Wire ALLID ':' type info    { $$ = newNode(P_WIRE_DEF, $4->lineno, $5, $2, 1, $4); }
*/
void visitWireDef(graph* g, PNode* wire) {
  TYPE_CHECK(wire, 1, 1, P_WIRE_DEF);

  prefix_append(SEP_MODULE, wire->name);

  TypeInfo* info = visitType(g, wire->getChild(0), NODE_OTHERS);

  for (Node* node : info->aggrMember) addSignal(node->name, node);
  for (AggrParentNode* dummy : info->aggrParent) addDummy(dummy->name, dummy);
  if (!info->isAggr()) {
    Node* node = allocNode(NODE_OTHERS, topPrefix());
    node->updateInfo(info);
    addSignal(node->name, node);
  }

  prefix_pop();
}

/*
statement: Reg ALLID ':' type ',' expr(1) RegWith INDENT RegReset '(' expr ',' expr ')' info DEDENT { $$ = newNode(P_REG_DEF, $4->lineno, $15, $2, 4, $4, $6, $11, $13); }
expr(1) must be clock
*/
void visitRegDef(graph* g, PNode* reg) {
  TYPE_CHECK(reg, 4, 4, P_REG_DEF);
  
  prefix_append(SEP_MODULE, reg->name);
  TypeInfo* info = visitType(g, reg->getChild(0), NODE_REG_SRC);
  /* alloc node for basic nodes */
  if (!info->isAggr()) {
    Node* src = allocNode(NODE_REG_SRC, topPrefix());
    src->updateInfo(info);
    info->add(src);
  }
  // add reg_src and red_dst to all signals
  for (Node* src : info->aggrMember) {
    g->addReg(src);
    Node* dst = src->dup();
    dst->type = NODE_REG_DST;
    dst->name += "$NEXT";
    addSignal(src->name, src);
    addSignal(dst->name, dst);
    src->bindReg(dst);
  }
  // only src dummy nodes are in allDummy
  for (AggrParentNode* dummy : info->aggrParent) addDummy(dummy->name, dummy);
  
  prefix_pop();

  ASTExpTree* resetCond = visitExpr(g, reg->getChild(2));
  ASTExpTree* resetVal = visitExpr(g, reg->getChild(3));
  Assert(!resetCond->isAggr(), "reg %s: reset cond can never be aggregate\n", reg->name.c_str());
  // all aggregate nodes share the same resetCond ExpRoot
  for (size_t i = 0; i < info->aggrMember.size(); i ++) {
    Node* src = info->aggrMember[i];
    src->resetCond = new ExpTree(resetCond->getExpRoot(), src);
    if (info->isAggr())
      src->resetVal = new ExpTree(resetVal->getAggr(i), src);
    else
      src->resetVal = new ExpTree(resetVal->getExpRoot(), src);
  }
}
/*
mem_datatype: DataType "=>" type { $$ = newNode(P_DATATYPE, synlineno(), NULL, 1, $3); }
*/
static inline TypeInfo* visitMemType(graph* g, PNode* dataType) {
  TYPE_CHECK(dataType, 1, 1, P_DATATYPE);
  return visitType(g, dataType->getChild(0), NODE_INVALID);
}
/*
mem_depth: Depth "=>" INT   { $$ = newNode(P_DEPTH, synlineno(), $3, 0); }
*/
static inline int visitMemDepth(PNode* depth) {
  TYPE_CHECK(depth, 0, 0, P_DEPTH);
  return p_stoi(depth->name.c_str());
}
/*
mem_rlatency: ReadLatency "=>" INT  { $$ = newNode(P_RLATENCT, synlineno(), $3, 0); }
*/
static inline int visitReadLatency(PNode* latency) {
  TYPE_CHECK(latency, 0, 0, P_RLATENCT);
  return p_stoi(latency->name.c_str());
}
/*
mem_wlatency: WriteLatency "=>" INT { $$ = newNode(P_WLATENCT, synlineno(), $3, 0); }
*/
static inline int visitWriteLatency(PNode* latency) {
  TYPE_CHECK(latency, 0, 0, P_WLATENCT);
  return p_stoi(latency->name.c_str());
}
/*
mem_ruw: ReadUnderwrite "=>" Ruw { $$ = newNode(P_RUW, synlineno(), $3, 0); }
*/
static inline void visitRUW(PNode* ruw) {
  TYPE_CHECK(ruw, 0, 0, P_RUW);
  Assert(ruw->name == "undefined", "IMPLEMENT ME");
}

static inline void add_member(Node* parent, std::string name, int idx, int width, int sign) {
  Node* member = allocNode(NODE_MEM_MEMBER, prefixName(SEP_AGGR, name));
  member->setType(width, sign);
  parent->set_member(idx, member);
  addSignal(member->name, member);
}
/*
mem_reader Reader "=>" ALLID  { $$ = $1; $$->append(newNode(P_READER, synlineno(), $4, 0));}
*/
static inline Node* visitReader(PNode* reader, int width, int depth, bool sign) {
  TYPE_CHECK(reader, 0, 0, P_READER);

  prefix_append(SEP_MODULE, reader->name);

  Node* ret = allocNode(NODE_READER, topPrefix());

  for (int i = 0; i < READER_MEMBER_NUM; i ++) { // resize member vector
    ret->add_member(nullptr);
  }

  add_member(ret, "addr", READER_ADDR, upperLog2(depth), false);
  add_member(ret, "en", READER_EN, 1, false);
  add_member(ret, "clk", READER_CLK, 1, false);
  add_member(ret, "data", READER_DATA, width, sign);

  // addDummy(ret->name, ret); // reader is not needed, use superNode to combine input
  prefix_pop();

  return ret;
}
/*
mem_writer Writer "=>" ALLID    { $$ = $1; $$->append(newNode(P_WRITER, synlineno(), $4, 0));}
*/
static inline Node* visitWriter(PNode* writer, int width, int depth, bool sign) {
  TYPE_CHECK(writer, 0, 0, P_WRITER);

  prefix_append(SEP_MODULE, writer->name);

  Node* ret = allocNode(NODE_WRITER, topPrefix());

  for (int i = 0; i < WRITER_MEMBER_NUM; i ++) {
    ret->add_member(nullptr);
  }

  add_member(ret, "addr", WRITER_ADDR, upperLog2(depth), false);
  add_member(ret, "en", WRITER_EN, 1, false);
  add_member(ret, "clk", WRITER_CLK, 1, false);
  add_member(ret, "data", WRITER_DATA, width, sign);
  add_member(ret, "mask", WRITER_MASK, width, false);

  prefix_pop();

  return ret;
}
/*
mem_readwriter Readwriter "=>" ALLID  { $$ = $1; $$->append(newNode(P_READWRITER, synlineno(), $4, 0));}
*/
static inline Node* visitReadWriter(PNode* readWriter, int width, int depth, bool sign) {
  TYPE_CHECK(readWriter, 0, 0, P_READWRITER);

  prefix_append(SEP_MODULE, readWriter->name);

  Node* ret = allocNode(NODE_READWRITER, topPrefix());

  for (int i = 0; i < READWRITER_MEMBER_NUM; i ++) {
    ret->add_member(nullptr);
  }

  add_member(ret, "addr", READWRITER_ADDR, upperLog2(depth), false);
  add_member(ret, "en", READWRITER_EN, 1, false);
  add_member(ret, "clk", READWRITER_CLK, 1, false);
  add_member(ret, "rdata", READWRITER_RDATA, width, sign);
  add_member(ret, "wdata", READWRITER_WDATA, width, sign);
  add_member(ret, "wmask", READWRITER_WMASK, width, false);
  add_member(ret, "wmode", READWRITER_WMODE, 1, false);

  prefix_pop();

  return ret;
}
/*
memory: Mem ALLID ':' info INDENT mem_compulsory mem_optional mem_ruw DEDENT { $$ = newNode(P_MEMORY, synlineno(), $4, $2, 0); $$->appendChildList($6); $$->appendChild($8); $$->appendChildList($7); }
mem_compulsory: mem_datatype mem_depth mem_rlatency mem_wlatency { $$ = new PList(); $$->append(4, $1, $2, $3, $4); }
mem_optional: mem_reader mem_writer mem_readwriter { $$ = $1; $$->concat($2); $$->concat($3); }
*/
void visitMemory(graph* g, PNode* mem) {
  TYPE_CHECK(mem, 5, INT32_MAX, P_MEMORY);
  
  TypeInfo* info = visitMemType(g, mem->getChild(0));
  if (info->isAggr()) TODO();
  Node* memNode = allocNode(NODE_MEMORY, prefixName(SEP_MODULE, mem->name));
  g->memory.push_back(memNode);
  memNode->updateInfo(info);

  int depth = visitMemDepth(mem->getChild(1));

  memNode->set_memory(depth, visitReadLatency(mem->getChild(2)), visitWriteLatency(mem->getChild(3)));

  visitRUW(mem->getChild(4));

  prefix_append(SEP_MODULE, mem->name);
  moduleInstances.insert(topPrefix());
  for (int i = 5; i < mem->getChildNum(); i ++) {
    PNode* port = mem->getChild(i);
    Node* portNode = nullptr;
    switch(port->type) {
      case P_READER: portNode = visitReader(port, info->width, depth, info->sign); break;
      case P_WRITER: portNode = visitWriter(port, info->width, depth, info->sign); break;
      case P_READWRITER: portNode = visitReadWriter(port, info->width, depth, info->sign); break;
      default: Panic();
    }
    memNode->add_member(portNode);
  }

  prefix_pop();
}
/*
| Inst ALLID Of ALLID info    { $$ = newNode(P_INST, synlineno(), $5, $2, 0); $$->appendExtraInfo($4); }
*/
void visitInst(graph* g, PNode* inst) {
  TYPE_CHECK(inst, 0, 0, P_INST);
  Assert(inst->getExtraNum() >= 1 && moduleMap.find(inst->getExtra(0)) != moduleMap.end(),
               "Module %s is not defined!\n", inst->getExtra(0).c_str());
  PNode* module = moduleMap[inst->getExtra(0)];
  prefix_append(SEP_MODULE, inst->name);
  moduleInstances.insert(topPrefix());
  switch(module->type) {
    case P_MOD: visitModule(g, module); break;
    case P_EXTMOD: visitExtModule(g, module); break;
    case P_INTMOD: TODO();
    default:
      Panic();
  }
  prefix_pop();
}
static inline std::string replacePrefix(std::string oldPrefix, std::string newPrefix, std::string str) {
  Assert(str.compare(0, oldPrefix.length(), oldPrefix) == 0, "member name %s does not start with %s", str.c_str(), oldPrefix.c_str());
  return newPrefix + str.substr(oldPrefix.length());
}

AggrParentNode* allocNodeFromAggr(graph* g, AggrParentNode* parent) {
  AggrParentNode* ret = new AggrParentNode(topPrefix());
  std::string oldPrefix = parent->name;
  /* alloc all real nodes */
  for (Node* member : parent->member) {
    std::string name = replacePrefix(oldPrefix, topPrefix(), member->name);
    /* the type of parent can be registers, thus the node->type cannot set to member->type */
    Node* node = member->dup(NODE_OTHERS, name); // SEP_AGGR is already in name
  
    addSignal(node->name, node);
    ret->addMember(node);
  }
  /* alloc all dummy nodes, and connect them to real nodes stored in allSignals */
  for (AggrParentNode* aggrMember : parent->parent) {
    // create new aggr node
    AggrParentNode* aggrNode = new AggrParentNode(replacePrefix(oldPrefix, topPrefix(), aggrMember->name));
    // update member and parent in new aggrNode
    for (Node* member : aggrMember->member) {
      aggrNode->addMember(getSignal(replacePrefix(oldPrefix, topPrefix(), member->name)));
    }
    // the children of aggrMember are earlier than it
    for (AggrParentNode* parent : aggrMember->parent) {
      aggrNode->addParent(getDummy(replacePrefix(oldPrefix, topPrefix(), parent->name)));
    }

    addDummy(aggrNode->name, aggrNode);
    ret->addParent(aggrNode);
  }
  return ret;
}

/*
| Node ALLID '=' expr info { $$ = newNode(P_NODE, synlineno(), $5, $2, 1, $4); }
*/
void visitNode(graph* g, PNode* node) {
  TYPE_CHECK(node, 1, 1, P_NODE);
  ASTExpTree* exp = visitExpr(g, node->getChild(0));
  prefix_append(SEP_MODULE, node->name);
  if (exp->isAggr()) {// create all nodes in aggregate
    AggrParentNode* aggrNode = allocNodeFromAggr(g, exp->getParent());
    Assert(aggrNode->size() == exp->getAggrNum(), "aggrMember num %d tree num %d", aggrNode->size(), exp->getAggrNum());
    for (int i = 0; i < aggrNode->size(); i ++) {
      aggrNode->member[i]->valTree = new ExpTree(exp->getAggr(i), aggrNode->member[i]);
    }
    addDummy(aggrNode->name, aggrNode);
  } else {
    Node* n = allocNode(NODE_OTHERS, topPrefix());
    n->valTree = new ExpTree(exp->getExpRoot(), n);
    addSignal(n->name, n);
  }
  prefix_pop();
}
/*
| reference "<=" expr info  { $$ = newNode(P_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
*/
void visitConnect(graph* g, PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_CONNECT);
  ASTExpTree* ref = visitReference(g, connect->getChild(0));
  ASTExpTree* exp = visitExpr(g, connect->getChild(1));
  Assert(!(ref->isAggr() ^ exp->isAggr()), "type not match, ref aggr %d exp aggr %d", ref->isAggr(), exp->isAggr());
  if (ref->isAggr()) {
    for (int i = 0; i < ref->getAggrNum(); i ++) {
      Node* node = ref->getAggr(i)->getNode();
      ExpTree* valTree = new ExpTree(exp->getAggr(i), ref->getAggr(i));
      if (node->isArray()) node->addArrayVal(valTree);
      else node->valTree = valTree;

    }
  } else {
    Node* node = ref->getExpRoot()->getNode();
    ExpTree* valTree = new ExpTree(exp->getExpRoot(), ref->getExpRoot());
    if (node->isArray()) node->addArrayVal(valTree);
    else node->valTree = valTree;
  }
}

bool matchWhen(ENode* enode, int depth) {
  if (enode->opType != OP_WHEN) return false;
  Assert(enode->getChildNum() == 3, "invalid child num %d", enode->getChildNum());
  Assert(enode->getChild(0) && enode->getChild(0)->nodePtr, "invalid cond");
  if (enode->getChild(0)->nodePtr == whenTrace[depth].second) return true;
  return false;
}

/* find the latest matched when ENode and the number of matched */
std::pair<ENode*, int> getDeepestWhen(ExpTree* valTree) {
  if (!valTree) return std::make_pair(nullptr, 0);
  ENode* checkNode = valTree->getRoot();
  ENode* whenNode = nullptr;
  int depth = 0;

  for (size_t i = 0; i < whenTrace.size(); i ++) {
    if (checkNode && matchWhen(checkNode, i)) {
      whenNode = checkNode;
      depth = i + 1;
      checkNode = whenTrace[i].first ? checkNode->getChild(1) : checkNode->getChild(2);
    } else {
      break;
    }
  }
  return std::make_pair(whenNode, depth);
}

/*
add when Enodes for node
e.g.
whenTrace: (when1, true), (when2, true), (when3, false)
node->valTree: when1
                |
            cond1 when2 any
                    |
              cond2 a any

oldparent: when2; depth: 2
oldRoot: a
newRoot:  when3
            |
      cond3 a b
replace oldRoot by newRoot
*/
ExpTree* growWhenTrace(ExpTree* valTree) {
  ENode* oldParent = nullptr;
  int maxDepth = 0;
  if (valTree) std::tie(oldParent, maxDepth) = getDeepestWhen(valTree);
  if (maxDepth == (int)whenTrace.size()) return valTree ? valTree : new ExpTree(nullptr);

  ENode* oldRoot = maxDepth == 0 ?
                          (valTree ? valTree->getRoot() : nullptr)
                        : (whenTrace[maxDepth-1].first ? oldParent->getChild(1) : oldParent->getChild(2));
  ENode* newRoot = nullptr; // latest whenNode
  
  for (int depth = whenTrace.size() - 1; depth >= maxDepth ; depth --) {
    ENode* whenNode = new ENode(OP_WHEN);
    ENode* condNode = new ENode(whenTrace[depth].second);

    if (whenTrace[depth].first) {
      whenNode->addChild(condNode);
      whenNode->addChild(newRoot);
      whenNode->addChild(oldRoot);
    } else {
      whenNode->addChild(condNode);
      whenNode->addChild(oldRoot);
      whenNode->addChild(newRoot);
    }
    newRoot = whenNode;

  }
  if (maxDepth == 0) {
    if (valTree) valTree->setRoot(newRoot);
    else valTree = new ExpTree(newRoot);
  } else {
    oldParent->setChild(whenTrace[maxDepth-1].first ? 1 : 2, newRoot);
  }
  return valTree;
}

ENode* getWhenEnode(ExpTree* valTree) {
  ENode* whenNode;
  int maxDepth;
  std::tie(whenNode, maxDepth) = getDeepestWhen(valTree);
  Assert(maxDepth == (int)whenTrace.size(), "when not match %d %ld", maxDepth, whenTrace.size());
  return whenNode;
}

/*
| reference "<=" expr info  { $$ = newNode(P_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
*/
void visitWhenConnect(graph* g, PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_CONNECT);
  ASTExpTree* ref = visitReference(g, connect->getChild(0));
  ASTExpTree* exp = visitExpr(g, connect->getChild(1));
  Assert(!(ref->isAggr() ^ exp->isAggr()), "type not match, ref aggr %d exp aggr %d", ref->isAggr(), exp->isAggr());

  if (ref->isAggr()) {
    for (int i = 0; i < ref->getAggrNum(); i++) {
      Node* node = ref->getAggr(i)->getNode();
      ExpTree* valTree = growWhenTrace(node->valTree);
      valTree->setlval(ref->getAggr(i));
      ENode* whenNode = getWhenEnode(valTree);
      whenNode->setChild(whenTrace.back().first ? 1 : 2, exp->getAggr(i));
      if (node->isArray()) node->addArrayVal(valTree);
      else node->valTree = valTree;
    }
    
  } else {
    Node* node = ref->getExpRoot()->getNode();
    ExpTree* valTree = growWhenTrace(node->valTree);
    valTree->setlval(ref->getExpRoot());
    ENode* whenNode = getWhenEnode(valTree);
    whenNode->setChild(whenTrace.back().first ? 1 : 2, exp->getExpRoot());
    if (node->isArray()) node->addArrayVal(valTree);
    else node->valTree = valTree;
  }
}
/*
  | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
  | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
*/
void visitWhenPrintf(graph* g, PNode* print) {
  TYPE_CHECK(print, 3, 3, P_PRINTF);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, print->name));
  ASTExpTree* exp = visitExpr(g, print->getChild(1));

  ENode* expRoot = exp->getExpRoot();
  for (size_t i = 0; i < whenTrace.size(); i ++) {
    ENode* andNode = new ENode(OP_AND);
    andNode->addChild(expRoot);
    ENode* condNode = new ENode(whenTrace[i].second);
    if (whenTrace[i].first) {
      andNode->addChild(condNode);
    } else {
      ENode* notNode = new ENode(OP_NOT);
      notNode->addChild(condNode);
      andNode->addChild(notNode);
    }
    expRoot = andNode;
  }

  ENode* enode = new ENode(OP_PRINTF);
  enode->strVal = print->getExtra(0);
  enode->addChild(expRoot);

  PNode* exprs = print->getChild(2);
  for (int i = 0; i < exprs->getChildNum(); i ++) {
    ASTExpTree* val = visitExpr(g, exprs->getChild(i));
    enode->addChild(val->getExpRoot());
  }

  n->valTree = new ExpTree(enode);
  addSignal(n->name, n);
}

void visitWhenAssert(graph* g, PNode* ass) {
  TYPE_CHECK(ass, 3, 3, P_ASSERT);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, ass->name));

  ASTExpTree* pred = visitExpr(g, ass->getChild(1));
  ASTExpTree* en = visitExpr(g, ass->getChild(2));

  ENode* enRoot = en->getExpRoot();
  for (size_t i = 0; i < whenTrace.size(); i ++) {
    ENode* andNode = new ENode(OP_AND);
    andNode->addChild(enRoot);
    ENode* condNode = new ENode(whenTrace[i].second);
    if (whenTrace[i].first) {
      andNode->addChild(condNode);
    } else {
      ENode* notNode = new ENode(OP_NOT);
      notNode->addChild(condNode);
      andNode->addChild(notNode);
    }
    enRoot = andNode;
  }

  ENode* enode = new ENode(OP_ASSERT);
  enode->strVal = ass->getExtra(0);

  enode->addChild(pred->getExpRoot());
  enode->addChild(enRoot);
  
  n->valTree = new ExpTree(enode);
  addSignal(n->name, n);
}

/* return the lvalue node */
void visitWhenStmt(graph* g, PNode* stmt) {
  switch (stmt->type) {
    case P_NODE: visitNode(g, stmt); break; // local nodes
    case P_CONNECT: visitWhenConnect(g, stmt); break;
    case P_WHEN: visitWhen(g, stmt); break;
    case P_WIRE_DEF: visitWireDef(g, stmt); break;
    case P_PRINTF: visitWhenPrintf(g, stmt); break;
    case P_ASSERT: visitWhenAssert(g, stmt); break;
    default: printf("Invalid type %d\n", stmt->type); Panic();
  }
}
void visitWhenStmts(graph* g, PNode* stmts) {
  TYPE_CHECK(stmts, 0, INT32_MAX, P_STATEMENTS);
  for (int i = 0; i < stmts->getChildNum(); i ++) {
    visitWhenStmt(g, stmts->getChild(i));
  }
}

Node* allocCondNode(ASTExpTree* condExp, PNode* when) {
  Node* cond = allocNode(NODE_OTHERS, prefixName(SEP_MODULE, "WHEN_COND_" + std::to_string(when->lineno)));
  cond->valTree = new ExpTree(condExp->getExpRoot(), cond);
  addSignal(cond->name, cond);
  return cond;
}

/*
| When expr ':' info INDENT statements DEDENT when_else   { $$ = newNode(P_WHEN, $2->lineno, $4, NULL, 3, $2, $6, $8); }
*/
void visitWhen(graph* g, PNode* when) {
  TYPE_CHECK(when, 3, 3, P_WHEN);
  ASTExpTree* condExp = visitExpr(g, when->getChild(0));
  Node* condNode = allocCondNode(condExp, when);
  // allocWhenId(when); distinguish when through condNode rather than id
  whenTrace.push_back(std::make_pair(true, condNode));
  visitWhenStmts(g, when->getChild(1));
  
  whenTrace.back().first = false;
  visitWhenStmts(g, when->getChild(2));
  
  whenTrace.pop_back();

}

/*
  | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
  | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
*/
void visitPrintf(graph* g, PNode* print) {
  TYPE_CHECK(print, 3, 3, P_PRINTF);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, print->name));
  ASTExpTree* exp = visitExpr(g, print->getChild(1));

  ENode* enode = new ENode(OP_PRINTF);
  enode->strVal = print->getExtra(0);

  enode->addChild(exp->getExpRoot());
  
  PNode* exprs = print->getChild(2);
  for (int i = 0; i < exprs->getChildNum(); i ++) {
    ASTExpTree* val = visitExpr(g, exprs->getChild(i));
    enode->addChild(val->getExpRoot());
  }

  n->valTree = new ExpTree(enode);
  addSignal(n->name, n);
}

/*
    | Assert '(' expr ',' expr ',' expr ',' String ')' ':' ALLID info { $$ = newNode(P_ASSERT, synlineno(), $13, $12, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' info { $$ = newNode(P_ASSERT, synlineno(), $11, NULL, 3, $3, $5, $7); $$->appendExtraInfo($9); }
*/
void visitAssert(graph* g, PNode* ass) {
  TYPE_CHECK(ass, 3, 3, P_ASSERT);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, ass->name));

  ASTExpTree* pred = visitExpr(g, ass->getChild(1));
  ASTExpTree* en = visitExpr(g, ass->getChild(2));

  ENode* enode = new ENode(OP_ASSERT);
  enode->strVal = ass->getExtra(0);

  enode->addChild(pred->getExpRoot());
  enode->addChild(en->getExpRoot());

  n->valTree = new ExpTree(enode);
  addSignal(n->name, n);
}

/*
statement: Wire ALLID ':' type info    { $$ = newNode(P_WIRE_DEF, $4->lineno, $5, $2, 1, $4); }
    | Reg ALLID ':' type ',' expr RegWith INDENT RegReset '(' expr ',' expr ')' info DEDENT { $$ = newNode(P_REG_DEF, $4->lineno, $15, $2, 4, $4, $6, $11, $13); }
    | memory    { $$ = $1;}
    | Inst ALLID Of ALLID info    { $$ = newNode(P_INST, synlineno(), $5, $2, 0); $$->appendExtraInfo($4); }
    | Node ALLID '=' expr info { $$ = newNode(P_NODE, synlineno(), $5, $2, 1, $4); }
    | reference "<=" expr info  { $$ = newNode(P_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
    | reference "<-" expr info  { TODO(); }
    | reference Is Invalid info { $$ = NULL; }
    | Attach '(' references ')' info { TODO(); }
    | When expr ':' info INDENT statements DEDENT when_else   { $$ = newNode(P_WHEN, $2->lineno, $4, NULL, 3, $2, $6, $8); }
    | Stop '(' expr ',' expr ',' INT ')' info   { TODO(); }
    | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
    | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' ':' ALLID info { $$ = newNode(P_ASSERT, synlineno(), $13, $12, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' info { $$ = newNode(P_ASSERT, synlineno(), $11, NULL, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Skip info { $$ = NULL; }
*/
void visitStmt(graph* g, PNode* stmt) {
  switch (stmt->type) {
    case P_WIRE_DEF: visitWireDef(g, stmt); break;
    case P_REG_DEF: visitRegDef(g, stmt); break;
    case P_INST: visitInst(g, stmt); break;
    case P_MEMORY: visitMemory(g, stmt); break;
    case P_NODE: visitNode(g, stmt); break;
    case P_CONNECT: visitConnect(g, stmt); break;
    case P_WHEN: visitWhen(g, stmt); break;
    case P_PRINTF: visitPrintf(g, stmt); break;
    case P_ASSERT: visitAssert(g, stmt); break;
    default: Panic();
  }
}

/*
statements: { $$ = new PNode(P_STATEMENTS, synlineno()); }
    | statements statement { $$ =  $1; $1->appendChild($2); }
*/
void visitStmts(graph* g, PNode* stmts) {
  TYPE_CHECK(stmts, 0, INT32_MAX, P_STATEMENTS);
  for (int i = 0; i < stmts->getChildNum(); i ++) {
    visitStmt(g, stmts->getChild(i));
  }
}

/*
  module: Module ALLID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, synlineno(), $4, $2, 2, $6, $7); }
  children: ports, statments
*/
void visitTopModule(graph* g, PNode* topModule) {
  TYPE_CHECK(topModule, 2, 2, P_MOD);
  visitTopPorts(g, topModule->getChild(0));
  visitStmts(g, topModule->getChild(1));
}

void updatePrevNext(Node* n) {
  switch (n->type) {
    case NODE_INP:
    case NODE_REG_SRC:
      Assert(!n->valTree, "valTree of %s should be empty", n->name.c_str());
      break;
    case NODE_REG_DST:
    case NODE_SPECIAL:
    case NODE_OUT:
    case NODE_MEM_MEMBER:
    case NODE_OTHERS:
      n->updateConnect();
      break;
/* should not exists in allSignals */
    // case NODE_L1_RDATA:
    case NODE_MEMORY:
    case NODE_READER:
    case NODE_WRITER:
    case NODE_READWRITER:
    case NODE_ARRAY_MEMBER:
    case NODE_INVALID:
    default: Panic();

  }
}


/*
  traverse the AST and generate graph
*/
graph* AST2Graph(PNode* root) {
  graph* g = new graph();
  g->name = root->name;

  PNode* topModule = NULL;

  for (int i = 0; i < root->getChildNum(); i++) {
    PNode* module = root->getChild(i);
    if (module->name == root->name) { topModule = module; }
    moduleMap[module->name] = module;
  }
  Assert(topModule, "Top module can not be NULL\n");
  visitTopModule(g, topModule);

  for (Node* reg : g->regsrc) {
    reg->addReset();
    /* set lvalue to regDst */
    if (reg->getSrc()->valTree->getlval()) {
      Assert(reg->getSrc()->valTree->getlval()->nodePtr, "lvalue in %s is not node", reg->name.c_str());
      reg->getSrc()->valTree->getlval()->nodePtr = reg->getDst();
    }
    reg->getDst()->valTree = reg->getSrc()->valTree;
    reg->getSrc()->valTree = NULL;
    for (ExpTree* tree : reg->getSrc()->arrayVal) {
      Assert(tree->getlval()->nodePtr, "lvalue in %s is not node", reg->name.c_str());
      tree->getlval()->nodePtr = reg->getDst();
    }
    reg->getDst()->arrayVal.insert(reg->getDst()->arrayVal.end(), reg->arrayVal.begin(), reg->arrayVal.end());
    reg->arrayVal.clear();
  }
  for (Node* memory : g->memory) {
    if (memory->rlatency != 0) continue;
    for (Node* port : memory->member) {
      if (port->type == NODE_WRITER) continue;
      if (port->type == NODE_READWRITER) TODO();
      ENode* enode = new ENode(OP_READ_MEM);

      port->get_member(READER_DATA)->valTree = new ExpTree(enode);
    }
  }

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    updatePrevNext(it->second);
    it->second->inferWidth();
  }

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    it->second->constructSuperNode();
  }
  /* must be called after constructSuperNode all finished */
  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    it->second->constructSuperConnect();
  }
  /* find all sources: regsrc, memory rdata, input, constant node */
  for (Node* reg : g->regsrc) {
    g->supersrc.insert(reg->super);
    // printf("reg %s super %d\n", reg->name.c_str(), reg->super->id);
  }
  for (Node* memory : g->memory) {
    if (memory->rlatency >= 1) {
      for (Node* port : memory->member) {
        if (port->type == NODE_READER) {
          g->supersrc.insert(port->get_member(READER_DATA)->super);
        }
      }
    }
  }
  for (Node* input : g->input) {
    g->supersrc.insert(input->super);
  }
  for (auto it : allSignals) {
    if (it.second->type == NODE_OTHERS && it.second->super->prev.size() == 0) {
      g->supersrc.insert(it.second->super);
    }
  }
  return g;
}