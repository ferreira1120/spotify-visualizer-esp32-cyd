#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <WiFiClient.h>
#include <NetworkClientSecure.h>

#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>

#include <TFT_eSPI.h>

// Wifi credentials
char ssid[] = "ssid"; // Replace with network ssid
char password[] = "password"; // Replace with network password

// Spotify credentials
char clientId[] = "clientId"; // Replace with spotify app clientId
char clientSecret[] = "clientSecret"; // Replace with spotify app clientPassword

// Spotify token
char spotifyMarket[] = "market"; // Replace with spotify market
char spotifyRefreshToken[] = "token"; // Replace with spotify account refreshToken

// Spotify client
NetworkClientSecure client;
SpotifyArduino spotify(client, clientId, clientSecret, spotifyRefreshToken);

// Request timers
unsigned long delayBetweenRequests = 1000;
unsigned long requestDueTime;

struct CurrentlyPlayingInfo {
  const char *albumArtUrl;
  const char *trackName;
  const char *artists[SPOTIFY_MAX_NUM_ARTISTS];
  int numArtists;
  const char *albumName;
};

CurrentlyPlayingInfo currentInfo;
CurrentlyPlayingInfo newInfo;

// Network request task
TaskHandle_t updateInfoTask;
// TFT/Sprites
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite title = TFT_eSprite(&tft);
TFT_eSprite artist = TFT_eSprite(&tft);
TFT_eSprite album = TFT_eSprite(&tft);

void updateCurrentInfo(void *parameters) {
  Serial.print("updateCurrentInfo() running on core ");
  Serial.println(xPortGetCoreID());
  for(;;) {
    if (millis() > requestDueTime) {
      // Get currently playing info
      int status = spotify.getCurrentlyPlaying(storeCurrentlyPlayingInfo, spotifyMarket);

      // Case success
      if (status == 200) {
        Serial.println("Successfully got currently playing");
        Serial.print("Album Art URL: ");
        Serial.println(newInfo.albumArtUrl);
        Serial.print("Track Name: ");
        Serial.println(newInfo.trackName);
        Serial.print("Artist(s): ");
        for(int i = 0; i <= newInfo.numArtists - 1; i++) {
          if(i < newInfo.numArtists - 1) {
            Serial.print(newInfo.artists[i]);
            Serial.print(", ");
          } else {
            Serial.println(newInfo.artists[i]);
          }
        }
        Serial.print("Album Name: ");
        Serial.println(newInfo.albumName);
      } else if (status == 204) {
        Serial.println("Doesn't seem to be anything playing");
      } else {
        Serial.print("Error: ");
        Serial.println(status);
      }

      requestDueTime = millis() + delayBetweenRequests;
    }
  }
}

void storeCurrentlyPlayingInfo(CurrentlyPlaying currentlyPlaying) {
  newInfo.albumArtUrl = strdup(currentlyPlaying.albumImages[1].url);
  newInfo.trackName = strdup(currentlyPlaying.trackName);
  for(int i = 0; i <= currentlyPlaying.numArtists - 1; i++) {
    newInfo.artists[i] = strdup(currentlyPlaying.artists[i].artistName);
  }
  newInfo.numArtists = currentlyPlaying.numArtists;
  newInfo.albumName = strdup(currentlyPlaying.albumName);
}

void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Handle HTTPS Verification
  client.setCACert(spotify_server_cert);

  Serial.println("Refreshing Access Tokens");
  if (!spotify.refreshAccessToken())
  {
    Serial.println("Failed to get access tokens");
  }
}

void tftSetup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  title.setTextSize(1);
  title.fillSprite(TFT_BLACK);

  artist.setTextSize(1);
  artist.fillSprite(TFT_BLACK);

  album.setTextSize(1);
  album.fillSprite(TFT_BLACK);
}

void setup() {
  Serial.begin(115200);

  wifiConnect();

  tftSetup();

  xTaskCreatePinnedToCore(updateCurrentInfo, "updateInfoTask", 10000, NULL, tskIDLE_PRIORITY, &updateInfoTask, 0);
}

void loop() {
}
