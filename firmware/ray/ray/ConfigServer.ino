#include <ESP8266WebServer.h>
#include <FS.h>
#include "ConfigMap.h"
#include <Ticker.h>
#include <functional>

ConfigMap configData;

#define CONFIG_FILENAME "/config.tsv"

ESP8266WebServer* _server;

void handleIndex() {
  String form =
    R"(<html>
      <body>
       <h1>LED Dimmer</h1>
       <ul>
        <li> <a href="/config/connection/">connection config</a> </li>
        <li> <a href="/config/lamp/">lamp config</a> </li>
       </ul>
       </body>
      </html>)";

  _server->send(200, "text/html", form);

}

void setDefaultConfig();

void setDefaultConfig() {
  char esp_id[32];

  // create an unique ID for the AP SSID and MQTT ID
  sprintf(esp_id, "MICRODIMMER_%08x", ESP.getChipId());
  configData.set("wifi_sta_ap", "nonet");
  configData.set("wifi_sta_pass", "nonet");
  configData.set("mode", "lamp");
  configData.set("wifi_ap_ssid", esp_id);
  configData.set("wifi_ap_pass", "dimmer123456");
  configData.set("mqtt_server", "192.168.1.251");
  configData.set("mqtt_path", "/home/kitchen/lamp1");
  configData.set("mqtt_out_path", "/home/kitchen/lamp1/status");
  configData.set("mqtt_id", esp_id);

  // TODO(mangelajo): we could make this a bit modular
  setupDefaultConfigLight();

}

void configSetup() {
  SPIFFS.begin();
  setDefaultConfig();
  configData.readTSV(CONFIG_FILENAME);
}

void handleConfig() {
  String form =
    R"(<html>
      <head>
        <style>
          label { float:left; padding-left:2em; width: 12em; }
         </style>
      <body>
       <h1>Configuration</h1>
       <form action="/config/connetion/" method="POST">
        <h2>Dimmer mode</h2>
        <div><label>Mode</label>$mode_select</div>

        <h2>WiFi settings</h2>
        <div><label>WiFi ssid:</label><input name='wifi_sta_ap' value='$wifi_sta_ap'></div>
        <div><label>WiFi password:</label><input name='wifi_sta_pass' type='password' value='$wifi_sta_pass'></div>
        <h3>WiFi AP (fallback config)</h3>
        <div><label>AP ssid:</label><input name='wifi_ap_ssid' value='$wifi_ap_ssid'></div>
        <div><label>AP password:</label><input name='wifi_ap_pass' type='password' value='$wifi_ap_pass'></div>

        <h2>MQTT settings</h2>
        <div><label>MQTT Server:</label><input name='mqtt_server' value='$mqtt_server'></div>
        <div><label>MQTT (in) path:</label><input name='mqtt_path' value='$mqtt_path'></div>
        <div><label>MQTT Device ID:</label><input name='mqtt_id' value='$mqtt_id'></div>
        <div><label>MQTT (out) path:</label><input name='mqtt_out_path' value='$mqtt_out_path'></div>

          <input type="submit" value="Save and Reboot">
       </form>
         My WiFi STA IP: $IP
       </body>
      </html>)";

  String mode_select = "<select name='mode'>";
  char* mode = configData["mode"];
  mode_select += "<option value='lamp' ";
  if (strcmp(mode, "lamp") == 0)
    mode_select += "selected";
  mode_select += ">Lamp</option>";

  mode_select += "<option value='greenhouse'";
  if (strcmp(mode, "greenhouse") == 0)
    mode_select += "selected";
  mode_select += ">Greenhouse</option>";

  mode_select += "</select>";


  form.replace("$mode_select", mode_select);

  configData.replaceVars(form);

  form.replace("$IP", WiFi.localIP().toString());

  _server->send(200, "text/html", form);
}

// convert a single hex digit character to its integer value
unsigned char h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}
void urldecode(char *urlbuf)
{
    char c;
    char *dst;
    dst=urlbuf;
    while ((c = *urlbuf)) {
        if (c == '+') c = ' ';
        if (c == '%') {
            urlbuf++;
            c = *urlbuf;
            urlbuf++;
            c = (h2int(c) << 4) | h2int(*urlbuf);
        }
        *dst = c;
        dst++;
        urlbuf++;
    }
    *dst = '\0';
}


void handlePost(std::function<void()> render_form_function) {
  /* Grab every known config entry from the POST request arguments,
   * urldecode it, and set it back to the config object.
   */
  configData.foreach(
      [](const char* key, const char* value) {
        const char *_str = _server->arg(key).c_str();
        if (_str) {
          char *str = strdup(_str);
          urldecode(str);
          configData.set(key, str);
          free(str);
        }
      }
  );

  /* write a TSV file */
  configData.writeTSV(CONFIG_FILENAME);

  /* return the form again with the new data */
  render_form_function();

  /* allow for the data to be sent back to browser */
  for (int i=0;i<100;i++){
    yield();
    delay(10);
  }
  ESP.restart();
}

void handleConfigPost() {
  handlePost(handleConfig);
}

float getDimmerStartupVal(int dimmer) {
    char key[16];
    const char *val;
    sprintf(key, "startup_val_l%d", dimmer);
    val = configData[key];

    if (val && strlen(val)) return atoi(val)/100.0;
    else                    return 1.0;
}

void configServerSetup(ESP8266WebServer *server) {
  _server = server;
  configSetup();

  server->on("/", HTTP_GET, handleIndex);
  server->on("/config/connection/", HTTP_GET,  handleConfig);
  server->on("/config/connection/", HTTP_POST,  handleConfigPost);
  server->on("/config/lamp/", HTTP_GET,  handleConfigLamp);
  server->on("/config/lamp/", HTTP_POST,  handleConfigLampPost);

}

