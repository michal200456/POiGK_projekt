#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <memory>

#define RAYGUI_IMPLEMENTATION
#include "external/raylib/raygui.h"

typedef struct {
    const char* objFile;
    Vector3 attachmentPoint;
} ObjInfo;

class clCamera {
    public:
    clCamera(Vector3 pos) {
        parameters.position = pos;
        parameters.target = {0, 0, 0};
        parameters.up = {0, 1, 0};
        parameters.fovy = 45.0f;
        parameters.projection = CAMERA_PERSPECTIVE;
        SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
        DisableCursor();
    }
    Camera3D parameters;
};

class Segment {
public:
    Segment(Vector3 Position, Vector3 Orientation, ObjInfo o ) {
        position = Position;
        orientation = Orientation;
        model = LoadModel(o.objFile);
        attachmentPoint = o.attachmentPoint;
    }
    ~Segment() {
        UnloadModel(model);
    }
    void Draw() {
        DrawModel(model, position, 1.f, WHITE);
        DrawModelWires(model, position, 1.f, BLACK);
    }
    Vector3 position;
    Vector3 orientation;
    Vector3 attachmentPoint;
    Model model;
};

const ObjInfo Iplatform = {"platform.obj",{0.f,1.f,0.f}};
const ObjInfo Isegment = {"segment.obj",{0.f,2.f,0.f}};
class RobotArm {
public:
    RobotArm() {
        Vector3 cPos = Vector3Zero();
        segments[0] = std::make_unique<Segment>(cPos, cPos, Iplatform);
        for (int i = 1;i < 4;i++) {
            cPos = Vector3Add(cPos, segments[i - 1]->attachmentPoint);
            segments[i] = std::make_unique<Segment>(cPos, Vector3Zero(), Isegment);
        }
    }
    void Draw() {
        for (int i = 0;i < 4;i++) {
            segments[i]->Draw();
        } 
    }
    std::unique_ptr<Segment> segments[4];
};

int main() {
    InitWindow(800, 800, "robot");

    // layout_name: controls initialization
    //----------------------------------------------------------------------------------
    bool DropdownBox000EditMode = false;
    int DropdownBox000Active = 0;
    bool TextBox001EditMode = false;
    char TextBox001Text[128] = "Tryb:";
    bool DropdownBox002EditMode = false;
    int DropdownBox002Active = 0;
    bool Button003Pressed = false;
    //----------------------------------------------------------------------------------

    SetTargetFPS(60);

    clCamera CamInstance({ 4.0f, 2.0f, 4.0f });
    RobotArm robot;

    while (!WindowShouldClose()) {
        UpdateCamera(&CamInstance.parameters, CAMERA_FIRST_PERSON);
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(CamInstance.parameters);
            DrawGrid(20, 1.0f);
            robot.Draw();
            EndMode3D();

            // raygui: controls drawing
            //----------------------------------------------------------------------------------
            if (DropdownBox000EditMode || DropdownBox002EditMode) GuiLock();

            if (GuiTextBox({ 144, 0, 40, 24 }, TextBox001Text, 128, TextBox001EditMode)) TextBox001EditMode = !TextBox001EditMode;
            Button003Pressed = GuiButton({ 640, 0, 160, 24 }, "Wlacz tryb uczenia/pracy"); 
            if (GuiDropdownBox({ 0, 0, 120, 24 }, "Typ robota;TWO;THREE", &DropdownBox000Active, DropdownBox000EditMode)) DropdownBox000EditMode = !DropdownBox000EditMode;
            if (GuiDropdownBox({ 184, 0, 152, 24 }, "Sterowanie zlaczami;Polozenie koncowe", &DropdownBox002Active, DropdownBox002EditMode)) DropdownBox002EditMode = !DropdownBox002EditMode;
            
            GuiUnlock();
            //----------------------------------------------------------------------------------

        EndDrawing();
    }

    EnableCursor();
    CloseWindow();
    return 0;
}
