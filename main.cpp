#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

#define HEADER_SIZE 20
typedef struct _JFIFHeader {
    unsigned char SOI[2];
    /* 00h  Start of Image Marker     */
    unsigned char APP0[2];
    /* 02h  Application Use Marker    */
    unsigned char Length[2];
    /* 04h  Length of APP0 Field      */
    unsigned char Identifier[5];
    /* 06h  "JFIF" (zero terminated) Id String */
    unsigned char Version[2];
    /* 07h  JFIF Format Revision      */
    unsigned char Units;
    /* 09h  Units used for Resolution */
    unsigned char Xdensity[2];
    /* 0Ah  Horizontal Resolution     */
    unsigned char Ydensity[2];
    /* 0Ch  Vertical Resolution       */
    unsigned char XThumbnail;
    /* 0Eh  Horizontal Pixel Count    */
    unsigned char YThumbnail;      /* 0Fh  Vertical Pixel Count      */

} JFIFHEAD;

void printJPEGHeader(JFIFHEAD);

void readQuantizationTable(unsigned char*);

FILE *imageInput;

int main() {


    FILE *imageOutput;
    unsigned char *quantizationTable = new unsigned char[100];
    JFIFHEAD jHead;
    if ((imageInput = fopen("C:\\Users\\g2525_000\\ClionProjects\\JPEG\\gig-sn01.jpg", "rb")) == NULL) {
        perror("");
    } else {

    }

    // fseek(imageInput, 0,  SEEK_SET);
    fread(&jHead, sizeof(JFIFHEAD), 1, imageInput);
    printJPEGHeader(jHead);
    readQuantizationTable(quantizationTable);
    return 0;
}

void printJPEGHeader(JFIFHEAD jfifhead) {
    for (int i = 0; i < HEADER_SIZE; i++) {
        if (i >= 0) {
            printf("%x", jfifhead.SOI[i]);
        }
        else if (i > 1) {
            printf("%x", jfifhead.APP0[i]);
        } else if (i > 3) {
            printf("%x", jfifhead.Length[i]);
        } else if (i > 5) {
            printf("%x", jfifhead.Identifier[i]);
        } else if (i > 10) {
            printf("%x", jfifhead.Version[i]);
        } else if (i > 12) {
            printf("%x", jfifhead.Units);
        } else if (i > 13) {
            printf("%x", jfifhead.Xdensity[i]);
        } else if (i > 15) {
            printf("%x", jfifhead.Ydensity[i]);
        } else if (i > 17) {
            printf("%x", jfifhead.XThumbnail);
        } else if (i > 18) {
            printf("%x", jfifhead.YThumbnail);
        }

    }
    printf("\n");
}

void readQuantizationTable(unsigned char *quantizationTable) {
    int size = 0;
    int over = 0;
    unsigned char buf[4];
    while (over != 2 ) {

        fread(buf, sizeof(unsigned char), 1, imageInput);
        size ++;
        printf("%x", buf[0]);
        if (buf[0] == 255 || over == 1) {

            if(buf[0] == 219 && over == 1){
                over = 2;
            }else if(over != 1) {
                over = 0;
            }
        }

    }
}