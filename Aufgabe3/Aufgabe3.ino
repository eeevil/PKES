#include <imu.h>

/*
 Teilaufgabe 1:
Implementiert eine einfache Verfahrfunktion für den Roboter, so dass dieser sich kollisionsfrei auf dem Labortisch bewegt. Ihr könnt dazu neben den 2 obligatorischen Distanzsensoren auch die IMU
(zum Beispiel für spannungsunabhängige Drehbewegungen) verwenden.
Die Implementierung der Motoransteuerung soll nicht mit den analogWrite()-Funktionen der Arduino Bibliothek erfolgen, sondern allein mit der avrlibc,
da die Arduino-Bibliothek an dieser Stelle nur sehr eingeschränkte Konfigurationen zulässt. So kann zum Beispiel die Auflösung des Zählers nicht konfiguriert werden. Wertet also zunächst
das Datenblatt des AVRs aus, um zu ermitteln, welche Timer und welche PWM mit den beiden Motortreiberkanälen verbunden sind.

Teilaufgabe 2:
In dieser Teilaufgabe geht es darum, den Roboter mittels des Gyroskops der IMU um einen wählbaren Winkel zu drehen. Dazu ist der Winkel von der seriellen Schnittstelle einzulesen und dann der Roboter zu drehen,
bis der gewünschte Winkel erreicht ist. Die Werte der IMU sollten gefiltert werden, da der Roboter sich sonst nicht wie erwünscht verhalten wird.
Bei der Drehung darf der Winkelfehler maximal 10% oder 10° betragen.
Empfehlung zum Vorgehen beim Verwenden des Gyroskops:
Beginnt am besten damit, den Offset des Gyroskops für den unbewegten Sensor bestimmen. Zeichnet diesen auf und berechnet den mittleren Offset für euren Sensor.
Beachtet dass im Sensor ein Low-Pass-Filter aktiviert werden sollte, um das Ergebnis zu stabilisieren, für Details siehe die Datenblätter zur verwendeten IMU. Für verschiedene Filter ergeben sich jeweils
unterschiedliche Verteilungsfunktionen, wie in der Abbildung dargestellt.

Je stärker die zulässige Bandbreite (256 – 5 Hz) limitiert wird, desto stabiler wird der Wert. Die Auflösung des AD-Wandlers des MPU 9150 beträgt 16 Bit. Als maximale Drehrate ist im Defaultzustand +-250°/s (für unsere Anwendung im Normalfall ausreichend) vorkonfiguiert.



Die PWM Frequenz sollte unter 100 oder über 15000Hz liegen, sodass die Motoren keinen Pfeifton erzeugen.

 */

// D6 PWM (OC4A), D7 PWM (OC4B)
// D11 PWM (OC1A), D12 PWM (OC1B)


#include <avr/io.h>
#include <util/delay.h>
#include <Wire.h>

/*
 * Clock PC2
 * Data PC3
 * Latch PC4
 */
#define CLOCK_DELAY_MS 1

int x = 0;
int dez = 0;
boolean neg = false;
double xangle = 0;
double yangle = 0;
String inputString = "";
boolean stringComplete = false;

byte EngIn[4] = {0, 0, 0, 0};

int m_targetAddress;

void waitms(double timer) {
  double newtimer;
  newtimer = millis() + newtimer;
  while (millis() < newtimer);
}

// Distance Variablen
uint16_t adcval;
uint32_t valSUM;
int distance;
//Konstanten für Sensorauswahl
const int SEN_GP2D12047 = 0;
const int SEN_2Y0A2121 = 1;


int mode = 3;
int dir;

// IMU Variablne
double IMU_Offset;  //Speichert den IMU_Offset
double IMU_Heading = 0;
double IMU_Heading_Target = 0;//90/36*50;
unsigned long IMU_IntTimer = 0;

double delta;

void setup() {
  Display_Init();

  //PA0 und PA1 als input
  //DDRA = 0x00;

  ADC_Init();

  Serial.begin(57600);
  _delay_ms(500);
  Serial.println("ADC Test");
  Serial.println("Begin Wire");

  Wire.begin();

  InitEngines();

  // Clear the 'sleep' bit to start the sensor.
  Serial.println("Clear the 'sleep' bit to start the sensor.");
  MPU9150_writeSensor(MPU9150_PWR_MGMT_1, 0);

  Serial.println("Setup compass");
  MPU9150_setupCompass();

  Serial.print("Get Offset = ");
  IMU_Offset = IMU_getOffset();
  Serial.println(IMU_Offset);

  Serial.println("Setup COMPLETE");

  updateOCR();

  inputString.reserve(200);

  Serial.println("Gebe einen Winkel ein (Fahren mit 42):");
}

void loop()
{
  if (Serial.available()) {
    int input = Serial.parseInt();
    if (input != 42){
      mode = 3;
      IMU_Heading_Target = input;
    }
    else{
      mode = 0;
    } 
    Serial.print("Drehe von aktuell ");
    Serial.print(IMU_Heading);
    Serial.print(" auf neu: ");
    Serial.println(IMU_Heading_Target);
  }

  IMU_calcHeading();

  switch (mode) {
    case 1:
      //Aufgabe 2: Entfernungsmesser
      distance = getDistance2(SEN_GP2D12047, 0);

      if (distance < 400 || distance > 4000) out(888); //Fehlerausgabe wenn kleiner als
      else out(distance);  //Ausgabe ans Display
      break;

    case 2:
      //Aufgabe 2: Wasserwaage
      //getIMUangle();
      Serial.print("x-Angle: ");
      Serial.print(xangle);
      Serial.print("  y-Angle: ");
      Serial.println(yangle);
      WaageOut();
      break;

    case 3:
      //Aufgabe 3: Winkel anfahren (Meggie)
      out(IMU_Heading * 100);
      /*
      Serial.print("Heading: ");
      Serial.print(IMU_Heading);
      Serial.print(" - Delta: ");
      Serial.println(delta);*/

      delta = IMU_Heading_Target - IMU_Heading;

      if (delta > 10) EngTurn(1, 154);
      else if (delta < -10) EngTurn(0, 154);
      else if (delta > 3) EngTurn(1, 145);
      else if (delta < -3) EngTurn(0, 145);
      else EngStopp();

      break;
    default:
      // Aufgabe 3a: Kollisionsfrei fahren (Meggie)
      distance = getDistance2(SEN_GP2D12047, 0);
      //distance *= 10;

      out(distance);  //Ausgabe ans Display
      //Serial.println(distance);


      // Fahrverhalten
      if (distance < 400 || distance > 4000) EngForward(150);
      else if (distance < 1300) {
        EngForward(-160);
      }
      else if (distance < 1700) {
        EngTurn(0, 160);
      }
      else {
        EngForward(160);
      }

      break;
  }
}
