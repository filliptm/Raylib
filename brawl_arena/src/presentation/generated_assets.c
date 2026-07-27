#include "generated_assets.h"

#include "raymath.h"
#include <math.h>
#include <stdlib.h>

static float Hash2(int x, int y, int seed)
{
    int hash = x*374761393 + y*668265263 + seed*1442695040;
    hash = (hash ^ (hash >> 13))*1274126177;
    return (float)((hash ^ (hash >> 16)) & 0xFFFFFF)/(float)0xFFFFFF;
}

static float ValueNoise(float x, float y, int seed)
{
    int cellX = (int)floorf(x);
    int cellY = (int)floorf(y);
    float localX = x - cellX;
    float localY = y - cellY;
    float smoothX = localX*localX*(3.0f - 2.0f*localX);
    float smoothY = localY*localY*(3.0f - 2.0f*localY);

    float a = Hash2(cellX, cellY, seed);
    float b = Hash2(cellX + 1, cellY, seed);
    float c = Hash2(cellX, cellY + 1, seed);
    float d = Hash2(cellX + 1, cellY + 1, seed);

    return (a*(1 - smoothX) + b*smoothX)*(1 - smoothY) +
           (c*(1 - smoothX) + d*smoothX)*smoothY;
}

static float FractalNoise(float x, float y, int seed, int octaves)
{
    float sum = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float normalization = 0.0f;

    for (int octave = 0; octave < octaves; octave++)
    {
        sum += ValueNoise(x*frequency, y*frequency,
                          seed + octave*17)*amplitude;
        normalization += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return sum/normalization;
}

static unsigned char ClampByte(float value)
{
    if (value < 0.0f) return 0;
    if (value > 255.0f) return 255;
    return (unsigned char)value;
}

typedef void (*PixelFunction)(int x, int y, int size, Color *output);

static Texture2D MakeTexture(int size, PixelFunction pixelFunction, bool mipmaps)
{
    Color *pixels = malloc((size_t)size*size*sizeof(*pixels));
    if (!pixels) return (Texture2D){ 0 };

    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            pixelFunction(x, y, size, &pixels[y*size + x]);

    Image image = {
        .data = pixels,
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };
    Texture2D texture = LoadTextureFromImage(image);
    free(pixels);

    if (mipmaps)
    {
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    }
    else
    {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }
    SetTextureWrap(texture, TEXTURE_WRAP_REPEAT);
    return texture;
}

static void FloorPixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size;
    float v = (float)y/size;
    float grain = FractalNoise(u*8.0f, v*8.0f, 11, 4);
    float coarse = FractalNoise(u*2.5f, v*2.5f, 91, 3);
    float base = 74.0f + grain*20.0f + coarse*14.0f;

    int edge = size/32;
    bool onEdge = x < edge || y < edge ||
                  x >= size - edge || y >= size - edge;
    if (onEdge) base *= 0.72f;
    if (x == edge || y == edge ||
        x == size - edge - 1 || y == size - edge - 1)
        base *= 1.22f;

    output->r = ClampByte(base*0.82f);
    output->g = ClampByte(base*0.92f);
    output->b = ClampByte(base*1.12f);
    output->a = 255;
}

static void WallPixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size;
    float v = (float)y/size;
    float upward = 1.0f - v;
    float grain = FractalNoise(u*26.0f, v*6.0f, 23, 3);
    float weather = FractalNoise(u*3.5f, v*3.5f, 77, 3);
    float base = 104.0f + grain*26.0f + weather*22.0f;
    base *= 0.76f + upward*0.32f;

    bool seam = fabsf(u - 0.5f) < 0.042f ||
                fabsf(v - 0.5f) < 0.042f;
    bool border = u < 0.052f || u > 0.948f ||
                  v < 0.052f || v > 0.948f;
    if (seam || border) base *= 0.50f;

    float red = base*0.91f;
    float green = base*0.97f;
    float blue = base*1.19f;
    const float bolts[4][2] = {
        { 0.18f, 0.18f }, { 0.82f, 0.18f },
        { 0.18f, 0.82f }, { 0.82f, 0.82f }
    };
    for (int bolt = 0; bolt < 4; bolt++)
    {
        float dx = u - bolts[bolt][0];
        float dy = v - bolts[bolt][1];
        float distance = sqrtf(dx*dx + dy*dy);
        if (distance < 0.034f)
        {
            float light = 1.62f - (distance/0.034f)*0.55f -
                          (dx + dy)*2.4f;
            if (light < 0.55f) light = 0.55f;
            red = base*light*1.02f;
            green = base*light*1.06f;
            blue = base*light*1.18f;
        }
    }
    if (Hash2(x, y, 5) > 0.988f)
    {
        red *= 0.68f;
        green *= 0.68f;
        blue *= 0.72f;
    }

    output->r = ClampByte(red);
    output->g = ClampByte(green);
    output->b = ClampByte(blue);
    output->a = 255;
}

static void CratePixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size;
    float v = (float)y/size;
    float plankY = v*4.0f;
    float withinPlank = plankY - floorf(plankY);
    float gap = withinPlank < 0.06f || withinPlank > 0.94f ? 0.55f : 1.0f;
    float grain =
        FractalNoise(u*22.0f, floorf(plankY)*9.7f + v*70.0f, 31, 3);
    float base = (150.0f + grain*46.0f)*gap;
    float red = base*1.02f;
    float green = base*0.72f;
    float blue = base*0.42f;

    int band = size/10;
    if (x < band || x >= size - band)
    {
        float metal = 118.0f +
                      FractalNoise(u*30.0f, v*30.0f, 61, 2)*40.0f;
        red = metal*0.96f;
        green = metal;
        blue = metal*1.08f;
    }

    output->r = ClampByte(red);
    output->g = ClampByte(green);
    output->b = ClampByte(blue);
    output->a = 255;
}

static void BushPixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size;
    float v = (float)y/size;
    float leaves = FractalNoise(u*14.0f, v*14.0f, 43, 4);
    float clumps = FractalNoise(u*5.0f, v*5.0f, 13, 3);
    float value = leaves*0.65f + clumps*0.35f;

    output->r = ClampByte(38.0f + value*54.0f);
    output->g = ClampByte(96.0f + value*104.0f);
    output->b = ClampByte(44.0f + value*44.0f);
    output->a = 255;
}

static void MetalPixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size;
    float v = (float)y/size;
    float brushed = FractalNoise(u*60.0f, v*4.0f, 71, 3);
    float base = 96.0f + brushed*54.0f;
    output->r = ClampByte(base*0.95f);
    output->g = ClampByte(base*0.99f);
    output->b = ClampByte(base*1.12f);
    output->a = 255;
}

static void ClothPixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size;
    float v = (float)y/size;
    float weave = FractalNoise(u*26.0f, v*26.0f, 97, 3);
    unsigned char base = ClampByte(214.0f + weave*41.0f);
    *output = (Color){ base, base, base, 255 };
}

static void GrassPixel(int x, int y, int size, Color *output)
{
    enum { BLADES = 5 };
    float u = (float)x/size;
    float heightPosition = 1.0f - (float)y/size;
    *output = (Color){ 255, 255, 255, 0 };

    for (int blade = 0; blade < BLADES; blade++)
    {
        float seed = Hash2(blade, 7, 3);
        float baseX = (blade + 0.5f)/BLADES + (seed - 0.5f)*0.10f;
        float lean = (seed - 0.5f)*0.42f;
        float height = 0.68f + seed*0.32f;
        if (heightPosition > height) continue;

        float along = heightPosition/height;
        float halfWidth =
            (0.055f + seed*0.022f)*(1.0f - along*0.92f);
        float center = baseX + lean*along*along;
        if (fabsf(u - center) < halfWidth)
        {
            float shade = 0.80f + seed*0.20f +
                          FractalNoise(u*20.0f, heightPosition*6.0f,
                                       5, 2)*0.14f;
            unsigned char color = ClampByte(255.0f*shade);
            *output = (Color){ color, color, color, 255 };
            return;
        }
    }
}

static void FlatPixel(int x, int y, int size, Color *output)
{
    (void)x;
    (void)y;
    (void)size;
    *output = WHITE;
}

static void GlowPixel(int x, int y, int size, Color *output)
{
    float centerX = (x + 0.5f)/size - 0.5f;
    float centerY = (y + 0.5f)/size - 0.5f;
    float distance = sqrtf(centerX*centerX + centerY*centerY)*2.0f;
    float alpha = 1.0f - distance;
    if (alpha < 0.0f) alpha = 0.0f;
    alpha *= alpha;
    *output = (Color){ 255, 255, 255, ClampByte(alpha*255.0f) };
}

static Mesh MakeGrassBlade(void)
{
    enum { QUADS = 3 };
    Mesh mesh = { 0 };
    mesh.vertexCount = QUADS*4;
    mesh.triangleCount = QUADS*2;
    mesh.vertices = MemAlloc(mesh.vertexCount*3*sizeof(float));
    mesh.texcoords = MemAlloc(mesh.vertexCount*2*sizeof(float));
    mesh.normals = MemAlloc(mesh.vertexCount*3*sizeof(float));
    mesh.indices =
        MemAlloc(mesh.triangleCount*3*sizeof(unsigned short));

    for (int quad = 0; quad < QUADS; quad++)
    {
        float angle = (quad/(float)QUADS)*PI;
        float centerX = cosf(angle)*0.5f;
        float centerZ = sinf(angle)*0.5f;
        float normalX = -sinf(angle);
        float normalZ = cosf(angle);
        Vector3 normal = Vector3Normalize(
            (Vector3){ normalX*0.45f, 0.78f, normalZ*0.45f });

        int vertex = quad*4;
        const float positionsX[4] = {
            -centerX, centerX, centerX, -centerX
        };
        const float positionsZ[4] = {
            -centerZ, centerZ, centerZ, -centerZ
        };
        const float positionsY[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        const float textureU[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
        const float textureV[4] = { 1.0f, 1.0f, 0.0f, 0.0f };

        for (int point = 0; point < 4; point++)
        {
            mesh.vertices[(vertex + point)*3 + 0] = positionsX[point];
            mesh.vertices[(vertex + point)*3 + 1] = positionsY[point];
            mesh.vertices[(vertex + point)*3 + 2] = positionsZ[point];
            mesh.texcoords[(vertex + point)*2 + 0] = textureU[point];
            mesh.texcoords[(vertex + point)*2 + 1] = textureV[point];
            mesh.normals[(vertex + point)*3 + 0] = normal.x;
            mesh.normals[(vertex + point)*3 + 1] = normal.y;
            mesh.normals[(vertex + point)*3 + 2] = normal.z;
        }

        int triangle = quad*6;
        mesh.indices[triangle + 0] = (unsigned short)(vertex + 0);
        mesh.indices[triangle + 1] = (unsigned short)(vertex + 1);
        mesh.indices[triangle + 2] = (unsigned short)(vertex + 2);
        mesh.indices[triangle + 3] = (unsigned short)(vertex + 0);
        mesh.indices[triangle + 4] = (unsigned short)(vertex + 2);
        mesh.indices[triangle + 5] = (unsigned short)(vertex + 3);
    }

    UploadMesh(&mesh, false);
    return mesh;
}

void GeneratedAssetsLoad(Assets *assets)
{
    assets->grassBlade = MakeGrassBlade();
    assets->texFloor = MakeTexture(256, FloorPixel, true);
    assets->texWall = MakeTexture(256, WallPixel, true);
    assets->texCrate = MakeTexture(256, CratePixel, true);
    assets->texBush = MakeTexture(128, BushPixel, true);
    assets->texMetal = MakeTexture(128, MetalPixel, true);
    assets->texCloth = MakeTexture(128, ClothPixel, true);
    assets->texFlat = MakeTexture(4, FlatPixel, false);
    assets->texGlow = MakeTexture(128, GlowPixel, false);
    assets->texGrass = MakeTexture(256, GrassPixel, true);
}
