#include <iostream>
#include "skiplist.h"

int main(){
    srand(time(0));
    SkipList S;
    S.put("hello", "world");
    S.put("hey", "there");
    S.put("I am", "Adityaa Saha");
    std::string str = S.get("I am").value_or("");
    std::cout << str << std::endl;
    S.remove("I am");
    str = S.get("I am").value_or("");
    std::cout << str << std::endl;
    str = S.get("hey").value_or("");
    std::cout << str << std::endl;
    return EXIT_SUCCESS;
}