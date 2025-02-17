//===================================================================
//  COLORRAMPS.H
//
//-- Define the colors used in term of RGB( RED, GREEN, BLUE )
//-- The 16 pure colors available on the VGA under Windows 3.0
//===================================================================


#pragma once
#include <COLORS.HPP>




inline
RGBA WRColorRamp(float vmin, float vmax, float m) //, unsigned char& r, unsigned char& g, unsigned char& b)
   {
   const int ncolors_WR = 27;
   int WR_ColorRamp[ncolors_WR][3] = {
      { 250, 250, 250 }, // #FAFAFA
      { 249, 240, 240 }, // #F9F0F0
      { 248, 230, 230 }, // #F8E6E6
      { 247, 221, 221 }, // #F7DDDD
      { 246, 211, 211 }, // #F6D3D3
      { 246, 201, 201 }, // #F6C9C9
      { 245, 192, 192 }, // #F5C0C0
      { 244, 182, 182 }, // #F4B6B6
      { 243, 173, 173 }, // #F3ADAD
      { 243, 163, 163 }, // #F3A3A3
      { 242, 153, 153 }, // #F29999
      { 241, 144, 144 }, // #F19090
      { 240, 134, 134 }, // #F08686
      { 240, 125, 125 }, // #F07D7D
      { 239, 115, 115 }, // #EF7373
      { 238, 105, 105 }, // #EE6969
      { 237, 96, 96 }, // #ED6060
      { 236, 86, 86 }, // #EC5656
      { 236, 76, 76 }, // #EC4C4C
      { 235, 67, 67 }, // #EB4343
      { 234, 57, 57 }, // #EA3939
      { 233, 48, 48 }, // #E93030
      { 233, 38, 38 }, // #E92626
      { 232, 28, 28 }, // #E81C1C
      { 231, 19, 19 }, // #E71313
      { 230, 9, 9 },   //#E60909
      { 230, 0, 0 } };  //#E60000

   int index = int(ncolors_WR * (m - vmin) / (vmax - vmin));
   if (index >= ncolors_WR)
      index = ncolors_WR - 1;

   int r = WR_ColorRamp[index][0];
   int g = WR_ColorRamp[index][1];
   int b = WR_ColorRamp[index][2];

   return RGBA(r, g, b, 1);
   }








inline
RGBA RColorRamp(float vmin, float vmax, float m ) //, unsigned char& r, unsigned char& g, unsigned char& b)
   {
   const int ncolors = 13;

   int index = int(ncolors * (m - vmin) / (vmax - vmin));
   if (index >= ncolors)
      index = ncolors - 1;

   int colorRamp[ncolors][3] =
      { { 255, 21, 216 },
        { 255, 19, 199 },
        { 255, 18, 183 },
        { 255, 16, 166 },
        { 255, 14, 149 },
        { 255, 13, 132 },
        { 255, 11, 115 },
        { 255,  98,   98 },
        { 255,  82,   82 },
        { 255,  65,   65 },
        { 255,  48,   48 },
        { 255,  31,   31 },
        { 255, 25, 25 } };

   int r = colorRamp[index][0];
   int g = colorRamp[index][1];
   int b = colorRamp[index][2];

   return RGBA(r,g,b,1);
   }





inline
void RWBColorRamp(float vmin, float vmax, float m, unsigned char& r, unsigned char& g, unsigned char& b)
   {
   const int ncolors = 252;

   int index = int(ncolors * (m-vmin) / (vmax - vmin));
   if (index >= ncolors)
      index = ncolors - 1;

   int rwbColorRamp[ncolors][3] =
      { { 25, 82, 255 },
        { 27, 84, 255 },
        { 28, 85, 255 },
        { 30, 86, 255 },
        { 31, 87, 255 },
        { 33, 88, 255 },
        { 34, 89, 255 },
        { 36, 90, 255 },
        { 37, 92, 255 },
        { 39, 93, 255 },
        { 40, 94, 255 },
        { 42, 95, 255 },
        { 43, 96, 255 },
        { 45, 97, 255 },
        { 46, 98, 255 },
        { 48, 10, 255 },
        { 49, 10, 255 },
        { 51, 10, 255 },
        { 53, 10, 255 },
        { 54, 10, 255 },
        { 56, 10, 255 },
        { 57, 10, 255 },
        { 59, 10, 255 },
        { 60, 10, 255 },
        { 62, 11, 255 },
        { 63, 11, 255 },
        { 65, 11, 255 },
        { 66, 11, 255 },
        { 68, 11, 255 },
        { 69, 11, 255 },
        { 71, 11, 255 },
        { 72, 11, 255 },
        { 74, 11, 255 },
        { 75, 12, 255 },
        { 77, 12, 255 },
        { 79, 12, 255 },
        { 80, 12, 255 },
        { 82, 12, 255 },
        { 83, 12, 255 },
        { 85, 12, 255 },
        { 86, 12, 255 },
        { 88, 12, 255 },
        { 89, 13, 255 },
        { 91, 13, 255 },
        { 92, 13, 255 },
        { 94, 13, 255 },
        { 95, 13, 255 },
        { 97, 13, 255 },
        { 98, 13, 255 },
        { 100, 13, 255 },
        { 102, 14, 255 },
        { 103, 14, 255 },
        { 105, 14, 255 },
        { 106, 14, 255 },
        { 108, 14, 255 },
        { 109, 14, 255 },
        { 111, 14, 255 },
        { 112, 14, 255 },
        { 114, 14, 255 },
        { 115, 15, 255 },
        { 117, 15, 255 },
        { 118, 15, 255 },
        { 120, 15, 255 },
        { 121, 15, 255 },
        { 123, 15, 255 },
        { 124, 15, 255 },
        { 126, 15, 255 },
        { 128, 15, 255 },
        { 129, 16, 255 },
        { 131, 16, 255 },
        { 132, 16, 255 },
        { 134, 16, 255 },
        { 135, 16, 255 },
        { 137, 16, 255 },
        { 138, 16, 255 },
        { 140, 16, 255 },
        { 141, 17, 255 },
        { 143, 17, 255 },
        { 144, 17, 255 },
        { 146, 17, 255 },
        { 147, 17, 255 },
        { 149, 17, 255 },
        { 150, 17, 255 },
        { 152, 17, 255 },
        { 154, 17, 255 },
        { 155, 18, 255 },
        { 157, 18, 255 },
        { 158, 18, 255 },
        { 160, 18, 255 },
        { 161, 18, 255 },
        { 163, 18, 255 },
        { 164, 18, 255 },
        { 166, 18, 255 },
        { 167, 18, 255 },
        { 169, 19, 255 },
        { 170, 19, 255 },
        { 172, 19, 255 },
        { 173, 19, 255 },
        { 175, 19, 255 },
        { 176, 19, 255 },
        { 178, 19, 255 },
        { 180, 19, 255 },
        { 181, 19, 255 },
        { 183, 20, 255 },
        { 184, 20, 255 },
        { 186, 20, 255 },
        { 187, 20, 255 },
        { 189, 20, 255 },
        { 190, 20, 255 },
        { 192, 20, 255 },
        { 193, 20, 255 },
        { 195, 21, 255 },
        { 196, 21, 255 },
        { 198, 21, 255 },
        { 199, 21, 255 },
        { 201, 21, 255 },
        { 202, 21, 255 },
        { 204, 21, 255 },
        { 206, 21, 255 },
        { 207, 21, 255 },
        { 209, 22, 255 },
        { 210, 22, 255 },
        { 212, 22, 255 },
        { 213, 22, 255 },
        { 215, 22, 255 },
        { 216, 22, 255 },
        { 255, 21, 216 },
        { 255, 21, 215 },
        { 255, 21, 213 },
        { 255, 21, 212 },
        { 255, 21, 210 },
        { 255, 20, 209 },
        { 255, 20, 207 },
        { 255, 20, 206 },
        { 255, 20, 204 },
        { 255, 20, 202 },
        { 255, 20, 201 },
        { 255, 19, 199 },
        { 255, 19, 198 },
        { 255, 19, 196 },
        { 255, 19, 195 },
        { 255, 19, 193 },
        { 255, 19, 192 },
        { 255, 19, 190 },
        { 255, 18, 189 },
        { 255, 18, 187 },
        { 255, 18, 186 },
        { 255, 18, 184 },
        { 255, 18, 183 },
        { 255, 18, 181 },
        { 255, 18, 180 },
        { 255, 17, 178 },
        { 255, 17, 176 },
        { 255, 17, 175 },
        { 255, 17, 173 },
        { 255, 17, 172 },
        { 255, 17, 170 },
        { 255, 16, 169 },
        { 255, 16, 167 },
        { 255, 16, 166 },
        { 255, 16, 164 },
        { 255, 16, 163 },
        { 255, 16, 161 },
        { 255, 16, 160 },
        { 255, 15, 158 },
        { 255, 15, 157 },
        { 255, 15, 155 },
        { 255, 15, 154 },
        { 255, 15, 152 },
        { 255, 15, 150 },
        { 255, 14, 149 },
        { 255, 14, 147 },
        { 255, 14, 146 },
        { 255, 14, 144 },
        { 255, 14, 143 },
        { 255, 14, 141 },
        { 255, 14, 140 },
        { 255, 13, 138 },
        { 255, 13, 137 },
        { 255, 13, 135 },
        { 255, 13, 134 },
        { 255, 13, 132 },
        { 255, 13, 131 },
        { 255, 12, 129 },
        { 255, 12, 128 },
        { 255, 12, 126 },
        { 255, 12, 124 },
        { 255, 12, 123 },
        { 255, 12, 121 },
        { 255, 12, 120 },
        { 255, 11, 118 },
        { 255, 11, 117 },
        { 255, 11, 115 },
        { 255, 11, 114 },
        { 255, 11, 112 },
        { 255, 11, 111 },
        { 255, 10, 109 },
        { 255, 10, 108 },
        { 255, 10, 106 },
        { 255, 10, 105 },
        { 255, 10, 103 },
        { 255, 10, 101 },
        { 255, 10, 100 },
        { 255,  98,   98 },
        { 255,  97,   97 },
        { 255,  95,   95 },
        { 255,  94,   94 },
        { 255,  92,   92 },
        { 255,  91,   91 },
        { 255,  89,   89 },
        { 255,  88,   88 },
        { 255,  86,   86 },
        { 255,  85,   85 },
        { 255,  83,   83 },
        { 255,  82,   82 },
        { 255,  80,   80 },
        { 255,  79,   79 },
        { 255,  77,   77 },
        { 255,  75,   75 },
        { 255,  74,   74 },
        { 255,  72,   72 },
        { 255,  71,   71 },
        { 255,  69,   69 },
        { 255,  68,   68 },
        { 255,  66,   66 },
        { 255,  65,   65 },
        { 255,  63,   63 },
        { 255,  62,   62 },
        { 255,  60,   60 },
        { 255,  59,   59 },
        { 255,  57,   57 },
        { 255,  56,   56 },
        { 255,  54,   54 },
        { 255,  53,   53 },
        { 255,  51,   51 },
        { 255,  49,   49 },
        { 255,  48,   48 },
        { 255,  46,   46 },
        { 255,  45,   45 },
        { 255,  43,   43 },
        { 255,  42,   42 },
        { 255,  40,   40 },
        { 255,  39,   39 },
        { 255,  37,   37 },
        { 255,  36,   36 },
        { 255,  34,   34 },
        { 255,  33,   33 },
        { 255,  31,   31 },
        { 255,  30,   30 },
        { 255,  28,   28 },
        { 255,  27,   27 },
        { 255, 25, 25 } };

   r = rwbColorRamp[index][0];
   g = rwbColorRamp[index][1];
   b = rwbColorRamp[index][2];

   return;
   }
