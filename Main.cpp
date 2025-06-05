/*

TODO:
-poprawić sterowanie kamerą(np. na podobne jak w Blenderze)
-dodać sterowanie
    -do wpisanych koordynatów
-dodać animacje przemieszczania do nowej pozycji
-dodać tryby uczenia i pracy
-dodać kolizje
-dodać interakcje z elemnetem otoczenia
-dodać oświetlenie
-GUI
Opcjonalnie:
-dodać możliwość wczytywania osi obrotu/przesuwu(?) i ograniczeń ruchu z pliku
-więcej modeli robotów(cylindryczny, polarny, itp.)
-model rzeczywistego robota

Sterowanie:
= zwiększ nastawę złącza
- zmniejsz nastawę złącza
. wybierz kolejne złącze
, wybierz poprzednie złącze

*/
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <math.h>
#include <memory>
#include <vector>

Matrix MatrixTranslate(Vector3 translation) {
    return MatrixTranslate(translation.x, translation.y, translation.z);
}

Matrix DHtoMatrix(Vector4 DH) {
    Matrix result;
    Matrix RT = MatrixMultiply(MatrixRotateX(DH.x), MatrixTranslate(DH.y, 0, 0));
    Matrix TR = MatrixMultiply(MatrixTranslate(0, DH.z, 0), MatrixRotateY(DH.w));
    result = MatrixMultiply(RT, TR);
    return result;
}

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

class RobotArm {
public:
    RobotArm(const char* fileName) {
        model = LoadModel(fileName);
        absoluteTransforms.resize(model.meshCount);
        DHparameters.resize(model.meshCount);
        absoluteTransforms[0] = MatrixTranslate(model.bindPose[0].translation);
        DHparameters[0].x = 0;
        DHparameters[0].y = 0;
        DHparameters[0].z = 0;
        DHparameters[0].w = 0;
        for (int i = 1;i < model.meshCount;i++) {
            absoluteTransforms[i] = MatrixTranslate(model.bindPose[i].translation);
            DHparameters[i].x = 0;
            DHparameters[i].y = 0;
            DHparameters[i].z = model.bindPose[i].translation.y - model.bindPose[i - 1].translation.y;
            DHparameters[i].w = 0;
        }
    }
    ~RobotArm() {
        UnloadModel(model);
    }
    void Draw(int selection) {
        for (int i = 0;i < model.meshCount;i++) {
            if (i == selection) {
                model.materials[model.meshMaterial[0]].maps[MATERIAL_MAP_DIFFUSE].color = YELLOW;
            }
            else {
                model.materials[model.meshMaterial[0]].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            }
            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[0]], absoluteTransforms[i]);
            rlEnableWireMode();
            model.materials[model.meshMaterial[0]].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[0]], absoluteTransforms[i]);
            rlDisableWireMode();
        }
    }
    void MoveJoint(int selection, int direction) {
        DHparameters[selection].x += direction * DEG2RAD * 5;
        absoluteTransforms[0] = DHtoMatrix(DHparameters[0]);
        for (int i = 1;i < model.meshCount;i++) {
            absoluteTransforms[i] = MatrixMultiply(DHtoMatrix(DHparameters[i]), absoluteTransforms[i - 1]);
        }
    }
    Model model;
    std::vector<Matrix> absoluteTransforms;
    std::vector<Vector4> DHparameters;
};

int main() {
    InitWindow(800, 800, "robot");

    SetTargetFPS(60);

    clCamera CamInstance({ 4.0f, 2.0f, 4.0f });
    RobotArm robot("models/robot.glb");

    int selection = 1;
    const int maxSelection = robot.model.meshCount - 1;

    while (!WindowShouldClose()) {
        UpdateCamera(&CamInstance.parameters, CAMERA_FIRST_PERSON);
        if (IsKeyPressed(KEY_PERIOD)) {
            if (selection == maxSelection) {
                selection = 1;
            }
            else {
                selection += 1;
            }
        }
        if (IsKeyPressed(KEY_COMMA)) {
            if (selection == 1) {
                selection = maxSelection;
            }
            else {
                selection -= 1;
            }
        }
        if (IsKeyPressed(KEY_EQUAL)) {
            robot.MoveJoint(selection, 1);
        }
        if (IsKeyPressed(KEY_MINUS)) {
            robot.MoveJoint(selection, -1);
        }
        BeginDrawing();
            ClearBackground(RAYWHITE);
            BeginMode3D(CamInstance.parameters);
            DrawGrid(20, 1.0f);
            DrawLine3D({ 0,0,0 }, { 500,0,0 }, RED);    //X
            DrawLine3D({ 0,0,0 }, { 0,500,0 }, GREEN);  //Y
            DrawLine3D({ 0,0,0 }, { 0,0,500 }, BLUE);   //Z
            robot.Draw(selection);
            EndMode3D();
        EndDrawing();
    }

    EnableCursor();
    CloseWindow();
    return 0;
}
