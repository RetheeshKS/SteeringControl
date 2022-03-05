#include "LittleFS.h"
#include "platform.h"
void print_FS_info();

#define MAP_FILE_NAME "mapfile.map"
#define MACRO_FILE_NAME "macros.bin"
extern macro_file_header_t macro_file_header;
extern  macro_node_t *macro_head;

#ifdef ESP32
#define LittleFS LITTLEFS
#endif

int load_index_map() {
  int read_count = 0;
  int i, j;
  File this_file = LittleFS.open(MAP_FILE_NAME, "r");
  if (!this_file) { // failed to open the file, retrn empty result
    Serial.println("Could not read index map, file open failed...");
    return 0;
  }
  /*while (this_file.available()) {
      result += (char)this_file.read();
  }*/

  read_count = this_file.read((uint8_t *)(&index_map[0]), (sizeof(index_map)));
#if DEBUG_MODE
  Serial.printf("Index map load complete, map size: %d...\n", read_count);
#endif
  this_file.close();
#if DEBUG_MODE
  Serial.println("===================Current Index Map=====================");
  for(int i=0;i<MAX_INPUT_VALUE * 2;i++){
    Serial.printf("%X ", (index_map[i/MAX_INPUT_VALUE][i%MAX_INPUT_VALUE]));
    if((i%32 == 0) && (i!=0) ) Serial.println();
  }
  Serial.println();
  Serial.println("======================================================");
#endif

  memset(&macro_file_header, 0, sizeof(macro_file_header));
  this_file = LittleFS.open(MACRO_FILE_NAME, "r");
  if (!this_file) { // failed to open the file, retrn empty result
    Serial.println("Could not read macro table, file open failed...");
    return 0;
  }
  this_file.read((uint8_t *)(&macro_file_header), (sizeof(macro_file_header)));
  for (i = 0; i<macro_file_header.num_macros;i++){
    macro_table[i] = (uint8_t *)malloc(macro_file_header.length_list[i]);
    this_file.read((uint8_t *)(macro_table[i]), macro_file_header.length_list[i]);
  }
  this_file.close();
#if DEBUG_MODE
  Serial.println("===================Current Marco Table=====================");
  for (i = 0; i<macro_file_header.num_macros;i++){
    Serial.printf("****** Macro Table Entry: %d *****\n", i+1);
    for (j = 0; j<macro_file_header.length_list[i];j++){
      Serial.printf("-----Command %d: %d------\n", j+1, *(macro_table[i] + j));
    }
  }
#endif
  return read_count;
}

bool save_index_map() {
  int write_count = 0;
  print_FS_info();
  File this_file = LittleFS.open(MAP_FILE_NAME, "w");
  if (!this_file) { // failed to open the file, return false
    Serial.println("Could not save index map, file open failed...");
    return false;
  }

#if DEBUG_MODE
  Serial.println("===================Saving Index Map=====================");
  for(int i=0;i<MAX_INPUT_VALUE * 2;i++){
    Serial.printf("%X ", (index_map[i/MAX_INPUT_VALUE][i%MAX_INPUT_VALUE]));
    if((i%50 == 0) && (i!=0) ) Serial.println();
  }
  Serial.println();
  Serial.println("======================================================");
#endif 
  write_count = this_file.write((uint8_t *)(&index_map[0]), (sizeof(index_map)));
  this_file.close();
  if (write_count == 0) { // write failed
      Serial.println("Could not save index map, write failed...");
      return false;
  }
  Serial.printf("Index map save complete, map size: %d...\n", write_count);
  
  File macro_file = LittleFS.open(MACRO_FILE_NAME, "w");
  if (!macro_file) { // failed to open the file, return false
    Serial.println("Could not save macros, file open failed...");
    return false;
  }
  macro_node_t *macro_itr = NULL;
  int i = 0;
  write_count = macro_file.write((uint8_t *)(&macro_file_header), (sizeof(macro_file_header)));
  for(macro_itr = macro_head; macro_itr; macro_itr= macro_itr->next){
    for(i = 1;i < macro_itr->macro_length;i++){
      macro_file.write((uint8_t *)(&(macro_itr->commands[i].command)), (sizeof(uint8_t))); // Saving the commands starting from second step
    }
    
  }
  this_file.close();
  
  print_FS_info();
  return true;
}

void init_FS()
{
 
  LittleFS.begin();
  print_FS_info();
}
#ifndef ESP32
void print_FS_info()
{
  FSInfo fsinfo;

  
  LittleFS.info(fsinfo);

  Serial.printf("fsinfo  totalBytes=%u, usedBytes=%u, blockSize=%u, pageSize=%u, maxOpenFiles=%u, maxPathLength=%u\n",fsinfo.totalBytes, fsinfo., fsinfo.blockSize, fsinfo.pageSize, fsinfo.maxOpenFiles, fsinfo.maxPathLength);


}
#else
void print_FS_info()
{
  Serial.printf("fsinfo  totalBytes=%u, usedBytes=%u\n",
                             LittleFS.totalBytes(), LittleFS.usedBytes());

}
#endif
void remove_map_file()
{
#if DEBUG_MODE
  Serial.println("///////////////////Remove map file ////////////////////");
  print_FS_info();
#endif
  LittleFS.remove(MAP_FILE_NAME);
#if DEBUG_MODE
  Serial.println("///////////////////Removed map file ////////////////////");
  print_FS_info();
#endif
}
