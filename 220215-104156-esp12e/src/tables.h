#ifndef TABLES_H
#define TABLES_H

#define MAX_KEY_COUNT 20
#define MAX_MACRO_LENGTH 15
#define MAX_NUM_MACROS 25

typedef struct {
  char key_name[2][MAX_KEYNAME_LENGTH + 1];
  uint16_t key_value;
}key_entry;

typedef struct {
  unsigned short functions[2];
}function_map_entry;

typedef struct {
  char key_name[MAX_KEYNAME_LENGTH + 1];
  unsigned char assigned;
}key_assign_entry;

typedef struct { // preset table loaded from preset file
  char func_name[16];
  unsigned short value;
}preset;

typedef struct {
  unsigned long start_magic;
  unsigned char length;
  unsigned long end_magic;
  char data[0];
}preset_header;

typedef struct {    
  uint16_t key_value;
  unsigned char preset_output;
  char func_name[16];
  char key_name[MAX_KEYNAME_LENGTH+1];
}map_func_to_key;

typedef struct {
  char key_name[MAX_KEYNAME_LENGTH + 1];
  uint16_t command;
} macro_step_entry;
struct macro_node {
  uint16_t click_type;
  uint16_t key_value;
  uint8_t macro_length;
  char *key_name;
  macro_step_entry commands[MAX_MACRO_LENGTH];
  struct macro_node *next;
};
typedef struct macro_node macro_node_t;

typedef struct {
  uint8_t num_macros;
  uint8_t length_list[MAX_NUM_MACROS];
}macro_file_header_t;



void inline set_key_assigned_status(int key_idx, unsigned char assigned);
uint8_t inline get_command_value(int assign_key_index);
void inline increase_command_value(int assign_key_index);
#endif
