#ifndef _CONTROLLER_H
#define _CONTROLLER_H
#include <cstdint>
class Controller {
public:
    using Byte = uint8_t;
    enum Button {
        A,
        B,
        Select,
        Start,
        Up,
        Down,
        Left,
        Right
    };
    void setInput(Button button);
    void clearInput();
    void setMode(Byte data);
    Byte readInput();
private:
    Byte input_index = 0;
    Byte input;
    bool strobe_mode = false;
};
#endif
