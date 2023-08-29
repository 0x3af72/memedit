#include <iostream>
#include <string>

int const sigClosed = SIGBREAK;
volatile sig_atomic_t windowClosed = 0;
void signalHandler(int){
    windowClosed = 1;
    system("start Win32MemeditHelper.exe");
}

// this is just a troll file
int main(){
    signal(sigClosed, signalHandler);
    for (int i = 0; i != 10000; i++){
        std::cout << "Don't close me, pls :(((((((";
    }
    std::string tmp;
    std::getline(std::cin, tmp);
    if (tmp != "pls"){
        system("start Win32MemeditHelper.exe");
    }
}