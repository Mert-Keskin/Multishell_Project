/*
Hazırlayan : Mert Keskin 
No: 21120205001
Kullanılan kaynaklar : c kütüphanelerini anlatan internet siteleri ve ders notları.chatgpt
Xterm ve execlp nasıl kullanacagımı chatgpt sayesinde ögrendim.
benim bilgisayarda programı derlerken -lrt eki kullanınca çalışıyor 'gcc multishell.c -o multishell -lrt'
programın özeti:'./multishell numara' girilen numara sayısı kadar xterm ile singleshell çalıştırır.Tüm sheller kapanınca /sharedlogfile daki data txt dosyasına yazılır.
2 programda sorunsuz bir şekilde bilgisayarımda çalışmaktadır.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#define INBUF_SIZE 256

/* 
*/
#define MY_FILE_SIZE 1024
#define MY_SHARED_FILE_NAME "/sharedlogfile"

char *addr = NULL;
int fd = -1;

int initmem()
{
    fd = shm_open(MY_SHARED_FILE_NAME, O_RDWR | O_CREAT , 0777);//0777 yaparak dosya izni artırdım.O_TRUNC flagı sorun çıkardıgı için kaldırdım.
    if (fd < 0) {
        perror("singleshell.c:fd:line31");
        exit(1);
    }

    if (ftruncate(fd, MY_FILE_SIZE) == -1) {
        perror("singleshell.c:ftruncate");
        close(fd);
        exit(1);
    }

    addr = mmap(NULL, MY_FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("singleshell.c:mmap:");
        close(fd);
        exit(1);
    }
    return 0;
}


int main(int argc, char *argv[])
{
    #define shell_say atoi(argv[1])
    if (argc != 2) {
        printf("arguman yanlis girdiniz şu şekilde çalisitirin './multishell numara'\n");
        exit(1);
    }
    char inbuf[INBUF_SIZE] = {'\0'};
    int nbyte; /* input byte count */

    // xterm için döngü ve fork çağrıları yaptım.
    pid_t terminal_arr[shell_say];
    for (int i = 0; i < shell_say; i++) {
    terminal_arr[i] = fork();
    if (terminal_arr[i] == 0) {
        char terminal_num[2];
        sprintf(terminal_num, "%d", i+1);
        execlp("xterm", "xterm", "-e", "./singleshell", terminal_num, NULL);
        perror("execlp");
        exit(1);
    } else if (terminal_arr[i] < 0) {
        perror("fork");
        exit(1);
    }
}

    // terminallerin kapanmasını bekliyor.
    for (int i = 0; i < shell_say; i++) {
        waitpid(terminal_arr[i], NULL, 0);
    }

    initmem();

    //txt dosyasının ismi hazırlanır.
    time_t tm; 
    time(&tm);
    char dosya_adi[256];
    sprintf(dosya_adi, "shelllog-");
    strncat(dosya_adi,ctime(&tm),strlen(ctime(&tm))-1);
    strcat(dosya_adi,".txt");

    int output_fd = open(dosya_adi, O_RDWR | O_CREAT | O_TRUNC, 0666);//txt dosya açılır.
    if (output_fd < 0) {
        perror("open");
        exit(1);
    }
    // /sharedlogfile dosyasındaki data burada addr sayesinde txt dosyasına yazılır.
    int byte_yazilan = write(output_fd, addr, strlen(addr));
    if (byte_yazilan == -1) {
        perror("write");
        exit(1);
    } 
    
    close(output_fd);
    // Unmap the shared memory
    munmap(addr, 1024);
    // Close the shared memory file     
    close(fd);

}