#include <can_msg_wrapper.h>

CANFDMessage CanMsgWrapper::getCanFdMsgContent() {
    return canFdMsg;
}

uint16_t CanMsgWrapper::resetCount() {
    lastFrequency = frequency; // 清零前保存当前频率
    frequency = 0;
    return frequency;
}

uint16_t CanMsgWrapper::getCurrentFrequency() {
    return frequency; // 返回上一次清零前的频率
}

uint16_t CanMsgWrapper::getFrequency() {
    return lastFrequency; // 返回上一次清零前的频率
}

uint16_t CanMsgWrapper::countPlusOne() {
    frequency++;
    return frequency;
}

void CanMsgWrapper::updateMessage(const CANFDMessage &msg) {
    canFdMsg = msg; // 更新报文内容
}