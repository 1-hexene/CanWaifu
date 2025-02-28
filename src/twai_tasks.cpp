#include <twai_tasks.h>

static unsigned twaiSendDate = 0;
static unsigned twaiSentCount = 0;
static unsigned twaiReceivedCount = 0;
static unsigned char twaiPayload[8] = {0x1,0x1,0x4,0x5,0x1,0x4,0x19,0x19};

template <typename T>
void print_twai_message(const T *_hardwareSerial, CanFrame *canFrame, bool direction_is_send)
{
#ifdef PRINT_CAN_MSGS
    if (direction_is_send)
    {
        _hardwareSerial->println("---------[TWAI][Send]----------");
        _hardwareSerial->print("Index = ");
        _hardwareSerial->println(twaiSentCount);
    }
    else
    {
        _hardwareSerial->println("--------[TWAI][Received]-------");
        _hardwareSerial->print("Index: ");
        _hardwareSerial->println(twaiReceivedCount);
    }

    _hardwareSerial->print("ID: ");
    _hardwareSerial->println(canFrame->identifier);

    _hardwareSerial->print("Length: ");
    _hardwareSerial->println(canFrame->data_length_code);

    _hardwareSerial->print("Extended Frame: ");
    _hardwareSerial->println(canFrame->extd ? "Yes" : "No");

    _hardwareSerial->print("Type: ");
    switch (canFrame->rtr)
    {
    case 1:
        _hardwareSerial->println("Can Remote frame");
        break;
    case 0:
        _hardwareSerial->println("Can Standard frame");
        break;
    default:
        _hardwareSerial->println("Error");
        break;
    }

    _hardwareSerial->print("Content: ");
    if (canFrame->data_length_code)
    {
        for (uint8_t i = 0; i < canFrame->data_length_code; i++)
        {
            if (canFrame->data[i] < 0x10)
            {
                _hardwareSerial->print("0");
            }
            _hardwareSerial->print(canFrame->data[i], HEX);
            _hardwareSerial->print(" ");
        }
        _hardwareSerial->println();
    }
    else
    {
        _hardwareSerial->println("Nothing.");
    }
#endif
}

void twai_init()
{
    pinMode(PIN_TWAI_STDBY, OUTPUT);
    digitalWrite(PIN_TWAI_STDBY, LOW);

    ESP32Can.setPins(PIN_TWAI_TX, PIN_TWAI_RX);
    ESP32Can.setRxQueueSize(10);
    ESP32Can.setTxQueueSize(10);
    ESP32Can.setSpeed(TWAI_SPEED_1000KBPS);

    if (ESP32Can.begin())
    {
        Serial.println("[TWAI] TWAI bus started!");
    }
    else
    {
        Serial.println("[TWAI] TWAI bus failed!");
    }
}

void twai_send_task(void *pvParameters)
{
    CanFrame message;
    message.identifier = TWAI_MSG_ID;
    message.extd = 0;
    message.data_length_code = 8;
    message.data[0] = 0x11;
    message.data[1] = 0x45;
    message.data[2] = 0x14;
    message.data[3] = 0x19;
    message.data[4] = 0x19;
    message.data[5] = 0x8;
    message.data[6] = 0x1;
    message.data[7] = 0x0;
    while (1)
    {
        if (ESP32Can.writeFrame(message))
        {
            print_twai_message(&Serial, &message, true);
            twaiSentCount += 1;
        }

        vTaskDelay((1000/TWAI_SEND_FREQ) / portTICK_PERIOD_MS);
    }
}

void twai_receive_task(void *pvParameters)
{
    CanFrame message;
    while (1)
    {
        if (ESP32Can.readFrame(message, 1000))
        {
            print_twai_message(&Serial, &message, false);
            twaiReceivedCount += 1;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}