#include <CTBot.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#define sensorMagnet D1
#define buzzer D2
#define button D3
#define relay D4
#define touch D5
#define ledGreen D6
#define ledRed D7
#define ledWhite D8

#define wifiSsid "Lavender"
#define wifiPass "jajajaja"

#define botToken "5357266448:AAH2pAJQOkbaK5eIemyV0FS32VqRFtLitY0"
#define telegramId 5538911886

#define url "http://iot-back-end.herokuapp.com/api/doors"
#define apiToken "kgbI2lLqKVQNUMNFkg9kE6DaMDQmX"

CTBot myBot; //objek untuk CTBot

StaticJsonDocument<2048> doc;

bool deviceActive = true; // variable untuk perangkat aktif / tidak
bool canAccessDoor = false; // akses untuk buka pintu
int canAccessDoorMsg = 0; // variable untuk kirim pesan(hanya sekali) pintu terbuka dengan akses

/*
  mode message:
  1. jika pintu terbuka maka pesan akan dikirim berkali-kali
  2. jika pintu terbuka maka pesan hanya dikirim sekali
*/
int modeMsg = 1;
int totalMsg = 0; //jika mode 2 (pesan hanya dikirim sekali) dan msgTotal > 0 maka pesan ga akan dikirim lagi
int msgDoorClosed = 1; // variable untuk mengecek pesan pintu tertutup
int sentDoorLog = 0; // variable untuk cek udah insert data pintu terbuka ke API

int checkAccess = 0;

String listCommand = "/start - Mengaktifkan bot.\n/mode_1 - Jika pintu terbuka akan mengirim pesan secara terus menerus sampai pintu kembali tertutup.\n/mode_2 - Jika pintu terbuka hanya akan mengirim pesan sekali.\n/log - Menampilkan daftar riwayat pintu tertutup & terbuka 10 terakhir.\n/give_access - Memberikan hak akses untuk membuka pintu, hak akses akan dihapus ketika pintu kembali tertutup.\n/remove_access - Mencabut hak akses untuk membuka pintu, dan buzzer akan berbunyi.\n/status - Melihat status perangkat dan mode yang digunakan.\n/ping - Mengecek koneksi perangkat dengan telegram.\n/help - Melihat daftar command yang tersedia.\n";

String getDoorLog();
String saveDoorLog(String tipe);

int arrTelegramId[] = {5538911886, 1300489861};

void checkMessage();
void checkWifiConnection();
void checkTelegramConnection();
bool checkTelegramId();

void setup() { // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(buzzer, OUTPUT);
  pinMode(sensorMagnet, INPUT_PULLUP);
  pinMode(button, INPUT_PULLUP);
  pinMode(touch, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
  pinMode(ledRed, OUTPUT);
  pinMode(ledWhite, OUTPUT);
  pinMode(ledGreen, OUTPUT);

  checkWifiConnection();
  checkTelegramConnection();
  digitalWrite(relay, HIGH);
}

void loop() {  // put your main code here, to run repeatedly:
  checkMessage();  // cek pesan atau command

  //  Serial.print(digitalRead(button));
  //  Serial.print(" <- button\n");

  switch (digitalRead(button)) {
    case HIGH:
      deviceActive = true;
      break;
    default:
      deviceActive = false;
      break;
  }

  if (deviceActive) { //jika deviceActive = true
    digitalWrite(ledWhite, HIGH);

    int statusMagnet = digitalRead(sensorMagnet); //  baca nilai sensor magnet
    //    Serial.print(statusMagnet);
    //    Serial.print(" <- status sensor\n");
    //
    //    Serial.print(checkAccess);
    //    Serial.print(" <- check access\n");

    if (digitalRead(touch) == HIGH) {
      canAccessDoor = true;
      digitalWrite(relay, LOW);
      myBot.sendMessage(telegramId, "Akses diberikan menggunakan sensor sentuh!");
    }

    if (statusMagnet == HIGH) { // sensor menjauh
      switch (canAccessDoor) { // kalo punya akses pintu
        case true:
          digitalWrite(ledGreen, HIGH);
          if (checkAccess == 1) {
            checkAccess = 2;
          }

          if (canAccessDoorMsg == 0) { // kirim pesan pintu terbuka dengan akses hanya sekali
            myBot.sendMessage(telegramId, "Halo, selamat datang!, kamu baru saja membuka pintu dengan hak akses.\nHak akses akan dihapus ketika pintu kembali tertutup.");
            canAccessDoorMsg = 1;
            digitalWrite(relay, HIGH);
          }
          break;
        default:
          tone(buzzer, 3000);
          digitalWrite(ledRed, HIGH);
          digitalWrite(relay, HIGH);
          checkAccess = 0;

          if (modeMsg == 1 || totalMsg == 0) { // jika mode 1 (kirim pesan berkali-kali) atau jika mode 2 dan totalMSg = 0
            myBot.sendMessage(telegramId, "Awas pintu baru saja terbuka secara paksa!");
            totalMsg = 1;
          }
          break;
      }

      if (sentDoorLog == 0) { // 1: udah insert data pintu terbuka ke API, 0: belum
        saveDoorLog("open");
        sentDoorLog = 1;
      }

      msgDoorClosed = 0; //ubah jadi 0, berarti pintu habis terbuka
    } else {  // jika pintu kembali tertutp / sensor berdekatan
      noTone(buzzer);
      //      delay(500);
      digitalWrite(ledRed, LOW);
      digitalWrite(ledGreen, LOW);

      //kalo 2 berarti pintu udh kebuka make akses
      if (checkAccess == 2) {
        canAccessDoor = false;
        checkAccess = 0;
      }

      switch (canAccessDoor) { // kalo punya akses pintu
        case true:
          checkAccess = 1;
          break;
        default:
          checkAccess = 0;
          canAccessDoor = false;
          break;
      }

      totalMsg = 0; // reset totalMsg menjadi 0 agar jika pintu terbuka kembali dapat mengirim pesan(sekali)

      if (msgDoorClosed == 0) { // jika pintu tertutup kirim pesan hanya sekali
        myBot.sendMessage(telegramId, "Pintu sudah kembali tertutup!");
        msgDoorClosed = 1;
        digitalWrite(relay, HIGH);
      }

      if (sentDoorLog == 1) { // kalo 1 berarti pintu habis terbuka, lalu insert log ke API
        saveDoorLog("closed");
        sentDoorLog = 0;
      }

      canAccessDoorMsg = 0;
    }
  } else { // kalo deviceActive = false / dimatikan
    noTone(buzzer);
    digitalWrite(ledRed, LOW);
    digitalWrite(ledWhite, LOW);
    digitalWrite(ledGreen, LOW);
    digitalWrite(relay, HIGH);

    totalMsg = 0;
  }
}

void checkWifiConnection() {
  WiFi.begin(wifiSsid, wifiPass);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi");
}

void checkTelegramConnection() {
  Serial.println("Connecting to Telegram...");

  myBot.wifiConnect(wifiSsid, wifiPass); //  koneksikan telegram melalui wifi
  myBot.setTelegramToken(botToken);

  //test koneksi ke telegram
  if (myBot.testConnection()) {
    digitalWrite(ledWhite, HIGH);
    Serial.println("Connected to Telegram!");
    for ( int i = 0; i < 3; i++ )
    {
      digitalWrite(ledWhite, HIGH);
      tone(buzzer, 3000);
      delay(250);
      noTone(buzzer);
      digitalWrite(ledWhite, LOW);
      delay(250);
    }
  } else {
    digitalWrite(ledRed, HIGH);
    Serial.println("Gagal terkoneksi dengan telegram!");
    while (true);
  }
}

bool checkTelegramId(int senderId) {
  for (int i = 0; i <= 5; i++) {
    if (arrTelegramId[i] != senderId) {
      Serial.println("false ID");
      return false;
    }
  }
  Serial.println("true ID");
  return true;
}

void checkMessage() {

  TBMessage msg;  //  baca pesan telegram yg terakhir dikirim (object dari CTBot)

  //  kalo yang ngirim orang laen
  //  switch (checkTelegramId(msg.sender.id)) {
  //  myBot.sendMessage(msg.sender.id, "Maaf nomer kamu tidak terdaftar dalam sistem!");

  if (myBot.getNewMessage(msg)) {
    //    String latestMsg = msg.text;
    if (msg.sender.id == 5538911886 || msg.sender.id == 1300489861) {
      String latestMsg = msg.text;

      if (latestMsg == "/start") {

        deviceActive = true;
        modeMsg = 1;
        totalMsg = 0;

        String replyMsg = "Bot berhasil diaktifkan! \n\n" + listCommand;

        // ambil id pengirim pesan
        myBot.sendMessage(msg.sender.id, replyMsg);

      } else if (latestMsg == "/mode_1") {

        modeMsg = 1;

        String replyMsg = "Menggunakan mode 1!\n";
        replyMsg += "Jika pintu terbuka, kamu akan mendapatkan pesan berkali-kali sampai pintu kembali tertutup.\n";

        myBot.sendMessage(msg.sender.id, replyMsg);

      } else if (latestMsg == "/mode_2") {

        modeMsg = 2;
        totalMsg = 0;

        String replyMsg = "Menggunakan mode 2!\n";
        replyMsg += "Jika pintu terbuka, kamu hanya akan menerima pesan sekali. \n";

        myBot.sendMessage(msg.sender.id, replyMsg);

      } else if (latestMsg == "/help") {

        String replyMsg = "Berikut adalah daftar command yang tersedia: \n\n" + listCommand;

        myBot.sendMessage(msg.sender.id, replyMsg);
      } else if (latestMsg == "/status") {

        String statusDevice;
        String statusAccess;
        String statusDoor;

        switch (deviceActive) {
          case true:
            statusDevice = "Aktif";
            break;
          default:
            statusDevice = "Nonaktif";
            break;
        }

        switch (canAccessDoor) {
          case true:
            statusAccess = "Akses diberikan";
            break;
          default:
            statusAccess = "Tidak ada";
            break;
        }

        switch (digitalRead(sensorMagnet)) {
          case HIGH:
            statusDoor = "Terbuka";
            break;
          default:
            statusDoor = "Tertutup";
            break;
        }

        String replyMsg = (String) "Status perangkat: " + statusDevice + ".\n";
        replyMsg += (String) "Mode yang digunakan: " + modeMsg + ".\n";
        replyMsg += (String) "Akses pintu: " + statusAccess + ".\n";
        replyMsg += (String) "Status pintu: " + statusDoor + ".\n";

        myBot.sendMessage(msg.sender.id, replyMsg);

      } else if (latestMsg == "/ping") {

        myBot.sendMessage(msg.sender.id, "Pong!");

      } else if (latestMsg == "/give_access") {
        canAccessDoor = true;

        myBot.sendMessage(msg.sender.id, "Kamu telah memiliki akses untuk membuka pintu!, alat tidak akan berbunyi meskipun pintu sedang terbuka.");
        //      Serial.print(canAccessDoor);
        //      Serial.println(" <- akses door");
        digitalWrite(relay, LOW);

      } else if (latestMsg == "/remove_access") {
        canAccessDoor = false;

        myBot.sendMessage(msg.sender.id, "Akses untuk membuka pintu telah dihilangkan!, alat akan berbunyi ketika pintu sedang terbuka.");
        //      Serial.print(canAccessDoor);
        //      Serial.println(" <- akses door");

        digitalWrite(relay, HIGH);

      } else if (latestMsg == "/log") {

        String logDoor = getDoorLog();
        myBot.sendMessage(msg.sender.id, logDoor);

      } else {

        String replyMsg = "Maaf, command " + latestMsg + " saat ini tidak tersedia! \n";
        replyMsg += "Untuk melihat daftar command yang tersedia kamu bisa gunakan /help \n";

        myBot.sendMessage(msg.sender.id, replyMsg);

      }
    } else {
      myBot.sendMessage(msg.sender.id, "Maaf nomer kamu tidak terdaftar dalam sistem!");
    } // end of msg.sender.id == 5538911886 ||
  } // end of myBot.getNewMessage(msg)
} // end of void 

String getDoorLog() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, (String) url + "?api_token=" + apiToken);

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String json = http.getString();

      DeserializationError error = deserializeJson(doc, json);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return (String) "deserializeJson() failed: " + error.f_str();
      }

      int index = 0;
      String logDoor = "Berikut adalah daftar riwayat pintu tertutup & terbuka 10 terakhir.\n\n";
      logDoor += "No. |  Pintu Terbuka             |  Pintu Tertutup            | Akses |  Lama Waktu\n";
      logDoor += "-------------------------------------------------------------------------------\n";
      for (JsonObject door : doc["data"].as<JsonArray>()) {
        index++;
        const char* doorOpen = door["open"]; // "14-06-2022 21:20:00", "14-06-2022 20:00:00"
        const char* doorClosed = door["closed"]; // "14-06-2022 21:24:13", "14-06-2022 21:13:21"
        const char* doorInterval = door["interval"]; // "4 menit, 13 detik", "0 hari, ...
        const char* doorAccess = door["access"];

        logDoor += (String) + index + ".   | " + doorOpen + " | " + doorClosed + " |    " + doorAccess + "    | " +  doorInterval + "\n";
        logDoor += "-------------------------------------------------------------------------------\n";
      }

      return logDoor;
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      return (String) "Error code: " + httpResponseCode;
    }
    http.end();
  }
  else {
    Serial.println("WiFi Disconnected");
    return "WiFi Disconnected";
  }
}

String saveDoorLog(String tipe) {
  if ((WiFi.status() == WL_CONNECTED)) {

    WiFiClient client;
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    Serial.print("[HTTP] POST...\n");
    int httpCode = http.POST("{\"type\":\"" + tipe + "\",\"api_token\":\"" + apiToken + "\",\"access\":\"" + canAccessDoor + "\"}");

    // httpCode will be negative on error
    if (httpCode > 0) {
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      const String json = http.getString();
      Serial.println("received payload:\n<<");
      Serial.println(json);

      return json;
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());

      return "[HTTP] POST... failed, error: %s\n";
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    return "WiFi Disconnected";
  }
}
