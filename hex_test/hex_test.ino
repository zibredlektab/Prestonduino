char sprintfoutput[100];

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println();
  Serial.println("hello!");
  char inputstr[] = "ABC";
  char outputstr[(strlen(inputstr) * 2) + 1];
  string2hexString(inputstr, outputstr);
  Serial.println(outputstr);
  Serial.println(checksum(outputstr));
}

void loop() {
  if(Serial.available()) {
    while(Serial.available()){
      Serial.println(checksum(Serial.read()));
    }
  }
}

void string2hexString(char* input, char* output) {
    int loop = 0;
    int i = 0; 
    
    while(input[loop] != '\0') {
        sprintf((char*)(output+i),"%02X", input[loop]);
        loop+=1;
        i+=2;
    }
    //insert NULL at the end of the output string
    output[i++] = '\0';
}

int checksum(char* input) {
  int i = 0;
  int currentchar = 0;
  int sum = 0;
  
  while (input[i] != '\0') {
    sprintf(sprintfoutput, "char at index %d is %c", i, input[i]);
    Serial.println(sprintfoutput);
    currentchar = atoi(input[i]);
    Serial.print("interpreted that as ");
    Serial.println(currentchar);
    sum += currentchar;
    Serial.print("sum is now ");
    Serial.println(sum);
    i++;
  }

  return sum;
}
