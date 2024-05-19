//引入ESP8266.h头文件，建议使用教程中修改后的文件
#include "ESP8266.h"
#include "dht11.h"
#include "SoftwareSerial.h"

#define U8X8_USE_PINS

#define DHTPIN 8         //对应D8针脚接入DHT11
#define DHTTYPE DHT11    //选择传感器类型 DHT11
#include <DHT.h>         //温度传感器运行库 
#include <U8glib.h>
DHT dht(DHTPIN, DHTTYPE);



#define SSID "ABCD"    
#define PASSWORD "00000001"
#define HOST_NAME "api.heclouds.com"  
#define DEVICE_ID "1172729201"       
#define HOST_PORT (80)                
String APIKey = "LgU42xT1gzq5nwQFFcUU5AJfGUM="; 

#define INTERVAL_SENSOR 5000 

uint32_t read_time = 0;
float temp_read,humi_read; 
int sensorPin = A0;           //设置模拟口A0为信号输入端
int sensorValue = 0;          //初始化土壤模拟信号的变量
float SoilPrecentValue = 0;   //初始化湿度变量
int waterPumpPin = 9;         //继电器 水泵控制 
int SoilSliderValue = 90;    //土壤湿度阈值 通过土壤传感器
int TempSliderValue = 40;     //空气温度阈值 通过气温传感器
bool manualFlag = false;      //手动控制默认关闭

#define INTERVAL_LCD             20                 //定义OLED刷新时间间隔  
unsigned long lcd_time = millis();                  //OLED刷新时间计时器
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE);        //设置OLED型号  
//-------字体设置，大、中、小
#define setFont_L u8g.setFont(u8g_font_7x13)
#define setFont_M u8g.setFont(u8g_font_fixed_v0r)
#define setFont_S u8g.setFont(u8g_font_fixed_v0r)
#define setFont_SS u8g.setFont(u8g_font_fub25n)
unsigned long previousMillis = 0;                   //存储LED最后一次的更新
unsigned long interval = 5000;                      //闪烁的时间间隔（毫秒）
unsigned long currentMillis=0;

//定义软接口
SoftwareSerial mySerial(3, 2);
ESP8266 wifi(mySerial);

void oledDisplay()
{

  u8g.firstPage();
  do
  { setFont_L;
    
    
    u8g.print("Soil Humi: " + String(SoilPrecentValue) + "%");
    u8g.setPrintPos(0, 43);
    if (digitalRead(waterPumpPin) == HIGH)
      u8g.print("Sys State: Pausing");
    else if (digitalRead(waterPumpPin) == LOW)
      u8g.print("Sys State: Watering");
    u8g.drawLine(0, 48, 128, 48);
    u8g.setPrintPos(0, 63);
    u8g.print("Thre:  H:" + String(SoilSliderValue));
    u8g.setPrintPos(85, 63);
    u8g.print("T:" + String(TempSliderValue));
  } while (u8g.nextPage());
}

void setup()
{
  mySerial.begin(115200);           //初始化软串口
  Serial.begin(9600);     //初始化串口
  Serial.print("setup begin\r\n");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  //关闭指示灯
  pinMode(waterPumpPin, OUTPUT);    //灌溉水泵
  digitalWrite(waterPumpPin, HIGH); //关闭水泵
  dht.begin();//初始化DHT11温度传感器

  //以下为ESP8266初始化的代码
  Serial.print("FW Version: ");
  Serial.println(wifi.getVersion().c_str());

  if (wifi.setOprToStation()) {
    Serial.print("to station ok\r\n");
  } else {
    Serial.print("to station err\r\n");
  }

  //ESP8266接入WIFI
  if (wifi.joinAP(SSID, PASSWORD)) {
    Serial.print("Join AP success\r\n");
    Serial.print("IP: ");
    Serial.println(wifi.getLocalIP().c_str());
  } else {
    Serial.print("Join AP failure\r\n");
  }

  Serial.println("");
  Serial.print("DHT11 LIBRARY VERSION: ");
  Serial.println(DHT11LIB_VERSION);

  mySerial.println("AT+UART_CUR=9600,8,1,0,0");
  mySerial.begin(9600);
  Serial.println("setup end\r\n");
}

unsigned long net_time1 = millis(); //数据上传服务器时间
int count=0;

void loop(){

  if (net_time1 > millis())
    net_time1 = millis();

  if (millis() - net_time1 > INTERVAL_SENSOR) //发送数据时间间隔
  {

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    sensorValue = analogRead(sensorPin); //读取土壤湿度数据
    SoilPrecentValue = float(((-(sensorValue - 866)) / 234.0 * 500-700)*3);//计算土壤湿度
    Serial.print("原始数据：");
    Serial.print(sensorValue);
  
    humi_read = h;
    temp_read = t;

    if (SoilPrecentValue <= SoilSliderValue) //水不足并且温度不高于阈值 则补水
      {
        digitalWrite(waterPumpPin, LOW);    //开启继电器
        Serial.print("正在浇水");
        count= 1;
      }
    else//不需要浇水
      {
        digitalWrite(waterPumpPin, HIGH);   //关掉继电器
        Serial.print("暂停浇水");
        count=0;
      }

    if (wifi.createTCP(HOST_NAME, HOST_PORT)) { //建立TCP连接，如果失败，不能发送该数据
      Serial.print("create tcp ok\r\n");
      char buf[100];
      //拼接发送data字段字符串
      String jsonToSend = "{\"Temperature\":";
      dtostrf(temp_read, 1, 2, buf);
      jsonToSend += "\"" + String(buf) + "\"";
      jsonToSend += ",\"Humidity\":";
      dtostrf(SoilPrecentValue, 1, 2, buf);
      jsonToSend += "\"" + String(buf) + "\"";

      jsonToSend += ",\"Count\":";
      dtostrf(count,1,2,buf);
      jsonToSend += "\"" + String(buf) + "\"";
      jsonToSend += "}";

      //拼接POST请求字符串
      String postString = "POST /devices/";
      postString += DEVICE_ID;
      postString += "/datapoints?type=3 HTTP/1.1";
      postString += "\r\n";
      postString += "api-key:";
      postString += APIKey;
      postString += "\r\n";
      postString += "Host:api.heclouds.com\r\n";
      postString += "Connection:close\r\n";
      postString += "Content-Length:";
      postString += jsonToSend.length();
      postString += "\r\n";
      postString += "\r\n";
      postString += jsonToSend;
      postString += "\r\n";
      postString += "\r\n";
      postString += "\r\n";

      const char *postArray = postString.c_str(); //将str转化为char数组

      Serial.println(postArray);
      wifi.send((const uint8_t *)postArray, strlen(postArray)); //send发送命令，参数必须是这两种格式，尤其是(const uint8_t*)
      Serial.println("send success");
      if (wifi.releaseTCP()) { //释放TCP连接
        Serial.print("release tcp ok\r\n");
      } else {
        Serial.print("release tcp err\r\n");
      }
      postArray = NULL; //清空数组，等待下次传输数据
    } else {
      Serial.print("create tcp err\r\n");
    }

    Serial.println("");

    net_time1 = millis();

    oledDisplay();
  }
}
