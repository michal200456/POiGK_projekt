/*
TODO:
-poprawić sterowanie kamerą(np. na podobne jak w Blenderze)
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
        std::vector<Matrix> absoluteTransforms;
        absoluteTransforms.resize(model.meshCount);
        relativeTransforms.resize(model.meshCount);
        absoluteTransforms[0] = MatrixTranslate(model.bindPose[0].translation);
        relativeTransforms[0] = absoluteTransforms[0];
        for (int i = 1;i < model.meshCount;i++) {
            absoluteTransforms[i] = MatrixTranslate(model.bindPose[i].translation);
            relativeTransforms[i] = MatrixMultiply(MatrixInvert(absoluteTransforms[i - 1]), absoluteTransforms[i]);
        }
    }
    ~RobotArm() {
        UnloadModel(model);
    }
    void Draw(int selection) {
        Matrix c = model.transform;
        for (int i = 0;i < model.meshCount;i++) {
            c = MatrixMultiply(c, relativeTransforms[i]);
            if (i == selection) {
                model.materials[model.meshMaterial[0]].maps[MATERIAL_MAP_DIFFUSE].color = YELLOW;
            }
            else {
                model.materials[model.meshMaterial[0]].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            }
            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[0]], c);
            rlEnableWireMode();
            model.materials[model.meshMaterial[0]].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
            DrawMesh(model.meshes[i], model.materials[model.meshMaterial[0]], c);
            rlDisableWireMode();
        }
    }
    void MoveJoint(int selection, int direction) {  //WIP, teraz nie działa jak powinno, trzeba by to zrobić metodą DH
        Matrix original = relativeTransforms[selection];
        relativeTransforms[selection] = MatrixIdentity();
        relativeTransforms[selection] = MatrixMultiply(relativeTransforms[selection], QuaternionToMatrix(QuaternionFromAxisAngle({ 1,0,0 }, direction * DEG2RAD * 5)));
        relativeTransforms[selection] = MatrixMultiply(relativeTransforms[selection], original);
    }
    Model model;
    std::vector<Matrix> relativeTransforms;
};

int main() {
    InitWindow(800, 800, "robot");

    SetTargetFPS(60);

    clCamera CamInstance({ 4.0f, 2.0f, 4.0f });
    RobotArm robot("models/robot.glb");

    int selection = 0;
    const int maxSelection=robot.model.meshCount;

    while (!WindowShouldClose()) {
        UpdateCamera(&CamInstance.parameters, CAMERA_FIRST_PERSON);
        if (IsKeyPressed(KEY_PERIOD)) {
            selection += 1;
            selection = selection % maxSelection;
        }
        if (IsKeyPressed(KEY_COMMA)) {
            selection -= 1;
            selection = selection % maxSelection;
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
            robot.Draw(selection);
            EndMode3D();

        EndDrawing();
    }

    EnableCursor();
    CloseWindow();
    return 0;
}
