
void clearScreen()
{
    system("chcp 65001");
    printf("%s", "\x1b[0;0H\x1b[2J");
}
