#include "wifi_control.h"

#include <Arduino.h>
#include <WiFiS3.h>
#include <string.h>

#include "calibration.h"
#include "color_data.h"
#include "command_input.h"
#include "config.h"
#include "detection.h"
#include "lcd_display.h"
#include "modes.h"
#include "pick_place.h"
#include "servo_arm.h"

#include "arduino_secrets.h"

static const unsigned long WIFI_RETRY_MS = 10000;
static const unsigned long WIFI_IP_TIMEOUT_MS = 15000;
static const unsigned long HTTP_TIMEOUT_MS = 800;

static WiFiServer httpServer(80);

static bool wifiEnabled = false;
static bool serverStarted = false;
static bool waitingForIpPrinted = false;
static unsigned long lastWifiAttemptMs = 0;
static unsigned long connectedSinceMs = 0;

static const char* modeName() {
  switch (mode) {
    case MODE_MENU: return "MENU";
    case MODE_CALIBRATION: return "CALIBRATION";
    case MODE_DETECTION: return "DETECTION";
  }

  return "UNKNOWN";
}

static void sendHeaders(
  WiFiClient& client,
  const char* status,
  const char* contentType
) {
  client.print("HTTP/1.1 ");
  client.println(status);
  client.print("Content-Type: ");
  client.println(contentType);
  client.println("Cache-Control: no-store");
  client.println("Connection: close");
  client.println();
}

static void sendNoContent(WiFiClient& client) {
  client.println("HTTP/1.1 204 No Content");
  client.println("Connection: close");
  client.println();
  client.flush();
  client.stop();
}

static void sendJsonOk(WiFiClient& client) {
  sendHeaders(client, "200 OK", "application/json");
  client.println("{\"ok\":true}");
  client.flush();
  client.stop();
}

static void sendJsonError(
  WiFiClient& client,
  const char* status,
  const char* error
) {
  sendHeaders(client, status, "application/json");
  client.print("{\"ok\":false,\"error\":\"");
  client.print(error);
  client.println("\"}");
  client.flush();
  client.stop();
}

static void sendText(
  WiFiClient& client,
  const char* status,
  const char* body
) {
  sendHeaders(client, status, "text/plain");
  client.println(body);
  client.flush();
  client.stop();
}

static void sendStatus(WiFiClient& client) {
  sendHeaders(client, "200 OK", "application/json");

  client.print("{\"mode\":\"");
  client.print(modeName());
  client.print("\",\"m2\":");
  client.print(servoAngleNow());
  client.print(",\"strips\":");
  client.print(stripsCalibrated);
  client.print(",\"displayed\":\"");
  client.print(colors[displayedColor].name);
  client.print("\",\"candidate\":\"");

  if (candidateColor >= 0 && candidateColor < NUM_COLORS) {
    client.print(colors[candidateColor].name);
  } else {
    client.print("none");
  }

  client.print("\",\"ldr\":");
  client.print(lastLdrReading, 2);
  client.print(",\"lift\":");
  client.print(liftAngleNow());
  client.print(",\"magnet\":");
  client.print(magnetIsOn() ? "true" : "false");
  client.println("}");
  client.flush();
  client.stop();
}

static int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;

  return -1;
}

// Decode %XX escapes. A '+' stays a plus, so the '+' jog command works.
static bool decodeToken(const char* input, char* output, size_t outputSize) {
  size_t length = 0;

  while (*input != '\0' && *input != '&') {
    char c = *input++;

    if (c == '%') {
      if (input[0] == '\0' || input[1] == '\0') return false;

      int high = hexValue(input[0]);
      int low = hexValue(input[1]);

      if (high < 0 || low < 0) return false;

      c = (char)((high << 4) | low);
      input += 2;
    }

    if (c <= ' ' || c == 127) return false;
    if (length + 1 > outputSize) return false;

    output[length++] = c;
  }

  output[length] = '\0';

  return length > 0;
}

static const char HTTP_PAGE[] = R"PAGE(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Color Sorter</title>
<style>
body{font-family:system-ui,sans-serif;margin:1rem;max-width:42rem}
h1{font-size:1.4rem}
h2{font-size:1rem;margin:1rem 0 .4rem}
button{margin:.15rem;padding:.5rem .7rem;font-size:1rem}
select{padding:.45rem;font-size:1rem}
input{padding:.45rem;font-size:1rem;width:5rem}
pre{background:#eee;padding:.6rem;border-radius:.4rem;white-space:pre-wrap}
</style>
</head>
<body>
<h1>Color Sorter</h1>
<pre id="status">loading...</pre>

<h2>Mode</h2>
<button onclick="cmd('s')">Legacy strip calibrate</button>
<button onclick="cmd('o')">Old values + detect</button>
<button onclick="cmd('c')">Menu</button>

<h2>White-patch RGB / assignment</h2>
<p>Manual: adjust the three pots, then press <b>Finish manual</b>. After the
base sweep, send a pickup color, then a drop color.</p>
<button onclick="cmd('man')">Manual RGB</button>
<button onclick="cmd('auto')">Auto RGB</button>
<button onclick="cmd('none')">Reuse RGB / start</button>
<button onclick="cmd('1')">Finish manual</button>
<button onclick="cmd('c')">Cancel prompt</button>
<p>Pickup color:</p>
<button onclick="cmd('w')">Pickup W</button>
<button onclick="cmd('g')">Pickup G</button>
<button onclick="cmd('r')">Pickup R</button>
<button onclick="cmd('b')">Pickup B</button>
<p>Drop color:</p>
<button onclick="cmd('w')">Drop W</button>
<button onclick="cmd('g')">Drop G</button>
<button onclick="cmd('r')">Drop R</button>
<button onclick="cmd('b')">Drop B</button>

<h2>Servo</h2>
<button onclick="cmd('a')">&lt; Left</button>
<button onclick="cmd('d')">Right &gt;</button>
<button onclick="cmd('k')">Limits</button>
<button onclick="cmd('v')">Scan</button>
<input id="angle" type="number" min="0" max="120" value="90">
<button onclick="cmd(document.getElementById('angle').value)">Go</button>

<h2>Lift / magnet</h2>
<button onclick="cmd('u')">Lift up (24&deg;)</button>
<button onclick="cmd('l')">Lift down (54&deg;)</button>
<button onclick="cmd('+')">Up +3&deg;</button>
<button onclick="cmd('-')">Down -3&deg;</button>
<button onclick="cmd('m')">Magnet toggle</button>
<button onclick="cmd('p')">Pick here, place at 90&deg;</button>

<h2>Pick color &rarr; place color</h2>
<label>Pick from
<select id="sourceColor">
<option value="w">White</option>
<option value="g">Green</option>
<option value="r">Red</option>
<option value="b">Blue</option>
</select>
</label>
<label>Place at
<select id="destColor">
<option value="w">White</option>
<option value="g">Green</option>
<option value="r" selected>Red</option>
<option value="b">Blue</option>
</select>
</label>
<button onclick="pickBetween()">Find and move</button>

<h2>Legacy four-strip calibration</h2>
<button onclick="cmd('w')">Cal W</button>
<button onclick="cmd('b')">Cal B</button>
<button onclick="cmd('g')">Cal G</button>
<button onclick="cmd('r')">Cal R</button>
<button onclick="cmd('x')">Results</button>
<button onclick="cmd('t')">Start detect</button>

<h2>Detection</h2>
<button onclick="cmd('w')">Seek W</button>
<button onclick="cmd('b')">Seek B</button>
<button onclick="cmd('g')">Seek G</button>
<button onclick="cmd('r')">Seek R</button>
<button onclick="cmd('x')">Values</button>

<h2>Any command</h2>
<input id="raw" type="text" placeholder="command">
<button onclick="cmd(document.getElementById('raw').value);document.getElementById('raw').value=''">Send</button>

<script>
function pickBetween() {
  const from = document.getElementById('sourceColor').value;
  const to = document.getElementById('destColor').value;
  cmd('pick' + from + to);
}
async function cmd(c) {
  if (!c) return;
  try {
    const r = await fetch('/cmd?c=' + encodeURIComponent(c), {cache: 'no-store'});
    const j = await r.json();
    if (!j.ok) alert(j.error || 'command rejected');
    setTimeout(refresh, 200);
  } catch (e) {
    alert('request failed');
  }
}
async function refresh() {
  try {
    const r = await fetch('/status', {cache: 'no-store'});
    const j = await r.json();
    document.getElementById('status').textContent =
      'Mode: ' + j.mode +
      '\nM2 servo: ' + j.m2 + ' deg' +
      '\nLift servo: ' + j.lift + ' deg' +
      '\nCalibrated strips: ' + j.strips +
      '\nLED: ' + j.displayed + ' | candidate: ' + j.candidate +
      '\nLDR: ' + j.ldr +
      '\nMagnet: ' + (j.magnet ? 'ON' : 'OFF');
  } catch (e) {
  }
}
refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)PAGE";

static void handleHttpClient() {
  WiFiClient client = httpServer.available();

  if (!client) return;

  char request[192];
  int length = 0;
  bool complete = false;
  unsigned long started = millis();

  while (!complete && (millis() - started) < HTTP_TIMEOUT_MS) {
    while (client.available() > 0) {
      char c = (char)client.read();

      if (c == '\n') {
        complete = true;
        break;
      }

      if (c != '\r' && length < (int)sizeof(request) - 1) {
        request[length++] = c;
      }
    }
  }

  request[length] = '\0';

  if (!complete || length == 0) {
    client.stop();
    return;
  }

  if (strncmp(request, "GET ", 4) != 0) {
    sendText(client, "405 Method Not Allowed", "GET only");
    return;
  }

  char* path = request + 4;
  char* space = strchr(path, ' ');

  if (space != NULL) *space = '\0';

  if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
    sendHeaders(client, "200 OK", "text/html; charset=utf-8");
    client.print(HTTP_PAGE);
    client.flush();
    client.stop();
    return;
  }

  if (strcmp(path, "/status") == 0) {
    sendStatus(client);
    return;
  }

  if (strncmp(path, "/cmd", 4) == 0) {
    if (path[4] != '?' || strncmp(path + 5, "c=", 2) != 0) {
      sendJsonError(client, "400 Bad Request", "missing c parameter");
      return;
    }

    char token[CMD_BUF_SIZE];

    if (!decodeToken(path + 7, token, sizeof(token))) {
      sendJsonError(client, "400 Bad Request", "invalid command");
      return;
    }

    if (!submitCommand(token)) {
      sendJsonError(client, "409 Conflict", "busy");
      return;
    }

    sendJsonOk(client);
    return;
  }

  if (strcmp(path, "/favicon.ico") == 0) {
    sendNoContent(client);
    return;
  }

  sendText(client, "404 Not Found", "not found");
}

void wifiControlBegin() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi: module not found; serial control only.");
    return;
  }

  if (SECRET_SSID[0] == '\0') {
    Serial.println("WiFi: set SECRET_SSID and SECRET_PASS in arduino_secrets.h.");
    return;
  }

  wifiEnabled = true;
  lastWifiAttemptMs = millis();

  Serial.println("WiFi: starting; the IP address will print when assigned.");
  Serial.print("WiFi: connecting to ");
  Serial.println(SECRET_SSID);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
}

void wifiControlPoll() {
  if (!wifiEnabled) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (serverStarted) {
      httpServer.end();
      serverStarted = false;
      Serial.println("WiFi: disconnected.");
    }

    waitingForIpPrinted = false;
    connectedSinceMs = 0;

    if (millis() - lastWifiAttemptMs >= WIFI_RETRY_MS) {
      lastWifiAttemptMs = millis();

      Serial.print("WiFi: retrying ");
      Serial.println(SECRET_SSID);

      WiFi.begin(SECRET_SSID, SECRET_PASS);
    }

    return;
  }

  IPAddress ip = WiFi.localIP();

  // The module can report WL_CONNECTED before DHCP has handed out an
  // address. Do not start the server on 0.0.0.0.
  if ((uint32_t)ip == 0) {
    if (serverStarted) {
      httpServer.end();
      serverStarted = false;
    }

    if (!waitingForIpPrinted) {
      waitingForIpPrinted = true;
      connectedSinceMs = millis();
      Serial.println("WiFi: connected, waiting for an IP address...");
    }

    if (millis() - connectedSinceMs >= WIFI_IP_TIMEOUT_MS) {
      Serial.println("WiFi: no IP address, reconnecting...");

      waitingForIpPrinted = false;
      connectedSinceMs = 0;
      lastWifiAttemptMs = millis();

      WiFi.disconnect();
      WiFi.begin(SECRET_SSID, SECRET_PASS);
    }

    return;
  }

  if (!serverStarted) {
    httpServer.begin();
    serverStarted = true;
    waitingForIpPrinted = false;

    Serial.println();
    Serial.println("=== WEB CONTROL READY ===");
    Serial.print("WiFi IP: ");
    Serial.println(ip);
    Serial.print("URL: http://");
    Serial.println(ip);
    Serial.println("========================");

    if (mode == MODE_MENU) {
      String ipText = ip.toString();
      lcdShowMessage("WEB UI", ipText.c_str());
    }
  }

  handleHttpClient();
}

void wifiServiceDelay(unsigned long ms) {
  unsigned long start = millis();

  while (millis() - start < ms) {
    wifiControlPoll();
    delay(5);
  }
}
