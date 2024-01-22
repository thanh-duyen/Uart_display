#include <TimerOne.h>
#include "glcdfont.c"

#define CLK_PIN A3
#define LATCH_PIN A4
#define DATA_PIN A5
#define BUTTON_PIN A7

#define ROW_AMOUNT 15
const uint8_t ROW_PIN[ROW_AMOUNT] = {A2,A1,A0,13,12,11,10,2,3,4,9,8,7,6,5};

#define COL_PER_BOARD 7
#define MAX_BOARD_AMOUNT 7
#define BOARD_BUFFER_MEMORY 30
#define COL_AMOUNT COL_PER_BOARD*(MAX_BOARD_AMOUNT+BOARD_BUFFER_MEMORY)

uint8_t pixel_display[ROW_AMOUNT][MAX_BOARD_AMOUNT];
uint8_t pixel_buffer[ROW_AMOUNT][MAX_BOARD_AMOUNT+BOARD_BUFFER_MEMORY];
uint8_t current_row_display = 0;
uint8_t board_amount_use = 3;

typedef enum eScrollDirection{
  emLeft,
  emNone,
  emRight
};
uint16_t cursor_x = 0, cursor_y = 0;
bool is_blink = false;

eScrollDirection scroll_select = emNone;
int16_t index_scroll = 0;
int16_t max_index_scroll = 0;
uint32_t time_scroll = 0;
uint16_t start_scroll_y, height_scroll_y;
uint32_t delay_scroll = 100;

#define MAX_PIXELRUNNING 4
struct sPixelRunning{
  int16_t x1, y1, x2, y2;
  int16_t old_x, old_y;
  int16_t new_x, new_y;
  float i, d;
  bool running;
  uint16_t angle;
  uint32_t time_run;
  uint32_t delay;
};
sPixelRunning pixelRunning[MAX_PIXELRUNNING];

uint32_t time_countTimer = -1;
int8_t last_direction = 1;
struct sPixelFlowing{
  int16_t x, y;
  int8_t direction;
  uint32_t time_refresh;
  bool running = false;
};
#define MAX_PIXELFLOWING 50
sPixelFlowing pixelFlowing[MAX_PIXELFLOWING];
uint16_t pixel_timer_total = 205;
uint32_t timer_count_value = 30000;
bool clear_displayForTimer = false;
bool is_print_done_timer = false;

uint32_t time_readButton = 0;
uint8_t button_pressing = 0;
bool is_holding = false;
uint32_t time_holdEvent = 0;

bool exit_request = false;

void IRQ_handler(){
  if(current_row_display == 0)
    digitalWrite(ROW_PIN[ROW_AMOUNT-1],HIGH);
  else
    digitalWrite(ROW_PIN[current_row_display-1],HIGH);
  
  digitalWrite(LATCH_PIN,LOW);
  for(int8_t j = board_amount_use - 1; j >= 0; j--){
    shiftOut(DATA_PIN,CLK_PIN,LSBFIRST,pixel_display[current_row_display][j]);
  }
  digitalWrite(LATCH_PIN,HIGH);
  
  digitalWrite(ROW_PIN[current_row_display],LOW);
  if(++current_row_display >= ROW_AMOUNT){
    current_row_display = 0;
  }
}
void display(){
  for(uint8_t i = 0; i < ROW_AMOUNT; i++){
    if(scroll_select == emNone || i < start_scroll_y || i >= start_scroll_y + height_scroll_y) {
      for(uint16_t j = 0; j < board_amount_use; j++){
        pixel_display[i][j] = pixel_buffer[i][j];
      }
    }
  }
}
bool get_pixel(uint16_t x, uint8_t y){
  if(y >= ROW_AMOUNT || x >= COL_AMOUNT)
    return 0;
  return (pixel_buffer[y][x/7] >> (x%7)) & 0x1;
}
void set_pixel(uint16_t x, uint8_t y, bool value){
  if(y >= ROW_AMOUNT || x >= COL_AMOUNT)
    return;
  if(value == 1)
    pixel_buffer[y][x/7] |= 1<<(x%7);
  else
    pixel_buffer[y][x/7] &= ~(1<<(x%7));
}
void clear_buffer(bool is_all = false){
  for(uint8_t i = 0; i < ROW_AMOUNT; i++){
    for(uint16_t j = 0; j < MAX_BOARD_AMOUNT + is_all*BOARD_BUFFER_MEMORY; j++){
      pixel_buffer[i][j] = 0;
    }
  }
}
void clear_buffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h){
  for(uint8_t i = 0; i < h; i++){
    for(uint16_t j = 0; j < w; j++){
      set_pixel(x+j,y+i,0);
    }
  }
}
void print(uint16_t x, uint16_t y, String str){
  for(uint8_t c = 0; c < str.length(); c++){
    for(uint8_t i = 0; i < 5; i++){
      uint8_t line = pgm_read_byte(&font[int(str.charAt(c)) * 5 + i]);
      for(uint8_t j = 0; j < 7; j++){
        set_pixel(i+x,j+y,(line>>j)&0x1);
      }
    }
    x += 6;
  }
}
void draw_line(uint16_t x, uint16_t y, uint16_t w, uint16_t h){
  float d = sqrt(pow(w,2)+pow(h,2));
  for(float i = 0; i <= 1; i+=0.001+3.2/(d*10)){
    uint16_t x_point = x+w*i;
    uint16_t y_point = y+h*i;
    set_pixel(x_point,y_point,1);
  }
  set_pixel(x+w,y+h,1);
}
void scroll_handle(){
  if(scroll_select == emLeft){
    if(millis() >= time_scroll){
      for(uint8_t i = start_scroll_y; i < start_scroll_y + height_scroll_y; i++){
        for(uint16_t j = 0; j < board_amount_use; j++){
          uint8_t data = 0;
          for(uint8_t p = 0; p < COL_PER_BOARD; p++){
            if(index_scroll + p + COL_PER_BOARD*j >= 0){
              data = data | (get_pixel(index_scroll+p+j*COL_PER_BOARD,i) << p);
            }
          }
          pixel_display[i][j] = data;
        }
      }
      index_scroll++;
      if(index_scroll >= max_index_scroll){
        index_scroll = -(board_amount_use*COL_PER_BOARD - 1);
      }
      time_scroll = millis() + delay_scroll;
    }
  }
}
void timer_handle(){
  if(millis() >= time_countTimer){
    if(clear_displayForTimer == false){
      scroll_select = emNone;
      clear_buffer();
      display();
      clear_displayForTimer = true;
    }
    if(get_pixel((board_amount_use*COL_PER_BOARD) / 2, 0) == 0){
      uint8_t i;
      for(i = 0; i < MAX_PIXELFLOWING; i++){
        if(pixelFlowing[i].running == false){
          break;
        }
      }
      pixelFlowing[i].y = -1;
      pixelFlowing[i].x = (board_amount_use*COL_PER_BOARD) / 2;
      pixelFlowing[i].running = true;
      pixelFlowing[i].direction = 0;
      pixelFlowing[i].time_refresh = millis();
      time_countTimer = millis() + timer_count_value/pixel_timer_total;
    }
    else{
      time_countTimer = -1;
      print(board_amount_use*COL_PER_BOARD, 8, "Comp");
      for(uint8_t i = 0; i < ROW_AMOUNT; i++){
        for(uint16_t j = 0; j < board_amount_use*COL_PER_BOARD; j++){
          set_pixel(j, i, get_pixel(j+board_amount_use*COL_PER_BOARD, i));
        }
        display();
        delay(50);
      }
      delay(1000);
      clear_displayForTimer = false;
      clear_buffer(true);
      String str = "Completed timer " + String(timer_count_value/1000) + "s";
      print(0, 8, str);
      scroll_select = emLeft;
      start_scroll_y = 8;
      height_scroll_y = 7;
      max_index_scroll = str.length()*6;
      delay_scroll = 100;
      index_scroll = 0;
      is_print_done_timer = true;
    }
  }
  for(uint8_t i = 0; i < MAX_PIXELFLOWING; i++){
    if(pixelFlowing[i].running == true && millis() >= pixelFlowing[i].time_refresh){
      set_pixel(pixelFlowing[i].x, pixelFlowing[i].y, 0);
      if(pixelFlowing[i].y + 1 >= ROW_AMOUNT){
        pixelFlowing[i].running = false;
      }
      else if(get_pixel(pixelFlowing[i].x, pixelFlowing[i].y + 1) == 1){
        if(pixelFlowing[i].direction == 0){
          if(last_direction < 0){
            last_direction = 1;
            pixelFlowing[i].direction = 1;
          }
          else if(last_direction >= 0){
            last_direction = -1;
            pixelFlowing[i].direction = -1;
          }
        }
        
        if(pixelFlowing[i].direction < 0 && pixelFlowing[i].x - 1 < 0){
          pixelFlowing[i].running = false;
        }
        else if(pixelFlowing[i].direction > 0 && pixelFlowing[i].x + 1 >= board_amount_use*COL_PER_BOARD){
          pixelFlowing[i].running = false;
        }
        else if(get_pixel(pixelFlowing[i].x + pixelFlowing[i].direction, pixelFlowing[i].y + 1) == 1){
          pixelFlowing[i].running = false;
        }
        else{
          pixelFlowing[i].y++;
          pixelFlowing[i].x += pixelFlowing[i].direction;
        }
      }
      else{
        pixelFlowing[i].y++;
      }
      set_pixel(pixelFlowing[i].x, pixelFlowing[i].y, 1);
      display();
      pixelFlowing[i].time_refresh = millis() + 40;
    }
  }
}
void read_button(){
  if(millis() >= time_readButton){
    uint16_t adc = analogRead(BUTTON_PIN);
    if(adc >= 1000 && button_pressing != 0){
      uint8_t i;
      for(i = 0; i < 10 && analogRead(BUTTON_PIN) >= 1000; i++){
        delay(1);
      }
      if(i >= 10){
        if(is_holding == false && button_pressing == 1 && board_amount_use > 0){
          board_amount_use--;
        }
        else if(is_holding == false && button_pressing == 2 && board_amount_use < MAX_BOARD_AMOUNT){
          board_amount_use++;
        }
        
        button_pressing = 0;
        // put the code for handle after release button here
      }
    }
    else if(adc < 1000 && button_pressing == 0){
      uint8_t i;
      uint16_t average = adc;
      for(i = 0; i < 10; i++){
        adc = analogRead(BUTTON_PIN);
        if(adc >= 1000){
          break;
        }
        average = (average + adc)/2;
        delay(1);
      }
      if(i >= 10){
        time_holdEvent = millis() + 2000;
        button_pressing = 2;
        if(average >= 400){
          button_pressing = 1; // F1
        }
        // put the code for handle after press button F1 or F2 here
        // use the variable button_pressing for check which does button pressed
      }
    }
    if(millis() >= time_holdEvent && button_pressing != 0){
      time_holdEvent = 0;
      exit_request = true;
      is_holding = true;
    }
    time_readButton = millis()+100;
  }
}
void read_serial(){
  if(Serial.available()){
    Timer1.stop();
    delay(2);
    Timer1.resume();
    String at_command = "";
    while(Serial.available()){
      at_command += (char)Serial.read();
    }
    String buff = "";
    int32_t arguments[10] = {0,0,0,0,0,0,0,0,0,0};
    uint8_t i = 0, amount_argument = 0;
    while(i < at_command.length()){
      if(at_command.charAt(i) == ' ' || (i + 1) >= at_command.length()){
        arguments[amount_argument] = buff.toInt();
        buff = "";
        amount_argument++;
        if(amount_argument >= 10){
          break;
        }
      }
      else{
        buff += at_command.charAt(i);
      }
      i++;
    }
    if(at_command.indexOf("pixel ") == 0){
      set_pixel(arguments[1],arguments[2],(bool)arguments[3]);
    }
    else if(at_command.indexOf("print ") == 0){
      String text = "";
      uint8_t i = 6;
      while(i < at_command.length() && at_command.charAt(i) != '\"'){
        i++;
      }
      i++;
      while(i < at_command.length() && at_command.charAt(i) != '\"'){
        text += at_command.charAt(i);
        i++;
      }
      print(arguments[1],arguments[2],text);
      Serial.println("Ok");
    }
    else if(at_command.indexOf("clear ") == 0){
      scroll_select = emNone;
      if(amount_argument <= 3){
        clear_buffer(arguments[1]);
      }
      else{
        clear_buffer(arguments[1],arguments[2],arguments[3],arguments[4]);
      }
      scroll_select = emNone;
      Serial.println("Ok");
    }
    else if(at_command.indexOf("scroll ") == 0){
      if(scroll_select != emNone){
        delay_scroll = arguments[1];
        if(at_command.indexOf("N") > 0){
          scroll_select = emNone;
        }
      }
      else{
        if(at_command.indexOf("L ") > 0){
          scroll_select = emLeft;
        }
        else if(at_command.indexOf("R ") > 0){
          // Not support yet
        }
  
        if(scroll_select != emNone){
          start_scroll_y = arguments[2];
          height_scroll_y = arguments[3];
          max_index_scroll = arguments[4]*6;
          delay_scroll = arguments[5];
          index_scroll = 0;
        }
      }
      Serial.println("Ok");
    }
    else if(at_command.indexOf("line ") == 0){
      draw_line(arguments[1],arguments[2],arguments[3],arguments[4]);
      Serial.println("Ok");
    }
    else if(at_command.indexOf("test ") == 0){
      Serial.println("test run");
      scroll_select = emNone;
      clear_buffer();
      display();
      exit_request = false;
      if(arguments[1] == 1){
        test_display1(arguments[2]);
      }
      else if(arguments[1] == 2){
        test_display2(arguments[2]);
      }
    }
    else if(at_command.indexOf("timer ") == 0){
      if(at_command.indexOf("off") > 0){
        time_countTimer = -1;
        clear_buffer();
        for(i = 0; i < MAX_PIXELFLOWING; i++){
          pixelFlowing[i].running = false;
        }
        if(is_print_done_timer == true){
          scroll_select = emNone;
          clear_buffer();
          display();
        }
        Serial.println("timer off");
      }
      else{
        time_countTimer = millis();
        timer_count_value = arguments[1]*1000;
        clear_displayForTimer = false;
        Serial.println("timer on");
      }
    }
    else if(at_command.indexOf("exit") == 0){
      exit_request = true;
      Serial.println("Ok");
    }
    display();
  }
}
void test_display1(uint32_t times){
  for(uint32_t k = 0; k < times && exit_request == false; k++){
    int8_t row_x = 0, col_y = 0;
    for(uint32_t i = 0; col_y < ROW_AMOUNT && row_x < (board_amount_use*COL_PER_BOARD) && exit_request == false; i++){
      if(i % ROW_AMOUNT == 0 || i % (board_amount_use*COL_PER_BOARD) == 0){
        if(i % ROW_AMOUNT == 0){
          row_x++;
        }
        if(i % (board_amount_use*COL_PER_BOARD) == 0){
          col_y++;
        }
        clear_buffer();
        draw_line(row_x,0,1,ROW_AMOUNT);
        draw_line(0,col_y,(board_amount_use*COL_PER_BOARD),1);
        display();
      }
      uint32_t time_exit = millis()+2;
      while(millis() < time_exit){
        read_button();
      }
    }
    row_x = board_amount_use*COL_PER_BOARD - 1, col_y = ROW_AMOUNT - 1;
    for(uint32_t i = 0; col_y >= 0 && row_x >= 0 && exit_request == false; i++){
      if(i % ROW_AMOUNT == 0 || i % (board_amount_use*COL_PER_BOARD) == 0){
        if(i % ROW_AMOUNT == 0){
          row_x--;
        }
        if(i % (board_amount_use*COL_PER_BOARD) == 0){
          col_y--;
        }
        clear_buffer();
        draw_line(row_x,0,1,ROW_AMOUNT);
        draw_line(0,col_y,board_amount_use*COL_PER_BOARD,1);
        display();
      }
      uint32_t time_exit = millis()+2;
      while(millis() < time_exit){
        read_button();
      }
    }
  }
  clear_buffer();
  display();
  exit_request = false;
}
void test_display2(uint32_t time_run){
  for(uint8_t i = 0; i < MAX_PIXELRUNNING; i++){
    pixelRunning[i].delay = 50+i*10;
    int16_t start_x = random(board_amount_use*COL_PER_BOARD-1);
    int16_t start_y = random(ROW_AMOUNT-1);
      
    pixelRunning[i].x1 = pixelRunning[i].x2 = pixelRunning[i].old_x = start_x;
    pixelRunning[i].y1 = pixelRunning[i].y2 = pixelRunning[i].old_y = start_y;
    pixelRunning[i].running = true;
    pixelRunning[i].time_run = millis();
    pixelRunning[i].angle = 45;
    float theta = PI*pixelRunning[i].angle/180;
    pixelRunning[i].new_x = pixelRunning[i].x1 + cos(theta)*100;
    pixelRunning[i].new_y = pixelRunning[i].y1 + sin(theta)*100;
    pixelRunning[i].d = sqrt(pow(pixelRunning[i].new_x-pixelRunning[i].old_x,2)+pow(pixelRunning[i].new_y-pixelRunning[i].old_y,2));
    pixelRunning[i].i = 0.001;
  }
  uint32_t timeout = millis()+time_run;
  while(millis() < timeout && exit_request == false){
    read_button();
    for(uint8_t i = 0; i < MAX_PIXELRUNNING && exit_request == false; i++){
      if(pixelRunning[i].running == true && millis() >= pixelRunning[i].time_run){
        while(pixelRunning[i].x2 == pixelRunning[i].x1 || pixelRunning[i].y2 == pixelRunning[i].y1){
          pixelRunning[i].x2 = pixelRunning[i].old_x+(pixelRunning[i].new_x-pixelRunning[i].old_x)*pixelRunning[i].i;
          pixelRunning[i].y2 = pixelRunning[i].old_y+(pixelRunning[i].new_y-pixelRunning[i].old_y)*pixelRunning[i].i;
          pixelRunning[i].i += 0.001+3.2/(pixelRunning[i].d*10);
        }
        
        clear_buffer();
        for(uint8_t j = 0; j < MAX_PIXELRUNNING; j++){
          if(pixelRunning[j].running == true){
            set_pixel(pixelRunning[j].x2, pixelRunning[j].y2, 1);
          }
        }
        display();
        
        if(pixelRunning[i].y2 >= ROW_AMOUNT - 1){
          pixelRunning[i].old_x = pixelRunning[i].x2;
          pixelRunning[i].old_y = pixelRunning[i].y2;
          if(pixelRunning[i].new_x > pixelRunning[i].old_x) pixelRunning[i].angle = 315;
          else pixelRunning[i].angle = 225;
          float theta = PI*pixelRunning[i].angle/180;
          pixelRunning[i].new_x = pixelRunning[i].x2 + cos(theta)*100;
          pixelRunning[i].new_y = pixelRunning[i].y2 + sin(theta)*100;
          pixelRunning[i].d = sqrt(pow(pixelRunning[i].new_x-pixelRunning[i].old_x,2)+pow(pixelRunning[i].new_y-pixelRunning[i].old_y,2));
          pixelRunning[i].i = 0.001;
        }
        else if(pixelRunning[i].y2 <= 0){
          pixelRunning[i].old_x = pixelRunning[i].x2;
          pixelRunning[i].old_y = pixelRunning[i].y2;
          if(pixelRunning[i].new_x > pixelRunning[i].old_x) pixelRunning[i].angle = 45;
          else pixelRunning[i].angle = 135;
          float theta = PI*pixelRunning[i].angle/180;
          pixelRunning[i].new_x = pixelRunning[i].x2 + cos(theta)*100;
          pixelRunning[i].new_y = pixelRunning[i].y2 + sin(theta)*100;
          pixelRunning[i].d = sqrt(pow(pixelRunning[i].new_x-pixelRunning[i].old_x,2)+pow(pixelRunning[i].new_y-pixelRunning[i].old_y,2));
          pixelRunning[i].i = 0.001;
        }
        else if(pixelRunning[i].x2 <= 0){
          pixelRunning[i].old_x = pixelRunning[i].x2;
          pixelRunning[i].old_y = pixelRunning[i].y2;
          if(pixelRunning[i].new_y < pixelRunning[i].old_y) pixelRunning[i].angle = 315;
          else pixelRunning[i].angle = 45;
          float theta = PI*pixelRunning[i].angle/180;
          pixelRunning[i].new_x = pixelRunning[i].x2 + cos(theta)*100;
          pixelRunning[i].new_y = pixelRunning[i].y2 + sin(theta)*100;
          pixelRunning[i].d = sqrt(pow(pixelRunning[i].new_x-pixelRunning[i].old_x,2)+pow(pixelRunning[i].new_y-pixelRunning[i].old_y,2));
          pixelRunning[i].i = 0.001;
        }
        else if(pixelRunning[i].x2 >= board_amount_use*COL_PER_BOARD - 1){
          pixelRunning[i].old_x = pixelRunning[i].x2;
          pixelRunning[i].old_y = pixelRunning[i].y2;
          if(pixelRunning[i].new_y < pixelRunning[i].old_y) pixelRunning[i].angle = 225;
          else pixelRunning[i].angle = 135;
          float theta = PI*pixelRunning[i].angle/180;
          pixelRunning[i].new_x = pixelRunning[i].x2 + cos(theta)*100;
          pixelRunning[i].new_y = pixelRunning[i].y2 + sin(theta)*100;
          pixelRunning[i].d = sqrt(pow(pixelRunning[i].new_x-pixelRunning[i].old_x,2)+pow(pixelRunning[i].new_y-pixelRunning[i].old_y,2));
          pixelRunning[i].i = 0.001;
        }
        pixelRunning[i].x1 = pixelRunning[i].x2;
        pixelRunning[i].y1 = pixelRunning[i].y2;
        pixelRunning[i].time_run = millis() + pixelRunning[i].delay;
      }
    }
  }
  clear_buffer();
  display();
  exit_request = false;
}
void setup() {
  Serial.begin(115200);
  for(uint8_t i = 0; i < ROW_AMOUNT; i++){
    pinMode(ROW_PIN[i],OUTPUT);
    digitalWrite(ROW_PIN[i],HIGH);
  }
  pinMode(CLK_PIN,OUTPUT);
  pinMode(LATCH_PIN,OUTPUT);
  pinMode(DATA_PIN,OUTPUT);

  clear_buffer();
  display();
  Timer1.initialize(1000);
  Timer1.attachInterrupt(IRQ_handler);
}
void loop() {
  scroll_handle();
  timer_handle();
  read_button();
  read_serial();
}
