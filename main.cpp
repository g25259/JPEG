#include <iostream>
#include <stdio.h>
#include <string.h>
#include <math.h>
using namespace std;

#define HEADER_SIZE 20

#define DQT      0xDB    // Define Quantization Table
#define SOF      0xC0    // Start of Frame (size information)
#define DHT      0xC4    // Huffman Table
#define SOI      0xD8    // Start of Image
#define SOS      0xDA    // Start of Scan
#define EOI      0xD9    // End of Image, or End of File
#define APP0     0xE0

#define BYTE_TO_WORD(x) (((x)[0]<<8)|(x)[1])


#define HUFFMAN_TABLES        4
#define COMPONENTS            4

#define cY    1
#define cCb    2
#define cCr    3


struct stBlock
{
    int value;                    // Decodes to.
    int length;                // Length in bits.
    unsigned short int code;    // 2 byte code (variable length)
};

/***************************************************************************/


struct stHuffmanTable
{
    unsigned char    m_length[17];        // 17 values from jpg file,
    // k =1-16 ; L[k] indicates the number of Huffman codes of length k
    unsigned char    m_hufVal[257];        // 256 codes read in from the jpeg file

    int                m_numBlocks;
    stBlock            m_blocks[1024];
};


struct stComponent
{
    unsigned int            m_hFactor;
    unsigned int            m_vFactor;
    float *                m_qTable;            // Pointer to the quantisation table to use
    stHuffmanTable*        m_acTable;
    stHuffmanTable*        m_dcTable;
    short int                m_DCT[65];            // DCT coef
    int                    m_previousDC;
};


struct stJpegData
{
    unsigned char*        m_rgb;                // Final Red Green Blue pixel data
    unsigned int        m_width;            // Width of image
    unsigned int        m_height;            // Height of image

    const unsigned char*m_stream;            // Pointer to the current stream

    stComponent            m_component_info[COMPONENTS];

    float                m_Q_tables[COMPONENTS][64];    // quantization tables
    stHuffmanTable        m_HTDC[HUFFMAN_TABLES];        // DC huffman tables
    stHuffmanTable        m_HTAC[HUFFMAN_TABLES];        // AC huffman tables

    // Temp space used after the IDCT to store each components
    unsigned char        m_Y[64*4];
    unsigned char        m_Cr[64];
    unsigned char        m_Cb[64];

    // Internal Pointer use for colorspace conversion, do not modify it !!!
    unsigned char *        m_colourspace;
};

static int ZigZagArray[64] =
        {
                0,   1,   5,  6,   14,  15,  27,  28,
                2,   4,   7,  13,  16,  26,  29,  42,
                3,   8,  12,  17,  25,  30,  41,  43,
                9,   11, 18,  24,  31,  40,  44,  53,
                10,  19, 23,  32,  39,  45,  52,  54,
                20,  22, 33,  38,  46,  51,  55,  60,
                21,  34, 37,  47,  50,  56,  59,  61,
                35,  36, 48,  49,  57,  58,  62,  63,
        };



inline int FileSize(FILE *fp)
{
    long pos;
    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return pos;
}

inline int ParseSOF(stJpegData *jdec, const unsigned char* stream){
    int Lf = BYTE_TO_WORD(stream);  //Frame Header length
    stream += 2;                    //Skip length

    int P = *stream++;

    int Y = BYTE_TO_WORD(stream);
    stream += 2;

    int X = BYTE_TO_WORD(stream);
    stream += 2;

    int Nf = *stream++;

    for(int i = 0; i < Nf; i++){
        stComponent *temp = &jdec->m_component_info[*stream++];
        temp->m_hFactor = *stream >> 4;
        temp->m_vFactor = *stream & 0xf;
        stream++;            //step over
        temp->m_qTable = jdec->m_Q_tables[*stream++];
    }
    jdec->m_width = Y;
    jdec->m_height = X;

    return 0;
}

inline int ParseDQT(stJpegData *jdec, const unsigned char* stream){
    int Lq = BYTE_TO_WORD(stream);  // DQT block length include length;
    float *quantizationTable;
    stream += 2;                       //skip length

    int Pq = *stream >> 4;
    int Tq = *stream & 0xf ;
    stream++;
    //printf("Pq: %d  Tq: %d\n", Pq, Tq);

    quantizationTable = jdec->m_Q_tables[Tq];
    for(int i = 0; i < 64; i++){
        quantizationTable[i] = stream[i];
    }


}

inline int ParseSOS(stJpegData *jdec, const unsigned char* stream){


}
inline int ParseDHT(stJpegData *jdec, const unsigned char* stream){
    int Lh = BYTE_TO_WORD(stream);
    int TableClassNIdentifier = stream[0];
    int Tc = TableClassNIdentifier >> 4;
    int Th = TableClassNIdentifier & 0xf;
    stHuffmanTable *huffmanTable;
    int count = 0;               //Number of Huffman codes

    // Build DC Huffman or AC Huffman
    if(Tc == 0)
        huffmanTable = &jdec->m_HTDC[Th];
    else if(Tc == 1)
        huffmanTable = &jdec->m_HTAC[Th];
    for(int i = 1; i < 17; i++){
        huffmanTable->m_length[i] = *stream++;
        count+= huffmanTable->m_length[i];
    }
    for(int i = 0; i < count; i++)
        huffmanTable->m_hufVal[i] = *stream++;





}
inline int parseJFIF(stJpegData *jdec, const unsigned char * stream){
    int chuck_len;
    int marker;
    int sos_marker_found = 0;
    int dht_marker_found = 0;

    // Parse marker
    while (!sos_marker_found)
    {
        if (*stream++ != 0xff)
        {
            goto bogus_jpeg_format;
        }

        // Skip any padding ff byte (this is normal)
        while (*stream == 0xff)
        {
            stream++;
        }

        marker = *stream++;
        chuck_len = BYTE_TO_WORD(stream);

        switch (marker)
        {
            case SOF:
            {
                if (ParseSOF(jdec, stream) < 0)
                    return -1;
            }
                break;

            case DQT:
            {
                if (ParseDQT(jdec, stream) < 0)
                    return -1;
            }
                break;

            case SOS:
            {
                if (ParseSOS(jdec, stream) < 0)
                    return -1;
                sos_marker_found = 1;
            }
                break;

            case DHT:
            {
                if (ParseDHT(jdec, stream) < 0)
                    return -1;
                dht_marker_found = 1;
            }
                break;

                // The reason I added these additional skips here, is because for
                // certain jpg compressions, like swf, it splits the encoding
                // and image data with SOI & EOI extra tags, so we need to skip
                // over them here and decode the whole image
            case SOI:
            case EOI:
            {
                chuck_len = 0;
                break;
            }
                break;

            case 0xDD: //DRI: Restart_markers=1;
            {
                printf("DRI - Restart_marker\n");
            }
                break;


            default:
            {
                printf("Skip marker %2.2x\n", marker);
            }
                break;
        }

        stream += chuck_len;
    }

    if (!dht_marker_found)
    {
        printf("ERROR> No Huffman table loaded\n");
    }

    return 0;

    bogus_jpeg_format:
    printf("ERROR> Bogus jpeg format\n");
    return -1;
}

inline int parseHeader(stJpegData *jdec, const unsigned char *buf){
    if((buf[0]!= 0xFF) || (buf[1] != SOI)){
        printf("Not a JPEG file\n");
        return -1;
    }
    printf("It should a JPEG file\n");

    const unsigned  char* position = buf + 2;
    parseJFIF(jdec, position);

}
int JpegDecoder(const unsigned char *buf, const unsigned int fileSize){
    stJpegData* jdec = new stJpegData();
    parseHeader(jdec, buf);

}
int test(const unsigned char * stream){
    stream += 2;
    printf("Test:%x\n", stream);
}
int main() {

    FILE *imageInput;
    FILE *imageOutput;
    unsigned  int lengthOfJpegImage;
    unsigned char *buffer;

    if ((imageInput = fopen("C:\\Users\\g2525_000\\ClionProjects\\JPEG\\gig-sn01.jpg", "rb")) == NULL) {
        perror("");
    }
    lengthOfJpegImage = FileSize(imageInput);
    buffer = new unsigned char[lengthOfJpegImage + 4];



    fread(buffer, lengthOfJpegImage, 1, imageInput);
    fclose(imageInput);

    unsigned char* RGB = NULL;
    unsigned int width = 0;
    unsigned int height = 0;

    JpegDecoder(buffer, lengthOfJpegImage);


    return 0;
}

