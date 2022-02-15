#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #define PIN_USER_INPUT_34 34
  #define MAX_INPUT_VALUE 4096
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #define PIN_USER_INPUT_34 A0
  #define MAX_INPUT_VALUE 1024
#endif
#define START_MAGIC 0xCAFEBABE
#define END_MAGIC 0xDEADBEAF


macro_node_t *macro_head;
macro_node_t *new_macro;
int last_step_index;
macro_file_header_t macro_file_header;

void init_macro_tables()
{
  macro_head = NULL;
  new_macro = NULL;
  last_step_index = 0;
}
void init_preset_table(void **p_t, char *p_data)
{
  preset **preset_table = (preset **)p_t;
  preset_header *preset_file_data = (preset_header *)p_data;
  preset_header *p_head;
  unsigned long data_end_magic = 0;
  int len, i;
  *preset_table = NULL;
  if(!preset_table){
    Serial.printf("\n%s: Error: preset table empty..\n"); 
    return;
  }
  
  if(!preset_file_data){
    Serial.printf("\n%s: Error: preset data empty..\n");
    return;
  }
  p_head = (preset_header *)preset_file_data;
  len = (p_head->length > 20)? 20 : p_head->length;
  Serial.printf("End address:%X, length;%d, len:%d\n", (&(p_head->data[0])) + (sizeof(preset)*len) , p_head->length, len);
  memcpy(&(data_end_magic), (&(p_head->data[0])) + (sizeof(preset)*len), sizeof(data_end_magic));
  if(!((p_head->start_magic == START_MAGIC) && (p_head->end_magic == END_MAGIC) && (data_end_magic == END_MAGIC))){
    Serial.printf("\n%s: Error: Invalid preset data, %X, %X, %X..\n", __FUNCTION__,
                      p_head->start_magic, p_head->end_magic, data_end_magic);
    return;
  }
  *preset_table = (preset *)malloc(sizeof(preset)*len);
  memcpy(*preset_table, p_head->data,sizeof(preset)*len);
  for(i = 0; i < p_head ->length;i++){
    Serial.printf("%s : %d\n", (*preset_table)[i].func_name, (*preset_table)[i].value);
  }
}


//preset format: [START_MAGIC]length[END_MAGIC]PRESET_DATA[END_MAGIC]
static char p_data[512];
char * prepare_dummy_preset()
{
  preset_header *p_head;
  preset *ptr;
  void *end;
  p_head = (preset_header *)(&p_data[0]);
  p_head -> start_magic = START_MAGIC;
  p_head -> end_magic = END_MAGIC;
  p_head -> length = 20;
  ptr = (preset *)(&(p_head->data));
  Serial.printf("\nData start: %X, size:%d\n", ptr, sizeof(preset));
  for(int i= 0; i<p_head -> length; i++){
    Serial.printf("Index:%d,  %X \n", i, ptr);
    sprintf(ptr->func_name,"FUNCT_%d",i+100);
    ptr->value = 200+(i*3);
    ptr ++;
  }
  
  end = (void *)ptr;
  Serial.printf("\nEnd magic at: %X, end:%X\n", ptr, (unsigned long*)end);
  *((unsigned long*)end) = END_MAGIC;
  Serial.printf("\nEnd magic value: %X, end:%X\n", ptr, *((unsigned long*)end));
  return (&p_data[0]);
}

key_entry key_name_table[MAX_KEY_COUNT];
int remove_key(String key)
{
  int i = key.toInt();
  if(i < key_list_last_idx){
    for(; i < key_list_last_idx - 1; i++){
      memcpy(&key_name_table[i], &key_name_table[i+1], sizeof(key_name_table[i]));
    }
    key_list_last_idx --;
    return 1;
  }
  return 0;
}

int check_duplicate(String new_name)
{
  int i;
  String temp_name = String("DBL_") + new_name;
  for(i = 0;i<key_list_last_idx;i++){
    if(new_name.equals(key_name_table[i].key_name[0]) || new_name.equals(key_name_table[i].key_name[1])
                 || temp_name.equals(key_name_table[i].key_name[0]) || temp_name.equals(key_name_table[i].key_name[1])){
      return 1;
    }
  }
  return 0;
}

void prepare_index_map()
{
  int start = 0, end, i, mid;
  //index_map[scan_machine.scanned_key_value] = key_list_last_idx;
  memset(index_map, INVALID_COMMAND, sizeof(index_map));
  for(i = 0; i < key_list_last_idx; i++){
    index_map[0][key_name_table[i].key_value] = i;
  }
  for(end = start; end < MAX_INPUT_VALUE; end ++){
    while(index_map[0][end] == INVALID_COMMAND && end < MAX_INPUT_VALUE) end++;
    if(index_map[0][start] == INVALID_COMMAND)  mid = start;
    else if(end < MAX_INPUT_VALUE) mid = (start + end)/2;
    else mid = MAX_INPUT_VALUE;

    for(i = start + 1; i< mid; i++){
      index_map[0][i] = index_map[0][start];
    }
    for(i = mid; i< end; i++){
      index_map[0][i] = index_map[0][end];
    }
    start = end;
  }
  for(i=0;i<MAX_INPUT_VALUE;i++){
    if(i == 0 || (i == MAX_INPUT_VALUE -1)){
      Serial.printf("map[%d]:%d\n", i,index_map[0][i]);
    }
    if((i < MAX_INPUT_VALUE -1) &&  (index_map[0][i] != index_map[0][i+1])){
      Serial.printf("map[%d]:%d\n", i,index_map[0][i]);
      Serial.printf("map[%d]:%d\n", i+1,index_map[0][i+1]);
    }
  }
}

function_map_entry function_map_table[MAX_KEY_COUNT];
void add_function_map_entry(preset *p_table, int p_table_len, String func_name, String key_name)
{
   int key_index, click_type = 0, p_index;
   for(key_index = 0; key_index < MAX_KEY_COUNT; key_index ++){
    if(key_name.equals(key_name_table[key_index].key_name[0])) {
      break;
    } else if (key_name.equals(key_name_table[key_index].key_name[1])){
      click_type = 1;
      break;
    }
   }
   if(key_index < MAX_KEY_COUNT){
     for(p_index = 0; p_index < p_table_len; p_index ++){
      if(func_name.equals(p_table[p_index].func_name)){
        function_map_table[key_index].functions[click_type] = p_table[p_index].value;
        break;
      }
     }
   }
}

unsigned char auto_command;
int pioneer_commands[]={1, 4 , 8, 12 , 16, 7, 9, 13, 17, 18, 21, 23, 29, 33, 42, 49, 71, 96};

void init_auto_output_values()
{
   auto_command = 0;
}

uint8_t get_next_auto_output_value()
{
   unsigned char auto_cmd = 0xFF;
   if(auto_command < sizeof(pioneer_commands)){
    auto_cmd = pioneer_commands[auto_command];
    auto_command +=1;
   }
   return (auto_cmd);
}

int preset_mode = 0;
map_func_to_key *func_to_key_table = NULL;
int func_to_key_len = 0;

void final_update_index_map()
{ 
  int start = 0, end, i;
  int macro_node_index = 0;
  macro_node_t *macro_itr = NULL;
  int low, high, low_inc, high_dec, range;
  String func_name, key_name;
  memset(index_map, INVALID_COMMAND, sizeof(index_map));
  
   if(preset_mode == 0){
    init_auto_output_values();
    for(i = 0; i < key_list_last_idx; i++){
      *((uint8_t *)&index_map[0][(int)key_name_table[i].key_value]) = get_next_auto_output_value();
    }
    for(i = 0; i < key_list_last_idx; i++){
      //*((uint8_t *)&index_map[1][(int)key_name_table[i].key_value]) = (uint8_t)(index_map[0][(int)key_name_table[i].key_value] + 40);
      *((uint8_t *)&index_map[1][(int)key_name_table[i].key_value]) = get_next_auto_output_value();
    }
   }else if(preset_mode == 1){
    for(i = 0; i < key_list_last_idx; i++){
      index_map[0][key_name_table[i].key_value] = function_map_table[i].functions[0];
      index_map[1][key_name_table[i].key_value] = function_map_table[i].functions[1];
    }
   }

   if(macro_head){
     memset(&macro_file_header, 0, sizeof(macro_file_header));
     macro_itr = macro_head;
     while(macro_itr){
      WORDLSBYTE(index_map[macro_itr->click_type][macro_itr->key_value]) = macro_itr->commands[0].command;
      WORDMSBYTE(index_map[macro_itr->click_type][macro_itr->key_value]) = macro_node_index;
      for(i=1; i< macro_itr->macro_length; i++){
        
      }
      // Set macro file header values, this will be saved in the save index map function
      macro_file_header.length_list[macro_node_index] = macro_itr->macro_length -1;
      macro_file_header.num_macros ++;
      macro_node_index ++;
      macro_itr = macro_itr->next;
     }
   }
#if DEBUG_MODE
  Serial.printf("=================== Index Map Key Allocation: %s=====================\n", __FUNCTION__);
  for(int i=0;i<MAX_INPUT_VALUE * 2;i++){
    if((uint8_t)index_map[i/MAX_INPUT_VALUE][i%MAX_INPUT_VALUE] != INVALID_COMMAND){
      Serial.printf("[**%d**]", i%MAX_INPUT_VALUE);
    }
    Serial.printf("%02X ", (index_map[i/MAX_INPUT_VALUE][i%MAX_INPUT_VALUE]));
    if((i%50 == 0) && (i!=0) ) Serial.println();
  }
  Serial.println();
  Serial.println("======================================================");
#endif
   for(int click_type = 0;click_type < 2; click_type ++){
    start = 0;
    for(end = start; end < MAX_INPUT_VALUE; end ++){
      while((uint8_t)index_map[click_type][end] == INVALID_COMMAND && end < (MAX_INPUT_VALUE -1)) end++;

      high_dec = (WORDLSBYTE(index_map[click_type][end]) != INVALID_COMMAND);
      low_inc = (WORDLSBYTE(index_map[click_type][start]) != INVALID_COMMAND);
      range = KEY_INPUT_TOLERANCE;
      for(low=start+1, high=end-1;low<high; low+=low_inc, high-=high_dec){
        index_map[click_type][low] = index_map[click_type][start];
        index_map[click_type][high] = index_map[click_type][end];
        range--;
        if(!range) break;
      }

      start = end;
    }
#ifdef DEBUG_MODE
    for(i=0;i<MAX_INPUT_VALUE;i++){
      if(i == 0 || (i == MAX_INPUT_VALUE -1)){
        Serial.printf("map[%d]:%d\n", i,index_map[click_type][i]);
      }
      if((i < MAX_INPUT_VALUE -1) &&  (index_map[click_type][i] != index_map[click_type][i+1])){
        Serial.printf("map[%d]:%d\n", i,index_map[click_type][i]);
        Serial.printf("map[%d]:%d\n", i+1,index_map[click_type][i+1]);
      }
    }
#endif
   }
}

//key_assign_entry *key_assign_table = NULL;
void init_key_assign_table()
{
  int index = 0;
  if(key_assign_table){
    free(key_assign_table);
  }
  key_assign_table = (key_assign_entry *)malloc(sizeof(key_assign_entry)*key_list_last_idx * 2);
  memset(key_assign_table, 0, sizeof(key_assign_entry)*key_list_last_idx * 2);
  for(index = 0; index < key_list_last_idx; index ++){
     memcpy(key_assign_table[index*2].key_name,key_name_table[index].key_name[0],sizeof(key_name_table[index].key_name[0]));
     memcpy(key_assign_table[index*2 + 1].key_name,key_name_table[index].key_name[1],sizeof(key_name_table[index].key_name[0]));
     key_assign_table[index*2].assigned = 0;
     key_assign_table[index*2 + 1].assigned = 0;
  }
}

void inline set_key_assigned_status(int key_idx, unsigned char assigned)
{
   key_assign_table[key_idx].assigned = assigned;
}

uint8_t inline get_command_value(int assign_key_index)
{
  //return (uint8_t)index_map[assign_key_index % 2][key_name_table[assign_key_index / 2].key_value];
  return WORDLSBYTE(index_map[assign_key_index % 2][key_name_table[assign_key_index / 2].key_value]);
}

void inline increase_command_value(int assign_key_index)
{
  //return (uint8_t)index_map[assign_key_index % 2][key_name_table[assign_key_index / 2].key_value];
  WORDLSBYTE(index_map[assign_key_index % 2][key_name_table[assign_key_index / 2].key_value]) = 
                 WORDLSBYTE(index_map[assign_key_index % 2][key_name_table[assign_key_index / 2].key_value]) + 1;
}

int get_assign_key_index(String key_name)
{
  int index;
  for(index = 0; index < key_list_last_idx * 2; index ++){
    if(key_name.equals(key_assign_table[index].key_name)){
      return index;
    }
  }
  return (-1);
}

void filter_index_map()
{
   int key_idx, map_index, click_index, tol_idx;
   uint8_t cmd;
   for(key_idx = 0; key_idx < key_list_last_idx * 2; key_idx ++){
    map_index = key_name_table[key_idx / 2].key_value;
    click_index = key_idx % 2;
    if(!key_assign_table[key_idx].assigned && WORDMSBYTE(index_map[click_index][map_index]) == INVALID_COMMAND){
       
       cmd = (uint8_t)index_map[click_index][map_index];
       for(tol_idx = 0; tol_idx <= KEY_INPUT_TOLERANCE; tol_idx ++){
         if((map_index + tol_idx) < MAX_INPUT_VALUE && (uint8_t)index_map[click_index][map_index + tol_idx] == cmd){
            WORDLSBYTE(index_map[click_index][map_index + tol_idx]) = INVALID_COMMAND;
         }
         if((map_index - tol_idx) >= 0 && (uint8_t)index_map[click_index][map_index - tol_idx] == cmd){
            WORDLSBYTE(index_map[click_index][map_index - tol_idx]) = INVALID_COMMAND;
         }
       }
    }
   }
}


void macro_clear_step(int step_idx)
{
  for(; step_idx < last_step_index - 1; step_idx++){
    new_macro->commands[step_idx] = new_macro->commands[step_idx + 1];
  }
  last_step_index--;
}
//void macro_add_step(int assign_key_idx) 
void macro_add_step(String key_name)
{
  int assign_key_idx;
  if(last_step_index >= MAX_MACRO_LENGTH){
    return;
  }
  assign_key_idx = get_assign_key_index(key_name);
#if DEBUG_MODE
  Serial.printf("%s: Got key index : %d\n", __FUNCTION__, assign_key_idx);
#endif
  new_macro->commands[last_step_index].command = get_command_value(assign_key_idx);
#if DEBUG_MODE
  Serial.printf("%s: Got command value : %d\n", __FUNCTION__, new_macro->commands[last_step_index].command);
#endif
  strcpy(new_macro->commands[last_step_index].key_name, key_name.c_str());
#if DEBUG_MODE
  Serial.printf("%s: Stored command value : %d, for %s \n", __FUNCTION__, new_macro->commands[last_step_index].command, new_macro->commands[last_step_index].key_name);
#endif
  last_step_index++;
}
void macro_def_init(int assign_key_idx)
{
  if(new_macro){
    free(new_macro);
  }
  new_macro = (macro_node_t *)malloc(sizeof(macro_node_t));
  memset(new_macro, 0, sizeof(macro_node_t));
  new_macro->click_type = assign_key_idx%2;
  new_macro->key_value = key_name_table[assign_key_idx/2].key_value;
  new_macro->key_name = (char *)&(key_name_table[assign_key_idx/2].key_name[new_macro->click_type]);
  last_step_index = 0;
#if DEBUG_MODE
  Serial.printf("%s: macro def init done..\n", __FUNCTION__);
#endif
}
void clear_current_macro()
{
   if(new_macro){
    free(new_macro);
    new_macro = NULL;
   }
}
void macro_table_delete(int assign_key_idx)
{
  uint16_t click_type, key_value;
  macro_node_t *macro_itr = macro_head, *old_next;
  click_type = assign_key_idx%2;
  key_value = key_name_table[assign_key_idx / 2].key_value;
  if (macro_head -> key_value == key_value && macro_head -> click_type == click_type){
    macro_head = macro_head -> next;
    free(macro_itr);
    return;
  }
  while(macro_itr -> next) {
    if(key_value == macro_itr->next->key_value && click_type == macro_itr->next->click_type){
      old_next = macro_itr -> next;
      macro_itr->next = macro_itr->next->next;
      free(old_next);
      return;
    }
    macro_itr = macro_itr -> next;
  }
}
void dump_macro()
{
  int i;
   macro_node_t *macro_itr = macro_head;
   for(;macro_itr;macro_itr = macro_itr->next){
     Serial.printf("****** Macro: %s[%d]*****\n", macro_itr->key_name, macro_itr->key_value); 
     for(i = 0; i<macro_itr->macro_length;i++){
       Serial.printf("-----%s[%d]------\n", macro_itr->commands[i].key_name, macro_itr->commands[i].command);
     }
   }
}
void macro_table_add()
{
  macro_node_t *macro_itr = macro_head;
  new_macro->macro_length = last_step_index;
  while(macro_itr) {
    if(!macro_itr -> next){
      macro_itr -> next = new_macro;
      break;
    }
    macro_itr = macro_itr -> next;
  }
  if(!macro_itr){
    macro_head = new_macro;
  }
  new_macro = NULL;
}

bool is_macro_assigned(int assign_key_idx)
{
  uint16_t click_type, key_value;
  macro_node_t *macro_itr = macro_head;
  click_type = assign_key_idx%2;
  key_value = key_name_table[assign_key_idx/2].key_value;
  while(macro_itr) {
    if((key_value == macro_itr -> key_value) && (click_type == macro_itr -> click_type)){
      return true;
    }
    macro_itr = macro_itr->next;
  }
  return false;
}

void macro_table_init()
{
  macro_node_t *macro_itr = macro_head, *macro_prev;
  while(macro_itr) {
    macro_prev = macro_itr;
    macro_itr = macro_itr -> next;
    free(macro_prev);
  }
  macro_head = NULL;
}
