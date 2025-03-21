#include <can_fd_tasks.h>

static unsigned gSendDate = 0;
static unsigned gSentCount = 0;
static unsigned gReceivedCount = 0;

static SPIClass SPI2(FSPI);
static ACAN2517FD can2(MCP2518FD_CHIP_SELECT, SPI2, PIN_MCP2518_INT);
static CanMsgWrapper canMsgWrapperList[63];

SemaphoreHandle_t canMsgMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t canMsgReceive = xSemaphoreCreateBinary();


CanMsgWrapper *getCanMsgWrapperList()
{
    return canMsgWrapperList;
}

// 每秒清零 frequency 的任务
void can_fd_reset_frequency_task(void *pvParameters)
{
    while (1)
    {
        xSemaphoreTake(canMsgMutex, portMAX_DELAY);
        for (int i = 0; i < 63; i++)
        {
            canMsgWrapperList[i].resetCount();
        }
        xSemaphoreGive(canMsgMutex);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // 每秒执行一次
    }
}

template <typename T>
void print_can_fd_message(const T _hardwareSerial, CANFDMessage *canFdMessage, bool direction_is_send)
{
#ifdef PRINT_CAN_MSGS
    if (direction_is_send)
    {
        _hardwareSerial->println("---------[MCP2518FD][Send]----------");
        _hardwareSerial->print("Index = ");
        _hardwareSerial->println(gSentCount);
    }
    else
    {
        _hardwareSerial->println("--------[MCP2518FD][Received]-------");
        _hardwareSerial->print("Index: ");
        _hardwareSerial->println(gReceivedCount);
    }

    _hardwareSerial->print("ID: ");
    _hardwareSerial->println(canFdMessage->id);

    _hardwareSerial->print("Length: ");
    _hardwareSerial->println(canFdMessage->len);

    _hardwareSerial->print("Extended Frame: ");
    _hardwareSerial->println(canFdMessage->ext ? "Yes" : "No");

    _hardwareSerial->print("Type: ");
    switch (canFdMessage->type)
    {
    case CANFDMessage::Type::CAN_DATA:
        _hardwareSerial->println("Can data frame");
        break;
    case CANFDMessage::Type::CAN_REMOTE:
        _hardwareSerial->println("Can remote frame");
        break;
    case CANFDMessage::Type::CANFD_NO_BIT_RATE_SWITCH:
        _hardwareSerial->println("Can frame without bitrate switch");
        break;
    case CANFDMessage::Type::CANFD_WITH_BIT_RATE_SWITCH:
        _hardwareSerial->println("Can frame with bitrate switch");
        break;
    default:
        _hardwareSerial->println("Error");
        break;
    }

    _hardwareSerial->print("Content: ");
    if (canFdMessage->len)
    {
        for (uint8_t i = 0; i < canFdMessage->len; i++)
        {
            if (canFdMessage->data[i] < 0x10)
            {
                _hardwareSerial->print("0");
            }
            _hardwareSerial->print(canFdMessage->data[i], HEX);
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

CANFDMessage getCanFdMsgFromList(uint8_t msgIndex)
{
    return (canMsgWrapperList + msgIndex)->getCanFdMsgContent();
}

/*
 * @brief 无参数的初始化canfd函数 以标准can模式、1MBit/s、40MHz二分频启动。
 */
void can_fd_init()
{
    can_fd_init(7, 1 * 1000 * 1000, 1, 6);
}

/*
 * @brief 初始化canfd函数
 * @param oscFreq 晶振设置。oscFreq=6: 40MHz; oscFreq=7: 40MHz二分频。不要设置为其他值。
 * @param arbitrationBitRate 仲裁（标准）部分比特率。例如设为1*1000*1000就是1MBit/s。
 * @param dataBitRatefactor 数据比特率乘数。是仲裁部分比特率和数据部分比特率的比值。例如设为8就是8MBit/s。需要注意的是，如果工作在标准can模式下，此项要设为1。
 * @param mode MCP2518FD的工作模式。0: FD模式; 6: 标准Can模式; 3: 仅监听模式。不要设置为其他值。
 */
void can_fd_init(uint8_t oscFreq, uint32_t arbitrationBitRate, uint8_t dataBitRatefactor, uint8_t mode)
{
    pinMode(PIN_CANFD_STATUS, OUTPUT);
    digitalWrite(PIN_CANFD_STATUS, 0);

    SPI2.begin(SOFT_SCK_PIN, SOFT_MISO_PIN, SOFT_MOSI_PIN);
    SPI2.setFrequency(20000000);

    /* ACAN2517FDSettings settings(
        ACAN2517FDSettings::OSC_40MHz,
        1000 * 1000, // DesiredArbitrationBitRate
        DataBitRateFactor::x8); */

    ACAN2517FDSettings settings(
        ACAN2517FDSettings::Oscillator(oscFreq),
        arbitrationBitRate, // DesiredArbitrationBitRate
        DataBitRateFactor(dataBitRatefactor));
    settings.mRequestedMode = ACAN2517FDSettings::OperationMode(mode);

    const uint32_t errorCode = can2.begin(settings, can_fd_ISR);

    if (!errorCode)
    {
        Serial.println("[MCP2518FD][Init] OK.");
    }
    else
    {
        Serial.println(errorCode);
        digitalWrite(PIN_CANFD_STATUS, 1);
    }
}

void can_fd_send_task(void *pvParameters)
{
    CANFDMessage message;
    while (1)
    {
        message.id = CAN_FD_MSG_ID;
        const bool ok = can2.tryToSend(message);
        if (ok)
        {
            gSentCount += 1;
            print_can_fd_message(&Serial, &message, true);
        }
        vTaskDelay((1000 / CAN_FD_SEND_FREQ) / portTICK_PERIOD_MS);
    }
}

void can_fd_receive_task(void *pvParameters)
{
    CANFDMessage message;
    while (true)
    {
        if (xSemaphoreTake(canMsgReceive, portMAX_DELAY) == pdTRUE)
        {
            xSemaphoreTake(canMsgMutex, portMAX_DELAY);
            can2.receive(message);
            int index = -1;
            for (int i = 0; i < 63; i++)
            {
                if (canMsgWrapperList[i].getCanFdMsgContent().id == message.id)
                {
                    //Serial.println("[FDCAN] Overwriting existing message.");
                    index = i;
                    break;
                }

                //如果这一条消息的频率为0，而且一秒前也是0，那就覆盖它
                if (!(canMsgWrapperList[i].getCurrentFrequency() | canMsgWrapperList[i].getFrequency()))
                {
                    index = i;
                    Serial.println("[FDCAN] Detected a new message. ");
                    break;
                }
            }

            if (index == -1) // 如果列表已满，覆盖第一个
            { 
                index = 0; 
            }

            canMsgWrapperList[index].updateMessage(message);
            canMsgWrapperList[index].countPlusOne();

            xSemaphoreGive(canMsgMutex);
        }
    }
}

void IRAM_ATTR can_fd_ISR()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    //Serial.println("[MCP2518FD][Int] Received interrupt");
    can2.isr(); // 库的中断处理
    xSemaphoreGiveFromISR(canMsgReceive, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
      }
}
