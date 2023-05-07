/*
Hazırlayan : Mert Keskin 
No: 21120205001
Kullanılan kaynaklar : c kütüphanelerini anlatan internet siteleri ve ders notları.
benim bilgisayarda programı derlerken -lrt eki kullanınca çalışıyor 'gcc singleshell.c -o singleshell -lrt'
programın özeti:program çalışınca process id parent id ve tarih saat dosyaya yazılır.
ardında girilen herbir komut dosyaya yazılır hemen altına ise process id si yazılır.komut çalıştırılır dongu devam eder.exit verilince tarih ve saat dosyaya yazılır.
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

/* these should be the same as multishell.c */
#define MY_FILE_SIZE 1024
#define MY_SHARED_FILE_NAME "/sharedlogfile"

char *addr = NULL;
int fd = -1;

int initmem()
{   
    /*
    0777 yaparak dosya izni artırılmıştır./sharedlogfile dosyasının default locationı benim bilgisayarda /dev/shm klasorudur./sharedlogfile ın orada olması gerekir.
    bu yüzden O_CREAT flagı ekledim boylelikle dosya orada olmasa bile çalışması saglanabilir.İsterseniz flagı kaldırıp manuel olarak paylastıgım /sharedlogfile
    dosyasını dogru konuma koyarakta çalıştırabilirsiniz. Size kalmış.Fakat /sharedlogfile dosyasının boş oldugundan emin olun yoksa datalar çakışabilir !!!
    */
    fd = shm_open(MY_SHARED_FILE_NAME, O_RDWR|O_CREAT ,0777); 
    if (fd < 0){
        perror("singleshell.c:fd:line31");
        exit(1);
    }
    addr = mmap(NULL, MY_FILE_SIZE,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == NULL){
        perror("singleshell.c:mmap:");
        close(fd);
        exit(1);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    initmem();

    //anlık tarih ve saati dosyaya yazdırır.
    time_t tm;  
    time(&tm);
    char stringim[256];  //process idleri ve tarihi bu arrayde bir araya getirecegim
    sprintf(stringim, "process id: %d - parent id: %d\n", getpid(), getppid());
    strcat(stringim,ctime(&tm));

    off_t lseek1 =lseek(fd, 0, SEEK_END);   //multishell çalıştıgında datanın çakışmaması için lseek yaptım.
    if (lseek1 == -1) {  
        perror("lseek");
        exit(1);
    }

    int yazilan_byte = write(fd, stringim, strlen(stringim));
    if (yazilan_byte == -1) {
        perror("write");
        exit(1);
    } 

    char inbuf[INBUF_SIZE] = {'\0'};
    int nbyte; /* input byte sayısı */
    
    while (1) {
        int yazilan_byte4 = write(1, "$", 2);;
            if ((yazilan_byte4 == -1)) {
                perror("write");
                exit(1);
            } 

        if ((nbyte = read(0, inbuf, 255)) <= 0) {
            perror("input <=0 byte");
        } 
        else {
            inbuf[nbyte - 1] = '\0';
        }
        
        if (strncmp(inbuf, "exit", 4) == 0) {
            off_t lseek3 = lseek(fd, 0, SEEK_END);   //dosyanın sonuna ekleme yapmak için lseek kullandım.
            if (lseek3 == -1) {  
                perror("lseek");
                exit(1);
            }

            int yazilan_byte5 = write(fd,ctime(&tm),strlen(ctime(&tm))); //exit yapıldıktan sonra tarihi ve saati yazdırır.
            if ((yazilan_byte5 == -1)) {
                perror("write");
                munmap(addr, 1024); // Unmap the shared memory
                close(fd);          // Close the shared memory file
                exit(1);
            }
            
            munmap(addr, 1024);     // Unmap the shared memory
            close(fd);              // Close the shared memory file
            exit(0);
        }

        pid_t child_pid = fork();

        if (child_pid == 0) {
            off_t lseek2 = lseek(fd, 0, SEEK_END);   //dosyanın sonuna ekleme yapmak için lseek kullandım.
            if (lseek2 == -1) {  
                perror("lseek");
                exit(1);
            }

            // terminale girdigimiz komutu dosyaya burada yazıyorum.hata kontrolleride yapılıyor.
            int yazilan_byte = write(fd, inbuf, strlen(inbuf));
            int yazilan_byte2 = write(fd,"\n",1);
            if ((yazilan_byte == -1)|(yazilan_byte2 == -1)) {
                perror("write");
                exit(1);
            } 
            else if (yazilan_byte < strlen(inbuf)) {
                fprintf(stderr, "komutun tamami yazilmadi.\n");
            }            
            
            char stringim2[256];
            sprintf(stringim2, "my process id %d\n",getpid());

            int yazilan_byte3 = write(fd, stringim2, strlen(stringim2));//her bir terminal komutunda sonra process id dosyaya burada yazılır.
            if ((yazilan_byte3 == -1)) {
                perror("write");
                exit(1);
            } 

            int r2 = 0; //bunu daha sonra execvp() nin çalışıp çalışmama durumunu kontrol için kullanacagım.

            // Komutu çalıştır
            int r = execl(inbuf, inbuf, NULL);
            if(r==-1){
                // eger girdigimiz komutta bosluk karakteri varsa execl komutu çalıştıramayacaktır bu yüzden tokenlara ayırıp execvp() fonkunda çalıştıracagım.
                char *ptr = strtok(inbuf, " ");
                char *komut[INBUF_SIZE];
                int i = 0;
                while (ptr) {
                    komut[i] = ptr;
                    ptr = strtok(NULL, " ");
                    i++;
                }
                komut[i] = NULL;

                // girdigimiz komut cd ise execvp fonksiyonu komutu çalıştıramayacaktır.bu yüzden cd komutu için ayrı bir blok hazırladım.
                if(!strcmp(komut[0],"cd")){
                    if (chdir(inbuf+3) != 0) { //inbuf +3 yazmamın sebebi eger komut 'cd klasoradı' ise buradali 'c' ,'d' ,' ' karakterlerini atlayıp sadece klasoradını kullanmak
                    perror("chdir error");
                    }
                    else{
                        printf("konumunuz %s taşinmiştir\n",inbuf+3);
                    }
                }
                else{
                    // komutumuz cd degil ve execl bu komutu çalıştıramadıysa en sonunda tokenlarına ayırdıgımız komutu burda çalıştırmayı deniyecek.
                    r2 = execvp(komut[0], komut); 
                }
            }
            

            if ((r == -1) & (r2==-1)) {  // en son eger hiçbiri çalışmadıysa bu bloga giridik.
                char command[255] = {'/', 'b', 'i', 'n', '/', '\0'};
                strncat(command, inbuf, 250);
                r = execl(command, inbuf, NULL);
                if (r == -1)
                    perror("execl");
            }
        } 
        else if (child_pid > 0) {
            // her dongude $  işareti ekrana yazılırken bazenleri hata oluyordu.else if blogu $ işareti sorununu çözdü.
            waitpid(child_pid, NULL, 0);
        } 
        else {
            perror("fork() hata");
        }
    }

    // Unmap the shared memory
    munmap(addr, 1024);
    // Close the shared memory file
    close(fd);
}