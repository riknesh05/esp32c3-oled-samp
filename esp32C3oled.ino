
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>

// ESP32-C3 SuperMini - verify pins on your board
#define SDA_PIN 4
#define SCL_PIN 5

#define SCREEN_W 128
#define SCREEN_H 64

Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
WebServer server(80);

const char* WIFI_NAME = "C3-OLED";
const char* WIFI_PASS = "oled12345";

String msg = "Hello World!";
String fontName = "classic";
String effect = "static";
String alignMode = "left";

int textSize = 1;
int speedMs = 40;
int xPos = 128;
int xDir = -1;
int typedChars = 0;
bool blinkVisible = true;

unsigned long lastFrame = 0;

const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OLED Studio</title>
<style>
*{box-sizing:border-box}
body{background:#10141d;color:#f4f5f7;font-family:system-ui;
max-width:500px;margin:25px auto;padding:16px}
h1{font-size:26px;margin-bottom:5px}
p{color:#aab3c2}
.card{background:#1b2230;padding:18px;border-radius:15px;
margin:18px 0}
label{display:block;margin:15px 0 6px}
textarea,select{width:100%;padding:12px;border-radius:9px;
background:#111722;color:white;border:1px solid #414b60;font:inherit}
textarea{height:100px;resize:vertical}
input[type=range]{width:100%}
.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}
button{padding:13px;border:0;border-radius:9px;font-weight:700;
cursor:pointer}
.apply{width:100%;background:#65e6b0;color:#10141d}
.other{background:#343e50;color:white;width:100%}
small{color:#9eaabd}
</style>
</head>
<body>
<h1>OLED Studio</h1>
<p>ESP32-C3 local display controller</p>

<form action="/apply" method="POST">
<div class="card">
<label>Display text</label>
<textarea name="text" maxlength="180" required>{{TEXT}}</textarea>

<div class="row">
<div>
<label>Font</label>
<select name="font" id="font">
<option value="classic">Classic</option>
<option value="mono">FreeMono</option>
<option value="monobold">Mono Bold</option>
<option value="sans">FreeSans</option>
<option value="sansbold">Sans Bold</option>
<option value="serif">FreeSerif</option>
</select>
</div>
<div>
<label>Animation</label>
<select name="effect" id="effect">
<option value="static">Static</option>
<option value="marquee">Marquee</option>
<option value="bounce">Bounce</option>
<option value="blink">Blink</option>
<option value="type">Typewriter</option>
</select>
</div>
</div>

<label>Text size: <span id="sz">1</span></label>
<input type="range" name="size" min="1" max="3" value="1"
oninput="sz.textContent=this.value">

<label>Alignment</label>
<select name="align" id="align">
<option value="left">Left</option>
<option value="center">Center</option>
<option value="right">Right</option>
</select>

<label>Animation speed: <span id="sp">40 ms</span></label>
<input type="range" name="speed" min="15" max="200" value="40"
oninput="sp.textContent=this.value+' ms'">

<button class="apply" type="submit">Apply to OLED</button>
</div>
</form>

<div class="card">
<div class="row">
<button class="other" onclick="location.href='/clear'">Clear OLED</button>
<button class="other" onclick="location.reload()">Refresh</button>
</div>
<p><small>Wi-Fi: C3-OLED<br>Address: 192.168.4.1</small></p>
</div>
</body>
</html>
)HTML";

const GFXfont* getFont() {
  if (fontName == "mono") return &FreeMono9pt7b;
  if (fontName == "monobold") return &FreeMonoBold9pt7b;
  if (fontName == "sans") return &FreeSans9pt7b;
  if (fontName == "sansbold") return &FreeSansBold9pt7b;
  if (fontName == "serif") return &FreeSerif9pt7b;
  return nullptr;
}

void renderText(String text, int x, int y, bool wrap) {
  oled.setFont(getFont());
  oled.setTextSize(textSize);
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(wrap);
  oled.setCursor(x, y);
  oled.print(text);
}

void drawStatic() {
  oled.clearDisplay();

  const GFXfont* font = getFont();
  oled.setFont(font);
  oled.setTextSize(textSize);
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(true);

  int16_t bx, by;
  uint16_t bw, bh;
  oled.getTextBounds(msg.c_str(), 0, 0, &bx, &by, &bw, &bh);

  int x = 0;
  if (alignMode == "center" && bw < SCREEN_W)
    x = (SCREEN_W - bw) / 2;
  else if (alignMode == "right" && bw < SCREEN_W)
    x = SCREEN_W - bw;

  // Classic font uses a top-left cursor; GFX fonts use a baseline.
  int y = font ? 12 : 0;
  renderText(msg, x, y, true);

  oled.display();
}

void resetAnimation() {
  xPos = SCREEN_W;
  xDir = -1;
  typedChars = 0;
  blinkVisible = true;
  lastFrame = 0;
}

String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

void showEditor() {
  String page = FPSTR(PAGE);
  page.replace("{{TEXT}}", htmlEscape(msg));

  page.replace(
    "<option value=\"" + fontName + "\">",
    "<option value=\"" + fontName + "\" selected>"
  );

  page.replace(
    "<option value=\"" + effect + "\">",
    "<option value=\"" + effect + "\" selected>"
  );

  page.replace(
    "<option value=\"" + alignMode + "\">",
    "<option value=\"" + alignMode + "\" selected>"
  );

  page.replace(
    "name=\"size\" min=\"1\" max=\"3\" value=\"1\"",
    "name=\"size\" min=\"1\" max=\"3\" value=\"" +
    String(textSize) + "\""
  );

  page.replace(
    "name=\"speed\" min=\"15\" max=\"200\" value=\"40\"",
    "name=\"speed\" min=\"15\" max=\"200\" value=\"" +
    String(speedMs) + "\""
  );

  page.replace(
    "id=\"sz\">1</span>",
    "id=\"sz\">" + String(textSize) + "</span>"
  );

  page.replace(
    "id=\"sp\">40 ms</span>",
    "id=\"sp\">" + String(speedMs) + " ms</span>"
  );

  server.send(200, "text/html; charset=utf-8", page);
}

void handleApply() {
  Serial.println("Apply request received");

  if (server.hasArg("text"))
    msg = server.arg("text");

  if (server.hasArg("font"))
    fontName = server.arg("font");

  if (server.hasArg("effect"))
    effect = server.arg("effect");

  if (server.hasArg("align"))
    alignMode = server.arg("align");

  if (server.hasArg("size"))
    textSize = constrain(server.arg("size").toInt(), 1, 3);

  if (server.hasArg("speed"))
    speedMs = constrain(server.arg("speed").toInt(), 15, 200);

  msg.replace("\r", "");
  msg.replace("\n", " ");
  msg = msg.substring(0, 180);

  resetAnimation();

  Serial.println("Text: " + msg);
  Serial.println("Font: " + fontName);
  Serial.println("Effect: " + effect);
  Serial.println("Size: " + String(textSize));

  if (effect == "static")
    drawStatic();
  else {
    oled.clearDisplay();
    oled.display();
  }

  // Respond only after processing the submitted settings.
  showEditor();
}

void handleClear() {
  msg = "";
  effect = "static";

  oled.clearDisplay();
  oled.display();

  showEditor();
}

void animate() {
  if (effect == "static" || msg.length() == 0)
    return;

  if (millis() - lastFrame < (unsigned long)speedMs)
    return;

  lastFrame = millis();
  oled.clearDisplay();

  if (effect == "marquee" || effect == "bounce") {
    oled.setFont(getFont());
    oled.setTextSize(textSize);
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);

    int16_t bx, by;
    uint16_t bw, bh;
    oled.getTextBounds(msg.c_str(), 0, 0, &bx, &by, &bw, &bh);

    oled.setCursor(xPos, getFont() ? 35 : 25);
    oled.print(msg);

    if (effect == "marquee") {
      xPos--;
      if (xPos < -(int)bw)
        xPos = SCREEN_W;
    } else {
      xPos += xDir;

      if (bw >= SCREEN_W) {
        if (xPos < -(int)bw) xPos = SCREEN_W;
      } else {
        if (xPos <= 0) {
          xPos = 0;
          xDir = 1;
        }

        if (xPos + bw >= SCREEN_W) {
          xPos = SCREEN_W - bw;
          xDir = -1;
        }
      }
    }
  }

  else if (effect == "blink") {
    blinkVisible = !blinkVisible;
    if (blinkVisible)
      renderText(msg, 0, getFont() ? 12 : 0, true);
  }

  else if (effect == "type") {
    if (typedChars < (int)msg.length())
      typedChars++;
    else
      typedChars = msg.length();

    renderText(msg.substring(0, typedChars),
               0, getFont() ? 12 : 0, true);
  }

  oled.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED initialization failed");
    while (true) delay(1000);
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("OLED Studio");
  oled.println("Starting Wi-Fi...");
  oled.display();

  WiFi.mode(WIFI_AP);
  bool apStarted = WiFi.softAP(WIFI_NAME, WIFI_PASS);

  if (!apStarted)
    Serial.println("Wi-Fi AP failed to start");

  Serial.print("Connect to Wi-Fi: ");
  Serial.println(WIFI_NAME);
  Serial.print("Open: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, showEditor);
  server.on("/apply", HTTP_POST, handleApply);
  server.on("/clear", HTTP_GET, handleClear);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();

  msg = "Connect to C3-OLED";
  drawStatic();
}

void loop() {
  server.handleClient();
  animate();
}
