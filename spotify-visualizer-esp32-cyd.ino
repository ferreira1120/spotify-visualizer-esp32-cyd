#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <WiFiClient.h>
#include <NetworkClientSecure.h>

#include <SpotifyArduino.h>
#include <SpotifyArduinoCert.h>

// Wifi credentials
char ssid[] = "ssid"; // Replace with network ssid
char password[] = "password"; // Replace with network password

// Spotify credentials
char clientId[] = "clientId"; // Replace with spotify app clientId
char clientSecret[] = "clientSecret"; // Replace with spotify app clientPassword

// Spotify token
char spotifyMarket[] = "market"; // Replace with spotify market
char spotifyRefreshToken[] = "token"; // Replace with spotify account refreshToken

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

void setup() {
  Serial.begin(115200);

  wifiConnect();
}

void loop() {
}
