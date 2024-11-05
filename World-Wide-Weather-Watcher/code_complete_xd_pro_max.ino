#include <Wire.h>
#include <EEPROM.h>
#include <DHT.h>
#include <BH1750.h>
#include <SD.h>
#include <RTClib.h>


// Déclaration des pins pour la LED RGB  
const int pinRouge = 5; // Connecté sur le pin 5  
const int pinVert = 6;  // Connecté sur le pin 6  
const int pinBleu = 7;  // Connecté sur le pin 7  
const int pinBoutonRouge = 2;  // Bouton rouge connecté sur pin 2  
const int pinBoutonVert = 3;   // Bouton vert connecté sur pin 3  
const int pinSD = 10;  // Pin pour la carte SD  

void setColor(int rouge, int vert, int bleu) {
  // Note : LOW pour allumer la LED, HIGH pour l'éteindre
  digitalWrite(pinRouge, rouge == 1 ? LOW : HIGH);  // Activer ou désactiver le canal rouge
  digitalWrite(pinVert, vert == 1 ? LOW : HIGH);    // Activer ou désactiver le canal vert
  digitalWrite(pinBleu, bleu == 1 ? LOW : HIGH);    // Activer ou désactiver le canal bleu
}

void BlinkLED(int rouge, int vert, int bleu, int dureeRouge, int dureeAutre) {
  for (int i = 0; i < 10; i++) { // Clignoter 5 fois
    setColor(rouge, 0, 0);
    delay(dureeRouge);
    setColor(0, vert, bleu);
    delay(dureeAutre);
  }
}

void problemRTC() {
  Serial.println(F("Erreur d'accès à l'horloge RTC"));
  BlinkLED(1, 0, 1, 500, 500); // Rouge et bleu clignotant
}

void problemCapteur() {
  Serial.println(F("Erreur accès aux données d’un capteur"));
  BlinkLED(1, 1, 0, 500, 1000); // Rouge et vert clignotant, vert 2x plus long
}

void problemCarteSD() {
  Serial.println(F("Erreur d’accès ou d’écriture sur la carte SD"));
  BlinkLED(1, 1, 1, 500, 1000); // Rouge et blanc clignotant, blanc 2x plus long
}



// Constantes pour les capteurs
#define DHTPIN 4 // Pin pour le DHT
#define DHTTYPE DHT11 // Type de DHT
DHT dht(DHTPIN, DHTTYPE); // Initialiser le capteur DHT
BH1750 lightSensor; // Initialiser le capteur de lumière
RTC_DS1307 rtc; // Initialiser le module RTC

// Enumération des modes
enum Mode { STANDARD, CONFIGURATION, MAINTENANCE, ECONOMIQUE }; // Modes  
Mode modeActuel;           // Mode actuel  
Mode modePrecedent;       // Mode précédent  
bool boutonRougeEstAppuye = false;  // État bouton rouge  
bool boutonVertEstAppuye = false;    // État bouton vert  
unsigned long tempsAppuiRouge = 0;   // Timestamp appui bouton rouge  
unsigned long tempsAppuiVert = 0;    // Timestamp appui bouton vert  
unsigned long tempsDemarrage;         // Timestamp démarrage  
unsigned long tempsConfiguration;      // Timestamp mode configuration  

// Déclaration des variables de configuration
int logInterval;
int fileMaxSize;
int timeout;
const int ADDR_LOG_INTERVAL = 0; // Adresse pour EEPROM
const int ADDR_FILE_MAX_SIZE = 1;
const int ADDR_TIMEOUT = 2;

// Prototypage des fonctions  
void setMode(Mode newMode);

void processCommand(String command); // Placeholder for processCommand function
void aquisitionData(); // Prototypage de la fonction d'acquisition de données
void saveData(float temperature, float humidity, uint16_t lux); // Prototypage de la fonction de sauvegarde des données

void setup() {  
  Serial.begin(9600);  // Initialiser communication série  
  
  Wire.begin();
  if (!lightSensor.begin()){
    problemCapteur();
  } // Démarrer le capteur de lumière
  
  
  dht.begin(); // Démarrer le capteur DHT
  
  
  if(!rtc.begin()){
    Serial.println("Échec de l'initialisation de Horloge RTC");
    problemRTC();
  } // Démarrer le module RTC
  
  // Configuration des pins comme sorties pour la LED RGB  
  pinMode(pinRouge, OUTPUT);  
  pinMode(pinVert, OUTPUT);  
  pinMode(pinBleu, OUTPUT);  
  
  
  // Configuration des boutons comme entrées avec pull-up  
  pinMode(pinBoutonRouge, INPUT_PULLUP);  
  pinMode(pinBoutonVert, INPUT_PULLUP);  

  // Initialiser la carte SD
  if (!SD.begin(pinSD)) {
    Serial.println("Échec de l'initialisation de la carte SD!");
    problemCarteSD();
    return;
  }

  // Démarrer en mode standard  
  modeActuel = STANDARD;  
  setColor(0, 1, 0); // LED verte pour mode standard  
  tempsDemarrage = millis(); // Enregistrer le temps de démarrage 
}

void loop() {  
  // Vérifier l'état du bouton rouge  
  if (digitalRead(pinBoutonRouge) == LOW) {  
    if (!boutonRougeEstAppuye) {  
      boutonRougeEstAppuye = true;  
      tempsAppuiRouge = millis();  // Enregistrer le temps de l'appui  
    } else {  
      if (millis() - tempsAppuiRouge >= 5000) { // Pour mode maintenance  
        if (modeActuel == STANDARD || modeActuel == ECONOMIQUE) {  
          modePrecedent = modeActuel;  
          modeActuel = MAINTENANCE; 
          setMode(MAINTENANCE);   
        }  
      }  
      if (millis() - tempsAppuiRouge >= 6000 && modeActuel == MAINTENANCE) { 
        if (modePrecedent == STANDARD) {  
          modeActuel = STANDARD; // Retour à mode standard  
          setMode(STANDARD);
        } else if (modePrecedent == ECONOMIQUE) {  
          modeActuel = ECONOMIQUE; // Revenir au mode économique  
          setMode(ECONOMIQUE);
        }  
        boutonRougeEstAppuye = false; // Réinitialiser l'état  
      }  
    }  
  } else {  
    if (boutonRougeEstAppuye) {  
      // Mode configuration si moins de 30 secondes  
      if (modeActuel == CONFIGURATION) {  
        modeActuel = STANDARD; // Retour au mode standard  
        setMode(STANDARD);
      } else if (modeActuel == STANDARD) {  
        modeActuel = CONFIGURATION; // Passer en mode configuration  
        setMode(CONFIGURATION);
        tempsConfiguration = millis(); // Enregistrer le temps d'entrée en configuration  
      }  
      boutonRougeEstAppuye = false;  
    }  
  }  

  // Vérifier l'état du bouton vert  
  if (digitalRead(pinBoutonVert) == LOW) {  
    if (!boutonVertEstAppuye) {  
      boutonVertEstAppuye = true;  
      if (modeActuel == STANDARD) {  
        modeActuel = ECONOMIQUE;  
        setMode(ECONOMIQUE);
      } else if (modeActuel == ECONOMIQUE) {  
        modeActuel = STANDARD; // Retour au mode standard 
        setMode(STANDARD);   
      }  
    }  
  } else {  
    if (boutonVertEstAppuye) {  
      boutonVertEstAppuye = false;  // Réinitialiser l'état  
    }  
  }  

  // Vérifier le mode configuration  
  if (modeActuel == CONFIGURATION) {  
    if (millis() - tempsConfiguration >= 30000) { // 30 secondes  
      modeActuel = STANDARD; // Bascule au mode standard 
      setMode(STANDARD); 
    }  
  }  

  // Appeler une fonction en fonction du mode actif  
  if (modeActuel == STANDARD) {
    aquisitionData(); // Appeler la fonction d'acquisition de données
  } else if (modeActuel == CONFIGURATION) {
    modeConfig();
  } else if (modeActuel == MAINTENANCE) {
    afficherData();
  } else if (modeActuel == ECONOMIQUE) {
    aquisitionData_eco();
  }
}

void setMode(Mode newMode) {
  modeActuel = newMode;
  switch (modeActuel) {
    case STANDARD:
      setColor(0, 1, 0);
      Serial.println("Mode standard: Enregistrement des données...");
      break;
    case CONFIGURATION:
      setColor(1, 1, 0);
      Serial.println("Mode configuration");
      break;
    case MAINTENANCE:
      setColor(1, 0, 0);
      Serial.println("Mode maintenance");
      Serial.println("Mode maintenance: vous puvez retirer la carte sd");
      break;
    case ECONOMIQUE:
      setColor(0, 0, 1);
      Serial.println("Mode économique");
      logInterval *= 2; // Assurez-vous que logInterval est défini
      break;
  }
}

// Fonction d'acquisition de données
void aquisitionData() {
  // Lecture de la température et de l'humidité
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (isnan (temperature) || isnan(humidity)){
    problemCapteur();
    return; 
  }

  // Lecture de la luminosité
  uint16_t lux = lightSensor.readLightLevel();
  if(lux == 65535){
    problemCapteur();
    return; 
  }

  delay(1000);

  // Sauvegarde des données sur la carte SD
  saveData(temperature, humidity, lux);
}

const uint16_t FILE_MAX_SIZE = 5000; // 2 KB

void saveData(float temperature, float humidity, uint16_t lux) {
  DateTime now = rtc.now();

  // Create the base file name based on the current date
  String baseFileName = String(now.year() % 100) + String(now.month(), DEC) + String(now.day(), DEC);
  String fileName;
  uint8_t revision = 0;

  // Check for existing files and find the next revision number
  do {
    fileName = baseFileName + "_" + String(revision) + ".LOG";
    revision++;
  } while (SD.exists(fileName));

  // Open the new file for writing
  File dataFile = SD.open(fileName, FILE_WRITE);
  
  if (!dataFile) {
    Serial.println("Erreur d'ouverture du fichier !");
    return; // Exit if file open fails
  }

  // Write data to the file
  dataFile.print(now.year(), DEC);
  dataFile.print('/');
  dataFile.print(now.month(), DEC);
  dataFile.print('/');
  dataFile.print(now.day(), DEC);
  dataFile.print(" ");
  dataFile.print(now.hour(), DEC);
  dataFile.print(':');
  dataFile.print(now.minute(), DEC);
  dataFile.print(':');
  dataFile.print(now.second(), DEC);
  dataFile.print(" - Temp: ");
  dataFile.print(temperature);
  dataFile.print(" °C, Hum: ");
  dataFile.print(humidity);
  dataFile.print(" %, Lux: ");
  dataFile.println(lux);
  
  dataFile.close(); // Close the file after writing
  
  Serial.println("Données sauvegardées avec succès dans le fichier " + fileName);
}



// Placeholder for the processCommand function
void processCommand(String command) {
  // Implement your command processing logic here
  Serial.print("Commande reçue: ");
  Serial.println(command);  
}



void afficherData() {
  // Lecture de la température et de l'humidité
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  // Lecture de la luminosité
  uint16_t lux = lightSensor.readLightLevel();

  // Sauvegarde des données sur la carte SD
   DateTime now = rtc.now();
    
    // Écrire les données dans le fichier
    Serial.print(" - Temp: ");
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print(" Température: ");  
    Serial.print(temperature);  
    Serial.print(" °C, Humidité: ");  
    Serial.print(humidity);  
    Serial.print(" %, Lumière: ");  
    Serial.print(lux); 
    Serial.println(); 

    delay (4000);
}


void aquisitionData_eco() {
  // Lecture de la température et de l'humidité
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (isnan (temperature) || isnan(humidity) ){
    problemCapteur();
    return; 
  }
  
  // Lecture de la luminosité
  uint16_t lux = lightSensor.readLightLevel();
  if(lux == 65535){
    problemCapteur();
    return;
  }


  delay(4000);

  // Sauvegarde des données sur la carte SD
  saveData(temperature, humidity, lux);

}

#define MAX_COMMAND_LENGTH 20
#define MAX_INPUT_LENGTH 10  // Define MAX_INPUT_LENGTH

// EEPROM addresses for example configuration values
#define ADDR_LOG_INTERVAL 0
#define ADDR_FILE_MAX_SIZE 1
#define ADDR_TIMEOUT 2

bool configModeActive = false; // To track if we are in config mode

// Skeleton function to handle commands in configuration mode
void modeConfig() {
    char command[MAX_COMMAND_LENGTH];
    char inputValue[MAX_INPUT_LENGTH]; // Buffer for user input values

    // Show the prompt only once when entering configuration mode
    if (!configModeActive) {
        Serial.println(F("Tapez une commande ou tapez 'HELP' pour de l'aide."));
        configModeActive = true; // Set the flag to indicate we are in config mode
    }

    // Check for user input
    if (Serial.available() > 0) {
        Serial.readBytesUntil('\n', command, sizeof(command));

        if (strncmp(command, "HELP", 4) == 0) {
            Serial.println(F("Commandes disponibles :"));
            Serial.println(F("  HELP        - Afficher les commandes disponibles"));
            Serial.println(F("  SET_LOG     - Définir l'intervalle de journalisation (en secondes)"));
            Serial.println(F("  SET_SIZE    - Définir la taille maximale du fichier (en Ko)"));
            Serial.println(F("  SET_TIMEOUT - Définir le délai d'attente du capteur (en secondes)"));
            Serial.println(F("  RESET       - Réinitialiser la configuration aux valeurs par défaut"));
            Serial.println(F("  STATUS      - Afficher la configuration actuelle"));
            Serial.println(F("  EXIT        - Quitter le mode configuration"));
            Serial.println(F("  VERSION     - Version du programme"));  // Fixed line
        } else if (strncmp(command, "SET_LOG", 7) == 0) {
            Serial.println(F("Entrez la nouvelle valeur de l'intervalle de journalisation :"));
            if (Serial.available() > 0) {
                Serial.readBytesUntil('\n', inputValue, sizeof(inputValue));
                Serial.print(F("Intervalle de journalisation défini sur : "));
                Serial.println(inputValue);
            }
        } else if (strncmp(command, "SET_SIZE", 8) == 0) {
            Serial.println(F("Entrez la nouvelle taille maximale du fichier :"));
            if (Serial.available() > 0) {
                Serial.readBytesUntil('\n', inputValue, sizeof(inputValue));
                Serial.print(F("Taille maximale du fichier définie sur : "));
                Serial.println(inputValue);
            }
        } else if (strncmp(command, "SET_TIMEOUT", 11) == 0) {
            Serial.println(F("Entrez le nouveau délai d'attente du capteur :"));
            if (Serial.available() > 0) {
                Serial.readBytesUntil('\n', inputValue, sizeof(inputValue));
                Serial.print(F("Délai d'attente du capteur défini sur : "));
                Serial.println(inputValue);
            }
        } else if (strncmp(command, "RESET", 5) == 0) {
            Serial.println(F("Réinitialisation de la configuration..."));
            
        } else if (strncmp(command, "STATUS", 6) == 0) {
            Serial.println(F("Affichage de la configuration actuelle..."));
            
        } else if (strncmp(command, "VERSION", 7) == 0) {
            Serial.println(F("1.4 VERSION BUILD BALDVEGETA"));   
        } else {
            Serial.println(F("Commande inconnue. Veuillez réessayer."));
        } 
    }
}






