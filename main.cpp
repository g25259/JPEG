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

inline unsigned char Clamp(int i) {
    if (i < 0)
        return 0;
    else if (i > 255)
        return 255;
    else
        return i;
}


void DequantizeBlock(int block[64], const float quantBlock[64]) {
    for (int c = 0; c < 64; c++) {
        block[c] = (int) (block[c] * quantBlock[c]);
    }
}


void DeZigZag(int outBlock[64], const int inBlock[64]) {
    for (int i = 0; i < 64; i++) {
        outBlock[i] = inBlock[ZigZagArray[i]];
    }
}


void TransformArray(int outArray[8][8], const int inArray[64]) {
    int cc = 0;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            outArray[x][y] = inArray[cc];
            cc++;
        }
    }
}


float C(int u) {
    if (u == 0)
        return (1.0f / sqrtf(2));
    else
        return 1.0f;
}

int func(int x, int y, const int block[8][8]) {
    const float PI = 3.14f;
    float sum = 0;
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            sum += (C(u) * C(v)) * block[u][v] * cosf(((2 * x + 1) * u * PI) / 16) * cosf(((2 * y + 1) * v * PI) / 16);
        }
    }
    return (int) ((1.0 / 4.0) * sum);
}

void PerformIDCT(int outBlock[8][8], const int inBlock[8][8]) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            outBlock[x][y] = func(x, y, inBlock);
        }
    }
}

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
    jdec->m_width = X;
    jdec->m_height = Y;

    return 0;
}

inline int ParseDQT(stJpegData *jdec, const unsigned char* stream){
    int Lq = BYTE_TO_WORD(stream);  // DQT block length include length;
    float *quantizationTable;
    stream += 2;                       //skip length

    int Pq = *stream >> 4;
    int Tq = *stream & 0xf ;
    stream++;
    printf("Pq: %d  Tq: %d\n", Pq, Tq);

    quantizationTable = jdec->m_Q_tables[Tq];
    for(int i = 0; i < 64; i++){
        quantizationTable[i] = stream[i];
        //printf("%d\n", quantizationTable[i]);
    }


}


inline int GenerateHuffmanCodes(stHuffmanTable * huffmanTable){

    int i = 0, codeValue = 0;

    for(int k = 1; k <= 16; k++){
        for(int j = 1; j <= huffmanTable->m_length[k]; j++){
            huffmanTable->m_blocks[i].code = codeValue;
            huffmanTable->m_blocks[i].value = huffmanTable->m_hufVal[i];
            codeValue++;
            i++;

        }
        codeValue *= 2;

    }

}
inline int BuildHuffmanTable(stHuffmanTable *huffmanTable, const unsigned char* stream){
    int count = 0;

    for(int i = 1; i < 17; i++){
        huffmanTable->m_length[i] = *stream++;
        count+= huffmanTable->m_length[i];
    }

    for(int i = 0; i < count; i++){
        huffmanTable->m_hufVal[i] = *stream++;
    }
    huffmanTable->m_numBlocks = count;

    count = 0;
    for(int i = 0; i < 17; i++){
        for(int j = 0; j < huffmanTable->m_length[i]; j++){
            huffmanTable->m_blocks[count].length = i;
            count++;
        }
    }


    GenerateHuffmanCodes(huffmanTable);
}
inline int ParseDHT(stJpegData *jdec, const unsigned char* stream){
    int Lh = BYTE_TO_WORD(stream);
    stream += 2;                            //skip length
    int TableClassNIdentifier = *stream++;
    int Tc = TableClassNIdentifier >> 4;
    int Th = TableClassNIdentifier & 0xf;
    stHuffmanTable *huffmanTable;
    int count = 0;               //Number of Huffman codes

    // Build DC Huffman or AC Huffman
    printf("Length : %d Tc:%d  Th: %d",Lh, Tc,Th);

    if(Tc == 0){
        BuildHuffmanTable(&jdec->m_HTDC[Th], stream);
    }

    else{
        BuildHuffmanTable(&jdec->m_HTAC[Th], stream);
    }



}

inline int ParseSOS(stJpegData *jdec, const unsigned char* stream){
    int Ls = BYTE_TO_WORD(stream);
    stream += 2;            //skip length
    int Ns = *stream++;      //Number of image components in scan
    int Cs;
    int TableDCNACSelector;


    for(int i = 0; i < Ns; i++){
        Cs = *stream++;
        TableDCNACSelector = *stream++;
        jdec->m_component_info[Cs].m_dcTable = &jdec->m_HTDC[TableDCNACSelector >> 4];
        jdec->m_component_info[Cs].m_acTable = &jdec->m_HTAC[TableDCNACSelector & 0xf];

    }

    jdec->m_stream = stream+3;
    return 0;

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
    return parseJFIF(jdec, position);
    
}


bool IsInHuffmanCodes(int code, int numberOfCodeBits, int numberOfBlocks, stBlock *blocks, int *decodedValue) {
    for (int i = 0; i < numberOfBlocks; i++) {
        int huffmanCode = blocks[i].code;
        int huffmanCodeLength = blocks[i].length;
        int huffmanValue = blocks[i].value;
        
        if ((code == huffmanCode) && (numberOfCodeBits == huffmanCodeLength)) {
            *decodedValue = huffmanValue;
            return true;
        }
    }
    return false;
}


int DetermineSign(int val, int nBits) {
    bool negative = val < (1 << (nBits - 1));

    if (negative) {
        // (-1 << (s)), makes the last bit a 1, so we have 1000,0000 for example for 8 bits

        val = val + (-1 << (nBits)) + 1;
    }

    // Else its unsigned, just return
    return val;
}

int reservoirBits = 0;
int bitInReservoir = 0;

inline int FillKBits(const unsigned char **stream, int kBits){
    while(bitInReservoir < kBits){
        const unsigned char nextByte = *(*stream)++;
        reservoirBits <<= 8;
        if(nextByte == 0xff && (**stream) == 0x00)
            (*stream)++;
        reservoirBits |= nextByte;
        bitInReservoir += 8;
    }
}


inline int LookKBits(const unsigned char **stream, int kBits){
    FillKBits(stream, kBits);

    int result =((reservoirBits) >> (bitInReservoir - (kBits)));
    return  result;
}

inline void SkipKBits(const unsigned char **stream, int &nbits_wanted) {
    FillKBits(stream, nbits_wanted);

    bitInReservoir -= (nbits_wanted);
    reservoirBits &= ((1U << bitInReservoir) - 1);

}

inline short GetNBits(const unsigned char **stream, int nbits_wanted) {
    FillKBits(stream, nbits_wanted);

    short result = ((reservoirBits) >> (bitInReservoir - (nbits_wanted)));

    bitInReservoir -= (nbits_wanted);
    reservoirBits &= ((1U << bitInReservoir) - 1);

    return result;
}



void ProcessHuffmanDataUnit(stJpegData *jdata, int indx) {
    stComponent *component = &jdata->m_component_info[indx];


    short DCT_tcoeff[64];
    memset(DCT_tcoeff, 0, sizeof(DCT_tcoeff)); //Initialize DCT_tcoeff

    int decodedValue = 0;
    
    for (int k = 1; k < 16; k++) {


        
        int code = LookKBits(&jdata->m_stream, k);
        
        if (IsInHuffmanCodes(code, k, component->m_dcTable->m_numBlocks, component->m_dcTable->m_blocks, &decodedValue)) {
            // Skip over the rest of the bits now.
            SkipKBits(&jdata->m_stream, k);

            // The decoded value is the number of bits we have to read in next
            int numDataBits = decodedValue;

            // We know the next k bits are for the actual data
            if (numDataBits == 0) {
                DCT_tcoeff[0] = component->m_previousDC;
            }
            else {
                short data = GetNBits(&jdata->m_stream, numDataBits);

                data = DetermineSign(data, numDataBits);

                DCT_tcoeff[0] = data + component->m_previousDC;
                component->m_previousDC = DCT_tcoeff[0];
            }

            break;
        }
    }


    // Second, the 63 AC coefficient
    int filledNumber = 1;
    bool EOB_found = false;
    while ((filledNumber < 64) && (!EOB_found)) {
        int k = 0;
        for (k = 1; k <= 16; k++) {
           
            int code = LookKBits(&jdata->m_stream, k);


            // Check if its one of our huffman codes
            if (IsInHuffmanCodes(code, k, component->m_acTable->m_numBlocks, component->m_acTable->m_blocks, &decodedValue)) {

                // Skip over k bits, since we found the huffman value
                SkipKBits(&jdata->m_stream, k);


                // Our decoded value is broken down into 2 parts, repeating RLE, and then
                // the number of bits that make up the actual value next
                int valCode = decodedValue;

                unsigned char size_val = valCode & 0xF;    // Number of bits for our data
                unsigned char count_0 = valCode >> 4;    // Number RunLengthZeros

                if (size_val == 0) {// RLE
                    if (count_0 == 0)EOB_found = true;    // EOB found, go out
                    else if (count_0 == 0xF) filledNumber += 16;  // skip 16 zeros
                }
                else {
                    filledNumber += count_0; //skip count_0 zeroes

                    if (filledNumber > 63) {
                        printf("-|- ##ERROR## Huffman Decoding\n");
                    }

                    short data = GetNBits(&jdata->m_stream, size_val);

                    data = DetermineSign(data, size_val);

                    DCT_tcoeff[filledNumber++] = data;

                }
                break;
            }
        }

        if (k > 16) {
            filledNumber++;
        }
    }


    for (int j = 0; j < 64; j++) {
        component->m_DCT[j] = DCT_tcoeff[j];
    }

}





inline void ConvertYCrCbtoRGB(int y, int cb, int cr,
                              int *r, int *g, int *b) {
    float red, green, blue;

    red = y + 1.402f * (cb - 128);
    green = y - 0.34414f * (cr - 128) - 0.71414f * (cb - 128);
    blue = y + 1.772f * (cr - 128);

    *r = (int) Clamp((int) red);
    *g = (int) Clamp((int) green);
    *b = (int) Clamp((int) blue);
}



inline void YCrCB_to_RGB24_Block8x8(stJpegData *jdata, int w, int h, int imgx, int imgy, int imgw, int imgh) {
    const unsigned char *Y, *Cb, *Cr;
    unsigned char *pix;

    int r, g, b;

    Y = jdata->m_Y;
    Cb = jdata->m_Cb;
    Cr = jdata->m_Cr;

    int olw = 0; // overlap
    if (imgx > (imgw - 8 * w)) {
        olw = imgw - imgx;
    }

    int olh = 0; // overlap
    if (imgy > (imgh - 8 * h)) {
        olh = imgh - imgy;
    }

//	dprintf("***pix***\n\n");
    for (int y = 0; y < (8 * h - olh); y++) {
        for (int x = 0; x < (8 * w - olw); x++) {
            int poff = x * 3 + jdata->m_width * 3 * y;
            pix = &(jdata->m_colourspace[poff]);

            int yoff = x + y * (w * 8);
            int coff = (int) (x * (1.0f / w)) + (int) (y * (1.0f / h)) * 8;

            int yc = Y[yoff];
            int cb = Cb[coff];
            int cr = Cr[coff];

            ConvertYCrCbtoRGB(yc, cr, cb, &r, &g, &b);

            pix[0] = Clamp(r);
            pix[1] = Clamp(g);
            pix[2] = Clamp(b);


        }

    }

}
inline void WriteBMP24(const char *szBmpFileName, int Width, int Height, unsigned char *RGB) {
#pragma pack(1)
    struct stBMFH // BitmapFileHeader & BitmapInfoHeader
    {
        // BitmapFileHeader
        char bmtype[2];     // 2 bytes - 'B' 'M'
        unsigned int iFileSize;     // 4 bytes
        short int reserved1;     // 2 bytes
        short int reserved2;     // 2 bytes
        unsigned int iOffsetBits;   // 4 bytes
        // End of stBMFH structure - size of 14 bytes
        // BitmapInfoHeader
        unsigned int iSizeHeader;    // 4 bytes - 40
        unsigned int iWidth;         // 4 bytes
        unsigned int iHeight;        // 4 bytes
        short int iPlanes;        // 2 bytes
        short int iBitCount;      // 2 bytes
        unsigned int Compression;    // 4 bytes
        unsigned int iSizeImage;     // 4 bytes
        unsigned int iXPelsPerMeter; // 4 bytes
        unsigned int iYPelsPerMeter; // 4 bytes
        unsigned int iClrUsed;       // 4 bytes
        unsigned int iClrImportant;  // 4 bytes
        // End of stBMIF structure - size 40 bytes
        // Total size - 54 bytes
    };
#pragma pack()

    // Round up the width to the nearest DWORD boundary
    int iNumPaddedBytes = 4 - (Width * 3) % 4;
    iNumPaddedBytes = iNumPaddedBytes % 4;

    stBMFH bh;
    memset(&bh, 0, sizeof(bh));
    bh.bmtype[0] = 'B';
    bh.bmtype[1] = 'M';
    bh.iFileSize = (Width * Height * 3) + (Height * iNumPaddedBytes) + sizeof(bh);
    bh.iOffsetBits = sizeof(stBMFH);
    bh.iSizeHeader = 40;
    bh.iPlanes = 1;
    bh.iWidth = Width;
    bh.iHeight = Height;
    bh.iBitCount = 24;


    char temp[1024] = {0};
    sprintf(temp, "%s", szBmpFileName);
    FILE *fp = fopen(temp, "wb");
    fwrite(&bh, sizeof(bh), 1, fp);
    for (int y = Height - 1; y >= 0; y--) {
        for (int x = 0; x < Width; x++) {
            int i = (x + (Width) * y) * 3;
            unsigned int rgbpix = (RGB[i] << 16) | (RGB[i + 1] << 8) | (RGB[i + 2] << 0);
            fwrite(&rgbpix, 3, 1, fp);
        }
        if (iNumPaddedBytes > 0) {
            unsigned char pad = 0;
            fwrite(&pad, iNumPaddedBytes, 1, fp);
        }
    }
    fclose(fp);
}

inline void DecodeSingleBlock(stComponent *comp, unsigned char *outputBuf, int stride) {
    short *inptr = comp->m_DCT;
    float *quantptr = comp->m_qTable;


    // Create a temp 8x8, i.e. 64 array for the data
    int data[64] = {0};

    // Copy our data into the temp array
    for (int i = 0; i < 64; i++) {
        data[i] = inptr[i];
    }

    // De-Quantize
    DequantizeBlock(data, quantptr);

    // De-Zig-Zag
    int block[64] = {0};
    DeZigZag(block, data);

    // Create an 8x8 array
    int arrayBlock[8][8] = {0};
    TransformArray(arrayBlock, block);

    // Inverse DCT
    int val[8][8] = {0};
    PerformIDCT(val, arrayBlock);

    // Level Shift each element (i.e. add 128), and copy to our
    // output
    unsigned char *outptr = outputBuf;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            val[x][y] += 128;

            outptr[x] = Clamp(val[x][y]);
        }

        outptr += stride;
    }

}
inline void DecodeMCU(stJpegData *jdata, int w, int h) {
    // Y
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int stride = w * 8;
            int offset = x * 8 + y * 64 * w;

            ProcessHuffmanDataUnit(jdata, cY);

            DecodeSingleBlock(&jdata->m_component_info[cY], &jdata->m_Y[offset], stride);
        }
    }

    // Cb
    ProcessHuffmanDataUnit(jdata, cCb);
    DecodeSingleBlock(&jdata->m_component_info[cCb], jdata->m_Cb, 8);

    // Cr
    ProcessHuffmanDataUnit(jdata, cCr);
    DecodeSingleBlock(&jdata->m_component_info[cCr], jdata->m_Cr, 8);

}


int Decode(stJpegData *jdec){
    int horizontalFactor = jdec->m_component_info[cY].m_hFactor;
    int verticalFactor = jdec->m_component_info[cY].m_vFactor;

    // RGB24:
    if (jdec->m_rgb == NULL) {
        int h = jdec->m_height * 3;
        int w = jdec->m_width * 3;
        int height = h + (8 * horizontalFactor) - (h % (8 * horizontalFactor));
        int width = w + (8 * verticalFactor) - (w % (8 * verticalFactor));
        jdec->m_rgb = new unsigned char[width * height];

        memset(jdec->m_rgb, 0, width * height);
    }

    for(int i = 0; i < 4; i++){
        jdec->m_component_info[i].m_previousDC = 0;
    }


    int x_stride_by_mcu = 8 * horizontalFactor;
    int y_stride_by_mcu = 8 * verticalFactor;


    // Just the decode the image by 'macroblock' (size is 8x8, 8x16, or 16x16)
    for (int y = 0; y < (int) jdec->m_height; y += y_stride_by_mcu) {
        for (int x = 0; x < (int) jdec->m_width; x += x_stride_by_mcu) {
            jdec->m_colourspace = jdec->m_rgb + x * 3 + (y * jdec->m_width * 3);

            // Decode MCU Plane
            DecodeMCU(jdec, horizontalFactor, verticalFactor);

            YCrCB_to_RGB24_Block8x8(jdec, horizontalFactor, verticalFactor, x, y, jdec->m_width, jdec->m_height);
        }
    }


    return 0;


}
inline void JpegGetImageSize(stJpegData *jdata, unsigned int *width, unsigned int *height) {
    *width = jdata->m_width;
    *height = jdata->m_height;
}

int JpegDecoder(const unsigned char *buf, const unsigned int fileSize, unsigned char **rgbpix,
                unsigned int *width,        // Output image width
                unsigned int *height){
    stJpegData* jdec = new stJpegData();
    if(parseHeader(jdec, buf))
        return 0;
    Decode(jdec);

    JpegGetImageSize(jdec, width, height);

    *rgbpix = jdec->m_rgb;

    // Release the memory for our jpeg decoder structure jdec
    delete jdec;

    return 1;
}


int main() {

    FILE *imageInput;
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

    JpegDecoder(buffer, lengthOfJpegImage, &RGB, &width, &height);
    delete[] buffer;

    // Save it
    WriteBMP24("E:\\gig-sn01.bmp", width, height, RGB);

    // Since we don't need the pixel information anymore, we must
    // release this as well
    delete[] RGB;

    return 1;
}

