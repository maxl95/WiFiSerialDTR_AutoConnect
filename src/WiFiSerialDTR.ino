/*
  WiFiTelnetToSerial - Example Transparent UART to Telnet Server for esp8266

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WiFi library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <ESP8266WiFi.h>
#include <CLI.h>
#include <ESP8266WebServer.h>
#include <AutoConnect.h>

#define IAC     255     /* interpret as command: */
#define DONT    254     /* you are not to use option */
#define DO      253     /* please, you use option */
#define WONT    252     /* I won't use option */
#define WILL    251     /* I will use option */
#define SB      250     /* interpret as subnegotiation */
#define GA      249     /* you may reverse the line */
#define EL      248     /* erase the current line */
#define EC      247     /* erase the current character */
#define AYT     246     /* are you there */
#define AO      245     /* abort output--but let prog finish */
#define IP      244     /* interrupt process--permanently */
#define BREAK   243     /* break */
#define DM      242     /* data mark--for connect. cleaning */
#define NOP     241     /* nop */
#define SE      240     /* end sub negotiation */
#define EOR     239             /* end of record (transparent mode) */
#define ABORT   238     /* Abort process */
#define SUSP    237     /* Suspend process */
#define xEOF    236     /* End of file: EOF is already used... */

#define SYNCH   242     /* for telfunc calls */

/* telnet options */
#define TELOPT_BINARY   0   /* 8-bit data path */
#define TELOPT_ECHO 1   /* echo */
#define TELOPT_RCP  2   /* prepare to reconnect */
#define TELOPT_SGA  3   /* suppress go ahead */
#define TELOPT_NAMS 4   /* approximate message size */
#define TELOPT_STATUS   5   /* give status */
#define TELOPT_TM   6   /* timing mark */
#define TELOPT_RCTE 7   /* remote controlled transmission and echo */
#define TELOPT_NAOL     8   /* negotiate about output line width */
#define TELOPT_NAOP     9   /* negotiate about output page size */
#define TELOPT_NAOCRD   10  /* negotiate about CR disposition */
#define TELOPT_NAOHTS   11  /* negotiate about horizontal tabstops */
#define TELOPT_NAOHTD   12  /* negotiate about horizontal tab disposition */
#define TELOPT_NAOFFD   13  /* negotiate about formfeed disposition */
#define TELOPT_NAOVTS   14  /* negotiate about vertical tab stops */
#define TELOPT_NAOVTD   15  /* negotiate about vertical tab disposition */
#define TELOPT_NAOLFD   16  /* negotiate about output LF disposition */
#define TELOPT_XASCII   17  /* extended ascii character set */
#define TELOPT_LOGOUT   18  /* force logout */
#define TELOPT_BM   19  /* byte macro */
#define TELOPT_DET  20  /* data entry terminal */
#define TELOPT_SUPDUP   21  /* supdup protocol */
#define TELOPT_SUPDUPOUTPUT 22  /* supdup output */
#define TELOPT_SNDLOC   23  /* send location */
#define TELOPT_TTYPE    24  /* terminal type */
#define TELOPT_EOR  25  /* end or record */
#define TELOPT_TUID 26  /* TACACS user identification */
#define TELOPT_OUTMRK   27  /* output marking */
#define TELOPT_TTYLOC   28  /* terminal location number */
#define TELOPT_3270REGIME 29    /* 3270 regime */
#define TELOPT_X3PAD    30  /* X.3 PAD */
#define TELOPT_NAWS 31  /* window size */
#define TELOPT_TSPEED   32  /* terminal speed */
#define TELOPT_LFLOW    33  /* remote flow control */
#define TELOPT_LINEMODE 34  /* Linemode option */
#define TELOPT_XDISPLOC 35  /* X Display Location */
#define TELOPT_OLD_ENVIRON 36   /* Old - Environment variables */
#define TELOPT_AUTHENTICATION 37/* Authenticate */
#define TELOPT_ENCRYPT  38  /* Encryption option */
#define TELOPT_NEW_ENVIRON 39   /* New - Environment variables */
#define TELOPT_COM_PORT_OPTION 44
#define TELOPT_EXOPL    255 /* extended-options-list */

#define CPO_SET_BAUDRATE        1
#define CPO_SET_DATASIZE        2
#define CPO_SET_PARITY          3
#define CPO_SET_STOPSIZE        4
#define CPO_SET_CONTROL         5
#define CPO_NOTIFY_LINESTATE    6
#define CPO_NOTIFY_MODEMSTATE   7
#define CPO_FLOWCONTROL_SUSPEND 8
#define CPO_FLOWCONTROL_RESUME  9
#define CPO_SET_LINESTATE_MASK  10
#define CPO_SET_MODEMSTATE_MASK 11
#define CPO_PURGE_DATA          12


const uint8_t pin_dtr = 0;
const uint8_t pin_conf = 2;

const char *spinner = "/-\\|";
WiFiServer serverRFC(23); // RFC2217 compliant port
WiFiServer serverRaw(24);

WiFiClient serverClient;

ESP8266WebServer webserver;
AutoConnect Portal(webserver);

//const uint8_t settings[4096] __attribute__((aligned(4096))) = {0};

void rootPage() {
  char content[] = "Hello, world";
  webserver.send(200, "text/plain", content);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);

  webserver.on("/", rootPage);
  Portal.begin();
  Serial.println("Webserver started:" + WiFi.localIP().toString());

  Serial.println("OK");
  //start UART and the server
  
  serverRFC.begin();
  serverRFC.setNoDelay(true);
  serverRaw.begin();
  serverRaw.setNoDelay(true);
  pinMode(pin_dtr, OUTPUT);
  digitalWrite(pin_dtr, HIGH);
}

void loop() {
  Portal.handleClient();
  static bool clientIsRFC = false;
  static uint8_t IACmode = 0;
  static uint8_t IACcode[100] = {0};

  //check if there are any new clients
  if (serverRFC.hasClient()) {
    if (serverClient && !serverClient.connected()) {
      serverClient.stop();
    }

    clientIsRFC = true;
    IACmode = 0;
    IACcode[0] = 0;
    serverClient = serverRFC.available();
    serverClient.write((uint8_t)IAC);
    serverClient.write((uint8_t)DO);
    serverClient.write((uint8_t)TELOPT_BINARY);
    serverClient.write((uint8_t)IAC);
    serverClient.write((uint8_t)WILL);
    serverClient.write((uint8_t)TELOPT_ECHO);
    serverClient.write((uint8_t)IAC);
    serverClient.write((uint8_t)WILL);
    serverClient.write((uint8_t)TELOPT_COM_PORT_OPTION);
  } else {
    //no free/disconnected spot so reject
    WiFiClient rejectClient = serverRFC.available();
    rejectClient.stop();
  }

  //check if there are any new clients
  if (serverRaw.hasClient()) {
    if (serverClient && !serverClient.connected()) {
      serverClient.stop();
    }

    clientIsRFC = false;
    serverClient = serverRaw.available();
  } else {
    //no free/disconnected spot so reject
    WiFiClient rejectClient = serverRaw.available();
    rejectClient.stop();
  }

  //set the DTR signal
  bool dtr = (serverClient && serverClient.connected());
  digitalWrite(pin_dtr, !dtr);

  //check clients for data
  if (serverClient && serverClient.connected()) {
    if (serverClient.available()) {
      //get data from the telnet client and push it to the UART
      if (clientIsRFC) {
        while (serverClient.available()) {
          int ch = serverClient.read();

          switch (IACmode) {
            case 0:
              if (ch == IAC) { // IAC
                IACmode = 1;
                IACcode[0] = ch;
                IACcode[1] = 0;
              } else {
                Serial.write(ch);
              }

              break;

            case 1:
              if (ch == IAC) { // Escaped 255
                IACmode = 0;
                IACcode[0] = 0;
                Serial.write(IAC);
              } else {
                IACcode[1] = ch;
                IACcode[2] = 0;
                IACmode = 2;
              }

              break;

            case 2:
              IACcode[2] = ch;

              if (IACcode[1] == WILL) {
                if (ch != TELOPT_BINARY) {
                  serverClient.write((uint8_t)IAC);
                  serverClient.write((uint8_t)DONT);
                  serverClient.write((uint8_t)ch);
                }

                IACcode[0] = 0;
                IACmode = 0;
              } else if (IACcode[1] == DO) {
                if (ch == TELOPT_SGA) {
                  serverClient.write((uint8_t)IAC);
                  serverClient.write((uint8_t)WILL);
                  serverClient.write((uint8_t)TELOPT_SGA);
                } else if (ch == TELOPT_ECHO) {
                  // We suggested it, so ignore this as it's a reply
                } else if (ch == TELOPT_COM_PORT_OPTION) {
                  // We suggested it, so ignore this as it's a reply
                } else {
                  serverClient.write((uint8_t)IAC);
                  serverClient.write((uint8_t)WONT);
                  serverClient.write((uint8_t)ch);
                }

                IACcode[0] = 0;
                IACmode = 0;
              } else {
                IACcode[0] = 0;
                IACmode = 0;
              }

              break;
          }
        }
      } else {
        while (serverClient.available()) {
          Serial.write(serverClient.read());
        }
      }
    }
  }

  //check UART for data
  if (Serial.available()) {
    size_t len = Serial.available();
    uint8_t sbuf[len];
    Serial.readBytes(sbuf, len);

    if (serverClient && serverClient.connected()) {
      serverClient.write(sbuf, len);
      delay(1);
    }
  }
}
