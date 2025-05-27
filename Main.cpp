#include <raylib.h>
#include <rlgl.h>
#include <math.h>
#include <memory>

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
    void Update() {

    }
    Camera3D parameters;

};

class Segment {
public:
    Segment(Vector3 Position, Vector3 Orientation) {
        position = Position;
        orientation = Orientation;
        model = LoadModel("segment.obj");
    }
    ~Segment() {
        UnloadModel(model);
    }
    void Draw() {
        DrawModel(model, position, 1.f, color);
        DrawModelWires(model, position, 1.f, BLACK);
    }
    Color color = { 123,130,122,255 };
    Vector3 position;
    Vector3 orientation;
    Model model;
};

int main() {
    
    InitWindow(800, 800, "robot");
    SetTargetFPS(60);

    clCamera CamInstance({ 4.0f, 2.0f, 4.0f });
    Segment robot({ 0,0,0 }, { 0,0,0 });

    while (!WindowShouldClose()) {
        UpdateCamera(&CamInstance.parameters, CAMERA_FIRST_PERSON);
        BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode3D(CamInstance.parameters);
            DrawGrid(20, 1.0f);
            robot.Draw();
            EndMode3D();

        EndDrawing();
    }

    EnableCursor();
    CloseWindow();
    return 0;
}
