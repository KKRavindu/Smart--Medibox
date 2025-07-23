#include <Adafruit_GFX.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define Screen_width 128
#define Screen_height 64
#define OLED_reset -1
#define Screen_address 0x3c

#define Buzzer 5
#define Led_1 15
#define PB_CANCEL 34

#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35  
#define DHTPIN 12

//wifi
#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET    0
#define UTC_OFFSET_DST 0

// Function prototypes
void print_line(String text, int colum, int row, int text_size);

Adafruit_SSD1306 display(Screen_width,Screen_height,&Wire,OLED_reset);
DHTesp dhtSensor;
//global variables
int days=0;
int hours=0;
int minutes=0;
int seconds=0;

unsigned long timeLast=0;
unsigned long timeNow=0;

bool alarm_enable=true;
int n_alarms=2;
int  alarm_hours[]={1,1};
int alarm_minutes[]={1,1};
bool alarm_triggered[]={false,false};

int n_notes=8;
int C=262;
int D=294;
int E=330;
int F=349;
int G=392;
int A=440;
int B=494;
int C_H=523;
int notes[]={C,D,E,F,G,A,B,C_H};

int current_mode=0;
int max_modes=5;
String modes[] = { "1- Set    Timezone","2 - Set   Alarm 1", "3 - Set   Alarm 2","4 - View   Alarms", "5 - Delete Alarm"};

char tempAr[6];
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void print_line(String text ,int colum,int row,int text_size){
  
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(colum,row);
  display.println(text);
  display.display();
  

}
void print_line_now(void){
  display.clearDisplay();
  print_line(String(days),0,0,2);
  print_line(":",20,0,2);
  print_line(String(hours),30,0,2);
  print_line(":",50,0,2);
  print_line(String(minutes),60,0,2);
  print_line(":",80,0,2);
  print_line(String(seconds),90,0,2);

  // Get Temperature and Humidity
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  // Display Temperature
  print_line("Temp: " + String(data.temperature) + " C", 0, 25, 1);

  // Display Humidity
  print_line("Humidity: " + String(data.humidity) + " %", 0, 40, 1);

  display.display(); // Refresh the screen
  
}

int wait_for_button_press(){
  while(true){
    if(digitalRead(PB_UP)==LOW){
      
      delay(200);
      return PB_UP;
    }
    else if(digitalRead(PB_DOWN)==LOW){
      delay(200);
      return PB_DOWN;
    }
    else if(digitalRead(PB_OK)==LOW){
      delay(200);
      return PB_OK;
    }
    else if(digitalRead(PB_CANCEL)==LOW){
      delay(200);
      return PB_CANCEL;
    }
    

  }
} 

void set_timezone() {
  float  temp_offset = UTC_OFFSET / 3600;  // Convert current offset to hours
  bool is_positive = temp_offset >= 0;
  
  while(true) {
    display.clearDisplay();
    String offset_display = (is_positive ? "+" : "") + String(temp_offset) + " UTC";
    print_line("Set Timezone: " , 0, 0, 1);
    print_line(offset_display, 0, 30, 2);
    
    int pressed = wait_for_button_press();
    
    if (pressed == PB_UP) {
      delay(200);
      temp_offset += 0.5;
      if (temp_offset > 12) temp_offset = -12;
    }
    else if (pressed == PB_DOWN) {
      delay(200);
      temp_offset -= 0.5;
      if (temp_offset < -12) temp_offset = 12;
    }
    else if (pressed == PB_OK) {
      delay(200);
      // Convert hours to seconds
      int new_offset = temp_offset * 3600;
      
      // Reconfigure time with new offset
      configTime(new_offset, 0, NTP_SERVER);
      
      display.clearDisplay();
      print_line("Timezone Set", 0, 0, 1);
      print_line(offset_display, 0, 30, 2);
      display.display();
      delay(2000);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }
}

void update_time(){
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  
  char timeHour[3];
  strftime(timeHour,3,"%H",&timeinfo);
  hours=atoi(timeHour);

  char timeMinutes[3];
  strftime(timeMinutes,3,"%M",&timeinfo);
  minutes=atoi(timeMinutes);

  char timeSecond[3];
  strftime(timeSecond,3,"%S",&timeinfo);
  seconds=atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay,3,"%d",&timeinfo);
  days=atoi(timeDay);

}
void ring_alarm(){
  display.clearDisplay();
  print_line("MEDICINE TIME",0,0,2);

  bool break_happened=false;
  int count=0;

  while(break_happened==false && digitalRead(PB_CANCEL)==HIGH){
    
    digitalWrite(Led_1, HIGH);
    
  //ring the Buzzer
    for(int i=0 ;i<n_notes;i++ ){
      if (digitalRead(PB_CANCEL)==LOW){
      delay(200);
      break_happened=true;
      break;
      }
      else if (count>=60000*5){
        break_happened=true;
        break;
      }
      tone(Buzzer,notes[i]);
      delay(200);
      noTone(Buzzer);
      delay(100); 
      
    count=count+300;  
    }
  }
  digitalWrite(Led_1, LOW);
  display.clearDisplay();


}
void update_time_with_check_alarm(){
  print_line_now();
  update_time();

  if (alarm_enable==true){
    for (int i=0;i<n_alarms;i++){
      if(alarm_triggered[i]==false && alarm_hours[i]==hours && alarm_minutes[i]==minutes){
        ring_alarm();
        alarm_triggered[i]=true;
        Serial.print("Alarm triggered status :");
        Serial.println(alarm_triggered[i]);
      }
    }
  }

}



void set_alarm(int alarm){
  int temp_hour=alarm_hours[alarm-1];
  while(true){
    display.clearDisplay();
    print_line("Enter hour: "+String(temp_hour),0,0,2);
    int pressed=wait_for_button_press();
    if (pressed==PB_UP){
      delay(200);
      temp_hour +=1;
      temp_hour=temp_hour % 24;
    }
    else if (pressed==PB_DOWN){
      delay(200);
      temp_hour -=1;
        if(temp_hour<0){
          temp_hour=23;
        }
    }
    else if(pressed==PB_OK){
        delay(200);
        alarm_hours[alarm-1]=temp_hour;
        break;
    }
    else if(pressed==PB_CANCEL){
      delay(200);
      break;
    }
  }
  //set Minutes
  int temp_min=alarm_minutes[alarm-1];
  while(true){
    display.clearDisplay();
    print_line("Enter minute: "+String(temp_min),0,0,2);
    int pressed=wait_for_button_press();
    if (pressed==PB_UP){
      delay(200);
      temp_min +=1;
      temp_min=temp_min % 60;
    }
    else if (pressed==PB_DOWN){
      delay(200);
      temp_min -=1;
        if(temp_min<0){
          temp_min=59;
        }
    }
    else if(pressed==PB_OK){
        delay(200);
        alarm_minutes[alarm-1]=temp_min;
        break;
    }
    else if(pressed==PB_CANCEL){
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  print_line("Alarm is set",0,0,2);
  delay(1000);



}

void view_alarms() {
  display.clearDisplay();
  for(int i = 0; i < n_alarms; i++) {
    String status = alarm_triggered[i] ? "TRIGGERED" : "ACTIVE";
    String alarmText = "Alarm " + String(i+1) + ": " + 
                      String(alarm_hours[i]) + ":" + 
                      String(alarm_minutes[i]) + " - " + status;
    print_line(alarmText, 0, i*15, 1);
  }
  display.display();
  delay(4000); // Show for 4 seconds
}

void delete_alarm() {
  int selected_alarm = 0;
  while(true) {
    display.clearDisplay();
    print_line("Delete Alarm:", 0, 0, 1);
    print_line("Alarm " + String(selected_alarm+1), 0, 20, 2);
    
    int pressed = wait_for_button_press();
    if(pressed == PB_UP) {
      selected_alarm = (selected_alarm + 1) % n_alarms;
    }
    else if(pressed == PB_DOWN) {
      selected_alarm = (selected_alarm - 1 + n_alarms) % n_alarms;
    }
    else if(pressed == PB_OK) {
      // Reset the selected alarm
      alarm_hours[selected_alarm] = 0;
      alarm_minutes[selected_alarm] = 0;
      alarm_triggered[selected_alarm] = false;
      display.clearDisplay();
      print_line("Alarm " + String(selected_alarm+1) + " Deleted", 0, 20, 1);
      display.display();
      delay(2000);
      break;
    }
    else if(pressed == PB_CANCEL) {
      break;
    }
  }
}

void run_mode(int mode){
  if(mode==0){
    set_timezone();
  }
  if(mode==1 || mode==2){
    set_alarm(mode);
  }
  
  else if(mode == 3) {
    view_alarms();
  }
  else if(mode == 4) {
    delete_alarm();
  }
}

void goto_menu(){
  while(digitalRead(PB_CANCEL)==HIGH){
    display.clearDisplay();
    print_line(modes[current_mode],0,0,2);
    int pressed=wait_for_button_press();

    if (pressed==PB_UP){
      delay(200);
      current_mode +=1;
      current_mode=current_mode % max_modes;

    }
    else if (pressed==PB_DOWN){
      delay(200);
      current_mode -=1;
      if(current_mode<0){
        current_mode=max_modes-1;
      }
    }
    else if(pressed==PB_OK){
          delay(200);
          Serial.println(current_mode);
          run_mode(current_mode);
    }
    else if(pressed==PB_CANCEL){
      delay(200);
      break;
    }

    
  }
}


void check_Temp(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float temperature = data.temperature;
  float humidity = data.humidity;

  bool alarm_triggered = false;

  display.clearDisplay();  // Clear previous display
  print_line_now();  // Show updated time

  // Temperature Warning Check
  if (temperature > 32) {
    display.clearDisplay();
    print_line(" TEMP      HIGH!", 0, 20, 2);
    alarm_triggered = true;
    for (int i = 0; i < 3; i++) {
      // High-low tone pattern
      tone(Buzzer, 800);  // Higher frequency for alert
      delay(200);
      noTone(Buzzer);
      delay(100);
      tone(Buzzer, 600);
      delay(200);
      noTone(Buzzer);
      delay(100);
    }
  } 
  else if (temperature < 24) {
    display.clearDisplay();
    print_line(" TEMP     LOW!", 0, 20, 2);
    alarm_triggered = true;
    for (int i = 0; i < 3; i++) {
      // High-low tone pattern
      tone(Buzzer, 800);  // Higher frequency for alert
      delay(200);
      noTone(Buzzer);
      delay(100);
      tone(Buzzer, 600);
      delay(200);
      noTone(Buzzer);
      delay(100);
    }
  } 

  // Humidity Warning Check
  if (humidity > 80) {
    display.clearDisplay();
    print_line(" HUMID      HIGH!", 0, 20, 2);
    alarm_triggered = true;
    for (int i = 0; i < 3; i++) {
      // High-low tone pattern
      tone(Buzzer, 800);  // Higher frequency for alert
      delay(200);
      noTone(Buzzer);
      delay(100);
      tone(Buzzer, 600);
      delay(200);
      noTone(Buzzer);
      delay(100);
    }
  } 
  else if (humidity < 65) {
    display.clearDisplay();
    print_line(" HUMID     LOW!", 0, 20, 2);
    alarm_triggered = true;
    for (int i = 0; i < 3; i++) {
      // High-low tone pattern
      tone(Buzzer, 800);  // Higher frequency for alert
      delay(200);
      noTone(Buzzer);
      delay(100);
      tone(Buzzer, 600);
      delay(200);
      noTone(Buzzer);
      delay(100);
    }
  }

  display.display();  // Update the OLED screen
}

void setupwifi(){
  Serial.println();
  Serial.print("Connecting to WiFi");
  Serial.println("wokwi-Guest");
  WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void updateTemperature() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature,2).toCharArray(tempAr, 6);

}

void setup() {
  
  // put your setup code here, to run once:
  pinMode(Led_1, OUTPUT);
  pinMode(Buzzer, OUTPUT);
  pinMode(PB_CANCEL, INPUT_PULLUP);
  pinMode(PB_OK, INPUT_PULLUP);
  pinMode(PB_DOWN, INPUT_PULLUP);
  pinMode(PB_UP, INPUT_PULLUP);

  dhtSensor.setup(DHTPIN,DHTesp::DHT22);

  Serial.begin(115200);
  setupwifi();

  if (! display.begin(SSD1306_SWITCHCAPVCC,Screen_address)){
    Serial.println(F("SSD1306 allocation Failed"));
    for(;;);
    }

  display.display();
  delay(2000);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to wifi....",0,0,2);
    
  }
  display.clearDisplay();
  print_line("wifi is connected",0,0,2);

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);


  display.clearDisplay();
  print_line("welcome to the mediBox",10,10,2);
  delay(100);
  display.clearDisplay();

 
}

void loop() {
  // put your main code here, to run repeatedly:
   // this speeds up the simulation
   updateTemperature();
   Serial1.print(tempAr);
   delay(1000);
   if(digitalRead(PB_OK)==LOW){
    delay(200);
    goto_menu();
   }
   delay(200);
  check_Temp();
  
  update_time_with_check_alarm();
  

}