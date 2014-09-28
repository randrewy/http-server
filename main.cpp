#include <iostream>
#include "server.h"

int main()
{
    return server::start(8080, 1);
}
