#include <controller.h>
void Controller::setInput(Button button) {
    this->input |= 1 << button;
}

void Controller::clearInput() {
    input = 0;
}

void Controller::setMode(Byte data) {
    strobe_mode = data & 0b1;
    if (strobe_mode) {
        input_index = 0;
    }
}

Controller::Byte Controller::readInput() {
    if (strobe_mode) {
        return input & 0b1;
    }
    else {
        if (input_index > 7) return 0b1;
        else return (input >> input_index++) & 0b1;
    }
}
