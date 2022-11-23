#include <SimpleCLI.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define pinESP 23
#define pinMes 34

SimpleCLI cli;
Command cmdInit;
Command cmdRun;
Command cmdMode;
Command cmdStop;
Command cmdPwm;
Command cmdList;
Command cmdRef;
Command cmdKp;
Command cmdKi;
Command cmdPwmMax;
Command cmdPlot;

//System mode of operation 0 - opened loop, 1 - closed loop
int mode = 0;

//State of the system 0 - run, 1 - stop
int state = 0;

//Power of the ESC
int pwmESC = 0;

int ref = 1000;

float Kp = 0.0;
float Ki = 0.0;

int pwmMax = 1700;

bool plot = false;


// the setup function runs once when you press reset or power the board
void setup() {
  //Pwm pin
  ledcSetup(0, 50, 14);
  ledcAttachPin(pinESP, 0);

  //serial communication
  Serial.begin(115200);

  cli.setOnError(errorCallback);
  cmdInit = cli.addCmd("init", cmdInitCallback);
  cmdRun = cli.addCmd("run", cmdRunCallback);
  cmdStop = cli.addCmd("stop", cmdStopCallback);
  cmdList = cli.addCmd("list", cmdListCallback);
  cmdMode = cli.addSingleArgCmd("mode", cmdModeCallback);
  cmdPwm = cli.addSingleArgCmd("pwm", cmdPwmCallback);
  cmdRef = cli.addSingleArgCmd("ref", cmdRefCallback);
  cmdKp = cli.addSingleArgCmd("kp", cmdKpCallback);
  cmdKi = cli.addSingleArgCmd("ki", cmdKiCallback);
  cmdPwmMax = cli.addSingleArgCmd("pwm_max", cmdPwmMaxCallback);
  cmdPlot = cli.addSingleArgCmd("plot", cmdPlotCallback);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  delay(2000);
}

long timeTarget = 0;
float freq = 0.0;

void loop() {
  // put your main code here, to run repeatedly:
  long t = millis();
  if (t - timeTarget > 100) {
    timeTarget = t;
    controlTask();
  }
  if (Serial.available()) {
    // Read out string from the serial monitor
    String input = Serial.readStringUntil('\n');
    cli.parse(input);
  }
}


int mesurement = 0;
float error = 0.0;

void oled() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("STATE:  ");
  (state == 0 ? display.println("STOP") : display.println("RUN"));
  // Display static text
  display.print("PWM:    ");
  display.println(pwmESC);
  display.print("MES:    ");
  display.println(mesurement);
  display.print("MOD:    ");
  (!mode ? display.println("OPEN LOOP") : display.println("CLOSE LOOP"));
  display.print("REF:    ");
  display.println(ref);
  display.print("Kp:     ");
  display.println(Kp);
  display.print("Ki:     ");
  display.println(Ki);
  display.print("ERROR:  ");
  display.println(error);
  display.display();
}

float alpha = 0.1;
float i_cul = 0.0;

void controlTask() {
  oled();
  //Averaging measured signal
  int m = analogRead(pinMes);
  mesurement = (float)mesurement * (1.0 - alpha) + m * alpha;


  if (mode == 0) {
    //Open loop mode
    switch (state) {
      //Stop pwm
      case 0:
        {
          ledcWrite(0, 0);
        }
        break;
      //Start pwm
      case 1:
        {
          ledcWrite(0, pwmESC);
          if (plot) {
            Serial.print(pwmESC);
            Serial.print(" ");
            Serial.println(mesurement);
          }
        }
        break;
    }
  } else {
    //Closed loop mode
    switch (state) {
      //Stop pwm
      case 0:
        {
          ledcWrite(0, 0);
        }
        break;
      //Start closed loop
      case 1:
        {
          error = (float)ref - (float)mesurement;
          float kp_ = error * Kp;
          float ki_ = i_cul + error * Ki * 0.001;
          i_cul = ki_;
          float out = kp_ + ki_ + 1000.0;
          if (out > pwmMax) out = pwmMax;
          if (out < 0) out = 0;
          pwmESC = (int)out;
          ledcWrite(0, pwmESC);
          if (plot) {
            Serial.print(pwmESC);
            Serial.print(" ");
            Serial.print(ref);
            Serial.print(" ");
            Serial.println(mesurement);
          }
        }
        break;
    }
  }
}

void errorCallback(cmd_error* e) {
  Serial.println("error: command not found");
}

void cmdInitCallback(cmd* c) {
  Serial.println("Initialised");
}

void cmdListCallback(cmd* c) {

  Serial.print("Mode    |");
  Serial.println(mode);
  Serial.print("State   |");
  Serial.println(state);
  Serial.print("Pwm     |");
  Serial.println(pwmESC);
  Serial.print("Mes     |");
  Serial.println(mesurement);
  Serial.print("Ref     |");
  Serial.println(ref);
  Serial.print("Kp     |");
  Serial.println(Kp);
  Serial.print("Ki     |");
  Serial.println(Ki);
  Serial.print("Error  |");
  Serial.println(error);
  Serial.print("Pwm max|");
  Serial.println(pwmMax);
}

void cmdRunCallback(cmd* c) {
  state = 1;
  Serial.println("System started");
}

void cmdStopCallback(cmd* c) {
  state = 0;
  Serial.println("System stopped");
}

void cmdModeCallback(cmd* c) {
  Command cmd(c);
  int argNum = cmd.countArgs();
  if (argNum > 0) {
    // Get argument
    int value = cmd.getArgument(0).getValue().toInt();
    if (value > 0) value = 1;
    mode = value;
    Serial.print("Switched to mode: ");
    Serial.println(value);
  } else {
    Serial.println("mode command without parameter");
  }
}

void cmdPlotCallback(cmd* c) {
  Command cmd(c);
  int argNum = cmd.countArgs();
  if (argNum > 0) {
    // Get argument
    int value = cmd.getArgument(0).getValue().toInt();
    if (value > 0) value = 1;
    plot = (bool)value;
    Serial.print("Switched plot to: ");
    Serial.println(value);
  } else {
    Serial.println("plot command without parameter");
  }
}

void cmdPwmCallback(cmd* c) {
  Command cmd(c);
  int argNum = cmd.countArgs();
  if (argNum > 0) {
    // Get argument
    int value = cmd.getArgument(0).getValue().toInt();
    if (value > pwmMax) {
      Serial.print("Pwm signal is limited to ");
      Serial.println(pwmMax);
      return;
    }
    if (value < 0) {
      Serial.println("Pwm signal is limited to 0");
      return;
    }
    pwmESC = value;
    Serial.print("Pwm set to: ");
    Serial.println(value);
  } else {
    Serial.println("pwm command without parameter");
  }
}

void cmdPwmMaxCallback(cmd* c) {
  Command cmd(c);
  int argNum = cmd.countArgs();
  if (argNum > 0) {
    // Get argument
    int value = cmd.getArgument(0).getValue().toInt();
    if (value < 100) {
      Serial.println("Pwm max signal is limited below 100");
      return;
    }
    pwmMax = value;
    Serial.print("Pwm set to: ");
    Serial.println(value);
  } else {
    Serial.println("pwm_max command without parameter");
  }
}

void cmdRefCallback(cmd* c) {
  Command cmd(c);
  int argNum = cmd.countArgs();
  if (argNum > 0) {
    // Get argument
    int value = cmd.getArgument(0).getValue().toInt();
    if (value > 4000) {
      Serial.println("Ref signal is limited to 4000");
      return;
    }
    if (value < 0) {
      Serial.println("Pwm signal is limited to 0");
      return;
    }
    ref = value;
    Serial.print("Ref set to: ");
    Serial.println(value);
  } else {
    Serial.println("ref command without parameter");
  }
}

void cmdKpCallback(cmd* c) {
  Command cmd(c);
  int argNum = cmd.countArgs();
  if (argNum > 0) {
    // Get argument
    float value = cmd.getArgument(0).getValue().toFloat();
    if (value > 100.0) {
      Serial.println("Kp is limited to 100.0");
      return;
    }
    if (value < 0.0) {
      Serial.println("Kp is limited to 0.0");
      return;
    }
    Kp = value;
    Serial.print("Kp is set to: ");
    Serial.println(value);
  } else {
    Serial.println("kp command without parameter");
  }
}

void cmdKiCallback(cmd* c) {
  Command cmd(c);
  int argNum = cmd.countArgs();
  if (argNum > 0) {
    // Get argument
    float value = cmd.getArgument(0).getValue().toFloat();
    if (value > 100.0) {
      Serial.println("Ki is limited to 100.0");
      return;
    }
    if (value < 0.0) {
      Serial.println("Ki is limited to 0.0");
      return;
    }
    Ki = value;
    Serial.print("Ki is set to: ");
    Serial.println(value);
  } else {
    Serial.println("ki command without parameter");
  }
}