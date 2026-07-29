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

// Deep space: near-black base, faint nebula wisps, and two star layers whose cells
// each own one star's position, brightness, and colour temperature.
static void StarfieldPixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size;
    float v = (float)y/size;

    float nebula = FractalNoise(u*3.0f, v*3.0f, 401, 4);
    float wisp = FractalNoise(u*6.0f + 7.0f, v*6.0f, 431, 4);
    wisp = wisp*wisp*wisp;
    float baseR = 5.0f + nebula*8.0f + wisp*40.0f;
    float baseG = 5.0f + nebula*6.0f + wisp*26.0f;
    float baseB = 9.0f + nebula*13.0f + wisp*56.0f;

    float star = 0.0f;
    float warmth = 0.5f;
    for (int layer = 0; layer < 2; layer++)
    {
        int cells = (layer == 0) ? 48 : 22;
        float density = (layer == 0) ? 0.30f : 0.14f;
        float radius = (layer == 0) ? 0.085f : 0.16f;
        float cx = u*cells, cy = v*cells;
        int ix = (int)cx, iy = (int)cy;
        if (Hash2(ix, iy, 701 + layer) >= density) continue;

        float dx = (cx - ix) - (0.2f + Hash2(ix, iy, 501 + layer)*0.6f);
        float dy = (cy - iy) - (0.2f + Hash2(ix, iy, 601 + layer)*0.6f);
        float d = sqrtf(dx*dx + dy*dy);
        float bright = 1.0f - d/radius;
        if (bright <= 0.0f) continue;
        bright *= bright;
        if (bright > star)
        {
            star = bright;
            warmth = Hash2(ix, iy, 801 + layer);
        }
    }

    float sr = warmth > 0.66f ? 1.00f : 0.86f;
    float sg = warmth > 0.66f ? 0.84f : 0.92f;
    float sb = warmth > 0.66f ? 0.60f : 1.00f;
    output->r = ClampByte(baseR + star*255.0f*sr);
    output->g = ClampByte(baseG + star*255.0f*sg);
    output->b = ClampByte(baseB + star*255.0f*sb);
    output->a = 255;
}

// A lit planet disc: sphere-projected normal, banded surface noise, a soft
// terminator, and a blue atmosphere halo that fades out through the alpha channel.
static void PlanetPixel(int x, int y, int size, Color *output)
{
    float u = (float)x/size - 0.5f;
    float v = (float)y/size - 0.5f;
    float r = sqrtf(u*u + v*v)/0.44f;

    if (r > 1.12f) { *output = (Color){ 0, 0, 0, 0 }; return; }
    if (r > 1.0f)
    {
        float halo = 1.0f - (r - 1.0f)/0.12f;
        halo *= halo;
        *output = (Color){ 120, 190, 255, ClampByte(halo*95.0f) };
        return;
    }

    float nx = u/0.44f;
    float ny = v/0.44f;
    float nz = sqrtf(fmaxf(0.0f, 1.0f - nx*nx - ny*ny));
    float diffuse = fmaxf(0.0f, -0.50f*nx - 0.55f*ny + 0.67f*nz);
    float shade = 0.06f + 0.94f*diffuse;

    // Latitude bands with a longitudinal swirl, plus sparse warm storm streaks.
    float band = FractalNoise((ny*0.5f + 0.5f)*7.0f,
                              (nx*0.5f + 0.5f)*1.6f, 901, 4);
    float storm = FractalNoise((nx*0.5f + 0.5f)*5.0f + band,
                               (ny*0.5f + 0.5f)*5.0f, 931, 3);
    storm = fmaxf(0.0f, storm - 0.62f)*2.6f;

    float cr = 28.0f + band*46.0f + storm*150.0f;
    float cg = 62.0f + band*66.0f + storm*96.0f;
    float cb = 104.0f + band*58.0f + storm*36.0f;

    // Atmosphere rim brightens the limb even on the night side.
    float rim = powf(1.0f - nz, 2.5f);
    cr = cr*shade + rim*70.0f;
    cg = cg*shade + rim*120.0f;
    cb = cb*shade + rim*180.0f;

    float edge = 1.0f - fmaxf(0.0f, (r - 0.985f)/0.015f);
    output->r = ClampByte(cr);
    output->g = ClampByte(cg);
    output->b = ClampByte(cb);
    output->a = ClampByte(edge*255.0f);
}

// A curved plate for authored shield-wave effects: a horizontal arc with a slight
// vertical barrel bulge, centred on the origin and facing +Z, double-sided so it
// reads from every camera angle. Scaled non-uniformly at draw time.
static Mesh MakeShieldArcMesh(void)
{
    const int columns = 16;
    const int rows = 4;
    const float arcSpan = 80.0f*DEG2RAD;

    int gridVerts = (columns + 1)*(rows + 1);
    int vertexCount = gridVerts*2;               // front and back faces
    int triangleCount = columns*rows*2*2;

    Mesh mesh = { 0 };
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = malloc((size_t)vertexCount*3*sizeof(float));
    mesh.normals = malloc((size_t)vertexCount*3*sizeof(float));
    mesh.texcoords = malloc((size_t)vertexCount*2*sizeof(float));
    mesh.indices = malloc((size_t)triangleCount*3*sizeof(unsigned short));
    if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.indices)
        return (Mesh){ 0 };

    for (int side = 0; side < 2; side++)
    {
        float flip = side == 0 ? 1.0f : -1.0f;
        for (int row = 0; row <= rows; row++)
        {
            float v = row/(float)rows;
            float bulge = 0.12f*(1.0f - (2.0f*v - 1.0f)*(2.0f*v - 1.0f));
            for (int col = 0; col <= columns; col++)
            {
                float u = col/(float)columns;
                float a = (u - 0.5f)*arcSpan;
                float radius = 1.0f + bulge;
                int index = side*gridVerts + row*(columns + 1) + col;

                mesh.vertices[index*3 + 0] = sinf(a)*radius;
                mesh.vertices[index*3 + 1] = v - 0.5f;
                mesh.vertices[index*3 + 2] = cosf(a)*radius - 1.0f;
                mesh.normals[index*3 + 0] = sinf(a)*flip;
                mesh.normals[index*3 + 1] = 0.0f;
                mesh.normals[index*3 + 2] = cosf(a)*flip;
                mesh.texcoords[index*2 + 0] = u;
                mesh.texcoords[index*2 + 1] = v;
            }
        }
    }

    int triangle = 0;
    for (int side = 0; side < 2; side++)
    {
        int base = side*gridVerts;
        for (int row = 0; row < rows; row++)
        {
            for (int col = 0; col < columns; col++)
            {
                unsigned short a = (unsigned short)(base + row*(columns + 1) + col);
                unsigned short b = (unsigned short)(a + 1);
                unsigned short c = (unsigned short)(a + columns + 1);
                unsigned short d = (unsigned short)(c + 1);
                if (side == 0)
                {
                    mesh.indices[triangle*3 + 0] = a;
                    mesh.indices[triangle*3 + 1] = c;
                    mesh.indices[triangle*3 + 2] = b;
                    triangle++;
                    mesh.indices[triangle*3 + 0] = b;
                    mesh.indices[triangle*3 + 1] = c;
                    mesh.indices[triangle*3 + 2] = d;
                    triangle++;
                }
                else
                {
                    mesh.indices[triangle*3 + 0] = a;
                    mesh.indices[triangle*3 + 1] = b;
                    mesh.indices[triangle*3 + 2] = c;
                    triangle++;
                    mesh.indices[triangle*3 + 0] = b;
                    mesh.indices[triangle*3 + 1] = d;
                    mesh.indices[triangle*3 + 2] = c;
                    triangle++;
                }
            }
        }
    }

    UploadMesh(&mesh, false);
    return mesh;
}

// Hemisphere for sanctuary domes: lat/long shell, base ring at y=0, apex at y=1.
static Mesh MakeDomeMesh(void)
{
    const int slices = 20;
    const int stacks = 7;
    int vertexCount = (slices + 1)*(stacks + 1);
    int triangleCount = slices*stacks*2;

    Mesh mesh = { 0 };
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = malloc((size_t)vertexCount*3*sizeof(float));
    mesh.normals = malloc((size_t)vertexCount*3*sizeof(float));
    mesh.texcoords = malloc((size_t)vertexCount*2*sizeof(float));
    mesh.indices = malloc((size_t)triangleCount*3*sizeof(unsigned short));
    if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.indices)
        return (Mesh){ 0 };

    for (int stack = 0; stack <= stacks; stack++)
    {
        float v = stack/(float)stacks;
        float pitch = v*(PI*0.5f);
        for (int slice = 0; slice <= slices; slice++)
        {
            float u = slice/(float)slices;
            float a = u*PI*2.0f;
            int index = stack*(slices + 1) + slice;
            float nx = sinf(a)*cosf(pitch);
            float ny = sinf(pitch);
            float nz = cosf(a)*cosf(pitch);
            mesh.vertices[index*3 + 0] = nx;
            mesh.vertices[index*3 + 1] = ny;
            mesh.vertices[index*3 + 2] = nz;
            mesh.normals[index*3 + 0] = nx;
            mesh.normals[index*3 + 1] = ny;
            mesh.normals[index*3 + 2] = nz;
            mesh.texcoords[index*2 + 0] = u;
            mesh.texcoords[index*2 + 1] = v;
        }
    }
    int triangle = 0;
    for (int stack = 0; stack < stacks; stack++)
        for (int slice = 0; slice < slices; slice++)
        {
            unsigned short a = (unsigned short)(stack*(slices + 1) + slice);
            unsigned short b = (unsigned short)(a + 1);
            unsigned short c = (unsigned short)(a + slices + 1);
            unsigned short d = (unsigned short)(c + 1);
            mesh.indices[triangle*3 + 0] = a;
            mesh.indices[triangle*3 + 1] = b;
            mesh.indices[triangle*3 + 2] = c;
            triangle++;
            mesh.indices[triangle*3 + 0] = b;
            mesh.indices[triangle*3 + 1] = d;
            mesh.indices[triangle*3 + 2] = c;
            triangle++;
        }
    UploadMesh(&mesh, false);
    return mesh;
}

// Open cylinder shell for light shafts: radius 1, y 0..1, no caps.
static Mesh MakeColumnMesh(void)
{
    const int slices = 16;
    int vertexCount = (slices + 1)*2;
    int triangleCount = slices*2;

    Mesh mesh = { 0 };
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = malloc((size_t)vertexCount*3*sizeof(float));
    mesh.normals = malloc((size_t)vertexCount*3*sizeof(float));
    mesh.texcoords = malloc((size_t)vertexCount*2*sizeof(float));
    mesh.indices = malloc((size_t)triangleCount*3*sizeof(unsigned short));
    if (!mesh.vertices || !mesh.normals || !mesh.texcoords || !mesh.indices)
        return (Mesh){ 0 };

    for (int ring = 0; ring <= 1; ring++)
        for (int slice = 0; slice <= slices; slice++)
        {
            float u = slice/(float)slices;
            float a = u*PI*2.0f;
            int index = ring*(slices + 1) + slice;
            mesh.vertices[index*3 + 0] = sinf(a);
            mesh.vertices[index*3 + 1] = (float)ring;
            mesh.vertices[index*3 + 2] = cosf(a);
            mesh.normals[index*3 + 0] = sinf(a);
            mesh.normals[index*3 + 1] = 0.0f;
            mesh.normals[index*3 + 2] = cosf(a);
            mesh.texcoords[index*2 + 0] = u;
            mesh.texcoords[index*2 + 1] = (float)ring;
        }
    int triangle = 0;
    for (int slice = 0; slice < slices; slice++)
    {
        unsigned short a = (unsigned short)slice;
        unsigned short b = (unsigned short)(slice + 1);
        unsigned short c = (unsigned short)(slice + slices + 1);
        unsigned short d = (unsigned short)(c + 1);
        mesh.indices[triangle*3 + 0] = a;
        mesh.indices[triangle*3 + 1] = b;
        mesh.indices[triangle*3 + 2] = c;
        triangle++;
        mesh.indices[triangle*3 + 0] = b;
        mesh.indices[triangle*3 + 1] = d;
        mesh.indices[triangle*3 + 2] = c;
        triangle++;
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
    assets->texStarfield = MakeTexture(512, StarfieldPixel, true);
    assets->texPlanet = MakeTexture(512, PlanetPixel, true);
    assets->shieldArc = MakeShieldArcMesh();
    assets->dome = MakeDomeMesh();
    assets->column = MakeColumnMesh();
}
