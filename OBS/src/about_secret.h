#pragma once
class MAAboutSecret {
    bool armed=false;
public:
    bool click(const std::string &word) {
        if (word=="farin") {armed=true;return false;}
        const bool open=armed && word=="bela";
        armed=false;
        return open;
    }
    void reset(){armed=false;}
};
