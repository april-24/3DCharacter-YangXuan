/*
================================================================================
 TEXTURE FILES NEEDED (place inside a "Textures" subfolder next to the .exe):\
 PLEASE ENSURE ALL FILES ARE PRESENT AND NAMED EXACTLY AS BELOW
   Textures\skin.bmp    -> face, neck, hands/palm, fingers
   Textures\armor.bmp   -> chest armor, shoulder plates, knee/elbow/wrist guards
   Textures\navy.bmp    -> body robe/tunic, upper arm sleeve, leg pants
   Textures\gold.bmp    -> belt, crown, armor trim, spear guard, bell body
   Textures\silver.bmp  -> spear center blade, side blades
   Textures\boot.bmp    -> boots, boot trim/collar
   Textures\hair.bmp    -> hair bun, ponytail, side hair
   Textures\ground.bmp  -> ground/grass plane
   Textures\Wall.bmp    -> sky background ONLY
   Textures\roof.bmp    -> temple roof tiles
   Textures\wood.bmp    -> temple doors, window frames
   Textures\stone.bmp   -> temple platforms/steps
   Textures\pillar.bmp  -> dragon pillar body + temple pillar shaft (same file, separate slots)
   Textures\bellbody.bmp-> bell main body texture
   Textures\bellrim.bmp -> bell rim/clapper accent texture

 HOW TO DEPLOY:
   1. Build the project in Visual Studio.
   2. Copy the "Textures" folder into the same directory as the compiled .exe
      (e.g. x64\Debug\Textures\).
   3. The program uses relative paths ("Textures\xxx.bmp"), so it works on
      any machine as long as the Textures folder sits beside the .exe.
================================================================================
*/

#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <string>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "GLU32.lib")

// Window title shown in the title bar
#define WINDOW_TITLE "Yang Xuan"

// Mathematical constant PI used for angle conversions (degrees <-> radians)
const float PI = 3.14159265f;

// ============================================================
//  GLOBALS - CAMERA
// ============================================================
float camDist = 400.0f; //distance from the camera to the scene pivot point
// pry: horizontal (yaw) rotation of the camera around the Y-axis (degrees)
// prx: vertical (pitch) tilt of the camera (degrees)
float pry = 0.0f, prx = 12.0f;
float prSpeed = 2.0f, prxSpeed = 2.0f; // Rotation speeds for camera yaw and pitch
float camPanX = 0.0f, camPanY = 0.0f; // Camera pan offsets (side-scroll and up-down translation in view space)
bool  isOrtho = false; // Toggle between perspective and orthographic projection
float PNear = 1.0f, PFar = 5000.0f; // Objects outside [PNear, PFar] range are not drawn
int   gWinW = 1024, gWinH = 768; // Window dimensions
bool isDragging = false; // Mouse drag state
int  lastMouseX = 0, lastMouseY = 0; // Last mouse position, used to calculate drag deltas for camera control

// ============================================================
//  GLOBALS - CHARACTER
// ============================================================
float charX = 0.0f, charZ = 200.0f, charRotY = 0.0f; // charRotY = 0 means facing down the -Z axis, positive values rotate clockwise when viewed from above
bool  isWalking = false; // true when W key is held down, false otherwise
float walkTimer = 0.0f; // Tracks time elapsed since the start of the current walk cycle, used to drive leg/arm animation
bool  enableEarthquake = true; // Earthquake effect toggle
// World boundary: character is clamped inside this rectangular play area
const float LAND_MIN_X = -480.0f, LAND_MAX_X = 480.0f;
const float LAND_MIN_Z = -480.0f, LAND_MAX_Z = 480.0f;
// Axis-aligned bounding box of the temple, block the character from walking through it
const float TEMPLE_MIN_X = -140.0f, TEMPLE_MAX_X = 140.0f;
const float TEMPLE_MIN_Z = -200.0f, TEMPLE_MAX_Z = 80.0f;

// ---- Joint rotations (degrees) ----
float headRotY = 0.0f, headRotX = 0.0f; // head
float lUpperArmPitch = 0.0f, lUpperArmSide = 0.0f, lForearmBend = 0.0f; // left arm
float lWristPitch = 0.0f, lWristYaw = 0.0f; // left wrist
float rUpperArmPitch = 0.0f, rUpperArmSide = 0.0f, rForearmBend = 0.0f; // right arm
float rWristPitch = 0.0f, rWristYaw = 0.0f; // right wrist
float lHipPitch = 0.0f, lKneeBend = 0.0f, lFootPitch = 0.0f; // left leg
float rHipPitch = 0.0f, rKneeBend = 0.0f, rFootPitch = 0.0f; // right leg

// ---- Spear & fire state ----
bool showWeapon = false; // spear visibility toggle
bool isFireActive = false; // fire effect toggle
bool isFist = false; // controls fist
float fistBlend = 0.0f; // open hand/close hand
bool  autoArmMove = false; // true when the arms are automatically moving to the bell launch pose, false otherwise
float autoArmTimer = 0.0f; // autoArmTimer tracks the time elapsed since the start of the automatic arm movement, drive the interpolation from the current arm pose to the bell launch pose

// ============================================================
//  GLOBALS - BELL WEAPON
// ============================================================

// States for the bell's lifecycle
enum BellState {
    BELL_HIDDEN = 0,   // Bell is not visible
    BELL_SUMMONING,    // Bell descends from the sky into Yang Xuan's palm
    BELL_PALM_HOVER,   // Bell floats and orbits above the open palm
    BELL_LAUNCHING,    // Bell accelerates forward after F2 is pressed
    BELL_ATTACK_ACTIVE,// Bell is flying forward, emitting shockwave rings
    BELL_RETURNING,    // Bell travels back to hover above the palm
    BELL_DISMISSING    // Bell shrinks and fades out on F3
};
BellState bellState = BELL_HIDDEN;

// Timers that track how long the bell has been in each phase
float bellSummonTimer = 0.0f;
float bellLaunchTimer = 0.0f;
float bellDismissTimer = 0.0f;
float bellWaveTimer = 0.0f;                                         // Tracks the timing of shockwave rings emitted while the bell is in flight
float bellRingTimer = 0.0f;                                         // Tracks the timing of ringing sound effect while hovering in the palm
float bellScale = 1.0f;                                             // Scale the bell
float bellOrbitAngle = 0.0f;                                        // Angle of bell's orbit around the palm 
float bellWorldX = 0.0f, bellWorldY = 0.0f, bellWorldZ = 0.0f;      // Bell position
float bellVelX = 0.0f, bellVelY = 0.0f, bellVelZ = 0.0f;            // Bell velocity
const float SUMMON_DURATION = 2.5f;                                 // Time to descend from the sky
const float LAUNCH_ACCEL_TIME = 0.4f;                               // Time when the bell is accelerating forward
const float BELL_PALM_SCALE = 0.08f;                                //Scale factor
const float BELL_ATTACK_SCALE = 0.75f;                              //Larger scale for attack

// ============================================================
//  GLOBALS - BELL ATTACK BODY ANIMATION
// ============================================================

float bellAttackBodyTimer = 0.0f;           // Current time in attack pose
bool  bellAttackBodyActive = false;         // Is body pose animation running?
const float BELL_ATTACK_BODY_DURATION = 1.2f; // Animation total time (s)

// Cinematic camera state
bool  cineCameraActive = false;             // Is camera currently cinematic?
float cineSavedPry, cineSavedPrx, cineSavedDist; // Cached camera settings
float cineSavedPanX, cineSavedPanY;         // Cached pan offsets

// ============================================================
//  GLOBALS - SCENE / ANIM
// ============================================================

int   lightMode = 3;                        // 0=Off, 1=Amb, 2=Diff, 3=Both
int   timeOfDay = 1;                        // 0=Morn, 1=Day, 2=Eve, 3=Night
bool  enableTextures = false;               // Enable/Disable texture mapping
int   activeAnim = 0;                       // Current animation ID
float animTimer = 0.0f;                     // Current animation elapsed time (s)

bool  doorOpen = false;                     // Open/Closed state
float doorAngle = 0.0f;                     // Current door rotation (0-100)

float manualLightAzimuth = 45.0f;           // Light rotation (azimuth)
float manualLightElevation = 60.0f;         // Light rotation (elevation)

auto lastTime = std::chrono::high_resolution_clock::now(); // Frame time tracking

// ============================================================
//  TEXTURES
// ============================================================

// Enumeration of all texture slots used in the scene.
// Each name maps to a specific BMP file loaded at startup.
enum TextureTypes {
    TEX_SKIN = 0, // skin.bmp   - character skin (face, neck, hands)
    TEX_NAVY,     // navy.bmp   - navy robe / pants / sleeves
    TEX_ARMOR,    // armor.bmp  - chest/shoulder/knee armor plates
    TEX_GOLD,     // gold.bmp   - gold trim, belt buckle, crown
    TEX_SILVER,   // silver.bmp - spear blades
    TEX_BOOT,     // boot.bmp   - boots and boot collar
    TEX_HAIR,     // hair.bmp   - hair bun and ponytail
    TEX_GROUND,   // ground.bmp - ground/grass plane
    TEX_SKY,      // Wall.bmp   - sky background (used as skybox walls)
    // Temple textures
    TEX_ROOF,     // roof.bmp   - temple roof tiles
    TEX_WOOD,     // wood.bmp   - temple doors and window frames
    TEX_GLASS,    // Tglass.bmp - window glass panes
    TEX_STONE,    // stone.bmp  - platform tiers and steps
    TEX_PILLAR,   // pillar.bmp - dragon pillar body (teal coil)
    TEX_WALL,     // Wall.bmp   - temple wall panels (vermillion tint, own GL object)
    TEX_Body,     // TBody.bmp  - temple body / wall surface
    TEX_TEMPPILLAR, // pillar.bmp - 4 front pillar shafts (own GL object)
    // Bell textures
    TEX_BELLBODY, // bellbody.bmp - bell main body bands
    TEX_BELLRIM,  // bellrim.bmp  - bell rim and clapper accent
    TEX_COUNT     // Total number of texture slots (must be last)
};

// OpenGL texture object IDs, indexed by the enum above
GLuint textures[TEX_COUNT];

// ------------------------------------------------------------
// loadBMP: Loads a 24-bit uncompressed BMP file from disk and
//          uploads it as an OpenGL 2D texture.
// Returns: GL texture ID on success, 0 on failure (file not found).
// Note:    BMP stores pixels in BGR order; we swap R<->B before upload.
// ------------------------------------------------------------
GLuint loadBMP(const char* filename) {
    // Open the BMP file in binary mode
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;

    // Read the 54-byte BMP file header
    unsigned char hdr[54];
    fread(hdr, 1, 54, f);

    // Extract image width, height, and pixel data offset from the header
    int w = *(int*)&hdr[18];
    int h = *(int*)&hdr[22];
    int dataOffset = *(int*)&hdr[10];

    // Seek to the start of pixel data
    fseek(f, dataOffset, SEEK_SET);

    // Allocate buffer for raw BGR pixel data (3 bytes per pixel)
    int sz = w * h * 3;
    unsigned char* data = new unsigned char[sz];
    fread(data, 1, sz, f);
    fclose(f);

    // Convert BGR -> RGB by swapping the first and third byte of each pixel
    for (int i = 0; i < sz; i += 3) {
        unsigned char tmp = data[i];
        data[i] = data[i + 2];
        data[i + 2] = tmp;
    }

    // Generate and bind a new OpenGL texture object
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // Set texture filtering: GL_LINEAR gives smooth (bilinear) sampling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Wrap mode GL_REPEAT tiles the texture for surfaces larger than 1 UV unit
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Ensure pixel rows are tightly packed (no padding)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Upload pixel data to GPU as an RGB texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    delete[] data;
    return id;
}

// ------------------------------------------------------------
// loadAllTextures: Loads every BMP listed in the files[] array
//                 and stores the resulting GL texture IDs in textures[].
//
// Path convention: "Textures\filename.bmp"
//   - Relative to the .exe directory, so the "Textures" folder must
//     be placed next to the executable (e.g. x64\Debug\Textures\).
//   - This makes the project portable: copy the folder alongside
//     the .exe and it works on any machine without code changes.
// ------------------------------------------------------------
void loadAllTextures() {
    const char* files[TEX_COUNT] = {
        "Textures\\skin.bmp",      // TEX_SKIN    - character skin
        "Textures\\navy.bmp",      // TEX_NAVY    - navy robe
        "Textures\\armor.bmp",     // TEX_ARMOR   - armor plates
        "Textures\\gold.bmp",      // TEX_GOLD    - gold trim/belt
        "Textures\\silver.bmp",    // TEX_SILVER  - spear blades
        "Textures\\boot.bmp",      // TEX_BOOT    - boots
        "Textures\\hair.bmp",      // TEX_HAIR    - hair
        "Textures\\ground.bmp",    // TEX_GROUND  - ground plane
        "Textures\\Wall.bmp",      // TEX_SKY     - sky background only
        "Textures\\roof.bmp",      // TEX_ROOF    - temple roof tiles
        "Textures\\wood.bmp",      // TEX_WOOD    - temple doors/frames
        "Textures\\Tglass.bmp",    // TEX_GLASS   - window glass
        "Textures\\stone.bmp",     // TEX_STONE   - platform/steps
        "Textures\\pillar.bmp",    // TEX_PILLAR  - dragon pillar body
        "Textures\\Wall.bmp",      // TEX_WALL    - temple wall panels (own GL object!)
        "Textures\\TBody.bmp",     // TEX_Body    - temple body surface
        "Textures\\pillar.bmp",    // TEX_TEMPPILLAR - front pillar shafts (own GL object!)
        "Textures\\bellbody.bmp",  // TEX_BELLBODY - bell main body
        "Textures\\bellrim.bmp"    // TEX_BELLRIM  - bell rim/clapper accent
    };

    // Load each BMP and store its OpenGL texture ID
    for (int i = 0; i < TEX_COUNT; i++) {
        textures[i] = loadBMP(files[i]);
    }
}

// ------------------------------------------------------------
// applyTexture: Binds a texture slot and sets the material to white
//               so the texture colour is used as-is.
//               If textures are disabled, turns off GL_TEXTURE_2D.
// ------------------------------------------------------------
void applyTexture(int texID) {
    if (enableTextures && textures[texID]) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[texID]);

        // White diffuse lets the texture colour pass through unchanged
        GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        GLfloat amb[] = { 0.5f, 0.5f, 0.5f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, white);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    else {
        // No texture mode: fall back to plain glColor3f values
        glDisable(GL_TEXTURE_2D);
    }
}

// ============================================================
//  MATERIALS
// ============================================================

// ------------------------------------------------------------
// setMaterial: Configures OpenGL material properties for the
//              next geometry that will be drawn.
//   r,g,b       - diffuse (base) colour
//   shininess   - specular exponent (higher = sharper highlight)
//   specR/G/B   - specular highlight colour intensity
// ------------------------------------------------------------
void setMaterial(float r, float g, float b, float shininess = 32.0f,
    float specR = 0.4f, float specG = 0.4f, float specB = 0.4f) {
    // Diffuse: the main lit colour seen under direct light
    GLfloat diff[] = { r, g, b, 1 };
    // Ambient: colour seen in shadow (set to 40% of diffuse for subtle fill)
    GLfloat amb[] = { r * 0.4f, g * 0.4f, b * 0.4f, 1 };
    // Specular: highlight colour for shiny surfaces
    GLfloat spec[] = { specR, specG, specB, 1 };
    GLfloat shin[] = { shininess };
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, amb);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shin);
    // Also set glColor so it shows correctly when lighting is off
    glColor3f(r, g, b);
}

// ---- Skin colour helpers ----
// colSkin: warm peach tone for face, neck, hands
void colSkin() {
    setMaterial(0.863f, 0.745f, 0.647f, 16, 0.2f, 0.15f, 0.1f);
    applyTexture(TEX_SKIN);
}

// colSkinDark: slightly darker shade for nose, chin shadow areas
void colSkinDark() {
    setMaterial(0.72f, 0.60f, 0.50f, 12, 0.1f, 0.1f, 0.1f);
    applyTexture(TEX_SKIN);
}

// ---- Hair colour helpers ----
// colHair: near-black base hair colour (bun, ponytail, side strands)
void colHair() {
    setMaterial(0.137f, 0.118f, 0.118f, 24, 0.3f, 0.3f, 0.3f);
    applyTexture(TEX_HAIR);
}

// colHairHigh: slightly lighter hair for highlight strands in the ponytail
void colHairHigh() {
    setMaterial(0.20f, 0.18f, 0.22f, 20, 0.2f, 0.2f, 0.2f);
    applyTexture(TEX_HAIR);
}

// ---- Boot colour helpers ----
// colBoot: very dark brown/black for the boot shaft
void colBoot() {
    glColor3f(0.12f, 0.10f, 0.09f);
    applyTexture(TEX_BOOT);
}

// colBootTrim: slightly lighter trim/collar at the boot top
void colBootTrim() {
    glColor3f(0.25f, 0.22f, 0.18f);
    applyTexture(TEX_BOOT);
}

// ---- Armour colour helpers ----
// colArmorBase: mid-grey metallic base for chest/shoulder/knee plates
void colArmorBase() {
    setMaterial(0.55f, 0.55f, 0.55f, 64, 0.8f, 0.8f, 0.8f);
    applyTexture(TEX_ARMOR);
}

// colArmorLight: lighter grey for raised/highlighted armour edges
void colArmorLight() {
    setMaterial(0.70f, 0.70f, 0.70f, 64, 0.9f, 0.9f, 0.9f);
    applyTexture(TEX_ARMOR);
}

// colArmorDark: darker grey for recessed/shadowed armour panels
void colArmorDark() {
    setMaterial(0.35f, 0.35f, 0.35f, 32, 0.4f, 0.4f, 0.4f);
    applyTexture(TEX_ARMOR);
}

// ---- Gold colour helpers (used for belt, crown, trim, spear guard) ----
// colGoldTrim: standard gold trim, medium brightness
void colGoldTrim() {
    setMaterial(0.745f, 0.569f, 0.176f, 80, 1.0f, 0.9f, 0.4f);
    applyTexture(TEX_GOLD);
}

// colGoldLight: bright highlight gold for prominent raised details
void colGoldLight() {
    setMaterial(1.00f, 0.86f, 0.42f, 96, 1.0f, 1.0f, 0.6f);
    applyTexture(TEX_GOLD);
}

// colGoldDark: darker, deeper gold for shadowed gold elements
void colGoldDark() {
    setMaterial(0.55f, 0.42f, 0.06f, 48, 0.7f, 0.6f, 0.2f);
    applyTexture(TEX_GOLD);
}

// colGold: general-purpose mid-range gold
void colGold() {
    setMaterial(0.85f, 0.68f, 0.18f, 80, 1.0f, 0.9f, 0.4f);
    applyTexture(TEX_GOLD);
}

// ---- Navy colour helpers (robe, sleeve, trouser) ----
// colNavy: deep navy blue for the base robe fabric
void colNavy() {
    setMaterial(0.063f, 0.153f, 0.318f, 16, 0.2f, 0.2f, 0.3f);
    applyTexture(TEX_NAVY);
}

// colNavyLight: slightly lighter navy for creases/highlights
void colNavyLight() {
    setMaterial(0.12f, 0.22f, 0.42f, 20, 0.2f, 0.2f, 0.3f);
    applyTexture(TEX_NAVY);
}

// colNavyDark: deeper navy for shadowed under-robe areas
void colNavyDark() {
    setMaterial(0.03f, 0.08f, 0.18f, 12, 0.1f, 0.1f, 0.2f);
    applyTexture(TEX_NAVY);
}

// ---- Silver colour helpers (spear blades) ----
// colSilver: polished silver for the main spear blade faces
void colSilver() {
    setMaterial(0.75f, 0.77f, 0.82f, 96, 1.0f, 1.0f, 1.0f);
    applyTexture(TEX_SILVER);
}

// colSilverLight: near-white highlight on blade edges/spine
void colSilverLight() {
    setMaterial(0.88f, 0.90f, 0.95f, 128, 1.0f, 1.0f, 1.0f);
    applyTexture(TEX_SILVER);
}

// ---- Spear shaft and engraving helpers ----
// colDarkShaft: very dark near-black for the spear pole body
void colDarkShaft() {
    setMaterial(0.13f, 0.12f, 0.15f, 32, 0.3f, 0.3f, 0.3f);
    glDisable(GL_TEXTURE_2D);
}

// colEngrave: dark steel-blue for decorative engravings on the blade surface
void colEngrave() {
    setMaterial(0.28f, 0.30f, 0.34f, 8, 0.1f, 0.1f, 0.1f);
    glDisable(GL_TEXTURE_2D);
}

// colCyanFaint: light cyan for the magical glow dots on the spear guard
void colCyanFaint() {
    setMaterial(0.62f, 0.90f, 0.95f, 16, 0.5f, 0.9f, 1.0f);
    glDisable(GL_TEXTURE_2D);
}

// ---- Utility material reset ----
// resetMat: removes specular highlight so subsequent glColor3f calls render flat
void resetMat() {
    GLfloat z[] = { 0, 0, 0, 1 };
    glMaterialfv(GL_FRONT, GL_SPECULAR, z);
    glMaterialf(GL_FRONT, GL_SHININESS, 0);
}

// ---- Temple flat-colour helpers ----
// colVermillion: bright Chinese red, used for temple wall accents (no texture)
void colVermillion() {
    glColor3f(0.89f, 0.26f, 0.20f);
    resetMat();
}

// colDarkVermillion: darker red for window lattice lines and shadow areas
void colDarkVermillion() {
    glColor3f(0.60f, 0.15f, 0.10f);
    resetMat();
}

// colGoldenYellow: bright yellow-gold, used for decorative flat gold pieces
void colGoldenYellow() {
    glColor3f(1.00f, 0.84f, 0.00f);
    GLfloat s[] = { 0.8f, 0.8f, 0.6f, 1 };
    glMaterialfv(GL_FRONT, GL_SPECULAR, s);
    glMaterialf(GL_FRONT, GL_SHININESS, 50);
}

// colGoldT: shiny gold used for temple ornaments, finial, lanterns
void colGoldT() {
    glColor3f(1.00f, 0.80f, 0.00f);
    GLfloat s[] = { 0.9f, 0.9f, 0.7f, 1 };
    glMaterialfv(GL_FRONT, GL_SPECULAR, s);
    glMaterialf(GL_FRONT, GL_SHININESS, 80);
}

// colBronze: warm bronze for the dragon coil on the pillar
void colBronze() {
    glColor3f(0.80f, 0.50f, 0.20f);
    GLfloat s[] = { 0.7f, 0.6f, 0.4f, 1 };
    glMaterialfv(GL_FRONT, GL_SPECULAR, s);
    glMaterialf(GL_FRONT, GL_SHININESS, 60);
}

// colTeal: flat teal, used for dragon pillar base colour when textures are off
void colTeal() {
    glColor3f(0.00f, 0.50f, 0.50f);
    resetMat();
}

// colBrown: dark brown for lantern poles and wooden accents
void colBrown() {
    glColor3f(0.38f, 0.20f, 0.08f);
    resetMat();
}

// colPearlWhite: near-white for stone highlights and lion details
void colPearlWhite() {
    glColor3f(0.95f, 0.95f, 0.95f);
    resetMat();
}

// colLightGrey: light grey for stone surfaces without texture
void colLightGrey() {
    glColor3f(0.85f, 0.85f, 0.85f);
    resetMat();
}

// colDarkStone: darker grey-brown for stone bases and guardian lion bodies
void colDarkStone() {
    glColor3f(0.40f, 0.37f, 0.32f);
    resetMat();
}

// ---- Bell gold helpers ----
// colGoldMain: primary bell body gold with high shininess
void colGoldMain() {
    setMaterial(0.90f, 0.72f, 0.18f, 96, 1.0f, 0.9f, 0.4f);
    applyTexture(TEX_GOLD);
}

// colGoldHigh: brightest gold for the bell's raised decorative ridges
void colGoldHigh() {
    setMaterial(1.00f, 0.90f, 0.45f, 128, 1.0f, 1.0f, 0.6f);
    applyTexture(TEX_GOLD);
}

// colGoldBell: darker antique gold tint for bell body (uses wood texture for warmth)
void colGoldBell() {
    setMaterial(0.55f, 0.35f, 0.08f, 48, 0.7f, 0.5f, 0.2f);
    applyTexture(TEX_WOOD);
}

// colGoldClapper: mid-range gold for the clapper rod and ball
void colGoldClapper() {
    setMaterial(0.75f, 0.58f, 0.12f, 80, 0.9f, 0.8f, 0.3f);
    applyTexture(TEX_BELLBODY);
}

// colBellBodyTex: main bell body material, uses roof texture for aged patina effect
void colBellBodyTex() {
    setMaterial(0.90f, 0.72f, 0.18f, 96, 1.0f, 0.9f, 0.4f);
    applyTexture(TEX_ROOF);
}

// colBellRimTex: bell rim/clapper accent, uses wood texture for contrast
void colBellRimTex() {
    setMaterial(1.00f, 0.90f, 0.45f, 128, 1.0f, 1.0f, 0.6f);
    applyTexture(TEX_WOOD);
}

// ============================================================
//  TEMPLE COLOUR HELPERS  
// ============================================================

// colRoof: golden-yellow tint + roof.bmp for temple roof tiles
void colRoof() {
    setMaterial(1.00f, 0.80f, 0.00f, 80, 0.9f, 0.9f, 0.7f);
    applyTexture(TEX_ROOF);
}

// colWall: vermillion red tint + TBody.bmp for temple wall panels
void colWall() {
    setMaterial(0.89f, 0.26f, 0.20f, 16, 0.2f, 0.1f, 0.1f);
    applyTexture(TEX_Body);
}

// colDoor: warm brown tint + wood.bmp for temple door panels
void colDoor() {
    setMaterial(0.38f, 0.20f, 0.08f, 24, 0.2f, 0.1f, 0.05f);
    applyTexture(TEX_WOOD);
}

// colStone: light grey + stone.bmp for platform tiers and steps
void colStone() {
    setMaterial(0.88f, 0.88f, 0.88f, 16, 0.1f, 0.1f, 0.1f);
    applyTexture(TEX_STONE);
}

// colStoneTrim: slightly darker stone for platform edge trim lines
// (avoids showing yellow trim when textures are off)
void colStoneTrim() {
    setMaterial(0.75f, 0.72f, 0.65f, 8, 0.05f, 0.05f, 0.05f);
    applyTexture(TEX_STONE);
}

// colPillarTex: teal tint + pillar.bmp (TEX_PILLAR) for the dragon pillar shaft
void colPillarTex() {
    setMaterial(0.00f, 0.50f, 0.50f, 32, 0.3f, 0.6f, 0.6f);
    applyTexture(TEX_PILLAR);
}

// colTempPillarShaft: vermillion tint + wood.bmp for the 4 front pillar shafts
void colTempPillarShaft() {
    setMaterial(0.85f, 0.22f, 0.18f, 24, 0.3f, 0.1f, 0.1f);
    applyTexture(TEX_WOOD);
}

// colTempPillarCap: darker red + roof.bmp for the pillar base and capital caps
void colTempPillarCap() {
    setMaterial(0.50f, 0.10f, 0.08f, 16, 0.2f, 0.1f, 0.1f);
    applyTexture(TEX_ROOF);
}

// ============================================================
//  PRIMITIVES 
// ============================================================

// ------------------------------------------------------------
// drawBox: Draws a solid axis-aligned box centred at the origin.
//   hx, hy, hz = half-extents along X, Y, Z respectively.
//   Each face is drawn as a quad with normals and tex-coords.
// ------------------------------------------------------------
void drawBox(float hx, float hy, float hz) {
    glBegin(GL_QUADS);
    // Front face (+Z)
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0); glVertex3f(-hx, -hy, hz);
    glTexCoord2f(1, 0); glVertex3f(hx, -hy, hz);
    glTexCoord2f(1, 1); glVertex3f(hx, hy, hz);
    glTexCoord2f(0, 1); glVertex3f(-hx, hy, hz);
    // Back face (-Z)
    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0); glVertex3f(hx, -hy, -hz);
    glTexCoord2f(1, 0); glVertex3f(-hx, -hy, -hz);
    glTexCoord2f(1, 1); glVertex3f(-hx, hy, -hz);
    glTexCoord2f(0, 1); glVertex3f(hx, hy, -hz);
    // Left face (-X)
    glNormal3f(-1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(-hx, -hy, -hz);
    glTexCoord2f(1, 0); glVertex3f(-hx, -hy, hz);
    glTexCoord2f(1, 1); glVertex3f(-hx, hy, hz);
    glTexCoord2f(0, 1); glVertex3f(-hx, hy, -hz);
    // Right face (+X)
    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(hx, -hy, hz);
    glTexCoord2f(1, 0); glVertex3f(hx, -hy, -hz);
    glTexCoord2f(1, 1); glVertex3f(hx, hy, -hz);
    glTexCoord2f(0, 1); glVertex3f(hx, hy, hz);
    // Top face (+Y)
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(-hx, hy, hz);
    glTexCoord2f(1, 0); glVertex3f(hx, hy, hz);
    glTexCoord2f(1, 1); glVertex3f(hx, hy, -hz);
    glTexCoord2f(0, 1); glVertex3f(-hx, hy, -hz);
    // Bottom face (-Y)
    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0); glVertex3f(-hx, -hy, -hz);
    glTexCoord2f(1, 0); glVertex3f(hx, -hy, -hz);
    glTexCoord2f(1, 1); glVertex3f(hx, -hy, hz);
    glTexCoord2f(0, 1); glVertex3f(-hx, -hy, hz);
    glEnd();
}

// ------------------------------------------------------------
// drawCylinderY: Draws a closed cylinder oriented along the Y-axis,
//                centred at the origin.
//   r      = radius
//   h      = total height
//   slices = number of sides (higher = smoother)
// Uses GLU quadrics so texture coordinates are generated automatically.
// ------------------------------------------------------------
void drawCylinderY(float r, float h, int slices = 20) {
    GLUquadric* q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    gluQuadricTexture(q, GL_TRUE); // enable automatic tex-coord generation

    glPushMatrix();
    // Translate so the cylinder is centred vertically at the origin
    glTranslatef(0, -h * 0.5f, 0);
    // GLU cylinders are aligned along Z; rotate -90 deg around X to align with Y
    glRotatef(-90, 1, 0, 0);
    gluCylinder(q, r, r, h, slices, 4);
    // Bottom disk cap
    gluDisk(q, 0, r, slices, 1);
    // Top disk cap: translate to the top end
    glTranslatef(0, 0, h);
    gluDisk(q, 0, r, slices, 1);
    glPopMatrix();

    gluDeleteQuadric(q);
}

// ------------------------------------------------------------
// drawConeY: Draws a truncated cone (frustum) along the Y-axis.
//   baseR  = radius at the bottom
//   tipR   = radius at the top (0 = sharp point)
//   h      = total height
//   slices = smoothness
// ------------------------------------------------------------
void drawConeY(float baseR, float tipR, float h, int slices = 18) {
    GLUquadric* q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    gluQuadricTexture(q, GL_TRUE);

    glPushMatrix();
    glTranslatef(0, -h * 0.5f, 0);
    glRotatef(-90, 1, 0, 0);
    // gluCylinder with different top/bottom radii creates a cone/frustum
    gluCylinder(q, baseR, tipR, h, slices, 4);
    glPopMatrix();

    gluDeleteQuadric(q);
}

// ------------------------------------------------------------
// drawSphere: Draws a UV-sphere centred at the origin.
//   r  = radius
//   sl = number of stacks/slices (higher = smoother)
// ------------------------------------------------------------
void drawSphere(float r, int sl = 16) {
    GLUquadric* q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    gluQuadricTexture(q, GL_TRUE);
    gluSphere(q, r, sl, sl);
    gluDeleteQuadric(q);
}

// ------------------------------------------------------------
// drawTrapezoidY: Draws a tapered rectangular prism (trapezoid cross-section)
//                 aligned along Y, wider at the bottom than the top.
//   topHX = top half-width along X
//   botHX = bottom half-width along X
//   hy    = half-height along Y
//   hz    = half-depth along Z (same top and bottom)
// Used for shoulder pads, armour panels, and other tapered shapes.
// ------------------------------------------------------------
void drawTrapezoidY(float topHX, float botHX, float hy, float hz) {
    glBegin(GL_QUADS);
    // Front face (+Z): wider at bottom, narrower at top
    glNormal3f(0, 0, 1);
    glTexCoord2f(0, 0); glVertex3f(-botHX, -hy, hz);
    glTexCoord2f(1, 0); glVertex3f(botHX, -hy, hz);
    glTexCoord2f(1, 1); glVertex3f(topHX, hy, hz);
    glTexCoord2f(0, 1); glVertex3f(-topHX, hy, hz);
    // Back face (-Z)
    glNormal3f(0, 0, -1);
    glTexCoord2f(0, 0); glVertex3f(botHX, -hy, -hz);
    glTexCoord2f(1, 0); glVertex3f(-botHX, -hy, -hz);
    glTexCoord2f(1, 1); glVertex3f(-topHX, hy, -hz);
    glTexCoord2f(0, 1); glVertex3f(topHX, hy, -hz);
    // Left face (-X): slanted because botHX != topHX
    glNormal3f(-1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(-botHX, -hy, -hz);
    glTexCoord2f(1, 0); glVertex3f(-botHX, -hy, hz);
    glTexCoord2f(1, 1); glVertex3f(-topHX, hy, hz);
    glTexCoord2f(0, 1); glVertex3f(-topHX, hy, -hz);
    // Right face (+X)
    glNormal3f(1, 0, 0);
    glTexCoord2f(0, 0); glVertex3f(botHX, -hy, hz);
    glTexCoord2f(1, 0); glVertex3f(botHX, -hy, -hz);
    glTexCoord2f(1, 1); glVertex3f(topHX, hy, -hz);
    glTexCoord2f(0, 1); glVertex3f(topHX, hy, hz);
    // Top face (+Y): narrower rectangle
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(-topHX, hy, hz);
    glTexCoord2f(1, 0); glVertex3f(topHX, hy, hz);
    glTexCoord2f(1, 1); glVertex3f(topHX, hy, -hz);
    glTexCoord2f(0, 1); glVertex3f(-topHX, hy, -hz);
    // Bottom face (-Y): wider rectangle
    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0); glVertex3f(-botHX, -hy, -hz);
    glTexCoord2f(1, 0); glVertex3f(botHX, -hy, -hz);
    glTexCoord2f(1, 1); glVertex3f(botHX, -hy, hz);
    glTexCoord2f(0, 1); glVertex3f(-botHX, -hy, hz);
    glEnd();
}

// ------------------------------------------------------------
// drawDiskY: Draws a flat annular disk (ring) in the XZ plane.
//   innerR = inner hole radius
//   outerR = outer edge radius
//   slices = smoothness
// Used for belt rings, pillar collars, bell decorations.
// ------------------------------------------------------------
void drawDiskY(float innerR, float outerR, int slices = 24) {
    GLUquadric* q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    gluQuadricTexture(q, GL_TRUE);

    glPushMatrix();
    // GLU disk lies in the XY plane; rotate -90 deg around X to lay it flat on XZ
    glRotatef(-90, 1, 0, 0);
    gluDisk(q, innerR, outerR, slices, 2);
    glPopMatrix();

    gluDeleteQuadric(q);
}

// ============================================================
//  TIME OF DAY
// ============================================================

// SkyColors: holds all colour and lighting values for a specific time of day.
// These are interpolated into the sky gradient, sun colour, and scene light.
struct SkyColors {
    // Sky gradient: top = zenith colour, horiz = horizon colour
    float topR, topG, topB, horizR, horizG, horizB;
    // Ambient light (fills shadows), diffuse light (main directional light)
    float ambR, ambG, ambB, diffR, diffG, diffB;
    // Sun/moon disc colour, azimuth (compass bearing) and elevation (height angle)
    float sunR, sunG, sunB, lightAz, lightEl;
    // isSun: true = draw a sun disc, false = draw a smaller moon disc
    bool isSun;
};

// ------------------------------------------------------------
// getTimeOfDayColors: Returns a SkyColors struct for the given
//                     time-of-day index (0=Morning, 1=Afternoon,
//                     2=Evening, 3=Night).
// ------------------------------------------------------------
SkyColors getTimeOfDayColors(int tod) {
    SkyColors c;
    switch (tod) {
    case 0: // Morning: warm orange horizon, low sun from the east
        c.topR = 0.53f;  c.topG = 0.73f;  c.topB = 0.98f;
        c.horizR = 0.99f; c.horizG = 0.75f; c.horizB = 0.50f;
        c.ambR = 0.35f;  c.ambG = 0.30f;  c.ambB = 0.25f;
        c.diffR = 1.0f;  c.diffG = 0.85f; c.diffB = 0.70f;
        c.sunR = 1.0f;   c.sunG = 0.90f;  c.sunB = 0.60f;
        c.lightAz = 110; c.lightEl = 15;   c.isSun = true;
        break;

    case 1: // Afternoon: bright blue sky, overhead white sun
        c.topR = 0.28f;  c.topG = 0.55f;  c.topB = 0.92f;
        c.horizR = 0.65f; c.horizG = 0.82f; c.horizB = 1.0f;
        c.ambR = 0.40f;  c.ambG = 0.40f;  c.ambB = 0.45f;
        c.diffR = 1.0f;  c.diffG = 1.0f;  c.diffB = 1.0f;
        c.sunR = 1.0f;   c.sunG = 0.97f;  c.sunB = 0.85f;
        c.lightAz = 45;  c.lightEl = 65;   c.isSun = true;
        break;

    case 2: // Evening: deep orange/red horizon, low western sun
        c.topR = 0.18f;  c.topG = 0.18f;  c.topB = 0.45f;
        c.horizR = 1.0f; c.horizG = 0.42f; c.horizB = 0.08f;
        c.ambR = 0.35f;  c.ambG = 0.20f;  c.ambB = 0.15f;
        c.diffR = 1.0f;  c.diffG = 0.65f; c.diffB = 0.30f;
        c.sunR = 1.0f;   c.sunG = 0.45f;  c.sunB = 0.10f;
        c.lightAz = 250; c.lightEl = 12;   c.isSun = true;
        break;

    default: // Night: near-black sky, cool blue moonlight
        c.topR = 0.05f;  c.topG = 0.05f;  c.topB = 0.15f;
        c.horizR = 0.10f; c.horizG = 0.10f; c.horizB = 0.25f;
        c.ambR = 0.35f;  c.ambG = 0.35f;  c.ambB = 0.45f;
        c.diffR = 0.60f; c.diffG = 0.65f; c.diffB = 0.80f;
        c.sunR = 0.90f;  c.sunG = 0.95f;  c.sunB = 1.0f;
        c.lightAz = 200; c.lightEl = 35;   c.isSun = false;
        break;
    }
    return c;
}

// ============================================================
//  FLAME
// ============================================================

// ------------------------------------------------------------
// drawFlame: Draws a layered particle flame effect at world position (x,y,z).
//   scale = size multiplier
//   time  = current animation time (drives flicker and rise speed)
// The flame uses additive blending for a glowing look, with 5 layers:
//   layer 0 = bright white core, layers 1-2 = orange mid, layers 3-4 = blue outer
// ------------------------------------------------------------
void drawFlame(float x, float y, float z, float scale, float time) {
    // Disable lighting so the flame colours are pure/unlit
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    // Additive blend: flame colour adds on top of what's behind (glow effect)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // Don't write to the depth buffer so the flame overlaps correctly
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);

    for (int i = 0; i < 5; i++) {
        // Each layer offsets in time to stagger the rise animation
        float offset = fmod(time * 10.0f + i * 0.2f, 1.0f);
        float s = scale * (1.0f - offset); // shrinks as it rises

        // Colour layers: white core -> orange mid -> blue outer glow
        if (i == 0)       glColor4f(1, 1, 1, 0.8f);          // white core
        else if (i < 3)   glColor4f(1, 0.8f, 0.2f, 0.6f);   // orange
        else              glColor4f(0.2f, 0.6f, 1, 0.3f);    // blue outer

        glPushMatrix();
        // Slight horizontal flicker (sinf gives side-to-side wobble)
        glTranslatef(x + sinf(time * 15 + i) * 0.2f, y + offset * scale * 4, z);
        // Flame is tall and thin: scale Y larger than X/Z
        glScalef(s * 0.5f, s * 1.5f, s * 0.5f);
        drawSphere(1, 8);
        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // Restore lighting if it was active before this call
    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ============================================================
//  HANDS
// ============================================================

// ------------------------------------------------------------
// drawFinger: Draws a 3-segment finger (base phalanx, middle, tip).
//   curlBase/Mid/Tip = bend angles (degrees) for each joint
//   scale            = overall size multiplier
// Each phalanx is a small box; joints rotate around the X-axis (curl direction).
// ------------------------------------------------------------
void drawFinger(float curlBase, float curlMid, float curlTip, float scale) {
    glPushMatrix();
    // Base knuckle rotation
    glRotatef(curlBase, 1, 0, 0);
    glTranslatef(0, -0.3f * scale, 0);
    drawBox(0.15f * scale, 0.3f * scale, 0.15f * scale); // base phalanx

    // Middle joint
    glTranslatef(0, -0.3f * scale, 0);
    glRotatef(curlMid, 1, 0, 0);
    glTranslatef(0, -0.25f * scale, 0);
    drawBox(0.12f * scale, 0.25f * scale, 0.12f * scale); // middle phalanx

    // Tip joint
    glTranslatef(0, -0.25f * scale, 0);
    glRotatef(curlTip, 1, 0, 0);
    glTranslatef(0, -0.2f * scale, 0);
    drawBox(0.1f * scale, 0.2f * scale, 0.1f * scale); // fingertip phalanx
    glPopMatrix();
}

// ------------------------------------------------------------
// drawThumb: Draws a 2-segment thumb (base, tip).
// Shorter and wider than a finger; only 2 joints.
// ------------------------------------------------------------
void drawThumb(float curlBase, float curlTip, float scale) {
    glPushMatrix();
    glRotatef(curlBase, 1, 0, 0);
    glTranslatef(0, -0.35f * scale, 0);
    drawBox(0.18f * scale, 0.35f * scale, 0.18f * scale); // base segment

    glTranslatef(0, -0.35f * scale, 0);
    glRotatef(curlTip, 1, 0, 0);
    glTranslatef(0, -0.25f * scale, 0);
    drawBox(0.15f * scale, 0.25f * scale, 0.15f * scale); // tip segment
    glPopMatrix();
}

// ------------------------------------------------------------
// drawComplexHand: Draws the full 5-finger hand with smooth
//                  open <-> fist blending.
//   blend = 0.0 = fully open hand, 1.0 = fully closed fist
//   side  = +1 for right hand, -1 for left hand (mirrors thumb position)
// The finger curl angles are linearly interpolated between open and fist poses.
// ------------------------------------------------------------
void drawComplexHand(float blend, float side) {
    colSkin();
    float scale = 1.3f;
    // Clamp blend to [0, 1]
    float t = fminf(fmaxf(blend, 0), 1);

    // Interpolate curl angles: open (10/5/5) -> fist (85/105/55)
    float fBase = 10 * (1 - t) + 85 * t;
    float fMid = 5 * (1 - t) + 105 * t;
    float fTip = 5 * (1 - t) + 55 * t;
    // Thumb: open (20/10) -> fist (55/60)
    float tBase = 20 * (1 - t) + 55 * t;
    float tTip = 10 * (1 - t) + 60 * t;

    glPushMatrix();
    // Draw 4 fingers, spread apart with slight offsets to look natural
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        float xOff = (i - 1.5f) * 0.25f * scale; // spread across the palm
        glTranslatef(xOff, -fabsf(i - 1.5f) * 0.04f, 0);
        glRotatef(xOff * 18, 0, 1, 0); // slight fan angle

        drawBox(0.13f * scale, 0.5f * scale, 0.2f * scale); // palm knuckle section
        glTranslatef(0, -0.5f * scale, 0);
        drawFinger(fBase, fMid, fTip, scale);
        glPopMatrix();
    }

    // Draw thumb on the outer side of the palm
    glPushMatrix();
    if (side > 0) {
        // Right hand: thumb on the left side of the palm
        glTranslatef(-0.6f * scale, 0.2f * scale, 0.2f * scale);
        glRotatef(-45, 0, 0, 1);
        glRotatef(-30, 0, 1, 0);
    }
    else {
        // Left hand: thumb on the right side of the palm
        glTranslatef(0.6f * scale, 0.2f * scale, 0.2f * scale);
        glRotatef(45, 0, 0, 1);
        glRotatef(30, 0, 1, 0);
    }
    drawThumb(tBase, tTip, scale);
    glPopMatrix();
    glPopMatrix();
}

// ------------------------------------------------------------
// drawGrippingHand: Draws the hand locked in a firm grip pose
//                   (used when Yang Xuan is holding the spear).
// Finger angles are fixed at gripping values; thumb wraps over fingers.
// ------------------------------------------------------------
void drawGrippingHand(float side) {
    colSkin();
    float scale = 1.3f;
    // Fixed grip angles for all 4 fingers
    float fBase = 85, fMid = 100, fTip = 60;
    // Fixed thumb wrap angle
    float tBase = 50, tTip = 65;

    glPushMatrix();
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        float xOff = (i - 1.5f) * 0.25f * scale;
        glTranslatef(xOff, -fabsf(i - 1.5f) * 0.04f, 0);
        drawBox(0.13f * scale, 0.5f * scale, 0.2f * scale);
        glTranslatef(0, -0.5f * scale, 0);
        drawFinger(fBase, fMid, fTip, scale);
        glPopMatrix();
    }
    // Thumb wraps around the shaft from the opposite side
    glPushMatrix();
    glTranslatef(side * -0.6f * scale, 0.2f * scale, 0.2f * scale);
    glRotatef(side * 45, 0, 0, 1);
    glRotatef(side * 30, 0, 1, 0);
    drawThumb(tBase, tTip, scale);
    glPopMatrix();
    glPopMatrix();
}

// ============================================================
//  SPEAR WEAPON
// ============================================================

// ------------------------------------------------------------
// drawCenterBlade: Draws the main centre blade of the spear.
//   halfW = half-width of the widest point
//   halfH = half-height (from tip to base)
//   thick = maximum blade thickness at the spine
// Uses a custom leaf-shaped profile: wide in the middle, tapering to
// a point at the tip and narrowing to a tang at the base.
// ------------------------------------------------------------
void drawCenterBlade(float halfW, float halfH, float thick) {
    const int segs = 28;
    float tipY = halfH, baseY = -halfH;

    // Width profile: rises quickly to max, then tapers slowly to the base
    auto profile = [&](float t)->float {
        if (t < 0.38f) return sinf(t / 0.38f * PI * 0.5f);
        else           return sinf((1 - (t - 0.38f) / 0.62f) * PI * 0.5f) * 0.38f;
        };

    // Thickness profile: full spine thickness in the middle, thins towards tip and base
    auto thickP = [&](float t)->float {
        float f1 = sinf(fminf(t / 0.35f, 1) * PI * 0.5f);
        float f2 = (t < 0.90f) ? 1 : (1 - (t - 0.90f) / 0.10f);
        return f1 * f2;
        };

    // Front face of blade (+Z side)
    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(0, 0.1f, 1);
    glVertex3f(0, tipY, 0);
    glVertex3f(0, tipY, 0);
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / segs;
        float y = tipY - t * (tipY - baseY);
        float w = profile(t) * halfW;
        float tz2 = thickP(t) * thick;
        glVertex3f(-w, y, tz2);
        glVertex3f(w, y, tz2);
    }
    glEnd();

    // Back face of blade (-Z side)
    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(0, 0.1f, -1);
    glVertex3f(0, tipY, 0);
    glVertex3f(0, tipY, 0);
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / segs;
        float y = tipY - t * (tipY - baseY);
        float w = profile(t) * halfW;
        float tz2 = thickP(t) * thick;
        glVertex3f(w, y, -tz2);
        glVertex3f(-w, y, -tz2);
    }
    glEnd();

    // Left edge of blade
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(0, tipY, 0);
    glVertex3f(0, tipY, 0);
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / segs;
        float y = tipY - t * (tipY - baseY);
        float w = profile(t) * halfW;
        float tz2 = thickP(t) * thick;
        glNormal3f(-1, 0, 0);
        glVertex3f(-w, y, -tz2);
        glVertex3f(-w, y, tz2);
    }
    glEnd();

    // Right edge of blade
    glBegin(GL_TRIANGLE_STRIP);
    glVertex3f(0, tipY, 0);
    glVertex3f(0, tipY, 0);
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / segs;
        float y = tipY - t * (tipY - baseY);
        float w = profile(t) * halfW;
        float tz2 = thickP(t) * thick;
        glNormal3f(1, 0, 0);
        glVertex3f(w, y, tz2);
        glVertex3f(w, y, -tz2);
    }
    glEnd();

    // Bottom base cap of the blade
    float bw = profile(1) * halfW, bt = thickP(1) * thick;
    glBegin(GL_QUADS);
    glNormal3f(0, -1, 0);
    glVertex3f(-bw, baseY, -bt);
    glVertex3f(bw, baseY, -bt);
    glVertex3f(bw, baseY, bt);
    glVertex3f(-bw, baseY, bt);
    glEnd();
}

// ------------------------------------------------------------
// drawSideBlade: Draws one curved crescent side blade on the spear.
//   side      = +1 for right blade, -1 for left blade
//   attachY   = Y position where the blade base meets the spear shaft
//   bladeLen  = length of the curved blade
//   bladeW    = maximum width of the blade
//   thick     = blade thickness at the spine
// The blade curves outward using polar coordinates: angBase sets the
// attachment angle and angSweep controls how much it curves.
// ------------------------------------------------------------
void drawSideBlade(float side, float attachY, float bladeLen, float bladeW, float thick) {
    const int segs = 20;
    // Base angle and sweep define the arc of the curved blade
    float angBase = side * 28 * PI / 180;
    float angSweep = side * 18 * PI / 180;

    // Width profile: narrows at the base and tip, widest in the middle
    auto widthP = [&](float t)->float {
        if (t < 0.32f) return t / 0.32f;
        else           return 1 - (t - 0.32f) / 0.68f;
        };

    // Thickness profile: full spine thickness early on, thins toward tip
    auto thickP = [&](float t)->float {
        return sinf(fminf(t / 0.28f, 1) * PI * 0.5f) * (1 - t * 0.72f);
        };

    // X position along the arc using polar sweep
    auto spX = [&](float t)->float {
        return side * t * bladeLen * cosf(angBase + angSweep * t);
        };

    // Y position: rises as the arc sweeps outward
    auto spY = [&](float t)->float {
        return attachY + t * bladeLen * fabsf(sinf(angBase + angSweep * t));
        };

    // Front face of side blade
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segs; i++) {
        float t = (float)i / segs;
        float cx = spX(t), cy = spY(t);
        float w = widthP(t) * bladeW;
        float tz2 = thickP(t) * thick;
        // Compute tangent for the normal (perpendicular to blade edge)
        float dt = 0.02f;
        float tx2 = spX(fminf(t + dt, 1)), ty2 = spY(fminf(t + dt, 1));
        float ex = -(ty2 - cy), ey = (tx2 - cx);
        float mag = sqrtf(ex * ex + ey * ey);
        if (mag > 0) { ex /= mag; ey /= mag; }
        glNormal3f(ex * 0.25f, ey * 0.25f, 1);
        glVertex3f(cx + ex * w * 0.55f, cy + ey * w * 0.55f, tz2);
        glVertex3f(cx - ex * w * 0.45f, cy - ey * w * 0.45f, tz2);
    }
    glEnd();

    // Back face of side blade
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segs; i++) {
        float t = (float)i / segs;
        float cx = spX(t), cy = spY(t);
        float w = widthP(t) * bladeW;
        float tz2 = thickP(t) * thick;
        float dt = 0.02f;
        float tx2 = spX(fminf(t + dt, 1)), ty2 = spY(fminf(t + dt, 1));
        float ex = -(ty2 - cy), ey = (tx2 - cx);
        float mag = sqrtf(ex * ex + ey * ey);
        if (mag > 0) { ex /= mag; ey /= mag; }
        glNormal3f(-ex * 0.25f, -ey * 0.25f, -1);
        glVertex3f(cx - ex * w * 0.45f, cy - ey * w * 0.45f, -tz2);
        glVertex3f(cx + ex * w * 0.55f, cy + ey * w * 0.55f, -tz2);
    }
    glEnd();
}

// ------------------------------------------------------------
// drawBladeEngravings: Draws decorative line engravings on the
//                      blade surface using GL_LINES.
// The lines simulate a central ridge and diagonal rune-like markings.
// ------------------------------------------------------------
void drawBladeEngravings(float halfH, float halfW) {
    glDisable(GL_LIGHTING);
    colEngrave();
    glLineWidth(1.3f);

    float tipY = halfH, baseY = -halfH;
    float zOff = 0.32f; // draw engravings slightly in front of the blade face

    // Central spine line running most of the blade length
    glBegin(GL_LINES);
    glVertex3f(0, tipY * 0.93f, zOff);
    glVertex3f(0, baseY * 0.40f, zOff);
    glEnd();

    // Diagonal rune bands across the blade width at 5 height positions
    float yBands[] = { halfH * 0.58f, halfH * 0.30f, halfH * 0.05f, -halfH * 0.14f, -halfH * 0.27f };
    float wBands[] = { halfW * 0.18f, halfW * 0.38f, halfW * 0.52f,  halfW * 0.50f,  halfW * 0.40f };

    glBegin(GL_LINES);
    for (int i = 0; i < 5; i++) {
        // Left diagonal stroke
        glVertex3f(0, yBands[i], zOff);
        glVertex3f(-wBands[i], yBands[i] - halfH * 0.09f, zOff);
        // Right diagonal stroke (mirror)
        glVertex3f(0, yBands[i], zOff);
        glVertex3f(wBands[i], yBands[i] - halfH * 0.09f, zOff);
    }
    glEnd();

    glLineWidth(1);
    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ------------------------------------------------------------
// drawGuard: Draws the ornate cross-guard of the spear.
//   centerY  = vertical centre position of the guard
//   outerR   = outer radius of the guard disc
//   height   = total guard height
// Consists of stacked cylinders and decorative knobs at 4 cardinal positions.
// ------------------------------------------------------------
void drawGuard(float centerY, float outerR, float height) {
    // Lower tapered ring
    colGoldDark();
    glPushMatrix();
    glTranslatef(0, centerY - height * 0.55f, 0);
    drawCylinderY(outerR * 0.92f, height * 0.30f, 24);
    glPopMatrix();

    // Main guard disc (widest part)
    colGold();
    glPushMatrix();
    glTranslatef(0, centerY, 0);
    drawCylinderY(outerR, height * 0.45f, 24);
    glPopMatrix();

    // Upper tapered ring
    colGoldLight();
    glPushMatrix();
    glTranslatef(0, centerY + height * 0.52f, 0);
    drawCylinderY(outerR * 0.78f, height * 0.25f, 24);
    glPopMatrix();

    // Wide flat flange ring (the prominent outer ring of the guard)
    colGold();
    glPushMatrix();
    glTranslatef(0, centerY + height * 0.06f, 0);
    drawCylinderY(outerR * 1.32f, height * 0.17f, 28);
    glPopMatrix();

    // Dark edge trim on the flange
    colGoldDark();
    glPushMatrix();
    glTranslatef(0, centerY + height * 0.06f, 0);
    drawCylinderY(outerR * 1.33f, height * 0.055f, 28);
    glPopMatrix();

    // 4 decorative knobs at North/South/East/West of the guard
    float curlAngles[] = { 0, 90, 180, 270 };
    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glTranslatef(0, centerY, 0);
        glRotatef(curlAngles[i], 0, 1, 0);

        colGold();
        glTranslatef(0, 0, outerR * 1.12f);
        drawBox(0.52f, height * 0.33f, 0.32f); // rectangular knob body

        // Glowing sphere tip on each knob
        colGoldLight();
        glTranslatef(0, height * 0.28f, 0.22f);
        drawSphere(0.26f, 8);
        glPopMatrix();
    }

    // Cyan glow dots along the guard edge (magical rune effect)
    glDisable(GL_LIGHTING);
    colCyanFaint();
    glPointSize(3.2f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 18; i++) {
        float a = (float)i / 18 * 2 * PI;
        glVertex3f(cosf(a) * outerR * 1.315f, centerY + height * 0.06f, sinf(a) * outerR * 1.315f);
    }
    glEnd();
    glPointSize(1);
    if (lightMode > 0) glEnable(GL_LIGHTING);

    // Small neck collar above the main guard
    colGoldDark();
    glPushMatrix();
    glTranslatef(0, centerY + height * 0.92f, 0);
    drawCylinderY(outerR * 0.60f, height * 0.18f, 20);
    glPopMatrix();

    // Transition ring connecting guard neck to blade base
    colGold();
    glPushMatrix();
    glTranslatef(0, centerY + height * 1.16f, 0);
    drawCylinderY(outerR * 0.50f, height * 0.14f, 20);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawShaft: Draws the spear pole (shaft) from botY to topY.
//   botY   = world Y of the butt end
//   topY   = world Y of the guard end
//   radius = pole radius
// The shaft body is dark, with 7 evenly spaced gold band rings
// and a dark collar just below the guard.
// ------------------------------------------------------------
void drawShaft(float botY, float topY, float radius) {
    float h = topY - botY; // total shaft length

    // Dark pole body
    colDarkShaft();
    glPushMatrix();
    glTranslatef(0, botY + h * 0.5f, 0);
    drawCylinderY(radius, h, 20);
    glPopMatrix();

    // 7 equally spaced gold decorative bands along the lower half of the shaft
    colGold();
    for (int i = 1; i <= 7; i++) {
        float bandY = botY + h * 0.04f + ((float)i / 8) * h * 0.56f;
        glPushMatrix();
        glTranslatef(0, bandY, 0);
        drawCylinderY(radius + 0.038f, 0.14f, 16); // slightly wider than shaft
        glPopMatrix();
    }

    // Dark collar ring near the top of the shaft (just below the guard)
    colGoldDark();
    glPushMatrix();
    glTranslatef(0, topY - 0.18f, 0);
    drawCylinderY(radius * 1.55f, 0.28f, 16);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawButtSpike: Draws the pointed butt cap at the bottom end of the spear.
//   botY   = Y position of the bottom of the butt
//   height = length of the spike
//   radius = shaft radius (used to scale the base of the spike)
// ------------------------------------------------------------
void drawButtSpike(float botY, float height, float radius) {
    // Wide base collar at the butt
    colGoldDark();
    glPushMatrix();
    glTranslatef(0, botY + 0.10f, 0);
    drawCylinderY(radius * 1.80f, 0.22f, 16);
    glPopMatrix();

    // Short tapered base cylinder
    colGold();
    glPushMatrix();
    glTranslatef(0, botY - height * 0.10f, 0);
    drawCylinderY(radius * 1.55f, height * 0.25f, 16);
    glPopMatrix();

    // Sharp conical point
    colGoldLight();
    glPushMatrix();
    glTranslatef(0, botY - height * 0.58f, 0);
    drawConeY(radius * 1.0f, 0.03f, height * 0.72f, 14);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawSpear: Assembles the complete three-pointed
//            double-edged spear 
//   animTime = current animation time, used to position fire effects
//
// Structure (bottom to top):
//   butt spike -> shaft -> guard -> centre blade + side blades
//   Optional divine fire rings on blade tips when isFireActive is true.
// ------------------------------------------------------------
void drawSpear(float animTime) {
    // Spear geometry constants
    const float SHAFT_RADIUS = 0.40f;
    const float SHAFT_BOT = -14;       // bottom of the shaft (butt end)
    const float SHAFT_TOP = 16;        // top of the shaft (guard end)
    const float GUARD_CTR = 10.5f;     // guard centre Y position
    const float GUARD_OUTER = 2.20f;     // guard disc outer radius
    const float GUARD_H = 4.20f;     // guard total height

    const float BLADE_BOT_Y = 15.8f;    // blade starts just above the guard
    const float BLADE_TIP_Y = 27.8f;    // blade tip position
    const float BLADE_HALF_H = (BLADE_TIP_Y - BLADE_BOT_Y) * 0.5f; // half blade height
    const float BLADE_CTR_Y = BLADE_BOT_Y + BLADE_HALF_H;          // blade centre Y

    const float BLADE_HALF_W = 1.55f;    // blade half-width at widest point
    const float BLADE_THICK = 0.30f;    // blade spine thickness

    // Side blade parameters
    const float SIDE_ATTACH_Y = BLADE_BOT_Y + 0.80f; // where side blades attach on the shaft
    const float SIDE_LEN = 6.50f;                // length of each side blade arc
    const float SIDE_W = 1.10f;                // side blade width
    const float SIDE_THICK = 0.22f;                // side blade thickness

    // Scale down slightly so the spear fits the character's hand proportions
    glPushMatrix();
    glScalef(0.85f, 0.85f, 0.85f);

    drawShaft(SHAFT_BOT, SHAFT_TOP, SHAFT_RADIUS);
    drawGuard(GUARD_CTR, GUARD_OUTER, GUARD_H);

    // Connecting collar between guard top and blade base
    colGoldDark();
    glPushMatrix();
    glTranslatef(0, GUARD_CTR + GUARD_H * 1.35f, 0);
    drawCylinderY(SHAFT_RADIUS * 1.75f, 1.10f, 16);
    glPopMatrix();

    // Centre blade (silver) with its engraving overlay
    colSilver();
    glPushMatrix();
    glTranslatef(0, BLADE_CTR_Y, 0);
    drawCenterBlade(BLADE_HALF_W, BLADE_HALF_H, BLADE_THICK);

    // Bright highlight ridge running down the blade spine
    colSilverLight();
    glPushMatrix();
    glTranslatef(0, 0, BLADE_THICK * 0.88f);
    drawBox(BLADE_HALF_W * 0.10f, BLADE_HALF_H * 0.86f, 0.01f);
    glPopMatrix();
    glPopMatrix();

    // Engravings drawn on top of the centre blade
    glPushMatrix();
    glTranslatef(0, BLADE_CTR_Y, 0);
    drawBladeEngravings(BLADE_HALF_H, BLADE_HALF_W);
    glPopMatrix();

    // Right and left curved side blades
    colSilver();
    drawSideBlade(1, SIDE_ATTACH_Y, SIDE_LEN, SIDE_W, SIDE_THICK);
    drawSideBlade(-1, SIDE_ATTACH_Y, SIDE_LEN, SIDE_W, SIDE_THICK);

    drawButtSpike(SHAFT_BOT, 3.20f, SHAFT_RADIUS);

    // Divine fire flames on blade tips when isFireActive is true (key '1')
    if (isFireActive) {
        // Compute where the right side blade tip lands in local space
        float sX = SIDE_LEN * cosf((28 + 18) * PI / 180);
        float sY = SIDE_ATTACH_Y + SIDE_LEN * sinf((28 + 18) * PI / 180);

        drawFlame(0, BLADE_TIP_Y + 1, 0, 1.5f, animTime); // centre tip flame
        drawFlame(sX, sY + 1, 0, 1.0f, animTime); // right tip flame
        drawFlame(-sX, sY + 1, 0, 1.0f, animTime); // left tip flame
    }
    glPopMatrix();
}

// ============================================================
//  BELL WEAPON GEOMETRY
// ============================================================

// Bell geometry constants (all in local bell-space units)
const float B_RING_R = 7;      // radius of the hanging ring torus
const float B_RING_TUBE = 2.5f;   // cross-section radius of the ring tube
const float B_CONN_H = 12;     // height of the neck connector below the ring
const float B_CONN_R = 4;      // radius of the neck connector cylinder

const float B_DOME_R = 28;     // hemisphere dome radius at the bell crown
const float B_BODY_TOP_R = 28;     // bell body Bezier control: top radius
const float B_BODY_MID_R = 22;     // bell body Bezier control: mid-waist radius (narrowest)
const float B_BODY_BOT_R = 38;     // bell body Bezier control: bottom flare radius

const float B_BODY_H = 55;     // total height of the bell body cylinder
const float B_RIM_INNER_R = 36;     // inner radius of the bell mouth rim
const float B_RIM_OUTER_R = 42;     // outer radius of the bell mouth rim (widest)
const float B_RIM_H = 6;      // height/thickness of the mouth rim

const float B_CLAPPER_ROD = 38;    // length of the clapper rod hanging inside the bell
const float B_CLAPPER_BALL = 6;     // radius of the clapper ball at the rod tip

// ------------------------------------------------------------
// drawBellHangingRing: Draws the torus-like hanging loop at the top of the bell.
//   ringR    = radius of the ring circle
//   tubeR    = radius of the tube (cross-section)
//   segments = number of short cylinders approximating the torus
// ------------------------------------------------------------
void drawBellHangingRing(float ringR, float tubeR, int segments = 24) {
    colBellRimTex();
    float segLen = 2 * PI * ringR / segments; // arc length per segment

    for (int i = 0; i < segments; i++) {
        // Midpoint angle of this segment on the ring circle
        float a0 = (float)i / segments * 2 * PI;
        float a1 = (float)(i + 1) / segments * 2 * PI;
        float am = (a0 + a1) * 0.5f;

        glPushMatrix();
        // Position the small cylinder at the midpoint of this ring arc
        glTranslatef(cosf(am) * ringR, sinf(am) * ringR, 0);
        // Orient the cylinder tangent to the ring
        glRotatef(am * 180 / PI, 0, 0, 1);
        glRotatef(90, 0, 1, 0);
        drawCylinderY(tubeR, segLen + 0.5f, 8); // slight overlap prevents gaps
        glPopMatrix();
    }
}

// ------------------------------------------------------------
// drawBellDome: Draws the hemispherical crown dome at the top of the bell body.
//   r      = hemisphere radius
//   stacks = vertical subdivisions (higher = rounder)
//   slices = horizontal subdivisions
// Uses a quad-strip per stack band to approximate the hemisphere.
// ------------------------------------------------------------
void drawBellDome(float r, int stacks = 8, int slices = 24) {
    colBellBodyTex();
    for (int i = 0; i < stacks; i++) {
        // Map stack index to polar angle (0 = equator, PI/2 = top)
        float p0 = (float)i / stacks * PI * 0.5f;
        float p1 = (float)(i + 1) / stacks * PI * 0.5f;
        float y0 = sinf(p0) * r, r0 = cosf(p0) * r;
        float y1 = sinf(p1) * r, r1 = cosf(p1) * r;

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float a = (float)j / slices * 2 * PI;
            glTexCoord2f((float)j / slices, (float)i / stacks);
            glNormal3f(cosf(p0) * cosf(a), sinf(p0), cosf(p0) * sinf(a));
            glVertex3f(r0 * cosf(a), y0, r0 * sinf(a));

            glTexCoord2f((float)j / slices, (float)(i + 1) / stacks);
            glNormal3f(cosf(p1) * cosf(a), sinf(p1), cosf(p1) * sinf(a));
            glVertex3f(r1 * cosf(a), y1, r1 * sinf(a));
        }
        glEnd();
    }
}

// ------------------------------------------------------------
// drawBellBody: Draws the main cylindrical body of the bell,
//               shaped by a quadratic Bezier curve profile.
//   stacks = vertical bands (alternating body/rim texture for pattern)
//   slices = horizontal subdivisions
//
// Profile: B(t) = (1-t)^2 * TOP_R + 2t(1-t) * MID_R + t^2 * BOT_R
//   This gives a curved silhouette: narrows slightly at the waist,
//   then flares out to the mouth.
// ------------------------------------------------------------
void drawBellBody(int stacks = 10, int slices = 24) {
    for (int i = 0; i < stacks; i++) {
        // Alternate between two textures to create decorative banding
        if (i % 2 == 0) colBellBodyTex();
        else            colBellRimTex();

        float t0 = (float)i / stacks;
        float t1 = (float)(i + 1) / stacks;

        // Quadratic Bezier profile: TOP_R -> MID_R -> BOT_R
        auto profile = [&](float t)->float {
            return B_BODY_TOP_R * (1 - t) * (1 - t)
                + B_BODY_MID_R * 2 * t * (1 - t)
                + B_BODY_BOT_R * t * t;
            };

        float r0 = profile(t0), r1 = profile(t1);
        float y0 = -t0 * B_BODY_H, y1 = -t1 * B_BODY_H; // body hangs downward

        // Compute surface normal direction from the slope of the profile
        float dr = r1 - r0, dy = y1 - y0;
        float nLen = sqrtf(dy * dy + dr * dr);
        float nR = dy / nLen, nY = -dr / nLen;

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float a = (float)j / slices * 2 * PI, ca = cosf(a), sa = sinf(a);
            glTexCoord2f((float)j / slices, t0);
            glNormal3f(nR * ca, nY, nR * sa);
            glVertex3f(r0 * ca, y0, r0 * sa);

            glTexCoord2f((float)j / slices, t1);
            glNormal3f(nR * ca, nY, nR * sa);
            glVertex3f(r1 * ca, y1, r1 * sa);
        }
        glEnd();
    }
}

// ------------------------------------------------------------
// drawBellRim: Draws the widened mouth rim at the bottom opening of the bell.
// Consists of an outer ring cylinder, an inner ring cylinder,
// and a flat annular disk closing the bottom face.
// ------------------------------------------------------------
void drawBellRim(int slices = 24) {
    GLUquadric* q = gluNewQuadric();
    gluQuadricNormals(q, GLU_SMOOTH);
    gluQuadricTexture(q, GL_TRUE);

    // Outer flared rim cylinder
    colBellRimTex();
    glPushMatrix();
    glRotatef(90, 1, 0, 0);
    gluCylinder(q, B_BODY_BOT_R, B_RIM_OUTER_R, B_RIM_H, slices, 4);
    glPopMatrix();

    // Bottom face annular disk (closes the rim at the mouth opening)
    colBellBodyTex();
    glPushMatrix();
    glTranslatef(0, -B_RIM_H, 0);
    glRotatef(90, 1, 0, 0);
    gluDisk(q, B_RIM_INNER_R, B_RIM_OUTER_R, slices, 2);
    glPopMatrix();

    // Inner rim cylinder (visible from below inside the bell)
    colBellRimTex();
    glPushMatrix();
    glRotatef(90, 1, 0, 0);
    gluCylinder(q, B_BODY_BOT_R, B_RIM_INNER_R, B_RIM_H, slices, 4);
    glPopMatrix();

    gluDeleteQuadric(q);
}

// ------------------------------------------------------------
// drawBellClapper: Draws the clapper (the striker inside the bell).
//   swingAngle = current swing rotation (degrees) around Z-axis
//                driven by bellRingTimer for a pendulum effect
// ------------------------------------------------------------
void drawBellClapper(float swingAngle) {
    glPushMatrix();
    glRotatef(swingAngle, 0, 0, 1); // swing the entire clapper assembly

    // Long thin rod hanging down from the crown
    colBellBodyTex();
    glPushMatrix();
    glTranslatef(0, -B_CLAPPER_ROD * 0.5f, 0);
    drawCylinderY(1.2f, B_CLAPPER_ROD, 10);
    glPopMatrix();

    // Spherical ball at the bottom of the clapper rod
    colBellRimTex();
    glPushMatrix();
    glTranslatef(0, -B_CLAPPER_ROD, 0);
    drawSphere(B_CLAPPER_BALL, 14);
    glPopMatrix();

    glPopMatrix();
}

// ------------------------------------------------------------
// drawBellDecorRings: Draws two decorative ring bands around the
//                     middle of the bell body.
//   bodyMidY = Y position of the mid-body (for vertical placement)
//   rMid     = radius at the mid-body (from the Bezier profile)
// ------------------------------------------------------------
void drawBellDecorRings(float bodyMidY, float rMid) {
    // Upper ring band (slightly above mid-body)
    colBellRimTex();
    glPushMatrix();
    glTranslatef(0, bodyMidY + 6, 0);
    drawCylinderY(rMid + 1.5f, 2.5f, 24);
    glPopMatrix();

    // Lower ring band (slightly below mid-body)
    colBellBodyTex();
    glPushMatrix();
    glTranslatef(0, bodyMidY - 2, 0);
    drawCylinderY(rMid + 1.5f, 2.5f, 24);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawDivineBell: Assembles the complete Divine Golden Bell.
//   bellSwing    = overall bell swing angle (pendulum rock)
//   clapperSwing = clapper's independent swing (slightly delayed from bell)
//
// Structure (top to bottom):
//   hanging ring -> connector -> dome crown -> bell body -> rim -> clapper
// ------------------------------------------------------------
void drawDivineBell(float bellSwing, float clapperSwing) {
    glPushMatrix();
    glRotatef(bellSwing, 0, 0, 1); // rock the entire bell on its axis

    // Hanging ring at the very top
    colBellRimTex();
    glPushMatrix();
    glRotatef(90, 1, 0, 0);
    drawBellHangingRing(B_RING_R, B_RING_TUBE, 24);
    glPopMatrix();

    // Connector neck below the hanging ring
    float connTop = -(B_RING_TUBE);
    float connBot = connTop - B_CONN_H;
    colBellBodyTex();
    glPushMatrix();
    glTranslatef(0, connBot + B_CONN_H * 0.5f, 0);
    drawCylinderY(B_CONN_R, B_CONN_H, 16);
    glPopMatrix();

    // Dome crown just below the connector
    float domeOrigin = connBot - B_DOME_R;
    glPushMatrix();
    glTranslatef(0, domeOrigin - 1.5f + B_DOME_R * 0.05f, 0);
    drawBellDome(B_DOME_R, 8, 24);
    // Close the bottom of the dome with a filled disk
    colBellRimTex();
    glRotatef(180, 1, 0, 0);
    drawDiskY(0, B_DOME_R, 24);
    glPopMatrix();

    // Main body hangs below the dome
    float bodyTop = domeOrigin;
    glPushMatrix();
    glTranslatef(0, bodyTop, 0);
    drawBellBody(12, 24);
    glPopMatrix();

    // Compute mid-body radius using the Bezier profile at t=0.45
    // This is used to place the decorative rings exactly at the waist
    float tMid = 0.45f;
    float rMid = B_BODY_TOP_R * (1 - tMid) * (1 - tMid)
        + B_BODY_MID_R * 2 * tMid * (1 - tMid)
        + B_BODY_BOT_R * tMid * tMid;

    drawBellDecorRings(bodyTop - B_BODY_H * 0.45f, rMid);

    // Flared mouth rim at the very bottom of the body
    glPushMatrix();
    glTranslatef(0, bodyTop - B_BODY_H, 0);
    drawBellRim(24);
    glPopMatrix();

    // Clapper hanging inside the bell from the dome origin
    glPushMatrix();
    glTranslatef(0, domeOrigin, 0);
    drawBellClapper(clapperSwing);
    glPopMatrix();

    glPopMatrix();
}

// ============================================================
//  BELL EFFECTS
// ============================================================

// ------------------------------------------------------------
// drawGlowHalo: Draws a glowing ring halo around the bell mouth.
//   radius = inner ring radius
//   alpha  = opacity of the glow (0=invisible, 1=full)
//   yPos   = Y position of the ring in local bell space
// Uses additive blending for a luminous energy effect.
// ------------------------------------------------------------
void drawGlowHalo(float radius, float alpha, float yPos) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive: brighter than background
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);

    const int segs = 36;
    const float thick = 4.5f; // total width of the halo band

    // Outer falloff: transparent at the outer edge, bright at the inner ring
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2 * PI, ca = cosf(a), sa = sinf(a);
        glColor4f(1, 0.95f, 0.55f, 0);     // transparent outer edge
        glVertex3f(ca * (radius + thick), yPos, sa * (radius + thick));
        glColor4f(1, 0.92f, 0.40f, alpha); // bright inner ring
        glVertex3f(ca * radius, yPos, sa * radius);
    }
    glEnd();

    // Inner falloff: bright at the ring, transparent at the inner edge
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2 * PI, ca = cosf(a), sa = sinf(a);
        glColor4f(1, 0.92f, 0.40f, alpha); // bright ring centre
        glVertex3f(ca * radius, yPos, sa * radius);
        glColor4f(1, 0.95f, 0.55f, 0);     // transparent inner edge
        glVertex3f(ca * (radius - thick), yPos, sa * (radius - thick));
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ------------------------------------------------------------
// drawSoundWaveRing: Draws one expanding shockwave ring.
//   centerR = initial ring radius before expansion
//   expand  = amount the ring has expanded from its origin
//   alpha   = current opacity (fades as the ring expands)
//   yPos    = Y height of the ring in world space
//   thick   = visual thickness of the ring band
// Called multiple times with different delays to create a ripple effect.
// ------------------------------------------------------------
void drawSoundWaveRing(float centerR, float expand, float alpha, float yPos, float thick = 3) {
    float r = centerR + expand; // current expanded radius

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive glow
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);

    const int segs = 48;

    // Outer falloff band
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2 * PI, ca = cosf(a), sa = sinf(a);
        glColor4f(1, 0.9f, 0.4f, 0);             // transparent outer edge
        glVertex3f(ca * (r + thick * 2), yPos, sa * (r + thick * 2));
        glColor4f(1, 0.9f, 0.4f, alpha * 0.6f);  // inner ring glow
        glVertex3f(ca * r, yPos, sa * r);
    }
    glEnd();

    // Inner falloff band (brightest at leading edge)
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segs; i++) {
        float a = (float)i / segs * 2 * PI, ca = cosf(a), sa = sinf(a);
        glColor4f(1, 1, 0.8f, alpha * 0.9f);     // bright white-gold leading edge
        glVertex3f(ca * r, yPos, sa * r);
        glColor4f(1, 0.85f, 0.3f, alpha * 0.5f); // trailing fade
        glVertex3f(ca * (r - thick), yPos, sa * (r - thick));
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ============================================================
//  CHARACTER BODY PARTS
// ============================================================

// ------------------------------------------------------------
// drawFace: Draws Yang Xuan's head with facial features.
//   eyeGlow     = intensity of the golden third-eye glow (0 = none)
//   thirdEyeRed = when true, the third eye turns red (fire active)
//
// Features drawn:
//   - Head sphere (slightly squashed for a realistic shape)
//   - Cheekbone bumps
//   - Nose
//   - White eyeballs + dark irises/pupils
//   - Eyebrow lines
//   - Thin mouth box
//   - Third eye in the forehead (golden glow or red fire mode)
// ------------------------------------------------------------
void drawFace(float eyeGlow, bool thirdEyeRed) {
    // Head sphere: slightly tall (scale Y 1.15) and shallow (scale Z 0.92)
    colSkin();
    glPushMatrix();
    glScalef(1, 1.15f, 0.92f);
    drawSphere(2.5f, 16);
    glPopMatrix();

    // Cheekbone bumps (left and right of head)
    colSkin();
    glPushMatrix();
    glTranslatef(-2.5f, 0, 0);
    glScalef(0.32f, 0.55f, 0.28f);
    drawSphere(1, 8);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(2.5f, 0, 0);
    glScalef(0.32f, 0.55f, 0.28f);
    drawSphere(1, 8);
    glPopMatrix();

    // Nose: small dark box centred on the face
    colSkinDark();
    glPushMatrix();
    glTranslatef(0, -0.3f, 2.3f);
    glScalef(0.28f, 0.40f, 0.40f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Disable lighting so face details render with their exact colours
    glDisable(GL_LIGHTING);

    // --- Eyes ---
    // White sclera
    glColor3f(0.95f, 0.95f, 0.95f);
    glPushMatrix();
    glTranslatef(-0.85f, 0.35f, 2.32f);
    glScalef(0.58f, 0.28f, 0.10f);
    drawSphere(1, 8);
    glPopMatrix();

    // Dark iris/pupil (left)
    glColor3f(0.10f, 0.08f, 0.12f);
    glPushMatrix();
    glTranslatef(-0.85f, 0.35f, 2.42f);
    glScalef(0.22f, 0.22f, 0.10f);
    drawSphere(1, 8);
    glPopMatrix();

    // White sclera (right)
    glColor3f(0.95f, 0.95f, 0.95f);
    glPushMatrix();
    glTranslatef(0.85f, 0.35f, 2.32f);
    glScalef(0.58f, 0.28f, 0.10f);
    drawSphere(1, 8);
    glPopMatrix();

    // Dark iris/pupil (right)
    glColor3f(0.10f, 0.08f, 0.12f);
    glPushMatrix();
    glTranslatef(0.85f, 0.35f, 2.42f);
    glScalef(0.22f, 0.22f, 0.10f);
    drawSphere(1, 8);
    glPopMatrix();

    // --- Eyebrows: drawn as GL_LINES for a sharp angled look ---
    glColor3f(0.08f, 0.06f, 0.06f);
    glLineWidth(2.2f);
    glBegin(GL_LINES);
    // Left eyebrow (angled downward towards the nose bridge)
    glVertex3f(-1.55f, 0.85f, 2.40f);
    glVertex3f(-0.20f, 0.72f, 2.42f);
    // Right eyebrow
    glVertex3f(0.20f, 0.72f, 2.42f);
    glVertex3f(1.55f, 0.85f, 2.40f);
    glEnd();
    glLineWidth(1);

    // --- Mouth: thin dark-red box below the nose ---
    glColor3f(0.545f, 0.0f, 0.0f); // reddish lip colour
    glPushMatrix();
    glTranslatef(0.0f, -0.95f, 2.2f); // positioned below nose
    glScalef(0.45f, 0.08f, 0.05f);    // flat and wide like lips
    drawBox(1.0f, 1.0f, 1.0f);
    glPopMatrix();

    // --- Third Eye (forehead) ---
    // Colour switches between pulsing gold (normal) and steady red (fire active)
    float teR = thirdEyeRed ? 0.9f : 0.55f + sinf(animTimer * 2) * 0.1f;
    float teG = thirdEyeRed ? 0 : 0.30f;
    float teB = thirdEyeRed ? 0 : 0.10f;

    // Outer third eye oval (coloured)
    glColor3f(teR, teG, teB);
    glPushMatrix();
    glTranslatef(0, 1.28f, 2.41f);
    glScalef(0.48f, 0.28f, 0.08f);
    drawSphere(1, 10);
    glPopMatrix();

    // White sclera inside the third eye
    glColor3f(0.96f, 0.96f, 0.94f);
    glPushMatrix();
    glTranslatef(0, 1.30f, 2.44f);
    glScalef(0.38f, 0.22f, 0.08f);
    drawSphere(1, 10);
    glPopMatrix();

    // Coloured pupil/iris of the third eye (in front of sclera)
    glColor3f(teR, teG, teB);
    glPushMatrix();
    glTranslatef(0, 1.30f, 2.49f);
    glScalef(0.18f, 0.18f, 0.08f);
    drawSphere(1, 10);
    glPopMatrix();

    // --- Eye Glow effect (only when fire is active) ---
    if (eyeGlow > 0.3f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive: luminous glow
        glDepthMask(GL_FALSE);
        glColor4f(1, 0.95f, 0.6f, eyeGlow * 0.7f);
        glPushMatrix();
        glTranslatef(0, 1.30f, 2.52f);
        glScalef(0.12f, 0.12f, 0.05f);
        drawSphere(1, 8);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ------------------------------------------------------------
// drawHair: Draws Yang Xuan's hair:
//   - Main hair cap (covering the skull)
//   - Top-knot bun with a gold ring hair tie
//   - Side hair panels
//   - Long ponytail hanging down the back
// ------------------------------------------------------------
void drawHair() {
    // Main hair cap: slightly scaled sphere sitting on the head
    colHair();
    glPushMatrix();
    glTranslatef(0, 1.5f, -0.3f);
    glScalef(1.02f, 0.85f, 1.02f);
    drawSphere(2.5f, 14);
    glPopMatrix();

    // Top-knot sphere at the very top
    glPushMatrix();
    glTranslatef(0, 3.8f, -0.5f);
    glScalef(0.70f, 0.80f, 0.70f);
    drawSphere(1, 10);
    glPopMatrix();

    // Cylindrical hair bun shaft connecting the knot to the head
    glPushMatrix();
    glTranslatef(0, 3.0f, -0.5f);
    drawCylinderY(0.45f, 1.8f, 10);
    glPopMatrix();

    // Gold hair tie ring clamping the bun
    colGoldTrim();
    glPushMatrix();
    glTranslatef(0, 2.8f, -0.5f);
    glScalef(1.05f, 0.15f, 1.05f);
    drawCylinderY(0.75f, 1, 14);
    glPopMatrix();

    // Side hair panels (left and right temples)
    colHair();
    glPushMatrix();
    glTranslatef(-2, 0.8f, 0.8f);
    glScalef(0.42f, 0.85f, 0.38f);
    drawSphere(1.8f, 10);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(2, 0.8f, 0.8f);
    glScalef(0.42f, 0.85f, 0.38f);
    drawSphere(1.8f, 10);
    glPopMatrix();

    // Upper ponytail volume (squashed sphere at the back of the head)
    glPushMatrix();
    glTranslatef(0, -1.5f, -2.7f);
    glRotatef(25, 1, 0, 0);
    glScalef(1.10f, 1.80f, 0.55f);
    drawSphere(2.2f, 12);
    glPopMatrix();

    // Lower ponytail hanging strands
    glPushMatrix();
    glTranslatef(0, -5, -4);
    glRotatef(15, 1, 0, 0);

    // Highlight side strands of the ponytail
    colHairHigh();
    glPushMatrix();
    glTranslatef(-0.8f, 0, 0);
    glScalef(0.35f, 2.50f, 0.20f);
    drawBox(1, 1, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.8f, 0, 0);
    glScalef(0.35f, 2.50f, 0.20f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Central main strand of the ponytail (dark hair)
    colHair();
    glPushMatrix();
    glTranslatef(0, -0.5f, 0.1f);
    glScalef(0.55f, 2.20f, 0.22f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Pointed tip of the ponytail
    glPushMatrix();
    glTranslatef(0, -3, 0.1f);
    drawConeY(1.1f, 0.1f, 2.5f, 8);
    glPopMatrix();

    glPopMatrix(); // end ponytail group
}

// ------------------------------------------------------------
// drawHeadCrown: Draws the divine crown/headband worn by Yang Xuan.
// Consists of a wide flat band across the forehead, a centre gem plate,
// and two angled horn-like prongs with small gemstone beads.
// ------------------------------------------------------------
void drawHeadCrown() {
    // Main horizontal crown band across the forehead
    colGoldTrim();
    glPushMatrix();
    glTranslatef(0, 1.20f, 2);
    glScalef(2.55f, 0.20f, 0.18f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Central ornamental plate above the crown band
    colGoldLight();
    glPushMatrix();
    glTranslatef(0, 1.60f, 2.25f);
    glScalef(0.28f, 0.38f, 0.18f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Left angled prong
    colGoldTrim();
    glPushMatrix();
    glTranslatef(-2, 1.80f, 1.60f);
    glRotatef(30, 0, 0, 1);
    glScalef(0.12f, 0.65f, 0.12f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Right angled prong (mirror of left)
    glPushMatrix();
    glTranslatef(2, 1.80f, 1.60f);
    glRotatef(-30, 0, 0, 1);
    glScalef(0.12f, 0.65f, 0.12f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Small gemstone beads at the tips of the prongs (cyan/jade colour)
    glColor3f(0.55f, 0.80f, 0.85f);
    glPushMatrix();
    glTranslatef(-2.45f, 1.25f, 1.55f);
    drawSphere(0.25f, 8);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(2.45f, 1.25f, 1.55f);
    drawSphere(0.25f, 8);
    glPopMatrix();
}

// drawNeck: Simple cylinder connecting the torso to the head.
void drawNeck() {
    colSkin();
    glPushMatrix();
    drawCylinderY(0.85f, 2.2f, 10);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawTorso: Draws Yang Xuan's armoured upper body:
//   - Navy cylindrical robe body (slightly flattened in Z)
//   - Chest armour plate (trapezoid shape)
//   - Chest plate trim and centre ornament
//   - Side shoulder pad flanges
//   - Gold vertical trim stripes
//   - Dark lower abdomen/robe section
// ------------------------------------------------------------
void drawTorso() {
    // Main robe cylinder (slightly flat on the Z axis for a more human shape)
    colNavy();
    glPushMatrix();
    glScalef(1, 1, 0.78f);
    drawCylinderY(3.20f, 8.50f, 16);
    glPopMatrix();

    // Chest armour plate (wider at bottom, forward-facing)
    colArmorBase();
    glPushMatrix();
    glTranslatef(0, 1.50f, 1.5f);
    glScalef(1, 1, 0.5f);
    drawTrapezoidY(2, 2.8f, 3, 1.5f);
    glPopMatrix();

    // Horizontal trim bar across the top of the chest plate
    colArmorLight();
    glPushMatrix();
    glTranslatef(0, 1.50f, 2.3f);
    glScalef(1.85f, 0.12f, 0.12f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Diamond-shaped centre ornament on the chest (gold trim)
    colGoldTrim();
    glPushMatrix();
    glTranslatef(0, 1.80f, 2.25f);
    glRotatef(45, 0, 0, 1); // rotate 45 deg to form a diamond
    glScalef(0.60f, 0.60f, 0.20f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Bright gold centre gem of the diamond
    colGoldLight();
    glPushMatrix();
    glTranslatef(0, 1.80f, 2.45f);
    glRotatef(45, 0, 0, 1);
    glScalef(0.28f, 0.28f, 0.12f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Side shoulder flanges (left and right) - dark armour
    colArmorDark();
    glPushMatrix();
    glTranslatef(-3, 0.8f, 0.8f);
    glScalef(0.5f, 1, 0.6f);
    drawTrapezoidY(0.8f, 1.3f, 2.5f, 1.5f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3, 0.8f, 0.8f);
    glScalef(0.5f, 1, 0.6f);
    drawTrapezoidY(0.8f, 1.3f, 2.5f, 1.5f);
    glPopMatrix();

    // Gold vertical trim stripes on the chest edges
    colGoldTrim();
    glPushMatrix();
    glTranslatef(-2.8f, 1.5f, 1.6f);
    glScalef(0.08f, 2.5f, 0.08f);
    drawBox(1, 1, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(2.8f, 1.5f, 1.6f);
    glScalef(0.08f, 2.5f, 0.08f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Lower abdomen / under-robe section (darker navy)
    colNavyDark();
    glPushMatrix();
    glTranslatef(0, -2, 0);
    glScalef(0.90f, 1, 0.75f);
    drawCylinderY(2.5f, 3, 14);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawBelt: Draws the gold waist belt and hanging sash panels.
//   - Main belt cylinder
//   - Decorative upper ring
//   - Front belt buckle with centre ornament
//   - Two front hanging sash panels (navy with gold trim tips)
//   - Rear hanging sash panel
// ------------------------------------------------------------
void drawBelt() {
    // Main belt cylinder
    colGoldTrim();
    glPushMatrix();
    drawCylinderY(3.35f, 1.30f, 20);
    glPopMatrix();

    // Thin bright ring at the top edge of the belt
    colGoldLight();
    glPushMatrix();
    glTranslatef(0, 0.30f, 0);
    drawCylinderY(3.38f, 0.22f, 20);
    glPopMatrix();

    // Belt buckle sphere (front-centre)
    colGoldLight();
    glPushMatrix();
    glTranslatef(0, 0, 3.38f);
    glScalef(0.8f, 0.8f, 0.25f);
    drawSphere(1, 10);
    glPopMatrix();

    // Centre ornament on the buckle
    colArmorBase();
    glPushMatrix();
    glTranslatef(0, 0, 3.58f);
    glScalef(0.38f, 0.38f, 0.15f);
    drawSphere(1, 8);
    glPopMatrix();

    // Left front hanging sash panel
    colNavyLight();
    glPushMatrix();
    glTranslatef(-1, -3.5f, 2.8f);
    glScalef(0.62f, 3.5f, 0.12f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Gold tip at the bottom of the left sash
    colGoldTrim();
    glPushMatrix();
    glTranslatef(-1, -7, 2.82f);
    glScalef(0.65f, 0.18f, 0.10f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Right front hanging sash panel (mirror of left)
    colNavyLight();
    glPushMatrix();
    glTranslatef(1, -3.5f, 2.8f);
    glScalef(0.62f, 3.5f, 0.12f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Gold tip at the bottom of the right sash
    colGoldTrim();
    glPushMatrix();
    glTranslatef(1, -7, 2.82f);
    glScalef(0.65f, 0.18f, 0.10f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Rear sash panel (behind the character)
    colNavy();
    glPushMatrix();
    glTranslatef(0, -3, -3);
    glScalef(1.20f, 3, 0.12f);
    drawBox(1, 1, 1);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawShoulder: Draws one shoulder pad assembly.
//   side  = +1 for right shoulder, -1 for left shoulder
//   pitch = current upper arm pitch (used to slightly tilt the pad with the arm)
// Three overlapping armour spheres create a layered pauldron look.
// ------------------------------------------------------------
void drawShoulder(float side, float pitch) {
    // Outer shoulder dome (base layer)
    colArmorBase();
    glPushMatrix();
    glTranslatef(side * 3.8f, 2.5f, 0);
    glRotatef(pitch * 0.5f, 1, 0, 0); // slight tilt follows arm movement
    glRotatef(side * -12, 0, 0, 1);   // angle the pad outward
    glScalef(0.55f, 0.45f, 0.68f);
    drawSphere(2.5f, 12);
    glPopMatrix();

    // Lower darker shoulder flap
    colArmorDark();
    glPushMatrix();
    glTranslatef(side * 4.2f, 1.8f, 0.2f);
    glRotatef(side * -20, 0, 0, 1);
    glScalef(0.45f, 0.25f, 0.55f);
    drawSphere(2.2f, 10);
    glPopMatrix();

    // Upper bright highlight highlight on the shoulder top
    colArmorLight();
    glPushMatrix();
    glTranslatef(side * 4, 2.8f, 0.5f);
    glScalef(0.40f, 0.18f, 0.50f);
    drawSphere(2, 10);
    glPopMatrix();

    // Gold decorative stud on the shoulder
    colGoldTrim();
    glPushMatrix();
    glTranslatef(side * 4.2f, 2, 0.8f);
    glScalef(0.08f, 1.2f, 0.08f);
    drawBox(1, 1, 1);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawArm: Draws one complete arm with all joints.
//   side           = +1 right arm, -1 left arm
//   upperArmPitch  = shoulder forward/back rotation
//   upperArmSide   = shoulder side raise
//   forearmBend    = elbow bend angle
//   handFistBlend  = 0=open, 1=fist (for drawComplexHand)
//   holdsWeapon    = true = draw spear in this hand
//   animId         = current animation ID (for special poses)
//   animTime       = animation time (for spear placement)
//   wristPitch/Yaw = wrist joint angles
//   holdsBell      = true = hand holds the bell (open palm pose)
// ------------------------------------------------------------
void drawArm(float side, float upperArmPitch, float upperArmSide,
    float forearmBend, float handFistBlend, bool holdsWeapon,
    int animId, float animTime, float wristPitch, float wristYaw,
    bool holdsBell) {

    glPushMatrix();
    // Position at shoulder joint
    glTranslatef(side * 3.8f, 2, 0);
    glRotatef(upperArmPitch, 1, 0, 0);        // swing forward/back
    glRotatef(side * upperArmSide, 0, 0, 1);  // raise to the side
    glRotatef(side * 12, 0, 0, 1);            // natural outward angle at rest

    // Upper arm sleeve (navy cylinder)
    colNavy();
    glPushMatrix();
    glTranslatef(0, -2.5f, 0);
    drawCylinderY(1.05f, 5, 10);
    glPopMatrix();

    // Move down to the elbow joint
    glTranslatef(0, -5, 0);

    // Elbow guard sphere
    colArmorBase();
    drawSphere(1.15f, 8);

    // Bend at the elbow
    glRotatef(-forearmBend, 1, 0, 0);

    // Lower arm sleeve (navy cylinder, slightly narrower)
    colNavy();
    glPushMatrix();
    glTranslatef(0, -2.5f, 0);
    drawCylinderY(0.90f, 5, 10);
    glPopMatrix();

    // Wrist guard cylinder
    colArmorBase();
    glPushMatrix();
    glTranslatef(0, -3.5f, 0);
    drawCylinderY(1.10f, 2.8f, 12);
    glPopMatrix();

    // Gold wrist trim rings (upper and lower edges of the wrist guard)
    colGoldTrim();
    glPushMatrix();
    glTranslatef(0, -2.8f, 0);
    drawCylinderY(1.12f, 0.22f, 12);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, -4.5f, 0);
    drawCylinderY(1.12f, 0.22f, 12);
    glPopMatrix();

    // Move to the wrist position
    glTranslatef(0, -5.5f, 0);
    glRotatef(wristPitch, 1, 0, 0);
    glRotatef(wristYaw, 0, 1, 0);

    colSkin();

    // Determine hand pose based on what the arm is doing
    bool handGrip = (holdsWeapon && showWeapon && !holdsBell);

    if (handGrip) {
        // Gripping hand pose when holding the spear
        glPushMatrix();
        glRotatef(side * 90, 0, 1, 0);
        drawGrippingHand(side);
        glPopMatrix();

        // Attach the spear to the right hand when it is held
        if (holdsWeapon && showWeapon && side > 0) {
            glPushMatrix();
            glTranslatef(-0.3f, -0.3f, -7);
            glRotatef(90, 1, 0, 0);
            glRotatef(-15, 0, 1, 0);
            glTranslatef(0, 10, 0);
            drawSpear(animTime);
            glPopMatrix();
        }
    }
    else if (holdsBell) {
        // Open upward-facing palm when holding/presenting the bell
        glPushMatrix();
        glRotatef(side * 90, 0, 1, 0);
        drawComplexHand(0.0f, side); // fully open (blend = 0)
        glPopMatrix();
    }
    else {
        // Normal open/fist hand based on fistBlend
        glPushMatrix();
        glRotatef(side * 90, 0, 1, 0);
        drawComplexHand(handFistBlend, side);
        glPopMatrix();
    }
    glPopMatrix();
}

// ------------------------------------------------------------
// drawLeg: Draws one complete leg with hip, knee, and foot joints.
//   side       = +1 right leg, -1 left leg
//   hipPitch   = manual hip swing (forward/back, degrees)
//   kneeBend   = knee bend angle
//   footPitch  = ankle/foot tilt angle
//   walkUpper  = walk animation contribution to the upper leg
//   walkLower  = walk animation contribution to the knee
// ------------------------------------------------------------
void drawLeg(float side, float hipPitch, float kneeBend, float footPitch,
    float walkUpper = 0, float walkLower = 0) {

    glPushMatrix();
    // Position at hip joint (offset sideways from body centre)
    glTranslatef(side * 1.8f, 8, 0);
    glRotatef(hipPitch + walkUpper, 1, 0, 0); // hip swing

    // Upper leg (thigh) cylinder - navy robe fabric
    colNavy();
    glPushMatrix();
    glTranslatef(0, -3.5f, 0);
    drawCylinderY(1.40f, 7, 12);
    glPopMatrix();

    // Knee guard (slightly forward-offset sphere)
    colArmorBase();
    glPushMatrix();
    glTranslatef(0, -7, 0.5f);
    glScalef(0.80f, 0.55f, 0.55f);
    drawSphere(1.5f, 8);
    glPopMatrix();

    // Gold trim ring on the knee guard
    colGoldTrim();
    glPushMatrix();
    glTranslatef(0, -7, 1);
    glScalef(0.70f, 0.18f, 0.22f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Move down to the knee joint
    glTranslatef(0, -7, 0);

    // Knee joint sphere
    colArmorBase();
    drawSphere(1.10f, 8);

    // Bend at the knee
    glRotatef(kneeBend + walkLower, 1, 0, 0);

    // Lower leg (shin) cylinder
    colNavy();
    glPushMatrix();
    glTranslatef(0, -3, 0);
    glScalef(0.88f, 1, 0.88f);
    drawCylinderY(1.20f, 6, 12);
    glPopMatrix();

    // Shin guard plate (forward-facing flat box)
    colArmorBase();
    glPushMatrix();
    glTranslatef(0, -3.5f, 0.8f);
    glScalef(0.70f, 1.8f, 0.35f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Gold trim strips on the shin guard (top and bottom edges)
    colGoldTrim();
    glPushMatrix();
    glTranslatef(0, -2.5f, 1.15f);
    glScalef(0.72f, 0.12f, 0.10f);
    drawBox(1, 1, 1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, -5.3f, 1.15f);
    glScalef(0.72f, 0.12f, 0.10f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Foot/ankle section: rotate at the ankle
    glPushMatrix();
    glTranslatef(0, -6.5f, 0);
    glRotatef(-footPitch, 1, 0, 0); // ankle tilt
    glTranslatef(0, 6.5f, 0);

    // Boot shaft cylinder
    colBoot();
    glPushMatrix();
    glTranslatef(0, -8.25f, 0);
    drawCylinderY(1.40f, 4.5f, 12);
    glPopMatrix();

    // Boot top collar (slightly wider ring)
    colBootTrim();
    glPushMatrix();
    glTranslatef(0, -6, 0);
    drawCylinderY(1.50f, 0.5f, 12);
    glPopMatrix();

    // Gold trim ring at the boot collar
    colGoldTrim();
    glPushMatrix();
    glTranslatef(0, -6, 0);
    drawCylinderY(1.52f, 0.18f, 12);
    glPopMatrix();

    // Boot sole/foot box
    colBoot();
    glPushMatrix();
    glTranslatef(0, -10, 0.6f);
    glScalef(0.90f, 0.55f, 1.20f);
    drawBox(1, 1, 1);
    glPopMatrix();

    // Toe cap box (extends forward from the sole)
    glPushMatrix();
    glTranslatef(0, -10.3f, 1.85f);
    glScalef(0.60f, 0.35f, 0.45f);
    drawBox(1, 1, 1);
    glPopMatrix();

    glPopMatrix(); // end foot group
    glPopMatrix(); // end leg group
}

// ============================================================
//  FULL CHARACTER DRAW
// ============================================================

// ------------------------------------------------------------
// drawYangXuan: Assembles and draws the complete Yang Xuan character.
//   animTime = seconds elapsed in the current animation
//   animId   = which scripted animation is playing (0 = idle)
//
// This function:
//   1. Reads manual joint values
//   2. Adds breathing offset (sinusoidal vertical bob)
//   3. Overlays walk cycle swing on arms and legs
//   4. Applies scripted animation overrides (anim 3 = rain throw, anim 6 = thrust)
//   5. Applies bell-hold pose override to the left arm
//   6. Draws legs, belt, torso, shoulders, arms, neck, head, hair, crown
// ------------------------------------------------------------
void drawYangXuan(float animTime, int animId) {
    // Breathing bob: gentle up-down oscillation at ~1.8 Hz
    float breathe = sinf(animTime * 1.8f) * 0.18f;

    // Eye glow: pulses when fire is active; zero otherwise
    float eyeGlow = isFireActive ? (0.85f + sinf(animTime * 8) * 0.15f) : 0;
    bool  thirdEyeRed = isFireActive;

    // Copy manual joint values into local variables (will be modified by animations)
    float sL = lUpperArmPitch, sLs = lUpperArmSide, eL = lForearmBend;
    float sR = rUpperArmPitch, sRs = rUpperArmSide, eR = rForearmBend;
    float hL = lHipPitch, kL = lKneeBend;
    float hR = rHipPitch, kR = rKneeBend;

    // Animation working variables
    float torsoTilt = 0, torsoTwist = 0, hairSway = 0;
    float wUL = 0, wLL = 0, wUR = 0, wLR = 0; // walk leg swing deltas
    float wAL = 0, wFL = 0, wAR = 0, wFR = 0; // walk arm swing deltas
    float extraHeadRotY = 0, extraHeadRotX = 0;

    // Start with the manual wrist values
    float wPitchL = lWristPitch;
    float wYawL = lWristYaw;

    // If the bell is active: override the left arm to an open-palm forward pose
    bool leftArmHoldsBell = (bellState != BELL_HIDDEN && bellState != BELL_DISMISSING);
    if (leftArmHoldsBell) {
        sL = -90.0f; // raise left arm forward
        eL = 0.0f;   // straighten the elbow
        wPitchL = 0.0f;
        wYawL = -90.0f; // rotate wrist to face palm upward
        sLs = 10.0f;  // slight side raise
    }

    // Bell attack body pose: lean/twist during and after bell launch
    if (bellAttackBodyActive) {
        float bt = bellAttackBodyTimer / BELL_ATTACK_BODY_DURATION;
        float bt_clamped = fminf(bt, 1.0f);

        if (bt_clamped < 0.3f) {
            // Phase 1 (0-30%): wind-up lean to the right, right arm swings up
            float p = bt_clamped / 0.3f;
            float ease = sinf(p * PI * 0.5f);
            torsoTilt = -ease * 12.0f;
            torsoTwist = ease * 10.0f;
            sR += -ease * 35.0f;
            eR += ease * 20.0f;
            hR += ease * 8.0f;
            kR += ease * 12.0f;
            hL -= ease * 5.0f;
            extraHeadRotX = -ease * 8.0f;
            extraHeadRotY = ease * 12.0f;
        }
        else if (bt_clamped < 0.6f) {
            // Phase 2 (30-60%): full extension follow-through, twist reverses
            float p = (bt_clamped - 0.3f) / 0.3f;
            float ease = sinf(p * PI * 0.5f);
            torsoTilt = -12.0f * (1 - ease) + 18.0f * ease;
            torsoTwist = 10.0f * (1 - ease) + (-15.0f) * ease;
            sR += (-35.0f * (1 - ease)) + (15.0f * ease);
            eR += 20.0f * (1 - ease);
            sL += -ease * 20.0f;
            hL += ease * 12.0f;
            kL += ease * 8.0f;
            hR += 8.0f * (1 - ease) - ease * 5.0f;
            kR += 12.0f * (1 - ease);
            extraHeadRotX = -8.0f * (1 - ease) + ease * 5.0f;
            extraHeadRotY = 12.0f * (1 - ease) + (-18.0f) * ease;
        }
        else {
            // Phase 3 (60-100%): recover back to neutral
            float p = (bt_clamped - 0.6f) / 0.4f;
            float ease = sinf(p * PI * 0.5f);
            torsoTilt = 18.0f * (1 - ease);
            torsoTwist = -15.0f * (1 - ease);
            sR += 15.0f * (1 - ease);
            sL += -20.0f * (1 - ease);
            hL += 12.0f * (1 - ease);
            kL += 8.0f * (1 - ease);
            extraHeadRotY = -18.0f * (1 - ease);
            extraHeadRotX = 5.0f * (1 - ease);
        }

        // Mark the animation complete after full duration
        if (bt >= 1.0f) {
            bellAttackBodyActive = false;
            bellAttackBodyTimer = 0.0f;
        }
    }

    // Auto-arm move: looping right arm "attack dash" swing when enabled (key 'B')
    if (autoArmMove) {
        float cycle = sinf(autoArmTimer * 8);
        if (cycle < 0) cycle = 0;
        sR += -20 + cycle * 20;
        eR += 20 + cycle * 45;
    }

    // Walk cycle: sinusoidal leg swing, arm counter-swing, and torso sway
    if (isWalking) {
        float wS = 10.0f;  // walk frequency
        float swL = 35.0f;  // leg swing amplitude (degrees)
        float swA = 25.0f;  // arm swing amplitude (degrees)
        float s = sinf(walkTimer * wS);

        // Legs swing opposite to each other
        wUL = s * swL;   wUR = -s * swL;
        // Knees bend on the back swing only (natural gait)
        wLL = fmaxf(0.0f, -s) * 45.0f;
        wLR = fmaxf(0.0f, s) * 45.0f;
        // Arms swing counter to the same-side leg
        wAR = s * swA - 10.0f;
        wAL = -s * swA - 10.0f;
        // Elbow naturally bends on the back swing
        wFR = fmaxf(0.0f, -wAR * 0.8f);
        wFL = fmaxf(0.0f, -wAL * 0.8f);
        // Torso side-tilt and rotation follow the step rhythm
        torsoTilt = cosf(walkTimer * wS * 2.0f) * 2.5f;
        torsoTwist = s * 8.0f;
        hairSway = s * 5.0f;
    }

    // Anim 3: Heavenly Rain (spear throw into sky)
    if (animId == 3) {
        if (animTime < 0.4f) {
            // Wind-up: right arm raises and left arm counterbalances
            float p = animTime / 0.4f;
            sR += -p * 60;
            eR = p * 30;
            torsoTilt -= p * 15;
            sL += p * 20;
            hL += p * 10;
        }
        else if (animTime < 0.8f) {
            // Release: arm thrusts straight up, body tilts forward
            float p = (animTime - 0.4f) / 0.4f;
            sR = -60 * (1 - p) + p * (-180);
            eR = 30 * (1 - p) + p * 10;
            torsoTilt = -15 * (1 - p) + p * 20;
            sL += (1 - p) * 20;
            hL += 10 * (1 - p) - p * 5;
            hR += p * 10;
        }
        else if (animTime < 1.2f) {
            // Recovery: arm lowers back to rest
            float p = (animTime - 0.8f) / 0.4f;
            sR = -180 * (1 - p) + p * (-30);
            torsoTilt = 20 * (1 - p);
            hR += 10 * (1 - p);
        }
        // Auto-end animation after 5 seconds
        if (animTime > 5) activeAnim = 0;
    }

    // Anim 6: Forward Thrust Attack
    if (animId == 6) {
        if (animTime < 0.25f) {
            // Chamber: pull right arm back and bend elbow
            float p = animTime / 0.25f;
            eR = p * 60;
            sR = -p * 45;
            torsoTwist += p * 15;
        }
        else if (animTime < 0.60f) {
            // Extension: arm extends forward quickly
            float p = (animTime - 0.25f) / 0.35f;
            eR = 60 * (1 - p);
            sR = -45 * (1 - p);
            torsoTwist += 15 * (1 - p);
        }
        else {
            // End animation
            activeAnim = 0;
        }
    }

    // Determine if the spear is currently flying (detached from hand during anim 3)
    bool weaponDetached = (animId == 3 && animTime >= 0.8f && animTime < 5);
    bool holdsWeapon = showWeapon && !weaponDetached;

    // Add walk arm swing to the arm angles (skip left if holding bell)
    if (!leftArmHoldsBell) {
        sL += wAL;
        eL += wFL;
    }
    sR += wAR;
    eR += wFR;

    // ---- Assemble the character ----
    glPushMatrix();
    // Scale the character up to scene proportions
    glScalef(2.5f, 2.5f, 2.5f);
    // Apply breathing vertical offset
    glTranslatef(0, 10.0f + breathe, 0);

    // Legs (drawn in the lower body space)
    drawLeg(-1, hL, kL, lFootPitch, wUL, wLL); // left leg
    drawLeg(1, hR, kR, rFootPitch, wUR, wLR); // right leg

    // Belt at waist height
    glPushMatrix();
    glTranslatef(0, 8.5f, 0);
    drawBelt();
    glPopMatrix();

    // Upper body: apply torso tilt and twist before drawing everything above the waist
    glPushMatrix();
    glTranslatef(0, 13, 0);
    glRotatef(torsoTilt, 1, 0, 0); // lean forward/back
    glRotatef(torsoTwist, 0, 1, 0); // twist left/right

    drawTorso();
    drawShoulder(-1, sL); // left shoulder pad
    drawShoulder(1, sR); // right shoulder pad

    // Arms
    drawArm(1, sR, sRs, eR, fistBlend, holdsWeapon, animId, animTime, rWristPitch, rWristYaw, false);
    drawArm(-1, sL, sLs, eL, fistBlend, false, animId, animTime, wPitchL, wYawL, leftArmHoldsBell);

    // Neck (centred, slightly forward)
    glPushMatrix();
    glTranslatef(0, 4.8f, 0.5f);
    drawNeck();
    glPopMatrix();

    // Head (at neck top, rotated by joint controls and animation overrides)
    glPushMatrix();
    glTranslatef(0, 7.8f, 0.5f);
    glRotatef(headRotY + extraHeadRotY, 0, 1, 0); // left/right turn
    glRotatef(headRotX + extraHeadRotX, 1, 0, 0); // up/down tilt
    drawFace(eyeGlow, thirdEyeRed);

    // Hair sways with torso twist during walking
    glPushMatrix();
    glRotatef(hairSway, 0, 1, 0);
    drawHair();
    glPopMatrix();

    drawHeadCrown();
    glPopMatrix(); // end head group

    glPopMatrix(); // end upper body group
    glPopMatrix(); // end character root
}

// ============================================================
//  BELL DRAW IN SCENE
// ============================================================

// ------------------------------------------------------------
// updateCinematicCamera: Smoothly moves the camera to follow
//                         the character and the descending bell during
//                         the summon cinematic (F1 pressed).
// Only active while bellState == BELL_SUMMONING.
// The camera interpolates from the saved player camera position to
// a fixed cinematic position looking at the character's palm from
// slightly above and to the side.
// ------------------------------------------------------------
void updateCinematicCamera(float dt, float palmX, float palmY, float palmZ) {
    if (bellState == BELL_SUMMONING) {
        float t = bellSummonTimer / SUMMON_DURATION; // 0 = start, 1 = end

        // Target camera angles/position for the cinematic view
        float targetPry = charRotY + 25.0f;                 // look slightly to the side of the character
        float targetPrx = 15.0f + (1.0f - t) * 20.0f;      // start high, lower as bell nears the palm
        float targetDist = 220.0f + (1.0f - t) * 120.0f;     // start far, zoom in as bell descends
        float targetPanX = 0.0f;                              // keep character horizontally centred
        float targetPanY = 2.5f + (1.0f - t) * 30.0f;        // start above, lower with the bell

        // Fast ease-in during the first 20% of the summon, then smooth lerp
        float ease = t < 0.2f ? (t / 0.2f) : 1.0f;
        float sm = sinf(ease * PI * 0.5f); // smoothstep easing

        if (t < 0.2f) {
            // Snap to cinematic target quickly at the start
            pry = cineSavedPry * (1 - sm) + targetPry * sm;
            prx = cineSavedPrx * (1 - sm) + targetPrx * sm;
            camDist = cineSavedDist * (1 - sm) + targetDist * sm;
            camPanX = cineSavedPanX * (1 - sm) + targetPanX * sm;
            camPanY = cineSavedPanY * (1 - sm) + targetPanY * sm;
        }
        else {
            // Continuously track the target as the bell descends
            float lerp = 5.0f * dt;
            pry += (targetPry - pry) * lerp;
            prx += (targetPrx - prx) * lerp;
            camDist += (targetDist - camDist) * lerp;
            camPanX += (targetPanX - camPanX) * lerp;
            camPanY += (targetPanY - camPanY) * lerp;
        }
    }
}

// ------------------------------------------------------------
// drawBellInScene: Manages the bell's per-frame state machine and
//                  renders the bell at its current world position.
// Called every frame from display().
// Handles: SUMMONING, PALM_HOVER, LAUNCHING, ATTACK_ACTIVE,
//          RETURNING, DISMISSING states.
// ------------------------------------------------------------
void drawBellInScene(float dt) {
    if (bellState == BELL_HIDDEN) return; // nothing to draw

    float bellSwing = 0, clapperSwing = 0;

    // Gentle bobbing offset while hovering over the palm (sinusoidal)
    float bobY = sinf(bellRingTimer * 3) * 3.0f;

    // Compute world-space forward and right vectors for the character
    float charFX = sinf((charRotY + 180) * PI / 180);
    float charFZ = cosf((charRotY + 180) * PI / 180);
    float charRX = cosf((charRotY + 180) * PI / 180);
    float charRZ = -sinf((charRotY + 180) * PI / 180);

    // Compute the world position of Yang Xuan's left palm
    // (20.5 units to the left, 30 units forward from the character root)
    float palmX = charX - charRX * 20.5f + charFX * 30.0f;
    float palmZ = charZ - charRZ * 20.5f + charFZ * 30.0f;

    // Palm height and bell geometry calculations
    float basePalmY = 10.0f, palmY = basePalmY;
    float bellTotalHeight = (B_CONN_H + B_DOME_R + B_BODY_H + B_RIM_H);
    float scaledHeight = bellTotalHeight * BELL_PALM_SCALE; // height at palm scale

    // Update cinematic camera if active
    if (cineCameraActive) updateCinematicCamera(dt, palmX, palmY, palmZ);

    // Update bell swing/clapper swing timers based on state
    if (bellState == BELL_PALM_HOVER || bellState == BELL_SUMMONING || bellState == BELL_RETURNING) {
        bellRingTimer += dt;
        bellSwing = sinf(bellRingTimer * 4.5f) * 8;        // gentle 8-degree rock
        clapperSwing = sinf(bellRingTimer * 4.5f - 0.4f) * 14; // clapper lags behind bell
    }

    if (bellState == BELL_ATTACK_ACTIVE || bellState == BELL_LAUNCHING) {
        bellWaveTimer += dt;
        bellSwing = sinf(bellWaveTimer * 3) * 5;          // faster vibration during flight
        clapperSwing = sinf(bellWaveTimer * 3 - 0.3f) * 20; // stronger clapper swing
    }

    glPushMatrix();

    // ---- State: BELL_SUMMONING ----
    // Bell descends from high above to just above the palm over SUMMON_DURATION seconds.
    // Uses a smoothstep ease: slow at start, accelerates, then slows near the palm.
    if (bellState == BELL_SUMMONING) {
        float t = fminf(bellSummonTimer / SUMMON_DURATION, 1.0f);
        float ease = t * t * (3 - 2 * t); // cubic smoothstep

        float startY = palmY + 600.0f; // start far above the scene
        float endY = palmY + scaledHeight + 2.0f; // end just above the palm

        // Bell stays directly above the palm horizontally throughout
        bellWorldX = palmX;
        bellWorldY = startY * (1 - ease) + endY * ease;
        bellWorldZ = palmZ;

        // Bell starts at 30% of palm scale and grows to full palm scale as it arrives
        float sc = BELL_PALM_SCALE * (0.3f + 0.7f * ease);

        glTranslatef(bellWorldX, bellWorldY, bellWorldZ);
        glScalef(sc, sc, sc);
        glRotatef(bellRingTimer * 60, 0, 1, 0); // slow spin during descent

        // Glow halo fades in during the second half of the summon
        if (ease > 0.5f) {
            float a = (ease - 0.5f) * 2 * 0.6f;
            float mouthY = -(B_CONN_H + B_DOME_R + B_BODY_H + B_RIM_H);
            drawGlowHalo(B_RIM_OUTER_R + 8.0f, a, mouthY);
        }
    }

    // ---- State: BELL_PALM_HOVER ----
    // Bell orbits slowly above the open palm, bobbing gently.
    else if (bellState == BELL_PALM_HOVER) {
        bellOrbitAngle += dt * 40; // degrees per second orbit speed
        bellWorldX = palmX;
        bellWorldY = palmY + scaledHeight + 2.0f + bobY;
        bellWorldZ = palmZ;

        float sc = BELL_PALM_SCALE;
        glTranslatef(bellWorldX, bellWorldY, bellWorldZ);
        glScalef(sc, sc, sc);
        glRotatef(bellOrbitAngle, 0, 1, 0); // slow orbit spin

        // Constant dim glow halo while hovering
        float mouthY = -(B_CONN_H + B_DOME_R + B_BODY_H + B_RIM_H);
        drawGlowHalo(B_RIM_OUTER_R + 8.0f, 0.25f, mouthY);
    }

    // ---- State: BELL_LAUNCHING ----
    // Bell accelerates from palm scale to attack scale, while flying forward.
    // Duration is LAUNCH_ACCEL_TIME seconds.
    else if (bellState == BELL_LAUNCHING) {
        bellLaunchTimer += dt;
        float t = fminf(bellLaunchTimer / LAUNCH_ACCEL_TIME, 1.0f);
        float ease = t * t; // ease-in: slow start, accelerates

        // Scale interpolates from tiny (palm) to large (attack)
        bellScale = BELL_PALM_SCALE * (1 - ease) + BELL_ATTACK_SCALE * ease;

        // Y rises to chest height during the launch
        float launchTargetY = palmY + 60.0f;
        bellWorldY = palmY * (1.0f - ease) + launchTargetY * ease;

        // X and Z advance by velocity each frame
        bellWorldX += bellVelX * dt;
        bellWorldZ += bellVelZ * dt;

        glTranslatef(bellWorldX, bellWorldY, bellWorldZ);
        glScalef(bellScale, bellScale, bellScale);
        glRotatef(bellLaunchTimer * 180, 1, 0, 0); // spin as it launches

        // Transition to attack state after acceleration phase ends
        if (bellLaunchTimer > LAUNCH_ACCEL_TIME) {
            bellState = BELL_ATTACK_ACTIVE;
            bellLaunchTimer = 0;
            bellWaveTimer = 0;
        }
    }

    // ---- State: BELL_ATTACK_ACTIVE ----
    // Bell flies forward with decelerating velocity, emitting 3 staggered shockwave rings.
    else if (bellState == BELL_ATTACK_ACTIVE) {
        // Gradually decelerate (95% of velocity each frame)
        bellVelX *= 0.95f;
        bellVelZ *= 0.95f;
        bellWorldX += bellVelX * dt;
        bellWorldZ += bellVelZ * dt;

        glTranslatef(bellWorldX, bellWorldY, bellWorldZ);
        glScalef(BELL_ATTACK_SCALE, BELL_ATTACK_SCALE, BELL_ATTACK_SCALE);
        glRotatef(bellWaveTimer * 90, 0, 1, 0); // slower spin during attack

        // Draw 3 expanding shockwave rings with staggered delays
        float mouthY = -(B_CONN_H + B_DOME_R + B_BODY_H + B_RIM_H);
        for (int r = 0; r < 3; r++) {
            float delay = (float)r * 0.3f;      // each ring starts 0.3s later
            float t = bellWaveTimer - delay;
            if (t < 0 || t > 3) continue;        // only draw if within lifetime
            float expand = t * 80;                // ring radius grows at 80 units/sec
            float alpha = fmaxf(0, 1 - t / 3.0f); // fades out over 3 seconds
            drawSoundWaveRing(B_RIM_OUTER_R, expand, alpha, mouthY, (r == 0 ? 6 : 3));
        }

        // Transition to RETURNING after 2.5 seconds
        if (bellWaveTimer > 2.5f) {
            bellState = BELL_RETURNING;
            bellWaveTimer = 0.0f;
        }
    }

    // ---- State: BELL_RETURNING ----
    // Bell lerps back to the palm position at a tracking speed of 5/sec.
    // Once close enough (within 3 units), switches to PALM_HOVER.
    else if (bellState == BELL_RETURNING) {
        float targetHoverY = palmY + scaledHeight + 2.0f;

        // Exponential interpolation toward palm position
        bellWorldX += (palmX - bellWorldX) * 5.0f * dt;
        bellWorldY += (targetHoverY - bellWorldY) * 5.0f * dt;
        bellWorldZ += (palmZ - bellWorldZ) * 5.0f * dt;

        // Also shrink back to palm scale
        bellScale += (BELL_PALM_SCALE - bellScale) * 5.0f * dt;

        glTranslatef(bellWorldX, bellWorldY, bellWorldZ);
        glScalef(bellScale, bellScale, bellScale);
        glRotatef(bellWaveTimer * 720, 0, 1, 0); // fast spin on return

        // Distance to palm: transition when close enough
        float dist = sqrt(pow(palmX - bellWorldX, 2) +
            pow(targetHoverY - bellWorldY, 2) +
            pow(palmZ - bellWorldZ, 2));
        if (dist < 3.0f) {
            bellState = BELL_PALM_HOVER;
            bellWaveTimer = 0.0f;
        }
        bellWaveTimer += dt;
    }

    // ---- State: BELL_DISMISSING ----
    // Bell shrinks and fades out over 0.8 seconds on F3.
    else if (bellState == BELL_DISMISSING) {
        bellDismissTimer += dt;
        float t = bellDismissTimer / 0.8f;

        if (t >= 1) {
            // Dismiss complete: hide the bell
            bellState = BELL_HIDDEN;
            bellDismissTimer = 0;
        }

        // Shrink from current palm scale to zero
        float sc = BELL_PALM_SCALE * (1 - t);
        glTranslatef(bellWorldX, bellWorldY, bellWorldZ);
        glScalef(sc, sc, sc);

        // Fade out: reduce diffuse alpha as t increases
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLfloat diffA[] = { 0.9f, 0.72f, 0.18f, 1 - t };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffA);
    }

    drawDivineBell(bellSwing, clapperSwing);

    if (bellState == BELL_DISMISSING) {
        glDisable(GL_BLEND);
    }
    glPopMatrix();
}

// ============================================================
//  TEMPLE HELPER FUNCTIONS
// ============================================================

// ------------------------------------------------------------
// drawRoofSlope: Draws a pyramid-like roof section covering all 6 faces
//                (4 slopes, top ridge cap, and bottom underside).
//   eHW/eHD = eave half-width/depth (bottom outer edge of the slope)
//   rHW/rHD = ridge half-width/depth (top inner edge of the slope)
//   roofH   = total height of the slope from eave to ridge
//   baseY   = Y position of the eave (bottom of the slope)
// The slope normals are computed from the geometry to ensure correct shading.
// ------------------------------------------------------------
void drawRoofSlope(float eHW, float eHD, float rHW, float rHD, float roofH, float baseY) {
    // Compute slope normal vectors:
    // dfz/dfy = forward slope tangent, dfx/dfy = side slope tangent
    float dfz = eHD - rHD, dfy = roofH, dfx = eHW - rHW;
    float magF = sqrtf(dfy * dfy + dfz * dfz); // magnitude of forward slope
    float magS = sqrtf(dfy * dfy + dfx * dfx); // magnitude of side slope

    colRoof();
    glBegin(GL_QUADS);

    // Front slope (facing +Z, towards the viewer)
    glNormal3f(0, dfy / magF, dfz / magF);
    glTexCoord2f(0, 0); glVertex3f(-eHW, baseY, eHD);
    glTexCoord2f(1, 0); glVertex3f(eHW, baseY, eHD);
    glTexCoord2f(1, 1); glVertex3f(rHW, baseY + roofH, rHD);
    glTexCoord2f(0, 1); glVertex3f(-rHW, baseY + roofH, rHD);

    // Back slope (facing -Z)
    glNormal3f(0, dfy / magF, -dfz / magF);
    glTexCoord2f(0, 0); glVertex3f(eHW, baseY, -eHD);
    glTexCoord2f(1, 0); glVertex3f(-eHW, baseY, -eHD);
    glTexCoord2f(1, 1); glVertex3f(-rHW, baseY + roofH, -rHD);
    glTexCoord2f(0, 1); glVertex3f(rHW, baseY + roofH, -rHD);

    // Left slope (facing -X)
    glNormal3f(-dfx / magS, dfy / magS, 0);
    glTexCoord2f(0, 0); glVertex3f(-eHW, baseY, -eHD);
    glTexCoord2f(1, 0); glVertex3f(-eHW, baseY, eHD);
    glTexCoord2f(1, 1); glVertex3f(-rHW, baseY + roofH, rHD);
    glTexCoord2f(0, 1); glVertex3f(-rHW, baseY + roofH, -rHD);

    // Right slope (facing +X)
    glNormal3f(dfx / magS, dfy / magS, 0);
    glTexCoord2f(0, 0); glVertex3f(eHW, baseY, eHD);
    glTexCoord2f(1, 0); glVertex3f(eHW, baseY, -eHD);
    glTexCoord2f(1, 1); glVertex3f(rHW, baseY + roofH, -rHD);
    glTexCoord2f(0, 1); glVertex3f(rHW, baseY + roofH, rHD);

    // Top ridge cap (flat top of the pyramid, facing up)
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(-rHW, baseY + roofH, rHD);
    glTexCoord2f(1, 0); glVertex3f(rHW, baseY + roofH, rHD);
    glTexCoord2f(1, 1); glVertex3f(rHW, baseY + roofH, -rHD);
    glTexCoord2f(0, 1); glVertex3f(-rHW, baseY + roofH, -rHD);

    // Bottom underside (rarely visible, but needed to close the geometry)
    glNormal3f(0, -1, 0);
    glTexCoord2f(0, 0); glVertex3f(-eHW, baseY, -eHD);
    glTexCoord2f(1, 0); glVertex3f(eHW, baseY, -eHD);
    glTexCoord2f(1, 1); glVertex3f(eHW, baseY, eHD);
    glTexCoord2f(0, 1); glVertex3f(-eHW, baseY, eHD);

    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// ------------------------------------------------------------
// drawUpturnCorner: Draws a single upturned roof corner ornament.
//   ex, ey, ez = world position of this corner
//   sx, sz     = direction vector pointing outward (corner direction)
// A small rotated box creates the upturn, and a golden ball sphere
// is placed at the tip.
// ------------------------------------------------------------
void drawUpturnCorner(float ex, float ey, float ez, float sx, float sz) {
    glPushMatrix();
    glTranslatef(ex, ey, ez);

    // Rotate the upturn box to align with the corner direction
    float ang = atan2f(sz, sx) * 180 / PI;
    glRotatef(ang, 0, 1, 0);
    glRotatef(-30, 0, 0, 1); // tilt up at 30 degrees

    glTranslatef(1.5f, 0, 0);
    colRoof();
    drawBox(1.4f, 0.28f, 0.65f); // the upturn tile piece
    glPopMatrix();

    // Golden ball at the very tip of the upturn
    colGoldT();
    glPushMatrix();
    glTranslatef(ex + sx * 1.2f, ey + 1, ez + sz * 1.2f);
    drawSphere(0.40f, 8);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawRoofTier: Draws one complete roof tier (eave slab + slopes +
//               ridge decorations + top ridge cap + upturned corners).
//   eHW/eHD  = eave half-extents (outer edge of the eave)
//   rHW/rHD  = ridge half-extents (inner top of the slope)
//   roofH    = height from eave to ridge
//   baseY    = Y position of the eave bottom
//   eaveThk  = thickness of the eave slab
// ------------------------------------------------------------
void drawRoofTier(float eHW, float eHD, float rHW, float rHD, float roofH, float baseY, float eaveThk) {
    // Eave slab: flat box forming the projecting eave overhang
    colRoof();
    glPushMatrix();
    glTranslatef(0, baseY, 0);
    drawBox(eHW + 0.6f, eaveThk, eHD + 0.6f);
    glPopMatrix();

    // Slopes: fill all gap including ridge top
    drawRoofSlope(eHW, eHD, rHW, rHD, roofH, baseY + eaveThk);

    // Ridge line decorations: 3 thin horizontal boxes evenly spaced up the slope
    colRoof();
    for (int i = 1; i <= 3; i++) {
        float t = (float)i / 4;
        float ry = baseY + eaveThk + t * roofH;                // Y position on slope
        float rw = eHW + t * (rHW - eHW);                      // interpolated width
        float rd = eHD + t * (rHD - eHD);                      // interpolated depth
        glPushMatrix();
        glTranslatef(0, ry, 0);
        drawBox(rw, 0.13f, rd + 0.13f);                         // thin ridge line bar
        glPopMatrix();
    }

    // Top ridge cap: covers the very peak of the roof
    colRoof();
    glPushMatrix();
    glTranslatef(0, baseY + eaveThk + roofH + 0.28f, 0);
    drawBox(rHW + 0.4f, 0.40f, rHD + 0.4f);
    glPopMatrix();

    // Upturned corners at all 4 eave corners
    float cx[4] = { 1, -1, -1,  1 };
    float cz[4] = { 1,  1, -1, -1 };
    for (int i = 0; i < 4; i++) {
        colRoof();
        drawUpturnCorner(cx[i] * (eHW + 0.15f), baseY + eaveThk - 0.05f,
            cz[i] * (eHD + 0.15f), cx[i], cz[i]);
    }
}

// ============================================================
//  FRONT PILLAR (4 columns in front of temple)
// ============================================================

// ------------------------------------------------------------
// drawTemPillar: Draws one of the 4 red front columns of the temple.
//   x, z    = position in temple local space
//   baseY   = Y of the column base (sits on the platform)
//   r       = column radius
//   h       = column height
// ------------------------------------------------------------
void drawTemPillar(float x, float baseY, float z, float r, float h) {
    glPushMatrix();
    glTranslatef(x, baseY, z);

    // Base cap: square platform block at the bottom of the column
    colTempPillarCap();
    glPushMatrix();
    glTranslatef(0, 0.3f, 0);
    drawBox(r * 1.8f, 0.30f, r * 1.8f);
    glPopMatrix();

    // Shaft: tall red cylinder (vermillion tint + wood texture)
    colTempPillarShaft();
    glPushMatrix();
    glTranslatef(0, h * 0.5f + 0.6f, 0);
    drawCylinderY(r, h);
    glPopMatrix();

    // Capital cap: square block at the top of the column
    colTempPillarCap();
    glPushMatrix();
    glTranslatef(0, h + 0.6f, 0);
    drawBox(r * 1.8f, 0.35f, r * 1.8f);
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

// ============================================================
//  DRAGON PILLAR 
// ============================================================

// ------------------------------------------------------------
// drawDragonPillar: Draws one dragon-coiled pillar.
//   x, y, z = world position of the pillar base
// The pillar consists of:
//   - Tall teal cylinder shaft (TEX_PILLAR texture)
//   - Bronze coil of spheres winding up the shaft (dragon body)
//   - Gold top and bottom cap boxes
//   - Gold orb sphere at the very top
// ------------------------------------------------------------
void drawDragonPillar(float x, float y, float z) {
    float h = 17, r = 0.5f; // pillar height and radius

    glPushMatrix();
    glTranslatef(x, y + h * 0.5f, z); // centre the pillar vertically

    // Set teal material and bind TEX_PILLAR before the cylinder
    // so the GLU quadric texture coords sample pillar.bmp correctly
    setMaterial(0.00f, 0.50f, 0.50f, 32, 0.3f, 0.6f, 0.6f);
    if (enableTextures && textures[TEX_PILLAR]) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[TEX_PILLAR]);
        glColor3f(1.0f, 1.0f, 1.0f); // let texture colour show through
    }
    else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.00f, 0.50f, 0.50f); // fallback flat teal
    }
    drawCylinderY(r, h, 16);
    glDisable(GL_TEXTURE_2D);

    // Dragon coil: bronze sphere beads spiralling up the shaft
    // Each bead is placed using polar coordinates offset from the shaft surface
    colBronze();
    for (float i = -h * 0.5f + 1; i < h * 0.5f - 1; i += 0.25f) {
        float a = i * 2.5f; // spiral angle advances with height
        float dx = (r + 0.15f) * cosf(a); // slightly outside the shaft radius
        float dz = (r + 0.15f) * sinf(a);
        glPushMatrix();
        glTranslatef(dx, i, dz);
        drawSphere(0.2f, 8);
        glPopMatrix();
    }

    // Gold caps and orb at the top (use TEX_GOLD if available)
    colGoldT();
    if (enableTextures && textures[TEX_GOLD]) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[TEX_GOLD]);
        glColor3f(1.0f, 0.80f, 0.0f);
    }

    // Bottom cap box
    glPushMatrix();
    glTranslatef(0, -h * 0.5f + 0.2f, 0);
    drawBox(r * 1.5f, 0.3f, r * 1.5f);
    glPopMatrix();

    // Top cap box
    glPushMatrix();
    glTranslatef(0, h * 0.5f - 0.2f, 0);
    drawBox(r * 1.5f, 0.3f, r * 1.5f);
    glPopMatrix();

    // Orb sphere at the very top of the pillar
    glPushMatrix();
    glTranslatef(0, h * 0.5f + 0.8f, 0);
    drawSphere(0.6f, 16);
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawSimpleWindow: Draws a temple side-wall window.
//   hw, hh = half-width and half-height of the glass pane
//
// Structure:
//   1. Gold frame: 4 separate border pieces (top, bottom, left, right)
//      leaving an opening in the middle
//   2. Glass pane: semi-transparent blue-tinted box (uses TEX_GLASS if available)
//   3. Window lattice: dark-red grid lines drawn over the glass
// ------------------------------------------------------------
void drawSimpleWindow(float hw, float hh) {
    // 1. Gold frame pieces (drawn separately to leave a visible hole)
    glDisable(GL_TEXTURE_2D);
    colGoldT();

    float frameThick = 0.11f; // half-thickness of each frame border strip
    float depth = 0.14f; // depth of the frame (into the wall)

    // Top border
    glPushMatrix();
    glTranslatef(0, hh + frameThick, 0);
    drawBox(hw + frameThick * 2, frameThick, depth);
    glPopMatrix();

    // Bottom border
    glPushMatrix();
    glTranslatef(0, -(hh + frameThick), 0);
    drawBox(hw + frameThick * 2, frameThick, depth);
    glPopMatrix();

    // Left border
    glPushMatrix();
    glTranslatef(-(hw + frameThick), 0, 0);
    drawBox(frameThick, hh, depth);
    glPopMatrix();

    // Right border
    glPushMatrix();
    glTranslatef((hw + frameThick), 0, 0);
    drawBox(frameThick, hh, depth);
    glPopMatrix();

    // 2. Glass pane (inside the frame hole)
    if (enableTextures && textures[TEX_GLASS]) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[TEX_GLASS]);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // White with slight blue tint, 65% opacity
        GLfloat diffA[] = { 0.7f, 0.85f, 1.0f, 0.65f };
        GLfloat ambA[] = { 0.4f, 0.50f, 0.6f, 0.65f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffA);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambA);
        glColor4f(0.7f, 0.85f, 1.0f, 0.65f);

        drawBox(hw, hh, 0.07f);

        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
    }
    else {
        // Fallback: semi-transparent blue without texture
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.5f, 0.75f, 0.95f, 0.35f);
        drawBox(hw, hh, 0.07f);
        glDisable(GL_BLEND);
    }

    // 3. Window lattice: traditional Chinese grid pattern
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    colDarkVermillion();
    glLineWidth(1.8f);

    float zf = 0.17f; // draw slightly in front of the glass
    glBegin(GL_LINES);
    // 3 vertical lines
    glVertex3f(-hw, -hh, zf); glVertex3f(-hw, hh, zf);
    glVertex3f(0, -hh, zf); glVertex3f(0, hh, zf);
    glVertex3f(hw, -hh, zf); glVertex3f(hw, hh, zf);
    // 2 horizontal lines at 1/3 and 2/3 heights
    glVertex3f(-hw, -hh * 0.33f, zf); glVertex3f(hw, -hh * 0.33f, zf);
    glVertex3f(-hw, hh * 0.33f, zf); glVertex3f(hw, hh * 0.33f, zf);
    glEnd();
    glLineWidth(1);

    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ------------------------------------------------------------
// drawTempleDoor: Draws the double-panel hinged temple door.
//   x, z    = centre X/Z position of the doorway
//   baseY   = Y of the bottom of the door
//   hw, hh  = half-width and half-height of the door opening
//
// The door opens by rotating around its hinge edge (doorAngle is
// updated in display() based on the doorOpen flag).
// ------------------------------------------------------------
void drawTempleDoor(float x, float baseY, float z, float hw, float hh) {
    // 1. Door frame: top beam and two side pillars (gold)
    colGoldT();
    float fw = 0.38f; // frame width extension
    float fh = 0.32f; // frame height extension
    float fd = 0.20f; // frame depth

    // Top beam spanning the full door width
    glPushMatrix();
    glTranslatef(x, baseY + hh * 2.0f + fh * 0.5f, z);
    drawBox(hw + fw, fh * 0.5f, fd);
    glPopMatrix();

    // Left frame pillar
    glPushMatrix();
    glTranslatef(x - hw - fw * 0.5f, baseY + hh, z);
    drawBox(fw * 0.5f, hh, fd);
    glPopMatrix();

    // Right frame pillar
    glPushMatrix();
    glTranslatef(x + hw + fw * 0.5f, baseY + hh, z);
    drawBox(fw * 0.5f, hh, fd);
    glPopMatrix();

    // 2. Left door panel: hinges on its left edge; swings inward when open
    glPushMatrix();
    glTranslatef(x - hw, baseY + hh, z + 0.12f);
    glRotatef(-doorAngle, 0, 1, 0); // negative angle = opens into the temple
    glTranslatef(hw * 0.5f, 0, 0);  // pivot around the hinge edge

    colDoor();
    drawBox(hw * 0.5f - 0.07f, hh, 0.12f);

    // Door handle sphere on the left panel
    colGoldT();
    glPushMatrix();
    glTranslatef(hw * 0.25f, 0, 0.13f);
    drawSphere(0.22f, 8);
    glPopMatrix();
    glPopMatrix();

    // 3. Right door panel: mirror of the left; swings in the opposite direction
    glPushMatrix();
    glTranslatef(x + hw, baseY + hh, z + 0.12f);
    glRotatef(doorAngle, 0, 1, 0);  // positive angle = opens into the temple
    glTranslatef(-hw * 0.5f, 0, 0); // pivot around the hinge edge

    colDoor();
    drawBox(hw * 0.5f - 0.07f, hh, 0.12f);

    // Door handle sphere on the right panel
    colGoldT();
    glPushMatrix();
    glTranslatef(-hw * 0.25f, 0, 0.13f);
    drawSphere(0.22f, 8);
    glPopMatrix();
    glPopMatrix();

    // 4. Bottom threshold bar
    colGoldT();
    glPushMatrix();
    glTranslatef(x, baseY + 0.18f, z + 0.1f);
    drawBox(hw, 0.18f, 0.18f);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawDougong: Draws the dougong (bracket set) — the traditional
//              Chinese corbel system that supports the eave overhang.
//   baseY        = Y position of the bracket base
//   halfW, halfD = half-extents of the supported beam span
// ------------------------------------------------------------
void drawDougong(float baseY, float halfW, float halfD) {
    // Main base beam across the full wall width
    colWall();
    glPushMatrix();
    glTranslatef(0, baseY, 0);
    drawBox(halfW + 1, 0.50f, halfD + 1);
    glPopMatrix();

    // Wide projecting bracket cap above the base beam
    colGoldT();
    glPushMatrix();
    glTranslatef(0, baseY + 1.1f, 0);
    drawBox(halfW + 2, 0.50f, halfD + 2);
    glPopMatrix();

    // 3 evenly spaced corbel blocks along the front face
    colWall();
    float bxs[3] = { -halfW * 0.65f, 0, halfW * 0.65f };
    for (int i = 0; i < 3; i++) {
        // Front corbel
        glPushMatrix();
        glTranslatef(bxs[i], baseY + 0.6f, halfD + 1.5f);
        drawBox(0.80f, 0.55f, 0.40f);
        glPopMatrix();
        // Rear corbel (mirrored)
        glPushMatrix();
        glTranslatef(bxs[i], baseY + 0.6f, -halfD - 1.5f);
        drawBox(0.80f, 0.55f, 0.40f);
        glPopMatrix();
    }

    // 2 evenly spaced corbel blocks along the side faces
    float bzs[2] = { -halfD * 0.45f, halfD * 0.45f };
    for (int i = 0; i < 2; i++) {
        // Right side corbel
        glPushMatrix();
        glTranslatef(halfW + 1.5f, baseY + 0.6f, bzs[i]);
        drawBox(0.40f, 0.55f, 0.80f);
        glPopMatrix();
        // Left side corbel (mirrored)
        glPushMatrix();
        glTranslatef(-halfW - 1.5f, baseY + 0.6f, bzs[i]);
        drawBox(0.40f, 0.55f, 0.80f);
        glPopMatrix();
    }
    glDisable(GL_TEXTURE_2D);
}

// ------------------------------------------------------------
// drawLantern: Draws a traditional Chinese red lantern hanging from a pole.
//   x, y, z = world position of the lantern attachment point
// Structure (top to bottom): pole -> gold cap -> red body -> glowing inner box -> gold base
// ------------------------------------------------------------
void drawLantern(float x, float y, float z) {
    glPushMatrix();
    glTranslatef(x, y, z);

    // Thin hanging pole
    colBrown();
    drawCylinderY(0.06f, 0.80f, 6);

    // Gold mounting cap at the top of the lantern
    colGoldT();
    glPushMatrix();
    glTranslatef(0, -0.60f, 0);
    drawBox(0.38f, 0.14f, 0.38f);
    glPopMatrix();

    // Red lantern body (outer shell)
    colVermillion();
    glPushMatrix();
    glTranslatef(0, -1.30f, 0);
    drawBox(0.52f, 0.70f, 0.52f);
    glPopMatrix();

    // Yellow glowing inner light (slightly smaller than the red body)
    glColor3f(1, 0.88f, 0.28f);
    resetMat();
    glPushMatrix();
    glTranslatef(0, -1.30f, 0);
    drawBox(0.36f, 0.58f, 0.36f);
    glPopMatrix();

    // Gold base cap at the bottom of the lantern
    colGoldT();
    glPushMatrix();
    glTranslatef(0, -2.08f, 0);
    drawBox(0.38f, 0.14f, 0.38f);
    glPopMatrix();

    glPopMatrix();
}

// ------------------------------------------------------------
// drawIncenseUrn: Draws a small stone incense urn/brazier.
//   x, y, z = world position of the urn base
// ------------------------------------------------------------
void drawIncenseUrn(float x, float y, float z) {
    glPushMatrix();
    glTranslatef(x, y, z);

    // Heavy dark-stone base block
    colDarkStone();
    glPushMatrix();
    glTranslatef(0, 0.4f, 0);
    drawBox(0.70f, 0.40f, 0.70f);
    glPopMatrix();

    // Light grey urn body (wider mid-section)
    colLightGrey();
    glPushMatrix();
    glTranslatef(0, 1.25f, 0);
    drawBox(0.55f, 0.45f, 0.55f);
    glPopMatrix();

    // Dark neck narrowing above the body
    colDarkStone();
    glPushMatrix();
    glTranslatef(0, 1.85f, 0);
    drawBox(0.38f, 0.22f, 0.38f);
    glPopMatrix();

    // Gold rim lip
    colGoldT();
    glPushMatrix();
    glTranslatef(0, 2.12f, 0);
    drawBox(0.45f, 0.10f, 0.45f);
    glPopMatrix();

    // Gold finial sphere at the top
    glPushMatrix();
    glTranslatef(0, 2.38f, 0);
    drawSphere(0.28f, 8);
    glPopMatrix();

    // Two small side handles
    colGoldT();
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(0, 1.25f, s * 0.68f);
        drawBox(0.14f, 0.36f, 0.14f);
        glPopMatrix();
    }
    glPopMatrix();
}

// ------------------------------------------------------------
// drawGuardianLion: Draws a stylized stone guardian lion (shishi/foo dog).
//   x, y, z = world position of the lion's base
// A simplified representation using boxes and spheres:
//   base pedestal -> body cube -> head sphere -> face sphere -> ear bumps -> tail spike
// ------------------------------------------------------------
void drawGuardianLion(float x, float y, float z) {
    glPushMatrix();
    glTranslatef(x, y, z);

    // Dark stone pedestal base
    colDarkStone();
    glPushMatrix();
    glTranslatef(0, 0.65f, 0);
    drawBox(1.10f, 0.65f, 1.10f);
    glPopMatrix();

    // Light grey body cube
    colLightGrey();
    glPushMatrix();
    glTranslatef(0, 2, 0);
    drawBox(0.75f, 0.75f, 0.90f);
    glPopMatrix();

    // Dark head sphere (sitting on top of the body)
    colDarkStone();
    glPushMatrix();
    glTranslatef(0, 3, 0);
    drawSphere(0.80f, 10);
    glPopMatrix();

    // Light muzzle/snout sphere (forward-facing)
    colLightGrey();
    glPushMatrix();
    glTranslatef(0, 3.20f, 0.45f);
    drawSphere(0.65f, 10);
    glPopMatrix();

    // Two small ear bumps
    for (int s = -1; s <= 1; s += 2) {
        glPushMatrix();
        glTranslatef(s * 0.45f, 3.68f, 0.15f);
        drawSphere(0.22f, 6);
        glPopMatrix();
    }

    // Upward-curving tail spike at the back
    colDarkStone();
    glPushMatrix();
    glTranslatef(0, 2.80f, -0.75f);
    glRotatef(55, 1, 0, 0); // angle upward and back
    drawConeY(0.22f, 0, 1.20f, 8);
    glPopMatrix();

    glPopMatrix();
}

// ============================================================
//  MAIN TEMPLE DRAW
// ============================================================

// ------------------------------------------------------------
// drawTemple: Draws the complete Chinese temple complex.
// The temple is drawn in a local coordinate space scaled up by 10x,
// then positioned in the world at (-55.5, -120) by the caller.
//
// Components (bottom to top):
//   1. Three stone platform tiers
//   2. Stone staircase (NS steps)
//   3. Red vermillion walls with window openings
//   4. Gold dougong bracket set
//   5. Three roof tiers (each getting smaller toward the top)
//   6. Gold finial at the peak
//   7. Decorative lanterns and incense urns
//   8. Dragon pillars at the sides
//   9. 4 front red columns
//   10. Double hinged door
//   11. Guardian lion statues at the base of the stairs
// ------------------------------------------------------------
void drawTemple() {
    glPushMatrix();
    glScalef(10, 10, 10); // scale to scene proportions

    // ---- Platform tier dimensions ----
    const float GY = 0.0f;       // ground Y level within temple space
    const float P1W = 22, P1D = 15, PH = 1.5f;   // tier 1: widest/lowest
    const float P2W = 17.5f, P2D = 11.5f;          // tier 2: middle
    const float P3W = 13, P3D = 8.5f;              // tier 3: top/narrowest
    const float platTopY = GY + PH * 3;             // Y of the top platform surface

    // ---- Wall dimensions ----
    const float WW = P3W - 1, WD = P3D - 1;         // wall half-extents (inset from tier 3)
    const float WH = 12;                             // wall height
    const float wallBaseY = platTopY, wallTopY = wallBaseY + WH;

    // ---- Staircase dimensions ----
    const int   NS = 5;             // number of steps
    const float SH = (PH * 3) / NS; // step height
    const float SD = 1.30f;         // step depth
    const float SW = 8;             // staircase width

    // ---- Platform tiers (stone texture) ----
    colStone();
    glPushMatrix();
    glTranslatef(0, GY + PH * 0.5f, 0);
    drawBox(P1W, PH * 0.5f, P1D);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, GY + PH * 1.5f, 0);
    drawBox(P2W, PH * 0.5f, P2D);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, GY + PH * 2.5f, 0);
    drawBox(P3W, PH * 0.5f, P3D);
    glPopMatrix();

    // Platform trim lines (stone texture instead of gold)
    colStoneTrim();
    glPushMatrix();
    glTranslatef(0, GY + PH + 0.08f, 0);
    drawBox(P2W + 0.15f, 0.09f, P2D + 0.15f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, GY + PH * 2 + 0.08f, 0);
    drawBox(P3W + 0.15f, 0.09f, P3D + 0.15f);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0, platTopY + 0.08f, 0);
    drawBox(P3W + 0.15f, 0.09f, P3D + 0.15f);
    glPopMatrix();

    // ---- Staircase (NS steps ascending from ground to platform) ----
    for (int i = 0; i < NS; i++) {
        colStone();
        glPushMatrix();
        glTranslatef(0, GY + PH * 3 - (i + 0.5f) * SH, P3D + (i + 0.5f) * SD);
        drawBox(SW, SH * 0.5f, SD * 0.5f);
        glPopMatrix();

        // Step edge trim (stone texture)
        colStoneTrim();
        glPushMatrix();
        glTranslatef(0, GY + PH * 3 - i * SH + 0.07f, P3D + i * SD);
        drawBox(SW + 0.12f, 0.07f, 0.09f);
        glPopMatrix();
    }

    // ---- Walls ----
    // Back wall
    colWall();
    glPushMatrix();
    glTranslatef(0, wallBaseY + WH * 0.5f, -WD + 0.6f);
    drawBox(WW, WH * 0.5f, 0.70f);
    glPopMatrix();

    // Two side walls
    for (int s = -1; s <= 1; s += 2) {
        colWall();
        glPushMatrix();
        glTranslatef(s * (WW - 0.70f), wallBaseY + WH * 0.5f, 0);
        drawBox(0.70f, WH * 0.5f, WD);
        glPopMatrix();
    }

    // Front wall: two sections flanking the door opening
    const float DW = 3.20f; // door half-width
    for (int s = -1; s <= 1; s += 2) {
        float fwCX = s * ((WW + DW) * 0.5f);
        float fwHW = (WW - DW) * 0.5f;
        colWall();
        glPushMatrix();
        glTranslatef(fwCX, wallBaseY + WH * 0.5f, WD - 0.60f);
        drawBox(fwHW, WH * 0.5f, 0.70f);
        glPopMatrix();
    }

    // Gold trim band just below the wall top
    colGoldT();
    glPushMatrix();
    glTranslatef(0, wallTopY - 1.25f, WD - 0.35f);
    drawBox(WW, 0.78f, 0.88f);
    glPopMatrix();

    // Wall top parapet
    colWall();
    glPushMatrix();
    glTranslatef(0, wallTopY + 0.22f, 0);
    drawBox(WW + 0.50f, 0.42f, WD + 0.50f);
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);

    // ---- Windows (2 per side wall) ----
    for (int s = -1; s <= 1; s += 2) {
        float wx = s * 12;
        float winZs[2] = { -WD * 0.38f, WD * 0.38f };
        for (int w = 0; w < 2; w++) {
            glPushMatrix();
            glTranslatef(wx, wallBaseY + WH * 0.45f, winZs[w]);
            glRotatef(s * (-90), 0, 1, 0); // face outward
            drawSimpleWindow(1.50f, 2);
            glPopMatrix();
        }
    }

    // ---- Front pillars (4 red columns in front of the door) ----
    const float PR = 0.55f;
    const float pillarZ = WD + 2;
    float pxs[4] = { -(WW - 1.2f), -(WW * 0.36f), (WW * 0.36f), (WW - 1.2f) };
    for (int p = 0; p < 4; p++) {
        drawTemPillar(pxs[p], wallBaseY, pillarZ, PR, WH);
    }

    // ---- Door ----
    drawTempleDoor(0, wallBaseY, WD - 0.28f, DW, WH * 0.44f);

    // ---- Dougong bracket set ----
    drawDougong(wallTopY + 0.65f, WW, WD);

    // ---- Three roof tiers (each tier is smaller and higher than the previous) ----
    float r1Y = wallTopY + 2.25f;
    float r1EHW = WW + 4, r1EHD = WD + 4, r1RHW = WW + 1, r1RHD = WD + 1;
    drawRoofTier(r1EHW, r1EHD, r1RHW, r1RHD, 5, r1Y, 0.65f);

    float r2Y = r1Y + 6.60f;
    float r2EHW = WW + 0.5f, r2EHD = WD + 0.5f, r2RHW = WW - 2.5f, r2RHD = WD - 2.5f;
    drawRoofTier(r2EHW, r2EHD, r2RHW, r2RHD, 4, r2Y, 0.55f);

    float r3Y = r2Y + 5.50f;
    float r3EHW = WW - 3, r3EHD = WD - 3, r3RHW = WW - 5.5f, r3RHD = WD - 5.5f;
    drawRoofTier(r3EHW, r3EHD, r3RHW, r3RHD, 3.5f, r3Y, 0.45f);

    // ---- Finial at the very top of the roof ----
    float finY = r3Y + 4.90f;
    colGoldT();
    glPushMatrix();
    glTranslatef(0, finY, 0);
    drawCylinderY(0.32f, 2.5f, 12);        // base cylinder
    glTranslatef(0, 1.80f, 0);
    drawSphere(0.65f, 14);                   // orb
    glTranslatef(0, 0.78f, 0);
    drawConeY(0.42f, 0, 2, 14);             // upward spike
    glTranslatef(0, 1.15f, 0);
    drawSphere(0.28f, 10);                   // tip ball
    glPopMatrix();

    // ---- Hanging lanterns (4 at eave level, 4 on pillar tops) ----
    float lanY = r1Y - 0.40f, lanFZ = WD + 1.90f;
    drawLantern(-WW * 0.55f, lanY, lanFZ);
    drawLantern(WW * 0.55f, lanY, lanFZ);
    drawLantern(-WW * 0.55f, lanY, -lanFZ + 1);
    drawLantern(WW * 0.55f, lanY, -lanFZ + 1);
    for (int p = 0; p < 4; p++) {
        drawLantern(pxs[p], wallBaseY + WH + 0.60f, pillarZ);
    }

    // ---- Incense urns flanking the door ----
    float urnY = platTopY, urnZ = WD + 0.50f;
    drawIncenseUrn(-WW * 0.45f, urnY, urnZ);
    drawIncenseUrn(WW * 0.45f, urnY, urnZ);

    // ---- Guardian lion statues at the base of the stairs ----
    float lionZ = P3D + NS * SD + 1.80f;
    drawGuardianLion(-WW * 0.55f, GY, lionZ);
    drawGuardianLion(WW * 0.55f, GY, lionZ);

    // ---- Dragon pillars on both sides of the temple ----
    float pX = P1W + 4;
    drawDragonPillar(-pX, GY, -5);
    drawDragonPillar(pX, GY, -5);
    drawDragonPillar(-pX, GY, 8);
    drawDragonPillar(pX, GY, 8);

    glPopMatrix(); // end temple scale
}

// ============================================================
//  SPEAR DETACHED (Anim 3 - Heavenly Rain)
// ============================================================

// ------------------------------------------------------------
// drawWeaponDetached: Draws the spear in a detached/flying state
//                     during the Heavenly Rain animation (anim 3).
// The spear launches upward from Yang Xuan's hand, spins, and then
// disappears off-screen before the rain begins to fall.
// ------------------------------------------------------------
void drawWeaponDetached(int animId, float t) {
    glPushMatrix();
    if (animId == 3) {
        // Position at the right hand location (scaled to scene proportions)
        glTranslatef(3.8f * 2.5f, 13.0f * 2.5f + 10.0f, charZ);

        if (t < 1.0f) {
            // Launch phase: translate upward and spin based on throw progress (0.8-1.0)
            float h = (t - 0.8f) / 0.2f;
            glTranslatef(0, h * 200, 0);      // fly up 200 units
            glRotatef(h * 540, 1, 0, 0);      // spin 1.5 full rotations during ascent
        }
        else {
            // After launch: teleport far above the visible scene (hides the spear)
            glTranslatef(0, 5000, 0);
        }
    }
    // Scale to match the character's world scale (character is drawn at 2.5x)
    glScalef(2.5f, 2.5f, 2.5f);
    drawSpear(t);
    glPopMatrix();
}

// ------------------------------------------------------------
// drawRainEffects: Draws the Heavenly Rain particle effect during anim 3.
// After the spear is thrown (t > 1.0), golden spear-like projectiles
// rain down from above.
// Each rain drop is a cylinder with a conical tip, falling at a random
// XZ position; a white flash marks the moment of impact.
// ------------------------------------------------------------
void drawRainEffects(int animId, float t) {
    if (animId == 3 && t > 1.0f && t < 4.5f) {
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive blend for bright rain

        // Screen flash just as the rain begins (t = 1.0 - 1.2)
        if (t < 1.2f) {
            float flash = 1 - (t - 1) * 5; // fade from 1 to 0 in 0.2 seconds
            glColor4f(1, 1, 1, flash);
            glPushMatrix();
            glTranslatef(0, 0, -200);
            drawBox(500, 500, 1); // full-screen white quad
            glPopMatrix();
        }

        // Deterministic random rain drops (same seed = same positions every frame)
        srand(12345);
        float fallProgress = (t - 1) / 3.0f; // 0=top, 1=ground level

        for (int i = 0; i < 80; i++) {
            float rx = (rand() % 800) - 400.0f; // random X in [-400, 400]
            float rz = (rand() % 800) - 400.0f; // random Z in [-400, 400]
            float sm = 1 + (rand() % 100) / 100.0f; // random fall speed multiplier

            // Current Y position: starts at 300, falls to ground (-60) at varying speeds
            float curY = 300 - (fallProgress * 500 * sm);

            if (curY > -60) { // only draw if above ground
                glPushMatrix();
                glTranslatef(rx, curY, rz);

                // Golden drop body (cylinder)
                glColor4f(1, 0.9f, 0.5f, 0.8f);
                drawCylinderY(0.5f, 15, 6);

                // White pointed tip at the bottom
                glColor4f(1, 1, 1, 1);
                glTranslatef(0, -7.5f, 0);
                drawConeY(0.8f, 0, 4, 6);
                glPopMatrix();
            }
        }

        glDisable(GL_BLEND);
        if (lightMode > 0) glEnable(GL_LIGHTING);
    }
}

// ============================================================
//  SKY & GROUND
// ============================================================

// ------------------------------------------------------------
// drawSun: Draws the sun or moon disc in the sky.
// The position is computed from the current time-of-day's azimuth and
// elevation angles, placed at a distance of 800 units from the origin.
// A large translucent halo sphere surrounds a brighter inner sphere.
// ------------------------------------------------------------
void drawSun(const SkyColors& sc) {
    // Convert azimuth + elevation to 3D Cartesian coordinates
    float az = sc.lightAz * PI / 180;
    float el = sc.lightEl * PI / 180;
    float dist = 800;
    float sx = dist * cosf(el) * sinf(az); // X: east/west
    float sy = dist * sinf(el);             // Y: up/down
    float sz = -dist * cosf(el) * cosf(az); // Z: north/south

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST); // draw behind everything else
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float size = sc.isSun ? 18 : 12; // sun is larger than the moon disc

    // Outer halo (large, very transparent)
    glPushMatrix();
    glTranslatef(sx, sy, sz);
    glColor4f(sc.sunR, sc.sunG, sc.sunB, 0.18f);
    glScalef(size * 2.2f, size * 2.2f, 1);
    drawSphere(1, 12);
    glPopMatrix();

    // Inner disc (smaller, mostly opaque)
    glPushMatrix();
    glTranslatef(sx, sy, sz);
    glColor4f(sc.sunR, sc.sunG, sc.sunB, 0.95f);
    glScalef(size, size, size);
    drawSphere(1, 16);
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ------------------------------------------------------------
// drawSky: Draws the skybox as a large cube surrounding the scene.
// With textures: applies Wall.bmp to all 5 visible faces, tinted
//                by the current horizon colour.
// Without textures: draws a gradient quad for each face (sky-colour
//                   at the top, horizon colour at the bottom).
// The sun/moon is drawn on top.
// ------------------------------------------------------------
void drawSky() {
    SkyColors sc = getTimeOfDayColors(timeOfDay);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST); // skybox always at the back

    float S = 1500.0f; // half-size of the skybox cube
    float H = 1500.0f; // top of the skybox (height)
    float G = -55.5f;  // bottom of the skybox (ground level)

    if (enableTextures && textures[TEX_SKY]) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[TEX_SKY]);

        // Tint the sky texture with the current horizon colour
        glColor3f(
            sc.horizR * 0.85f + sc.topR * 0.15f,
            sc.horizG * 0.85f + sc.topG * 0.15f,
            sc.horizB * 0.85f + sc.topB * 0.15f
        );

        glBegin(GL_QUADS);

        // Top face (ceiling): tiled 3x3
        glTexCoord2f(0, 0); glVertex3f(-S, H, -S);
        glTexCoord2f(3, 0); glVertex3f(S, H, -S);
        glTexCoord2f(3, 3); glVertex3f(S, H, S);
        glTexCoord2f(0, 3); glVertex3f(-S, H, S);

        // Back side wall (behind the temple)
        glTexCoord2f(0, 0); glVertex3f(S, G, -S);
        glTexCoord2f(3, 0); glVertex3f(-S, G, -S);
        glTexCoord2f(3, 1); glVertex3f(-S, H, -S);
        glTexCoord2f(0, 1); glVertex3f(S, H, -S);

        // Front side wall
        glTexCoord2f(0, 0); glVertex3f(-S, G, S);
        glTexCoord2f(3, 0); glVertex3f(S, G, S);
        glTexCoord2f(3, 1); glVertex3f(S, H, S);
        glTexCoord2f(0, 1); glVertex3f(-S, H, S);

        // Left side wall
        glTexCoord2f(0, 0); glVertex3f(-S, G, S);
        glTexCoord2f(3, 0); glVertex3f(-S, G, -S);
        glTexCoord2f(3, 1); glVertex3f(-S, H, -S);
        glTexCoord2f(0, 1); glVertex3f(-S, H, S);

        // Right side wall
        glTexCoord2f(0, 0); glVertex3f(S, G, -S);
        glTexCoord2f(3, 0); glVertex3f(S, G, S);
        glTexCoord2f(3, 1); glVertex3f(S, H, S);
        glTexCoord2f(0, 1); glVertex3f(S, H, -S);

        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
    else {
        // Gradient mode: interpolate between top colour and horizon colour per face
        glBegin(GL_QUADS);

        // Top face: solid sky-top colour
        glColor3f(sc.topR, sc.topG, sc.topB);
        glVertex3f(-S, H, -S);
        glVertex3f(S, H, -S);
        glVertex3f(S, H, S);
        glVertex3f(-S, H, S);

        // Back face: top at top, horizon at ground
        glColor3f(sc.topR, sc.topG, sc.topB);   glVertex3f(-S, H, -S); glVertex3f(S, H, -S);
        glColor3f(sc.horizR, sc.horizG, sc.horizB); glVertex3f(S, G, -S); glVertex3f(-S, G, -S);

        // Front face
        glColor3f(sc.topR, sc.topG, sc.topB);   glVertex3f(S, H, S); glVertex3f(-S, H, S);
        glColor3f(sc.horizR, sc.horizG, sc.horizB); glVertex3f(-S, G, S); glVertex3f(S, G, S);

        // Left face
        glColor3f(sc.topR, sc.topG, sc.topB);   glVertex3f(-S, H, S); glVertex3f(-S, H, -S);
        glColor3f(sc.horizR, sc.horizG, sc.horizB); glVertex3f(-S, G, -S); glVertex3f(-S, G, S);

        // Right face
        glColor3f(sc.topR, sc.topG, sc.topB);   glVertex3f(S, H, -S); glVertex3f(S, H, S);
        glColor3f(sc.horizR, sc.horizG, sc.horizB); glVertex3f(S, G, S); glVertex3f(S, G, -S);

        glEnd();
    }

    glEnable(GL_DEPTH_TEST);
    if (lightMode > 0) glEnable(GL_LIGHTING);
    drawSun(sc); // draw sun/moon on top of the skybox
}

// ------------------------------------------------------------
// drawGround: Draws the large flat ground plane.
// With textures: tiles ground.bmp 60x60 over a 1500x1500 unit area.
// Without textures: draws a green-tinted wireframe grid over a filled quad.
// The ground colour is tinted by the current time-of-day diffuse light.
// ------------------------------------------------------------
void drawGround() {
    glDisable(GL_LIGHTING);

    if (enableTextures && textures[TEX_GROUND]) {
        SkyColors sc = getTimeOfDayColors(timeOfDay);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textures[TEX_GROUND]);

        // Tint ground by the current light colour (darker at night, warmer in morning)
        glColor3f(
            0.4f + sc.diffR * 0.6f,
            0.4f + sc.diffG * 0.6f,
            0.3f + sc.diffB * 0.6f
        );

        glBegin(GL_QUADS);
        // Tile texture 60 times across the ground for appropriate grass density
        glTexCoord2f(0, 0); glVertex3f(-1500, -55.5f, -1500);
        glTexCoord2f(60, 0); glVertex3f(1500, -55.5f, -1500);
        glTexCoord2f(60, 60); glVertex3f(1500, -55.5f, 1500);
        glTexCoord2f(0, 60); glVertex3f(-1500, -55.5f, 1500);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
    else {
        // No texture: draw a green grid over a solid quad
        glDisable(GL_TEXTURE_2D);
        SkyColors sc = getTimeOfDayColors(timeOfDay);

        // Grid lines (slightly darker than the fill)
        glColor3f(
            0.3f * (0.4f + sc.diffR * 0.6f),
            0.6f * (0.4f + sc.diffG * 0.6f),
            0.3f * (0.4f + sc.diffB * 0.6f)
        );
        glLineWidth(1);
        glBegin(GL_LINES);
        for (int i = -1500; i <= 1500; i += 25) {
            glVertex3f((float)i, -55, -1500); glVertex3f((float)i, -55, 1500); // N-S lines
            glVertex3f(-1500, -55, (float)i); glVertex3f(1500, -55, (float)i); // E-W lines
        }
        glEnd();
        glLineWidth(1);

        // Filled ground quad (slightly lighter green)
        glBegin(GL_QUADS);
        glColor3f(
            0.4f * (0.4f + sc.diffR * 0.6f),
            0.7f * (0.4f + sc.diffG * 0.6f),
            0.4f * (0.4f + sc.diffB * 0.6f)
        );
        glVertex3f(-1500, -55.5f, -1500);
        glVertex3f(1500, -55.5f, -1500);
        glVertex3f(1500, -55.5f, 1500);
        glVertex3f(-1500, -55.5f, 1500);
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    if (lightMode > 0) glEnable(GL_LIGHTING);
}

// ============================================================
//  LIGHTING
// ============================================================

// ------------------------------------------------------------
// setupLighting: Configures OpenGL lights for the current frame.
//
// lightMode values:
//   0 = Off:            No lighting; flat glColor3f colours only
//   1 = Ambient only:   Global ambient fill; no directional lights
//   2 = Diffuse only:   Two directional lights (no ambient fill, no specular)
//   3 = Both (default): Ambient fill + directional diffuse + specular highlights
//
// Two lights are used:
//   GL_LIGHT0 = Manual light (controlled by azimuth/elevation in processInput)
//   GL_LIGHT1 = Time-of-day sun/moon (follows the sky's lightAz/lightEl values)
// ------------------------------------------------------------
void setupLighting() {
    if (lightMode == 0) {
        glDisable(GL_LIGHTING);
        return;
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); // colours affect both components
    glShadeModel(GL_SMOOTH);                                     // Gouraud shading
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);             // light both front and back faces

    SkyColors sc = getTimeOfDayColors(timeOfDay);

    // Compute GL_LIGHT0 direction from manual azimuth/elevation (spherical -> Cartesian)
    float mAz = manualLightAzimuth * PI / 180;
    float mEl = manualLightElevation * PI / 180;
    GLfloat L0pos[] = { cosf(mEl) * sinf(mAz), sinf(mEl), cosf(mEl) * cosf(mAz), 0 }; // w=0 = directional

    // Compute GL_LIGHT1 direction from time-of-day sun position
    float sAz = sc.lightAz * PI / 180;
    float sEl = sc.lightEl * PI / 180;
    GLfloat L1pos[] = { cosf(sEl) * sinf(sAz), sinf(sEl), -cosf(sEl) * cosf(sAz), 0 };

    // Ambient, diffuse, specular, and zero arrays
    GLfloat amb[4] = { sc.ambR,  sc.ambG,  sc.ambB,  1.0f };
    GLfloat diff[4] = { sc.diffR, sc.diffG, sc.diffB, 1.0f };
    GLfloat spec[4] = { 1.0f,     1.0f,     1.0f,     1.0f };
    GLfloat zero[4] = { 0,        0,        0,        1 };

    // Apply global ambient light (modes 1 and 3)
    if (lightMode == 1 || lightMode == 3) {
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    }
    else {
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, zero); // no ambient in diffuse-only mode
    }

    // Configure directional lights based on mode
    if (lightMode == 1) {
        // Ambient-only mode: disable both directional lights
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
    }
    else {
        // Configure GL_LIGHT0 (manual directional)
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, L0pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, (lightMode >= 2) ? diff : zero);
        glLightfv(GL_LIGHT0, GL_SPECULAR, (lightMode == 3) ? spec : zero); // specular only in mode 3

        // Configure GL_LIGHT1 (time-of-day sun)
        glEnable(GL_LIGHT1);
        glLightfv(GL_LIGHT1, GL_POSITION, L1pos);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, (lightMode >= 2) ? diff : zero);
        glLightfv(GL_LIGHT1, GL_SPECULAR, (lightMode == 3) ? spec : zero);
    }
}

// ============================================================
//  COLLISION
// ============================================================

// ------------------------------------------------------------
// applyCollision: Clamps the character's proposed new position (nx, nz)
//                 to keep them within the play area and outside the temple.
//
// Two collision checks:
//   1. Land boundary: prevents walking off the edge of the ground plane
//   2. Temple box: pushes the character out through the nearest face
//      when they try to walk into the temple
//
// margin/tm = distance buffer so the character doesn't clip geometry
// ------------------------------------------------------------
void applyCollision(float& nx, float& nz) {
    // Land boundary: 12-unit inset from the edge of the ground plane
    const float margin = 12.0f;
    if (nx < LAND_MIN_X + margin) nx = LAND_MIN_X + margin;
    if (nx > LAND_MAX_X - margin) nx = LAND_MAX_X - margin;
    if (nz < LAND_MIN_Z + margin) nz = LAND_MIN_Z + margin;
    if (nz > LAND_MAX_Z - margin) nz = LAND_MAX_Z - margin;

    // Temple collision: 15-unit margin around the temple bounding box
    const float tm = 15.0f;
    if (nx > TEMPLE_MIN_X - tm && nx  < TEMPLE_MAX_X + tm &&
        nz >  TEMPLE_MIN_Z - tm && nz < TEMPLE_MAX_Z + tm) {

        // Find which temple face is nearest to the character's current position
        float dLeft = nx - (TEMPLE_MIN_X - tm);
        float dRight = (TEMPLE_MAX_X + tm) - nx;
        float dFront = nz - (TEMPLE_MIN_Z - tm);
        float dBack = (TEMPLE_MAX_Z + tm) - nz;
        float minD = fminf(fminf(dLeft, dRight), fminf(dFront, dBack));

        // Push character out through the nearest face
        if (minD == dLeft)       nx = TEMPLE_MIN_X - tm;
        else if (minD == dRight) nx = TEMPLE_MAX_X + tm;
        else if (minD == dFront) nz = TEMPLE_MIN_Z - tm;
        else                     nz = TEMPLE_MAX_Z + tm;
    }
}

// ============================================================
//  INPUT
// ============================================================

// ------------------------------------------------------------
// processInput: Reads keyboard state and updates all game variables.
// Called every frame from display() with the delta time (dt).
//
// Key groups handled:
//   Character movement : W/A/S/D (strafe/walk), Q/E (rotate)
//   Camera panning     : Arrow keys (pan) / Shift+Arrow (rotate)
//   Joint control      : T/G, Y/H, U/J etc. (without Shift)
//   Joint bend         : Same keys with Shift held
//   Manual light       : Keys 5/6/7/8 adjust azimuth/elevation
//   Fist/Walk toggles  : driven by state flags set in WM_KEYDOWN
//
void processInput(float dt) {
    isWalking = false;
    float moveSpeed = 60.0f * dt, rotSpeed = 100.0f * dt, jSpeed = 80.0f * dt;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    if (autoArmMove)
        autoArmTimer += dt;
    if (bellAttackBodyActive)
        bellAttackBodyTimer += dt;

    float fx = sinf((charRotY + 180) * PI / 180), fz = cosf((charRotY + 180) * PI / 180);
    float rx = cosf((charRotY + 180) * PI / 180), rz = -sinf((charRotY + 180) * PI / 180);
    float nx = charX, nz = charZ;

    // Constrain panning to stay within the box boundaries
    float panLimit = 400.0f;
    if (camPanX > panLimit)
        camPanX = panLimit;
    if (camPanX < -panLimit)
        camPanX = -panLimit;
    if (camPanY > panLimit)
        camPanY = panLimit;
    if (camPanY < -panLimit)
        camPanY = -panLimit;

    // -------------------------------------------------------------
    // CHARACTER MOVEMENT (W, A, S, D, Q, E)
    // -------------------------------------------------------------
    if (!cineCameraActive) {
        if (GetAsyncKeyState('W') & 0x8000) { // When press W, move forward 
            nx += fx * moveSpeed;
            nz += fz * moveSpeed;
            isWalking = true;
        }
        if (GetAsyncKeyState('S') & 0x8000) { // When press S, move backward 
            nx -= fx * moveSpeed;
            nz -= fz * moveSpeed;
            isWalking = true;
        }
        if (GetAsyncKeyState('D') & 0x8000) { // When press D, strafe right
            nx -= rx * moveSpeed;
            nz -= rz * moveSpeed;
            isWalking = true;
        }
        if (GetAsyncKeyState('A') & 0x8000) { // When press A, strafe left 
            nx += rx * moveSpeed;
            nz += rz * moveSpeed;
            isWalking = true;
        }
        if (GetAsyncKeyState('Q') & 0x8000)
            charRotY += rotSpeed;
        if (GetAsyncKeyState('E') & 0x8000)
            charRotY -= rotSpeed;
    }

    applyCollision(nx, nz);
    charX = nx; charZ = nz;
    if (isWalking)
        walkTimer += dt;

    if (!cineCameraActive) {
        // -------------------------------------------------------------
        // MANUAL JOINT ROTATIONS (WITHOUT SHIFT)
        // -------------------------------------------------------------
        if (!shift) { // no shift
            // HEAD
            if (GetAsyncKeyState('Z') & 0x8000) { // When press Z, head move left
                headRotY += jSpeed;
                if (headRotY > 40) headRotY = 40;
            }
            if (GetAsyncKeyState('X') & 0x8000) { // When press X, head move right
                headRotY -= jSpeed;
                if (headRotY < -40)
                    headRotY = -40;
            }

            // RIGHT ARM
            if (GetAsyncKeyState('Y') & 0x8000) { // When press Y, right upper arm pitch goes down 
                lUpperArmPitch -= jSpeed;
                if (lUpperArmPitch < -180)
                    lUpperArmPitch = -180;
            }
            if (GetAsyncKeyState('H') & 0x8000) { // When press H, right upper arm pitch goes up
                lUpperArmPitch += jSpeed;
                if (lUpperArmPitch > 50)
                    lUpperArmPitch = 50;
            }
            if (GetAsyncKeyState(VK_OEM_PERIOD) & 0x8000) { // When press ., wrist move down
                lWristPitch += jSpeed;
                if (lWristPitch > 10)
                    lWristPitch = 10;
            }
            if (GetAsyncKeyState(VK_OEM_COMMA) & 0x8000) { // When press ,, wrist move up
                lWristPitch -= jSpeed;
                if (lWristPitch < -10)
                    lWristPitch = -10;
            }

            // LEFT ARM
            if (GetAsyncKeyState('T') & 0x8000) { // When press T, left upper arm pitch goes down
                rUpperArmPitch -= jSpeed;
                if (rUpperArmPitch < -180)
                    rUpperArmPitch = -180;
            }
            if (GetAsyncKeyState('G') & 0x8000) { // When press G, left upper arm pitch goes up
                rUpperArmPitch += jSpeed;
                if (rUpperArmPitch > 50)
                    rUpperArmPitch = 50;
            }
            if (GetAsyncKeyState(VK_OEM_4) & 0x8000) { // When press [, left upper arm side goes down
                rUpperArmSide -= jSpeed;
                if (rUpperArmSide < 0)
                    rUpperArmSide = 0;
            }
            if (GetAsyncKeyState(VK_OEM_6) & 0x8000) { // When press ], left upper arm side goes up
                rUpperArmSide += jSpeed;
                if (rUpperArmSide > 110)
                    rUpperArmSide = 110;
            }
            if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) { // When press -, left wrist pitch goes up
                rWristPitch -= jSpeed;
                if (rWristPitch < -10)
                    rWristPitch = -10;
            }
            if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) { // When press +, left wrist pitch goes down
                rWristPitch += jSpeed;
                if (rWristPitch > 10)
                    rWristPitch = 10;
            }

            // LEGS & FEET
            if (GetAsyncKeyState('U') & 0x8000) { // When press U, left hip pitch goes down
                rHipPitch -= jSpeed;
                if (rHipPitch < -55)
                    rHipPitch = -55;
            }
            if (GetAsyncKeyState('J') & 0x8000) { // When press J, left hip pitch goes up
                rHipPitch += jSpeed;
                if (rHipPitch > 55)
                    rHipPitch = 55;
            }
            if (GetAsyncKeyState('I') & 0x8000) { // When press I, right hip pitch goes down
                lHipPitch -= jSpeed;
                if (lHipPitch < -55)
                    lHipPitch = -55;
            }
            if (GetAsyncKeyState('K') & 0x8000) { // When press K, right hip pitch goes up
                lHipPitch += jSpeed;
                if (lHipPitch > 55)
                    lHipPitch = 55;
            }
            if (GetAsyncKeyState('N') & 0x8000) { // When press N, left foot pitch goes down
                rFootPitch += jSpeed;
                if (rFootPitch > 10)
                    rFootPitch = 10;
            }
            if (GetAsyncKeyState('M') & 0x8000) { // When press M, left foot pitch goes up
                rFootPitch -= jSpeed;
                if (rFootPitch < 0)
                    rFootPitch = 0;
            }
        }

        // -------------------------------------------------------------
        // MANUAL JOINT ROTATIONS (WITH SHIFT)
        // -------------------------------------------------------------
        else { // with shift
            // HEAD
            if (GetAsyncKeyState('Z') & 0x8000) { // When press Z, head move up
                headRotX -= jSpeed;
                if (headRotX < -10)
                    headRotX = -10;
            }
            if (GetAsyncKeyState('X') & 0x8000) { // When press X, head move down
                headRotX += jSpeed;
                if (headRotX > 25)
                    headRotX = 25;
            }

            // RIGHT ARM
            if (GetAsyncKeyState('Y') & 0x8000) { // When press Y, right forearm bend goes up
                lForearmBend += jSpeed;
                if (lForearmBend > 75)
                    lForearmBend = 75;
            }
            if (GetAsyncKeyState('H') & 0x8000) { // When press H, right forearm bend goes down
                lForearmBend -= jSpeed;
                if (lForearmBend < 0)
                    lForearmBend = 0;
            }
            if (GetAsyncKeyState(VK_OEM_4) & 0x8000) { // When press [, right upper arm side goes down
                lUpperArmSide -= jSpeed;
                if (lUpperArmSide < 0)
                    lUpperArmSide = 0;
            }
            if (GetAsyncKeyState(VK_OEM_6) & 0x8000) { // When press ], right upper arm side goes up
                lUpperArmSide += jSpeed;
                if (lUpperArmSide > 110)
                    lUpperArmSide = 110;
            }
            if (GetAsyncKeyState(VK_OEM_COMMA) & 0x8000) { // When press ,, wrist move left
                lWristYaw += jSpeed;
                if (lWristYaw > 15)
                    lWristYaw = 15;
            }
            if (GetAsyncKeyState(VK_OEM_PERIOD) & 0x8000) { // When press ., wrist move right
                lWristYaw -= jSpeed;
                if (lWristYaw < -15)
                    lWristYaw = -15;
            }

            // LEFT ARM
            if (GetAsyncKeyState('T') & 0x8000) { // When press T, left forearm bend goes up
                rForearmBend += jSpeed;
                if (rForearmBend > 75)
                    rForearmBend = 75;
            }
            if (GetAsyncKeyState('G') & 0x8000) { // When press G, left forearm bend goes down
                rForearmBend -= jSpeed;
                if (rForearmBend < 0)
                    rForearmBend = 0;
            }
            if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) { // When press -, left wrist pitch goes down
                rWristYaw += jSpeed;
                if (rWristYaw > 15)
                    rWristYaw = 15;
            }
            if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) { // When press +, left wrist pitch goes up
                rWristYaw -= jSpeed;
                if (rWristYaw < -15)
                    rWristYaw = -15;
            }

            // LEGS & FEET
            if (GetAsyncKeyState('U') & 0x8000) { // When press U, left knee bend goes up
                rKneeBend += jSpeed;
                if (rKneeBend > 120)
                    rKneeBend = 120;
            }
            if (GetAsyncKeyState('J') & 0x8000) { // When press J, left knee bend goes down
                rKneeBend -= jSpeed;
                if (rKneeBend < 0)
                    rKneeBend = 0;
            }
            if (GetAsyncKeyState('I') & 0x8000) { // When press I, right knee bend goes up
                lKneeBend += jSpeed;
                if (lKneeBend > 120)
                    lKneeBend = 120;
            }
            if (GetAsyncKeyState('K') & 0x8000) { // When press K, right knee bend goes down
                lKneeBend -= jSpeed;
                if (lKneeBend < 0)
                    lKneeBend = 0;
            }
            if (GetAsyncKeyState('N') & 0x8000) { // When press N, right foot pitch goes up
                lFootPitch += jSpeed;
                if (lFootPitch > 10)
                    lFootPitch = 10;
            }
            if (GetAsyncKeyState('M') & 0x8000) { // When press M, right foot pitch goes down
                lFootPitch -= jSpeed;
                if (lFootPitch < 0)
                    lFootPitch = 0;
            }
        }

        // -------------------------------------------------------------
        // MANUAL LIGHTING CONTROL (5, 6, 7, 8)
        // -------------------------------------------------------------
        if (GetAsyncKeyState('5') & 0x8000) // When press 5, light move to left
            manualLightAzimuth -= rotSpeed;
        if (GetAsyncKeyState('6') & 0x8000) // When press 6, light move to right
            manualLightAzimuth += rotSpeed;
        if (GetAsyncKeyState('7') & 0x8000) { // When press 7, light move up
            manualLightElevation += rotSpeed * 0.5f;
            if (manualLightElevation > 89)
                manualLightElevation = 89;
        }
        if (GetAsyncKeyState('8') & 0x8000) { // When press 8, light move down
            manualLightElevation -= rotSpeed * 0.5f;
            if (manualLightElevation < 1)
                manualLightElevation = 1;
        }

        // -------------------------------------------------------------
        // CAMERA CONTROLS (Arrows = Pan, Shift+Arrows = Rotate)
        // -------------------------------------------------------------
        if (shift) { // with shift
            if (GetAsyncKeyState(VK_LEFT) & 0x8000) { // When press left arrow, camera rotate left
                pry += rotSpeed;
                if (pry >= 360.0f)
                    pry -= 360.0f;
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { // When press right arrow, camera rotate right
                pry -= rotSpeed;
                if (pry < 0.0f)
                    pry += 360.0f;
            }
            if (GetAsyncKeyState(VK_UP) & 0x8000) { // When press up arrow, camera rotate up
                prx += rotSpeed;
                if (prx > 89.0f)
                    prx = 89.0f;
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) { // When press down arrow, camera rotate down
                prx -= rotSpeed;
                if (prx < 0.0f)
                    prx = 0.0f;
            }
        }
        else { // No shift
            if (GetAsyncKeyState(VK_LEFT) & 0x8000) { // When press left arrow, camera pan left
                camPanX += moveSpeed * 2.0f;
                if (camPanX > 350.0f)
                    camPanX = 350.0f;
            }
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { // When press right arrow, camera pan right
                camPanX -= moveSpeed * 2.0f;
                if (camPanX < -350.0f)
                    camPanX = -350.0f;
            }
            if (GetAsyncKeyState(VK_UP) & 0x8000) { // When press up arrow, camera pan up
                camPanY -= moveSpeed * 2.0f;
                if (camPanY < -350.0f)
                    camPanY = -350.0f;
            }
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) { // When press down arrow, camera pan down
                camPanY += moveSpeed * 2.0f;
                if (camPanY > 350.0f)
                    camPanY = 350.0f;
            }
        }
    }

    // Smooth transition for fist blending
    if (isFist && fistBlend < 1)
        fistBlend = fminf(1, fistBlend + dt * 3);
    if (!isFist && fistBlend > 0)
        fistBlend = fmaxf(0, fistBlend - dt * 3);

    // Bell summoning timing
    if (bellState == BELL_SUMMONING) {
        bellSummonTimer += dt;
        if (bellSummonTimer >= SUMMON_DURATION) { bellState = BELL_PALM_HOVER; bellSummonTimer = 0; cineCameraActive = false; }
    }
}

// ============================================================
//  RESET
// ============================================================
void resetAll() {
    camDist = 400; pry = 0; prx = 12; camPanX = 0; camPanY = 0;
    charX = 0; charZ = 200; charRotY = 0;
    headRotY = 0; headRotX = 0;
    lUpperArmPitch = 0; lUpperArmSide = 0; lForearmBend = 0; lWristPitch = 0; lWristYaw = 0;
    rUpperArmPitch = 0; rUpperArmSide = 0; rForearmBend = 0; rWristPitch = 0; rWristYaw = 0;
    lHipPitch = 0; lKneeBend = 0; lFootPitch = 0; rHipPitch = 0; rKneeBend = 0; rFootPitch = 0;
    activeAnim = 0; animTimer = 0; autoArmMove = false; timeOfDay = 1;
    isFireActive = false; showWeapon = false; fistBlend = 0; isFist = false;
    manualLightAzimuth = 45; manualLightElevation = 60;
    bellState = BELL_HIDDEN; bellSummonTimer = 0; bellLaunchTimer = 0; bellDismissTimer = 0;
    bellWaveTimer = 0; bellRingTimer = 0; bellScale = BELL_PALM_SCALE; bellOrbitAngle = 0;
    cineCameraActive = false; doorOpen = false; doorAngle = 0;
    bellAttackBodyActive = false; bellAttackBodyTimer = 0.0f;
    isOrtho = false;
    lightMode = 3;
    enableTextures = false;
}

// ============================================================
//  DISPLAY
// ============================================================
void display(HDC hdc) {

    // After all calculations, force the camera to stay inside the box
    if (camPanX > 400)
        camPanX = 400;
    if (camPanX < -400)
        camPanX = -400;
    if (camPanY > 400)
        camPanY = 400;
    if (camPanY < -400)
        camPanY = -400;
    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(now - lastTime).count();

    if (dt > 0.1f)dt = 0.1f;
    lastTime = now;
    animTimer += dt;
    if (doorOpen && doorAngle < 100)
        doorAngle += 1.5f;
    if (!doorOpen && doorAngle > 0)
        doorAngle -= 1.5f;

    processInput(dt);

    SkyColors sc = getTimeOfDayColors(timeOfDay);
    glClearColor(sc.horizR * 0.8f, sc.horizG * 0.8f, sc.horizB * 0.8f, 1);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glEnable(GL_NORMALIZE);
    glViewport(0, 0, gWinW, gWinH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    float asp = (gWinH > 0) ? (float)gWinW / gWinH : 1;

    if (isOrtho) {
        float hw = camDist * asp * 0.5f, hh = camDist * 0.5f;
        glOrtho(-hw, hw, -hh, hh, -5000, 5000);
    }
    else {
        gluPerspective(45, asp, PNear, PFar);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(camPanX, camPanY, 0);
    glTranslatef(0, 0, -camDist);
    glRotatef(prx, 1, 0, 0);
    glRotatef(pry, 0, 1, 0);
    glTranslatef(-charX, 0, -charZ);

    if (isWalking && enableEarthquake) {
        glTranslatef(sinf(walkTimer * 30) * 1.5f, cosf(walkTimer * 45) * 1.5f, 0);
    }

    drawSky();
    setupLighting();
    drawGround();

    glPushMatrix();
    glTranslatef(0, -55.5f, -120.0f);
    drawTemple();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(charX, -55.0f, charZ);
    glRotatef(charRotY + 180, 0, 1, 0);
    drawYangXuan(animTimer, activeAnim);
    if (activeAnim == 3 && showWeapon && animTimer >= 0.8f && animTimer < 5) { drawWeaponDetached(activeAnim, animTimer); }
    glPopMatrix();
    drawBellInScene(dt);
    drawRainEffects(activeAnim, animTimer);
    SwapBuffers(hdc);
}

// ============================================================
//  WINDOW PROCEDURE
// ============================================================
LRESULT WINAPI WindowProcedure(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static bool key1Pressed = false, key2Pressed = false, key3Pressed = false, key4Pressed = false;
    static bool keyF1Pressed = false, keyF2Pressed = false, keyF3Pressed = false;
    static bool keyF4Pressed = false, keyF5Pressed = false, keyF6Pressed = false;
    static bool keyF7Pressed = false, keyF8Pressed = false;
    static bool keyFPressed = false, keyEnterPressed = false;

    switch (msg) {
    case WM_DESTROY: PostQuitMessage(0); break;
    case WM_SIZE:
        gWinW = LOWORD(lParam);
        gWinH = HIWORD(lParam);
        if (gWinH == 0) gWinH = 1;
        break;

    case WM_MOUSEWHEEL: {
        short z = GET_WHEEL_DELTA_WPARAM(wParam);
        camDist -= (z / 120.0f) * 10;
        if (camDist < 40) camDist = 40;
        if (camDist > 450) camDist = 450;
        break;
    }

    case WM_LBUTTONDOWN:
        isDragging = true;
        lastMouseX = (short)LOWORD(lParam);
        lastMouseY = (short)HIWORD(lParam);
        SetCapture(hWnd);
        break;

    case WM_LBUTTONUP:
        isDragging = false;
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
        if (isDragging && !cineCameraActive) {
            int mx = (short)LOWORD(lParam), my = (short)HIWORD(lParam);
            pry += (float)(mx - lastMouseX) * 0.5f; prx += (float)(my - lastMouseY) * 0.5f;
            if (prx > 89) prx = 89; if (prx < 0) prx = 0;
            if (pry >= 360.0f) pry -= 360.0f;
            if (pry < 0.0f) pry += 360.0f;
            lastMouseX = mx; lastMouseY = my;
        }
        break;

    case WM_KEYUP:
        if (wParam == '1')
            key1Pressed = false;
        if (wParam == '2')
            key2Pressed = false;
        if (wParam == '3')
            key3Pressed = false;
        if (wParam == '4')
            key4Pressed = false;
        if (wParam == 'F')
            keyFPressed = false;
        if (wParam == VK_F1)
            keyF1Pressed = false;
        if (wParam == VK_F2)
            keyF2Pressed = false;
        if (wParam == VK_F3)
            keyF3Pressed = false;
        if (wParam == VK_F4)
            keyF4Pressed = false;
        if (wParam == VK_F5)
            keyF5Pressed = false;
        if (wParam == VK_F6)
            keyF6Pressed = false;
        if (wParam == VK_F7)
            keyF7Pressed = false;
        if (wParam == VK_F8)
            keyF8Pressed = false;
        if (wParam == VK_RETURN)
            keyEnterPressed = false;
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            PostQuitMessage(0);
        else if (wParam == 'P')
            isOrtho = !isOrtho;
        else if (wParam == 'R')
            resetAll();
        else if (wParam == 'F') {
            if (!keyFPressed) {
                isFist = !isFist;
                keyFPressed = true;
            }
        }
        else if (wParam == VK_RETURN) {
            if (!keyEnterPressed) {
                doorOpen = !doorOpen;
                keyEnterPressed = true;
            }
        }

        // --- 1 to 4 System Toggles ---
        else if (wParam == '1') {
            if (!key1Pressed) {
                enableEarthquake = !enableEarthquake;
                key1Pressed = true;
            }
        }
        else if (wParam == '2') {
            if (!key2Pressed) {
                timeOfDay = (timeOfDay + 1) % 4;
                key2Pressed = true;
            }
        }
        else if (wParam == '3') {
            if (!key3Pressed) {
                enableTextures = !enableTextures;
                key3Pressed = true;
            }
        }
        else if (wParam == '4') {
            if (!key4Pressed) {
                lightMode = (lightMode + 1) % 4;
                key4Pressed = true;
            }
        }

        // --- F1 to F8 Weapon/Animation Toggles ---
        else if (wParam == VK_F1) {
            if (!keyF1Pressed) {
                showWeapon = !showWeapon;
                keyF1Pressed = true;
            }
        }
        else if (wParam == VK_F2) {
            if (!keyF2Pressed) {
                isFireActive = !isFireActive;
                keyF2Pressed = true;
            }
        }
        else if (wParam == VK_F3) {
            activeAnim = 3;
            animTimer = 0;
        } // Heavenly Rain
        else if (wParam == VK_F4) {
            activeAnim = 6;
            animTimer = 0;
        } // Forward Thrust
        else if (wParam == VK_F5) {
            if (!keyF5Pressed) {
                autoArmMove = !autoArmMove;
                keyF5Pressed = true;
            }
        } // Dash loop

        // Bell Commands
        else if (wParam == VK_F6) {
            if (!keyF6Pressed) {
                keyF6Pressed = true;
                if (bellState == BELL_HIDDEN) {
                    bellState = BELL_SUMMONING; bellSummonTimer = 0; bellRingTimer = 0;
                    cineSavedPry = pry; cineSavedPrx = prx; cineSavedDist = camDist;
                    cineSavedPanX = camPanX; cineSavedPanY = camPanY;
                    cineCameraActive = true;
                }
            }
        }
        else if (wParam == VK_F7) {
            if (!keyF7Pressed) {
                keyF7Pressed = true;
                if (bellState == BELL_PALM_HOVER || bellState == BELL_ATTACK_ACTIVE || bellState == BELL_RETURNING) {
                    bellState = BELL_LAUNCHING; bellLaunchTimer = 0; bellWaveTimer = 0;
                    float fwdX = sinf((charRotY + 180) * PI / 180);
                    float fwdZ = cosf((charRotY + 180) * PI / 180);
                    float launchSpeed = 220.0f;
                    bellVelX = fwdX * launchSpeed; bellVelY = 0.0f; bellVelZ = fwdZ * launchSpeed;
                    bellScale = BELL_PALM_SCALE;
                    bellAttackBodyActive = true; bellAttackBodyTimer = 0.0f;
                }
            }
        }
        else if (wParam == VK_F8) {
            if (!keyF8Pressed) {
                keyF8Pressed = true;
                if (bellState != BELL_HIDDEN && bellState != BELL_DISMISSING) {
                    bellState = BELL_DISMISSING; bellDismissTimer = 0; cineCameraActive = false;
                }
            }
        }
        break;
    default: break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================
//  MAIN
// ============================================================
bool initPixelFormat(HDC hdc) {
    PIXELFORMATDESCRIPTOR pfd; ZeroMemory(&pfd, sizeof(pfd));
    pfd.cAlphaBits = 8; pfd.cColorBits = 32; pfd.cDepthBits = 24;
    pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
    pfd.iLayerType = PFD_MAIN_PLANE; pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    int n = ChoosePixelFormat(hdc, &pfd);
    return SetPixelFormat(hdc, n, &pfd) != 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEX wc; ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc); wc.hInstance = GetModuleHandle(NULL);
    wc.lpfnWndProc = WindowProcedure; wc.lpszClassName = WINDOW_TITLE;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassEx(&wc))return 0;
    HWND hWnd = CreateWindow(WINDOW_TITLE, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
        50, 30, 1024, 768, NULL, NULL, wc.hInstance, NULL);
    HDC hdc = GetDC(hWnd);
    initPixelFormat(hdc);
    HGLRC hglrc = wglCreateContext(hdc);
    if (!wglMakeCurrent(hdc, hglrc))return 0;
    glGenTextures(TEX_COUNT, textures);
    loadAllTextures();
    ShowWindow(hWnd, nCmdShow);
    lastTime = std::chrono::high_resolution_clock::now();
    MSG msg; ZeroMemory(&msg, sizeof(msg));
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)break;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        display(hdc);
    }
    UnregisterClass(WINDOW_TITLE, wc.hInstance);
    return 0;
}