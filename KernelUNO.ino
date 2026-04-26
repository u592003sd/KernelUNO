/*
  BSD 3-Clause License

  Copyright (c) 2026, Arc1011

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Project: KernelUNO

  A lightweight RAM-based shell for Arduino UNO with filesystem simulation,
  hardware control, and interactive shell.

  created by Arc1011
  Link to Original Project: https://github.com/Arc1011/KernelUNO

  modified 26 April 2026
  By u592003sd

  This project is in the public domain.
  Link to Repository Fork: https://github.com/u592003sd/KernelUNO
*/

/******************************************************************************/
/*                          Include Headers                                   */
/******************************************************************************/

/* Standard Includes */
#include <Arduino.h>
#include <string.h>

/******************************************************************************/
/*                          Private Macros                                    */
/******************************************************************************/
/* Maximum number of files permitted in FS */
#define MAX_FILES                   (10)

/* Maximum length of the name of an FS Object */
#define NAME_LEN                    (12)

/* Maximum Size of Content that can be attached to a RAM File Object */
#define CONTENT_LEN                 (32)

/* Maximum Permitted length of Path by the FS */
#define PATH_LEN                    (16)

/* Maximum number of Dmesg Entries permitted in the FS */
#define DMESG_LINES                 (6)

/* Maximum size of the a Dmesg log on the serial console */
#define DMESG_LEN                   (40)

/******************************************************************************/
/*                          Private Typedefs                                  */
/******************************************************************************/

/* Structure of a RAM File object in FS */
typedef struct 
{
  char name[NAME_LEN];              /**< Name of the FS Object */
  char content[CONTENT_LEN];        /**< Stores the content of the RAMFile */
  char parentDir[PATH_LEN];         /**< Stores the Path Name of the Parent Dir */
  int isDirectory;                  /**< Is the RAMFile Object a File or Folder */
  int active;                       /**< Is the RAM file Active */
} RAMFile;

/* Structure used to store Kernel Dmesg to be logged on console */
typedef struct {
  unsigned long timestamp;          /**< Timestamp to be added on Dmesg log */
  char message[DMESG_LEN];          /**< Message to be displayed on log */
} DmesgEntry;

/******************************************************************************/
/*                     Private Variable Declarations                          */
/******************************************************************************/

/* Declare the file system object (a collection of RAMFile objects) */
RAMFile fs[MAX_FILES];

/* The FS begins at Root Directory */
char currentPath[PATH_LEN] = "/";

/* Declare an empty Input Buffer for interactive Prompt */
char inputBuffer[32] = "";

/* Define a variable to store input Length of the Prompt */
int inputLen = 0;

/* Declare a global structure to log Kernel Dmesgs*/
DmesgEntry dmesg[DMESG_LINES];

/* Define a variable to keep track of number of Dmesgs passed */
int dmesgIndex = 0;

/******************************************************************************/
/*                   Private Functions Prototype                              */
/******************************************************************************/

/* Function to report the amount of free SRAM available */
int freeMemory(void);

/* Function Defined to reboot the system by transfering system control to 
 * the reset vector located at address 0x0 */
void(* resetFunc) (void) = 0;

/* Function to add an entry to the FS dmesg log */
void addDmesg(const char* msg);

/* Function to initialize the fs object */
void initFS(void);

/* Adds the command line prompt text on the serial console for 
 * user interaction */
void printPrompt(void);

/* Function which inteprets the content of a RAM File object 
 * and runs commands on Kernel CLI accordingly */
void runScript(const char* content);

/* Returns the index at which the substr occurs in the str 
 * Returns -1 if substr is not found in str */
int indexOf(const char* str, const char* substr);

/* Convert a numeric string into an integer */
int atoi_safe(const char* str);

/* Converts the all the letters to lowercase for command interpretation */
void toLowercase(char* str);

/* Function to concatenate two paths and store in 1 variable 
 * Returns [0] on Success 
 * Returns [1] on String Length too long */
int safeConcatPath(char* dest, const char* add);

/* The Ultimate SwitchCase to interpret the command provided on the CLI/Script */
void executeCommand(char* line);

/******************************************************************************/
/*                           Public Functions                                 */
/******************************************************************************/

/* -------------------------------------------------------------------------- */
/*                            Setup Function                                  */
/* -------------------------------------------------------------------------- */

void setup() 
{
  /* Begin UART communication */
  Serial.begin(115200);
  
  /* Initialize the FS*/
  initFS(); 
  
  /* TODO: Modify to 100ms and see if there is behavioural change */
  delay(1000);
  
  /* Indicate the completion of initialization */
  Serial.println(F("\n--- KernelUNO v1.0 ---"));
  Serial.println(F("Type 'help' for commands"));
  
  /* Show the command prompt to accept user input */
  printPrompt();
}

/* -------------------------------------------------------------------------- */
/*                            Loop Function                                   */
/* -------------------------------------------------------------------------- */

void loop() {
  /* Check if the user has provided any input on the interactive prompt */
  if (Serial.available() > 0) 
  {
    /* Pick up the available character */
    char c = Serial.read();
    
    if (c == '\r' || c == '\n') 
    {
      /* if the user presses an 'ENTER' key to validate the command */
      if (inputLen > 0) 
      {
        inputBuffer[inputLen] = '\0';
        Serial.println();
        
        /* Execute the complete command in the inputBuffer */
        executeCommand(inputBuffer);

        /* reset the inputBuffer */
        inputLen = 0;
        memset(inputBuffer, 0, 32);

        /* Show the user the interactive command prompt once again */
        printPrompt();
      }
    } 
    else if (c == 8 || c == 127) 
    {
      /* if the user presses a 'backspace' or a 'delete' key */
      if (inputLen > 0) 
      {
        inputLen--;
        inputBuffer[inputLen] = '\0';
        Serial.print(F("\b \b")); 
      }
    } 
    else if (inputLen < 31) 
    {
      /* Load the character from the command line into the input buffer */
      Serial.print(c);
      inputBuffer[inputLen] = c;
      inputLen++;
    }
  }
}

/******************************************************************************/
/*                          Private Functions                                 */
/******************************************************************************/

int freeMemory() 
{
  /* memory pointer that marks the boundary between heap and stack */
  extern int *__brkval;
  /* Contains the address to the start of heap */
  extern int __heap_start; 
  int v;
  
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void addDmesg(const char* msg) 
{
  /* Add Check to see if we have reached the maximum number of Dmesg logs */
  if (dmesgIndex >= DMESG_LINES) dmesgIndex = 0;

  /* Log the dmesg Object */
  dmesg[dmesgIndex].timestamp = millis() / 1000;
  strncpy(dmesg[dmesgIndex].message, msg, DMESG_LEN - 1);
  dmesg[dmesgIndex].message[DMESG_LEN - 1] = '\0';

  /* Increment the Dmesg Index */
  dmesgIndex++;
}

void initFS() 
{
  /* Define local variables */
  const char* dirs[] = {"home", "dev"};
  int d, i;
  
  /* Iterate over directories in FS */
  for (d = 0; d < 2; d++) 
  {
    /* Iterate over RAM File Objects in the Directory */
    for (i = 0; i < MAX_FILES; i++) 
    {
      /* For every inactive RAM File Object */
      if (!fs[i].active) 
      {
        /* Initialize the FS Object Name field */
        strncpy(fs[i].name, dirs[d], NAME_LEN - 1);
        fs[i].name[NAME_LEN - 1] = '\0';
        
        /* Initialize the FS Object Parent Directory field */
        strncpy(fs[i].parentDir, "/", PATH_LEN - 1);
        fs[i].parentDir[PATH_LEN - 1] = '\0';
        
        /* Define the FS Object as active directories in the FS */
        fs[i].isDirectory = 1;
        fs[i].active = 1;
        break;
      }
    }
  }
  
  char devPath[PATH_LEN] = "/dev/";
  const char* pins[] = {"pin2", "pin3", "pin4"};
  
  for (d = 0; d < 3; d++) 
  {
    for (i = 0; i < MAX_FILES; i++) 
    {
      if (!fs[i].active) 
      {
        /* Initialize the FS Object Name field */
        strncpy(fs[i].name, pins[d], NAME_LEN - 1);
        fs[i].name[NAME_LEN - 1] = '\0';
        
        /* Initialize the FS Object Parent Directory field */
        strncpy(fs[i].parentDir, devPath, PATH_LEN - 1);
        fs[i].parentDir[PATH_LEN - 1] = '\0';
        
        /* Define the FS Object as active files in the FS */
        fs[i].isDirectory = 0;
        fs[i].content[0] = '\0';
        fs[i].active = 1;
        break;
      }
    }
  }
  
  addDmesg("Kernel initialized");
  addDmesg("Filesystem mounted");
  addDmesg("Ready for commands");
}

void printPrompt() 
{
  Serial.print(F("root@arduino:"));
  Serial.print(currentPath);
  Serial.print(F("# "));
}

void runScript(const char* content) 
{
  /* Define local Variables */
  char line[CONTENT_LEN];
  int ci = 0, li = 0, lineNum = 0;
  int len = strlen(content);

  while (ci <= len) 
  {
    /* add ';' at the end of command */
    char c = (ci < len) ? content[ci] : ';';
    ci++;

    /* Check if the context index has reached the end of a singular command*/
    if (c == ';' || c == '\n' || c == '\r') 
    {
      if (li > 0) 
      {
        line[li] = '\0';
        lineNum++;
        Serial.print(F("[sh:")); Serial.print(lineNum); Serial.print(F("] "));
        Serial.println(line);
        
        /* Execute the formed command and reset the line index to 
         * read the following script */
        executeCommand(line);
        li = 0;
      }
    } else 
    {
      if (li < (CONTENT_LEN - 1)) line[li++] = c;
    }
  }

  /* Add logs to indicate Script Excution Completion */
  addDmesg("sh: script done");
  Serial.println(F("[sh] done."));
}

int indexOf(const char* str, const char* substr) 
{
  int i, j, slen = strlen(str), sublen = strlen(substr);
  for (i = 0; i <= slen - sublen; i++) 
  {
    /* Assume match is found */
    int match = 1;
    for (j = 0; j < sublen; j++) 
    {
      if (str[i + j] != substr[j]) 
      {
        /* if the comparision diverges anywhere, reset the match variable */
        match = 0;
        break;
      }
    }

    /* If a match is found, return the start index of 
     * the sub string in the complete string */
    if (match) return i;
  }
  return -1;
}

int atoi_safe(const char* str) 
{
  int num = 0;
  
  while (*str >= '0' && *str <= '9') 
  {
    num = num * 10 + (*str - '0');
    str++;
  }

  return num;
}

void toLowercase(char* str) 
{
  int i;
  for (i = 0; str[i] != '\0'; i++) 
  {
    /* If the current character is an uppercase letter */
    if (str[i] >= 'A' && str[i] <= 'Z') 
    {
      /* Convert it to lowercase */
      str[i] = str[i] - 'A' + 'a';
    }
  }
}

int safeConcatPath(char* dest, const char* add) 
{
  /* Define Local Variables */
  int retVal = 0;
  int destLen = strlen(dest);
  int addLen = strlen(add);
  
  /* Check whether it is possible to concatenate the 2 strings */
  if (destLen + addLen + 2 >= PATH_LEN) 
  {
    /* Return Error Code 1: Path too long */  
    retVal = 1;
  } else 
  {
    /* Perform intended path concatenation */
    strncat(dest, add, PATH_LEN - destLen - 1);
    strncat(dest, "/", PATH_LEN - destLen - 1);
  }
  
  return retVal;
} 

void executeCommand(char* line) 
{
  char cmd[32] = "";
  char args[32] = "";
  int space1 = -1;
  int i, sp, pin, count;
  char buf[40];
  
  strncpy(cmd, line, 31);
  cmd[31] = '\0';
  
  for (i = 0; cmd[i] != '\0'; i++) {
    if (cmd[i] == ' ') {
      space1 = i;
      strncpy(args, cmd + i + 1, 31);
      args[31] = '\0';
      cmd[i] = '\0';
      break;
    }
  }
  
  toLowercase(cmd);
  
  if (strcmp(cmd, "pinmode") == 0) {
    sp = indexOf(args, " ");
    if (sp == -1) { Serial.println(F("Usage: pinmode [pin] [in/out]")); return; }
    pin = atoi_safe(args);
    char mode[8] = "";
    strncpy(mode, args + sp + 1, 7);
    mode[7] = '\0';
    toLowercase(mode);
    if (strcmp(mode, "out") == 0) { 
      pinMode(pin, OUTPUT); 
      snprintf(buf, sizeof(buf), "Pin %d set to OUTPUT", pin);
      addDmesg(buf);
      Serial.println(F("Pin set to OUTPUT")); 
    }
    else if (strcmp(mode, "in") == 0) { 
      pinMode(pin, INPUT_PULLUP); 
      snprintf(buf, sizeof(buf), "Pin %d set to INPUT", pin);
      addDmesg(buf);
      Serial.println(F("Pin set to INPUT_PULLUP")); 
    }
  }
  else if (strcmp(cmd, "write") == 0) {
    sp = indexOf(args, " ");
    if (sp == -1) { Serial.println(F("Usage: write [pin] [high/low]")); return; }
    pin = atoi_safe(args);
    char val[8] = "";
    strncpy(val, args + sp + 1, 7);
    val[7] = '\0';
    toLowercase(val);
    digitalWrite(pin, (strcmp(val, "high") == 0 ? HIGH : LOW));
    snprintf(buf, sizeof(buf), "Pin %d wrote %s", pin, strcmp(val, "high") == 0 ? "HIGH" : "LOW");
    addDmesg(buf);
    Serial.println(F("Write OK."));
  }
  else if (strcmp(cmd, "read") == 0) {
    pin = atoi_safe(args);
    int value = digitalRead(pin);
    Serial.print(F("Pin ")); Serial.print(pin);
    Serial.print(F(" value: ")); Serial.println(value);
    snprintf(buf, sizeof(buf), "Pin %d read: %d", pin, value);
    addDmesg(buf);
  }
  else if (strcmp(cmd, "gpio") == 0) {
    sp = indexOf(args, " ");
    if (sp == -1) { 
      Serial.println(F("Usage: gpio [pin] [on/off] OR gpio vixa [count]"));
      return; 
    }
    char pinStr[8] = "";
    strncpy(pinStr, args, sp);
    pinStr[sp] = '\0';
    char action[8] = "";
    strncpy(action, args + sp + 1, 7);
    action[7] = '\0';
    toLowercase(action);
    
    if (strcmp(pinStr, "vixa") == 0) {
      count = atoi_safe(action);
      if (count <= 0) count = 10;
      addDmesg("LED disco mode activated");
      Serial.println(F("LED DISCO MODE!"));
      
      int cycle, p;
      for (cycle = 0; cycle < count; cycle++) {
        for (p = 2; p <= 13; p++) {
          pinMode(p, OUTPUT);
          digitalWrite(p, HIGH);
          delay(50);
          digitalWrite(p, LOW);
        }
      }
      Serial.println(F("Disco finished!"));
      addDmesg("Disco complete");
    } else {
      pin = atoi_safe(pinStr);
      if (strcmp(action, "on") == 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        snprintf(buf, sizeof(buf), "GPIO %d ON", pin);
        addDmesg(buf);
        Serial.print(F("GPIO ")); Serial.print(pin); Serial.println(F(" ON"));
      }
      else if (strcmp(action, "off") == 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        snprintf(buf, sizeof(buf), "GPIO %d OFF", pin);
        addDmesg(buf);
        Serial.print(F("GPIO ")); Serial.print(pin); Serial.println(F(" OFF"));
      }
      else if (strcmp(action, "toggle") == 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, !digitalRead(pin));
        snprintf(buf, sizeof(buf), "GPIO %d toggled", pin);
        addDmesg(buf);
        Serial.print(F("GPIO ")); Serial.print(pin); Serial.println(F(" toggled"));
      }
    }
  }
  else if (strcmp(cmd, "ls") == 0) {
    int empty = 1, j;
    for (j = 0; j < MAX_FILES; j++) {
      if (fs[j].active && strcmp(fs[j].parentDir, currentPath) == 0) {
        Serial.print(fs[j].name);
        if (fs[j].isDirectory) Serial.print(F("/"));
        Serial.print(F("  "));
        empty = 0;
      }
    }
    if (empty) Serial.print(F("(empty)"));
    Serial.println();
  }
  else if (strcmp(cmd, "mkdir") == 0 || strcmp(cmd, "touch") == 0) {
    int foundSlot = -1, j;
    for (j = 0; j < MAX_FILES; j++) { 
      if (!fs[j].active) { foundSlot = j; break; } 
    }
    if (foundSlot == -1) {
      Serial.println(F("No space."));
      return;
    }
    
    strncpy(fs[foundSlot].name, args, NAME_LEN - 1);
    fs[foundSlot].name[NAME_LEN - 1] = '\0';
    strncpy(fs[foundSlot].parentDir, currentPath, PATH_LEN - 1);
    fs[foundSlot].parentDir[PATH_LEN - 1] = '\0';
    fs[foundSlot].isDirectory = (strcmp(cmd, "mkdir") == 0);
    fs[foundSlot].content[0] = '\0';
    fs[foundSlot].active = 1;
    Serial.println(F("OK."));
  }
  else if (strcmp(cmd, "cd") == 0) {
    if (strcmp(args, "..") == 0 || strcmp(args, "/") == 0) {
      strncpy(currentPath, "/", PATH_LEN - 1);
      currentPath[PATH_LEN - 1] = '\0';
    }
    else {
      int j, found = 0;
      for (j = 0; j < MAX_FILES; j++) {
        if (fs[j].active && fs[j].isDirectory && strcmp(args, fs[j].name) == 0 && strcmp(fs[j].parentDir, currentPath) == 0) {
          /* safeConcatPath Doesn't return 0, the Path was too long to be formed */
          if (safeConcatPath(currentPath, fs[j].name)) 
          {
            strncpy(currentPath, "/", PATH_LEN - 1);
            currentPath[PATH_LEN - 1] = '\0';
            Serial.println(F("Path too long."));
            return;
          }
          found = 1;
          break;
        }
      }
      if (!found) Serial.println(F("No dir."));
    }
  }
  else if (strcmp(cmd, "pwd") == 0) {
    Serial.println(currentPath);
  }
  else if (strcmp(cmd, "echo") == 0) {
    int arrow = indexOf(args, " > ");
    if (arrow != -1) {
      char text[40] = "";
      strncpy(text, args, arrow);
      text[arrow] = '\0';
      char filename[12] = "";
      strncpy(filename, args + arrow + 3, NAME_LEN - 1);
      filename[NAME_LEN - 1] = '\0';
      
      int j, found = 0;
      for (j = 0; j < MAX_FILES; j++) {
        if (fs[j].active && !fs[j].isDirectory && strcmp(filename, fs[j].name) == 0 && strcmp(fs[j].parentDir, currentPath) == 0) {
          strncpy(fs[j].content, text, CONTENT_LEN - 1);
          fs[j].content[CONTENT_LEN - 1] = '\0';
          Serial.println(F("Saved."));
          // Jeśli plik jest w /dev/ i nazywa się pinX
          if (strcmp(fs[j].parentDir, "/dev/") == 0 && strncmp(fs[j].name, "pin", 3) == 0) {
            int devPin = atoi_safe(fs[j].name + 3);
            if (devPin > 0) {
              pinMode(devPin, OUTPUT);
              digitalWrite(devPin, (text[0] == '1') ? HIGH : LOW);
              snprintf(buf, sizeof(buf), "GPIO %d %s via echo", devPin, (text[0] == '1') ? "HIGH" : "LOW");
              addDmesg(buf);
            }
          }
          found = 1;
          break;
        }
      }
      if (!found) Serial.println(F("File not found."));
    }
    else {
      Serial.println(args);
    }
  }
  else if (strcmp(cmd, "cat") == 0) {
    int j, found = 0;
    for (j = 0; j < MAX_FILES; j++) {
      if (fs[j].active && !fs[j].isDirectory && strcmp(args, fs[j].name) == 0 && strcmp(fs[j].parentDir, currentPath) == 0) {
        Serial.println(fs[j].content);
        found = 1;
        break;
      }
    }
    if (!found) Serial.println(F("File not found."));
  }
  else if (strcmp(cmd, "info") == 0) {
    int j, found = 0;
    for (j = 0; j < MAX_FILES; j++) {
      if (fs[j].active && strcmp(args, fs[j].name) == 0 && strcmp(fs[j].parentDir, currentPath) == 0) {
        Serial.print(F("Name: ")); Serial.println(fs[j].name);
        Serial.print(F("Type: ")); Serial.println(fs[j].isDirectory ? F("Directory") : F("File"));
        Serial.print(F("Size: ")); Serial.print(strlen(fs[j].content)); Serial.println(F(" bytes"));
        found = 1;
        break;
      }
    }
    if (!found) Serial.println(F("Not found."));
  }
  else if (strcmp(cmd, "rm") == 0) {
    int j, found = 0;
    for (j = 0; j < MAX_FILES; j++) {
      if (fs[j].active && strcmp(args, fs[j].name) == 0 && strcmp(fs[j].parentDir, currentPath) == 0) {
        if (fs[j].isDirectory) {
          // Rekursywnie usuń wszystko wewnątrz katalogu
          char dirPath[PATH_LEN];
          strncpy(dirPath, currentPath, PATH_LEN - 1);
          dirPath[PATH_LEN - 1] = '\0';
          snprintf(dirPath, PATH_LEN, "%s%s/", currentPath, args);
          
          int k;
          for (k = 0; k < MAX_FILES; k++) {
            if (fs[k].active && strncmp(fs[k].parentDir, dirPath, strlen(dirPath)) == 0) {
              fs[k].active = 0;
            }
          }
        }
        fs[j].active = 0;
        Serial.println(F("Removed."));
        found = 1;
        break;
      }
    }
    if (!found) Serial.println(F("Not found."));
  }
  else if (strcmp(cmd, "dmesg") == 0) {
    Serial.println(F("=== KERNEL MESSAGES ==="));
    int j;
    for (j = 0; j < DMESG_LINES; j++) {
      if (dmesg[j].message[0] != '\0') {
        Serial.print(F("["));
        Serial.print(dmesg[j].timestamp);
        Serial.print(F("] "));
        Serial.println(dmesg[j].message);
      }
    }
  }
  else if (strcmp(cmd, "uptime") == 0) {
    unsigned long s = millis()/1000;
    unsigned long h = s / 3600;
    unsigned long m = (s % 3600) / 60;
    unsigned long sec = s % 60;
    Serial.print(F("up "));
    Serial.print(h); Serial.print(F("h "));
    Serial.print(m); Serial.print(F("m "));
    Serial.print(sec); Serial.println(F("s"));
    addDmesg("uptime command");
  }
  else if (strcmp(cmd, "df") == 0 || strcmp(cmd, "free") == 0) {
    Serial.print(F("Free RAM: "));
    Serial.print(freeMemory());
    Serial.println(F(" bytes"));
  }
  else if (strcmp(cmd, "whoami") == 0) {
    Serial.println(F("root"));
  }
  else if (strcmp(cmd, "uname") == 0) {
    Serial.println(F("KernelUNO v1.0"));
    Serial.print(F("Kernel: Arduino "));
    Serial.println(F("AVR"));
    Serial.print(F("Hardware: "));
    Serial.println(F("Arduino UNO"));
    Serial.print(F("RAM: "));
    Serial.print(freeMemory());
    Serial.println(F(" bytes free"));
  }
  else if (strcmp(cmd, "reboot") == 0) {
    Serial.println(F("Rebooting..."));
    addDmesg("System reboot");
    delay(500);
    resetFunc();
  }
  else if (strcmp(cmd, "clear") == 0) {
    int j;
    for(j = 0; j < 30; j++) Serial.println();
  }
  else if (strcmp(cmd, "sh") == 0) {
    if (args[0] == '\0') {
      Serial.println(F("Usage: sh [script]"));
      return;
    }
    int j, found = 0;
    for (j = 0; j < MAX_FILES; j++) {
      if (fs[j].active && !fs[j].isDirectory &&
          strcmp(args, fs[j].name) == 0 &&
          strcmp(fs[j].parentDir, currentPath) == 0) {
        found = 1;
        addDmesg("sh: running script");
        runScript(fs[j].content);
        break;
      }
    }
    if (!found) Serial.println(F("Script not found."));
  }
  else if (strcmp(cmd, "help") == 0) {
    Serial.println(F("Commands: ls, cd, pwd, mkdir, touch, cat, echo, rm, info"));
    Serial.println(F("          pinmode, write, read, gpio, sh"));
    Serial.println(F("          uptime, uname, dmesg, df, free, whoami, clear, reboot"));
    Serial.println(F("GPIO: gpio [pin] on/off/toggle  |  gpio vixa [count]"));
    Serial.println(F("SH:   sh [file]  -- run script (use ; as line separator)"));
  }
  else {
    Serial.println(F("Unknown command."));
  }
}