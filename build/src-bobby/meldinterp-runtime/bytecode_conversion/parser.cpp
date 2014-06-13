
#include "reader.hpp"
#include "parser.hpp"
#include "defs.hpp"
#include "instr.hpp"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>

using namespace std;

uint32_t major_version, minor_version;
size_t number_of_predicates;
size_t bytecode_offset;

std::vector<code_size_t> code_size;
std::vector<byte_code> pred_bytecode;
std::vector<predicate*> predicates;
std::vector<std::string> tuple_names;
std::vector<byte> types;

std::ofstream bbFile;

void print_program(const string& in_filename)
{
code_reader read(in_filename);
  
// Open and check output file
bbFile.open("rainbow.bb", ios::out);
if (!bbFile.is_open())
  {
throw load_file_error("rainbow.bb", std::string("could not open file"));
}
 
// read magic
uint32_t magic1, magic2;
read.read_type<uint32_t>(&magic1);
read.read_type<uint32_t>(&magic2);
if(magic1 != MAGIC1 || magic2 != MAGIC2)
  throw load_file_error(in_filename, "not a meld byte code file");
   
cout << "magic1: " << magic1 << " magic2: " << magic2 << endl;
cout << endl << "MELD BYTECODE FILE" << endl;
   
// read version
read.read_type<uint32_t>(&major_version);
read.read_type<uint32_t>(&minor_version);
if(!VERSION_AT_LEAST(0, 11))
  throw load_file_error(in_filename, string("unsupported byte code version"));

if(VERSION_AT_LEAST(0, 12))
  throw load_file_error(in_filename, string("unsupported byte code version"));

cout << "Bytecode version: "<< major_version << "." << minor_version << endl << endl;

// read number of predicates
byte num_preds;
read.read_type<byte>(&num_preds);
number_of_predicates = num_preds;
  
cout << "Number of predicates: " << (int)num_preds << endl;   

// skip nodes
uint_val num_nodes;
read.read_type<uint_val>(&num_nodes);
  
cout << "Number of nodes: " << num_nodes << endl;   
read.seek(num_nodes * node_obj_size);

cout << endl << "TYPES" << endl;

// read number of types
byte ntypes;
read.read_type<byte>(&ntypes);

cout << "Number of types: " << (int)ntypes << endl;   
for(size_t i(0); i < ntypes; ++i) {
types.push_back(get_type(read));
}

cout << endl << "IMPORTED / EXPORTED PREDICATES" << endl;

// read imported/exported predicates
uint32_t number_imported_predicates;

read.read_type<uint32_t>(&number_imported_predicates);
cout << "Number of imported predicates: " << number_imported_predicates << endl;   

for(uint32_t i(0); i < number_imported_predicates; ++i) {
cout << "Predicate " << i << ": " << endl;   
uint32_t size;
read.read_type<uint32_t>(&size);
cout << " size: " << size << endl;   
 
char buf_imp[size + 1];
read.read_any(buf_imp, size);
buf_imp[size] = '\0';

read.read_type<uint32_t>(&size);
char buf_as[size + 1];
read.read_any(buf_as, size);
buf_as[size] = '\0';

read.read_type<uint32_t>(&size);
char buf_file[size + 1];
read.read_any(buf_file, size);
buf_file[size] = '\0';

cout << " import " << buf_imp << " as " << buf_as << " from " << buf_file << endl;
}

uint32_t number_exported_predicates;
read.read_type<uint32_t>(&number_exported_predicates);
cout << "Number of exported predicates: " << number_exported_predicates << endl;   

for(uint32_t i(0); i < number_exported_predicates; ++i) {
cout << "Predicate " << i << ": " << endl;   
uint32_t str_size;
read.read_type<uint32_t>(&str_size);
cout << " str_size: " << str_size << endl;   

char buf[str_size + 1];
read.read_any(buf, str_size);
buf[str_size] = '\0';

cout << " export " << buf << endl;
}

// get number of args needed
byte n_args;
read.read_type<byte>(&n_args);

cout << endl << "Number of args needed by Meld program: "<< (int)n_args << endl;   

cout << endl << "RULE INFORMATION" << endl;

// get rule information
uint32_t n_rules;
read.read_type<uint32_t>(&n_rules);
cout << "Number of rules: "<< n_rules << endl;   
 
for(size_t i(0); i < n_rules; ++i) {
cout << "rule: " << i << endl;

// read rule string length
uint32_t rule_len;
read.read_type<uint32_t>(&rule_len);
cout << " length: " << rule_len << endl;

char str[rule_len + 1];
read.read_any(str, rule_len);
str[rule_len] = '\0';

cout << " string: " << str << endl;
}

cout << endl << "STRING CONSTANTS" << endl;

// read string constants
uint32_t num_strings;
read.read_type<uint32_t>(&num_strings);
cout << "Number of string constants: " << num_strings << endl;
  
for(uint32_t i(0); i < num_strings; ++i) {
cout << "String Const: " << i << endl;
    
uint32_t length;
read.read_type<uint32_t>(&length);
cout << "length: " << length << endl;
    
char str[length + 1];
read.read_any(str, length);
str[length] = '\0';

cout << "string: " << str << endl;
}	

cout << endl << "CONSTANTS" << endl;

// read constants code
uint32_t num_constants;
read.read_type<uint32_t>(&num_constants);
cout << "Number of constants: " << num_constants << endl;

for(uint_val i(0); i < num_constants; ++i) {
read_type_from_reader(read, true);
}

// read constants code
code_size_t const_code_size;
read.read_type<code_size_t>(&const_code_size);
cout << "Size of constants bytecode: " << const_code_size << endl;

byte_code const_code = new byte_code_el[const_code_size];
read.read_any(const_code, const_code_size);
 cout << "bytecode: " << const_code << endl;
 
 read_node_references(const_code, read);

 cout << endl << "FUNCTIONS" << endl;

 // get function code
 uint32_t n_functions;
 read.read_type<uint32_t>(&n_functions);
 cout << "Number of functions: " << n_functions << endl;

 for(uint32_t i(0); i < n_functions; ++i) {
   cout << "function " << i << endl;
 
   code_size_t fun_size;
   read.read_type<code_size_t>(&fun_size);
   cout << " size: " << fun_size << endl;

   byte_code fun_code(new byte_code_el[fun_size]);
   read.read_any(fun_code, fun_size);
   cout << "bytecode: " << hex << showbase << (int)*fun_code << endl;

   read_node_references(fun_code, read);
 }

 cout << endl << "EXTERNAL FUNCTIONS DEFINITIONS" << endl;

 // get external functions definitions
 uint32_t n_externs;
 read.read_type<uint32_t>(&n_externs);
 cout << "Number of external functions: " << n_externs << endl;

 for(uint32_t i(0); i < n_externs; ++i) {
   cout << "external function " << i << endl;

   uint32_t extern_id;
   read.read_type<uint32_t>(&extern_id);
   cout << " id: " << extern_id << endl;

   char extern_name[256];
   read.read_any(extern_name, sizeof(extern_name));
   cout << " name: " << extern_name << endl;

   char skip_filename[1024];
   read.read_any(skip_filename, sizeof(skip_filename));
   cout << " filename: " << skip_filename << endl;
    
   ptr_val skip_ptr;
   read.read_type<ptr_val>(&skip_ptr);

   uint32_t num_args;
   read.read_type<uint32_t>(&num_args);
   cout << " number of arguments: " << num_args << endl;
    
   cout << " return type: ";
   read_type_from_reader(read, true);

   if(num_args) {
     cout << " args: " << endl;
     for(uint32_t j(0); j != num_args; ++j) {
       cout << " ";
       read_type_from_reader(read, true);
     }
   }
 }

 cout << endl << "PREDICATE INFORMATION" << endl;

 // read predicate information
 size_t total_arguments = 0;
 strat_level MAX_STRAT_LEVEL = 0;

 for(size_t i(0); i < number_of_predicates; ++i) {
   cout << endl << "predicate " << i << endl;

   // get code size
   code_size_t pred_code_size;
   read.read_type<code_size_t>(&pred_code_size);
   code_size.push_back(pred_code_size);
   cout << " code size: " << code_size[i] << endl;
    
   // get predicate properties
   cout << " properties: ";
   byte prop;
   read.read_type<byte>(&prop);
   // predicate[i]

   if(prop & PRED_AGG)
     cout << "agg " << endl;
   // build aggregate info?
    
   if(prop & PRED_LINEAR) cout << "linear ";
   if(prop & PRED_ROUTE) cout << "route ";
   if(prop & PRED_REVERSE_ROUTE) cout << "reverse_route ";
   if(prop & PRED_ACTION) cout << "action ";
   if(prop & PRED_REUSED) cout << "reused ";
   if(prop & PRED_CYCLE) cout << "cycle ";
   cout << endl;

   // get aggregate information, if any
   byte agg;
   read.read_type<byte>(&agg);
   if(prop & PRED_AGG) {
     cout << " aggregate: " << endl;
     byte field = agg & 0xf;
     cout << "  field index: " << (int)field<< endl;
     byte type = ((0xf0 & agg) >> 4);
     cout << "  type: ";
     switch(type) {
     case AGG_FIRST: cout << "first" << endl; break;
     case AGG_MAX_INT: cout << "max_int" << endl; break;
     case AGG_MIN_INT: cout << "min_int" << endl; break;
     case AGG_SUM_INT: cout << "sum_int" << endl; break;
     case AGG_MAX_FLOAT: cout << "max_float" << endl; break;
     case AGG_MIN_FLOAT: cout << "min_float" << endl; break;
     case AGG_SUM_FLOAT: cout << "sum_float" << endl; break;
     case AGG_SUM_LIST_FLOAT: cout << "sum_list_float" << endl; break;
     }
   }
   
   // read stratification level
   byte level;
   read.read_type<byte>(&level);
   cout << " strat level: " << (int)level << endl;

   // read number of fields
   byte nfields;
   read.read_type<byte>(&nfields);
   cout << " number of fields: " << (int)nfields << endl;
   // add 1 to number of field to allow room for mandatory node type
   nfields++;

   // read argument types
   cout << " field types: ";
   std::vector<byte> arg_types;
   // Push back mandatory node arg
   arg_types.push_back(0x02);
   for(size_t i(0); i < nfields; ++i) {
     byte field_type_index;
     read.read_type<byte>(&field_type_index);
     byte field_type = types[field_type_index];
     cout << hex << showbase << (int)field_type << " "; 
     arg_types.push_back(field_type);
   }
   cout << endl;
    
   // read predicate name
   char name_buf[PRED_NAME_SIZE_MAX];
   read.read_any(name_buf, PRED_NAME_SIZE_MAX);
   tuple_names.push_back(string((const char*)name_buf));
   cout << " name: " << tuple_names[i] << endl;

   char buf_vec[PRED_AGG_INFO_MAX];
   read.read_any(buf_vec, PRED_AGG_INFO_MAX);
   char *buf = buf_vec;
   cout << " aggregate info (if any): ";
    
   if(prop & PRED_AGG) {
     if(buf[0] == PRED_AGG_LOCAL) {
       buf++;
       cout << "  local_agg" << endl;
     } else if(buf[0] == PRED_AGG_REMOTE) {
       buf++;
       cout << "  neighborhood_agg" << endl;
     } else if(buf[0] == PRED_AGG_REMOTE_AND_SELF) {
       buf++;
       cout << "  neighborhood_and_self_agg" << endl;
     } else if(buf[0] == PRED_AGG_IMMEDIATE) {
       buf++;
       cout << "  immediate_agg" << endl;
     } else if(buf[0] & PRED_AGG_UNSAFE) {
       buf++;
       cout << "  unsafe_agg" << endl;
     }
   } else cout << "  none" << endl;

   MAX_STRAT_LEVEL = (size_t)level + 1 > MAX_STRAT_LEVEL ? level + 1 : MAX_STRAT_LEVEL;
   total_arguments += nfields;
   std::vector<byte_code_el> _code;
   predicates.push_back(new predicate(prop, agg, level, nfields, 
				      i, arg_types, _code));
 }

 cout << endl << "PRIORITY INFO" << endl;

 // get global priority information
 byte global_info;
 read.read_type<byte>(&global_info);
	
 switch(global_info) {
 case 0x01: { // priority by predicate
   cout << "priority by predicate" << endl;
   cerr << "Not supported anymore" << endl;
 }
   break;
 case 0x02: { // normal priority
   cout << "normal priority" << endl;
   byte type(0x0);
   byte asc_desc;

   read.read_type<byte>(&type);
   cout << " type: field foat" << endl;

   read.read_type<byte>(&asc_desc);
   if(asc_desc & 0x01)
     cout << " order: asc" << endl;
   else
     cout << " order: desc" << endl;
    
   float_val initial_priority_value;
   read.read_type<float_val>(&initial_priority_value);
   cout << " initial priority value: " << initial_priority_value << endl;
 }
   break;
 case 0x03: { // data file
   cerr << "File wrongly appears as data file" << endl;
 }
   break;
 }

 cout << endl << "EXTRACTING PREDICATE BYTECODE..." << endl;

 // read predicate code
 for(size_t i(0); i < number_of_predicates; ++i) {
   const size_t size = code_size[i];
   byte_code current_bytecode = new byte_code_el[size];

   read.read_any(current_bytecode, size);
   pred_bytecode.push_back(current_bytecode);
    
   read_node_references(pred_bytecode[i], read);
 }

 cout << endl << "RULE CODE" << endl;  

 // read rules code
 uint32_t num_rules_code;
 read.read_type<uint32_t>(&num_rules_code);
 cout << "Number of rule codes: " << num_rules_code << endl;   

 for(size_t i(0); i < num_rules_code; ++i) {
   cout << endl << "rule " << i << endl;
   code_size_t code_size;
   byte_code code;
   read.read_type<code_size_t>(&code_size);
   cout << " code size: " << code_size << endl;

   code = new byte_code_el[code_size];
   read.read_any(code, code_size);
   read_node_references(code, read);
      
   byte is_persistent(0x0);
   read.read_type<byte>(&is_persistent);
   cout << " is persistent? : " << (bool)is_persistent << endl;
	    
   uint32_t num_preds;
   read.read_type<uint32_t>(&num_preds);
   cout << " number of included predicates: " << num_preds << endl;
   cout << " included predicate ids : ";
   for(size_t j(0); j < num_preds; ++j) {
     predicate_id id;
     read.read_type<predicate_id>(&id);
     cout << (int)id << ", ";
   }
   cout << endl;
 }

 cout << endl << "GENERATING BYTECODE HEADER..." << endl;
 generate_bytecode_header();

 cout << endl << "CONVERTING AND GENERATING BYTECODE INSTRUCTIONS..." << endl;
 for (size_t i(0); i < number_of_predicates; i++) {
   predicates[i]->bytecode_offset = bytecode_offset;
   cout << "PROCESS " << tuple_names[i] << ":" << endl;
   bytecode_offset += convert_and_generate_bytecode(pred_bytecode[i], i);
   cout << "DONE" << endl;
 }

 cout << endl << "BYTECODE OFFSETS: " << endl;
 for (size_t i(0); i < number_of_predicates; i++) {
   cout << dec << predicates[i]->bytecode_offset << " ";
 }
 cout << endl;

 cout << endl << "PRINTING PREDICATE DESCRIPTORS..." << endl;
 for (size_t i(0); i < number_of_predicates; i++) {
   print_predicate_descriptor(i);
 }

 cout << endl << "PRINTING BYTECODE..." << endl;
 for (size_t i(0); i < number_of_predicates; i++) {
   for (size_t j(0); 
	j < predicates[i]->code.size();
	j++) {
     bbFile << hex << showbase << (int)predicates[i]->code[j] << ", ";
   }
 }  
 // Close bytecode array
 bbFile << "};" << endl;
  
 cout << endl << "GENERATING TUPLE_NAMES LIST..." << endl;
 generate_tuple_names_list();

 cout << endl << "PRINTING EXTERN FUNCTIONS..." << endl;
 bbFile << "#include \"extern_functions.bbh\"" << endl;
 bbFile << "Register (*extern_functs[])() = {};" << endl; 
 bbFile << "int extern_functs_args[] = {};" << endl;
 
 cout << endl << "ALL TASKS DONE!" << endl;
}

byte
read_type_from_reader(code_reader& read, bool id)
{
  byte f;
  read.read_type<byte>(&f);

  // Return corresponding type in MPA format
  switch(f) {
    // New type
  case FIELD_BOOL: cout << " bool" << endl; return f; break;
  case FIELD_INT: cout << " int" << endl; return f; break;
  case FIELD_FLOAT: cout << " float" << endl; return f; break;
  case FIELD_NODE: cout << " node" << endl; return f; break;
  case FIELD_STRING: cout << " string" << endl; return f; break;
  case FIELD_LIST:  
    if (id){
      cout << " list" << endl;
      return f;
    }
    else {
      cout << " list of ";
      read_type_from_reader(read, false);
    }
    break;
  case FIELD_STRUCT: {
    if (id) {
      cout << " struct" << endl;
      return f;
    }
    else {
      byte size;
      read.read_type<byte>(&size);
      cout << " struct containing " << (int)size << " elements:" << endl;
      for(size_t i(0); i < size; ++i)
	cout << " item " << i << ": "; 
      read_type_from_reader(read, false);
    }
  }
  default:
    cerr << " Impossible field type" << endl;
  }
  return f;
}

void
read_node_references(byte_code code, code_reader& read)
{
  //cout << "NODE REFERENCES " << endl;
  uint_val size_nodes;
  read.read_type<uint_val>(&size_nodes);
  //cout << "Size of a nodes (?): " << size_nodes << endl;
    
  uint_val pos[size_nodes];
  read.read_type<uint_val>(pos, size_nodes);

  (void)code;
}

void
generate_tuple_names_list(void) 
{
  bbFile << endl << "char *tuple_names[] = {";
  for (size_t i(0); i < tuple_names.size(); i++) {
    bbFile << "\"" << tuple_names[i] << "\", ";
  }
  bbFile << "};" << endl << endl;
}

void
generate_bytecode_header(void)
{
  bbFile << endl << "const unsigned char meld_prog[] = {";

  /* Print number of predicates */
  bbFile << hex << showbase << number_of_predicates;

  /* Calculate and print offset to predicate descriptor for every predicate */
  size_t descriptor_start = 1 + number_of_predicates * sizeof(byte);
  size_t current_offset = descriptor_start;
  
  for (size_t i(0); i < number_of_predicates; i++) {
    // Assign offset to predicate object
    predicates[i]->desc_offset = current_offset;
    
    // Print it to bytecode array
    bbFile << " ," << hex << showbase << current_offset;

    // Increment current offset
    current_offset += PREDICATE_DESCRIPTOR_SIZE + predicates[i]->num_args + 
      DELTA_TYPE_FIELD_SIZE;
  }
  
  bytecode_offset = current_offset;
}

void
print_predicate_descriptor(const predicate_id id)
{   
  // Force printing of the 2 bytes of the offset
  bbFile << hex << showbase << (predicates[id]->bytecode_offset & 0x00ff) << " ,";
  bbFile << hex << showbase << 
    ((predicates[id]->bytecode_offset & 0xff00) >> 8) << " ,";

  // Type properties
  bbFile << hex << showbase << predicates[id]->properties << " ,";

  // Aggregate type
  bbFile << hex << showbase << predicates[id]->agg_type << " ,";
    
  // Stratification round
  bbFile << hex << showbase << predicates[id]->strat_round << " ,";

  // Number of arguments
  bbFile << hex << showbase << predicates[id]->num_args << " ,";

  // Number of deltas -- FORCED TO 0
  bbFile << 0 << " ,";

  // Print argument descriptor
  for (size_t j(0); j < predicates[id]->num_args; j++) {
    bbFile << hex << showbase << predicates[id]->num_args << " ,";
  }
}

byte
get_type(code_reader& read)
{
  byte t;
  read.read_type<byte>(&t);
 
  switch(t) {
  case FIELD_BOOL:
    cout << " NOT YET IMPLEMENTED" << endl;
    exit(2);
    break;
  case FIELD_INT:
    cout << " INT" << endl;
    return 0x0;
    break;
  case FIELD_FLOAT:
    cout << " FLOAT" << endl;
    return 0x01;
    break;
  case FIELD_NODE:
    cout << " NODE" << endl;
    return 0x02;
    break;
  case FIELD_STRING:
    cout << " NOT YET IMPLEMENTED" << endl;
    return 0xff;
    break;
  case FIELD_LIST: 
    {
      byte tl;
      read.read_type<byte>(&tl);

      switch(tl) {
      case FIELD_INT:
	cout << " INT LIST" << endl;
	return 0x03;
	break;
      case FIELD_FLOAT:
	cout << " FLOAT LIST" << endl;
	return 0x04;
	break;
      case FIELD_NODE:
	cout << " NODE LIST" << endl;
	return 0x05;
	break;
      default:
	cout << " NOT YET IMPLEMENTED" << endl;
	exit(2);
	  break;
      }
    }
  case FIELD_STRUCT: {
    cout << " NOT YET IMPLEMENTED" << endl;
    exit(2);
    break;
  }
  default:
    cout << "IMPOSSIBLE TYPE" << endl; 
    exit(2);
    break;
  }
}

size_t
convert_and_generate_bytecode(pcounter pc, predicate_id i)
{
  cout << endl;
  // Code_size to return
  size_t new_code_size = 0;

  // BYTECODE CONVERSION
#define JUMP_NEXT() goto eval_loop
#define CASE(INSTR) case INSTR:
#define JUMP(label, jump_offset) { const pcounter npc = pc + jump_offset; (void)npc;
#define COMPLEX_JUMP(label) {
#define ADVANCE() pc = npc; JUMP_NEXT();
#define ENDOP() }

  while(true)
    {
    eval_loop:
      switch(fetch(pc)) {
	CASE(RETURN_INSTR)
	  COMPLEX_JUMP(return_instr)
	  cout << "RETURN ";
	// Print 8-bit instruction 
	byte b1 = 0x00;
	predicates[i]->code.push_back(b1);
	cout << hex << showbase << (int)b1;
	cout << endl;
	new_code_size += 1;
	return new_code_size;
	ENDOP()
	  
	  CASE(NEXT_INSTR)
	  COMPLEX_JUMP(next_instr)
	  cout << "NEXT ";
	// Print 8-bit instruction
	byte b1 = 0x01;
	predicates[i]->code.push_back(b1);
	cout << hex << showbase << (int)b1;
	cout << endl;
	pc++;
	new_code_size += 1;
	JUMP_NEXT();
	ENDOP()

	  CASE(RETURN_LINEAR_INSTR)
	  COMPLEX_JUMP(return_linear)
	  cout << "RETURN LINEAR ";
	// Print 8-bit instruction 
	byte b1 = 0x00;
	predicates[i]->code.push_back(b1);
	cout << hex << showbase << (int)b1;
	new_code_size += 1;
	return new_code_size;
	ENDOP()
         
	  CASE(RETURN_DERIVED_INSTR)
	  COMPLEX_JUMP(return_derived)
	  cout << "RETURN ";
	// Print 8-bit instruction 
	byte b1 = 0x00;
	predicates[i]->code.push_back(b1);
	cout << hex << showbase << (int)b1;
	new_code_size += 1;
	return new_code_size;
	ENDOP()
         
	  CASE(RETURN_SELECT_INSTR)
	  COMPLEX_JUMP(return_select)
	  pc += return_select_jump(pc);
	JUMP_NEXT();
	ENDOP()
         
	  CASE(IF_INSTR)
	  JUMP(if_instr, IF_BASE)
	  // Write instruction
	  cout << "IF ";
	// first byte: if instr + reg
	byte b1 = 0x60 | (0x1f & (*(pc + instr_size)));
	predicates[i]->code.push_back(b1);
	cout << hex << showbase << (int)b1 << " "; 
	// if_jump (4 bytes)
	byte b2 = *(pc + instr_size + reg_val_size);
	byte b3 = *(pc + instr_size + reg_val_size + 1);
	predicates[i]->code.push_back(b2);
	cout << (int)b2 << " ";
	predicates[i]->code.push_back(b3);
	cout << (int)b3 << " ";
	// However only 2 bytes on MPA's, so ignore other 2 bytes
	cout << endl;
	new_code_size += 3;
	ADVANCE();
	ENDOP()

	  CASE(IF_ELSE_INSTR)
	  JUMP(if_else, IF_ELSE_BASE)
	  // if(!state.get_bool(if_reg(pc))) {
	  pc += if_else_jump_else(pc);
	JUMP_NEXT();
	// }
	ADVANCE();
	ENDOP()

	  CASE(JUMP_INSTR)
	  COMPLEX_JUMP(jump)
	  pc += jump_get(pc, instr_size) + JUMP_BASE;

	JUMP_NEXT();
        ENDOP()
         
	  CASE(END_LINEAR_INSTR)
	  COMPLEX_JUMP(end_linear)
	  return RETURN_END_LINEAR;
	ENDOP()
			
	  CASE(RESET_LINEAR_INSTR)
	  JUMP(reset_linear, RESET_LINEAR_BASE)
	  {
	    // const bool old_is_linear(state.is_linear);
               
	    // state.is_linear = false;
	    
	    // // return_type ret(execute(pc + RESET_LINEAR_BASE, state, 0, NULL, NULL));

	    // assert(ret == RETURN_END_LINEAR);
	    // (void)ret;

	    // state.is_linear = old_is_linear;
               
	    // pc += reset_linear_jump(pc);

	    // JUMP_NEXT();
	  }
	ADVANCE();
	ENDOP()
            
	  /*	  #define DECIDE_NEXT_ITER_INSTR()			\
		  if(ret == RETURN_LINEAR) return RETURN_LINEAR;		\
		  if(ret == RETURN_DERIVED && state.is_linear) return RETURN_DERIVED; \
		  pc += iter_outer_jump(pc);					\
		  JUMP_NEXT()*/

	  CASE(PERS_ITER_INSTR)
	  JUMP(pers_iter, PERS_ITER_BASE)
	  {
	    cout << "PERS_ITER handle later" << endl;
	  }
	new_code_size += 0;
	ADVANCE();
	ENDOP()

	  CASE(LINEAR_ITER_INSTR)
	  COMPLEX_JUMP(linear_iter)
	  {
	    // predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
	    // const reg_num reg(iter_reg(pc));
	    // match *mobj(retrieve_match_object(state, pc, pred, LINEAR_ITER_BASE));

	    // const return_type ret(execute_linear_iter(reg, mobj, pc + iter_inner_jump(pc), state, pred));

	    // DECIDE_NEXT_ITER_INSTR();
	  }
	ENDOP()

	  CASE(RLINEAR_ITER_INSTR)
	  COMPLEX_JUMP(rlinear_iter)
	  {
	    // predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
	    // const reg_num reg(iter_reg(pc));
	    // match *mobj(retrieve_match_object(state, pc, pred, RLINEAR_ITER_BASE));

	    // const return_type ret(execute_rlinear_iter(reg, mobj, pc + iter_inner_jump(pc), state, pred));

	    // DECIDE_NEXT_ITER_INSTR();
	  }
	ENDOP()

	  CASE(OPERS_ITER_INSTR)
	  COMPLEX_JUMP(opers_iter)
	  {
	    // predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
	    // const reg_num reg(iter_reg(pc));
	    // match *mobj(retrieve_match_object(state, pc, pred, OPERS_ITER_BASE));

	    // // const return_type ret(execute_opers_iter(reg, mobj, pc, pc + iter_inner_jump(pc), state, pred));

	    // DECIDE_NEXT_ITER_INSTR();
	  }
	ENDOP()

	  CASE(OLINEAR_ITER_INSTR)
	  COMPLEX_JUMP(olinear_iter)
	  {
	    // predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
	    // const reg_num reg(iter_reg(pc));
	    // match *mobj(retrieve_match_object(state, pc, pred, OLINEAR_ITER_BASE));

	    // // const return_type ret(execute_olinear_iter(reg, mobj, pc, pc + iter_inner_jump(pc), state, pred));

	    // DECIDE_NEXT_ITER_INSTR();
	  }
	ENDOP()

	  CASE(ORLINEAR_ITER_INSTR)
	  COMPLEX_JUMP(orlinear_iter)
	  {
	    // predicate *pred(theProgram->get_predicate(iter_predicate(pc)));
	    // const reg_num reg(iter_reg(pc));
	    // match *mobj(retrieve_match_object(state, pc, pred, ORLINEAR_ITER_BASE));

	    // // const return_type ret(execute_orlinear_iter(reg, mobj, pc, pc + iter_inner_jump(pc), state, pred));

	    // DECIDE_NEXT_ITER_INSTR();
	  }
	ENDOP()
            
	  CASE(REMOVE_INSTR)
	  JUMP(remove, REMOVE_BASE)
	  // execute_remove(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(UPDATE_INSTR)
	  JUMP(update, UPDATE_BASE)
	  // execute_update(pc, state);
	  ADVANCE()
	  ENDOP()
            
	  CASE(ALLOC_INSTR)
	  JUMP(alloc, ALLOC_BASE)
	  // Write instruction
	  cout << "ALLOC ";
	// get predicate id
	byte id = *(pc + instr_size);
	// get reg num		
	byte regNum = *(pc + instr_size + predicate_size);
	// Generate old 2-byte format: 
	// ALLOC + 5 first bits of type
	byte b1 = 0x40 | ((id & 0xf8) >> 3);
	// last 2 bits of type + dst_reg
	byte b2 = ((id & 0x03) << 6) | (regNum & 0x3f);
	// Print them
	predicates[i]->code.push_back(b1);
	cout << hex << showbase << (int)b1 << " ";
	predicates[i]->code.push_back(b2);
	cout << hex << showbase << (int)b2 << " ";
      	cout << endl;	  
	new_code_size += 2;
	ADVANCE()
	  ENDOP()
            
	  CASE(SEND_INSTR)
	  JUMP(send, SEND_BASE)
	  cout << "SEND ";
	// get tuple reg
	byte msg_reg = *(pc + instr_size);
	// get path reg
	byte path_reg = *(pc + instr_size + reg_val_size);
	// Write first byte: instr + first 2 bits of msg_reg
	byte b1 = 0x80 | ((msg_reg & 0x18) >> 3);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// Write second byte: last 3 bits of msg_reg + path_reg
	byte b2 = ((msg_reg & 0x07) << 5) | (path_reg & 0x1f);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write third byte: delay type
	byte b3 = 0x01;
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << " ";
	// Write bytes 4-7: delay = 0
	byte b4, b5, b6, b7;
	b4 = 0x0;
	predicates[i]->code.push_back(b4);
	cout << hex << (int)b4 << " ";
	b5 = 0x0;
	predicates[i]->code.push_back(b5);
	cout << hex << (int)b5 << " ";
	b6 = 0x0;
	predicates[i]->code.push_back(b6);
	cout << hex << (int)b6 << " ";
	b7 = 0x0;
	predicates[i]->code.push_back(b7);
	cout << hex << (int)b7 << " ";
       	cout << endl;
	new_code_size += 7;
	ADVANCE()
	  ENDOP()

	  CASE(ADDLINEAR_INSTR)
	  JUMP(addlinear, ADDLINEAR_BASE)
	  // execute_add_linear(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(ADDPERS_INSTR)
	  JUMP(addpers, ADDPERS_BASE)
	  cout << "ADDPERS ";
	/* ADDPERS is same thing as sending
	   sending allocated tuple to self on MPA's VM: */

	// get register in which tuple is stored
	byte reg_num = *(pc + instr_size);
	// Format SEND to self instruction
	// msg_reg = path_reg if (to_self)
	byte msg_reg = reg_num;
	byte path_reg = reg_num;
	// Write first byte: instr + first 2 bits of msg_reg
	byte b1 = 0x80 | ((msg_reg & 0x18) >> 3);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// Write second byte: last 3 bits of msg_reg + path_reg
	byte b2 = ((msg_reg & 0x07) << 5) | (path_reg & 0x1f);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write third byte: delay type
	byte b3 = 0x01;		// int
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << " ";
	// Write bytes 4-7: delay = 0
	byte b4, b5, b6, b7;
	b4 = 0x0;
	predicates[i]->code.push_back(b4);
	cout << hex << (int)b4 << " ";
	b5 = 0x0;
	predicates[i]->code.push_back(b5);
	cout << hex << (int)b5 << " ";
	b6 = 0x0;
	predicates[i]->code.push_back(b6);
	cout << hex << (int)b6 << " ";
	b7 = 0x0;
	predicates[i]->code.push_back(b7);
	cout << endl;
	new_code_size += 7;
	ADVANCE()
	  ENDOP()

	  CASE(RUNACTION_INSTR)
	  JUMP(runaction, RUNACTION_BASE)
	  // Write instruction
	  cout << "RUNACTION ";
	/* ON MPA's VM, run action is done by sending action tuple to self: */
	// get register in which tuple is
	byte reg_num = *(pc + instr_size);
	// Format SEND to self instruction
	// msg_reg = path_reg if (to_self)
	byte msg_reg = reg_num;
	byte path_reg = reg_num;
	// Write first byte: instr + first 2 bits of msg_reg
	byte b1 = 0x80 | ((msg_reg & 0x18) >> 3);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// Write second byte: last 3 bits of msg_reg + path_reg
	byte b2 = ((msg_reg & 0x07) << 5) | (path_reg & 0x1f);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write third byte: delay type
	byte b3 = 0x01;		// int
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << " ";
	// Write bytes 4-7: delay = 0
	byte b4, b5, b6, b7;
	b4 = 0x0;
	predicates[i]->code.push_back(b4);
	cout << hex << (int)b4 << " ";
	b5 = 0x0;
	predicates[i]->code.push_back(b5);
	cout << hex << (int)b5 << " ";
	b6 = 0x0;
	predicates[i]->code.push_back(b6);
	cout << hex << (int)b6 << " ";
	b7 = 0x0;
	predicates[i]->code.push_back(b7);
       	cout << endl;
	new_code_size += 7;
	ADVANCE()
	  ENDOP()

	  CASE(ENQUEUE_LINEAR_INSTR)
	  JUMP(enqueue_linear, ENQUEUE_LINEAR_BASE)
	  // execute_enqueue_linear(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(SEND_DELAY_INSTR)
	  JUMP(send_delay, SEND_DELAY_BASE)
	  // execute_send_delay(pc, state);
	  ADVANCE()
	  ENDOP()
            
	  CASE(NOT_INSTR)
	  JUMP(not_instr, NOT_BASE)
	  // execute_not(pc, state);
	  ADVANCE()
	  ENDOP()
            
	  CASE(TESTNIL_INSTR)
	  JUMP(testnil, TESTNIL_BASE)
	  // execute_testnil(pc, state);
	  ADVANCE()
	  ENDOP()
            
	  CASE(FLOAT_INSTR)
	  JUMP(float_instr, FLOAT_BASE)
	  // execute_float(pc, state);
	  ADVANCE()
	  ENDOP()
            
	  CASE(SELECT_INSTR)
	  COMPLEX_JUMP(select)
	  // pc = execute_select(pc, state);
	  JUMP_NEXT();
	ENDOP()
            
	  CASE(DELETE_INSTR)
	  JUMP(delete_instr, DELETE_BASE + instr_delete_args_size(pc + DELETE_BASE, delete_num_args(pc)))
	  //execute_delete(pc, state);
	  ADVANCE()
	  ENDOP()
            
	  CASE(CALL_INSTR)
	  JUMP(call, CALL_BASE + call_num_args(pc) * reg_val_size)
	  // execute_call(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(CALL0_INSTR)
	  JUMP(call0, CALL0_BASE)
	  // execute_call0(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(CALL1_INSTR)
	  JUMP(call1, CALL1_BASE)
	  // execute_call1(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(CALL2_INSTR)
	  JUMP(call2, CALL2_BASE)
	  // execute_call2(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(CALL3_INSTR)
	  JUMP(call3, CALL3_BASE)
	  // execute_call3(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(RULE_INSTR)
	  JUMP(rule, RULE_BASE)
	  const size_t rule_id(rule_get_id(pc));
	cout << "RULE " << dec << rule_id << endl;
	// execute_rule(pc, state);
	new_code_size += 0;
	ADVANCE()
	  ENDOP()

	  CASE(RULE_DONE_INSTR)
	  JUMP(rule_done, RULE_DONE_BASE)
	  // Bytecode instruction with no effect, ignore
	  cout << "RULE DONE" << endl;
	new_code_size += 0;
	ADVANCE()
	  ENDOP()

	  CASE(NEW_NODE_INSTR)
	  JUMP(new_node, NEW_NODE_BASE)
	  // execute_new_node(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(NEW_AXIOMS_INSTR)
	  JUMP(new_axioms, new_axioms_jump(pc))
	  // execute_new_axioms(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(PUSH_INSTR)
	  JUMP(push, PUSH_BASE)
	  // state.stack.push();
	  ADVANCE()
	  ENDOP()

	  CASE(PUSHN_INSTR)
	  JUMP(pushn, PUSHN_BASE)
	  // state.stack.push(push_n(pc));
	  ADVANCE()
	  ENDOP()

	  CASE(POP_INSTR)
	  JUMP(pop, POP_BASE)
	  // state.stack.pop();
	  ADVANCE()
	  ENDOP()

	  CASE(PUSH_REGS_INSTR)
	  JUMP(push_regs, PUSH_REGS_BASE)
	  // state.stack.push_regs(state.regs);
	  ADVANCE()
	  ENDOP()

	  CASE(POP_REGS_INSTR)
	  JUMP(pop_regs, POP_REGS_BASE)
	  // state.stack.pop_regs(state.regs);
	  ADVANCE()
	  ENDOP()

	  CASE(CALLF_INSTR)
	  COMPLEX_JUMP(callf)
	  {
	    // const vm::callf_id id(callf_get_id(pc));
	    // function *fun(theProgram->get_function(id));

	    // pc = fun->get_bytecode();
	    JUMP_NEXT();
	  }
	ENDOP()

	  CASE(MAKE_STRUCTR_INSTR)
	  JUMP(make_structr, MAKE_STRUCTR_BASE)
	  // execute_make_structr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MAKE_STRUCTF_INSTR)
	  JUMP(make_structf, MAKE_STRUCTF_BASE)
	  // execute_make_structf(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(STRUCT_VALRR_INSTR)
	  JUMP(struct_valrr, STRUCT_VALRR_BASE)
	  // execute_struct_valrr(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(STRUCT_VALFR_INSTR)
	  JUMP(struct_valfr, STRUCT_VALFR_BASE)
	  // execute_struct_valfr(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(STRUCT_VALRF_INSTR)
	  JUMP(struct_valrf, STRUCT_VALRF_BASE)
	  // execute_struct_valrf(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(STRUCT_VALRFR_INSTR)
	  JUMP(struct_valrfr, STRUCT_VALRFR_BASE)
	  // execute_struct_valrfr(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(STRUCT_VALFF_INSTR)
	  JUMP(struct_valff, STRUCT_VALFF_BASE)
	  // execute_struct_valff(pc, state);
	  ADVANCE()
	  ENDOP()
	  CASE(STRUCT_VALFFR_INSTR)
	  JUMP(struct_valffr, STRUCT_VALFFR_BASE)
	  // execute_struct_valffr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVINTFIELD_INSTR)
	  JUMP(mvintfield, MVINTFIELD_BASE)
	  // Write instruction
	  cout << "MVINTFIELD ";
	// Set 1st byte for instr + first 4 bits of type of first arg
	byte b1 = 0x30 | ((0x01 & 0x3c) >> 2);
	predicates[i]->code.push_back(b1);
	cout << (int)b1 << " ";
	// Set 2nd byte for last 2 bits of 1st arg + 2nd 
	byte b2 = ((0x01 & 0x03) << 6) | (0x02 & 0x3f);
	predicates[i]->code.push_back(b2);
	cout << (int)b2 << " ";
	// int32_t value
	byte b3 = *(pc + instr_size);
	byte b4 = *(pc + instr_size + 1);
	predicates[i]->code.push_back(b3);
	cout << hex << showbase << (int)b3 << " ";
	predicates[i]->code.push_back(b4);
	cout << hex << showbase << (int)b4 << " ";
	// field num 1 and reg index
	byte b5 = *(pc + instr_size + int_size);
	byte b6 = *(pc + instr_size + int_size + 1);
	predicates[i]->code.push_back(b5);
	cout << hex << showbase << (int)b5 << " ";
	predicates[i]->code.push_back(b6);
	cout << hex << showbase << (int)b6 << " ";
	cout << endl;
	new_code_size += 6;
	ADVANCE()
	  ENDOP()

	  CASE(MVINTREG_INSTR)
	  JUMP(mvintreg, MVINTREG_BASE)
	  // Write instruction
	  cout << "MVINTREG ";
	// Set 1st byte for instr + first 4 bits of type of first arg
	byte b1 = 0x30 | ((0x01 & 0x3c) >> 2);
	predicates[i]->code.push_back(b1);
	cout << (int)b1 << " ";
	// Set 2nd byte for last 2 bits of 1st arg + reg?bit + 2nd 
	byte dst_reg = *(pc + instr_size + int_size);
	byte b2 = ((0x01 & 0x03) << 6) | 0x20 | (dst_reg & 0x1f);
	predicates[i]->code.push_back(b2);
	cout << (int)b2 << " ";
	// int32_t value
	byte b3 = *(pc + instr_size);
	byte b4 = *(pc + instr_size + 1);
	predicates[i]->code.push_back(b3);
	cout << hex << showbase << (int)b3 << " ";
	predicates[i]->code.push_back(b4);
	cout << hex << showbase << (int)b4 << " ";	cout << endl;
	new_code_size += 4;
	ADVANCE()
	  ENDOP()

	  CASE(MVFIELDFIELD_INSTR)
	  JUMP(mvfieldfield, MVFIELDFIELD_BASE)
	  // Write instruction
	  cout << "MVFIELDFIELD ";
	// Set 1st byte for instr + 4 bits oftype of first arg
	byte b1 = 0x30 | ((0x02 & 0x3c) >> 2);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// Set 2nd byte for last 2 bits of 1st type + 2nd type 
	byte b2 = ((0x02 & 0x03) << 6) | (0x02 & 0x3f);
	predicates[i]->code.push_back(b2);
	cout << (int)b2 << " ";	
	// field num 1 and reg index 1
	byte b3 = *(pc + instr_size + int_size);
	byte b4 = *(pc + instr_size + int_size + 1);
	predicates[i]->code.push_back(b3);
	cout << hex << showbase << (int)b3 << " ";
	predicates[i]->code.push_back(b4);
	cout << hex << showbase << (int)b4 << " ";
	// field num 2 and reg index 2
	byte b5 = *(pc + instr_size + int_size);
	byte b6 = *(pc + instr_size + int_size + 1);
	predicates[i]->code.push_back(b5);
	cout << hex << showbase << (int)b5 << " ";
	predicates[i]->code.push_back(b6);
	cout << hex << showbase << (int)b6 << " ";
	cout << endl;
	new_code_size += 6;
	ADVANCE()
	  ENDOP()

	  CASE(MVFIELDFIELDR_INSTR)
	  JUMP(mvfieldfieldr, MVFIELDFIELD_BASE)
	  // execute_mvfieldfieldr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVFIELDREG_INSTR)
	  JUMP(mvfieldreg, MVFIELDREG_BASE)
	  // Write instruction
	  cout << "MVFIELDREG ";
	// Set 1st byte for instr + field type
	byte b1 = 0x30 | ((0x02 & 0x3c) >> 2);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// Set 2nd byte for last 2 bits of 1st field type + reg?bit + reg 
	byte dst_reg = *(pc + instr_size + field_size);
	byte b2 = ((0x02 & 0x03) << 6)  | 0x20 | (dst_reg & 0x1f);
	predicates[i]->code.push_back(b2);
	cout << (int)b2 << " ";	
	// field num and reg index
	byte b3 = *(pc + instr_size + int_size);
	byte b4 = *(pc + instr_size + int_size + 1);
	predicates[i]->code.push_back(b3);
	cout << hex << showbase << (int)b3 << " ";
	predicates[i]->code.push_back(b4);
	cout << hex << showbase << (int)b4 << " ";
	cout << endl;
	new_code_size += 4;
	ADVANCE()
	  ENDOP()

	  CASE(MVPTRREG_INSTR)
	  JUMP(mvptrreg, MVPTRREG_BASE)
	  // execute_mvptrreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVNILFIELD_INSTR)
	  JUMP(mvnilfield, MVNILFIELD_BASE)
	  // execute_mvnilfield(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVNILREG_INSTR)
	  JUMP(mvnilreg, MVNILREG_BASE)
	  // execute_mvnilreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVREGFIELD_INSTR)
	  JUMP(mvregfield, MVREGFIELD_BASE)
	  cout << "MVREGFIELD";
	// Get reg num
	byte reg = *(pc + instr_size);
	// Set 1st byte for instr + reg?bit + first 3 bits of reg
	byte b1 = 0x30 | 0x08 | ((reg & 0x1C) >> 2);
	cout << hex << (int)b1 << " ";
	predicates[i]->code.push_back(b1);
	// Set 2nd byte for last 2 bits of 1st arg + 2nd type (field) 
	byte b2 = 0x02 | ((reg & 0x03) << 6);
	predicates[i]->code.push_back(b2);
	cout << (int)b2 << " ";	
	// Get field num
	byte b3 = *(pc + instr_size + reg_val_size);
	// Get reg index
	byte b4 = *(pc + instr_size + reg_val_size + 1);
	predicates[i]->code.push_back(b3);
	cout << hex << showbase << (int)b3 << " ";
	predicates[i]->code.push_back(b4);
	cout << hex << showbase << (int)b4 << " ";
	new_code_size += 4;
	cout << endl;
	ADVANCE()
	  ENDOP()

	  CASE(MVREGFIELDR_INSTR)
	  JUMP(mvregfieldr, MVREGFIELD_BASE)
	  // execute_mvregfieldr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVHOSTFIELD_INSTR)
	  JUMP(mvhostfield, MVHOSTFIELD_BASE)
	  cout << "MVHOSTFIELD";
	// Set 1st byte for instr + type of first arg
	byte b1 = 0x30;
	cout << hex << (int)b1 << " ";
	// Set 2nd byte for last 2 bits of 1st arg + 2nd type 
	byte b2 = 0xC2;
	cout << (int)b2 << " ";	
	// No field for host, host means blockID
	// field num and reg index
	byte b3 = *(pc + instr_size + int_size);
	byte b4 = *(pc + instr_size + int_size + 1);
	predicates[i]->code.push_back(b3);
	cout << hex << showbase << (int)b3 << " ";
	predicates[i]->code.push_back(b4);
	cout << hex << showbase << (int)b4 << " ";
	cout << endl;
	new_code_size += 4;
	ADVANCE()
	  ENDOP()

	  CASE(MVREGCONST_INSTR)
	  JUMP(mvregconst, MVREGCONST_BASE)
	  // execute_mvregconst(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVCONSTFIELD_INSTR)
	  JUMP(mvconstfield, MVCONSTFIELD_BASE)
	  // execute_mvconstfield(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVCONSTFIELDR_INSTR)
	  JUMP(mvconstfieldr, MVCONSTFIELD_BASE)
	  // execute_mvconstfieldr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVADDRFIELD_INSTR)
	  JUMP(mvaddrfield, MVADDRFIELD_BASE)
	  // execute_mvaddrfield(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVFLOATFIELD_INSTR)
	  JUMP(mvfloatfield, MVFLOATFIELD_BASE)
	  // execute_mvfloatfield(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVFLOATREG_INSTR)
	  JUMP(mvfloatreg, MVFLOATREG_BASE)
	  // execute_mvfloatreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVINTCONST_INSTR)
	  JUMP(mvintconst, MVINTCONST_BASE)
	  // execute_mvintconst(pc);
	  ADVANCE()
	  ENDOP()

	  CASE(MVWORLDFIELD_INSTR)
	  JUMP(mvworldfield, MVWORLDFIELD_BASE)
	  // execute_mvworldfield(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVSTACKPCOUNTER_INSTR)
	  COMPLEX_JUMP(mvstackpcounter)
	  // pc = execute_mvstackpcounter(pc, state);
	  JUMP_NEXT();
	ENDOP()

	  CASE(MVPCOUNTERSTACK_INSTR)
	  JUMP(mvpcounterstack, MVPCOUNTERSTACK_BASE)
	  // execute_mvpcounterstack(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVSTACKREG_INSTR)
	  JUMP(mvstackreg, MVSTACKREG_BASE)
	  // execute_mvstackreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVREGSTACK_INSTR)
	  JUMP(mvregstack, MVREGSTACK_BASE)
	  // execute_mvregstack(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVADDRREG_INSTR)
	  JUMP(mvaddrreg, MVADDRREG_BASE)
	  // execute_mvaddrreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVHOSTREG_INSTR)
	  JUMP(mvhostreg, MVHOSTREG_BASE)
	  // execute_mvhostreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(ADDRNOTEQUAL_INSTR)
	  JUMP(addrnotequal, operation_size)
	  // execute_addrnotequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(ADDREQUAL_INSTR)
	  JUMP(addrequal, operation_size)
	  // Write instruction
	  cout << "ADDREQUAL ";
	// get reg arg 1
	byte arg1 = *(pc + instr_size); 
	// Write first byte : INSTR + reg arg 1
	byte b1 = 0xc0 | (arg1 & 0x3f);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// get reg arg 2
	byte arg2 = *(pc + instr_size + reg_val_size);
	// get dst reg num
	byte dst = *(pc + instr_size + 2*reg_val_size);
	// Write 2nd byte : reg arg 2 + 2 bits of dst
	byte b2 = ((arg2 & 0x3f) << 2) | ((dst & 0x18) >> 3);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write 3rd byte : last 3 bits of dst + OP code
	byte b3 = ((dst & 0x07) << 5) | 0x17;
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << endl;
	new_code_size += 3;
	ADVANCE()
	  ENDOP()

	  CASE(INTMINUS_INSTR)
	  JUMP(intminus, operation_size)
	  // Write instruction
	  cout << "INTMINUS ";
	// get reg arg 1
	byte arg1 = *(pc + instr_size); 
	// Write first byte : INSTR + reg arg 1
	byte b1 = 0xc0 | (arg1 & 0x3f);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// get reg arg 2
	byte arg2 = *(pc + instr_size + reg_val_size);
	// get dst reg num
	byte dst = *(pc + instr_size + 2*reg_val_size);
	// Write 2nd byte : reg arg 2 + 2 bits of dst
	byte b2 = ((arg2 & 0x3f) << 2) | ((dst & 0x18) >> 3);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write 3rd byte : last 3 bits of dst + OP code
	byte b3 = ((dst & 0x07) << 5) | 0x11;
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << endl;
	new_code_size += 3;
	ADVANCE()
	  ENDOP()

	  CASE(INTEQUAL_INSTR)
	  JUMP(intequal, operation_size)
	  // Write instruction
	  cout << "INTEQUAL ";
	// get reg arg 1
	byte arg1 = *(pc + instr_size); 
	// Write first byte : INSTR + reg arg 1
	byte b1 = 0xc0 | (arg1 & 0x3f);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// get reg arg 2
	byte arg2 = *(pc + instr_size + reg_val_size);
	// get dst reg num
	byte dst = *(pc + instr_size + 2*reg_val_size);
	// Write 2nd byte : reg arg 2 + 2 bits of dst
	byte b2 = ((arg2 & 0x3f) << 2) | ((dst & 0x18) >> 3);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write 3rd byte : last 3 bits of dst + OP code
	byte b3 = ((dst & 0x07) << 5) | 0x03;
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << endl;
	new_code_size += 3;
	ADVANCE()
	  ENDOP()

	  CASE(INTNOTEQUAL_INSTR)
	  JUMP(intnotequal, operation_size)
	  // Write instruction
	  cout << "INTNOTEQUAL ";
	// get reg arg 1
	byte arg1 = *(pc + instr_size); 
	// Write first byte : INSTR + reg arg 1
	byte b1 = 0xc0 | (arg1 & 0x3f);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// get reg arg 2
	byte arg2 = *(pc + instr_size + reg_val_size);
	// get dst reg num
	byte dst = *(pc + instr_size + 2*reg_val_size);
	// Write 2nd byte : reg arg 2 + 2 bits of dst
	byte b2 = ((arg2 & 0x3f) << 2) | ((dst & 0x18) >> 3);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write 3rd byte : last 3 bits of dst + OP code
	byte b3 = ((dst & 0x07) << 5) | 0x01;
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << endl;
	new_code_size += 3;
	ADVANCE()
	  ENDOP()

	  CASE(INTPLUS_INSTR)
	  JUMP(intplus, operation_size)
	  // Write instruction
	  cout << "INTPLUS ";
	// get reg arg 1
	byte arg1 = *(pc + instr_size); 
	// Write first byte : INSTR + reg arg 1
	byte b1 = 0xc0 | (arg1 & 0x3f);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// get reg arg 2
	byte arg2 = *(pc + instr_size + reg_val_size);
	// get dst reg num
	byte dst = *(pc + instr_size + 2*reg_val_size);
	// Write 2nd byte : reg arg 2 + 2 bits of dst
	byte b2 = ((arg2 & 0x3f) << 2) | ((dst & 0x18) >> 3);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write 3rd byte : last 3 bits of dst + OP code
	byte b3 = ((dst & 0x07) << 5) | 0x0f;
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << endl;
	new_code_size += 3;
	ADVANCE()
	  ENDOP()

	  CASE(INTLESSER_INSTR)
	  JUMP(intlesser, operation_size)
	  // execute_intlesser(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(INTGREATEREQUAL_INSTR)
	  JUMP(intgreaterequal, operation_size)
	  // execute_intgreaterequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(BOOLOR_INSTR)
	  JUMP(boolor, operation_size)
	  // execute_boolor(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(INTLESSEREQUAL_INSTR)
	  JUMP(intlesserequal, operation_size)
	  // execute_intlesserequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(INTGREATER_INSTR)
	  JUMP(intgreater, operation_size)
	  // execute_intgreater(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(INTMUL_INSTR)
	  JUMP(intmul, operation_size)
	  // execute_intmul(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(INTDIV_INSTR)
	  JUMP(intdiv, operation_size)
	  // execute_intdiv(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(INTMOD_INSTR)
	  JUMP(intmod, operation_size)
	  // Write instruction
	  cout << "INTMOD ";
	// get reg arg 1
	byte arg1 = *(pc + instr_size); 
	// Write first byte : INSTR + reg arg 1
	byte b1 = 0xc0 | (arg1 & 0x3f);
	predicates[i]->code.push_back(b1);
	cout << hex << (int)b1 << " ";
	// get reg arg 2
	byte arg2 = *(pc + instr_size + reg_val_size);
	// get dst reg num
	byte dst = *(pc + instr_size + 2*reg_val_size);
	// Write 2nd byte : reg arg 2 + 2 bits of dst
	byte b2 = ((arg2 & 0x3f) << 2) | ((dst & 0x18) >> 3);
	predicates[i]->code.push_back(b2);
	cout << hex << (int)b2 << " ";
	// Write 3rd byte : last 3 bits of dst + OP code
	byte b3 = ((dst & 0x07) << 5) | 0x0d;
	predicates[i]->code.push_back(b3);
	cout << hex << (int)b3 << endl;
	new_code_size += 3;
	ADVANCE()
	  ENDOP()

	  CASE(FLOATPLUS_INSTR)
	  JUMP(floatplus, operation_size)
	  // execute_floatplus(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATMINUS_INSTR)
	  JUMP(floatminus, operation_size)
	  // execute_floatminus(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATMUL_INSTR)
	  JUMP(floatmul, operation_size)
	  // execute_floatmul(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATDIV_INSTR)
	  JUMP(floatdiv, operation_size)
	  // execute_floatdiv(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATEQUAL_INSTR)
	  JUMP(floatequal, operation_size)
	  // execute_floatequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATNOTEQUAL_INSTR)
	  JUMP(floatnotequal, operation_size)
	  // execute_floatnotequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATLESSER_INSTR)
	  JUMP(floatlesser, operation_size)
	  // execute_floatlesser(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATLESSEREQUAL_INSTR)
	  JUMP(floatlesserequal, operation_size)
	  // execute_floatlesserequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATGREATER_INSTR)
	  JUMP(floatgreater, operation_size)
	  // execute_floatgreater(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(FLOATGREATEREQUAL_INSTR)
	  JUMP(floatgreaterequal, operation_size)
	  // execute_floatgreaterequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVREGREG_INSTR)
	  JUMP(mvregreg, MVREGREG_BASE)
	  // execute_mvregreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(BOOLEQUAL_INSTR)
	  JUMP(boolequal, operation_size)
	  // execute_boolequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(BOOLNOTEQUAL_INSTR)
	  JUMP(boolnotequal, operation_size)
	  // execute_boolnotequal(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(HEADRR_INSTR)
	  JUMP(headrr, HEADRR_BASE)
	  // execute_headrr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(HEADFR_INSTR)
	  JUMP(headfr, HEADFR_BASE)
	  // execute_headfr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(HEADFF_INSTR)
	  JUMP(headff, HEADFF_BASE)
	  // execute_headff(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(HEADRF_INSTR)
	  JUMP(headrf, HEADRF_BASE)
	  // execute_headrf(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(HEADFFR_INSTR)
	  JUMP(headffr, HEADFF_BASE)
	  // execute_headffr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(HEADRFR_INSTR)
	  JUMP(headrfr, HEADRF_BASE)
	  // execute_headrfr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(TAILRR_INSTR)
	  JUMP(tailrr, TAILRR_BASE)
	  // execute_tailrr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(TAILFR_INSTR)
	  JUMP(tailfr, TAILFR_BASE)
	  // execute_tailfr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(TAILFF_INSTR)
	  JUMP(tailff, TAILFF_BASE)
	  // execute_tailff(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(TAILRF_INSTR)
	  JUMP(tailrf, TAILRF_BASE)
	  // execute_tailrf(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVWORLDREG_INSTR)
	  JUMP(mvworldreg, MVWORLDREG_BASE)
	  // execute_mvworldreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVCONSTREG_INSTR)
	  JUMP(mvconstreg, MVCONSTREG_BASE)
	  // execute_mvconstreg(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVINTSTACK_INSTR)
	  JUMP(mvintstack, MVINTSTACK_BASE)
	  // execute_mvintstack(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVFLOATSTACK_INSTR)
	  JUMP(mvfloatstack, MVFLOATSTACK_BASE)
	  // execute_mvfloatstack(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(MVARGREG_INSTR)
	  JUMP(mvargreg, MVARGREG_BASE)
	  // execute_mvargreg(pc, state);
	  ADVANCE()
	  ENDOP()
            
	  CASE(CONSRRR_INSTR)
	  JUMP(consrrr, CONSRRR_BASE)
	  // execute_consrrr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CONSRFF_INSTR)
	  JUMP(consrff, CONSRFF_BASE)
	  // execute_consrff(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CONSFRF_INSTR)
	  JUMP(consfrf, CONSFRF_BASE)
	  // execute_consfrf(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CONSFFR_INSTR)
	  JUMP(consffr, CONSFFR_BASE)
	  // execute_consffr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CONSRRF_INSTR)
	  JUMP(consrrf, CONSRRF_BASE)
	  // execute_consrrf(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CONSRFR_INSTR)
	  JUMP(consrfr, CONSRFR_BASE)
	  // execute_consrfr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CONSFRR_INSTR)
	  JUMP(consfrr, CONSFRR_BASE)
	  // execute_consfrr(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CONSFFF_INSTR)
	  JUMP(consfff, CONSFFF_BASE)
	  // execute_consfff(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(CALLE_INSTR)
	  JUMP(calle, CALLE_BASE * calle_num_args(pc))
	  // execute_calle(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(SET_PRIORITY_INSTR)
	  JUMP(set_priority, SET_PRIORITY_BASE)
	  // execute_set_priority(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(SET_PRIORITYH_INSTR)
	  JUMP(set_priorityh, SET_PRIORITYH_BASE)
	  // execute_set_priority_here(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(ADD_PRIORITY_INSTR)
	  JUMP(add_priority, ADD_PRIORITY_BASE)
	  // execute_add_priority(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(ADD_PRIORITYH_INSTR)
	  JUMP(add_priorityh, ADD_PRIORITYH_BASE)
	  // execute_add_priority_here(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(STOP_PROG_INSTR)
	  JUMP(stop_program, STOP_PROG_BASE)
	  // sched::base::stop_flag = true;
	  ADVANCE()
	  ENDOP()

	  CASE(CPU_ID_INSTR)
	  JUMP(cpu_id, CPU_ID_BASE)
	  // execute_cpu_id(pc, state);
	  ADVANCE()
	  ENDOP()

	  CASE(NODE_PRIORITY_INSTR)
	  JUMP(node_priority, NODE_PRIORITY_BASE)
	  // execute_node_priority(pc, state);
	  ADVANCE()
	  ENDOP()

	  COMPLEX_JUMP(not_found)
      default:
	  cerr << "unsupported instruction" << endl;
	ENDOP()
	  }
    }
}
