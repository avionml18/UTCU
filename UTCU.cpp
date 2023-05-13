/*
File:         UTCU.ino
Name:         Team 7
Date:         5/7/2023
Date (done):  5/7/2023
Section:      UTCU
Description:  This program will simulate a UTCU which knows timing and then will send appropriate data to USSIM
*/

/* COMMENTS

function IDs were found in Annex 10 page 46

Aux data is 72 bits total
baisc data is 32 total
first twelve is ignore

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

Telling Miki team to turn off the transmitter or that we do that - what was this

A cycle through the code takes about 34 - 36 seconds
Takes 17 seconds to cycle until the LED is OFF
Takes 18 seconds to cycle until the LED is ON

every 50 scaled microseconds
Calculate how long to do to and to do fro
*/

// Includes files
#include <stdlib.h>
#include <cstdint>
#include <string>
#include <ostream>
#include <iostream>
using namespace std;

// Global Constants
const double sequenceTime = 615;    // total time given from shift to one function to another (in miliseconds) [615ms orginally]
const double bitReadTime = 0.15;    // total time to read each sent bit (in miliseconds) [64ms orginally]
// Miki's Team reads bits every 0.1 miroseconds
const int totalBits_ID = 6;
const int8_t maxBarker = 5;
const int maxData = 32;  // aux data. Hold 32 bits total
const int maxAux = 72;   // basic data. holds 72 bits total
const int maxRaw = 60;
const string OUTPUT = " set to output";
const bool HIGH = 1;
const bool LOW = 0;


// Global Variables/Arrays
bool function_ID_array[totalBits_ID] = { 0 };
char function_Type_name[] = "AZ";  // can also be EL, and BAZ
bool barker_code[maxBarker] = { 1, 1, 1, 0, 1 };
bool basic_data[maxData] = { 0 };  // basic data. holds 32 bits total
bool aux_data[maxAux] = { 0 };     // aux data. Hold 72 bits total
int8_t raw_data[maxRaw] = { 0 };
short antennaType[totalBits_ID] = { 0 };  // global antenna type
int8_t function_ID = 0b0000000;
string stateType;              // global state type declaration
int totalTime = 0;

// Global boolean variable for tracking if the basic or aux data has been sent to USSIM
// --Will intiially start as 1 to prevent sending data dueing Reset or noData states
bool sentData = 1;

// declaration of all required UTCU states below:
string allStates[totalBits_ID] = { "NoData", "TakeData", "To", "Wait", "Fro" };  //  All antenna states
short stateReset[totalBits_ID] =        { 0, 0, 0, 1, 1, 1 };                            //  Reset state; 
short stateNoData[totalBits_ID] =       { 0, 0, 0, 0, 0, 0 };                            //  NoData state; -> 615ms -> Hold for unmodulated data 
short stateTakeData[totalBits_ID] =     { 1, 1, 0, 0, 0, 0 };                            //  TakeData state; 
short stateTo[totalBits_ID] =           { 1, 0, 1, 0, 0, 1 };                            //  To state;
short stateWait[totalBits_ID] =         { 0, 0, 1, 1, 1, 1 };                            //  Wait state; 
short stateFro[totalBits_ID] =          { 1, 0, 0, 0, 0, 1 };                            //  Fro state; 

// Fucntion Prototypes 
void generateData();
void format_func();
void sendData();
void sendDataToUSSIM();
void sendPreamble();
void binToArray_ID();
void pinMode(int, string);
void delay(double);
void digitalWrite(int, bool);
void test();

// put your setup code here, to run once:
void setup() {

    // Set Up Pins
    pinMode(5, OUTPUT); // Trasnmit bit - MSB
    pinMode(7, OUTPUT); // Data bit
    pinMode(9, OUTPUT); // To/Fro bit
    pinMode(10, OUTPUT); // Antenna 1 bit 
    pinMode(11, OUTPUT); // Antenna 2 bit 
    pinMode(13, OUTPUT); // Antenna 3 bit (RED LED) - LSB

    // Begin Timer and set antennaType to reset state
    cout << "Serial.begin(9600)" << endl;
    for (int i = 0; i < totalBits_ID; i++) {
        antennaType[i] = stateReset[i];  // antennaType is set to reset at beginning to accept states later on
    }
}

// put your main code here, to run repeatedly:
void main() {
    setup();
    generateData();
    format_func();
    sendData();
    // Reset the antenna to a reset state or let USSIM know we are about to generate new data and loop through the process
    for (int i = 0; i < totalBits_ID; i++)
        antennaType[i] = stateReset[i];  // antennaType will be converted to state reset once time exceeds 615 ms
    sendDataToUSSIM();

    cout << endl << totalTime << " miliseconds -> " << totalTime/1000 << " seconds" << endl;
}

// Function Defintions

/**
 * Function IDs will be set based on the station
 * and unmodulated data will be filled in -> raw_data array
 */
void generateData() {
    if (function_Type_name == "AZ") {
        int8_t AZ_function = 0b0011001;  //all function ID's are 7 bits
        function_ID = AZ_function;       //stored in the first slot
    }

    else if (function_Type_name == "BAZ") {
        int8_t BAZ_function = 0b1001001;  //all function ID's are 7 bits
        function_ID = BAZ_function;       //stored in the first slot
    }

    else if (function_Type_name == "EL") {
        int8_t EL_function = 0b1100001;  //all function ID's are 7 bits
        function_ID = EL_function;       //stored in the first slot
    }

    // After updating funciton ID, convert function ID to an array form
    binToArray_ID();

    // If it's the first go, then put the 20 bits of unmodualted data, otherwise fill all 60 bits with unmodualted data
    if (antennaType[1]) {
        for (int i = 0; i < 20; i++) {
            // 20 bits of raw data with each bit ranging from 0 to 1
            raw_data[i] = rand() % 2;
        }
    }

    else {
        for (int i = 0; i < maxRaw; i++) {
            // 60 bits of raw data with each bit ranging from 0 to 1
            raw_data[i] = rand() % 2;
        }

    }
}

/**
 * Format the genereated data to be sent.
 * aux_data will be filled in or basic_data will be filled in
 */
void format_func() {
    // Set the databit to the "data bit" which is atenna type's 2 bool element
    bool dataBit = antennaType[1];

    // if data_word equals 1 then the data gets placed into the basic data array
    if (dataBit) {

        for (int i = 0; i < 20; i++) {
            raw_data[i] = basic_data[i];
        }

        for (int i = 20; i < maxData; i++) {
            basic_data[i] = rand() % 2;
        }
    }

    // if data_word equals 0 then the data gets placed into the aux data array
    else {

        for (int i = 0; i < maxRaw; i++) {
            raw_data[i] = aux_data[i];
        }

        for (int i = maxRaw; i < maxAux; i++) {
            aux_data[i] = rand() % 2;
        }
    }
}

/**
 * Function made to check for elasped time to determine if antennaType is set to reset + if antennaType is set to every listed state *
 */
void sendData() {
    for (int i = 0; i < 5; i++) {  // for loop that will run through each listed state
        stateType = allStates[i];

        // Update the antennaType to be in a certain state
        //  By the end of this for loop, send the antennaType to USSIM
        for (int j = 0; j < totalBits_ID; j++) {

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

        // Sent Data -> Set the bool flag sentData to 0 to intiate reading data bits in serial 
        if (stateType == allStates[1])
            sentData = 0;

        // SETTING PINS -> COMMUNICATING WITH USSIM //
        sendDataToUSSIM();

        // Delay

        // To or fro delay
        if (stateType == allStates[2] || stateType == allStates[4])
            delay(5000);

        // Wait
        else if (stateType == allStates[3])
            delay(10);
    }
    // delay(sequenceTime); // 615ms delay
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

    delay(bitReadTime);

    // Data bit
    // If data bit is 1 -> send basic data
    if (antennaType[1]) {

        // Sending the data bit 1 to USSIM
        digitalWrite(7, HIGH);

        if (!sentData) {
            // Sending Preamble in serial using pin 7
            sendPreamble();

            // Send Basic Data
            for (int i = 0; i < maxData; i++) {
                delay(bitReadTime);

                if (basic_data[i])
                    digitalWrite(7, HIGH);
                else
                    digitalWrite(7, LOW);
            }
            // Set the sentData global variable back to 1 to signify you've sent the data
            sentData = 1;
        }
    }

    // If data bit is 0 -> send aux data
    else {

        // Sending the data bit 0 to USSIM
        digitalWrite(7, LOW);

        if (!sentData) {
            // Sending Preamble in serial using pin 7
            sendPreamble();

            // Send Aux Data
            for (int i = 0; i < maxAux; i++) {
                delay(bitReadTime);

                if (aux_data[i])
                    digitalWrite(7, HIGH);
                else
                    digitalWrite(7, LOW);
            }
            // Set the sentData global variable back to 1 to signify you've sent the data
            sentData = 1;
        }
    }
    delay(bitReadTime);

    // To/Fro bit
    if (antennaType[2])
        digitalWrite(9, HIGH);
    else
        digitalWrite(9, LOW);

    delay(bitReadTime);

    // Anteanna 0
    if (antennaType[3])
        digitalWrite(10, HIGH);
    else
        digitalWrite(10, LOW);

    delay(bitReadTime);

    // Anteanna 1
    if (antennaType[4])
        digitalWrite(11, HIGH);
    else
        digitalWrite(11, LOW);

    delay(bitReadTime);

    // Anteanna 2 - Least Sig Bit
    if (antennaType[5])
        digitalWrite(13, HIGH);
    else
        digitalWrite(13, LOW);
}

/**
 * This function will take function ID in the form of binary digits and make them into an array to be iterated through
 * in sendData()
 */
void binToArray_ID() {
    int8_t temp = function_ID;
    bool reminder;
    for (int i = 0; i < totalBits_ID; i++) {

        reminder = temp % 2;
        temp = temp >> 2;
        function_ID_array[(totalBits_ID - 1) - i] = reminder;
    }
}

/**
 * This function will send the Preamble -- Barker Code and Function ID -- to USSIM
 */
void sendPreamble() {

    // Send Barker code
    for (int i = 0; i < maxBarker; i++) {
        delay(bitReadTime);  // send bits every 64 miliseconds
        if (barker_code[i])
            digitalWrite(7, HIGH);
        else
            digitalWrite(7, LOW);
    }

    // Brief pause before sending 
    delay(1000);
    // Send Function ID
    for (int i = 0; i < totalBits_ID; i++) {
        delay(bitReadTime);  // send bits every 64 miliseconds
        if (function_ID_array[i])
            digitalWrite(7, HIGH);
        else
            digitalWrite(7, LOW);
    }
}

/**
 * This is what the supergroup ran for testing through things
 */
void test() {
    // Transmit 1
    digitalWrite(5, HIGH);
    delay(500);
    // Serial.print(*basic_data);

    // Data pin 7
    digitalWrite(7, HIGH);
    delay(1000);
    digitalWrite(7, LOW);
    delay(1000);

    // Something Before scan!
    digitalWrite(9, LOW);  // to fro
    // DATA amplitudes, just set to something that isn't scan so we know to restart scan when amplitudes are switched to scan
    // we can talk about if you want us to use txen instead
    digitalWrite(10, HIGH);
    digitalWrite(11, HIGH);
    digitalWrite(13, HIGH);
    delay(100);
    // To
    digitalWrite(9, HIGH);  // set to fro to high to start to scan
    // amplitudes for scan
    digitalWrite(10, LOW);
    digitalWrite(11, LOW);
    digitalWrite(13, HIGH);
    delay(5000);
    // Pause
    digitalWrite(9, HIGH);  // to/fro
    // set to amplitude off, something that isn't scan
    // We can talk about if you want us to use txen instaed
    digitalWrite(10, HIGH);
    digitalWrite(11, HIGH);
    digitalWrite(13, HIGH);
    delay(5000);
    // Fro
    digitalWrite(9, LOW);  // set fro to low to let us know to start fro scan
    // initate scan
    digitalWrite(10, LOW);
    digitalWrite(11, LOW);
    digitalWrite(13, HIGH);
    delay(10000);
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