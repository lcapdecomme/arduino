/*
 * Test Ok d'un servimoteur sur un esp8266
 * Attention au branchement de la DATA, se référer au tableau ci-dessous
 * Courant : 3,3v
 * Masse   : Ground
 * 
 * This ESP8266 NodeMCU code was developed by newbiely.com
 *
 * This ESP8266 NodeMCU code is made available for public use without any restriction
 *
 * For comprehensive instructions and wiring diagrams, please visit:
 * https://newbiely.com/tutorials/esp8266/esp8266-servo-motor
 */
/*
| Broche (NodeMCU) | GPIO n° (à utiliser) |
| ---------------- | -------------------- |
| D0               | GPIO16               |
| D1               | GPIO5                |
| D2               | GPIO4                |
| D3               | GPIO0                |
| D4               | GPIO2                |
| D5               | GPIO14               |
| D6               | GPIO12               |
| D7               | GPIO13               |
| D8               | GPIO15               |

 */
#include <Servo.h>

Servo servo;  // create servo object to control a servo

int pos = 0;    // variable to store the servo position

void setup() {
  servo.attach(13);  // attaches the servo on pin D7 to the servo object
}

void loop() {
  for (pos = 0; pos <= 180; pos += 1) { // rotate from 0 degrees to 180 degrees
    // in steps of 1 degree
    servo.write(pos);                   // tell servo to go to position in variable 'pos'
    delay(10);                          // waits 10ms for the servo to reach the position
  }

  for (pos = 180; pos >= 0; pos -= 1) { // rotate from 180 degrees to 0 degrees
    servo.write(pos);                   // tell servo to go to position in variable 'pos'
    delay(10);                          // waits 10ms for the servo to reach the position
  }
}
