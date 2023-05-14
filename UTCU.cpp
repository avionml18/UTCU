/*
File:         UTCU.ino
Name:         Team 7
Date:         5/7/2023
Date (done):  5/14/2023
Section:      UTCU
Description:  This program will simulate a UTCU which knows timing and then will send appropriate data to USSIM
*/

/* COMMENTS

function IDs were found in Annex 10 page 46

Analog/Digital output: PWM -> A1, A2, 5, 9, 10, 11, 13 (connected to red LED)

Arduino envirnoment makes function prototypes automatically from the defintions

DELAY is in milisecond
  delay(100) => 100 milisecond = 0.1 seconds
  delay(0.1) => 100 microseconds = 0.1 milisecond = 0.001 seconds

The fastest Miki's ARduino can go is 1kHz so 1 milisecond
  New scale factor needs to be > 1 milisecond
    Ex: 100 miliseconds

ASSUMPTION: I'm assuming somewhere in the code we are holding some bit as
            a 0 for a certain amoutn of time for unmodulated carriers/data

When LED is on ->  where are scanning (State To,Wait,Fro)
When LED is off everything else

every 50 scaled microseconds
Calculate how long to do to and to do fro for each station which means changing the delay parameters depending on the station

We took out delays from sendDataToUSSIM function becuase the "only 'time sensitive' thing is data and txen." - Miki
*/

// Includes files
#include <stdlib.h>
#include <string>
#include <ostream>
#include <iostream>
using namespace std;

// Global Constants
const double bitReadTime = 1;    // total time to read each sent bit (in miliseconds) [64ms orginally]
// Miki's Team read0s bits every 0.1 miroseconds
const int maxAnt = 6;
const int maxID = 7;
const int maxBarker = 5;
const int maxData = 32;  // aux data. Hold 32 bits total
const int maxAux = 72;   // basic data. holds 72 bits total
const int maxRaw = 60;
const int maxRawFirstGo = 20;


// AZ Time: 1550 ms
// BAZ TIme: 1550 ms
// EL Time: 2000 ms (Need to ask Miki)
const int delayToFro = 5000; // in ms
const int delayWait = 10; // in ms
const int delayPause = 1000; // The pause between the Barker and the Function ID

// Delete these for Arduino
const string OUTPUT = " set to output";
const bool HIGH = 1;
const bool LOW = 0;


// Global Variables/Arrays
bool function_ID[maxID] = { 0 };  // function ID is 7 bits long
string function_Type_name = "AZ";  // can also be EL, and BAZ

// Function IDs from Annex10/FAA-std-022c
bool AZ_function[] = { 0, 0, 1, 1, 0, 0, 1 };
bool HRAZ_function[] = { 0, 0, 1, 0, 1, 0, 0 };
bool BAZ_function[] = { 1, 0, 0, 1, 0, 0, 1 };
bool EL_function[] = { 1, 1, 0, 0, 0, 0, 1 };

bool barker_code[maxBarker] = { 1, 1, 1, 0, 1 };

bool basic_data[maxData] = { 0 };  // basic data. holds 32 bits total
bool aux_data[maxAux] = { 0 };     // aux data. Hold 72 bits total
bool raw_data[maxRaw] = { 0 };

short antennaType[maxAnt] = { 0 };  // global antenna typestring stateType;              

double totalTime = 0; // Total time for one sequnce which should be about 615
string stateType = "";  // global state type declaration

// Global boolean flag for tracking if the basic or aux data has been sent to USSIM
// --Will intiially start as 1 to prevent sending data dueing Reset or noData states
bool sentData = 1;
// Global boolean flag - to send basic or aux data when data bit is 1
// Extra functionality, to alternate per loop/cycle
bool sendBasic = 1; 

// declaration of all required UTCU states below:
string allStates[maxAnt] =        { "NoData", "TakeData", "To", "Wait", "Fro" };  //  All antenna states
short stateReset[maxAnt] =        { 0, 0, 0, 1, 1, 1 };                            //  Reset state; 
short stateNoData[maxAnt] =       { 0, 0, 0, 0, 0, 0 };                            //  NoData state; ->Hold for unmodulated data long?
short stateTakeData[maxAnt] =     { 1, 1, 0, 0, 0, 0 };                            //  TakeData state;
short stateTo[maxAnt] =           { 1, 0, 1, 0, 0, 1 };                            //  To state;
short stateWait[maxAnt] =         { 0, 0, 1, 1, 1, 1 };                            //  Wait state; 
short stateFro[maxAnt] =          { 1, 0, 0, 0, 0, 1 };                            //  Fro state; 

// Fucntion Prototypes 
void generateData();
void format_func();
void sendData();
void sendDataToUSSIM();
void sendPreamble();
void pinMode(int, string);
void delay(double);
void digitalWrite(int, bool);

// put your setup code here, to run once:
void setup() {
    cout << "setup()" << endl << endl;
    // Set Up Pins
    pinMode(5, OUTPUT); // Trasnmit bit - MSB
    pinMode(7, OUTPUT); // Data bit
    pinMode(9, OUTPUT); // To/Fro bit
    pinMode(10, OUTPUT); // Antenna 1 bit 
    pinMode(11, OUTPUT); // Antenna 2 bit 
    pinMode(13, OUTPUT); // Antenna 3 bit (RED LED) - LSB

    // Begin Timer and set antennaType to reset state
    cout << "Serial.begin(9600)" << endl;
    //Serial.begin(9600);
    for (int i = 0; i < maxAnt; i++) {
        antennaType[i] = stateReset[i];  // antennaType is set to reset at beginning to accept states later on
    }
    return;
}

// put your main code here, to run repeatedly:
void main() {
    setup();
    generateData();
    format_func();
    sendData();

    // Alternator for sending either basic or aux data
    sendBasic ? sendBasic = 0 : sendBasic = 1;

    cout << endl << "Reseting for new cycle" << endl << endl;
    // Reset the antenna to a reset state or let USSIM know we are about to generate new data and loop through the process
    for (int i = 0; i < maxAnt; i++)
        antennaType[i] = stateReset[i];
    sendDataToUSSIM();

    cout << endl << "Total Time (ns): " << round(totalTime * 1000) << " nanoseconds"
        << endl << "Total Time (ms): " << totalTime << " miliseconds" << endl
        << endl << "Total Time (s): " << round(totalTime / 1000) << " seconds" << endl;

}

// Function Defintions

/**
 * Function IDs will be set based on the station
 * and unmodulated data will be filled in -> raw_data array
 */
void generateData() {
    cout << endl << "generateData()" << endl << endl;
    for (int i = 0; i < maxID; i++) {
        // AZ Function
        if (function_Type_name == "AZ") {
            function_ID[i] = AZ_function[i];
        }
        
        // HRAZ Function
        if (function_Type_name == "HRAZ") {
            function_ID[i] = HRAZ_function[i];
        }

        // BAZ Function
        else if (function_Type_name == "BAZ") {
            function_ID[i] = BAZ_function[i];
        }

        // EL Function
        else if (function_Type_name == "EL") {
            function_ID[i] = EL_function[i];
        }
    }
    // If it's the first go, then put the 20 bits of unmodualted data, otherwise fill all 60 bits with unmodualted data
    if (antennaType[1]) {
        for (int i = 0; i < maxRawFirstGo; i++) {
            // 20 bits of raw data with each bit ranging from 0 to 1
            rand() % 2 ? raw_data[i] = true : raw_data[i] = false;
        }
    }

    else {
        for (int i = 0; i < maxRaw; i++) {
            // 60 bits of raw data with each bit ranging from 0 to 1
            rand() % 2 ? raw_data[i] = true : raw_data[i] = false;
        }

    }
}

/**
 * Format the genereated data to be sent.
 * aux_data will be filled in or basic_data will be filled in
 */
void format_func() {
    cout << "format_func()" << endl << endl;

    // if sendBasic is 1 then the data gets placed into the basic data array
    if (sendBasic) {

        for (int i = 0; i < maxRawFirstGo; i++) {
            basic_data[i] = raw_data[i];
        }

        for (int i = maxRawFirstGo; i < maxData; i++) {
            rand() % 2 ? basic_data[i] = true : basic_data[i] = false;
        }
    }

    // if sendBasic is 0 then the data gets placed into the aux data array
    else {

        for (int i = 0; i < maxRaw; i++) {
            aux_data[i] = raw_data[i];
        }

        for (int i = maxRaw; i < maxAux; i++) {
            rand() % 2 ? aux_data[i] = true : aux_data[i] = false;
        }
    }
}

/**
 * Function made to check for elasped time to determine if antennaType is set to reset + if antennaType is set to every listed state *
 */
void sendData() {
    cout << "sendData()" << endl;
    for (int i = 0; i < 5; i++) {  // for loop that will run through each listed state
        stateType = allStates[i];

        // Update the antennaType to be in a certain state
        //  By the end of this for loop, send the antennaType to USSIM
        for (int j = 0; j < maxAnt; j++) {

            if (stateType == allStates[0]) {
                antennaType[j] = stateNoData[j];  //antennaType set to sending no data
            }

            else if (stateType == allStates[1]) {
                antennaType[j] = stateTakeData[j];  // antennaType set to take in data
            }

            else if (stateType == allStates[2]) {
                antennaType[j] = stateTo[j];  // antennaType set to "To" scan
            }

            else if (stateType == allStates[3]) {
                antennaType[j] = stateWait[j];  // antennaType set to pause scanning
            }

            else if (stateType == allStates[4]) {
                antennaType[j] = stateFro[j];  // antennaType set to "Fro" scan
            }
        }

        cout << endl << "Sending " << stateType << endl << endl;

        
        // State = TakeData
        //  Format the data after the data pin has been set or not
        //  Sent Data -> Set the bool flag sentData to 0 to intiate reading data bits in serial 
        if (stateType == allStates[1])
            sentData = 0;

        // SETTING PINS -> COMMUNICATING WITH USSIM //
        sendDataToUSSIM();

        // Delay - Give USSIM enough time to scan

        // To or fro delay
        if (stateType == allStates[2] || stateType == allStates[4])
            delay(delayToFro);

        // Wait
        else if (stateType == allStates[3])
            delay(delayWait);
    }
}

/**
 * This function will take the antennaType and read it out to USSIM thorugh the use of approporiate pins
 * Sending data in parallel and in serial
 * Could be written in a for loop -> with switch statements and then one delay
 */
void sendDataToUSSIM() {

    delay(bitReadTime); // send bits every 64 miliseconds

    // Transmit bit - Most Sig Bit
    if (antennaType[0])
        digitalWrite(5, HIGH);
    else
        digitalWrite(5, LOW);

    // Data bit
    // If data bit is 1 -> send basic data
    if (antennaType[1]) {

        // Sending the data bit 1 to USSIM
        digitalWrite(7, HIGH);

        if (!sentData) {
            cout << "Sending Preamble" << endl << endl;
            // Sending Preamble in serial using pin 7
            sendPreamble();
            if (sendBasic) {
                cout << endl << "Sending Basic" << endl << endl;
            
                // Send Basic Data
                for (int i = 0; i < maxData; i++) {
                    delay(bitReadTime); // send bits every 64 miliseconds

                    if (basic_data[i])
                        digitalWrite(7, HIGH);
                    else
                        digitalWrite(7, LOW);
                }
            }
            else {
                cout << endl << "Sending Aux" << endl << endl;

                // Send Aux Data
                for (int i = 0; i < maxAux; i++) {
                    delay(bitReadTime); // send bits every 64 miliseconds

                    if (aux_data[i])
                        digitalWrite(7, HIGH);
                    else
                        digitalWrite(7, LOW);
                }
            }
            // Set the sentData global variable back to 1 to signify you've sent the data
            sentData = 1;
        }
    }

    else
        digitalWrite(7, LOW);

    // To/Fro bit
    if (antennaType[2])
        digitalWrite(9, HIGH);
    else
        digitalWrite(9, LOW);

    // Anteanna 0
    if (antennaType[3])
        digitalWrite(10, HIGH);
    else
        digitalWrite(10, LOW);

    // Anteanna 1
    if (antennaType[4])
        digitalWrite(11, HIGH);
    else
        digitalWrite(11, LOW);

    // Anteanna 2 - Least Sig Bit
    if (antennaType[5])
        digitalWrite(13, HIGH);
    else
        digitalWrite(13, LOW);
}

/**
 * This function will send the Preamble -- Barker Code and Function ID -- to USSIM
 */
void sendPreamble() {
    cout << "Sending Barker Code" << endl << endl;
    // Send Barker code
    for (int i = 0; i < maxBarker; i++) {
        delay(bitReadTime);  // send bits every 64 miliseconds
        if (barker_code[i])
            digitalWrite(7, HIGH);
        else
            digitalWrite(7, LOW);
    }

    // Brief pause before sending barker code as per Dr. Laberge's diagram
    delay(delayPause);

    cout << endl << "Sending Function ID Code" << endl << endl;
    // Send Function ID
    for (int i = 0; i < maxID; i++) {
        delay(bitReadTime);  // send bits every 64 miliseconds
        if (function_ID[i])
            digitalWrite(7, HIGH);
        else
            digitalWrite(7, LOW);
    }
}

void pinMode(int pinNum, string strOut) {
    cout << "Pin " << pinNum << strOut << endl;
}

void delay(double num_ms) {
    cout << "Delay: " << num_ms << "ms" << endl;
    totalTime += num_ms;
}

void digitalWrite(int pin, bool H_L) {
    cout << pin << " : " << H_L << endl;
}