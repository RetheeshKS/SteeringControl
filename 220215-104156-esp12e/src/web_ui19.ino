#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "common.h"
#include "tables.h"
#include "html_formats.h"

#define THIRTY_SECONDS 30000
#define ONEHALF_MINUTS 90000
#define START_MAGIC 0xCAFEBABE
#define END_MAGIC 0xDEADBEAF

#define SCAN_STATE_INIT 0
#define SCAN_STATE_PROGRESSING 1
#define SCAN_STATE_END 2

#define SCAN_START 1
#define SCAN_PROGRESS 2
#define SHORT_KEY_PRESS 3
#define LONG_IDLE 4
#define SCAN_SUCCESS 5
#define SCAN_STOPPED 6

IPAddress    apIP(10, 10, 10, 10);
static unsigned long wifi_start_time;

AsyncWebServer *server;
int page_seq;
bool first_request_received = false;
bool wifi_up = true;
bool config_enabled = false;

char *html_buffer = NULL;
int key_list_last_idx = 0;
unsigned short index_map[2][MAX_INPUT_VALUE];
uint8_t *macro_table[MAX_NUM_MACROS];
int progress_val = 0; // scan progress value
int key_scan_enable = 0;
static unsigned int ui_pre_key_val = KEY_IDLE_MIN_VALUE;
static unsigned int ui_cur_key_val;

extern void init_key_handle_vars();
extern void init_macro_tables();
extern void handle_key();
extern void HU_assign_key(uint8_t cmd);
key_assign_entry *key_assign_table;

extern key_entry key_name_table[20];

struct {
  unsigned long scan_init_time;
  unsigned long scanned_key_value;
  unsigned int scan_progress_value;
  unsigned char state;
  unsigned char scan_status;
  String key_name;
}scan_machine;


String get_value(AsyncWebServerRequest *request, char *p_name)
{
  int params = request->params();
  for (int i = 0; i < params; i++){
    AsyncWebParameter *p = request->getParam(i);
    if(p->name().equals(p_name)){
        return p->value();
    }
  }
  return (String)NULL;
}


void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void page_1(AsyncWebServerRequest *request, int alert)
{
  char * index_html = html_buffer;
  sprintf(index_html,index_html_p1, page_seq);
  if(alert){
    strcpy(index_html+strlen(index_html),page1_alert);
  }
  Serial.printf("Executing page 1: Size of : %d\n", sizeof(index_html_p1));
  strcpy(index_html+strlen(index_html),index_html_p2);
  Serial.printf("Executing page 1: total : %d\n", strlen(index_html));
  request->send_P(200, "text/html", index_html);
}
void init_all_data_structures()
{
  page_seq = 0;
  if(html_buffer){
    free(html_buffer);
  }
  html_buffer = (char *)malloc(8192);
  if(key_assign_table){
    free(key_assign_table);
    key_assign_table = NULL;
  }
  key_list_last_idx = 0;
  
}

bool web_server_initialized = false;
void destroy_web_server()
{
  server->end();
  delete(server);
}

void prepare_key_name_list(char *key_name_list_html)
{
  int i, len, tot_len = 0;
  memset(key_name_list_html, 0, sizeof(key_name_list_html));
  //Serial.printf("Start key list html\n");
  for(i = 0; i < key_list_last_idx;i++){
    len =  sprintf(&(key_name_list_html[tot_len]),
               "<tr><td width=\"100%\" onClick=\"onRmv('%d');\">%s</td>\
               <td width=\"100%\" onClick=\"onRmv('%d');\">%s</td>",
                                                                i, key_name_table[i].key_name[0], i, key_name_table[i].key_name[1]);
#if DEBUG_MODE
    tot_len += len;
    len = sprintf(&(key_name_list_html[tot_len]),"<td>%d</td>", key_name_table[i].key_value);
#endif
    tot_len += len;
    len =  sprintf(&(key_name_list_html[tot_len]),"</tr>");
    tot_len += len;
  }
}

String get_status_string(int status_val)
{
  switch(status_val){
    case SCAN_START:
    case SCAN_PROGRESS:   return ((String)"progress");
    case SHORT_KEY_PRESS: return ((String)"short_key");
    case LONG_IDLE:       return ((String)"long_idle");
    case SCAN_SUCCESS:    return ((String)"success");
    case SCAN_STOPPED:    return ((String)"stopped");
    default:              return ((String)"unknown");
  }
}
void init_scan_machine()
{
  scan_machine.scanned_key_value = KEY_IDLE_MIN_VALUE; 
  scan_machine.state = SCAN_STATE_INIT;
  scan_machine.scan_status = SCAN_START;
  scan_machine.scan_progress_value = 0;
  ui_pre_key_val = KEY_IDLE_MIN_VALUE;
  scan_machine.scan_init_time = millis();
}
void start_key_scan()
{
  init_scan_machine();
  key_scan_enable = 1;
  
}
void stop_key_scan(){
  key_scan_enable = 0;
}

void do_key_scan()
{
  static unsigned long progress_start_time;
  unsigned long cur_time = millis();
  
  static unsigned long last_read_time = 0;
  if(cur_time < (last_read_time + 5)) return;
  last_read_time = cur_time;
  ui_cur_key_val = analogRead(PIN_USER_INPUT);
  
  switch(scan_machine.state){
    case SCAN_STATE_INIT:
      if(ui_cur_key_val < KEY_IDLE_MIN_VALUE
                      && ui_pre_key_val >= KEY_IDLE_MIN_VALUE){
        progress_start_time = cur_time;
        scan_machine.state = SCAN_STATE_PROGRESSING;
        scan_machine.scan_status = SCAN_PROGRESS;
        scan_machine.scanned_key_value = ui_cur_key_val;
      } else if(cur_time > scan_machine.scan_init_time + 5000){
        scan_machine.scan_status = LONG_IDLE;
        scan_machine.state = SCAN_STATE_END;
      }
    break;
    case SCAN_STATE_PROGRESSING:
      if((ui_cur_key_val >= KEY_IDLE_MIN_VALUE
                        && ui_pre_key_val < KEY_IDLE_MIN_VALUE)){
        if((cur_time < progress_start_time + 300)){
          scan_machine.scan_progress_value = 100;
          scan_machine.scan_status = SHORT_KEY_PRESS;
        } else {
          scan_machine.scan_status = SCAN_SUCCESS;
        }
        scan_machine.state = SCAN_STATE_END;
      }else if(cur_time > progress_start_time + 300){
        scan_machine.scan_progress_value = 100;
        scan_machine.scan_status = SCAN_SUCCESS;
        scan_machine.state = SCAN_STATE_END;
      }else if(ui_cur_key_val < scan_machine.scanned_key_value){
        scan_machine.scanned_key_value = ui_cur_key_val;
      }
      scan_machine.scan_progress_value = ((cur_time - progress_start_time)*100)/(progress_start_time+300-progress_start_time);
    break;
    case SCAN_STATE_END:
      stop_key_scan();
    break;
  }
  ui_pre_key_val = ui_cur_key_val;
}

void show_add_key(AsyncWebServerRequest *request, int state)
{
  char key_name_list_html[2048] = {""};
  char *add_key_html;//[sizeof(add_key_fmt) + sizeof(key_name_list_html) + 40];
  
  String progress_hidden = "style=\"display:none;\"";
  String key_scan_start_message = "";
  String k_name = "";
  int page_size=0;
  String prg_en = "";
  //page_size = sizeof(add_key_fmt) + sizeof(key_name_list_html) + 40;
  add_key_html = html_buffer; //(char *)malloc(page_size);
  //memset(add_key_html, 0, 4096);
  add_key_html[0] = 0;
  //Serial.printf("Page size: %d\n", sizeof(add_key_fmt));
   
  if(state == 0){
    //Show add key form with out progress bar
    prepare_key_name_list(key_name_list_html);
  } else if (state == 1){ // clicked scan key, so show progress bar with 0% at first, and show a message to press the key for 3 seconds
    //Serial.printf("New key name: %s\n", get_value(request,"k_name").c_str());
    progress_val = 0;
    k_name = String("value=") + String("\"") + get_value(request,"k_name")+ String("\"");
    if(check_duplicate(get_value(request,"k_name"))){
      prg_en = "dup_name";
      prepare_key_name_list(key_name_list_html);
    } else {
      Serial.printf("Not Duplicate: calling start_key_scan\n");
      progress_hidden = "";
      k_name = k_name + String(" disabled");
      key_scan_start_message = "<p style=\"color:red;animation:blinker 0.6s linear infinite;\">!Please press and hold steering key for 3 seconds...</p>";
      scan_machine.key_name = get_value(request,"k_name");
      start_key_scan();
      prg_en = get_status_string(scan_machine.scan_status);
    }
  } else if(state == 2){ // Checking the  progress of scan process 
    //Serial.printf("Current key name: %s\n", scan_machine.key_name.c_str());
    progress_hidden = "";
    k_name = "";//String("value=") + String("\"") + scan_machine.key_name + String("\"");
    progress_val = scan_machine.scan_progress_value;
    prg_en = get_status_string(scan_machine.scan_status);
    if (scan_machine.scan_status != SCAN_SUCCESS){
      key_scan_start_message = "<p style=\"color:red;animation:blinker 0.6s linear infinite;\">!Please press and hold steering key for 3 seconds...</p>";
      k_name = String("value=") + String("\"") + scan_machine.key_name + String("\"");
    } 
    if (scan_machine.scan_status == SCAN_START || scan_machine.scan_status == SCAN_PROGRESS)
    {
      k_name += String(" disabled");
    } 
  } else if(state == 3){ // Key scanning completed , add key to the existing list
    strncpy(&(key_name_table[key_list_last_idx].key_name[0][0]),scan_machine.key_name.c_str(), (MAX_KEYNAME_LENGTH-4));
    strncpy(&(key_name_table[key_list_last_idx].key_name[1][0]), (String("DBL_") + scan_machine.key_name).c_str(), MAX_KEYNAME_LENGTH);
    key_name_table[key_list_last_idx].key_value = scan_machine.scanned_key_value;
    //index_map[scan_machine.scanned_key_value] = key_list_last_idx;
    scan_machine.key_name = "";
    key_list_last_idx ++;
    prepare_key_name_list(key_name_list_html);
    Serial.printf("Scan completed, keyname: %s/%s, key_value :%d [%d][%d][%d]\n",
        key_name_table[key_list_last_idx - 1].key_name[0], key_name_table[key_list_last_idx - 1].key_name[1],
         scan_machine.scanned_key_value, sizeof(unsigned char), sizeof(unsigned int),sizeof(unsigned short));
  } else if(state == 4){
    
  }
  page_seq++;
  int leng = sprintf(add_key_html, add_key_fmt, page_seq, prg_en.c_str(), progress_val,k_name.c_str(), k_name.c_str(),
                                        progress_val, progress_hidden.c_str(), key_scan_start_message.c_str(), key_name_list_html);
  request->send_P(200, PSTR("text/html"), add_key_html);
  Serial.printf("Page sent, Page Size :%d\n", leng);
}


void prepare_assign_key_name_list(char *key_name_list_html)
{
  int i, len, tot_len = 0;
  memset(key_name_list_html, 0, sizeof(key_name_list_html));
  Serial.printf("Start key list html: %s\n", __FUNCTION__);
  for(i = 0; i < key_list_last_idx * 2;i++){
    if(key_assign_table[i].assigned){
        len =  sprintf(&(key_name_list_html[tot_len]),
               "<tr><td style=\"background-color:#FF0000\" width=\"100%\" onClick=\"onRmv('%d');\">%s</td>",
                                                                            i, key_assign_table[i].key_name);
    } else {
        len =  sprintf(&(key_name_list_html[tot_len]),
               "<tr><td style=\"background-color:#00FF00\" width=\"100%\" onClick=\"onAssign('%d');\">%s</td>",
                                                                             i, key_assign_table[i].key_name);
        /*len =  sprintf(&(key_name_list_html[tot_len]),
               "<tr><td style=\"background-color:#00FF00\" width=\"100%\" onClick=\"onAssign('key_cmd_%d');\">%s</td>",
                                                                             i, key_assign_table[i].key_name);*/
    }
#if DEBUG_MODE
    /*if(key_assign_table[i].assigned){
      tot_len += len;
      len = sprintf(&(key_name_list_html[tot_len]),"<td>%d</td>", (int)get_command_value(i));
    }*/
    tot_len += len;
    len = sprintf(&(key_name_list_html[tot_len]),"<td><input type=\"text\" id=\"key_cmd_%d\" maxlength=\"4\"/></td>", i);
    
#endif
    tot_len += len;
    len =  sprintf(&(key_name_list_html[tot_len]),"</tr>");
    tot_len += len;
  }
  Serial.printf("Finished: %s\n", __FUNCTION__);
}

void show_assign_key(AsyncWebServerRequest *request, int state)
{
  char *assign_key_html;
  uint8_t cmd;
  char key_name_list_html[2048] = {""};
  assign_key_html = html_buffer;
  assign_key_html[0] = 0;
  if (state == 0) {
    init_key_assign_table();
  } else if(state == 1) {
    //Assign this key to headunit
    set_key_assigned_status(get_value(request,"key_idx").toInt(), 1);
    //cmd = get_command_value(get_value(request,"key_idx").toInt());
    cmd = get_value(request,"debug_val").toInt();
    //Send this command to digipot
    HU_assign_key(cmd);
    //increase_command_value(get_value(request,"key_idx").toInt());
  } else if(state == 2) {
    //Reenable this key as user wants to cancel this key assignment.
    set_key_assigned_status(get_value(request,"key_idx").toInt(), 0);
  }
  prepare_assign_key_name_list(key_name_list_html);
  page_seq++;
  int leng = sprintf(assign_key_html,assign_key_fmt, "Key", "Key", "Key", page_seq, key_name_list_html);
  request->send_P(200, PSTR("text/html"), assign_key_html);
  Serial.printf("Page sent, Page Size :%d\n", leng);
}

void prepare_macro_key_name_list(char *key_name_list_html)
{
  int i, tot_len = 0;
  bool macro_assigned = false;
  
  memset(key_name_list_html, 0, sizeof(key_name_list_html));
  //Serial.printf("Start key list html\n");
  for(i = 0; i < key_list_last_idx * 2;i++){
    if(!key_assign_table[i].assigned){
        macro_assigned = is_macro_assigned(i);
        if(macro_assigned) { 
#if DEBUG_MODE
            Serial.printf("[%d]%s Macro assigned\n", i, key_assign_table[i].key_name);
#endif
            tot_len +=  sprintf(&(key_name_list_html[tot_len]),
               "<tr><td style=\"background-color:#FF0000\" width=\"100%\" onClick=\"onRmv('%d');\">%s</td>",
                                                                            i, key_assign_table[i].key_name);
        } else {
#if DEBUG_MODE
            Serial.printf("[%d]%s Free key\n", i, key_assign_table[i].key_name);
#endif
            tot_len +=  sprintf(&(key_name_list_html[tot_len]),
               "<tr><td style=\"background-color:#00FF00\" width=\"100%\" onClick=\"onAssign('%d');\">%s</td>",
                                                                            i, key_assign_table[i].key_name);
        }
    
#if DEBUG_MODE
        tot_len += sprintf(&(key_name_list_html[tot_len]),"<td>%d</td>", (int)get_command_value(i));
#endif
        tot_len += sprintf(&(key_name_list_html[tot_len]),"</tr>");
     } else {
#if DEBUG_MODE
      Serial.printf("[%d]%s Key is assigned to HU\n", i, key_assign_table[i].key_name);
#endif
    }
  }
}
char cmd_dropdown_html[1024];
void prepare_cmd_dropdown(char *cmd_table)
{
  int i, tot_len = 0;
  tot_len = sprintf(cmd_table,"<select id=\"drdn_steps\">");
  
  for(i = 0; i < key_list_last_idx * 2;i++){
    if(key_assign_table[i].assigned){
      tot_len += sprintf(&(cmd_table[tot_len]),"<option>%s</option>", key_assign_table[i].key_name);
    }
  }
  sprintf(&(cmd_table[tot_len]),"</select>");
}

void show_assign_macro(AsyncWebServerRequest *request, int state)
{
  char *assign_key_html;
  uint8_t assign_key_idx;
  char key_name_list_html[2048] = {""};
  assign_key_html = html_buffer;
  assign_key_html[0] = 0;
  if (state == 0) {
    macro_table_init();
    memset(cmd_dropdown_html, 0, sizeof(cmd_dropdown_html));
    prepare_cmd_dropdown(cmd_dropdown_html);
  } else if(state == 1) {
    //Add the current macro to the macro table
    macro_table_add();
  } else if(state == 3) {
    assign_key_idx = get_value(request,"key_idx").toInt();
    macro_table_delete(assign_key_idx);
    //Reenable this key as user wants to cancel this macro assignment.
    //set_key_assigned_status(get_value(request,"key_idx").toInt(), 0);
  } else if (state == 2) {
    clear_current_macro();
  } else if(state == 4) {
    //macro config finished and save the indexmap table to file
#if DEBUG_MODE
    dump_macro();
#endif
    remove_map_file();
    filter_index_map();
    save_index_map();
  }
  prepare_macro_key_name_list(key_name_list_html);
  page_seq++;
  int leng = sprintf(assign_key_html,assign_key_fmt, "Macro", "Macro", "Macro", page_seq, key_name_list_html);
  request->send_P(200, PSTR("text/html"), assign_key_html);
  Serial.printf("Page sent, Page Size :%d\n", leng);
}

extern macro_node_t *new_macro;
extern int last_step_index;
int prepare_macro_step_list(char *step_list_html, int buf_size){
  int i, tot_len = 0;
  memset(step_list_html, 0, buf_size);
  //Serial.printf("Start key list html\n");
  for(i = 0; i < last_step_index;i++){
    tot_len +=  sprintf(&(step_list_html[tot_len]),
       "<tr><td style=\"background-color:#FF0000\" width=\"100%\" onClick=\"onClearStep('%d');\">%s</td>", i, new_macro->commands[i].key_name);
    //tot_len +=  sprintf(&(step_list_html[tot_len]),
      // "<td><input type=\"button\" id=\"rem_bt\" name=\"submit_bt\" onClick=\"onClearStep('%d');\" value=\"Remove\"/></td>", i);
#if DEBUG_MODE
    tot_len += sprintf(&(step_list_html[tot_len]),"<td>%d</td>", new_macro->commands[i].command);
#endif
    tot_len += sprintf(&(step_list_html[tot_len]),"</tr>");
  }
  tot_len += sprintf(&(step_list_html[tot_len]),"<tr><td>%s</td>", cmd_dropdown_html);
  tot_len += sprintf(&(step_list_html[tot_len]),"<td><input type=\"button\" id=\"rem_bt\" name=\"submit_bt\" onClick=\"onAddStep();\" value=\"Add\"/></td></tr>");
#if DEBUG_MODE
  Serial.printf("%s: Macro list size:%d\n", __FUNCTION__, tot_len);
#endif
  return tot_len;
}

void show_define_macro(AsyncWebServerRequest *request, int state)
{
  uint8_t assign_key_idx;
  
  if(state == 0){
    assign_key_idx = get_value(request,"key_idx").toInt();
    macro_def_init(assign_key_idx);
  } else if(state == 1){
    macro_add_step(get_value(request,"key_name"));
  } else if(state == 2){
    assign_key_idx = get_value(request,"key_idx").toInt();
    macro_clear_step(assign_key_idx);
  }
  //prepare_macro_step_list(macro_step_list_html);
  page_seq++;
  int leng = sprintf(html_buffer,define_macro_fmt_part1, page_seq, new_macro->key_name);
  leng += prepare_macro_step_list(&html_buffer[leng], (sizeof(html_buffer)- leng));
  leng += sprintf(&html_buffer[leng],define_macro_fmt_part2);
  request->send_P(200, PSTR("text/html"), html_buffer);
  Serial.printf("Page sent, Page Size :%d\n", leng);
}

void init_web_server()
{
  // Send web page with input fields to client
  server = new AsyncWebServer(80);
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    //request->send_P(200, "text/html", index_html);
    init_all_data_structures();
    page_1(request, 0);
  });
  
  
  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
   server->on(
    "/post",
    HTTP_POST,
    [](AsyncWebServerRequest * request){
      String message;
      int params = request->params();
      //Serial.printf("%d params sent in\n", params);
      for (int i = 0; i < params; i++){
          AsyncWebParameter *p = request->getParam(i);
          if (p->isFile()){
              //Serial.printf("_FILE[%s]: %s, size: %u", p->name().c_str(), p->value().c_str(), p->size());
          }
          else if (p->isPost()){
              //Serial.printf("%s: %s \n", p->name().c_str(), p->value().c_str());
          }
          else{
              //Serial.printf("_GET[%s]: %s", p->name().c_str(), p->value().c_str());
          }
      }
      first_request_received = true;
      Serial.printf("New page request came with index: %d(%d) .\n",
                    request->hasParam("page_seq", true)?get_value(request, "page_seq").toInt():0, request->hasParam("page_seq", true));
      if((!request->hasParam("page_seq", true)) || get_value(request, "page_seq").toInt()!= page_seq){
        init_all_data_structures();
        page_1(request, 0);
      } else if(request->hasParam("submit_btn", true) && (request->getParam("submit_btn", true)->value().length() > 0 )) {
        String p_value = request->getParam("submit_btn", true)->value(); //get_value(request, "submit_btn");
        Serial.printf("Got command from browser: \"%s\" \n", p_value.c_str());
        if(p_value.equals("Add Keys >>")){
          p_value = get_value(request, "mode");
          if(p_value.toInt() > 0){
            key_list_last_idx = 0;
            show_add_key(request, 0);
          } else {
            page_1(request, 1);
          }
        } else if(p_value.equals("Scan Key")){ // Browser triggers a Key scan process.
          //init_scan_machine();
          show_add_key(request, 1);
        } else if(p_value.equals("Get Progress")){ // Browser asking progress of a key scan process.
          show_add_key(request, 2);
        } else if(p_value.equals("Scan Done")){ // Browser replied after showing a success message to the user.
          show_add_key(request, 3);
          //prepare_index_map();
        } else if(p_value.equals("Scan Cancel")){ // Browser cancelled the scanning process.Move to initial state.
          show_add_key(request, 0);
        } else if(p_value.equals("Remove")){
          if(remove_key(request->getParam("k_name", true)->value())){
            //prepare_index_map();
          }
          show_add_key(request, 0);
        } else if(p_value.equals("All keys Done")){ // Browser says all keys are scanned, and go to the next step.
          macro_table_init();
          final_update_index_map();
          show_assign_key(request, 0);
        } else if(p_value.equals("Assign Key")){
          show_assign_key(request, 1);
        } else if(p_value.equals("Cancel Key")){
          show_assign_key(request, 2);
        } else if(p_value.equals("Finish Key")){// Key assignments completed, now move to macro definition page
          show_assign_macro(request, 0);  
        } else if(p_value.equals("Assign Macro")){ // Start defining a macro for the selected key
          show_define_macro(request, 0);  
        } else if(p_value.equals("Add Step")){ // Macro definition for the key is going on, append the step to the macro under construction.
          show_define_macro(request, 1);  
        } else if(p_value.equals("Clear Step")){ // Macro definition for the key is going on, remove the step from the macro under construction.
          show_define_macro(request, 2);  
        } else if(p_value.equals("Save Def")){ // Macro definition for the current key is completed, save the sequence to macro table.
          show_assign_macro(request, 1);  
        } else if(p_value.equals("Discard Def")){ // Macro definition process for the current key is cancelled.
          show_assign_macro(request, 2);  
        } else if(p_value.equals("Cancel Macro")){ // Clear the macro assignment from the selected key
          show_assign_macro(request, 3);  
        } else if(p_value.equals("Finish Macro")){ // All done
          final_update_index_map();
          show_assign_macro(request, 4); 
        }
        
      }
    });

  server->onNotFound(notFound);
  server->begin();
}
void setup() {
  int map_length = 0;
  Serial.begin(SERIAL_BAUDRATE);
  pinMode(PIN_CONFIG_ENABLE, OUTPUT);
  digitalWrite(PIN_CONFIG_ENABLE, LOW);
  pinMode(PIN_CONFIG_ENABLE, INPUT);
  init_FS();
  
  //Check map file, load it to index map if present
  Serial.println();
  
  config_enabled = digitalRead(PIN_CONFIG_ENABLE);
  
  Serial.println(config_enabled?"Starting in Config Mode":"Starting in Operation mode");
  
  if (config_enabled){
    WiFi.mode(WIFI_AP);
    Serial.println(WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0))? "Ready" : "Failed!");
    Serial.println(WiFi.softAP("STEERING1", "temp00000")? "Ready" : "Failed!");
    Serial.println();
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    wifi_start_time = millis();
    wifi_up = true;
    init_web_server();
    memset(index_map, INVALID_COMMAND, sizeof(index_map));
  } else {
    Serial.println("Loading index map");
    map_length = load_index_map();
    Serial.printf("Map size: %d\n", map_length);
  }
  init_key_handle_vars();
  Serial.printf("init_key_handle_vars done\n");
  init_macro_tables();
  /*void *p = NULL;
  char *p_data = prepare_dummy_preset();
  Serial.printf("prepare_dummy_preset done\n");
  init_preset_table(&p, p_data);
  Serial.printf("init_preset_table done\n");
  if(p)
    free(p);*/
  pinMode(PIN_GND_RELAY_ENABLE, OUTPUT);
  digitalWrite(PIN_GND_RELAY_ENABLE, LOW);
  pinMode(PIN_BRAKE_BYPASS_RELAY, OUTPUT);
  digitalWrite(PIN_BRAKE_BYPASS_RELAY, LOW);
  Serial.printf("Setup is done\n");
}
bool HU_key_waiting = 0;
unsigned long key_assign_start_time = 0;
void loop() {
  if(config_enabled){
      if(key_scan_enable){
         do_key_scan();
      } else if(HU_key_waiting){
        if(millis() > (key_assign_start_time + (HU_KEY_PRESS_WIDTH*2))){
          stop_command();
          HU_key_waiting = 0;
        }
      }
  } else {
     handle_key();
  }
}
