/*
 * A Processing sketch example of how to interact with the simulator
 */

import processing.serial.*;

Serial UART;  

void up(int mole) {
  UART.write("up " + mole + "\n");
}

void down(int mole) {
  UART.write("down " + mole + "\n");
}

void led(int mole, int red, int green, int blue) {
  UART.write("color " + mole + " " + red + " " + green + " " + blue + "\n");
}

void setup() {
  size(480, 120);

  println("Avaiable serial ports:");
  printArray(Serial.list());
  println("Picking", Serial.list()[0], "\n");  
  
  UART = new Serial(this, Serial.list()[0], 19200);
  
  for (int i = 1; i <= 4; i++) {
    down(i);
    led(i, 0, 0, 0);
  }
}

void draw() {
  //if (mousePressed) {
  //  led(3, 0, 255, 0);
  //  fill(0);
  //} else {
  //  led(3, 255, 0, 0);
  //  fill(255);
  //}
  //ellipse(mouseX, mouseY, 80, 80);

  if (mousePressed) {
    up(1);
  }
  else {
    down(1);
  } 

  while (UART.available() > 0) {
    char hit = UART.readChar();
    println(hit);
  }
}
