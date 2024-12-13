#ifndef SHADER_BICUBIC_TEXTURE_FILTERING_SLANG
#define SHADER_BICUBIC_TEXTURE_FILTERING_SLANG

float c_textureSize = 64.0;

#define c_onePixel 1
#define c_twoPixels 2

float c_x0 = -1.0;
float c_x1 = 0.0;
float c_x2 = 1.0;
float c_x3 = 2.0;

//=======================================================================================
float4 CubicLagrange(float4 A, float4 B, float4 C, float4 D, float t)
{
    return
        A *
               (
            (t - c_x1) / (c_x0 - c_x1) *
                (t - c_x2) / (c_x0 - c_x2) *
                (t - c_x3) / (c_x0 - c_x3)
        ) +
           B *
               (
            (t - c_x0) / (c_x1 - c_x0) *
                (t - c_x2) / (c_x1 - c_x2) *
                (t - c_x3) / (c_x1 - c_x3)
        ) +
           C *
               (
            (t - c_x0) / (c_x2 - c_x0) *
                (t - c_x1) / (c_x2 - c_x1) *
                (t - c_x3) / (c_x2 - c_x3)
        ) +
           D *
               (
            (t - c_x0) / (c_x3 - c_x0) *
                (t - c_x1) / (c_x3 - c_x1) *
                (t - c_x2) / (c_x3 - c_x2)
        );
}

//=======================================================================================
float4 BicubicLagrangeTextureSample(float2 P, Texture2D Texture, uint2 TextureRes)
{
    float2 pixel = P * TextureRes + 0.5;

    float2 frac = fract(pixel);
    uint2 PixelID = uint2(pixel);

    float4 C00 = Texture[PixelID + int2(-c_onePixel, -c_onePixel)];
    float4 C10 = Texture[PixelID + int2(0, -c_onePixel)];
    float4 C20 = Texture[PixelID + int2(c_onePixel, -c_onePixel)];
    float4 C30 = Texture[PixelID + int2(c_twoPixels, -c_onePixel)];
    float4 C01 = Texture[PixelID + int2(-c_onePixel, 0)];
    float4 C11 = Texture[PixelID + int2(0, 0)];
    float4 C21 = Texture[PixelID + int2(c_onePixel, 0)];
    float4 C31 = Texture[PixelID + int2(c_twoPixels, 0)];
    float4 C02 = Texture[PixelID + int2(-c_onePixel, c_onePixel)];
    float4 C12 = Texture[PixelID + int2(0, c_onePixel)];
    float4 C22 = Texture[PixelID + int2(c_onePixel, c_onePixel)];
    float4 C32 = Texture[PixelID + int2(c_twoPixels, c_onePixel)];
    float4 C03 = Texture[PixelID + int2(-c_onePixel, c_twoPixels)];
    float4 C13 = Texture[PixelID + int2(0, c_twoPixels)];
    float4 C23 = Texture[PixelID + int2(c_onePixel, c_twoPixels)];
    float4 C33 = Texture[PixelID + int2(c_twoPixels, c_twoPixels)];

    float4 CP0X = CubicLagrange(C00, C10, C20, C30, frac.x);
    float4 CP1X = CubicLagrange(C01, C11, C21, C31, frac.x);
    float4 CP2X = CubicLagrange(C02, C12, C22, C32, frac.x);
    float4 CP3X = CubicLagrange(C03, C13, C23, C33, frac.x);

    return CubicLagrange(CP0X, CP1X, CP2X, CP3X, frac.y);
}

//=======================================================================================
float4 CubicHermite(float4 A, float4 B, float4 C, float4 D, float t)
{
    float t2 = t * t;
    float t3 = t * t * t;
    float4 a = -A / 2.0 + (3.0 * B) / 2.0 - (3.0 * C) / 2.0 + D / 2.0;
    float4 b = A - (5.0 * B) / 2.0 + 2.0 * C - D / 2.0;
    float4 c = -A / 2.0 + C / 2.0;
    float4 d = B;

    return a * t3 + b * t2 + c * t + d;
}

//=======================================================================================
float4 BicubicHermiteTextureSample(float2 P, Texture2D Texture, uint2 TextureRes)
{
    float2 pixel = P * TextureRes + 0.5;

    float2 frac = fract(pixel);
    uint2 PixelID = uint2(pixel);

    float4 C00 = Texture[PixelID + int2(-c_onePixel, -c_onePixel)];
    float4 C10 = Texture[PixelID + int2(0, -c_onePixel)];
    float4 C20 = Texture[PixelID + int2(c_onePixel, -c_onePixel)];
    float4 C30 = Texture[PixelID + int2(c_twoPixels, -c_onePixel)];
    float4 C01 = Texture[PixelID + int2(-c_onePixel, 0)];
    float4 C11 = Texture[PixelID + int2(0, 0)];
    float4 C21 = Texture[PixelID + int2(c_onePixel, 0)];
    float4 C31 = Texture[PixelID + int2(c_twoPixels, 0)];
    float4 C02 = Texture[PixelID + int2(-c_onePixel, c_onePixel)];
    float4 C12 = Texture[PixelID + int2(0, c_onePixel)];
    float4 C22 = Texture[PixelID + int2(c_onePixel, c_onePixel)];
    float4 C32 = Texture[PixelID + int2(c_twoPixels, c_onePixel)];
    float4 C03 = Texture[PixelID + int2(-c_onePixel, c_twoPixels)];
    float4 C13 = Texture[PixelID + int2(0, c_twoPixels)];
    float4 C23 = Texture[PixelID + int2(c_onePixel, c_twoPixels)];
    float4 C33 = Texture[PixelID + int2(c_twoPixels, c_twoPixels)];

    float4 CP0X = CubicHermite(C00, C10, C20, C30, frac.x);
    float4 CP1X = CubicHermite(C01, C11, C21, C31, frac.x);
    float4 CP2X = CubicHermite(C02, C12, C22, C32, frac.x);
    float4 CP3X = CubicHermite(C03, C13, C23, C33, frac.x);

    return CubicHermite(CP0X, CP1X, CP2X, CP3X, frac.y);
}

#endif