#include "system.h"
#include "ncurses_display.h"

int main() {
  System system;
  NCursesDisplay::Display(system);
}