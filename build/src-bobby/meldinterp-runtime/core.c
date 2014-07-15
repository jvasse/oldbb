
#include "api.h"
#include "core.h"
#include "model.h"

#include "set_runtime.h"
#include "list_runtime.h"
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "../system/myassert.h"
#include <stdio.h>

#define DEBUG_INSTRS
/* #define DEBUG_ALLOCS */
//#define DEBUG_PROVED_TUPLES
/* #define DEBUG_MOVE */

#define inline 

static unsigned char **deltas = NULL;
int *delta_sizes = NULL;
unsigned char *arguments = NULL;
tuple_type TYPE_INIT = 0;
tuple_type TYPE_EDGE = -1;
tuple_type TYPE_COLOCATED = -1;
tuple_type TYPE_PROVED = -1;
tuple_type TYPE_TERMINATE = -1;

extern persistent_set *persistent;

int
queue_length (tuple_queue *queue)
{
  int i;
  tuple_entry *entry = queue->head;
  
  for (i = 0; entry != NULL; entry = entry->next, i++);

  return i;
}

/* EVAL FUNCTIONS */

inline 
void* eval_field (const unsigned char value, tuple_t tuple,
		  const unsigned char **pc, Register *reg)
{
  const unsigned char field_num = VAL_FIELD_NUM(*pc);
  (*pc) += 2;

  return GET_TUPLE_FIELD(tuple, field_num);
}

inline 
void* eval_reg(const unsigned char value, const unsigned char **pc, Register *reg)
{ 
  ++(*pc);
  return &(reg)[VAL_REG(value)]; 
}

inline 
void* eval_int (const unsigned char value,
		const unsigned char **pc, Register *reg)
{
  void *ret = (void *)(*pc);
  *pc += sizeof(meld_int);

  return ret;
}

inline 
void* eval_float (const unsigned char value,
		const unsigned char **pc, Register *reg)
{
  void *ret = (void *)(*pc);
  *pc += sizeof(meld_float);

  return ret;
}

inline 
void moveTupleToReg (const unsigned char reg_index, tuple_t tuple, Register *reg)
{
  Register *dst = &(reg)[VAL_REG(reg_index)];
  *dst = (Register)tuple;

#if 0
  printf ("--%d--\t MOVE %s to reg %d\n", getBlockId(),
	  tuple_names[TUPLE_TYPE(reg[reg_index])], reg_index);
#endif
}

/* END OF EVAL FUNCTIONS */

/* INSTR EXECUTION FUNCTIONS */

inline void
execute_alloc (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  tuple_type type = FETCH(pc++);  
  byte reg_index = FETCH(pc);
  tuple_t *dst = eval_reg (reg_index, &pc, reg);			
  *dst = ALLOC_TUPLE(TYPE_SIZE(type));

#if defined(DEBUG_INSTRS) || defined(DEBUG_ALLOCS)
  {
    printf ("--%d--\t ALLOC %s TO reg %d\n", 
	    getBlockId(), tuple_names[type], reg_index);
  }
#endif

  memset (*dst, 0, TYPE_SIZE(type));
  TUPLE_TYPE(*dst) = type;
}

inline void
execute_addpers (const unsigned char *pc, 
		 Register *reg, int isNew) 
{
  ++pc;  
  
  byte reg_index = FETCH(pc);  
  Register send_reg = reg[reg_index];

#ifdef DEBUG_INSTRS
  tuple_type type = TUPLE_TYPE((tuple_t)send_reg);
  printf ("--%d--\t ADDPERS reg %d: %s\n", 
	  getBlockId(), reg_index, tuple_names[type]);
#endif

  enqueueNewTuple((tuple_t)MELD_CONVERT_REG_TO_PTR(send_reg), 
		  (record_type) isNew);
}

inline void
execute_send (const unsigned char *pc,
	      Register *reg, int isNew)
{
  ++pc;
  Register send_reg = reg[SEND_MSG(pc)];
  Register send_rt = reg[SEND_RT(pc)];

#ifdef DEBUG_INSTRS
  printf("--%d--\t SEND reg %d TO reg %d\n", 
	 getBlockId(), SEND_MSG(pc), SEND_RT(pc));
#endif

  tuple_send((tuple_t)MELD_CONVERT_REG_TO_PTR(send_reg),
	     MELD_CONVERT_REG_TO_PTR(send_rt), 0, isNew);
}

inline void
execute_iter (const unsigned char *pc, 
	      Register *reg, int isNew)
{
  const unsigned char *inner_jump = pc + ITER_INNER_JUMP(pc);
  const tuple_type type = ITER_TYPE(pc);
  int i, k, length;
  void **list;
  int size = TYPE_SIZE(type);

  /* Reg in which match will be stored during execution*/
  byte reg_store_index = FETCH(pc+10);
			
  /* produce a random ordering for all tuples of the appropriate type */
			
  if(TYPE_IS_PERSISTENT(type) && !TYPE_IS_AGG(type)) {
    /* persistent aggregate types not supported */
    persistent_set *persistents = &PERSISTENT[type];
        
    length = persistents->current;
    list = malloc(sizeof(tuple_t) * length);

    for(i = 0; i < length; i++) {
      int j = random() % (i + 1);
          
      list[i] = list[j];
      list[j] = persistents->array + i * size;
    }
  } else {
    /* non-persistent type */
    tuple_entry *entry = TUPLES[type].head;
		    
    length = queue_length(&TUPLES[ITER_TYPE(pc)]);
    list = malloc(sizeof(tuple_t) * length);
		    
    for (i = 0; i < length; i++) {
      int j = random() % (i+1);

      list[i] = list[j];
      list[j] = entry->tuple;

      entry = entry->next;
    }
  }
			
#ifdef DEBUG_INSTRS
  printf("--%d--\t ITER %s len=%d\n",
	 getBlockId(), tuple_names[type], length);
#endif

  if(length == 0) {
    /* no need to execute any further code, just jump! */
    return;
  }

  /* iterate over all tuples of the appropriate type */
  void *next_tuple;
      
  for (i = 0; i < length; i++) {
    next_tuple = list[i];

    unsigned char matched = 1;
    unsigned char num_args = ITER_NUM_ARGS(pc);	
    const unsigned char *tmppc = pc + PERS_ITER_BASE;

    /* check to see if it matches */
    for (k = 0; k < num_args; ++k) {
      const unsigned char fieldnum = ITER_MATCH_FIELD(tmppc);
      const unsigned char fieldtype = TYPE_ARG_TYPE(type, fieldnum);
      const unsigned char type_size = TYPE_ARG_SIZE(type, fieldnum);
      const unsigned char value_type = ITER_MATCH_VAL(tmppc);

      Register *field = GET_TUPLE_FIELD(next_tuple, fieldnum);
      Register *val;      

      if (val_is_int (value_type)) {
	tmppc += 2;
	val = eval_int(value_type, &tmppc, reg);
      } else if (val_is_float (value_type)) {
	tmppc += 2;
	val = eval_float(value_type, &tmppc, reg);
      } else if (val_is_field (value_type)) {
	tmppc += 2;
	byte reg_index = FETCH(tmppc+1);
	tuple_t tpl = (tuple_t)reg[reg_index];
	val = eval_field(value_type, tpl, &tmppc, reg);
      }  else {
        /* Don't know what to do */
       	fprintf (stderr, "Type %d not support yet - don't know what to do.\n", fieldtype);
	assert (0);
	exit (2);
      }

      matched = matched && (memcmp(field, val, type_size) == 0);
    }

#ifdef DEBUG_INSTRS
    printf("--%d--\t MATCHED: %d | length: %d\n", getBlockId(),
	   matched, length);
#endif
          
    if (matched) {
      moveTupleToReg (reg_store_index, next_tuple, reg);
      if (RET_RET == tuple_process(next_tuple, inner_jump, isNew, reg)) {
	free(list);
	return;
      }
    }
  }

  free(list);

  /* process next instructions */
  return;
}

inline void
execute_rule (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  (void)tuple;
  (void)pc;
  (void)reg;
}

inline void
execute_rule_done (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  (void)tuple;
  (void)pc;
  (void)reg;
}

inline void
execute_mvintfield (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  Register *src = eval_int (FETCH(pc), &pc, reg);
  byte reg_index = FETCH(pc+1);
  byte field_num = FETCH(pc);
  
  tuple_t dst_tuple = (tuple_t)reg[reg_index];
  tuple_type type = TUPLE_TYPE(dst_tuple);

  Register *dst = eval_field (FETCH(pc), dst_tuple, &pc, reg); 


#ifdef DEBUG_INSTRS
  printf ("--%d--\t MOVE INT %d TO FIELD %d.%d\n", 
	  getBlockId(), MELD_INT(src), reg_index, field_num);
#endif
  
  size_t size = TYPE_ARG_SIZE(type, field_num);

  memcpy(dst, src, size);
}

inline void
execute_mvintreg (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;

  Register *src = eval_int (FETCH(pc), &pc, reg);
  byte reg_index = FETCH(pc);  
  Register *dst = eval_reg (reg_index, &pc, reg); 

#ifdef DEBUG_INSTRS
  printf ("--%d--\t MOVE INT %d TO reg %d\n", 
	  getBlockId(), MELD_INT(src), reg_index);
#endif

  size_t size = sizeof(Register);
  memcpy(dst, src, size);
}

inline void
execute_mvfieldreg (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  byte field_reg = FETCH(pc+1);    
  byte field_num = FETCH(pc);

  tuple_t tpl = (tuple_t)reg[field_reg];
  Register *src = eval_field (FETCH(pc), tpl, &pc, reg);
  
  byte reg_index = FETCH(pc);
  Register *dst = eval_reg (reg_index, &pc, reg); 

#ifdef DEBUG_INSTRS
  printf ("--%d--\t MOVE FIELD %d.%d TO reg %d\n", 
	  getBlockId(), field_reg, field_num,
	  reg_index);
#endif

#ifdef DEBUG_MOVE
  printf (
    "\nInstruction: mvfieldreg \n"
    "src = %#x\n"
    "dst = %#x\n"
    ,src, dst);
#endif

  size_t size = sizeof(Register);
  memcpy(dst, src, size);
}

inline void
execute_mvregfield (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;

  byte reg_index = FETCH(pc);
  Register *src = eval_reg (reg_index, &pc, reg); 
  
  byte field_reg = FETCH(pc+1);
  byte field_num = FETCH(pc);
  
  tuple_t field_tpl = (tuple_t)reg[field_reg];
  tuple_type type = TUPLE_TYPE(field_tpl);
  Register *dst = eval_field (field_num, field_tpl, &pc, reg); 

#ifdef DEBUG_INSTRS
  printf ("--%d--\t MOVE REG %d TO FIELD %d.%d\n", 
	  getBlockId(), reg_index, field_reg, field_num);
#endif

#ifdef DEBUG_MOVE
  printf (
    "\nInstruction: mvregfield\n"
    "src = %#x\n"
    "dst = %#x\n"
    ,src, dst);
#endif

  size_t size = TYPE_ARG_SIZE(type, field_num);

  memcpy(dst, src, size);
}

inline void
execute_mvfieldfield (tuple_t tuple, const unsigned char *pc, 
		      Register *reg) 
{
  ++pc;
  byte src_field_reg = FETCH(pc+1);
  byte src_field_num = FETCH(pc);

  tuple_t src_field_tpl = (tuple_t)reg[src_field_reg];
  Register *src = eval_field (src_field_num, src_field_tpl, 
			      &pc, reg);

  byte dst_field_reg = FETCH(pc+1);
  byte dst_field_num = FETCH(pc);

  tuple_t dst_field_tpl = (tuple_t)reg[dst_field_reg];
  tuple_type type = TUPLE_TYPE(dst_field_tpl);
  Register *dst = eval_field (dst_field_num, dst_field_tpl,
			      &pc, reg); 

#ifdef DEBUG_INSTRS
  printf ("--%d--\t MOVE FIELD %d.%d TO FIELD %d.%d\n", 
	  getBlockId(), src_field_reg, src_field_num, 
	  dst_field_reg, dst_field_num);
#endif

#ifdef DEBUG_MOVE
  printf (
    "\nInstruction: mvfieldfield\n"
    "src = %#x\n"
    "dst = %#x\n"
    ,src, dst);
#endif

  size_t size = TYPE_ARG_SIZE(type, dst_field_num);

  memcpy(dst, src, size);
}

inline void
execute_mvhostfield (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;

  Register *src = (void*)EVAL_HOST;

  byte field_reg = FETCH(pc+1);
  byte field_num = FETCH(pc);

  tuple_t field_tpl = (tuple_t)reg[field_reg];
  tuple_type type = TUPLE_TYPE(field_tpl);
  Register *dst = eval_field (field_num, field_tpl, 
			      &pc, reg); 

#ifdef DEBUG_INSTRS
  printf ("--%d--\t MOVE HOST TO FIELD %d.%d\n", 
	  getBlockId(), field_reg, field_num);
#endif

#ifdef DEBUG_MOVE
  printf (
    "\nInstruction: mvhostfield\n"
    "src = %#x\n"
    "dst = %#x\n"
    ,src, dst);
#endif

  size_t size = TYPE_ARG_SIZE(type, field_num);

  memcpy(dst, src, size);
}

inline void
execute_addrequal (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  byte reg1 = FETCH(pc);
  byte reg2 = FETCH(pc+1);
  byte reg3 = FETCH(pc+2);

  Register *arg1 = eval_reg (reg1, &pc, reg);
  Register *arg2 = eval_reg (reg2, &pc, reg);
  Register *dest = eval_reg (reg3, &pc, reg);
  *dest = (MELD_PTR(arg1) == MELD_PTR(arg2));
#ifdef DEBUG_INSTRS
  printf ("--%d--\t ADDR reg %d EQUAL reg %d TO reg %d\n", 
	  getBlockId(), reg1, reg2, reg3);
#endif
}

inline void
execute_intequal (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  byte reg1 = FETCH(pc);
  byte reg2 = FETCH(pc+1);
  byte reg3 = FETCH(pc+2);

  Register *arg1 = eval_reg (reg1, &pc, reg);
  Register *arg2 = eval_reg (reg2, &pc, reg);
  Register *dest = eval_reg (reg3, &pc, reg);
  *dest = (MELD_INT(arg1) == MELD_INT(arg2));
#ifdef DEBUG_INSTRS
  printf ("--%d--\t INT reg %d EQUAL reg %d TO reg %d\n", 
	  getBlockId(), reg1, reg2, reg3);
#endif
}

inline void
execute_intnotequal (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  byte reg1 = FETCH(pc);
  byte reg2 = FETCH(pc+1);
  byte reg3 = FETCH(pc+2);

  Register *arg1 = eval_reg (reg1, &pc, reg);
  Register *arg2 = eval_reg (reg2, &pc, reg);
  Register *dest = eval_reg (reg3, &pc, reg);
  *dest = (MELD_INT(arg1) != MELD_INT(arg2));
#ifdef DEBUG_INSTRS
  printf ("--%d--\t INT reg %d NOTEQUAL reg %d TO reg %d\n", 
	  getBlockId(), reg1, reg2, reg3);
#endif
}

inline void
execute_intgreater (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  byte reg1 = FETCH(pc);
  byte reg2 = FETCH(pc+1);
  byte reg3 = FETCH(pc+2);

  Register *arg1 = eval_reg (reg1, &pc, reg);
  Register *arg2 = eval_reg (reg2, &pc, reg);
  Register *dest = eval_reg (reg3, &pc, reg);
  *dest = (MELD_INT(arg1) > MELD_INT(arg2));
#ifdef DEBUG_INSTRS
  printf ("--%d--\t INT reg %d GREATER THAN reg %d TO reg %d\n", 
	  getBlockId(), reg1, reg2, reg3);
#endif
}

inline void
execute_intmod (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  byte reg1 = FETCH(pc);
  byte reg2 = FETCH(pc+1);
  byte reg3 = FETCH(pc+2);

  Register *arg1 = eval_reg (reg1, &pc, reg);
  Register *arg2 = eval_reg (reg2, &pc, reg);
  Register *dest = eval_reg (reg3, &pc, reg);
  *dest = (MELD_INT(arg1) % MELD_INT(arg2));
#ifdef DEBUG_INSTRS
  printf ("--%d--\t INT reg %d MOD reg %d TO reg %d\n", 
	  getBlockId(), reg1, reg2, reg3);
#endif

}
inline void
execute_intplus (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  byte reg1 = FETCH(pc);
  byte reg2 = FETCH(pc+1);
  byte reg3 = FETCH(pc+2);

  Register *arg1 = eval_reg (reg1, &pc, reg);
  Register *arg2 = eval_reg (reg2, &pc, reg);
  Register *dest = eval_reg (reg3, &pc, reg);
  *dest = (MELD_INT(arg1) + MELD_INT(arg2));
#ifdef DEBUG_INSTRS
  printf ("--%d--\t INT reg %d PLUS reg %d TO reg %d\n", 
	  getBlockId(), reg1, reg2, reg3);
#endif
}

inline void
execute_intminus (tuple_t tuple, const unsigned char *pc, Register *reg) 
{
  ++pc;
  
  byte reg1 = FETCH(pc);
  byte reg2 = FETCH(pc+1);
  byte reg3 = FETCH(pc+2);

  Register *arg1 = eval_reg (reg1, &pc, reg);
  Register *arg2 = eval_reg (reg2, &pc, reg);
  Register *dest = eval_reg (reg3, &pc, reg);
  *dest = (MELD_INT(arg1) - MELD_INT(arg2));
#ifdef DEBUG_INSTRS
  printf ("--%d--\t INT reg %d MINUS reg %d TO reg %d\n", 
	  getBlockId(), reg1, reg2, reg3);
#endif
}

inline void
execute_run_action (const unsigned char *pc, 
		    Register *reg, byte isNew) 
{
  ++pc;

  byte reg_index = FETCH(pc);

  tuple_t action_tuple = (tuple_t)reg[reg_index];
  tuple_type type = TUPLE_TYPE(action_tuple);
  
  if (isNew <= 0)
    fprintf (stderr, "Run action ERROR?! -- isNew <= 0\n");
  
  switch (type) {
  case TYPE_SETCOLOR:
    if (isNew <= 0) return;

#ifdef DEBUG_INSTRS
    printf ("--%d--\t RUN ACTION: %s(currentNode, %d, %d, %d, %d)\n", 
	    getBlockId(), tuple_names[type], 
	    MELD_INT(GET_TUPLE_FIELD(action_tuple, 0)),
	    MELD_INT(GET_TUPLE_FIELD(action_tuple, 1)),
	    MELD_INT(GET_TUPLE_FIELD(action_tuple, 2)),
	    MELD_INT(GET_TUPLE_FIELD(action_tuple, 3))); 
#endif
   
    setLEDWrapper(*(byte *)GET_TUPLE_FIELD(action_tuple, 0),
		  *(byte *)GET_TUPLE_FIELD(action_tuple, 1),
		  *(byte *)GET_TUPLE_FIELD(action_tuple, 2),
		  *(byte *)GET_TUPLE_FIELD(action_tuple, 3));
    FREE_TUPLE(action_tuple);
    return;
   
  case TYPE_SETCOLOR2:
    if (isNew <= 0) return;
#ifdef DEBUG_INSTRS
    printf ("--%d--\t RUN ACTION: %s(currentNode, %d)\n", 
    getBlockId(), tuple_names[type], 
    MELD_INT(GET_TUPLE_FIELD(action_tuple, 0))); 
#endif
   
    setColorWrapper(MELD_INT(GET_TUPLE_FIELD(action_tuple, 0)));
    
    FREE_TUPLE(action_tuple);
    return;
  }
}
 
/* END OF INSTR EXECUTION FUNCTIONS */
 
bool
queue_is_empty(tuple_queue *queue)
{
  return queue->head == NULL;
}

void
queue_push_tuple(tuple_queue *queue, tuple_entry *entry)
{
  if(queue->head == NULL)
    queue->head = queue->tail = entry;
  else {
    queue->tail->next = entry;
    queue->tail = entry;
  }
}

tuple_t
queue_pop_tuple(tuple_queue *queue)
{
  tuple_entry *entry = NULL;
  
  if (queue->head) {
    entry = queue->head;
    queue->head = queue->head->next;
    
    if (queue->head == NULL)
      queue->tail = NULL;
  }
  
  return entry;
}

static tuple_t
queue_dequeue_pos(tuple_queue *queue, tuple_entry **pos)
{
  tuple_entry *entry = *pos;
  tuple_entry *next = (*pos)->next;
  
  if (entry == queue->tail) {
    if(entry == queue->head)
      queue->tail = NULL;
    else
      queue->tail = (tuple_entry *)pos; /* previous */
  }
  
  *pos = next;
    
  tuple_t tuple = entry->tuple;
  free(entry);
  
  return tuple;
}

tuple_entry*
queue_enqueue(tuple_queue *queue, tuple_t tuple, record_type isNew)
{
  tuple_entry *entry = malloc(sizeof(tuple_entry));
  
  entry->tuple = tuple;
  entry->records = isNew;
  entry->next = NULL;
  
  queue_push_tuple(queue, entry);

  return entry;
}

tuple_t
queue_dequeue(tuple_queue *queue, int *isNew)
{
  tuple_entry *entry = queue_pop_tuple(queue);

  tuple_t tuple = entry->tuple;

  if(isNew)
    *isNew = entry->records.count;

  free(entry);

  return tuple;
}

tuple_pentry*
p_dequeue(tuple_pqueue *q)
{
  tuple_pentry *ret = q->queue;
  
  if(q->queue != NULL)
    q->queue = q->queue->next;
    
  return ret;
}

void
p_enqueue(tuple_pqueue *queue, meld_int priority, tuple_t tuple,
	  void *rt, record_type isNew)
{
  tuple_pentry *entry = malloc(sizeof(tuple_pentry));

  entry->tuple = tuple;
  entry->records = isNew;
  entry->priority = priority;
  entry->rt = rt;

  tuple_pentry **spot;
  for (spot = &(queue->queue);
       *spot != NULL &&
	 (*spot)->priority < priority;
       spot = &((*spot)->next));

  entry->next = *spot;
  *spot = entry;
}
    
void
init_deltas(void)
{
  int i;

  deltas = (unsigned char **)malloc(sizeof(unsigned char*)*NUM_TYPES);
  delta_sizes = (int *)malloc(sizeof(int)*NUM_TYPES);

  for (i = 0; i < NUM_TYPES; ++i) {
    delta_sizes[i] = TYPE_NODELTAS(i);
    deltas[i] = (unsigned char*)TYPE_DELTAS(i);
  }
}

static int type;
void
init_fields(void)
{
  size_t total = 2*NUM_TYPES;
  int i, j;
  
  for(i = 0; i < NUM_TYPES; ++i)
    total += TYPE_NOARGS(i) * 2;
  
  arguments = malloc(total);
  unsigned char *start = arguments + 2*NUM_TYPES;
  unsigned char offset, size;
  
  for(i = 0; i < NUM_TYPES; ++i) {
    arguments[i*2] = start - arguments; /* start */
    offset = 0;
    
    for(j = 0; j < TYPE_NOARGS(i); ++j) {
      type = TYPE_ARG_TYPE(i, j);
      switch (type) {

      case (int)FIELD_INT:
      case (int)FIELD_TYPE:
	size = sizeof(meld_int);
	break;

      case (int)FIELD_FLOAT:
	size = sizeof(meld_float);
	break;

      case (int)FIELD_ADDR:
	size = sizeof(NodeID);
	break;

      case (int)FIELD_LIST_INT:
      case (int)FIELD_LIST_FLOAT:
      case (int)FIELD_LIST_ADDR:
      case (int)FIELD_SET_INT:
      case (int)FIELD_SET_FLOAT:
      case (int)FIELD_STRING:
	size = sizeof(void*);
	break;

      default:
	assert(0);
	size = 0;
	break;
      }
      
      start[0] = size; /* argument size */
      start[1] = offset; /* argument offset */

      offset += size;
      start += 2;
    }
    arguments[i*2+1] = offset + TYPE_FIELD_SIZE; /* tuple size */
  }
}

void init_consts(void)
{
  tuple_type i;
  for (i = 0; i < NUM_TYPES; i++) {
    if(strcmp(TYPE_NAME(i), "edge") == 0)
      TYPE_EDGE = i;
    else if(strcmp(TYPE_NAME(i), "colocated") == 0)
      TYPE_COLOCATED = i;
    else if(strcmp(TYPE_NAME(i), "proved") == 0)
      TYPE_PROVED = i;
    else if(strcmp(TYPE_NAME(i), "terminate") == 0)
      TYPE_TERMINATE = i;
  }	
}

static inline
void *eval_dst(const unsigned char value,
	       const unsigned char **pc, Register *reg, size_t *size)
{
  if (VAL_IS_REG(value)) {
    *size = sizeof(Register);
    return &(reg)[VAL_REG(value)];
  } else if (VAL_IS_FIELD(value)) {
    int reg_index = VAL_FIELD_REG(*pc);
    int field_num = VAL_FIELD_NUM(*pc);
    tuple_t tuple = (tuple_t)MELD_CONVERT_REG_TO_PTR(reg[reg_index]);
    tuple_type type = TUPLE_TYPE(tuple);

    *size = TYPE_ARG_SIZE(type, field_num);

    (*pc) += 2;

    return GET_TUPLE_FIELD(tuple, field_num);
  } else if (VAL_IS_INT(value)) {
    assert(0);
  } else if (VAL_IS_FLOAT(value)) {
    assert(0);
  } else if(VAL_IS_TUPLE(value)) {
    assert(0);
  } else if(VAL_IS_HOST(value)) {
    assert(0);
  } else {
    assert(0 /* invalid value */ );
  }

  assert(0);
  return NULL;
}

static inline
void* eval(const unsigned char value, tuple_t tuple,
	   const unsigned char **pc, Register *reg)
{
  if (VAL_IS_REG(value)) {
    return (void*)&(reg[VAL_REG(value)]);
  }

  switch(value) {
  case VALUE_TYPE_FLOAT: {
    void *ret = (void *)(*pc);
    *pc = *pc + sizeof(meld_float);
    return ret;
    break;
  }

  case VALUE_TYPE_INT: {
    void *ret = (void *)(*pc);
    *pc = *pc + sizeof(meld_int);
    return ret;
    break;
  }

  case VALUE_TYPE_HOST: {
    return (void*)EVAL_HOST;
    break;
  }

  case VALUE_TYPE_REVERSE: {
    const int reg_index = VAL_FIELD_REG(*pc);
    const int field_num = VAL_FIELD_NUM(*pc);
    tuple_t tuple = (tuple_t)MELD_CONVERT_REG_TO_PTR(reg[reg_index]);

    (*pc) += 2;

#ifdef PARALLEL_MACHINE
    List *route = MELD_LIST(GET_TUPLE_FIELD(tuple, field_num));
    List *clone = list_copy(route);

    list_reverse_first(clone);

    thread_self()->reverse_list = clone;
    
    return (void *)&thread_self()->reverse_list;
#else
    return GET_TUPLE_FIELD(tuple, field_num);
#endif /* PARALLEL_MACHINE */
    break;
  }

  case VALUE_TYPE_TUPLE: {
    return (void*)tuple;
    break;
  }

  case VALUE_TYPE_FIELD: {
    const unsigned char reg_index = VAL_FIELD_REG(*pc);
    const unsigned char field_num = VAL_FIELD_NUM(*pc);
    tuple_t tuple = (tuple_t)MELD_CONVERT_REG_TO_PTR(reg[reg_index]);
    (*pc) += 2;

#ifdef DEBUG_INSTRS
    printf ("tuple = ");
    tuple_print(tuple, stdout);
    printf ("\n");
    printf ("tuple[%d] = %#x\n", field_num, MELD_INT(GET_TUPLE_FIELD(tuple, field_num)));
#endif

    return GET_TUPLE_FIELD(tuple, field_num);
    break;
  }
  }    

  assert(0);
  return NULL;
}

static inline
bool aggregate_accumulate(int agg_type, void *acc, void *obj, int count)
{
  switch (agg_type) {
  case AGG_SET_UNION_INT: {
    Set *set = MELD_SET(acc);
    set_int_insert(set, MELD_INT(obj));
    set_print(set);
    return false;
  }
  case AGG_SET_UNION_FLOAT: {
    Set *set = MELD_SET(acc);
    set_float_insert(set, MELD_FLOAT(obj));
    set_print(set);
    return false;
  }

  case AGG_FIRST:
    return false;

  case AGG_MAX_INT:
    if (MELD_INT(obj) > MELD_INT(acc)) {
      MELD_INT(acc) = MELD_INT(obj);
      return true;
    } else
      return false;

  case AGG_MIN_INT:
    if (MELD_INT(obj) < MELD_INT(acc)) {
      MELD_INT(acc) = MELD_INT(obj);
      return true;
    } else
      return false;
		
  case AGG_SUM_INT:
    MELD_INT(acc) += MELD_INT(obj) * count;
    return false;
			
  case AGG_MAX_FLOAT:
    if(MELD_FLOAT(obj) > MELD_FLOAT(acc)) {
      MELD_FLOAT(acc) = MELD_FLOAT(obj);
      return true;
    } else
      return false;
    
  case AGG_MIN_FLOAT:
    if(MELD_FLOAT(obj) < MELD_FLOAT(acc)) {
      MELD_FLOAT(acc) = MELD_FLOAT(obj);
      return true;
    } else
      return false;
    
  case AGG_SUM_FLOAT:
    MELD_FLOAT(acc) += MELD_FLOAT(obj) * (meld_float)count;
    return false;

  case AGG_SUM_LIST_INT: {
    List *result_list = MELD_LIST(acc);
    List *other_list = MELD_LIST(obj);

    if(list_total(result_list) != list_total(other_list)) {
      fprintf(stderr, "lists differ in size for accumulator AGG_SUM_LIST_INT:"
	      " %d vs %d\n", list_total(result_list), list_total(other_list));
      exit(1);
    }

    list_iterator it_result = list_get_iterator(result_list);
    list_iterator it_other = list_get_iterator(other_list);

    while(list_iterator_has_next(it_result)) {
      list_iterator_int(it_result) += list_iterator_int(it_other) * (meld_int)count;

      it_other = list_iterator_next(it_other);
      it_result = list_iterator_next(it_result);
    }
			
    return false;
  }
	  
  case AGG_SUM_LIST_FLOAT: {
    List *result_list = MELD_LIST(acc);
    List *other_list = MELD_LIST(obj);

    if(list_total(result_list) != list_total(other_list)) {
      fprintf(stderr, "lists differ in size for accumulator AGG_SUM_LIST_FLOAT: "
	      "%d vs %d\n", list_total(result_list), list_total(other_list));
      exit(1);
    }

    list_iterator it_result = list_get_iterator(result_list);
    list_iterator it_other = list_get_iterator(other_list);

    while(list_iterator_has_next(it_result)) {
      list_iterator_float(it_result) += list_iterator_float(it_other) * (meld_float)count;

      it_result = list_iterator_next(it_result);
      it_other = list_iterator_next(it_other);
    }
			
    return false;
  }
  }

  assert(0);
  while(1);
}

static inline bool
aggregate_changed(int agg_type, void *v1, void *v2)
{
  switch(agg_type) {
  case AGG_FIRST:
    return false;

  case AGG_MIN_INT:
  case AGG_MAX_INT:
  case AGG_SUM_INT:
    return MELD_INT(v1) != MELD_INT(v2);
    
  case AGG_MIN_FLOAT:
  case AGG_MAX_FLOAT:
  case AGG_SUM_FLOAT:
    return MELD_FLOAT(v1) != MELD_FLOAT(v2);

  case AGG_SET_UNION_INT:
  case AGG_SET_UNION_FLOAT: {
    Set *setOld = MELD_SET(v1);
    Set *setNew = MELD_SET(v2);

    if(!set_equal(setOld, setNew))
      return true;

    /* delete new set union */
    set_delete(setNew);
    return false;
  }
    break;

  case AGG_SUM_LIST_INT:
  case AGG_SUM_LIST_FLOAT: {
    List *listOld = MELD_LIST(v1);
    List *listNew = MELD_LIST(v2);

    if(!list_equal(listOld, listNew))
      return true;

    /* delete new list */
    list_delete(listNew);
    return false;
  }
    break;

  default:
    assert(0);
    return true;
  }

  assert(0);
  while(1);
}

static inline void
aggregate_seed(int agg_type, void *acc, void *start, int count, size_t size)
{
  switch(agg_type) {
  case AGG_FIRST:
    memcpy(acc, start, size);
    return;
  case AGG_MIN_INT:
  case AGG_MAX_INT:
    MELD_INT(acc) = MELD_INT(start);
    return;
  case AGG_SUM_INT:
    MELD_INT(acc) = MELD_INT(start) * count;
    return;
  case AGG_MIN_FLOAT:
  case AGG_MAX_FLOAT:
    MELD_FLOAT(acc) = MELD_FLOAT(start);
    return;
  case AGG_SUM_FLOAT:
    MELD_FLOAT(acc) = MELD_FLOAT(start) * count;
    return;
  case AGG_SET_UNION_INT: {
    Set *set = set_int_create();
    set_int_insert(set, MELD_INT(start));
    set_print(set);
    MELD_SET(acc) = set;
    return;
  }
  case AGG_SET_UNION_FLOAT: {
    Set *set = set_float_create();
    set_float_insert(set, MELD_FLOAT(start));
    set_print(set);
    MELD_SET(acc) = set;
    return;
  }
  case AGG_SUM_LIST_INT: {
    List *result_list = list_int_create();
    List *start_list = MELD_LIST(start);

    /* add values to result_list */
    list_iterator it;
    for(it = list_get_iterator(start_list); list_iterator_has_next(it);
	it = list_iterator_next(it))
      {
	meld_int total = list_iterator_int(it) * (meld_int)count;
	list_int_push_tail(result_list, total);
      }

    MELD_LIST(acc) = result_list;
    return;
  }
  case AGG_SUM_LIST_FLOAT: {
    List *result_list = list_float_create();
    List *start_list = MELD_LIST(start);

    /* add values to result_list */
    list_iterator it;
    for(it = list_get_iterator(start_list); list_iterator_has_next(it);
	it = list_iterator_next(it))
      {
	meld_float total = list_iterator_float(it) * (meld_float)count;
	list_float_push_tail(result_list, total);
      }

    MELD_LIST(acc) = result_list;
    return;
  }
  }

  assert(0);
  while(1);
}

static inline void
aggregate_free(tuple_t tuple, unsigned char field_aggregate,
	       unsigned char type_aggregate)
{
  switch(type_aggregate) {
  case AGG_FIRST:
  case AGG_MIN_INT:
  case AGG_MAX_INT:
  case AGG_SUM_INT:
  case AGG_MIN_FLOAT:
  case AGG_MAX_FLOAT:
  case AGG_SUM_FLOAT:
    /* nothing to do */
    break;

  case AGG_SET_UNION_INT:
  case AGG_SET_UNION_FLOAT:
    set_delete(MELD_SET(GET_TUPLE_FIELD(tuple, field_aggregate)));
    break;

  case AGG_SUM_LIST_INT:
  case AGG_SUM_LIST_FLOAT:
    list_delete(MELD_LIST(GET_TUPLE_FIELD(tuple, field_aggregate)));
    break;

  default:
    assert(0);
    break;
  }
}

static inline
void aggregate_recalc(tuple_entry *agg, Register *reg,
		      bool first_run)
{
  tuple_type type = TUPLE_TYPE(agg->tuple);

  tuple_entry *cur;
	
  int agg_type = AGG_AGG(TYPE_AGGREGATE(type));
  int agg_field = AGG_FIELD(TYPE_AGGREGATE(type));
  tuple_queue *agg_queue = agg->records.agg_queue;
  tuple_entry *agg_list = agg_queue->head;
  tuple_t tuple = agg_list->tuple;
  
  void* start = GET_TUPLE_FIELD(tuple, agg_field);

  /* make copy */
  size_t size = TYPE_ARG_SIZE(type, agg_field);
  void* accumulator = malloc(size);

  aggregate_seed(agg_type, accumulator, start, agg_list->records.count, size);
	
  /* calculate offsets to copy right side to aggregated tuple */
  size_t size_offset = TYPE_FIELD_SIZE + TYPE_ARG_OFFSET(type, agg_field) + TYPE_ARG_SIZE(type, agg_field);
  size_t total_copy = TYPE_SIZE(type) - size_offset;
  tuple_t target_tuple = NULL;
  
  if (total_copy > 0)
    target_tuple = tuple;

  for (cur = agg_list->next; cur != NULL; cur = cur->next) {
    if(aggregate_accumulate(agg_type, accumulator,
			    GET_TUPLE_FIELD(cur->tuple, agg_field), cur->records.count))
      target_tuple = cur->tuple;
  }

  void *acc_area = GET_TUPLE_FIELD(agg->tuple, agg_field);

  if(first_run)
    memcpy(acc_area, accumulator, size);
  else if (aggregate_changed(agg_type, acc_area, accumulator)) {
    tuple_process(agg->tuple, TYPE_START(type), -1, reg);
    aggregate_free(agg->tuple, agg_field, agg_type);
    memcpy(acc_area, accumulator, size);
    if (total_copy > 0) /* copy right side from target tuple */
      memcpy(((unsigned char *)agg->tuple) + size_offset, ((unsigned char *)target_tuple) + size_offset, total_copy);
    tuple_process(agg->tuple, TYPE_START(type), 1, reg);
  }

  free(accumulator);
}

static inline
void process_deltas(tuple_t tuple, tuple_type type, Register *reg)
{
  void *old = OLDTUPLES[type];
  
  if(old == NULL)
    return;
    
  OLDTUPLES[type] = NULL;

  int i;
  for(i = 0; i < DELTA_TOTAL(type); ++i) {
    int delta_type = DELTA_TYPE(type, i);
    int delta_pos = DELTA_POSITION(type, i);
    void *delta_tuple = ALLOC_TUPLE(TYPE_SIZE(delta_type));

    memcpy(delta_tuple, old, TYPE_SIZE(delta_type));
    TUPLE_TYPE(delta_tuple) = delta_type;

    void *field_delta = GET_TUPLE_FIELD(delta_tuple, delta_pos);
    void *field_old = GET_TUPLE_FIELD(old, delta_pos);
    void *field_new = GET_TUPLE_FIELD(tuple, delta_pos);

    switch(TYPE_ARG_TYPE(type, delta_pos)) {
    case FIELD_INT:
      MELD_INT(field_delta) = MELD_INT(field_new) - MELD_INT(field_old);
      break;
    case FIELD_FLOAT:
      MELD_FLOAT(field_delta) = MELD_FLOAT(field_new) - MELD_FLOAT(field_old);
      break;
    default:
      assert(0);
      break;
    }

    tuple_process(delta_tuple, TYPE_START(TUPLE_TYPE(delta_tuple)), 1, reg);
  }

  FREE_TUPLE(old);
}

static inline tuple_t
tuple_build_proved(tuple_type type, meld_int total)
{
  tuple_t tuple = tuple_alloc(TYPE_PROVED);
  meld_int type_int = (meld_int)type;
  
  SET_TUPLE_FIELD(tuple, 0, &type_int);
  SET_TUPLE_FIELD(tuple, 1, &total);
  
  return tuple;
}

void tuple_do_handle(tuple_type type,	tuple_t tuple, int isNew, Register *reg)
{
  if(TYPE_IS_PROVED(type)) {
    PROVED[type] += (meld_int)isNew;
    tuple_t _proved = tuple_build_proved(type, PROVED[type]);
#ifdef DEBUG_PROVED_TUPLES
    printf("New proved for tuple %s: %d\n", tuple_names[type], PROVED[type]);
#endif
    PUSH_NEW_TUPLE(_proved);
  } else if(type == TYPE_PROVED) {
    tuple_process(tuple, TYPE_START(type), isNew, reg);
    FREE_TUPLE(tuple);
    return;
  } else if(type == TYPE_TERMINATE) {
    FREE_TUPLE(tuple);
    TERMINATE_CURRENT();
    return;
  }
  
  if(!TYPE_IS_AGG(type) && TYPE_IS_PERSISTENT(type)) {
    persistent_set *persistents = &PERSISTENT[type];
    int i;
    int size = TYPE_SIZE(type);
    
    if(isNew < 0) {
      fprintf(stderr, "meld: persistent types can't be deleted\n");
      printf("type: %s\n", tuple_names[type]);
      exit(EXIT_FAILURE);
    }
    
    for(i = 0; i < persistents->total; ++i) {
      void *stored_tuple = persistents->array + i * size;
      
      if(memcmp(stored_tuple, tuple, size) == 0) {
        FREE_TUPLE(tuple);
        return;
      }
    }
    
    /* new tuple */
    if(persistents->total == persistents->current) {
      if(persistents->total == 0)
        persistents->total = PERSISTENT_INITIAL;
      else
        persistents->total *= 2;
        
      persistents->array = realloc(persistents->array, size * persistents->total);
    }
    
    memcpy(persistents->array + persistents->current * size, tuple, size);
    ++persistents->current;
    tuple_process(tuple, TYPE_START(type), isNew, reg);
    
    return;
  }
  
  if (!TYPE_IS_AGG(type) || TYPE_IS_LINEAR(type))
    {
      tuple_queue *queue = &TUPLES[type];
      tuple_entry** current;
      tuple_entry* cur;
		
      for (current = &queue->head;
	   *current != NULL;
	   current = &(*current)->next)
	{
	  cur = *current;

	  if (memcmp(cur->tuple,
		     tuple,
		     TYPE_SIZE(type)) == 0)
	    {
	      cur->records.count += isNew;

	      if (cur->records.count <= 0) {
		/* only process if it isn't linear */
		if (!TYPE_IS_LINEAR(type)) {
		  tuple_process(tuple, TYPE_START(TUPLE_TYPE(tuple)), -1, reg);
		  FREE_TUPLE(queue_dequeue_pos(queue, current));
		} else {
		  if(DELTA_WITH(type)) {
		    if(OLDTUPLES[type])
		      FREE_TUPLE(OLDTUPLES[type]);
                
		    OLDTUPLES[type] = queue_dequeue_pos(queue, current);
		  } else
		    FREE_TUPLE(queue_dequeue_pos(queue, current));
		}
	      }

	      FREE_TUPLE(tuple);

	      return;
	    }
	}

      // if deleting, return
      if (isNew <= 0) {
	FREE_TUPLE(tuple);
	return;
      }

      queue_enqueue(queue, tuple, (record_type) isNew);

      if(TYPE_IS_LINEAR(type) && DELTA_WITH(type))
	process_deltas(tuple, type, reg);

      tuple_process(tuple, TYPE_START(TUPLE_TYPE(tuple)), isNew, reg);

      return;
    }

  unsigned char type_aggregate = TYPE_AGGREGATE(type);
  unsigned char field_aggregate = AGG_FIELD(type_aggregate);

  tuple_entry **current;
  tuple_entry *cur;
  tuple_queue *queue = &(TUPLES[type]);
	
  for (current = &queue->head;
       (*current) != NULL;
       current = &(*current)->next)
    {
      cur = *current;
    
      size_t sizeBegin = TYPE_FIELD_SIZE + TYPE_ARG_OFFSET(type, field_aggregate);
      char *start = (char*)(cur->tuple);

      if(memcmp(start, tuple, sizeBegin))
	continue;

      /*
	size_t sizeOffset = sizeBegin + TYPE_ARG_SIZE(type, field_aggregate);
	size_t sizeEnd = TYPE_SIZE(type) - sizeOffset;

	if (memcmp(start + sizeOffset, (char*)tuple + sizeOffset, sizeEnd))
	continue;*/

      tuple_queue *agg_queue = cur->records.agg_queue;

      /* AGG_FIRST aggregate optimization */
      if(AGG_AGG(type_aggregate) == AGG_FIRST
	 && isNew > 0
	 && !queue_is_empty(agg_queue))
	{
	  FREE_TUPLE(tuple);
	  return;
	}

      tuple_entry** current2;
      tuple_entry* cur2;
		
      for (current2 = &agg_queue->head;
	   *current2 != NULL;
	   current2 = &(*current2)->next)
	{
	  cur2 = *current2;

	  if (memcmp(cur2->tuple, tuple, TYPE_SIZE(type)) == 0)
	    {
	      cur2->records.count += isNew;

	      if (cur2->records.count <= 0) {
		// remove it
		FREE_TUPLE(queue_dequeue_pos(agg_queue, current2));

		if (queue_is_empty(agg_queue)) {
		  /* aggregate is removed */
		  void *aggTuple = queue_dequeue_pos(queue, current);
						
		  /* delete queue */
		  free(agg_queue);

		  tuple_process(aggTuple, TYPE_START(TUPLE_TYPE(aggTuple)), -1, reg);
		  aggregate_free(aggTuple, field_aggregate, AGG_AGG(type_aggregate));
		  FREE_TUPLE(aggTuple);
		} else
		  aggregate_recalc(cur, reg, false);
	      } else
		aggregate_recalc(cur, reg, false);

	      FREE_TUPLE(tuple);
	      return;
	    }
	}

      // if deleting, return
      if (isNew <= 0) {
	FREE_TUPLE(tuple);
	return;
      }

      queue_enqueue(agg_queue, tuple, (record_type) isNew);
      aggregate_recalc(cur, reg, false);
		
      return;
    }

  // if deleting, return
  if (isNew <= 0) {
    FREE_TUPLE(tuple);
    return;
  }

  // So now we know we have a new tuple
  tuple_t tuple_cpy = ALLOC_TUPLE(TYPE_SIZE(type));
  memcpy(tuple_cpy, tuple, TYPE_SIZE(type));

  /* create aggregate queue */
  tuple_queue *agg_queue = malloc(sizeof(tuple_queue));
  
  queue_init(agg_queue);
  
  queue_enqueue(agg_queue, tuple, (record_type) isNew);
  tuple_entry *entry =
    queue_enqueue(&TUPLES[type], tuple_cpy, (record_type)agg_queue);

  aggregate_recalc(entry, reg, true);
  tuple_process(tuple, TYPE_START(type), isNew, reg);
}

int 
tuple_process(tuple_t tuple, const unsigned char *pc,
	      int isNew, Register *reg)
{
#ifdef DEBUG_INSTRS
  printf ("\n--%d--\tPROCESS %s\n", getBlockId(), 
	  tuple_names[TUPLE_TYPE(tuple)]);
#endif
  /* Move tuple to register 0 so it can be accessed */
  moveTupleToReg (0, tuple, reg);

  for (;;) {
  eval_loop:
    switch (*(const unsigned char*)pc) {
    case RETURN_INSTR: 		/* 0x0 */
      {
#ifdef DEBUG_INSTRS
	printf ("--%d--\tRETURN\n", getBlockId());
#endif
	return RET_RET;
      }

    case NEXT_INSTR: 		/* 0x1 */
      {
#ifdef DEBUG_INSTRS
	printf ("--%d--\t NEXT\n", getBlockId());	
#endif
	return RET_NEXT;
      }

    case PERS_ITER_INSTR: 		/* 0x02 */ 
      {
	const byte *npc = pc + ITER_OUTER_JUMP(pc);
	execute_iter (pc, reg, isNew);
	pc = npc; goto eval_loop;
      }

    case SEND_INSTR: 		/* 0x08 */ 
      {
	const byte *npc = pc + SEND_BASE;
	execute_send (pc, reg, isNew);
	pc = npc; goto eval_loop;
      }
      
    case RULE_INSTR: 		/* 0x10 */ 
      {
	/* TODO: Trigger rule byte code processing */
	const byte *npc = pc + RULE_BASE;
	byte rule_number = FETCH(++pc);
#ifdef DEBUG_INSTRS
	printf ("--%d--\t RULE %d\n", getBlockId(), rule_number);
#else
	(void)rule_number;
#endif	
	execute_rule (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case RULE_DONE_INSTR: 		/* 0x11 */
      {
#ifdef DEBUG_INSTRS
	printf ("--%d--\t RULE DONE\n", getBlockId());
#endif
	const byte *npc = pc + RULE_DONE_BASE;
	/* Nothing to be done */
	pc = npc; goto eval_loop;
      }
 
    case RETURN_LINEAR_INSTR:		/* 0xd0 */
#ifdef DEBUG_INSTRS
      printf ("--%d--\tRETURN LINEAR\n", getBlockId());
#endif       
      return RET_RET;

    case MVINTFIELD_INSTR: 		/* 0x1e */
      {
	const byte *npc = pc + MVINTFIELD_BASE;
	execute_mvintfield (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case MVINTREG_INSTR: 		/* 0x1f */
      {
	const byte *npc = pc + MVINTREG_BASE;
	execute_mvintreg (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case MVFIELDFIELD_INSTR: 		/* 0x21 */
      {
	const byte *npc = pc + MVFIELDFIELD_BASE;
	execute_mvfieldfield (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }
   
    case MVFIELDREG_INSTR: 		/* 0x22 */
      {
	const byte *npc = pc + MVFIELDREG_BASE;
	execute_mvfieldreg (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case MVREGFIELD_INSTR: 		/* 0x26 */
      {
	const byte *npc = pc + MVREGFIELD_BASE;
	execute_mvregfield (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case MVHOSTFIELD_INSTR: 		/* 0x28 */
      {
	const byte *npc = pc + MVHOSTFIELD_BASE;
	execute_mvhostfield (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case INTMINUS_INSTR: 		/* 0x3a */
      {
	const byte *npc = pc + OP_BASE;
	execute_intminus (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case ADDREQUAL_INSTR: 		/* 0x39 */
      {
	const byte *npc = pc + OP_BASE;
	execute_addrequal (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case INTEQUAL_INSTR: 		/* 0x3b */
      {
	const byte *npc = pc + OP_BASE;
	execute_intequal (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }
      
    case INTNOTEQUAL_INSTR: 		/* 0x3c */
      {
	const byte *npc = pc + OP_BASE;
	execute_intnotequal (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case INTPLUS_INSTR: 		/* 0x3d */
      {
	const byte *npc = pc + OP_BASE;
	execute_intplus (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case ALLOC_INSTR: 		/* 0x40 */
      {
	const byte *npc = pc + ALLOC_BASE;
	execute_alloc (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case INTGREATER_INSTR: 		/* 0x43 */
      {
	const byte *npc = pc + OP_BASE;
	execute_intgreater (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    case IF_INSTR: 		/* 0x60 */
      {
	const byte *npc = pc + IF_BASE;
	byte *base = (byte*)pc;
	++pc;
	byte reg_index = FETCH(pc);
	Register *if_reg = eval_reg (reg_index, &pc, reg);

	if (!(char)(*if_reg)) {
#ifdef DEBUG_INSTRS
	  printf ("--%d--\t IF (reg %d) -- Failed\n", 
		  getBlockId(), reg_index);
#endif

	  pc = base + IF_JUMP(pc); goto eval_loop;
	}
	/* else process if content */
#ifdef DEBUG_INSTRS
	printf ("--%d--\t IF (reg %d) -- Success\n", 
	  getBlockId(), reg_index);
#endif
	pc = npc; goto eval_loop;
      }

    case ADDPERS_INSTR: 		/* 0x78 */
      {
	const byte *npc = pc + ADDPERS_BASE;
	execute_addpers (pc, reg, isNew);
	pc = npc; goto eval_loop;
      }
      
    case RUNACTION_INSTR: 		/* 0x79 */
      {
	const byte *npc = pc + RUNACTION_BASE;
	execute_run_action (pc, reg, isNew);
	pc = npc; goto eval_loop;
      }

    case INTMOD_INSTR: 		/* 0x3d */
      {
	const byte *npc = pc + OP_BASE;
	execute_intmod (tuple, pc, reg);
	pc = npc; goto eval_loop;
      }

    default:
#ifdef DEBUG_INSTRS
      printf ("--%d--\t "
	      "INSTRUCTION NOT IMPLEMENTED YET: %#x %#x %#x %#x %#x\n", 
	      getBlockId(), 
	      (unsigned char)*pc, (unsigned char)*(pc+1), 
	      (unsigned char)*(pc+2), (unsigned char)*(pc+3), 
	      (unsigned char)*(pc+4));
#endif
      return RET_ERROR;
    }
  }
}
      
void
tuple_print(tuple_t tuple, FILE *fp)
{
  unsigned char tuple_type = TUPLE_TYPE(tuple);
  int j;

  fprintf(fp, "%s(", TYPE_NAME(tuple_type));
  for(j = 0; j < TYPE_NOARGS(tuple_type); ++j) {
    void *field = GET_TUPLE_FIELD(tuple, j);

    if (j > 0)
      fprintf(fp, ", ");

    switch(TYPE_ARG_TYPE(tuple_type, j)) {
    case FIELD_INT:
#ifndef BBSIM
      fprintf(fp, "%ld", MELD_INT(field));
#else
      fprintf(fp, "%d", MELD_INT(field));
#endif
      break;
    case FIELD_FLOAT:
      fprintf(fp, "%f", (double)MELD_FLOAT(field));
      break;
    case FIELD_ADDR:
      fprintf(fp, "%p", MELD_PTR(field));
      break;
    case FIELD_LIST_INT:
      fprintf(fp, "list_int[%d][%p]", list_total(MELD_LIST(field)),
	      MELD_LIST(field));
      break;
    case FIELD_LIST_FLOAT:
      fprintf(fp, "list_float[%d][%p]", list_total(MELD_LIST(field)),
	      MELD_LIST(field));
      break;
    case FIELD_LIST_ADDR:
      fprintf(fp, "list_addr[%p]", *(void **)field);
      break;
    case FIELD_SET_INT:
      fprintf(fp, "set_int[%d][%p]", set_total(MELD_SET(field)),
	      MELD_SET(field));
      break;
    case FIELD_SET_FLOAT:
      fprintf(fp, "set_float[%d][%p]", set_total(MELD_SET(field)),
	      MELD_SET(field));
      break;
    case FIELD_TYPE:
      fprintf(fp, "%s", TYPE_NAME(MELD_INT(field)));
      break;
    default:
      assert(0);
      break;
    }
  }
  fprintf(fp, ")");
}

void facts_dump(void)
{
  int i;

  for (i = 0; i < NUM_TYPES; i++) {
    // don't print fact types that don't exist
    if (TUPLES[i].head == NULL)
      continue;

    // don't print artificial tuple types
    /*
      if (tuple_names[i][0] == '_')
      continue;
    */

    fprintf(stderr, "tuple %s (type %d)\n", tuple_names[i], i);
    tuple_entry *tupleEntry;
    for (tupleEntry = TUPLES[i].head; tupleEntry != NULL; tupleEntry = tupleEntry->next) {
      fprintf(stderr, "  ");
      tuple_print(tupleEntry->tuple, stderr);
      if (TYPE_IS_AGG(i)) {
	fprintf(stderr, "\n    [[[");
	tuple_entry *tpE;
	for (tpE = tupleEntry->records.agg_queue->head;
	     tpE != NULL;
	     tpE = tpE->next) {
	  tuple_print(tpE->tuple, stderr);
	  fprintf(stderr, "x%d\n       ", tpE->records.count);
	}
	fprintf(stderr, "\b\b\b]]]\n");
      }
      else {
	fprintf(stderr, "x%d\n", tupleEntry->records.count);
      }
    }
  }
}

void
print_program_info(void)
{
  /* print program info */
  int i;
  for(i = 0; i < NUM_TYPES; ++i) {
    printf("Tuple (%s:%d:%d) ", tuple_names[i], i, TYPE_SIZE(i));
    
    printf("[");
    if(TYPE_IS_AGG(i))
      printf("agg");
    if(TYPE_IS_PERSISTENT(i))
      printf("per");
    if(TYPE_IS_LINEAR(i))
      printf("linear");
    if(TYPE_IS_ROUTING(i))
      printf("route");
    if(TYPE_IS_PROVED(i))
      printf("proved");
    printf("] ");
    
    printf("num_args:%d deltas:%d off:%d ; args(offset, arg_size): ",
	   TYPE_NOARGS(i), TYPE_NODELTAS(i), TYPE_OFFSET(i));
		
    int j;
    for (j = 0; j < TYPE_NOARGS(i); ++j) {
      printf(" %d:%d", TYPE_ARG_OFFSET(i, j), TYPE_ARG_SIZE(i, j));
    }
    printf("\n");
  }
}
