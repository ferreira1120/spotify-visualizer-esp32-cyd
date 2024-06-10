#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <WiFiClient.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Web_Fetch.h>

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
  String artist;
  int numArtists;
  const char *albumName;
};

CurrentlyPlayingInfo currentInfo;
CurrentlyPlayingInfo newInfo;

// Network request task
TaskHandle_t updateInfoTask;

// NewInfo read/write mutex
SemaphoreHandle_t mutex;

// TFT/Sprites
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite titleSprite = TFT_eSprite(&tft);
TFT_eSprite artistSprite = TFT_eSprite(&tft);
TFT_eSprite albumSprite = TFT_eSprite(&tft);

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
  xSemaphoreTake(mutex, portMAX_DELAY);

  newInfo.albumArtUrl = strdup(currentlyPlaying.albumImages[1].url);
  newInfo.trackName = strdup(currentlyPlaying.trackName);
  newInfo.artist = "";
  for(int i = 0; i <= currentlyPlaying.numArtists - 1; i++) {
    newInfo.artist += currentlyPlaying.artists[i].artistName;
    if(i != currentlyPlaying.numArtists - 1) {
      newInfo.artist += ", ";
    }
  }
  newInfo.numArtists = currentlyPlaying.numArtists;
  newInfo.albumName = strdup(currentlyPlaying.albumName);

  xSemaphoreGive(mutex);
}

//Setups
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

  TJpgDec.setJpgScale(2);
  TJpgDec.setCallback(displayOutput);
  TJpgDec.setSwapBytes(true);

  titleSprite.setTextSize(2);
  titleSprite.fillSprite(TFT_BLACK);

  artistSprite.setTextSize(1);
  artistSprite.fillSprite(TFT_BLACK);

  albumSprite.setTextSize(1);
  albumSprite.fillSprite(TFT_BLACK);
}

void spiffsSetup() {
  // Initialise SPIFFS
  SPIFFS.begin(true);
  Serial.println("\r\nInitialisation done.");
}

void setup() {
  Serial.begin(115200);

  wifiConnect();

  tftSetup();

  spiffsSetup();

  mutex = xSemaphoreCreateMutex();
  if(mutex == NULL)
  {
    Serial.println("Mutex cannot be created");
  }

  xTaskCreatePinnedToCore(updateCurrentInfo, "updateInfoTask", 10000, NULL, tskIDLE_PRIORITY, &updateInfoTask, 0);
}

bool displayOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  // Stop further decoding as image is running off bottom of screen
  if (y >= tft.height())
    return 0;

  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}

void displayImage() {
  TJpgDec.drawFsJpg(tft.width() / 2  - 75, 0, "/albumArt.jpg");

  roundAlbumArtCorners();
}

void roundAlbumArtCorners() {
  int r = 4;
  int x = tft.width() / 2 - 75;
  int y = 0;
  int w = 150;
  int h = 150;

  uint32_t color = TFT_BLACK;

  for (int i = x; i <= x + r; i++) {
    for (int j = y; j <= y + r; j ++) {
      int xp = - (i - x) + r;
      int yp = - (j - y) + r;

      if ((xp * xp + yp * yp) > r * r) {
        tft.drawPixel(i, j, color);
      }
    }
  }

  for (int i = x + h - r - 1; i <= x + h-1; i++) {
    for (int j = y; j <= y + r; j ++) {
      int xp = i - (x + h - r-1);
      int yp = - (j - y) + r;

      if ((xp * xp + yp * yp) > r * r) {
        tft.drawPixel(i, j, color);
      }
    }
  }

  for (int i = x; i <= x + r; i++) {
    for (int j = y + w - r; j <= y + w - 1; j ++) {
      int xp = - (i - x) + r;
      int yp = j - (y + w - r-1);

      if ((xp * xp + yp * yp) > r * r) {
        tft.drawPixel(i, j, color);
      }
    }
  }

  for (int i = x + h - r; i <= x + h -1; i++) {
    for (int j = y + w - r ; j <= y + w-1; j ++) {
      int xp = i - (x + h - r -1);
      int yp = j - (y + w - r-1);

      if ((xp * xp + yp * yp) > r * r) {
        tft.drawPixel(i, j, color);
      }
    }
  }
}

int titlePixels = 0;

void displayTrackName(const char *trackName) {
  int x = 150;
  int y = 0;
  int margin = 8;

  if(titleSprite.created()) {
    titleSprite.deleteSprite();
  }

  titleSprite.createSprite(tft.width() + titleSprite.textWidth(trackName, 2), 32);
  Serial.print("Title Sprite Width: ");
  Serial.println(titleSprite.width());
  titleSprite.fillSprite(TFT_BLACK);
  titleSprite.drawString(trackName, max(0, tft.width() / 2 - titleSprite.textWidth(trackName, 2) / 2), 0, 2);
  titleSprite.pushSprite(y, x + margin);
  titlePixels = titleSprite.textWidth(trackName, 2) + 40 - tft.width();
}

int artistPixels = 0;

void displayArtistName(String artist, int numArtists) {
  int x = 182;
  int y = 0;
  int margin = 16;

  if(artistSprite.created()) {
    artistSprite.deleteSprite();
  }

  artistSprite.createSprite(tft.width() + artistSprite.textWidth(artist, 2), 16);
  Serial.print("Artist Sprite Width: ");
  Serial.println(artistSprite.width());
  artistSprite.fillSprite(TFT_BLACK);
  artistSprite.drawString(artist, max(0, tft.width() / 2 - artistSprite.textWidth(artist, 2) / 2), 0, 2);
  artistSprite.pushSprite(y, x + margin);
  artistPixels = artistSprite.textWidth(artist, 2) + 40 - tft.width();
}

void scrollSprites() {
  if (titleSprite.textWidth(currentInfo.trackName, 2) > tft.width()) {
    titleSprite.scroll(-1, 0);
    titleSprite.pushSprite(0, 158);
    titlePixels--;
    //Serial.println(pixels);
    if (titlePixels <= 0) {
      titleSprite.drawString(currentInfo.trackName, tft.width(), 0, 2);
      titlePixels = titleSprite.textWidth(currentInfo.trackName, 2) + 40;
    }
  }

  if (artistSprite.textWidth(currentInfo.artist, 2) > tft.width()) {
    artistSprite.scroll(-1, 0);
    artistSprite.pushSprite(0, 198);
    artistPixels--;
    //Serial.println(pixels);
    if (artistPixels <= 0) {
      artistSprite.drawString(currentInfo.artist, tft.width(), 0, 2);
      artistPixels = artistSprite.textWidth(currentInfo.artist, 2) + 40;
    }
  }
}

void loop() {
  //Serial.print("loop() running on core ");
  //Serial.println(xPortGetCoreID());

  xSemaphoreTake(mutex, portMAX_DELAY);

  if(newInfo.trackName != NULL) {
    // Compare album arts
    if (String(newInfo.albumArtUrl) != String(currentInfo.albumArtUrl)) {
      Serial.println("Updating Art");
      
      if (SPIFFS.exists("/albumArt.jpg") == true) {
        SPIFFS.remove("/albumArt.jpg");
      }

      bool loaded_ok = getFile(newInfo.albumArtUrl, "/albumArt.jpg");
    
      // Display album art      
      displayImage();
    }

    // Compare song titles
    if (String(newInfo.trackName) != String(currentInfo.trackName)) {
      displayTrackName(newInfo.trackName);
    }

    // Compare song artists
    if (newInfo.artist != currentInfo.artist) {
      displayArtistName(newInfo.artist, newInfo.numArtists);
    }

    currentInfo = newInfo;
  }

  xSemaphoreGive(mutex);

  if(currentInfo.trackName != NULL) {
    scrollSprites();
    delay(10);
  }
}
