#include <math.h>
#include <stdio.h>
#include "color.h"

void HSVtoRGB(float* fR, float* fG, float* fB, float fH, float fS, float fV) {
  float fC = fV * fS; // Chroma
  float fHPrime = fmod(fH / 60.0F, 6.0);
  float fX = fC * (1 - fabs(fmod(fHPrime, 2.0) - 1.0));
  float fM = fV - fC;
  
  if(0.0F <= fHPrime && fHPrime < 1.0F) {
    *fR = fC;
    *fG = fX;
    *fB = 0;
  } else if(1.0F <= fHPrime && fHPrime < 2.0F) {
    *fR = fX;
    *fG = fC;
    *fB = 0.0F;
  } else if(2.0F<= fHPrime && fHPrime < 3.0F) {
    *fR = 0.0F;
    *fG = fC;
    *fB = fX;
  } else if(3.0F <= fHPrime && fHPrime < 4.0F) {
    *fR = 0.0F;
    *fG = fX;
    *fB = fC;
  } else if(4.0F <= fHPrime && fHPrime < 5.0F) {
    *fR = fX;
    *fG = 0.0F;
    *fB = fC;
  } else if(5.0F <= fHPrime && fHPrime < 6.0F) {
    *fR = fC;
    *fG = 0.0F;
    *fB = fX;
  } else {
    *fR = 0.0F;
    *fG = 0.0F;
    *fB = 0.0F;
  }
  
  *fR += fM;
  *fG += fM;
  *fB += fM;
}


int color_convert_test() {
  float fR = 0.0F, fG = 0.0F, fB = 0.0F, fH = 0.0F, fS = 0.0F, fV = 0.0F;
  
  fH = 146.0F;
  fS = 0.19F;
  fV = 0.66F;
  
  HSVtoRGB(&fR, &fG, &fB, fH, fS, fV);

  for (int i=0; i<360; i+= 60) {
    fH = (float) i;
    fS = 1.0F;
    fV = 0.5F;
    HSVtoRGB(&fR, &fG, &fB, fH, fS, fV);
    int r = round(255.0F * fR);
    int g = round(255.0F * fG);
    int b = round(255.0F * fB);
    printf("[HSV] (%f, %f, %f)\n", fH, fS, fV);
    printf("[RGB] Float (%f, %f, %f)\n", fR, fG, fB);
    printf("      Integer: (%d, %d, %d)\n\n", r, g, b);
  }
  return 0;
}
