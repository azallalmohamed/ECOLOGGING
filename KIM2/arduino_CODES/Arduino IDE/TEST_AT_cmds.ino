void setup() {
  Serial.begin(115200);   // PC
  Serial1.begin(9600);    // KIM2
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);  // Allume KIM2
  delay(8000);            // Attend démarrage
}

void loop() {
  // Lit les commandes du PC
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    Serial.print(">> ");
    Serial.println(cmd);          // Affiche la commande
    
    Serial1.print(cmd);           // Envoie au KIM2
    Serial1.write("\r\n");        // CR+LF
  }
  
  // Lit les réponses du KIM2
  if (Serial1.available()) {
    Serial.write(Serial1.read()); // Affiche la réponse
  }
}