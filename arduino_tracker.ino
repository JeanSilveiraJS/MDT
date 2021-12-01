#include <SoftwareSerial.h>
#include <TinyGPS.h>

SoftwareSerial serialGSM(9, 10); // RX, TX
SoftwareSerial serialGPS(11, 12);

TinyGPS gps;

const String listaComandos = "Lista de comandos:\n  LOCAL - receber o local atual;\n  RT - receber o local a cada 1 minuto;\n  RESET - reconfigurar modulo GSM.";

static unsigned long delayStart = millis();

bool temSMS = false;
bool RT = false;
bool lido = false;

String telefoneSMS;
String telefoneRT;
String mensagemSMS;
String mensagem = "";
String comandoGSM = "";
String ultimoGSM;
String urlMapa;

void leGSM();
void leGPS();
void enviaSMS(String telefone, String mensagem);
void configuraGSM();

void setup() //Checklist dos módulos para garantir que estão funcionando
{
   pinMode(LED_BUILTIN, OUTPUT);
   Serial.begin(9600);
   serialGPS.begin(9600);   
   serialGSM.begin(9600);
   
   delay(3000);

   Serial.println("Iniciando. Aguarde alguns instantes... ----------------------------------------");

   Serial.println("(1/2) Aguardando GSM... Voce devera ver 4 'OK'. -------------------------------");

   while (serialGPS.available() < 0 || ultimoGSM.substring(2, 4) != "OK") {
      while (ultimoGSM.substring(2, 4) != "OK")
      {
         digitalWrite(LED_BUILTIN, HIGH); delay(5000);
         configuraGSM();

         if ( (millis() - delayStart) > 90000) //Timeout 1:30m GSM
         {
            Serial.println("Sem resposta, tentando novamente..."); 
            digitalWrite(LED_BUILTIN, LOW); delay(500);
            break;
         }
      }
      delayStart = millis();
   }

   Serial.println("\nGSM OK!\n");

   Serial.println("(2/2) Aguardando GPS, isso pode levar alguns minutos... -----------------------");

   while (serialGPS.available() < 0 || !lido) {   
      while (!lido)
      {
         digitalWrite(LED_BUILTIN, HIGH);

         leGPS();

         if ( (millis() - delayStart) > 90000) //Timeout 1:30m GPS
         {
            Serial.println("Sem resposta, tentando novamente..."); 
            digitalWrite(LED_BUILTIN, LOW); delay(500);
            break;
         }
      }
      delayStart = millis();
   }
   
   Serial.println("\nGPS OK!\n"); digitalWrite(LED_BUILTIN, LOW);

   Serial.println("Iniciado com sucesso! ---------------------------------------------------------");
}

void loop() 
{
   if ( (millis() - delayStart) > 60000) { leGPS(); delayStart = millis(); } //Lendo o GPS a cada 1 min

   leGSM(); //Lendo GSM constantemente

   if (temSMS) //Caso houver mensagen, exibi-la no terminal
   {
      Serial.println("\nNova mensagem! ----------------");
      
      Serial.print("Remetente: ");  
      Serial.println(telefoneSMS);
      
      Serial.println("Mensagem:");  
      Serial.print(mensagemSMS);

      Serial.println("-------------------------------\n");

      mensagemSMS.trim();
      
      //Switch (mensagemSMS)
      if (mensagemSMS == "LOCAL") { enviaSMS(telefoneSMS, String(urlMapa)); } //Requisição do local atual
      else if (mensagemSMS == "RT") //Requisição de localização em tempo real
      { 
         if (!RT)
         { 
            RT = true; 
            telefoneRT = telefoneSMS;
            enviaSMS(telefoneRT, "Local em tempo real ativado. O novo local sera atualizado a cada 1 minuto.");
         }
         else { 
            RT = false;
            enviaSMS(telefoneRT, "Local em tempo real desativado.");
            telefoneRT = "";
         }
      }
      else if (mensagemSMS == "RESET") { configuraGSM(); } //Requisição de RESET
      else { enviaSMS(telefoneSMS, String(listaComandos)); } //Default 

      temSMS = false;
   }
}

void leGSM() 
{
   static String textoRec = "";
   static unsigned long delay1 = 0;
   static int count = 0;  
   static unsigned char buffer[64];

   serialGSM.listen(); 

   if (serialGSM.available()) 
   {
      while (serialGSM.available()) 
      {
         buffer[count++] = serialGSM.read();     
         if (count == 64) break;
      }

      textoRec += (char*)buffer;
      delay1 = millis();

      for (int i=0; i<count; i++) { buffer[i] = NULL; }

      count = 0;
   }

   if ( ((millis() - delay1) > 100) && textoRec != "") 
   {
      if (textoRec.substring(2,7) == "+CMT:") { temSMS = true; }

      if (temSMS) //Se for um SMS, converter em Strings
      {
         telefoneSMS = "";
         mensagemSMS = "";

         byte linha = 0;  
         byte aspas = 0;

         for (int nL=1; nL < textoRec.length(); nL++)
         {
            if (textoRec.charAt(nL) == '"') { aspas++; continue; }

            if ( (linha == 1) && (aspas == 1) ) { telefoneSMS += textoRec.charAt(nL); }

            if (linha == 2) { mensagemSMS += textoRec.charAt(nL); }

            if (textoRec.substring(nL - 1, nL + 1) == "\r\n") { linha++; }
         }
      } else { comandoGSM = textoRec; } //Senão, enviar para 'comandoGSM'

      textoRec = "";  
   }

   if (comandoGSM != "") //Se houver uma mensage do módulo, mostrar no terminal
   {
      Serial.print(comandoGSM);
      ultimoGSM = comandoGSM;
      comandoGSM = "";
   }
}

void leGPS()
{
   unsigned long delayGPS = millis();
   
   serialGPS.listen();
   lido = false;

   while ( (millis() - delayGPS) < 500)
   {
      while (serialGPS.available())
      {
         char cIn = serialGPS.read();
         lido = gps.encode(cIn);
      }

      if (lido) //Se houver dados vindos do módulo GPS, convertê-los em uma String
      {
         float flat, flon;
         unsigned long age;

         gps.f_get_position(&flat, &flon, &age);

         urlMapa = "";
         
         if ( (flat == TinyGPS::GPS_INVALID_F_ANGLE) || (flon == TinyGPS::GPS_INVALID_F_ANGLE) ) 
         { 
            urlMapa += "GPS sem sinal!"; 
         } else
         {
            urlMapa += "Local Identificado: https://maps.google.com/maps/?&z=10&q=";
            urlMapa += String(flat,6);
            urlMapa += ",";
            urlMapa += String(flon,6);
         }

         Serial.println();
         Serial.println(urlMapa);

         if (RT) { enviaSMS(telefoneRT, urlMapa); } //Enviar a localização para o numero registrado para tempo real

         break;
      }
   }
}

void enviaSMS(String telefone, String mensagem) { //Envia comando AT para o módulo GSM com o número e a mensagem
   delay(500);
   serialGSM.print("AT+CMGS=\"" + telefone + "\"\n");
   delay(500);
   if (mensagem != "") { serialGSM.print(mensagem); }
   else { serialGSM.print("Erro, tente novamente."); }
   serialGSM.print((char)26);
}

void configuraGSM() { //Configurações iniciais do módulo GSM para que mande e receba SMS 
   delayStart = millis();
   digitalWrite(LED_BUILTIN, HIGH);

   serialGSM.print("AT+CMGF=1\n;AT+CNMI=2,2,0,0,0\n;ATX4\n;AT+COLP=1\n");
   
   while ((millis() - delayStart) < 10000) { leGSM(); }

   delayStart = millis();
   digitalWrite(LED_BUILTIN, LOW);
}