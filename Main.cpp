/*
TODO:
-poprawić sterowanie kamerą(np. na podobne jak w Blenderze)
-poprawić wczytywanie modelu robota
-dodać możliwość wczytywania osi obrotu/przesuwu(?) i ograniczeń ruchu z pliku
-dodać sterowanie
    -poszczególnymi złączami
    -do wpisanych koordynatów
-dodać animacje przemieszczania do nowej pozycji
-dodać tryby uczenia i pracy
-dodać kolizje
-dodać interakcje z elemnetem otoczenia
-dodać oświetlenie
-GUI
Opcjonalnie:
-więcej modeli robotów(cylindryczny, polarny, itp.)
-model rzeczywistego robota
*/
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <memory>
#include <vector>

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
    Segment(Model m, int mesh) {
        model.materialCount = 1;
        model.transform = m.transform;
        model.meshCount = 1;
        model.meshes = (Mesh*)RL_CALLOC(model.meshCount, sizeof(Mesh));
        model.meshMaterial = (int*)RL_CALLOC(model.meshCount, sizeof(int));
        model.materials = (Material*)RL_CALLOC(model.materialCount, sizeof(Material));
        model.meshMaterial[0] = 0;
        model.meshes[0] = m.meshes[mesh];
        model.materials[0] = m.materials[mesh];
    }
    Segment(const char* fileName) {
        model = LoadModel(fileName);
    }
    ~Segment() {
        UnloadModel(model);
    }
    void Draw() {
        DrawModel(model, Vector3Zero(), 1.f, WHITE);
        DrawModelWires(model, Vector3Zero(), 1.f, BLACK);
    }
    Model model;
};

class RobotArm {
public:
    RobotArm(const char* fileName) {
        Model model = LoadModel(fileName);
        segments.resize(static_cast<size_t>(model.meshCount));
        for (int i = 0; i < model.meshCount; i++) {
            segments[i] = std::make_unique<Segment>(model, i);
        }
        UnloadModel(model);
    }
    RobotArm() {
        segments.resize(4);
        for (int i = 0; i < 4; i++) {
            char buffer[32];
            snprintf(buffer, 32, "models/robot1/%d.obj", i + 1);
            segments[i] = std::make_unique<Segment>(buffer);
        }
    }
    void Draw() {
        for (int i = 0; i < static_cast<int>(segments.size()); i++) {
            segments[i]->Draw();
        } 
    }
    std::vector<std::unique_ptr<Segment>> segments;
};

int main() {
    InitWindow(800, 800, "robot");

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

        EndDrawing();
    }


    EnableCursor();
    CloseWindow();
    return 0;
}
