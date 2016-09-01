#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define READ_BUFFER_SIZE            16384
#define BMP_HEADER_BM_FIELD_SIZE        2
#define BMP_HEADER_UNUSED_FILEDS_SIZE   8
#define BMP_HEADER_PA_OFFSET_FIELD_SIZE 4
#define BMP_HEADER_SIZE                14
#define DIB_HEADER_WIDTH_OFFSET      0x12
#define DIB_HEADER_HEIGHT_OFFSET     0x16
#define DIB_HEADER_WIDTH_SIZE           2
#define DIB_HEADER_HEIGHT_SIZE          2
#define GBLUR_CORE_SIZE                 7
#define GAUSS_K                      1024

#define ERROR_OPENING_INPUT_FILE  0x0001
#define ERROR_OPENING_OUTPUT_FILE 0x0002
#define INVALID_FILE_TYPE         0x0003
#define WRONG_ARGUMENT_NUMBER     0x0004

typedef struct{
  char b; /* blue  */
  char g; /* red   */
  char r; /* green */
}pixel;

/* Converting byte representation of integers. */
int bytesToInt(const unsigned char* bytes, unsigned int bytesNum);

/* Gauss blur. */
int gBlur (const char*  inputFilepath,
           const char*  outputFilepath,
           unsigned int radius);

/* Generating Gauss blur filter. */
float* gBlurFilter (int coreSize);

int main (int argc, char* argv[])
{
  if(argc != 3){
    printf("Wrong argument number!\n");
    exit(WRONG_ARGUMENT_NUMBER);
  }
  gBlur (argv[1], argv[2], GBLUR_CORE_SIZE);

  return 0;
}

float* gBlurFilter (int coreSize){
  srand(time(NULL));
  float* filter = malloc(sizeof(float) * coreSize);

  /* Acquiring Gauss distribution, using CLT. */
  int i;
  for(i = 0; i <= floor(coreSize / 2.0); i++){
    filter[i] = 0;
    int j;
    for(j = 0; j < GAUSS_K; j++){
      filter[i] += (rand() % 100) / 100.0;
    }
  }

  /* Bubble sort.*/
  int sortCondition = 1;
  float t;
  for(i = 0; i <= floor(coreSize / 2.0) && sortCondition; i++){
    sortCondition = 0;
    int j;
    for(j = 0; j < floor(coreSize / 2.0); j++){
      if(filter[j] > filter[j + 1]){
        sortCondition = 1;
        t = filter[j];
        filter[j] = filter[j + 1];
        filter[j+1] = t;
      }
    }
  }

  /* Creating filter. */
  for(i = floor(coreSize / 2.0) + 1; i < coreSize; i++){
    filter[i] = filter[coreSize - i - 1];
  }

  /*  Normalizing filter. */
  int summ = 0;
  for(i = 0; i < coreSize; i++){
    summ += filter[i];
  }
  for(i = 0; i < coreSize; i++){
    filter[i] /= summ;
  }
 
  return filter;
}

int gBlur (const char*  inputFilepath,
           const char*  outputFilepath,
           unsigned int radius)
{

  int input = open(inputFilepath, O_RDONLY);
  if(input <= 0){
    printf("Error opening input file!\n");
    return ERROR_OPENING_INPUT_FILE;
  }
  int output = open(outputFilepath, O_WRONLY | O_CREAT,
                                    S_IRUSR | S_IWUSR | S_IXUSR);
  if(input <= 0){
    printf("Error opening output file!\n");
    return ERROR_OPENING_OUTPUT_FILE;
  }

  unsigned char readBuffer[READ_BUFFER_SIZE];

  read(input, readBuffer, BMP_HEADER_BM_FIELD_SIZE);
  if(readBuffer[0] != 'B' || readBuffer[1] != 'M'){
    printf("Invalid file type!\n");
    return INVALID_FILE_TYPE;
  }

  /* Skipping unused field while reading BMP-file. */
  read(input, readBuffer, BMP_HEADER_UNUSED_FILEDS_SIZE);
  read(input, readBuffer, BMP_HEADER_PA_OFFSET_FIELD_SIZE);
  /* Locating pixel array data and acquring image resolution.*/
  int pixelArrayOffset = bytesToInt(readBuffer, 4);
  lseek(input, DIB_HEADER_WIDTH_OFFSET, SEEK_SET);
  read(input, readBuffer, DIB_HEADER_WIDTH_SIZE);
  int width = bytesToInt(readBuffer, DIB_HEADER_WIDTH_SIZE);
  lseek(input, DIB_HEADER_HEIGHT_OFFSET, SEEK_SET);
  read(input, readBuffer, DIB_HEADER_HEIGHT_SIZE);
  int height = bytesToInt(readBuffer, DIB_HEADER_HEIGHT_SIZE);
  lseek(input, 0, SEEK_SET);
  read(input, readBuffer, pixelArrayOffset);
  write(output, readBuffer, pixelArrayOffset);  

  /* Image have to be extended a little bit in order to perform filtering
   * without the ‘frame of unblurness’ and stuff.
   */
  int imageExpansion = floor(radius / 2.0);
  pixel** pixelArray = malloc(sizeof(pixel*) * (height + 2*imageExpansion));
  int i;  
  for(i = 0; i < width + 2*imageExpansion; i++){
    pixelArray[i] = malloc(sizeof(pixel) * (width + 2*imageExpansion));
  }

  /* Filling with black just in case. */
  for(i = 0; i < height + 2*imageExpansion; i++){
    int j;
    for(j = 0; j < width + 2*imageExpansion; j++){
      pixelArray[i][j].r = 0;
      pixelArray[i][j].g = 0;
      pixelArray[i][j].b = 0;
    }
  }


  /* Acquiring pixel array data. */
  for(i = imageExpansion; i < height + imageExpansion; i++){
    int j;
    for(j = imageExpansion; j < width + imageExpansion; j++){
      read(input, readBuffer, 3);
      pixelArray[i][j].r = (char)(readBuffer[2]);
      pixelArray[i][j].g = (char)(readBuffer[1]);
      pixelArray[i][j].b = (char)(readBuffer[0]);
    }
    if((width*3) % 4 != 0){ /* padding */
      read(input, readBuffer, 4 - ((width * 3) % 4));
    }
  }

  /* Extending image (filling cell with neighbour colours). */
  for(i = 0; i < imageExpansion; i++){
    int j;
    for(j = imageExpansion - i; j < width + imageExpansion; j++){
      pixelArray[imageExpansion - 1 - i][j].r = 
          pixelArray[imageExpansion - i][j].r;

      pixelArray[imageExpansion - 1 - i][j].g =
          pixelArray[imageExpansion - i][j].g;

      pixelArray[imageExpansion - 1 - i][j].b =
          pixelArray[imageExpansion - i][j].b;

         pixelArray[height + imageExpansion + i][j].r =
      pixelArray[height + imageExpansion + i -1][j].r;

         pixelArray[height + imageExpansion + i][j].g =
      pixelArray[height + imageExpansion + i -1][j].g;

         pixelArray[height + imageExpansion + i][j].b =
      pixelArray[height + imageExpansion + i -1][j].b;
    }
    for(j = imageExpansion - i; j < height + imageExpansion; j++){
      pixelArray[j][imageExpansion - 1 - i].r =
      pixelArray[j][imageExpansion - i].r;

      pixelArray[j][imageExpansion - 1 - i].g =
      pixelArray[j][imageExpansion - i].g;

      pixelArray[j][imageExpansion - 1 - i].b =
      pixelArray[j][imageExpansion - i].b;

      pixelArray[j][width + imageExpansion + i].r =
      pixelArray[j][width + imageExpansion + i - 1].r;

      pixelArray[j][width + imageExpansion + i].g =
      pixelArray[j][width + imageExpansion + i - 1].g;

      pixelArray[j][width + imageExpansion + i].b =
      pixelArray[j][width + imageExpansion + i - 1].b;
    }
  }

  /* Due to some transformation properties don’t have to use real matrix. This
   * way is both easier and faster.
   */
  float* filter = gBlurFilter(radius);
  for(i = imageExpansion; i < height + imageExpansion; i++){
    int j;
    for(j = imageExpansion; j < width + imageExpansion; j++){
      float summR = 0;
      float summG = 0;
      float summB = 0;
      int k;
      for(k = -3; k <= imageExpansion; k++){
        summR += (unsigned char)(pixelArray[i + k][j].r) * 
                 filter[k + imageExpansion];

        summG += (unsigned char)(pixelArray[i + k][j].g) *
                 filter[k + imageExpansion];

        summB += (unsigned char)(pixelArray[i + k][j].b) *
                 filter[k + imageExpansion];
      }
      pixelArray[i][j].r = floor(summR-1)+1;
      pixelArray[i][j].g = floor(summG-1)+1;
      pixelArray[i][j].b = floor(summB-1)+1;
    }
  }
  for(i =  imageExpansion; i < height + imageExpansion; i++){
    int j;
    for(j = imageExpansion; j < width + imageExpansion; j++){
      float summR = 0;
      float summG = 0;
      float summB = 0;
      int k;
      for(k = -3; k <= imageExpansion; k++){
        summR += (unsigned char)(pixelArray[i][j+k].r) *
                 filter[k + imageExpansion];

        summG += (unsigned char)(pixelArray[i][j+k].g) *
                 filter[k + imageExpansion];

        summB += (unsigned char)(pixelArray[i][j+k].b) *
                 filter[k + imageExpansion];
      }
      pixelArray[i][j].r = floor(summR-1)+1;
      pixelArray[i][j].g = floor(summG-1)+1;
      pixelArray[i][j].b = floor(summB-1)+1;
    }
  }

  /* Just writing into output files and cleaning. */
  for(i = imageExpansion; i < height + imageExpansion; i++){
    write(output, pixelArray[i] + imageExpansion, 3 * width);
    if((width*3) % 4 != 0){
      char padding[5] = {(char)0, (char)0, (char)0, (char)0, '\0'};
     write(output, padding, (4 -((width * 3) % 4)));
    }
  }

  free(filter);
  /* Can’t free pixelArray for some reason. */
  /*for(i = imageExpansion; i < height + 2*imageExpansion; i++){
    free(pixelArray[i]);
  }
  free(pixelArray);*/

  close(input);
  close(output);

  return 0;
}

int bytesToInt(const unsigned char* bytes, unsigned int bytesNum)
{
  int i;
  int j = 1;
  int result = 0;

  for(i = 0; i < bytesNum; i++){
    result += j*bytes[i];
    j *= 256;
  }

  return result;
}
