#include <webserver_tasks.h>

extern SemaphoreHandle_t canMsgMutex;

AsyncWebServer webServer(80);
String index_html_content;

void webserver_init()
{
    SPIFFS.begin(true);

    File file = SPIFFS.open("/index.html");
    index_html_content = file.readString();
    file.close();

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                 { request->send(200, "text/html", index_html_content); });

    webServer.on("/version", HTTP_GET, [](AsyncWebServerRequest *request)
                 { request->send(200, "text/html", SW_VER); });

    webServer.on("/data", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        xSemaphoreTake(canMsgMutex, portMAX_DELAY);
        for (int i = 0; i < 63; i++) {
            CanMsgWrapper msgWrapper = getCanMsgWrapperList()[i];
            if (msgWrapper.getFrequency() == 0) continue; // 跳过未使用的条目

            CANFDMessage msg = msgWrapper.getCanFdMsgContent();
            JsonObject obj = array.add<JsonObject>();
            obj["id"] = msg.id;
            obj["len"] = msg.len;

            switch (msg.type) {
                case CANFDMessage::CAN_DATA: obj["type"] = "标准帧"; break;
                case CANFDMessage::CAN_REMOTE: obj["type"] = "远程帧"; break;
                case CANFDMessage::Type::CANFD_NO_BIT_RATE_SWITCH: obj["type"] = "CANFD帧(固定比特率)"; break;
                case CANFDMessage::Type::CANFD_WITH_BIT_RATE_SWITCH: obj["type"] = "CANFD帧(可变比特率)"; break;
                default: obj["type"] = "UNKNOWN"; break;
            }

            //JsonArray data = obj.createNestedArray("data");
            JsonArray data = obj["data"].to<JsonArray>();
            for (int j = 0; j < msg.len; j++) {
                data.add(msg.data[j]);
            }
            obj["frequency"] = msgWrapper.getFrequency(); // 返回上一次清零前的频率
        }
        xSemaphoreGive(canMsgMutex);

        String json;
        serializeJson(array, json);
        request->send(200, "application/json", json); });

    webServer.begin();
}