/* Moving back to single ESP32 implementation. Using 8266 GPIO makes more complications.  
 *  The wakeup input circuit has been changed to implement a transister switching circuit(PNP). The last 
 *  version had a disadvantage that the highest resistance key(pickup) was not able to reduce the potential
 *  at the wakeup reading point so that the wakeup pin kept high when that particular key is pressed. So the 
 *  wakeup circuite has been modified with a transister(PNP) switching circuit. This is working fine with all
 *  keys but the new resistance values in the circuit caused a change in the potential ranges of switches so that
 *  the key map is revised from print_resistance5.
 *  
 *         PIN_USER_INPUT_34        
 *                |                       
 *                |                       
 *  3V3--*--R10K--*-------*---STEERING-----------*-----------GND
 *       |                |                      |
 *       |                |                      |
 *       |                |                      |
 *       |                |          (~3.47K)<---+------Multimeter reading.
 *       *---R10K--*--[E  B  C]-----R3K--R530----*
 *                 |                 ^     ^
 *                 |             (2K+1K)(330+100+100)
 *           PIN_WAKEUP_33
 *       
 *  SPI PINS:                          DIGIPOT     
 *  SS(CS)     5  D5 -------------- 1  MCP4131  8 ---- 5V
 *  SCK(CLK)  18 D18 -------------- 2           7 ---- GND
 *  MOSI      23 D23 -------------- 3           6 -------------- Output Resisatance ---- GND
 *                        GND------ 4           5 ---- FLOAT
 *  MISO      19 D19                                            
 *  
 * 8266  
 *         PIN_USER_INPUT_A0        
 *                |                       
 *                |                       
 *  3V3--*--R10K--*-------*---STEERING-----------*-----------GND
 *       
 *               VIN ---------------------------------*-------------------------------------------- +5V
 *                                                    |                                            Power Supply
 *               GND ---------------------------------+------------*------------------------------ - GND
 *                                                    |            |
 *               V3V ---------------*                 |            |
 *                                  |                 |            |
 *                                 10K                |            |
 *                                  |                 |            |
 *                A0 ---------------*                 |            |
 *                                  |                 |            |
 *                              STREERING             |            |
 *                                  |                 |            |
 *               GND ---------------*                 |            | 
 *  SPI PINS:                          DIGIPOT        |            |
 *  SS(CS)    15  D8 -------------- 1  MCP4131  8 ----* 5V         |
 *  SCK(CLK)  14  D5 -------------- 2           7 -----------------* GND         
 *  MOSI      13  D7 -------------- 3           6 -----------------|--------------------------------- HU +
 *                        GND*----- 4           5 ---- float       |
 *  MISO      12  D6         |                                     |
 *                           *-------------------------------------*--------------------------------- HU - 
 *
 *  The values SS, SCK, MOSI and MISO have already defined in SPI.h
 */

#include <Arduino.h>
#include <SPI.h>
#include "common.h"

#ifdef ESP32

#define RF_RECEIVER 13

#define PIN_LED_19 19
#define PIN_UNO_STATUS 18
#define PIN_WAKEUP_33 GPIO_NUM_33 // GPIO 33, this is defind differently because the sleep enable function
                                  // takes the pin number in GPIO_NUM_# format. Since we are using external 
                                  // wakeup , we have to use one of the RTC enabled pin and 33 is one of them.
#define PIN_DIGIPOT_CS SS
#else
#define PIN_DIGIPOT_CS 15
#define HIGH_RES_VALUE 255
#endif

#define KEY_IDLE_GO_SLEEP_DURATION 30000 // 30 seconds key idle time to go to sleep.
#define CMD_ENABLE_DURATION 500
#define READ_WINDOW_WIDTH 350
#define SINGLE_CLICK_WIDTH 140
#define MINIMUM_CLICK_WIDTH 80


#define DIGI_POT_COMMAND 0x00


static unsigned int cur_key_val =  KEY_IDLE_MIN_VALUE;
static unsigned int pre_key_val =  KEY_IDLE_MIN_VALUE;
static int min_key_val;

int click_count;

int key_read_window;
int read_window_locked;

unsigned long cur_time;
unsigned long read_start_time;
unsigned long window_end_time;
unsigned long cmd_start_time;
unsigned long cmd_stop_time;

int key_state;
bool extended_window;
int cur_cmd_value;

unsigned long key_press_time;
int d_click_flag = 0;

int tmp_val;
extern unsigned short index_map[2][MAX_INPUT_VALUE];

int inline get_key_command()
{
   return (unsigned char)index_map[0][min_key_val];
}

extern unsigned long key_assign_start_time;
extern bool HU_key_waiting;
void HU_assign_key(uint8_t cmd)
{
#if DEBUG_MODE
  Serial.printf("%s: Sending command : index:%d, commnd:%d, double-click: %d \n", __FUNCTION__, min_key_val, cmd, d_click_flag);
#endif
  digitalWrite(PIN_DIGIPOT_CS, LOW);
  SPI.transfer(DIGI_POT_COMMAND);
  SPI.transfer(cmd);
  digitalWrite(PIN_DIGIPOT_CS, HIGH);
  digitalWrite(PIN_GND_RELAY_ENABLE, HIGH);
#if DEBUG_MODE
  int start_idx, end_idx;
  start_idx = end_idx = min_key_val;
  for (;start_idx > 0 && (unsigned char)index_map[d_click_flag][start_idx - 1] == cmd; start_idx --);
  for (;end_idx < (MAX_INPUT_VALUE - 1) && (unsigned char)index_map[d_click_flag][end_idx + 1] == cmd; end_idx ++);
  Serial.printf("Sending command [%d][%d..(%d)..%d] ---> %u\n", d_click_flag, start_idx, min_key_val, end_idx, cmd);
#endif
  key_assign_start_time = millis();
  HU_key_waiting = 1;
  //while(millis() < key_assign_start_time + (HU_KEY_PRESS_WIDTH*2));
  //stop_command();
}

int macro_idx;
int macro_step_idx;
bool macro_started;
int start_gap_inc;
void run_macro_step()
{
  if (macro_step_idx < macro_file_header.length_list[macro_idx]){
#if DEBUG_MODE
    Serial.printf("%s: Sending command : %d \n", __FUNCTION__, *(macro_table[macro_idx] + macro_step_idx));
#endif
    digitalWrite(PIN_DIGIPOT_CS, LOW);
    SPI.transfer(DIGI_POT_COMMAND);
    SPI.transfer(*(macro_table[macro_idx] + macro_step_idx));
    digitalWrite(PIN_DIGIPOT_CS, HIGH);
    digitalWrite(PIN_GND_RELAY_ENABLE, HIGH);
    macro_step_idx ++;
  }
  if (macro_step_idx >= macro_file_header.length_list[macro_idx]){
    macro_started = false;
  }
}
uint8_t cmd;
void start_command()
{
  cmd = WORDLSBYTE(index_map[d_click_flag][min_key_val]);
  
#if DEBUG_MODE
  Serial.printf("%s(time:%lu): Sending command : index:%d, commnd:%d, double-click: %d \n", __FUNCTION__, cmd_start_time, min_key_val, WORDLSBYTE(index_map[d_click_flag][min_key_val]), d_click_flag);
#endif
  digitalWrite(PIN_DIGIPOT_CS, LOW);
  SPI.transfer(DIGI_POT_COMMAND);
  SPI.transfer(cmd);
  digitalWrite(PIN_DIGIPOT_CS, HIGH);
  digitalWrite(PIN_GND_RELAY_ENABLE, HIGH);
  cmd_start_time = cur_time;
  cmd_stop_time = cmd_start_time + CMD_ENABLE_DURATION;
  macro_started = false;
  macro_idx = WORDMSBYTE(index_map[d_click_flag][min_key_val]);
  if(macro_idx != INVALID_COMMAND){
    cmd_stop_time = cmd_start_time + (CMD_ENABLE_DURATION/2);
    macro_step_idx = 0;
    macro_started = true;
  }
  if (cmd == INVALID_COMMAND){
      digitalWrite(PIN_BRAKE_BYPASS_RELAY, HIGH);
  }
  
#if DEBUG_MODE
  int start_idx, end_idx;
  start_idx = end_idx = min_key_val;
  for (;start_idx > 0 && (unsigned char)index_map[d_click_flag][start_idx - 1] == cmd; start_idx --);
  for (;end_idx < (MAX_INPUT_VALUE - 1) && (unsigned char)index_map[d_click_flag][end_idx + 1] == cmd; end_idx ++);
  Serial.printf("Sending command [%d][%d..(%d)..%d] ---> %u\n", d_click_flag, start_idx, min_key_val, end_idx, cmd);
#endif
}
void stop_command()
{
  cmd_start_time = 0;
  digitalWrite(PIN_GND_RELAY_ENABLE, LOW);
  digitalWrite(PIN_DIGIPOT_CS, LOW);
  SPI.transfer(DIGI_POT_COMMAND);
  SPI.transfer(HIGH_RES_VALUE);
  digitalWrite(PIN_DIGIPOT_CS, HIGH);
  digitalWrite(PIN_BRAKE_BYPASS_RELAY, LOW);
#if DEBUG_MODE
  Serial.printf("%s(time:%lu) :Stopping command:\n", __FUNCTION__, millis());
#endif
  if (macro_started){
    cmd_start_time = millis();
    Serial.printf("%s :%lu\n", __FUNCTION__, cmd_start_time);
    cmd_stop_time = cmd_start_time + (CMD_ENABLE_DURATION/2);
    Serial.printf("%s :%lu\n", __FUNCTION__, cmd_start_time);
    run_macro_step();
  }
}

void init_key_handle_vars()
{
  pre_key_val = KEY_IDLE_MIN_VALUE;
  min_key_val = KEY_IDLE_MIN_VALUE;
  click_count = 0;
  key_read_window = 0;
  read_window_locked = 0;
  cmd_start_time = 0;
  d_click_flag = 0;

#ifdef ESP32
  analogReadResolution(12);
#endif
  pinMode (PIN_DIGIPOT_CS, OUTPUT);
  SPI.begin();
  stop_command();
}

void handle_key() {
  cur_time = millis();
  cur_key_val = analogRead(PIN_USER_INPUT);
  
  key_state = (cur_key_val < KEY_IDLE_MIN_VALUE)? STATE_KEY_DOWN: STATE_KEY_UP;
  
  if(cur_key_val <KEY_IDLE_MIN_VALUE && pre_key_val >= KEY_IDLE_MIN_VALUE){
    if(!key_read_window){
      read_start_time = cur_time;
      //cmd_start_time = 0;
      key_read_window = 1;
      window_end_time = read_start_time + READ_WINDOW_WIDTH;
      key_press_time = millis();
      click_count = 0;
      d_click_flag = 0;
      extended_window = 0;
      min_key_val = KEY_IDLE_MIN_VALUE;
    }
  }
  //if(cmd_start_time && (cmd_start_time + CMD_ENABLE_DURATION) < cur_time){
  if(cmd_start_time && cur_time > cmd_stop_time){
    cmd_start_time = 0;
    cur_cmd_value = 0;
    stop_command();
  }
  if(key_read_window == 1){
    //Serial.printf("Key value:%d.\n", cur_key_val);
    if(cur_time > (window_end_time)){ // Current window over
      if(key_state == STATE_KEY_UP){
        if(!read_window_locked && click_count >0){
          if(click_count == 2){
#if DEBUG_MODE
            Serial.printf("Sending DOUBLE at  place 1.\n");
#endif
            d_click_flag = 1;
          } else {
#if DEBUG_MODE
            Serial.printf("Sending single at  place 1.\n");
#endif
          }
          start_command(); //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
          cmd_start_time = cur_time;
          //generate_and_send_command();
          click_count = 0;
        } else {
          read_window_locked = 0;
        }
        key_read_window = 0;
      } else if(key_state == STATE_KEY_DOWN){ // Current window over but the key is still in down state.
        if(click_count == 0){// Crossing the window during a continuous key press, send single key click command.
          //click_count = 1;
#if DEBUG_MODE
          Serial.printf("Calling place 1.\n");
#endif
          cur_cmd_value = get_key_command();
          start_command();//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
          cmd_start_time = cur_time;
          //click_count = 0;
          read_start_time = cur_time; // Extending the key read window
          window_end_time = cur_time + READ_WINDOW_WIDTH;
        } else { // One click is done but window border reached before completing the next click, it is fine to consider this as a double click.
          tmp_val = get_key_command();
          if(cur_cmd_value == tmp_val){// But before sending double click, confirm that the previous click was on the same key that is currently down.
#if DEBUG_MODE
            Serial.printf("Sending DOUBLE at  place 2.\n");
#endif
            d_click_flag = 1;
            start_command(); // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
            click_count = 0;
            cmd_start_time = cur_time;
            read_start_time = cur_time; // Extending the key read window
            window_end_time = cur_time + READ_WINDOW_WIDTH;
          } else {
            read_window_locked = 1;
          }
        }
        extended_window = 1;
      }
    } else if(key_state == STATE_KEY_DOWN){ // Current window not over and key is in pressed state.
      if(pre_key_val >= KEY_IDLE_MIN_VALUE){// Key has just pressed now, record the key press time
        key_press_time = cur_time; 
      }
      if(min_key_val > cur_key_val){
       min_key_val = cur_key_val;
      }
    } else { // Current window not over and key is in up state.
      if(pre_key_val < KEY_IDLE_MIN_VALUE){// The key has just came to up state
        if(!extended_window){
          if(cur_time > (key_press_time + 5)){ // Key was in down state for morethan 50 milliseconds, it is a valid click.
            if(click_count == 0){ // This is the first click in this window.
              //Serial.printf("Calling place 3.\n");
              cur_cmd_value = get_key_command();
              click_count ++;               
            } else if( click_count == 1){ // This is the second click in the window, 
              if(cur_cmd_value == get_key_command()){ // Before finalizing it as a double click, confirm both the click were on the same key.
                click_count ++;
              } else { //Two different keys clicked instead of a double click in same window, cancel these clicks. 
                click_count = 0;
                key_read_window = 0;
              }
            }
          }
          if(click_count == 1 && cur_time > (read_start_time + SINGLE_CLICK_WIDTH)){ // Just finished s long duration click and there is no room for one more click in this window.
            start_command();//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
            cmd_start_time = cur_time;
#if DEBUG_MODE
            Serial.printf("Called as expected 1.\n");
#endif
            click_count = 0;
            key_read_window = 0;
          }
        }
        key_press_time = cur_time;
      } else if (click_count == 1 && cur_time > (window_end_time-MINIMUM_CLICK_WIDTH)){// Key is up for long time after one click is and there is no room for one more click in this window. 
          if(!extended_window){
            start_command(); //<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
            cmd_start_time = cur_time;
            //generate_and_send_command();
#if DEBUG_MODE
            Serial.printf("Called as expected 2.\n");
#endif
            click_count = 0;
            key_read_window = 0;
          }
      }
       
    }
  }
  pre_key_val = cur_key_val;
}
